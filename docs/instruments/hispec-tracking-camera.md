# HISPEC tracking camera

The HISPEC acquisition and tracking camera (ATC): a 2048x2048 H2RG on an Archon controller. This is
the most complete instrument module and the one the emulator integration tests exercise.

Repository: [hispec-tracking-camera-instrument](https://github.com/CaltechOpticalObservatories/hispec-tracking-camera-instrument),
checked out at `camerad/Instruments/hispec_tracking_camera`.

## Build

```bash
cd build
cmake -DCONTROLLER=archon -DINSTRUMENT=hispec_tracking_camera ..
make
```

Shipped configuration is in the submodule's `config/`: `hispecatc.cfg` and `hispecatc.acf`.

## Three things called "mode"

The single most confusing thing about this module is that three unrelated settings are all called a
mode. They are selected by different commands and do different things.

Camera mode (`mode`)
: Names a `[MODE_*]` section of the loaded ACF. Selecting one loads that section's geometry
  (`PIXELCOUNT`, `LINECOUNT`), its parameters, and its tapline layout (`TAPLINES`, `TAPLINE0..N`)
  into the controller. This is the heavyweight one: it changes the shape of the data coming back.
  Requires firmware to already be loaded.

Exposure mode (`exposure`)
: Selects the H2RG readout scheme. Implemented by setting the matching `mode_*` ACF parameter to 1
  and every other one to 0, so the ACF must define all four.

    | Argument | ACF parameter | Scheme |
    |---|---|---|
    | `utr_rr` | `mode_UTR_RR` | Up the ramp, reset-read |
    | `utr_gr` | `mode_UTR_GR` | Up the ramp, guided read |
    | `rx` | `mode_RX` | Reset-execute |
    | `rxr` | `mode_RXR` | Reset-execute-read |

Acquisition mode (`exposuremode`)
: The base command, selecting which `Camera::ExposureMode` implementation drives acquisition. This
  module provides `DEFAULT` and `AUTOFETCH`. See [architecture](../architecture/index.md) for what
  an exposure mode is.

:::{tip}
`exposure` with no argument reports the current readout scheme. `mode` with no argument reports the
current camera mode. Neither changes anything when queried.
:::

## Instrument commands

Reached through the normal command interface, and from Python via `instrument_cmd()`.

| Command | Purpose |
|---|---|
| `h2rg_init` | Re-trigger the H2RG main reset and enable Pad B output with HIGHOHM |
| `mode` | Select or report the camera mode from the ACF |
| `exposure` | Select or report the H2RG readout scheme |
| `exposuremode` | Select the acquisition mode (base command) |
| `autofetch_mode` | Control autofetch, where the controller streams frames continuously |
| `freerun` | Arm (`1`) or disarm (`0`) continuous exposure |
| `window_mode` | Enter or leave windowed readout |
| `roi` | Set or report the region of interest |
| `take_stats` | Report pixel statistics |
| `debug` | Development diagnostics |

`instrument_commands()` enumerates them at runtime, which is authoritative for a given build.

:::{note}
`h2rg_init` exists because the ACF defaults `Start` to 1, so it is already true at load time and
never sees the 0 to 1 edge the H2RG main reset needs. Running it after power-up re-triggers that
edge. It is not optional on a cold start.
:::

Setting `autofetch_mode` also resets the readout scheme to the default, so select `exposure` after
`autofetch_mode`, not before.

## Region of interest

`roi` takes four argument shapes:

| Form | Meaning |
|---|---|
| `roi` | Report the current window as `vstart vstop hstart hstop` |
| `roi <height> <width>` | A centred region of that size |
| `roi <vstart> <vstop> <hstart> <hstop>` | An explicit region |
| `roi fullframe` | Return to the full 2048x2048 frame |

`window_mode` is the underlying toggle. Leaving window mode restores the saved `TAPLINES` and
`TAPLINE0` values, switches the camera back to the `DEFAULT` ACF mode to reset the internal buffer
geometry, and reapplies that mode's `PIXELCOUNT`. That teardown is why leaving window mode is not
simply the inverse of entering it, and why it needs the ACF to have a `DEFAULT` mode.

## Configuration

Alongside the [core keys](../configuration/index.md), this module reads its own:

| Key | Default | Meaning |
|---|---|---|
| `REFPIX_AMP` | `AM52` | Amplifier reading the reference channel, recorded as `REFPXAMP`. The reference channel's tapline moves with the camera mode; its amplifier does not. |
| `PIXEL_TIME_USEC` | built-in | Pixel time used to model the readout deadline, when the ACF does not supply one |
| `READOUT_MARGIN_MSEC` | built-in | Slack added to the computed per-frame readout deadline |

`PIXEL_TIME_USEC` and `READOUT_MARGIN_MSEC` set how long the acquisition thread waits for a frame
before calling it lost. Both must be positive, and a non-numeric value makes startup fail rather
than silently falling back.

Fixed in `configure_instrument()` rather than configured: the LVDS module is 10 and the detector's
maximum pixel index is 2047. The Archon socket is also tuned there for streaming, with `TCP_NODELAY`
and 1 MB buffers.

:::{warning}
`WRITE_TAPINFO_TO_FITS` appears in the shipped `hispecatc.cfg` but is read by nothing. Setting it
has no effect.
:::

## FITS keywords

The module carries its own keyword dictionary mapping each internal property to a keyword, comment,
type and default. It covers two cameras, ATC and SPEC, with separate defaults; the table shows the
ATC default, falling back to the SPEC one where ATC has none.

`python/tests/fits_header_check.py` validates a written file against this dictionary.

```{eval-rst}
.. camerad-fits-keywords::
   :widths: 12 20 10 10 48
```

:::{note}
Generated from {source}`camerad/Instruments/hispec_tracking_camera/fits_header_dictionary.cpp`, so
it cannot drift from the dictionary the instrument actually writes.
:::

## Error reporting

Every error this module logs carries a one-line state summary, so a failure records the camera state
that produced it rather than leaving it to be reconstructed from surrounding log lines that a
concurrent command may have interleaved. Callers get a short reason; the log gets the root cause and
the state.
