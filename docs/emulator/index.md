# Emulator

`camerad-emulator` impersonates an Archon controller over TCP, so the server, an instrument module
and a client can all be exercised with no hardware. CI uses it for the end-to-end tests.

There is no ARC emulator. `-DINTERFACE_TYPE=AstroCam` therefore builds none, and the default
`-DINTERFACE_TYPE=Archon` builds this one regardless of which `CONTROLLER` was selected.

## Running

```bash
camerad-emulator <file.cfg> -i <instrument>
```

It reads its keys from the same `.cfg` the server uses, so the only thing separating a test rig from
real hardware is where `ARCHON_IP` and `ARCHON_PORT` point. `-i generic` suits the shipped test
configs.

```{eval-rst}
.. camerad-config-keys:: Emulator
   :widths: 30 70
```

`EMULATOR_SYSTEM` names a `.system` file describing the module complement to report, which is what
lets the emulator answer `SYSTEM` convincingly for a given instrument. `config/demo/demo.system` is
the shipped example.

## What it emulates

The emulator answers the Archon command set the server actually uses:

Configuration
: `WCONFIG`, `RCONFIG`, `CLEARCONFIG`, `APPLYALL`, `APPLYMOD`, `APPLYDIO`

Parameters
: `LOADPARAM`, `PREPPARAM`, `FASTLOADPARAM`, `FASTPREPPARAM`

Timing and power
: `RESETTIMING`, `HOLDTIMING`, `RELEASETIMING`, `POWERON`, `POWEROFF`

Status and data
: `STATUS`, `SYSTEM`, `FRAME`, `TIMER`, `FETCH`, `LOCK`, `POLLON`, `POLLOFF`

It tracks configuration memory and parameters across `WCONFIG` and `LOADPARAM`, so a timing script
loaded by `load` reads back the way the server expects, and it delivers pixels on timing derived
from the exposure and readout parameters rather than instantly.

## Pixel data

Frames carry synthetic pixel data by default, which validates plumbing, geometry, timing and headers
rather than image quality. Setting `EMULATOR_DATADIR` to a directory of real frames makes it serve
those instead, which is what makes it useful for exercising downstream processing.

Per-detector frame sources live alongside the emulator ({source}`emulator/generic.h`,
{source}`emulator/nirc2.h`) and are selected by `-i`. They also parse their own keys from the
config, including `READOUT_TIME` and the pixel and row timings that set how fast a readout appears
to proceed.

:::{note}
Those detector-description keys are a separate namespace from the server configuration keys in the
[configuration reference](../configuration/index.md), even though they are read from the same file.
:::

## Limits

The emulator models the command and data protocol, not the controller. It does not reproduce
electrical behaviour, real detector noise, or the failure modes of a misconfigured ACF. A timing
script that is wrong in a way the Archon would reject may still appear to work here.

## In CI

Two workflow jobs run against it on every push and pull request:

- A frame outputs test that takes an exposure with both FITS and shared memory enabled, then
  validates the written header and reads the shared-memory segment back.
- A Python module test that drives the same exposure through the `camera_interface` bindings.

Both are in `.github/workflows/emulator-integration.yml` and are the closest thing the project has
to a regression suite for the acquisition path.

:::{tip}
The frame outputs job is also the most complete worked example of a full setup: it builds with
shared memory enabled, starts the emulator, starts `camerad` against it, drives an exposure with
`camerad-socksend`, then checks both outputs. Read it when a local setup misbehaves.
:::
