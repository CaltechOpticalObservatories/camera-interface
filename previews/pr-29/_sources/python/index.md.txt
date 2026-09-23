# Python bindings

`camera_interface` lets a Python process own a camera directly: no `camerad` process, no socket, no
text protocol. Constructing a `Camera` performs the same one-time setup `camerad` does at startup,
then the command set is available as methods.

```python
import camera_interface

camera = camera_interface.Camera("hispecatc.cfg")
camera.open()
camera.load()
camera.power("on")
camera.exptime("1.5")
camera.expose("1")
```

Use it when the caller *is* the control system, so the text protocol would only be overhead. Keep
`camerad` when several clients share one camera, or when the camera must outlive the client.

## Installing

`pip install` compiles `camerad` and the module together and puts both in the environment, so
`import camera_interface` needs no `PYTHONPATH` and `camerad` is on `PATH`:

```bash
pip install ./camera-interface \
  --config-settings=cmake.define.INSTRUMENT=hispec_tracking_camera
```

Any CMake option can be passed the same way, so
`--config-settings=cmake.define.ENABLE_SHM_OUTPUT=ON` works too. `CONTROLLER` defaults to `archon`
and `BUILD_PYTHON_MODULE` is forced on. pybind11 comes from the build requirements into an isolated
build environment and is never installed into the target environment.

A compiler and the full C++ dependency set must be present wherever `pip install` runs, since it
builds from source.

:::{important}
The controller and instrument are fixed when the wheel is built, and the module is always named
`camera_interface`, so one environment holds one instrument. Install into a separate environment per
instrument, and have the caller assert which one it got:

```python
assert camera_interface.instrument_name() == "hispec_tracking_camera"
```
:::

Alternatively `cmake -DBUILD_PYTHON_MODULE=ON` builds it in the source tree, where importing means
putting the build's `lib` directory on `PYTHONPATH`.

## Command coverage

Every command `camerad` accepts is reachable, through three routes:

Base commands
: Bound as methods: `open`, `close`, `load`, `power`, `exptime`, `expose`, `abort`, `key`,
  `datacube` and the rest.

Instrument commands
: `instrument_cmd(command, args)`, a deliberate passthrough rather than one binding each, so a build
  whose instrument gains a command exposes it with no change to the module. Enumerate with
  `instrument_commands()`, or test one with `is_instrument_command()`.

Controller commands
: `controller_cmd(command, args)` for `mode`, `raw`, `readacf`, `loadtiming`, `heater`, `sensor` and
  the other Archon-only commands.

Only `exit` is missing, since the process belongs to the caller.

## Errors

A command that fails raises `RuntimeError`, carrying the server's own error detail where there is
one:

```python
try:
    camera.expose("1")
except RuntimeError as error:
    log.error("exposure failed: %s", error)
```

Commands that succeed return the command's return string, which is often empty.

## Concurrency

Blocking commands release the GIL while they run, so a long `expose()` leaves the rest of the
process responsive. For a daemon that is the difference between one exposure stalling and its whole
RPC loop stalling.

This does not make the object thread-safe. The underlying interface serializes hardware access, but
issuing conflicting commands from several threads is still a logic error.

## Frame outputs

The frame outputs are configured from the `.cfg` exactly as they are for `camerad`, and
`output_status()` reports on them. It returns a list of dicts, one per configured output:

```python
for output in camera.output_status():
    print(output["name"], output["frames_written"],
          output["frames_dropped"], output["last_written"])
```

:::{warning}
`output_status()` is a snapshot, never a barrier. The FITS writer queues and drops frames by design,
and nothing in this API lets a caller stall acquisition by waiting on an output, because that would
serialize acquisition behind disk I/O. Anything that must be woken per frame should attach to the
[shared-memory segment](../configuration/frame-outputs.md), which posts a semaphore per frame.
:::

## Logging

Logging follows `LOG_STDERR` from the `.cfg`, overridable per session with `log_to_stderr=`:

```python
camera = camera_interface.Camera("hispecatc.cfg", log_to_stderr=True)
```

The C++ log always goes to its daily file under `LOGPATH` regardless.

## API

```{eval-rst}
.. only:: has_python_module

   .. automodule:: camera_interface
      :members:
      :undoc-members:
      :member-order: bysource

.. only:: not has_python_module

   .. note::

      The API reference is generated from the compiled module, which was not importable when these
      docs were built. Build with ``-DBUILD_PYTHON_MODULE=ON`` or ``pip install .`` and rebuild the
      docs to see it. The published documentation always includes it.
```

## Examples

`python/examples/shm_read_frames.py` is a streaming shared-memory consumer that blocks on the
stream's semaphore and flags frames it missed. See
[frame outputs](../configuration/frame-outputs.md) for what it needs.
