"""Sphinx configuration for the camera-interface documentation."""

import importlib.util
import sys
import tomllib
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent / "_ext"))

project = "camera-interface"
author = "Caltech Optical Observatories"
copyright = "Caltech Optical Observatories"

with (REPO_ROOT / "pyproject.toml").open("rb") as pyproject:
    release = tomllib.load(pyproject)["project"]["version"]
version = release

extensions = [
    "myst_parser",
    "sphinx.ext.autodoc",
    "sphinx.ext.extlinks",
    "sphinx_copybutton",
    "sphinx_design",
    "camerad_tables",
]

exclude_patterns = ["_build", "data"]

# Link to a source file with {source}`camerad/camera_interface.h` instead of pasting signatures,
# since the C++ reference is narrative rather than generated
extlinks = {
    "source": (
        "https://github.com/CaltechOpticalObservatories/camera-interface/blob/main/%s",
        "%s",
    ),
}

myst_enable_extensions = ["colon_fence", "deflist", "substitution"]
myst_heading_anchors = 3

# The Python module is a compiled extension, so autodoc needs a real build to import. CI always has
# one, and a genuine import failure there must break the build; a local docs build without one still
# succeeds and shows a note in place of the API reference.
if importlib.util.find_spec("camera_interface"):
    tags.add("has_python_module")  # noqa: F821  (Sphinx injects `tags` into this namespace)
else:
    suppress_warnings = ["autodoc"]

autodoc_member_order = "bysource"
autodoc_default_options = {"members": True}

html_theme = "furo"
html_title = f"camera-interface {release}"
html_theme_options = {
    "source_repository": "https://github.com/CaltechOpticalObservatories/camera-interface/",
    "source_branch": "main",
    "source_directory": "docs/",
}
