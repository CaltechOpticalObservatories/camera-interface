# FITS output

:::{note}
Expands in M2, when the ATC keyword table becomes generated output. Naming, cube layout and the
system keyword tables are written then.
:::

`camerad` writes FITS through an asynchronous writer: the readout thread hands a completed frame to
a queue, and a dedicated thread writes it. Configuration is under
[frame output keys](../configuration/frame-outputs.md).

## Filenames

`fitsnaming` selects between two schemes:

`time`
: The filename carries a timestamp, so names never collide and sort chronologically.

`number`
: The filename carries an incrementing image number, reported and set with `imnum`.

`autodir` adds a `YYYYMMDD` subdirectory under the image directory. Which midnight that rolls over
on follows `TM_ZONE`.

## Cubes and extensions

`datacube` writes successive frames as planes of one cube rather than separate files. For detectors
read through several amplifiers, `mexamps` writes each amplifier as its own extension, and `mex`
controls multi-extension output generally.

## Keywords

Three sources of keywords end up in a header:

1. **System keywords**, written by the server: geometry, timing, exposure and controller state.
2. **Instrument keywords**, from the instrument module's header dictionary. The ATC dictionary is in
   {source}`camerad/Instruments/hispec_tracking_camera/fits_header_dictionary.cpp`.
3. **User keywords**, added at runtime with `key`. `writekeys` controls whether they are written
   before or after the exposure, which matters for anything whose value changes during it.

## Checking a header

`python/tests/fits_header_check.py` asserts that every keyword the instrument's dictionary promises
is present and carries the expected value, so a keyword that silently stops being populated fails
rather than going unnoticed. It needs no FITS library, and both emulator CI jobs run it.

```bash
python3 python/tests/fits_header_check.py <file.fits> --exptime <sec>
```
