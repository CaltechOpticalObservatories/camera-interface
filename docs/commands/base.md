# Base commands

Every command camerad dispatches. Instrument modules add their own on top of these; see
[instruments](../instruments/index.md).

Any command accepts `?` as its argument to return its own syntax.

The **Controller** column marks the commands only the Archon interface implements. The server
recognizes the core's commands and calls the interface's function for each. It
hands any other name to `controller_cmd`, where the Archon interface handles its own commands and
forwards the rest to the instrument. On an ARC build the Archon commands return `not_supported`
unless the instrument claims them.

```{eval-rst}
.. camerad-commands::
   :widths: 12 30 10 48
```

:::{note}
This table is generated from the dispatch chains in {source}`camerad/camera_server.cpp` and
{source}`camerad/archon_interface.cpp`, so it lists what camerad actually answers. The `help` command is a separate hand-maintained list in
{source}`common/camerad_commands.h` that has drifted: it advertises around two dozen commands the
server no longer implements, and omits several it does, including `power`. Trust this table over
`help`.
:::

## Syntax not shown

A few commands are dispatched but absent from the syntax list, so no argument syntax is generated
for them. They are `power`, `getp`, `setp`, `inreg` and `autofetch_mode`. Use `?` against a
running server for the authoritative syntax.
