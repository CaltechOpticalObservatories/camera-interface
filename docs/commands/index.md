# Command reference

The server speaks a line-oriented ASCII protocol over TCP. Commands are short mnemonics with
space-separated arguments; replies are plain text.

## Ports

Three ports are configured in the `.cfg` file, and they behave differently.

Blocking port (`BLKPORT`)
: The connection stays open for as long as the client holds it, so it works directly with `telnet`
  as an ad hoc command line. One command at a time: a command sent before the previous one has
  replied is ignored. The reply on the same connection is what signals completion. Use this when the
  order of execution matters.

Non-blocking port (`NBPORT`)
: Accepts one command, then closes the connection. Each connection is handled on its own thread, so
  commands can run concurrently. Their relative order is not guaranteed, which is the trade for the
  concurrency.

Asynchronous message port (`ASYNCPORT`)
: Connectionless UDP, multicast to `ASYNCGROUP`. Listen-only. Carries status the server emits on its
  own schedule, such as exposure progress, along with replies to non-blocking commands and messages
  too long for a command reply. Each message is prefixed with a tag naming its type.

Connections to the non-blocking port that sit idle are closed after 3 seconds
({source}`utils/network.h`), so a client that opens a connection and never sends anything cannot
accumulate threads.

The server serializes access to hardware that cannot tolerate concurrent use, whichever port the
commands arrive on.

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
async-messages
```

:::{note}
The base command table is generated from `CAMERAD_SYNTAX` in
{source}`common/camerad_commands.h` and cross-checked against the descriptions kept alongside the
docs, so a command added to the server without a description here fails the documentation build.
:::
