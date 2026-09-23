# Architecture

How the pieces fit together, and why acquisition is kept separate from output. The C++ reference
here is narrative and links to source rather than reproducing signatures.

## Layers

```
        client (socket, camerad-socksend, or the Python module)
                          |
   Camera::Server .......................  one TCP port, command dispatch, logging
                          |
   Camera::Interface ....................  the camera, expressed as commands
        |            |              |
        |            |              +-----  FrameOutput ....  FITS file, shared memory
        |            |
        |            +--------------------  Camera::ExposureMode ....  how one exposure runs
        |
   Camera::Controller ...................  the wire to the hardware
                          |
              Archon (TCP) or ARC (PCIe)
```

`Camera::Server` ({source}`camerad/camera_server.h`)
: Owns the listening socket and the command dispatch chain. Parses a command line, calls the
  matching `Interface` method, appends `DONE` or `ERROR`, writes the reply back. Knows nothing about
  detectors. Each client connection is served on its own thread.

`Camera::Interface` ({source}`camerad/camera_interface.h`)
: The abstract camera. Declares one pure virtual per command, and owns the config, the
  `Camera::Information` for the current exposure, and the list of frame outputs.
  `ArchonInterface` and `AstroCamInterface` implement it for the two controller families; an
  instrument module subclasses one of those.

`Camera::Controller`
: The transport to the hardware, kept separate from `Interface` so command semantics and wire
  protocol vary independently.

`Camera::ExposureMode` ({source}`camerad/exposure_modes.h`)
: One way of running an exposure. `ExposureModeTemplate<InterfaceType>` is the base each concrete
  mode derives from; the template parameter gives a mode typed access to its interface. Each mode
  owns its own `Camera::Information` for processed and unprocessed images, and an `ImageProcessor`
  for deinterlacing.

A single `expose` means different things for a slow CCD readout and a continuously reading H2RG,
which is exactly what the exposure mode abstracts.

## Acquisition: producer and consumer

An exposure mode is a producer/consumer pair over a bounded queue:

`image_acquisition_thread()`
: The producer. Pulls frames off the controller as fast as the hardware delivers them and enqueues
  them.

`image_processing_thread()`
: The consumer. Dequeues a frame, runs it through the deinterlacer, and hands the result to the
  frame outputs.

The base class carries the synchronization (`queue_mutex`, `queue_cv`, and the
`is_producer_finished` / `is_producer_error` / `is_consumer_error` flags), so a concrete mode
implements only the parts that differ. In the tracking camera module, for instance, a shared base
owns the queue and the consumer and each subclass implements only the producer.

Two lifetimes exist. For a counted exposure the consumer is one-shot: `do_expose()` spawns it,
and it terminates once the producer is finished and the queue is drained. In freerun, one producer
and one consumer run for the whole session and `expose` returns immediately, leaving them going
until an abort or a producer error.

## The frame path

A processed frame is fanned out synchronously to every configured output by
`Interface::dispatch_frame()`, which simply calls `write()` on each in turn. When the whole exposure
command finishes, `end_exposure()` tells each output, so a multi-frame output such as a data cube
can finalize its file.

Outputs are built once at startup by `Interface::configure_frame_outputs()`, called from
{source}`camerad/camerad.cpp` for every instrument. It is deliberately not virtual, so no derived
class can silently skip wiring its outputs.

### Why outputs never block

`dispatch_frame()` runs on the consumer thread, so anything slow inside an output would stall
acquisition. Each output is therefore required to return promptly, and absorbs the mismatch itself.

The FITS writer is the clearest case ({source}`utils/fits_writer.h`): `write()` copies the pixels
into a bounded queue and returns, never touching CCfits. A dedicated worker thread drains that queue
to disk. When the queue is full the oldest frame is dropped. Disk is slower than acquisition can be,
and the design chooses to lose a frame rather than apply backpressure to the detector.

This is also why `FrameOutput::status()` is documented as a snapshot and never a barrier: letting a
caller wait on an output would serialize acquisition behind disk I/O, reintroducing the coupling the
queue exists to remove.

The shared-memory output makes the opposite trade ({source}`utils/shared_memory_writer.cpp`): it
copies the frame into the stream buffer and calls `ImageStreamIO_UpdateIm`, which posts the
semaphores. Nothing it does depends on a reader, so a reader that cannot keep up misses frames but
never delays the writer. Anything that must be woken per frame should attach there rather than watch
for files.

See [frame output keys](../configuration/frame-outputs.md) for configuring them.

## Instrument modules

An instrument is a separate repository, checked out as a submodule under
`camerad/Instruments/<name>` and selected with `-DINSTRUMENT=`. It contributes:

1. A `<name>.cmake` fragment setting `INSTRUMENT_SOURCES`. That is the whole build integration.
2. An `Interface` subclass deriving from `ArchonInterface` or `AstroCamInterface`, overriding what
   the detector needs and adding commands through `instrument_cmd()` and `is_instrument_command()`.
3. Its own `ExposureMode` implementations, where the stock ones do not fit.
4. Optionally a FITS header dictionary and shipped `.cfg` and `.acf` files.

An interface factory ties it together, so the core builds against the base class and never names a
concrete instrument.

Instrument commands are checked *before* the base command chain, so an instrument can add commands
and also override a base one of the same name.

See [instruments](../instruments/index.md) for the modules that exist, and
{source}`camerad/Instruments/hispec_tracking_camera` for the most complete example.
