# Reference instrument

The smallest thing that builds against camerad, and the starting point for a new
instrument repository.

An instrument owns its interface class, the translation unit defining
`Camera::Interface::create()`, its ACF and cfg, and its own `CMakeLists.txt`. It
fetches camerad and links `camerad_server`, which carries the daemon and its
whole link interface, so the instrument supplies only what is specific to it.

```bash
cmake -S . -B build
cmake --build build
```

That produces a `camerad` running this instrument. Copy the four files into a
new repository and pin `GIT_TAG` to a release of the core.

The core's own CI builds this with
`-DFETCHCONTENT_SOURCE_DIR_CAMERA_INTERFACE` pointed at its working tree, so a
change that breaks a consumer fails there rather than in every instrument later.
