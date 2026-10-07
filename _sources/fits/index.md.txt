# FITS output

`camerad` writes FITS through an asynchronous writer: the consumer thread hands a completed frame to
a queue and returns, and a dedicated thread writes it. Enabling and locating the output is covered
in [frame output keys](../configuration/frame-outputs.md).

## Filenames

The writer builds each name itself:

```
<FITS_OUTPUT_DIR>/<FITS_BASENAME>_<frame number>[_<stream>].fits
```

The frame number is zero-padded to eight digits. If that path already exists the writer appends
`_1`, `_2` and so on until it finds a free name, so a file is never silently overwritten.

The stream is omitted for the primary image and present for anything published alongside it, such
as the `raw` stream from [raw samples](../commands/controller.md#raw-samples). It comes last so a
frame and its companions sort together.

`FITS_AUTODIR` puts all of this inside a `YYYYMMDD` subdirectory of `FITS_OUTPUT_DIR`.

:::{warning}
Older documentation describes a `fitsnaming` command choosing between timestamp and number
schemes, with `imnum` and `fitsname` to go with it. Those commands no longer exist. Naming is
entirely `FITS_BASENAME` plus the frame number.
:::

## Cubes

`datacube true` makes the writer accumulate frames into one file instead of writing one file per
frame. The primary HDU is header-only (`NAXIS=0`) and each frame becomes an image extension. The
cube is finalized when the exposure command finishes, which the server signals to every output
after the last frame.

`datacube` is the only runtime option the FITS writer accepts; anything else is rejected.

## Keywords

Four sources end up in a header.

Writer keywords
: Added to every file: `FRAMENO`, `TIMESTMP` (the Archon timestamp in 0.01 microsecond units),
  `DATE` (when the file was written) and `FILENAME`. `FILENAME` carries the base name only, because
  a FITS card holds 68 characters and a deployment path can exceed that.

Per-exposure keywords
: Resolved once when the exposure starts and shared by all of its frames.

Per-frame keywords
: Rebuilt for each frame, for values that change between reads within one exposure.

User keywords
: Added at runtime with `key KEYWORD=VALUE//COMMENT`. `key list` shows both the system and user
  sets, and `key KEYWORD=.` deletes one.

Instrument modules supply their own dictionary on top of this; see the
[tracking camera keyword table](../instruments/hispec-tracking-camera.md) for the worked example.

## Checking a header

`python/tests/fits_header_check.py` asserts that every keyword the instrument's dictionary promises
is present and carries the expected value, so a keyword that silently stops being populated fails
rather than going unnoticed. It needs no FITS library, and both emulator CI jobs run it.

```bash
python3 python/tests/fits_header_check.py <file.fits> --exptime <sec>
```

## Pixel format

Frames of 2 bytes per pixel are written as `USHORT_IMG`, anything wider as `ULONG_IMG`.
