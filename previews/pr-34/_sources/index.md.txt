# camera-interface

`camerad` is a detector controller server. It owns the connection to a detector controller, exposes
every detector function as a line-oriented ASCII command over TCP, and writes acquired frames to FITS
files and to shared memory.

It supports two controller families, selected at build time with `-DCONTROLLER=`:

`archon`
: STA/Archon controllers, reached over TCP. This is the path most actively developed, and the only
  one with an emulator.

`astrocam`
: Astronomical Research Cameras ("Leach") controllers, reached over a PCIe driver. Requires ARC API
  3.6 and the Arc66PCIe driver.

A detector-specific **instrument** module is layered on top of the controller, also selected at build
time, with `-DINSTRUMENT=`. The instrument supplies the exposure modes, extra commands and FITS
keywords that a particular camera needs.

Clients can drive the server three ways: the text protocol over a socket, the `camerad-socksend`
command line tool, or the `camera_interface` Python module, which skips the server process entirely
and owns the camera in-process.

## Start here

::::{grid} 1 1 2 2
:gutter: 2

:::{grid-item-card} {octicon}`rocket` Getting started
:link: getting-started/index
:link-type: doc

Build the software, run the server, and take a first exposure against the emulator.
:::

:::{grid-item-card} {octicon}`terminal` Command reference
:link: commands/index
:link-type: doc

Every command the server accepts, the ports it listens on, and what it returns.
:::

:::{grid-item-card} {octicon}`gear` Configuration reference
:link: configuration/index
:link-type: doc

Every key the `.cfg` file honours, including frame outputs and exposure time.
:::

:::{grid-item-card} {octicon}`stack` Architecture
:link: architecture/index
:link-type: doc

How the server, interface, controller and exposure modes fit together.
:::

::::

```{toctree}
:hidden:
:caption: Using camerad

getting-started/index
configuration/index
commands/index
fits/index
```

```{toctree}
:hidden:
:caption: Reference

architecture/index
instruments/index
emulator/index
python/index
```

```{toctree}
:hidden:
:caption: Contributing

development/index
```
