# Base commands

Every command the server dispatches. Instrument modules add their own on top of these; see
[instruments](../instruments/index.md).

Any command accepts `?` as its argument to return its own syntax.

The **Controller** column distinguishes commands the interface implements directly from those
routed through `controller_cmd`, which only the Archon interface implements. On an ARC build the
latter return `not_supported`.

```{eval-rst}
.. camerad-commands::
   :widths: 12 30 10 48
```

:::{note}
This table is generated from the dispatch chain in {source}`camerad/camera_server.cpp`, so it lists
what the server actually answers. The `help` command is a separate hand-maintained list in
{source}`common/camerad_commands.h` that has drifted: it advertises around two dozen commands the
server no longer implements, and omits several it does, including `power`. Trust this table over
`help`.
:::

## Syntax not shown

A few commands are dispatched but absent from the syntax list, so no argument syntax is generated
for them. They are `power`, `getp`, `setp`, `inreg`, `autofetch_mode` and `bob`. Use `?` against a
running server for the authoritative syntax.
