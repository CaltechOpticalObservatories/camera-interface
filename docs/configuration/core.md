# Core keys

Keys the server itself reads, independent of the frame outputs.

:::{important}
These tables list only keys the code actually reads. Configuration files in the wild, and the
superseded 2022 ICD, carry a number of keys that nothing reads any more: `IMDIR`, `BASENAME`,
`AUTODIR`, `DIRMODE`, `DAEMON`, `LONGERROR`, `TM_ZONE`, `TZ_ENV`, `ASYNCPORT`, `ASYNCGROUP` and
`START_PARAM` among them. Setting them has no effect. Image naming and location moved to the
[frame output keys](frame-outputs.md).
:::

## Controller connection

```{eval-rst}
.. camerad-config-keys:: Controller connection
   :widths: 30 70
```

## Archon parameters

The server drives an Archon by writing named parameters defined in the ACF. These keys say which
names to use, so one binary works with differently authored timing scripts.

```{eval-rst}
.. camerad-config-keys:: Archon parameters
   :widths: 30 70
```

## Server

```{eval-rst}
.. camerad-config-keys:: Server
   :widths: 30 70
```

## Exposure

```{eval-rst}
.. camerad-config-keys:: Exposure
   :widths: 30 70
```

See [exposure time](exposure-time.md) for what the unit affects.

## Heater

```{eval-rst}
.. camerad-config-keys:: Heater
   :widths: 30 70
```

## Emulator

Read by `camerad-emulator`, not by the server, but conventionally kept in the same file.

```{eval-rst}
.. camerad-config-keys:: Emulator
   :widths: 30 70
```
