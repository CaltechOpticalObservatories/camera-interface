# CryoScope

An H2RG on an Archon controller, reading in RXR mode.

Repository: [cryoscope-instrument](https://github.com/CaltechOpticalObservatories/cryoscope-instrument),
checked out at `camerad/Instruments/cryoscope`.

:::{note}
Expands in M4. The module defines a `CryoScope` interface deriving from `ArchonInterface`, with its
own exposure modes, and registers no additional instrument commands beyond the base set.
:::

## Build

```bash
cd build
cmake -DCONTROLLER=archon -DINSTRUMENT=cryoscope ..
make
```
