# Base commands

Commands the server accepts regardless of which instrument it was built for. Availability still
depends on the controller: some are Archon-only, some ARC-only.

:::{note}
This page currently lists the command set grouped by purpose. M2 of the documentation work replaces
it with a table generated from `CAMERAD_SYNTAX` in {source}`common/camerad_commands.h`, carrying the
full argument syntax and a description per command.
:::

Any command accepts `?` as its argument to return its own syntax.

## Connection and firmware

`open`, `close`, `isopen`, `load`, `loadtiming`, `readacf`, `power`, `interface`, `config`

## Exposure

`expose`, `abort`, `stop`, `pause`, `resume`, `exptime`, `modexptime`, `exposuremode`,
`preexposures`, `shutter`, `readout`, `useframes`

## Geometry and readout

`geometry`, `imsize`, `buffer`, `bin`, `boi`, `bias`, `frametransfer`, `mode`

## Output and FITS

`imdir`, `autodir`, `basename`, `imnum`, `fitsname`, `fitsnaming`, `datacube`, `mex`, `mexamps`,
`key`, `writekeys`

## Controller access

`native`, `raw`, `heater`, `sensor`

## Diagnostics

`echo`, `test`, `longerror`, `exit`

Instrument modules add their own commands on top of these; see [instruments](../instruments/index.md).
