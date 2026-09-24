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

Beyond `native`, the Archon-specific commands are `raw` for pre-CDS sample capture, `getp` and
`setp` for parameters, `inreg` for a VCPU input register, `loadtiming` and `readacf` for loading,
`mode` for camera modes, `autofetch_mode`, and `heater` and `sensor` for the thermal modules. The
[raw samples](#raw-samples) and [heater and sensor](#heater-and-sensor) syntax is below.

## ARC (AstroCam)

:::{warning}
`native` is not implemented on an ARC build. `AstroCamInterface::native` logs "not yet implemented"
and returns success, so the command replies `DONE` while doing nothing.

`controller_cmd` is not implemented either, so every command in the Archon list above returns
`not_supported` on an ARC build.
:::

Passing three-letter DSP commands through to an ARC controller is therefore not currently possible
from the command interface.

## Raw samples

The Archon can capture a window of unprocessed ADC samples from a single channel alongside the
normal CDS frame, which is how a detector engineer inspects the output waveform for settling times,
sampling windows, reset level stability and clock feed through. `raw` configures and retrieves it.

```
raw [ config | set <KEY> <VAL> [...] | read ]
```

| Form | Effect |
|---|---|
| `raw config` | Report the six RAW keywords |
| `raw set <KEY> <VAL> ...` | Write the keyword(s) to configuration memory, then apply |
| `raw read` | Fetch the raw region of the newest buffer and dispatch it as a frame |

The keywords are `RAWENABLE`, `RAWSEL`, `RAWSTARTLINE`, `RAWENDLINE`, `RAWSTARTPIXEL` and
`RAWSAMPLES`. `RAWSAMPLES` is rounded up to a whole 1024-byte block per line.

:::{warning}
`RAWENABLE` takes effect at capture time, not at read time. The controller acquires raw samples
while it fills the frame buffer, so enabling it after an exposure cannot populate that buffer. Set
it before the exposure the samples should come from.

`raw set` can only write keywords that already exist in the loaded ACF, so an ACF that omits them
cannot be driven from the command interface at all.
:::

`raw read` refuses while `RAWENABLE` is 0. A controller with capture disabled reports zero raw
blocks and lines, which is indistinguishable from one that reports nothing, so the fetch would
otherwise return whatever happens to sit at the raw offset and label it as raw data.

The result is dispatched on its own stream, named `raw`, so it never collides with the image. The
FITS writer gives it a separate file and the shared-memory writer a separate segment. See
[frame output keys](../configuration/frame-outputs.md).

### Interpreting the samples

An AD channel and an ADM channel both arrive as an identical block of `uint16`, but they are not
sampled the same way:

AD
: 16 bits at 100 MHz, so consecutive samples are 10 ns apart.

ADM
: 18 bits at 12.5 MHz, truncated to 16 bits, with each value repeated eight times and dithered so
  that averaging the eight recovers the original 18-bit sample. The effective period is 80 ns.

Nothing in the data itself distinguishes the two, so `raw read` records the provenance in the
header instead.

| Keyword | Meaning |
|---|---|
| `RAWSEL` | The channel selector in force for the capture |
| `RAWMOD5` to `RAWMOD8` | `MODn_TYPE` of each slot `RAWSEL` can address |
| `RAWSAMP` | `RAWSAMPLES` |
| `RAWSLINE`, `RAWELINE` | `RAWSTARTLINE`, `RAWENDLINE` |
| `RAWSPIX` | `RAWSTARTPIXEL` |

Every candidate slot is reported rather than the one slot `RAWSEL` selects, because the mapping
from `RAWSEL` to a slot is not reliably known. See below.

### Module types and the RAWSEL range

The Archon manual is incomplete here and partly stale, and the GUI source distributed with the
controller is the better reference. `archongui/src/archon.h` carries the full module type list,
where the manual stops at `16+: Unknown`:

| Type | Module |
|---|---|
| 6 | Atlas |
| 16 | DriverX |
| 17 | ADM |
| 18 | Unknown, the sentinel |

`archongui/src/archongui.cpp` then builds its Raw Channel Select control with
`for (i = 1; i <= 72; i++)` and stores the zero-based index as `RAWSEL`, so the selector spans 0 to
71. That is 18 channels across each of slots 5 to 8, matching the ADM channel layout. The manual
instead documents `RAWSEL` as 0 to 15, four per slot, described purely in terms of AD modules.

:::{note}
The two strides disagree, and on a chassis with a mix of AD and ADM modules they resolve the same
`RAWSEL` to different slots. `camerad` therefore derives nothing from `RAWSEL` and records it
verbatim beside the type of every slot it could refer to, leaving the reader to resolve it.
:::

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
