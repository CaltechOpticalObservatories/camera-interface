# CryoScope

An H2RG on an Archon controller, reading in RXR mode.

Repository: [cryoscope-instrument](https://github.com/CaltechOpticalObservatories/cryoscope-instrument),
checked out at `camerad/Instruments/cryoscope`.

:::{warning}
This module does not currently build against the core. Its exposure mode instantiates
`ExposureModeTemplate` with two template parameters where the base declares one, and assigns to
`modetype` and `modeargs`, which are named `type` and `args` in
{source}`camerad/exposure_modes.h`. It was left behind by a refactor of the exposure mode base
class.

CI does not catch this, because the build workflow compiles the default target and only builds an
instrument when one is named explicitly.
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

## Build

```bash
cd build
cmake -DCONTROLLER=archon -DINSTRUMENT=cryoscope ..
make
```
