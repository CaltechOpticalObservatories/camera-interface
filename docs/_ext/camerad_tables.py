"""Sphinx directives that build reference tables from the camerad sources.

Each table is generated from the code that defines the thing being documented, and cross-checked
against hand-written descriptions in ``docs/data``. A command or configuration key that
gains or loses a definition without a matching description fails the build, so the reference
cannot silently drift from the source.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from functools import cache
from pathlib import Path

import yaml
from docutils import nodes
from docutils.parsers.rst import Directive
from docutils.statemachine import ViewList
from sphinx.errors import ExtensionError

DOCS_DIR = Path(__file__).resolve().parent.parent
REPO_ROOT = DOCS_DIR.parent
DATA_DIR = DOCS_DIR / "data"

# Where a configuration key may be recognized
CONFIG_SEARCH_DIRS = ("camerad", "common", "utils", "emulator")

# Reading the server config always goes through a config object's `param` array, which is what
# separates a real key from the Archon protocol tokens matched the same way elsewhere
_PARAM = (
    r'config(?:file)?\s*\.\s*param\s*'
    r'(?:\[\s*\w+\s*\]|\.\s*at\s*\(\s*\w+\s*\))'
)
CONFIG_KEY_PATTERNS = (
    re.compile(_PARAM + r'\s*==\s*"([A-Z][A-Z0-9_]*)"'),
    re.compile(_PARAM + r'\s*\.\s*compare\s*\(\s*\d+\s*,\s*\d+\s*,\s*"([A-Z][A-Z0-9_]*)"\s*\)'),
)

# The frame output keys are parsed from an already-split key/value pair rather than the param array,
# so this one idiom is scoped to the file that does it
FRAME_OUTPUT_SOURCE = "utils/frame_output_factory.cpp"
FRAME_OUTPUT_KEY_PATTERN = re.compile(r'\bkey\s*==\s*"([A-Z][A-Z0-9_]*)"')

# The server hands every name it does not recognize to the controller interface, so the names that
# dispatcher handles are dispatched commands too. Its body runs from the definition to the closing
# banner
CONTROLLER_DISPATCH_SOURCE = "camerad/archon_interface.cpp"
CONTROLLER_DISPATCH_START = "long ArchonInterface::controller_cmd("
CONTROLLER_DISPATCH_END = "/***** Camera::ArchonInterface::controller_cmd"

# Matched by the dispatch scan but not commands
NON_COMMANDS = frozenset({"_EXCEPTION_", "-h", "--help", "help", "?"})


@dataclass(frozen=True)
class Command:
    """One command camerad dispatches, with its advertised syntax."""

    name: str
    syntax: str
    summary: str
    controller: str
    note: str = ""


@dataclass(frozen=True)
class ConfigKey:
    """One configuration file key the code honours."""

    name: str
    summary: str
    group: str


def _read(relative_path: str) -> str:
    path = REPO_ROOT / relative_path
    if not path.is_file():
        raise ExtensionError(f"{relative_path} is missing")
    return path.read_text(encoding="utf-8")


def _load_data(name: str) -> dict:
    path = DATA_DIR / name
    if not path.is_file():
        raise ExtensionError(f"missing documentation data file {path}")
    return yaml.safe_load(path.read_text(encoding="utf-8")) or {}


def _check_coverage(kind: str, in_source: set[str], described: set[str]) -> None:
    undocumented = sorted(in_source - described)
    stale = sorted(described - in_source)
    problems = []
    if undocumented:
        problems.append(f"{kind} in the source with no description: {', '.join(undocumented)}")
    if stale:
        problems.append(f"{kind} described but no longer in the source: {', '.join(stale)}")
    if problems:
        raise ExtensionError(
            "; ".join(problems) + ". Update docs/data/ to match, or remove the stale entries."
        )


def _command_constants(header: str) -> dict[str, str]:
    pattern = r'const\s+std::string\s+(CAMERAD_\w+)\s*\(\s*"([^"]*)"\s*\)'
    return dict(re.findall(pattern, header))


def _advertised_syntax(header: str, constants: dict[str, str]) -> dict[str, str]:
    """Map command name to the syntax string CAMERAD_SYNTAX advertises for it."""
    block = re.search(r"CAMERAD_SYNTAX\s*=\s*\{(.*?)\n\s*\};", header, re.S)
    if not block:
        raise ExtensionError("could not find CAMERAD_SYNTAX in common/camerad_commands.h")
    syntax = {}
    entry = re.compile(r'(CAMERAD_\w+)((?:\s*\+\s*"(?:[^"\\]|\\.)*")*)')
    for constant, suffix in entry.findall(block.group(1)):
        parts = re.findall(r'"((?:[^"\\]|\\.)*)"', suffix)
        name = constants[constant]
        syntax[name] = name + "".join(parts).replace("\\|", "|")
    return syntax


def _matched_commands(body: str, constants: dict[str, str]) -> list[str]:
    found = []
    for constant, literal in re.findall(r'cmd\s*==\s*(?:(CAMERAD_\w+)|"([^"]+)")', body):
        name = constants[constant] if constant else literal
        if name not in NON_COMMANDS and name not in found:
            found.append(name)
    return found


def _dispatched_commands(server: str, controller: str, constants: dict[str, str]) -> list[str]:
    """Names the server recognizes, then those the controller interface handles itself."""
    found = _matched_commands(server[server.index("Process commands here"):], constants)
    start = controller.index(CONTROLLER_DISPATCH_START)
    body = controller[start:controller.index(CONTROLLER_DISPATCH_END, start)]
    found += [name for name in _matched_commands(body, constants) if name not in found]
    return found


@cache
def load_commands() -> list[Command]:
    header = _read("common/camerad_commands.h")
    constants = _command_constants(header)
    syntax = _advertised_syntax(header, constants)
    dispatched = _dispatched_commands(
        _read("camerad/camera_server.cpp"), _read(CONTROLLER_DISPATCH_SOURCE), constants
    )

    described = _load_data("commands.yaml")
    _check_coverage("commands", set(dispatched), set(described))

    return [
        Command(
            name=name,
            syntax=syntax.get(name, name),
            summary=described[name]["summary"],
            controller=described[name].get("controller", "any"),
            note=described[name].get("note", ""),
        )
        for name in sorted(dispatched)
    ]


@cache
def load_config_keys() -> list[ConfigKey]:
    found: set[str] = set()
    for directory in CONFIG_SEARCH_DIRS:
        for source in sorted((REPO_ROOT / directory).rglob("*")):
            if source.suffix not in (".cpp", ".h"):
                continue
            text = source.read_text(encoding="utf-8", errors="replace")
            for pattern in CONFIG_KEY_PATTERNS:
                found.update(pattern.findall(text))
    found.update(FRAME_OUTPUT_KEY_PATTERN.findall(_read(FRAME_OUTPUT_SOURCE)))

    described = _load_data("config_keys.yaml")
    _check_coverage("configuration keys", found, set(described))

    return [
        ConfigKey(name=name, summary=described[name]["summary"], group=described[name]["group"])
        for name in sorted(found)
    ]


def _literal(text: str) -> str:
    return f"``{text}``" if text else ""


class _TableDirective(Directive):
    """Base for directives that render a generated list-table."""

    has_content = False

    def _render(self, headers: list[str], rows: list[list[str]]) -> list[nodes.Node]:
        widths = self.options.get("widths", "")
        lines = [".. list-table::", "   :header-rows: 1"]
        if widths:
            lines.append(f"   :widths: {widths}")
        lines.append("")
        for row in [headers, *rows]:
            lines.append(f"   * - {row[0]}")
            lines.extend(f"     - {cell}" for cell in row[1:])
        lines.append("")

        view = ViewList(lines, source="")
        container = nodes.Element()
        self.state.nested_parse(view, self.content_offset, container)
        return container.children


class CameradCommands(_TableDirective):
    """Render the table of commands camerad dispatches."""

    option_spec = {"widths": str}

    def run(self) -> list[nodes.Node]:
        rows = []
        for command in load_commands():
            summary = command.summary
            if command.note:
                summary += f" {command.note}"
            rows.append(
                [
                    _literal(command.name),
                    _literal(command.syntax),
                    "Archon" if command.controller == "archon" else "any",
                    summary,
                ]
            )
        return self._render(["Command", "Syntax", "Controller", "Description"], rows)


class CameradConfigKeys(_TableDirective):
    """Render the configuration keys belonging to one group."""

    required_arguments = 1
    final_argument_whitespace = True
    option_spec = {"widths": str}

    def run(self) -> list[nodes.Node]:
        group = self.arguments[0].strip()
        keys = [key for key in load_config_keys() if key.group == group]
        if not keys:
            raise ExtensionError(f"no configuration keys in group {group!r}")
        rows = [[_literal(key.name), key.summary] for key in keys]
        return self._render(["Key", "Meaning"], rows)


def _validate(app) -> None:
    """Fail the build early when the source and the descriptions disagree."""
    load_commands()
    load_config_keys()


def setup(app):
    app.add_directive("camerad-commands", CameradCommands)
    app.add_directive("camerad-config-keys", CameradConfigKeys)
    app.connect("builder-inited", _validate)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
