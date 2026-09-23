# Python bindings

`camera_interface` lets a Python process own a camera directly: no `camerad` process, no socket, no
text protocol. It performs the same startup the server does, then exposes the command set as
methods.

```python
import camera_interface

camera = camera_interface.Camera("hispecatc.cfg")
camera.open()
camera.load()
camera.power("on")
camera.exptime("1.5")
camera.expose("1")
```

## Installing

`pip install` compiles `camerad` and the module and puts both in the environment, so `import
camera_interface` needs no `PYTHONPATH` and `camerad` is on `PATH`:

```bash
pip install ./camera-interface \
  --config-settings=cmake.define.INSTRUMENT=hispec_tracking_camera
```

Any CMake option can be passed the same way. `CONTROLLER` defaults to `archon` and
`BUILD_PYTHON_MODULE` is forced on. pybind11 comes from the build requirements into an isolated
build environment, and is never installed into the target environment.

A compiler and the full C++ dependency set must be present wherever `pip install` runs.

:::{important}
The controller and instrument are fixed when the wheel is built, and the module is always named
`camera_interface`, so one environment holds one instrument. Install into a separate environment per
instrument, and have the caller assert which one it got:

```python
assert camera_interface.instrument_name() == "hispec_tracking_camera"
```
:::

Alternatively, `cmake -DBUILD_PYTHON_MODULE=ON` builds it in the source tree, where importing it
means putting the build's `lib` directory on `PYTHONPATH`.

## Behaviour worth knowing

Commands release the GIL while they run, so a blocking `expose()` leaves the rest of the process
responsive. For a daemon, that is the difference between one exposure and its whole RPC loop
stalling.

A failed command raises `RuntimeError`.

Every command `camerad` accepts is reachable. Base commands are bound as methods; instrument
commands go through `instrument_cmd()`, enumerable with `instrument_commands()`; controller
commands such as `mode`, `raw`, `readacf`, `loadtiming`, `heater` and `sensor` go through
`controller_cmd()`. Only `exit` is missing, since the process belongs to the caller.

Logging follows `LOG_STDERR` from the `.cfg`, overridable per session with `log_to_stderr=`. The C++
log always goes to its daily file under `LOGPATH`.

:::{warning}
`output_status()` is a snapshot, never a barrier. The FITS writer queues and drops frames by design,
and nothing in this API lets a caller stall acquisition by waiting on an output. Anything that must
be woken per frame should attach to the
[shared-memory segment](../configuration/frame-outputs.md), which posts semaphores.
:::

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

`python/examples/shm_read_frames.py` is a streaming shared-memory consumer; see
[frame outputs](../configuration/frame-outputs.md).
