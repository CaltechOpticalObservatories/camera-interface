# Instruments

An instrument module adapts the core to one detector: its exposure modes, its extra commands, its
FITS keywords. A given binary serves exactly one instrument.

Each lives in its own repository and builds there, pulling in `camera-interface` with
`FetchContent`. The instrument pins the core, so a given instrument commit builds against one
specific core revision. Each instrument documents itself in its own repository.

:::{note}
The core's CI builds only {source}`reference_instrument`, so an instrument can fall behind changes
to the core without anything here noticing.
:::

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
