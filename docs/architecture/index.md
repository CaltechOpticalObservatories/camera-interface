# Architecture

:::{note}
Expands in M3 with the thread model, the frame path and the instrument plugin contract in full. The
layering below is accurate now.
:::

The C++ reference here is narrative and links to source rather than reproducing signatures.

## Layers

```
        client (socket, socksend, or Python module)
                          |
   Camera::Server ........|...... command dispatch, ports, logging
                          |
   Camera::Interface .....|...... the camera, as commands
        |         |
        |         +------ Camera::ExposureMode ..... how one exposure runs
        |
   Camera::Controller ....|...... the wire to the hardware
                          |
              Archon (TCP) or ARC (PCIe)
```

`Camera::Server` ({source}`camerad/camera_server.h`)
: Owns the ports and the command dispatch loop. Parses a command line, calls the matching
  `Interface` method, and formats the reply. Knows nothing about detectors.

`Camera::Interface` ({source}`camerad/camera_interface.h`)
: The abstract camera. Declares one pure virtual per command: `expose`, `exptime`, `bias`, `bin`,
  `load_firmware`, `power` and the rest. `ArchonInterface` and `AstroCamInterface` implement it for
  the two controller families, and an instrument module subclasses one of those.

`Camera::Controller`
: The transport to the hardware, separate from `Interface` so that the command semantics and the
  wire protocol can vary independently.

`Camera::ExposureMode` ({source}`camerad/exposure_modes.h`)
: One way of running an exposure, as a producer/consumer pair: an acquisition thread pulls frames
  from the controller, a processing thread turns them into images. A single `expose` command means
  different things for a slow CCD readout and a continuously reading H2RG, and this is where that
  difference lives. `Interface::select_expose_mode()` picks one.

## Instrument modules

An instrument is a separate git repository, checked out as a submodule under
`camerad/Instruments/<name>` and selected with `-DINSTRUMENT=`. It contributes four things:

1. A `<name>.cmake` fragment setting `INSTRUMENT_SOURCES`, which is the whole of the build
   integration.
2. An `Interface` subclass, deriving from `ArchonInterface` or `AstroCamInterface`, overriding what
   the detector needs and adding instrument commands via `instrument_cmd()` and
   `is_instrument_command()`.
3. Its own `ExposureMode` implementations, if the stock ones do not fit.
4. Optionally a FITS header dictionary and shipped `.cfg` and `.acf` files.

An interface factory function ties it together, so the core builds against the base class and never
names a concrete instrument.

See [instruments](../instruments/index.md) for the four that exist, and
{source}`camerad/Instruments/hispec_tracking_camera` for the most complete example.

## Frame path

Acquisition is deliberately decoupled from output. The exposure mode's processing thread publishes a
completed frame to every configured [frame output](../configuration/frame-outputs.md); each output
owns its own queue and thread. Nothing an output does can block acquisition, which is why the FITS
writer drops frames instead of applying backpressure.
