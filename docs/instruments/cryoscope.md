# CryoScope

An H2RG on an Archon controller, reading in RXR mode.

Repository: [cryoscope-instrument](https://github.com/CaltechOpticalObservatories/cryoscope-instrument).

:::{warning}
This module does not currently build. It has no `CMakeLists.txt` of its own, only a `cryoscope.cmake`
for the retired in-core build. Its exposure mode also instantiates `ExposureModeTemplate` with two
template parameters where the base declares one, and assigns to `modetype` and `modeargs`, which
are named `type` and `args` in {source}`camerad/exposure_modes.h`.
:::

## What it defines

`CryoScope` derives from `ArchonInterface` and overrides `instrument_cmd`,
`configure_instrument`, `power`, `get_exposure_modes` and `set_exposure_mode`, plus a private
`setup_detector()`.

It registers no instrument-specific commands beyond the base set, and its exposure mode is still a
placeholder named `XXX`.

## Configuration

| Key | Meaning |
|---|---|
| `START_PARAM` | Archon parameter used to start the timing script |

`START_PARAM` is read only by this module, which is why it does not appear in the
[core key tables](../configuration/core.md).
