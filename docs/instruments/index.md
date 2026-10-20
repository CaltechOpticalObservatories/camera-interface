# Instruments

An instrument module adapts the core to one detector: its exposure modes, its extra commands, its
FITS keywords. A given binary serves exactly one instrument.

Each lives in its own repository and builds there, pulling in `camera-interface` with
`FetchContent`. The instrument pins the core, so a given instrument commit builds against one
specific core revision.

## Status

| Instrument | Controller | Detector | State |
|---|---|---|---|
| [hispec_tracking_camera](hispec-tracking-camera.md) | Archon | H2RG | In active use. The reference implementation. |
| [cryoscope](cryoscope.md) | Archon | H2RG | Scaffolding, and does not currently build |
| `hispec` | Archon | | Repository exists, no sources yet |
| `deimos` | | | Repository exists, no sources yet |

:::{note}
`hispec` and `deimos` are README-only repositories today. They are listed so the set is not misleading
about what exists; there is nothing to document until they carry sources.

The core's CI builds only {source}`reference_instrument`, so an instrument can fall behind changes
to the core without anything here noticing.
:::

```{toctree}
:hidden:

hispec-tracking-camera
cryoscope
```

## Building one

From the instrument's own checkout:

```bash
cmake -S . -B build
cmake --build build
```

{source}`reference_instrument` is the template for a new one, including its `CMakeLists.txt`.

The instrument name is also checked against the `INSTRUMENT` key in the `.cfg`, so a config cannot
be pointed at a binary built for a different camera.

For how a module plugs into the core, see [architecture](../architecture/index.md).
