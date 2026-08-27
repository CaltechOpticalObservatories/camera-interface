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

3. **Create the Makefile by running CMake** (from the build directory):

   | Archon                 | ARC                              |
   |------------------------|----------------------------------|
   | `$ cmake ..`           | `$ cmake -DINTERFACE_TYPE=AstroCam ..` |

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

4. **Compile the sources:**

    ```bash
    $ make
    ```

5. **Run the Camera Server:**

    - **As a foreground process:**

        ```bash
        $ ../bin/camerad <file.cfg> --foreground
        ```

    - **As a daemon:**

        ```bash
        $ ../bin/camerad -d <file.cfg>
        ```

   *Replace `<file.cfg>` with an appropriate configuration file. See the example `.cfg` files in the `config` directory (per-instrument deployment configs live in each instrument's own repo under its `config/` directory; `config/demo` here is a generic example).*

6. **(Optional) Run the Archon Emulator:**

    ```bash
    $ ../bin/emulator <file.cfg>
    ```

   *Note: The emulator software will only be compiled when `INTERFACE_TYPE` is set to Archon (default).*

7. **(Optional) Run Unit Tests:**

    ```bash
    $ ../bin/run_unit_tests
    ```

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
| `SHM_MAX_FRAME_BYTES`   | `67108864` (64 MiB) | Validation ceiling for a frame's byte size; a frame larger than this is rejected rather than written. The default comfortably covers any detector geometry realistic for this codebase; set explicitly for a tighter bound. |
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