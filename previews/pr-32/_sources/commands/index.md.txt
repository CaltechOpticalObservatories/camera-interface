# Command reference

The server speaks a line-oriented ASCII protocol over TCP. Commands are short mnemonics with
space-separated arguments; replies are plain text.

## The port

`camerad` opens exactly one TCP port, `BLKPORT` ({source}`camerad/camerad.cpp`). The connection
stays open for as long as the client holds it, so it works directly with `telnet` as an ad hoc
command line, and the reply on the same connection is what signals completion.

Each client connection is served on its own thread. A socket that sits idle is closed after 3
seconds ({source}`utils/network.h`).

:::{warning}
Older configuration files and the superseded 2022 ICD describe two more ports: a non-blocking
command port (`NBPORT`) and a UDP multicast port for asynchronous status messages (`ASYNCPORT`,
`ASYNCGROUP`). Neither exists in camerad 2.0.

`NBPORT` is read only by the emulator. `ASYNCPORT` and `ASYNCGROUP` are read by nothing: the UDP
multicast class still exists in {source}`utils/network.cpp` but is never instantiated, so no
asynchronous messages are ever sent. Setting those keys has no effect.
:::

## Replies

A reply is the command's return value, if any, followed by `DONE` or `ERROR`:

```
exptime 1.5
1.500 DONE

expose
DONE

bogus
ERROR
```

`ERROR` means the server rejected or failed the command. With `LONGERROR=true` the reply carries a
human-readable reason as well.

Two commands break the pattern: a command invoked with `?` returns its syntax with no `DONE`
suffix, and commands that reply with JSON return the JSON document alone.

:::{warning}
`DONE` reports on the command, not on the frame outputs. `expose` returns `DONE` once the exposure
and readout complete, whether or not the FITS writer kept up. See
[frame outputs](../configuration/frame-outputs.md).
:::

## Command tables

```{toctree}
:maxdepth: 1

base
controller
```

:::{note}
The base command table is generated from the server's dispatch chain and cross-checked against the
descriptions kept alongside the docs, so a command the server gains or loses without a matching
description fails the documentation build.
:::
