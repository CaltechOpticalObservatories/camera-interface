# Instruments

An instrument module adapts the core to one detector: its exposure modes, its extra commands, its
FITS keywords. One is chosen at build time with `-DINSTRUMENT=`, and a given binary serves exactly
one instrument.

Each lives in its own repository, pulled in as a submodule under `camerad/Instruments/`. The
submodule is pinned, so a given `camera-interface` commit builds one specific instrument revision.

## Status

| Instrument | Controller | Detector | State |
|---|---|---|---|
| [hispec_tracking_camera](hispec-tracking-camera.md) | Archon | H2RG | In active use. The reference implementation. |
| [cryoscope](cryoscope.md) | Archon | H2RG | Scaffolding, and does not currently build |
| `hispec` | Archon | | Repository exists, no sources yet |
| `deimos` | | | Repository exists, no sources yet |

:::{note}
`hispec` and `deimos` are README-only submodules today. They are listed so the set is not misleading
about what exists; there is nothing to document until they carry sources.

Only `hispec_tracking_camera` is built in CI, so the others can fall behind changes to the core
without anything noticing.
:::

```{toctree}
:hidden:

hispec-tracking-camera
cryoscope
```

## Building one

```bash
git submodule update --init camerad/Instruments/<name>
cd build
cmake -DCONTROLLER=archon -DINSTRUMENT=<name> ..
make
```

The instrument name is also checked against the `INSTRUMENT` key in the `.cfg`, so a config cannot
be pointed at a binary built for a different camera.

For how a module plugs into the core, see [architecture](../architecture/index.md).
