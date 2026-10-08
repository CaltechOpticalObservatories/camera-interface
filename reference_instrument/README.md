# Reference instrument

This is an instrument that belongs to no project. The core's CI builds it the way an instrument repository builds,
and runs it against the emulator, so that each point where an instrument specializes the core is exercised on every change.

It derives from `ArchonInterface` and overrides the three specialization points, each observable over the socket:

| Point | Override | Observed by |
|---|---|---|
| Instrument command | `instrument_commands`, `instrument_cmd` | `refvalue` replies with the configured value |
| Instrument configuration | `configure_instrument` | reads `REFERENCE_VALUE` from the cfg, which `refvalue` reports |
| Mode selection | `get_exposure_modes`, `set_exposure_mode` | `exposuremode ?` lists `REFERENCE`; `exposuremode REFERENCE` selects it |

`is_instrument_command` is also overridden, only because the current server consults it before every command. That
is not the design, and the dispatch fix removes it.

`REFERENCE` reads out as `SINGLE` and differs only in the type it reports. Any other mode is constructed by
`ArchonInterface`.

The CI job checks command dispatch, not frame readout: `expose` replies DONE whatever `do_expose()` returns, so a
passing job does not show that a frame was read.

## Building

```bash
cmake -S reference_instrument -B build-reference
cmake --build build-reference
ctest --test-dir build-reference
```

This produces `camerad` and the `camera_interface` module, each compiled with `reference_interface_factory.cpp`.
`ctest` imports the module and checks `instrument_name()` and `controller_name()`.

`CMakeLists.txt` declares the core with `FetchContent` at a pinned commit, but inside the core it defaults
`FETCHCONTENT_SOURCE_DIR_CAMERAD` to the enclosing checkout, so it always builds the surrounding tree and never
fetches. Passing `-DFETCHCONTENT_SOURCE_DIR_CAMERAD=<path>` points it at another checkout.

## Running

Paths in `reference.cfg` are relative to the working directory, so run from the core's root. The emulator and
`camerad-socksend` come from a top-level build of the core:

```bash
bin/camerad-emulator reference_instrument/reference.cfg -i generic &
build-reference/camerad --foreground --config reference_instrument/reference.cfg &
bin/camerad-socksend -p 3231 "refvalue"
```

A DONE reply to `expose` means the command was accepted, not that a frame was read; check the log for that.

## Starting a new instrument from a copy

- Remove the `FETCHCONTENT_SOURCE_DIR_CAMERAD` default, and set `GIT_TAG` to a released version of the core.
- Rename `reference`, `ReferenceInstrument` and `REFERENCE` throughout, including `CAMERAD_INSTRUMENT_NAME`.
- Bring your own ACF and system file; `reference.cfg` uses the core's `config/demo/` ones.
