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

   *Replace `<file.cfg>` with an appropriate configuration file. See the example `.cfg` files in the `Config` and `Config/demo` directories.*

6. **(Optional) Run the Archon Emulator:**

    ```bash
    $ ../bin/emulator <file.cfg>
    ```

   *Note: The emulator software will only be compiled when `INTERFACE_TYPE` is set to Archon (default).*

7. **(Optional) Run Unit Tests:**

    ```bash
    $ ../bin/run_unit_tests
    ```

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