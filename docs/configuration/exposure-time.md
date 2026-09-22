# Exposure time

```
exptime [ <time> [ s | ms ] ]
```

Sets the exposure time, or reports it when given no argument.

A bare argument is interpreted in whatever unit `LONGEXPOSURE` selects, and the reported value uses
that same unit. An explicit `s` or `ms` suffix overrides it for that one command, so `exptime 500 ms`
means the same thing whichever way the instrument is configured.

| Key | Default | Meaning |
|---|---|---|
| `LONGEXPOSURE` | `true` | `true`: bare `exptime` arguments are seconds. `false`: milliseconds. |
| `EXPTIME_MSEC_PARAM` | none | Archon parameter holding the milliseconds part. Required. |
| `EXPTIME_SEC_PARAM` | none | Archon parameter holding the whole-seconds part. Optional. |

Internally the exposure time is always held in seconds, which is also the unit of the `EXPTIME` FITS
keyword, so the configured unit never changes what ends up archived.

:::{warning}
Archon parameters are 20 bits. Without `EXPTIME_SEC_PARAM` the whole exposure time has to fit in the
milliseconds parameter, capping it at 2^20 msec, about 1048 seconds. Beyond that `exptime` returns an
error rather than silently leaving the controller and the FITS header disagreeing.
:::
