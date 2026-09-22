# HISPEC tracking camera

The HISPEC acquisition and tracking camera (ATC): an H2RG on an Archon controller. This is the most
complete instrument module and the one the emulator integration tests exercise.

Repository: [hispec-tracking-camera-instrument](https://github.com/CaltechOpticalObservatories/hispec-tracking-camera-instrument),
checked out at `camerad/Instruments/hispec_tracking_camera`.

:::{note}
Expands in M4 with the readout and operational modes, the ROI and guiding geometry rules, and the
full keyword table. What is here is verified against the current submodule.
:::

## Build

```bash
cd build
cmake -DCONTROLLER=archon -DINSTRUMENT=hispec_tracking_camera ..
make
```

Shipped configuration is in the submodule's `config/`: `hispecatc.cfg` and `hispecatc.acf`.

## Instrument commands

These are reached through the normal command interface, and from Python via `instrument_cmd()`.

| Command | Purpose |
|---|---|
| `h2rg_init` | Initialize the H2RG |
| `mode` | Select the readout mode |
| `exposure` | Select the exposure mode |
| `autofetch_mode` | Control autofetch, where the controller pushes frames continuously |
| `freerun` | Continuous acquisition |
| `window_mode` | Windowed readout |
| `roi` | Set the region of interest |
| `take_stats` | Report pixel statistics |
| `debug` | Development diagnostics |

`instrument_commands()` enumerates them at runtime, which is the authoritative list for a given
build.

## Readout modes

`mode` selects among the H2RG readout schemes, each backed by an ACF timing mode:

| Mode | ACF timing mode |
|---|---|
| `utr_rr` | `mode_UTR_RR`, up-the-ramp, reset-read |
| `utr_gr` | `mode_UTR_GR`, up-the-ramp, guided read |
| `rx` | `mode_RX`, reset-execute |
| `rxr` | `mode_RXR`, reset-execute-read |

## FITS keywords

The module carries its own keyword dictionary
({source}`camerad/Instruments/hispec_tracking_camera/fits_header_dictionary.cpp`) mapping each
internal property to a keyword, comment, type and default. It covers two cameras, ATC and SPEC, with
separate defaults.

The generated keyword table lands here in M2. `python/tests/fits_header_check.py` validates a
written file against this dictionary.
