# Emulator

`camerad-emulator` stands in for an Archon controller so the server, an instrument module and a
client can all be exercised with no hardware. CI uses it for the end-to-end tests.

:::{note}
Expands in M3 with the emulated command list and the limits of the emulation.
:::

There is no ARC emulator. `-DINTERFACE_TYPE=AstroCam` therefore builds none, and the default
`-DINTERFACE_TYPE=Archon` builds this one regardless of which `CONTROLLER` was selected.

## Running

```bash
camerad-emulator <file.cfg> -i <instrument>
```

It reads `EMULATOR_PORT` and `EMULATOR_SYSTEM` from the same `.cfg` the server uses, so the only
thing that makes a config point at the emulator rather than hardware is `ARCHON_IP` and
`ARCHON_PORT`. `-i generic` suits the shipped test configs.

`EMULATOR_SYSTEM` names a `.system` file describing the module complement to report, which is what
lets the emulator answer `SYSTEM` convincingly for a given instrument.

## What it models

The emulator answers the Archon command set the server uses: configuration load, parameter writes,
frame status, and pixel delivery on the timing the exposure implies.

Frames carry synthetic pixel data by default, so it validates plumbing, geometry, timing and headers
rather than image quality. Point `EMULATOR_DATADIR` at a directory of real frames to have it serve
those instead, which is what makes it useful for exercising downstream processing.

Sources are in {source}`emulator`, with the per-detector frame sources alongside
(`generic.h`, `nirc2.h`).

## In CI

Two workflow jobs run against it on every push and pull request:

- A frame outputs test that takes an exposure with both FITS and shared memory enabled, then
  validates the written header and reads the shared-memory segment back.
- A Python module test that drives the same exposure through the `camera_interface` bindings.

Both are in `.github/workflows/emulator-integration.yml` and are the closest thing to a
regression suite for the acquisition path.
