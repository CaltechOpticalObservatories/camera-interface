# Development

## Building the documentation

```bash
pip install -r docs/requirements.txt
sphinx-build -W -b html docs docs/_build/html
```

`-W` turns warnings into errors, which is what CI uses, so a broken cross-reference fails the build
rather than shipping a dead link.

The Python API page needs the compiled `camera_interface` module to be importable. Without it the
build still succeeds and the page shows a note in place of the reference, so a docs-only change does
not require a full C++ build locally.

## How the docs stay current

Reference tables that restate something the source already knows are generated at documentation
build time and cross-checked against the source, so the build fails when they diverge:

| Table | Source of truth |
|---|---|
| Base commands | The dispatch chain in {source}`camerad/camera_server.cpp`, with syntax from `CAMERAD_SYNTAX` |
| Configuration keys | Reads through a config object's `param` array, plus the frame output parser |
| ATC FITS keywords | The `HeaderDictEntry` table in {source}`camerad/Instruments/hispec_tracking_camera/fits_header_dictionary.cpp` |

Descriptions are written by hand in `docs/data/`, keyed by command or key name. Adding a command or
a config key to the source without a description there fails the docs build; so does describing one
that no longer exists.

The generators are Sphinx directives in `docs/_ext/camerad_tables.py`. Validation runs once at
`builder-inited`, so a mismatch fails immediately rather than partway through writing pages.

:::{tip}
Commands are taken from the dispatch chain rather than from `CAMERAD_SYNTAX`, because the two have
diverged: `CAMERAD_SYNTAX` feeds the `help` output and still advertises commands the server no
longer implements. The dispatch is what actually answers a client.
:::

## Documentation layout

```
docs/
  conf.py             Sphinx configuration
  requirements.txt    pinned docs toolchain
  data/               hand-written descriptions the generators consume
  _ext/               generator extensions
  <chapter>/          one directory per chapter
```

Pages are Markdown via MyST. Use `{source}`` `path` `` to link to a file in the repository rather
than pasting signatures into the prose.

## Publishing

`.github/workflows/docs.yml` builds on every pull request and every push to `main`.

- A pull request publishes to `previews/pr-<N>/` on the `gh-pages` branch, and a bot comments the
  link. The preview is removed when the PR closes.
- A merge to `main` publishes to the site root.
- Every build uploads the rendered HTML as a workflow artifact, which is the fallback for pull
  requests from forks, since those get a read-only token and cannot deploy.

:::{important}
The `gh-pages` branch needs a `.nojekyll` file at its root. Without it Pages runs the output through
Jekyll, which skips directories beginning with an underscore, and the whole site loads with no CSS
because `_static/` returns 404. A marker inside a preview subdirectory is not enough; it has to be at
the branch root. Recreating `gh-pages` from scratch means adding it again.
:::

## Testing camerad itself

```bash
make run_unit_tests && ./bin/run_unit_tests
```

The end-to-end tests run against the [emulator](../emulator/index.md) in CI.

## Instrument submodules

Instrument modules are pinned submodules. Updating one is a commit to `camera-interface` that moves
the pin, which `.github/workflows/update-submodules.yml` automates. Documentation for an instrument
lives here, in this repository, while each instrument repository keeps its own README.
