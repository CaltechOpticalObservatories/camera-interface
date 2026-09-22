# Asynchronous messages

Messages the server multicasts to `ASYNCGROUP` on `ASYNCPORT`, unprompted. Each is prefixed with a
tag naming its type, so a listener can filter without parsing the payload.

:::{note}
The tag list below comes from the superseded 2022 ICD and is being re-verified against the source in
M2. Treat it as indicative until then.
:::

| Tag | Meaning |
|---|---|
| `ERROR:message` | An error occurred |
| `NOTICE:message` | Informational notice |
| `EXPOSURE:n` | Exposure progress, Archon only |
| `EXPOSURE_d:n` | Exposure progress for device `d`, ARC only |
| `LINECOUNT:n` | Lines read out so far, Archon only |
| `PIXELCOUNT:n` | Pixels read out so far, ARC only |
| `FILE:<path> COMPLETE` | A FITS file finished writing |
| `DATACUBE:n COMPLETE` or `ERROR` | A data cube finished |

A listener wanting to wake on every frame should attach to the
[shared-memory output](../configuration/frame-outputs.md) instead, which posts a semaphore per
frame. The asynchronous port is for status, and is not a delivery guarantee.
