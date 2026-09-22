# Configuration reference

`camerad` reads a single plain-text configuration file, named with `--config`. By convention it ends
in `.cfg`, but nothing enforces that.

## File format

One key per line, with an optional trailing comment:

```
KEY=VALUE            # optional comment
```

Keys that take several values are written as an indexed array, repeating the key:

```
DEFAULT_FIRMWARE=(0 /home/dsp/E2V4240/tim.lod)
DEFAULT_FIRMWARE=(1 /home/dsp/E2V4240/tim.lod)
```

Anything after `#` is ignored.

When the server runs as a daemon, the file is re-read on `SIGHUP`. Not every key takes effect on
reload: some are read once at startup, and some are only defaults that a command can override at
runtime. The key tables note which is which.

## Key tables

```{toctree}
:maxdepth: 1

core
frame-outputs
exposure-time
```

:::{note}
The key tables are generated from the source at documentation build time and cross-checked against
the keys `camerad` actually honours, so a key added to the code without a description here fails the
build. See [development](../development/index.md).
:::

## Build options

These are CMake options, fixed when the software is compiled, not `.cfg` keys.

| Option | Default | Meaning |
|---|---|---|
| `CONTROLLER` | none, required | `archon` or `astrocam`. CMake errors out if unset. |
| `INSTRUMENT` | none | Instrument module to build, from `camerad/Instruments/<name>` |
| `INTERFACE_TYPE` | `Archon` | Selects which emulator is built, independently of `CONTROLLER`. `AstroCam` builds none, since none exists for ARC. |
| `ENABLE_SHM_OUTPUT` | `OFF` | Build the ImageStreamIO shared-memory output. Also needs `-DImageStreamIO_DIR=<prefix>/lib/cmake`. |
| `BUILD_PYTHON_MODULE` | `OFF` | Build the `camera_interface` Python module. Needs pybind11 at compile time only. |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Where `make install` puts binaries and the module |

`ENABLE_SHM_OUTPUT` and `SHM_ENABLED` are separate gates: a `.cfg` that sets `SHM_ENABLED=yes` on a
build compiled without `ENABLE_SHM_OUTPUT` logs a warning and carries on without shared memory,
rather than failing to start.
