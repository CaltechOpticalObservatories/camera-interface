# Getting started

This walks from a clean checkout to a FITS file on disk, with no detector controller attached: the
Archon emulator stands in for the hardware.

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

`-DCONTROLLER=` is required; CMake stops with an error without it. `-DINSTRUMENT=` is optional and
selects an instrument module from `camerad/Instruments/<name>`.

```bash
git clone --recurse-submodules \
  https://github.com/CaltechOpticalObservatories/camera-interface.git
cd camera-interface/build
cmake -DCONTROLLER=archon -DINSTRUMENT=hispec_tracking_camera ..
make
```

Binaries land in `bin/` in the source tree. `make install` copies them under
`CMAKE_INSTALL_PREFIX` instead, which is the better choice for anything deployed.

The build options are covered in full under [configuration](../configuration/index.md); the ones that
change what gets built are `ENABLE_SHM_OUTPUT`, `BUILD_PYTHON_MODULE` and `INTERFACE_TYPE`.

:::{note}
Instrument modules are git submodules. A clone without `--recurse-submodules` leaves
`camerad/Instruments/<name>` empty and `-DINSTRUMENT=` will fail. Fix it with
`git submodule update --init --recursive`.
:::

## Run the emulator

The emulator reads `EMULATOR_PORT` and `EMULATOR_SYSTEM` from the same `.cfg` the server uses, so
pointing `ARCHON_IP` and `ARCHON_PORT` at it is all that separates a test rig from real hardware. The
shipped `config/demo/demo.cfg` already does this.

```bash
bin/camerad-emulator config/demo/demo.cfg -i generic
```

## Run the server

```bash
bin/camerad --foreground --config config/demo/demo.cfg
```

`--config` is required. Without `--foreground` the server daemonizes. Logging always goes to a daily
file under `LOGPATH`; `--foreground` additionally writes it to stderr.

## Take an exposure

`camerad-socksend` sends one command and prints the reply. Point it at the blocking port from the
`.cfg` (`BLKPORT`).

```bash
send() { bin/camerad-socksend -p 3031 -t 60 "$1"; }

send "open"          # connect to the controller
send "load"          # load firmware named by DEFAULT_FIRMWARE
send "power on"
send "exptime 1.5"   # seconds, because this config leaves LONGEXPOSURE at its default
send "expose 1"
```

Each returns `DONE` or `ERROR`. The FITS file appears under `IMDIR`, named from `BASENAME`.

:::{warning}
`DONE` means the server accepted and completed the command, not that every frame output succeeded.
The FITS writer drops frames by design when the queue backs up, so `DONE` from `expose` is not a
promise that a file was written. [Frame outputs](../fits/index.md) explains how to check.
:::

## Next

- Drive the camera from Python instead of a socket: [Python bindings](../python/index.md)
- Understand what the emulator does and does not model: [Emulator](../emulator/index.md)
- Configure a real instrument: [Instruments](../instruments/index.md)
