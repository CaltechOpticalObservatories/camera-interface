# Controller commands

Commands that reach the detector controller rather than being interpreted by the server. This is
the escape hatch for anything the higher-level commands do not cover.

Two routes exist. `native` passes a command straight through and returns the reply. The commands
routed through `controller_cmd`, marked Archon in the [base command table](base.md), are
server-side implementations of controller-specific features.

## Archon

`native <cmd>` sends `<cmd>` to the Archon and returns its reply without parsing it, beyond
confirming that a reply came back. The commands most worth knowing:

`FRAME`
: Frame buffer status: which buffer is complete, frame numbers, timestamps and sizes.

`STATUS`
: Backplane status, including module temperatures, voltages and currents.

`SYSTEM`
: Module complement: what occupies each slot, with type, revision and version.

`TIMER`
: The controller's free-running timer, useful for checking the link is alive.

The server itself parses `FRAME`, `STATUS` and `SYSTEM` replies for its own bookkeeping, so use
`native` for inspection rather than as a control path.

Beyond `native`, the Archon-specific commands are `raw` for configuration memory, `getp` and `setp`
for parameters, `inreg` for a VCPU input register, `loadtiming` and `readacf` for loading, `mode`
for camera modes, `autofetch_mode`, and `heater` and `sensor` for the thermal modules. The
[heater and sensor](#heater-and-sensor) syntax is below.

## ARC (AstroCam)

:::{warning}
`native` is not implemented on an ARC build. `AstroCamInterface::native` logs "not yet implemented"
and returns success, so the command replies `DONE` while doing nothing.

`controller_cmd` is not implemented either, so every command in the Archon list above returns
`not_supported` on an ARC build.
:::

Passing three-letter DSP commands through to an ARC controller is therefore not currently possible
from the command interface.

## Heater and sensor

For Archon **Heater** and **HeaterX** modules. Both require firmware to be loaded and a
sufficiently recent backplane.

### heater

```
heater <module> <A|B> [ <on|off> [target] | <target> | PID [<p> <i> <d>]
                        | RAMP [<on|off> [rate]] | ILIM [val] | INPUT [A|B|C] ]
```

| Form | Effect |
|---|---|
| `heater <module> <A\|B>` | Get enable state and target |
| `heater <module> <A\|B> <on\|off> [target]` | Set enable state, optionally the target |
| `heater <module> <A\|B> <target>` | Set the target |
| `heater <module> <A\|B> PID [<p> <i> <d>]` | Get or set the P, I and D terms, each 0 to 10000 |
| `heater <module> <A\|B> RAMP [<on\|off> [rate]]` | Get or set ramp enable and rate, 1 to 32767 |
| `heater <module> <A\|B> ILIM [val]` | Get or set the current limit, 0 to 10000 |
| `heater <module> <A\|B> INPUT [A\|B\|C]` | Get or set the input sensor. `C` requires HeaterX. |

The target range defaults to backplane-dependent limits, overridable with `HEATER_TARGET_MIN` and
`HEATER_TARGET_MAX` in degrees C. See [core keys](../configuration/core.md).

### sensor

```
sensor <module> <A|B|C> [ <current> | AVG [ <N> ] ]
```

| Form | Effect |
|---|---|
| `sensor <module> <A\|B\|C>` | Get the RTD excitation current in nanoamps |
| `sensor <module> <A\|B\|C> <current>` | Set it, 0 to 1600000 nA |
| `sensor <module> <A\|B\|C> AVG` | Get the digital averaging count |
| `sensor <module> <A\|B\|C> AVG <N>` | Set it, N in {1, 2, 4, 8, ... 256} |

Sensor `C` exists only on HeaterX modules.
