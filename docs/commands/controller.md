# Controller commands

Commands passed through to the detector controller largely untouched. The server does not interpret
them, so this is the escape hatch for anything the higher-level commands do not cover.

:::{note}
Fills out in M3 alongside the architecture chapter.
:::

## Archon

`native` sends an Archon command directly and returns its reply. The commands most worth knowing:

`FRAME`
: Frame buffer status: which buffer is complete, frame numbers, timestamps, sizes.

`STATUS`
: Backplane status, including module temperatures, voltages and currents.

`SYSTEM`
: Module complement: what is in each slot, with type, revision and version.

`raw` reaches the Archon configuration memory directly, to read the loaded configuration or set keys
in it.

## ARC (AstroCam)

ARC controllers take three-letter DSP commands. The ones the server exposes:

| Command | Meaning |
|---|---|
| `PON` | Power on |
| `POF` | Power off |
| `RDM` | Read memory |
| `WRM` | Write memory |
| `SBN` | Set bias number |
| `SMX` | Set multiplexer |
| `TDL` | Test data link |
