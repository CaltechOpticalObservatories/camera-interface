# Core keys

Keys read by the server itself, independent of instrument or frame output.

:::{note}
This table is written by hand today. M2 of the documentation work replaces it with a generated table
cross-checked against the keys the source actually honours.
:::

## Controller connection

| Key | Meaning |
|---|---|
| `ARCHON_IP` | IP address of the Archon controller. Point at the emulator to run without hardware. |
| `ARCHON_PORT` | Port of the Archon controller |
| `DEFAULT_FIRMWARE` | Firmware loaded by `load` with no argument. Indexed array for multi-controller ARC systems. |
| `INSTRUMENT` | Instrument name, checked against the instrument the binary was built for |

## Archon parameters

The server drives an Archon by writing named parameters defined in the ACF. These keys tell it which
names to use, so the same binary works with differently authored timing scripts.

| Key | Meaning |
|---|---|
| `EXPOSE_PARAM` | Parameter written to trigger an exposure |
| `START_PARAM` | Parameter written to start the timing script |
| `ABORT_PARAM` | Parameter written to abort an exposure |
| `EXPTIME_MSEC_PARAM` | Parameter holding the milliseconds part of the exposure time. Required. |
| `EXPTIME_SEC_PARAM` | Parameter holding the whole-seconds part. Optional, but see [exposure time](exposure-time.md) for the limit without it. |

## Ports

| Key | Meaning |
|---|---|
| `BLKPORT` | Blocking command port. One command at a time, connection stays open. |
| `NBPORT` | Non-blocking command port. One command per connection, then closed. |
| `ASYNCPORT` | UDP port for asynchronous status messages |
| `ASYNCGROUP` | Multicast group the asynchronous messages are sent to |

See [the command protocol](../commands/index.md) for how the ports differ in behaviour.

## Files and logging

| Key | Meaning |
|---|---|
| `IMDIR` | Base directory for image files |
| `AUTODIR` | `yes` to write into a `YYYYMMDD` subdirectory of `IMDIR` |
| `BASENAME` | Base filename for images |
| `DIRMODE` | Permissions for directories the server creates |
| `LOGPATH` | Directory for the daily log file |
| `LOG_STDERR` | Also write the log to stderr, overriding what `--foreground` implies |
| `TM_ZONE_LOG` | `UTC` or `local`, for log entry timestamps only |
| `TM_ZONE` | `UTC` or `local`, for everything else including FITS times and `AUTODIR` |
| `TZ_ENV` | POSIX `TZ` string used when a zone is set to `local` |

:::{tip}
`TM_ZONE=local` is useful in the lab, where a UTC date rollover in the middle of a working day splits
one session across two `AUTODIR` directories. The time zone is recorded in the FITS header either way.
:::

## Behaviour

| Key | Meaning |
|---|---|
| `DAEMON` | `yes` or `no`. The `--foreground` command line option overrides it. |
| `LONGERROR` | `true` to return long error messages on the command port |
| `LONGEXPOSURE` | Unit for bare `exptime` arguments. See [exposure time](exposure-time.md). |
| `READOUT_TIME` | Expected readout time in msec, used to time out a readout that never completes |
| `HEATER_TARGET_MIN` | Lower bound for heater targets, overriding the backplane default |
| `HEATER_TARGET_MAX` | Upper bound for heater targets, overriding the backplane default |

## Emulator

Read by `camerad-emulator`, not by the server, but conventionally kept in the same file.

| Key | Meaning |
|---|---|
| `EMULATOR_PORT` | Port the emulator listens on. `ARCHON_PORT` points here to run without hardware. |
| `EMULATOR_SYSTEM` | Path to the `.system` file describing the emulated module complement |
