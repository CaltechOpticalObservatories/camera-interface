# Getting started

From a clean checkout to a FITS file on disk, with no detector controller attached: the Archon
emulator stands in for the hardware.

## Dependencies

`camerad` needs a C++20 compiler, CMake 3.12 or newer, and:

- cfitsio and CCfits, for FITS output
- OpenCV, Boost (thread and chrono), nlohmann-json
- ZeroMQ and [zmqpp](https://github.com/zeromq/zmqpp), which is not usually packaged and is built from source
- gtest, to run the unit tests
- [ImageStreamIO](https://github.com/milk-org/ImageStreamIO), only for the shared-memory output

On Debian or Ubuntu the packaged ones are what CI installs:

```bash
sudo apt-get install -y build-essential cmake ninja-build \
  libccfits-dev libcfitsio-dev libcurl4-openssl-dev libgtest-dev \
  nlohmann-json3-dev libzmq3-dev libopencv-dev \
  libboost-thread-dev libboost-chrono-dev
```

## Build

`-DCONTROLLER=` is required; CMake stops with an error without it. `-DINSTRUMENT=` selects an
instrument module from `camerad/Instruments/<name>`.

```bash
git clone --recurse-submodules \
  https://github.com/CaltechOpticalObservatories/camera-interface.git
cd camera-interface/build
cmake -DCONTROLLER=archon -DINSTRUMENT=hispec_tracking_camera ..
make
```

Binaries land in `bin/` in the source tree. `make install` copies them under
`CMAKE_INSTALL_PREFIX` instead, which is the better choice for anything deployed. The full set of
build options is in the [configuration reference](../configuration/index.md).

:::{note}
Instrument modules are git submodules. A clone without `--recurse-submodules` leaves
`camerad/Instruments/<name>` empty and `-DINSTRUMENT=` will fail. Fix it with
`git submodule update --init --recursive`.
:::

## Run it

This uses `config/frame_outputs_test/frame_outputs_test.cfg`, which is the config the CI integration
test drives, so it is known to work end to end. It enables the FITS writer, points `ARCHON_IP` and
`ARCHON_PORT` at the emulator, and listens on port 3131.

Start the emulator:

```bash
bin/camerad-emulator config/frame_outputs_test/frame_outputs_test.cfg -i generic
```

Then the server, in another terminal:

```bash
bin/camerad --foreground --config config/frame_outputs_test/frame_outputs_test.cfg
```

`--config` is required. Without `--foreground` the server daemonizes. Logging always goes to a daily
file under `LOGPATH`; `--foreground` additionally writes it to stderr.

## Take an exposure

`camerad-socksend` sends one command and prints the reply. Point it at `BLKPORT`.

```bash
send() { bin/camerad-socksend -p 3131 -t 60 "$1"; }

send "open"          # connect to the controller
send "load"          # load firmware named by DEFAULT_FIRMWARE
send "power on"
send "exptime 1.5"   # seconds, because this config leaves LONGEXPOSURE at its default
send "expose 1"
```

Each returns `DONE` or `ERROR`. The FITS file appears under `FITS_OUTPUT_DIR`, which this config
sets to `/tmp/ci_fits_test`, named from `FITS_BASENAME`.

Check the header carries what the instrument promises:

```bash
python3 python/tests/fits_header_check.py /tmp/ci_fits_test/ci_frame_outputs_*.fits --exptime 1.5
```

:::{warning}
`DONE` means the server accepted and completed the command, not that a file was written. The FITS
writer drops frames by design when its queue backs up, so the file on disk is the real confirmation.
See [frame outputs](../configuration/frame-outputs.md).
:::

:::{note}
`config/demo/demo.cfg` is a smaller example, but it enables no frame outputs at all, so an exposure
against it returns `DONE` and writes nothing. It is a starting point for a config, not a working
demonstration.
:::

## Where things went

| What | Where |
|---|---|
| FITS files | `FITS_OUTPUT_DIR` |
| Log file | a daily file under `LOGPATH` |
| Shared-memory stream | `SHM_DIR`, or `MILK_SHM_DIR`, or `/milk/shm` |

Note that `IMDIR` and `BASENAME` appear in the shipped configs but are read by nothing. Output
location is set entirely by the [frame output keys](../configuration/frame-outputs.md).

## Next

- What the commands are: [command reference](../commands/index.md)
- Drive the camera from Python instead of a socket: [Python bindings](../python/index.md)
- What the emulator does and does not model: [emulator](../emulator/index.md)
- Configure a real instrument: [instruments](../instruments/index.md)
