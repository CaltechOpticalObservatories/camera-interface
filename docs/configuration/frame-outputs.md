# Frame output keys

Every instrument publishes each acquired frame to zero or more outputs, built at startup from these
keys. The two outputs are independent: either, both or neither can be enabled.

## FITS

Writes one FITS file per frame. A queue and a dedicated writer thread keep the readout thread from
ever blocking on disk.

| Key | Default | Meaning |
|---|---|---|
| `FITS_ENABLED` | `no` | Enable the FITS writer |
| `FITS_OUTPUT_DIR` | `/tmp/images` | Base directory. Must already exist. |
| `FITS_AUTODIR` | `no` | Write into a `YYYYMMDD` subdirectory of `FITS_OUTPUT_DIR` |
| `FITS_BASENAME` | `tracking` | Base filename |
| `FITS_QUEUE_SIZE` | `32` | Frames buffered for the writer thread. The oldest is dropped when full. |
| `FITS_DRAIN_TIMEOUT_MS` | `5000` | How long to keep draining the queue at shutdown before giving up |

:::{warning}
Dropping frames is the designed behaviour, not a failure mode: disk is slower than acquisition can
be, and stalling acquisition to wait for a write would be worse. If you need every frame, size
`FITS_QUEUE_SIZE` for the burst and watch the dropped count in `output_status()`.
:::

## Shared memory

Publishes each frame as an [ImageStreamIO](https://github.com/milk-org/ImageStreamIO) stream, which
AO frameworks such as [cacao](https://github.com/cacao-org/cacao) read directly. Needs a build with
`-DENABLE_SHM_OUTPUT=ON`.

| Key | Default | Meaning |
|---|---|---|
| `SHM_ENABLED` | `no` | Enable the shared-memory writer |
| `SHM_SEGMENT_NAME` | `camera` | ImageStreamIO stream name |
| `SHM_RING_BUFFER_SIZE` | `4` | Depth of the internal history ring buffer (`CBsize`). Separate from the live frame a real-time reader sees. |
| `SHM_DIR` | unset | Directory ImageStreamIO writes into. Unset falls back to `MILK_SHM_DIR`, then `/milk/shm`. If set, it must exist and be writable. |

Frame geometry is deliberately not a key. An ImageStreamIO stream's geometry is fixed for its
lifetime, so the writer recreates the stream whenever the geometry changes from what is allocated.

Unlike the FITS writer, this output posts a semaphore per frame, so it is the right attachment point
for anything that has to wake up on every frame.

### Readers

`camerad-shm-reader` prints geometry, keywords and pixel statistics once, for diagnostics:

```bash
camerad-shm-reader <segment-name> [shm-dir]
```

`python/examples/shm_read_frames.py` is a streaming consumer that blocks on the semaphore and flags
frames it missed:

```bash
python python/examples/shm_read_frames.py --segment hispec_tracking_camera --count 10
```

It needs numpy and `ImageStreamIOWrap`, the Python wrapper from the ImageStreamIO source tree built
with `-DPYTHON_WRAPPER=ON`. The wrapper is not on PyPI.
