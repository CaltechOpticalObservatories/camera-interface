# Camera Interface

Camera Detector Controller Interface Software

## Reporting Issues

If you encounter any problems or have questions about this project, please open an issue on the [GitHub Issues page](https://github.com/CaltechOpticalObservatories/camera-interface/issues). Your feedback helps us improve the project!

## Requirements

- **CMake** 3.12 or higher
- **cfitsio** and **CCFits** libraries (expected in `/usr/local/lib`)
- **gtest** (Google Test) library (needed to run unit tests)

### Controller Compatibility

| Archon Controllers                  | ARC Controllers                              |
|------------------------------------|----------------------------------------------|
| `g++ 8.1` or higher (and C++17)    | `g++ 8.3` (and C++17)                        |
|                                    | ARC API 3.6 and Arc66PCIe driver             |

## Build Instructions

1. **Change to the build directory:**

    ```bash
    $ cd build
    ```

2. **Start with a clean build:** Delete the contents of the build directory, including the `CMakeFiles/` subdirectory, but **not** the `.gitignore` file.

    ```bash
    $ rm -Rf *
    ```

3. **Create the Makefile by running CMake** (from the build directory). `-DCONTROLLER=` is required; CMake stops with an error if it is missing:

   | Archon                            | ARC                                  |
   |-----------------------------------|--------------------------------------|
   | `$ cmake -DCONTROLLER=archon ..`  | `$ cmake -DCONTROLLER=astrocam ..`   |

   Add `-DINSTRUMENT=` to build an instrument module, whose sources come from `camerad/Instruments/<name>`:

    ```bash
    $ cmake -DCONTROLLER=archon -DINSTRUMENT=hispec_tracking_camera ..
    ```

   To enable the shared-memory output (`SHM_ENABLED` in a `.cfg` file, see [Frame Outputs](#frame-outputs) below), add `-DENABLE_SHM_OUTPUT=ON -DImageStreamIO_DIR=<prefix>/lib/cmake`:

    ```bash
    $ cmake -DENABLE_SHM_OUTPUT=ON -DImageStreamIO_DIR=/usr/local/lib/cmake ..
    ```

   This requires [ImageStreamIO](https://github.com/milk-org/ImageStreamIO) to already be built and installed, since it isn't packaged for common distros:

    ```bash
    $ git clone https://github.com/milk-org/ImageStreamIO.git
    $ cd ImageStreamIO && mkdir build && cd build
    $ cmake ..
    $ make
    $ sudo make install
    ```

   ImageStreamIO's own `Config.cmake` files install directly under `<prefix>/lib/cmake/` rather than the CMake-conventional `<prefix>/lib/cmake/ImageStreamIO/`, so `-DImageStreamIO_DIR=...` must always be given explicitly, even for a standard system-wide install.

   To build the Python module (see [Python Module](#python-module) below), add `-DBUILD_PYTHON_MODULE=ON`. It is off by default, so builds that don't want it never need pybind11:

    ```bash
    $ pip install pybind11
    $ cmake -DBUILD_PYTHON_MODULE=ON ..
    ```

   pybind11 is located by asking the interpreter CMake selected, so pass `-DPython3_EXECUTABLE=...` to build against a specific one (a virtualenv, say). The module and that interpreter then always agree on the ABI.

   pybind11 is header-only and needed only to compile: the built module links cfitsio, CCfits and OpenCV but not pybind11, so it does not have to be present where the module is imported.

4. **Compile the sources:**

    ```bash
    $ make
    ```

5. **Run the Camera Server:**

   The configuration file is passed with `--config` and is required.

    - **As a foreground process**, logging to the console as well as to `LOGPATH`:

        ```bash
        $ ../bin/camerad --foreground --config <file.cfg>
        ```

    - **As a daemon**, which is the default without `--foreground`:

        ```bash
        $ ../bin/camerad --config <file.cfg>
        ```

   *Replace `<file.cfg>` with an appropriate configuration file. See the example `.cfg` files in the `config` directory (per-instrument deployment configs live in each instrument's own repo under its `config/` directory; `config/demo` here is a generic example).*

   Logging always goes to a daily file under `LOGPATH`. Whether it is also written to stderr follows `--foreground`, so an operator watching a console sees it and a daemon does not duplicate its whole log into the stderr redirect. `LOG_STDERR` in the `.cfg` overrides that either way.

6. **(Optional) Run the Archon Emulator:**

    ```bash
    $ ../bin/emulator <file.cfg> -i <instrument>
    ```

   The emulator reads `EMULATOR_PORT` and `EMULATOR_SYSTEM` from the same `.cfg` the server uses, so point `ARCHON_IP`/`ARCHON_PORT` at it to run without hardware. `-i generic` suits the shipped test configs. It is built only for `-DCONTROLLER=archon`.

7. **(Optional) Run Unit Tests.** The tests are excluded from the default target, so build them first:

    ```bash
    $ make run_unit_tests
    $ ../bin/run_unit_tests
    ```

## Python Module

Built with `-DBUILD_PYTHON_MODULE=ON`, `camera_interface` lets a Python process own a camera directly, with no `camerad` process and no text protocol in between. It performs the same startup `camerad` does, then exposes the interface as methods:

```python
import camera_interface

camera = camera_interface.Camera("hispecatc.cfg")
camera.open()
camera.load()
camera.power("on")
camera.exptime("0")
camera.expose("1")

print(camera.instrument_commands())   # this build's instrument-specific commands
camera.instrument_cmd("roi", "51 60 51 60")
print(camera.output_status())         # frames written, dropped, last file
```

Every command `camerad` accepts is reachable: the base commands are bound as methods, instrument-specific ones go through `instrument_cmd()` (enumerable with `instrument_commands()`), and controller-specific ones such as `mode`, `raw`, `readacf`, `loadtiming`, `heater` and `sensor` go through `controller_cmd()`. Only `exit` is omitted, since the process belongs to the caller.

The controller and instrument are fixed at CMake configure time, so `instrument_name()` and `controller_name()` report which build was loaded. A failed command raises `RuntimeError`.

`output_status()` is a snapshot, never a barrier: the FITS writer queues and drops frames by design because disk is slower than acquisition can be, so nothing here lets a caller stall acquisition by waiting on an output. Anything needing to be woken per frame should attach to the shared-memory segment, which posts semaphores.

Commands release the GIL while they run, so a blocking `expose()` leaves the rest of the process responsive.

Logging follows `LOG_STDERR` from the `.cfg`; pass `log_to_stderr=` to override it per session. The C++ log always goes to its daily file under `LOGPATH`.

## Frame Outputs

Every instrument publishes each acquired frame to one or more outputs, configured entirely via `.cfg` file keys (`Camera::Interface::configure_frame_outputs()` builds them from `Camera::apply_config_overrides()`, called once at startup for every instrument, not just HISPEC). Both outputs are independent; either, both, or neither can be enabled per instrument.

### FITS

Writes one FITS file per frame asynchronously (a queue plus a dedicated writer thread, so the readout thread never blocks on disk I/O).

| Key                      | Default        | Meaning                                                              |
|--------------------------|----------------|-----------------------------------------------------------------------|
| `FITS_ENABLED`           | `no`           | Enable the FITS writer                                                |
| `FITS_OUTPUT_DIR`        | `/tmp/images`  | Base directory for FITS files; must already exist                     |
| `FITS_AUTODIR`           | `no`           | Write into a `YYYYMMDD` subdirectory of `FITS_OUTPUT_DIR`              |
| `FITS_BASENAME`          | `tracking`     | Base filename for FITS files                                          |
| `FITS_QUEUE_SIZE`        | `32`           | Max frames buffered for the writer thread; oldest is dropped if full  |
| `FITS_DRAIN_TIMEOUT_MS`  | `5000`         | On shutdown, how long to keep draining the queue before giving up     |

### Shared Memory (ImageStreamIO)

Publishes each frame as an [ImageStreamIO](https://github.com/milk-org/ImageStreamIO) shared-memory stream, readable by AO frameworks like [cacao](https://github.com/cacao-org/cacao). Requires building with `-DENABLE_SHM_OUTPUT=ON` (see Build Instructions above); if a `.cfg` file sets `SHM_ENABLED=yes` on a build compiled without that flag, `camerad` logs a warning and skips it rather than failing.

| Key                     | Default    | Meaning                                                                                   |
|-------------------------|------------|--------------------------------------------------------------------------------------------|
| `SHM_ENABLED`           | `no`       | Enable the shared-memory writer                                                            |
| `SHM_SEGMENT_NAME`      | `camera`   | ImageStreamIO stream name                                                                  |
| `SHM_RING_BUFFER_SIZE`  | `4`        | Depth of ImageStreamIO's internal history ring buffer (`CBsize`); the live frame a real-time reader sees is separate from this |
| `SHM_DIR`               | (unset)    | Base directory ImageStreamIO writes into. If unset, ImageStreamIO falls back to its own default resolution (`MILK_SHM_DIR` env var, then `/milk/shm`). If set, it must already exist and be writable. |

Frame geometry (width/height/pixel depth) isn't a config key: it's fixed for an ImageStreamIO stream's whole life, so the writer (re)creates the stream automatically whenever it sees the geometry change from what's currently allocated.

## Heater & Sensor Control

For Archon **Heater** and **HeaterX** modules, the server exposes commands to
control the closed-loop heaters and read/control the on-board temperature
sensors. These require firmware to be loaded and a sufficiently recent Archon
backplane.

### `heater`

Control heater `A` or `B` on the given module: enable state and target, PID
parameters, ramp, current limit, and input sensor.

```
heater <module> <A|B> [ <on|off> [target] | <target> | PID [<p> <i> <d>]
                        | RAMP [<on|off> [rate]] | ILIM [val] | INPUT [A|B|C] ]
```

| Form                                   | Effect                                                  |
|----------------------------------------|---------------------------------------------------------|
| `heater <module> <A\|B>`                | get enable state and target                             |
| `heater <module> <A\|B> <on\|off> [target]` | set enable state, optionally the target            |
| `heater <module> <A\|B> <target>`       | set the target (range depends on backplane version)     |
| `heater <module> <A\|B> PID [<p> <i> <d>]` | get/set the P, I, D parameters (`0`–`10000` each)    |
| `heater <module> <A\|B> RAMP [<on\|off> [rate]]` | get/set ramp enable and ramprate (`1`–`32767`)  |
| `heater <module> <A\|B> ILIM [val]`     | get/set the current limit (`0`–`10000`)                 |
| `heater <module> <A\|B> INPUT [A\|B\|C]` | get/set the input sensor (`C` requires HeaterX)        |

The target range defaults to backplane-version-dependent limits and can be
overridden in the `.cfg` file with `HEATER_TARGET_MIN` / `HEATER_TARGET_MAX`
(degrees C).

### `sensor`

Set or get a temperature sensor's RTD excitation current and digital averaging.

```
sensor <module> <A|B|C> [ <current> | AVG [ <N> ] ]
```

| Form                              | Effect                                                        |
|-----------------------------------|--------------------------------------------------------------|
| `sensor <module> <A\|B\|C>`        | get the excitation current (nano-amps)                       |
| `sensor <module> <A\|B\|C> <current>` | set the excitation current, `0`–`1600000` nA             |
| `sensor <module> <A\|B\|C> AVG`    | get the digital averaging count                              |
| `sensor <module> <A\|B\|C> AVG <N>`| set the digital averaging count `N` ∈ {1,2,4,8,…,256}        |

Sensor `C` is available only on **HeaterX** modules.

---

David Hale  
<dhale@astro.caltech.edu>