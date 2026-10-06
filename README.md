# Camera Interface

Camera Detector Controller Interface Software

## Documentation

Full documentation is at
<https://caltechopticalobservatories.github.io/camera-interface/>, covering the architecture, the
command and configuration references, the instruments, the emulator and the Python bindings. This
README stays focused on building and running; the site is the reference.

## Reporting Issues

If you encounter any problems or have questions about this project, please open an issue on the [GitHub Issues page](https://github.com/CaltechOpticalObservatories/camera-interface/issues). Your feedback helps us improve the project!

## Requirements

- **CMake** 3.24 or higher
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

   `-DINTERFACE_TYPE=` is separate from `-DCONTROLLER=` and selects only which emulator is built. It defaults to `Archon`; `-DINTERFACE_TYPE=AstroCam` skips the emulator, since none is implemented for ARC.

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

   To build the Python module (see [Python Module](#python-module) below), add `-DBUILD_PYTHON_MODULE=ON`. It is off by default, so builds that don't want it never need pybind11. To install it into an environment rather than build it here, see [Installing with pip](#installing-with-pip) instead, which fetches pybind11 itself:

    ```bash
    $ pip install pybind11
    $ cmake -DBUILD_PYTHON_MODULE=ON ..
    ```

   pybind11 is located by asking the interpreter CMake selected, so pass `-DPython3_EXECUTABLE=...` to build against a specific one (a virtualenv, say). The module and that interpreter then always agree on the ABI.

   pybind11 is header-only and needed only to compile: the built module links cfitsio and CCfits but not pybind11, so it does not have to be present where the module is imported.

4. **Compile the sources:**

    ```bash
    $ make
    ```

5. **(Optional) Install:** `make install` copies what the build produced under
   `${CMAKE_INSTALL_PREFIX}`, which defaults to `/usr/local`:

    ```bash
    $ cmake -DCONTROLLER=archon -DCMAKE_INSTALL_PREFIX=$HOME/.local ..
    $ make && make install
    ```

   | Artifact             | Installed to | Built when               |
   |----------------------|--------------|--------------------------|
   | `camerad`            | `bin`        | always                   |
   | `camerad-socksend`   | `bin`        | always                   |
   | `camerad-emulator`   | `bin`        | `-DINTERFACE_TYPE=Archon` (the default) |
   | `camerad-shm-reader` | `bin`        | `-DENABLE_SHM_OUTPUT=ON` |
   | `camera_interface`   | `lib`        | `-DBUILD_PYTHON_MODULE=ON` |

   The tools carry a `camerad-` prefix because names like `socksend` are too
   generic for a directory shared with every other package.

   The Python module installs to `lib`, so importing it means putting that
   directory on `PYTHONPATH`:

    ```bash
    $ PYTHONPATH=$HOME/.local/lib python3 -c "import camera_interface"
    ```

   Without installing, the binaries are only ever run from `bin/` in the source
   tree, so a rebuild replaces whatever is deployed and there is no way to keep
   two versions or to tell which one is running.

6. **Run the Camera Server:**

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

7. **(Optional) Run the Archon Emulator:**

    ```bash
    $ ../bin/camerad-emulator <file.cfg> -i <instrument>
    ```

   The emulator reads `EMULATOR_PORT` and `EMULATOR_SYSTEM` from the same `.cfg` the server uses, so point `ARCHON_IP`/`ARCHON_PORT` at it to run without hardware. `-i generic` suits the shipped test configs.

8. **(Optional) Run Unit Tests.** The tests are excluded from the default target, so build them first:

    ```bash
    $ make run_unit_tests
    $ ../bin/run_unit_tests
    ```

9. **(Optional) Check the FITS headers** of a file the writer produced, against the instrument's header definition:

    ```bash
    $ python3 python/tests/fits_header_check.py <file.fits> --exptime <sec>
    ```

   Asserts every expected keyword is present and carries the value the emulator's `MODE_DEFAULT` implies, so a keyword that stops being populated fails rather than going unnoticed. Needs no FITS library. Both emulator CI jobs run it after their exposure.

### Composing the core

An instrument's build composes the core as a subproject (CMake `FetchContent`) and declares its own `camerad` and Python module. The core declares those two only when it is the top-level project. Link only the `camerad::` names:

- `camerad::base` — the `Interface` base class and image processing
- `camerad::archon` — the controller library, named after `CONTROLLER`
- `camerad::daemon` — the daemon's `main` and its server
- `camerad::python` — the module's source, compiled inside the target that links it

```cmake
set(CONTROLLER archon CACHE STRING "")
set(BUILD_PYTHON_MODULE ON CACHE BOOL "")
FetchContent_Declare(camerad GIT_REPOSITORY <url> GIT_TAG <version>)
FetchContent_MakeAvailable(camerad)

add_executable(myinst_camerad myinst_interface_factory.cpp myinst_instrument.cpp)
target_link_libraries(myinst_camerad camerad::daemon camerad::archon)
set_target_properties(myinst_camerad PROPERTIES OUTPUT_NAME camerad)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
# pybind11 must come from the interpreter found above. A pip-installed one is
# not on CMake's search path, and CMake may silently find a system copy
# instead: pass -Dpybind11_DIR=$(<that python> -m pybind11 --cmakedir), or
# ask ${Python3_EXECUTABLE} as python/CMakeLists.txt does
find_package(pybind11 CONFIG REQUIRED)
pybind11_add_module(camera_interface myinst_interface_factory.cpp myinst_instrument.cpp)
target_link_libraries(camera_interface PRIVATE camerad::python camerad::archon)
target_compile_definitions(camera_interface PRIVATE CAMERAD_INSTRUMENT_NAME="myinst")
```

- Each final target compiles exactly one interface factory definition. The linker rejects zero or two for the daemon. A module with none still links and fails only when imported, so the instrument's tests import it and check `instrument_name()`.
- `BUILD_PYTHON_MODULE=ON` is needed for a module: it declares `camerad::python` and makes the core's static libraries position-independent.
- The module's file name is `camera_interface`, the import name fixed in its source.
- Without `CAMERAD_INSTRUMENT_NAME` the module reports `"none"`.
- Finding Python and pybind11 is the consumer's job; the core does neither as a subproject. `python/CMakeLists.txt` shows one way.

An instrument repository that installs its module puts it inside a Python package of its own, e.g. `myinst/camera_interface…so` with an `__init__.py` that re-exports it, so the module keeps its name and both `import myinst` and `import myinst.camera_interface` work. It names its daemon and emulator for the package, e.g. `camerad-myinst` and `camerad-emulator-myinst`, so that installing it never replaces another package's files.

## Installing with pip

`pip install` builds the same artifacts and places them in the target environment, so `import camera_interface` needs no `PYTHONPATH` and `camerad` is on `PATH` whenever the environment is active:

```bash
$ pip install ./camera-interface \
    --config-settings=cmake.define.INSTRUMENT=hispec_tracking_camera
```

Any CMake option can be passed the same way, so `--config-settings=cmake.define.ENABLE_SHM_OUTPUT=ON` works as well. `CONTROLLER` defaults to `archon` and `BUILD_PYTHON_MODULE` is forced on.

pybind11 comes from `[build-system] requires`, so pip fetches it into an isolated build environment. It is never installed into the environment being built for.

The controller and instrument are fixed when the wheel is built, so a caller should assert which build it loaded:

```python
assert camera_interface.instrument_name() == "hispec_tracking_camera"
```

A compiler and the full dependency set have to be present wherever `pip install` runs, since it compiles camerad and the module from source.

### Instrument packages

An instrument package can share an environment with the generic package and with other instruments. Its build sets `CAMERAD_PYTHON_PACKAGE` to the package's name, e.g. in a `pyproject.toml` in the instrument's own repository that builds the core from its checkout:

```toml
[tool.scikit-build]
cmake.source-dir = "external/camera-interface"
wheel.packages = []
cmake.define.BUILD_PYTHON_MODULE = "ON"
cmake.define.CONTROLLER = "archon"
cmake.define.INSTRUMENT = "myinst"
cmake.define.CAMERAD_PYTHON_PACKAGE = "myinst_camera"
```

That package installs:

- `myinst_camera/camera_interface…so` and `myinst_camera/__init__.py`, imported as `import myinst_camera` or `import myinst_camera.camera_interface`;
- `camerad-myinst-camera` and `camerad-emulator-myinst-camera`, named for the package with `_` replaced by `-`.

It does not install `camerad-socksend` or `camerad-shm-reader`, which come with the generic package. The generic package is unchanged: `import camera_interface`, plus `camerad`, `camerad-emulator` and `camerad-socksend`. Uninstalling one package leaves the others' files in place.

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

Every build's module is named `camera_interface`. The generic build installs it at the top level, and an instrument package installs it inside a package of its own ([Instrument packages](#instrument-packages)), so several instruments can share one environment. Importing several instruments into one process is intended to be supported but is not yet tested; for now, use one instrument per process.

`output_status()` is a snapshot, never a barrier: the FITS writer queues and drops frames by design because disk is slower than acquisition can be, so nothing here lets a caller stall acquisition by waiting on an output. Anything needing to be woken per frame should attach to the shared-memory segment, which posts semaphores.

Commands release the GIL while they run, so a blocking `expose()` leaves the rest of the process responsive.

Logging follows `LOG_STDERR` from the `.cfg`; pass `log_to_stderr=` to override it per session. The C++ log always goes to its daily file under `LOGPATH`.

`python/examples/raw_channel_sweep.py` uses the module to capture a pre-CDS waveform from several channels. The Archon covers one channel per exposure, so the script loops `RAWSEL`, exposes and fetches, leaving one RAW file per channel next to each paired image.

```bash
$ python python/examples/raw_channel_sweep.py --config lris2.cfg --fits-dir /tmp/images --channels 0-3,18-21
```

## Frame Outputs

Every instrument publishes each acquired frame to one or more outputs, configured entirely via `.cfg` file keys (`Camera::Interface::configure_frame_outputs()` builds them from `Camera::apply_config_overrides()`, called once at startup for every instrument, not just HISPEC). Both outputs are independent; either, both, or neither can be enabled per instrument.

### Streams

Every frame carries an optional stream name so that one exposure can deliver outputs of different geometry without them colliding. The primary image leaves it empty; an Archon `raw read` sets it to `raw`. FITS appends the name to the filename (`image_00000123_raw.fits`) and shared memory appends it to the segment name (`camera_raw`), so a RAW capture neither overwrites the image file nor resizes the image stream.

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

Frame geometry (width/height/pixel depth) isn't a config key: it's fixed for an ImageStreamIO stream's whole life, so the writer (re)creates a stream automatically whenever it sees the geometry change from what's currently allocated for that stream.

Two readers ship with the repo. `camerad-shm-reader` prints geometry, keywords and pixel statistics once, for diagnostics. `python/examples/shm_read_frames.py` is a sample streaming consumer: it blocks on the stream's semaphore and reports every frame as it arrives, flagging any it missed.

```bash
$ python python/examples/shm_read_frames.py --segment hispec_tracking_camera --count 10
```

It needs numpy and `ImageStreamIOWrap`, the Python wrapper from the ImageStreamIO source tree, built with `-DPYTHON_WRAPPER=ON`; the wrapper is not on PyPI.

## Exposure Time

`exptime [ <time> [ s | ms ] ]` sets or reports the exposure time. Without a unit on the argument, the value is in whatever `LONGEXPOSURE` selects, and the value reported back uses that same unit. A unit on the argument overrides it for that one command, so `exptime 500 ms` is unambiguous whichever way the instrument is configured.

| Key                   | Default | Meaning                                                            |
|-----------------------|---------|--------------------------------------------------------------------|
| `LONGEXPOSURE`        | `true`  | `true`: `exptime` arguments are seconds; `false`: milliseconds      |
| `EXPTIME_MSEC_PARAM`  | none    | Archon parameter holding the milliseconds part; required            |
| `EXPTIME_SEC_PARAM`   | none    | Archon parameter holding the whole-seconds part; optional           |

Internally the exposure time is always seconds, which is also the unit of the `EXPTIME` FITS keyword, so the configured unit never changes what is archived. Archon parameters are 20 bits, so without `EXPTIME_SEC_PARAM` the longest exposure is 2^20 msec (about 1048 sec); beyond that `exptime` returns an error rather than leaving the controller and the header disagreeing.

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
