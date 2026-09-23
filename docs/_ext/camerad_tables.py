"""Sphinx directives that build reference tables from the camerad sources.

Each table is generated from the code that defines the thing being documented, and cross-checked
against hand-written descriptions in ``docs/data``. A command, configuration key or FITS keyword
that gains or loses a definition without a matching description fails the build, so the reference
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

# Where a configuration key may be recognized. Instrument submodules are excluded so that
# instrument-specific keys do not leak into the core table.
CONFIG_SEARCH_DIRS = ("camerad", "common", "utils", "emulator")

ATC_DICTIONARY = (
    REPO_ROOT
    / "camerad/Instruments/hispec_tracking_camera/fits_header_dictionary.cpp"
)

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

# Matched by the dispatch scan but not commands
NON_COMMANDS = frozenset({"_EXCEPTION_", "-h", "--help", "help", "?"})


@dataclass(frozen=True)
class Command:
    """One command the server dispatches, with its advertised syntax."""

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


@dataclass(frozen=True)
class FitsKeyword:
    """One entry of an instrument's FITS header dictionary."""

    keyword: str
    property: str
    comment: str
    type: str
    default_atc: str
    default_spec: str
    enum_values: tuple[str, ...] = ()


def _read(relative_path: str) -> str:
    path = REPO_ROOT / relative_path
    if not path.is_file():
        raise ExtensionError(
            f"{relative_path} is missing. Instrument modules are submodules; run "
            "`git submodule update --init --recursive` before building the documentation."
        )
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


def _dispatched_commands(server: str, constants: dict[str, str]) -> list[str]:
    body = server[server.index("Process commands here"):]
    found = []
    for constant, literal in re.findall(r'cmd\s*==\s*(?:(CAMERAD_\w+)|"([^"]+)")', body):
        name = constants[constant] if constant else literal
        if name not in NON_COMMANDS and name not in found:
            found.append(name)
    return found


@cache
def load_commands() -> list[Command]:
    header = _read("common/camerad_commands.h")
    constants = _command_constants(header)
    syntax = _advertised_syntax(header, constants)
    dispatched = _dispatched_commands(_read("camerad/camera_server.cpp"), constants)

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
            if source.suffix not in (".cpp", ".h") or "Instruments" in source.parts:
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


def _split_top_level(text: str) -> list[str]:
    """Split on commas that are not nested inside braces or a string literal."""
    parts, depth, in_string, escaped, current = [], 0, False, False, []
    for char in text:
        if in_string:
            current.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
            current.append(char)
        elif char == "{":
            depth += 1
            current.append(char)
        elif char == "}":
            depth -= 1
            current.append(char)
        elif char == "," and depth == 0:
            parts.append("".join(current))
            current = []
        else:
            current.append(char)
    parts.append("".join(current))
    return [part.strip() for part in parts]


def _entry_bodies(block: str) -> list[str]:
    """Yield the text inside each top-level ``{ ... }`` of an initializer list."""
    bodies, depth, in_string, escaped, start = [], 0, False, False, 0
    for index, char in enumerate(block):
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "{":
            if depth == 0:
                start = index + 1
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                bodies.append(block[start:index])
    return bodies


def _unquote(field_text: str) -> str:
    """Join adjacent C++ string literals into their value."""
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', field_text)).replace('\\"', '"')


def _balanced_body(text: str, open_index: int) -> str:
    """Return what is between the brace at ``open_index`` and its match."""
    depth, in_string, escaped = 0, False, False
    for index in range(open_index, len(text)):
        char = text[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[open_index + 1:index]
    raise ExtensionError("unbalanced braces in the FITS header dictionary")


@cache
def load_fits_keywords() -> list[FitsKeyword]:
    text = _read(str(ATC_DICTIONARY.relative_to(REPO_ROOT)))
    block = _balanced_body(text, text.index("dictionary = {") + len("dictionary = "))

    keywords = []
    for body in _entry_bodies(block):
        fields = _split_top_level(body)
        if len(fields) != 7:
            continue
        keywords.append(
            FitsKeyword(
                property=_unquote(fields[0]),
                keyword=_unquote(fields[1]),
                comment=_unquote(fields[2]),
                type=fields[3].replace("Type::", ""),
                default_atc=_unquote(fields[4]),
                default_spec=_unquote(fields[5]),
                enum_values=tuple(re.findall(r'"([^"]*)"', fields[6])),
            )
        )
    if not keywords:
        raise ExtensionError(f"parsed no entries from {ATC_DICTIONARY}")
    return keywords


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
    """Render the table of commands the server dispatches."""

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


class CameradFitsKeywords(_TableDirective):
    """Render an instrument's FITS header dictionary."""

    option_spec = {"widths": str}

    def run(self) -> list[nodes.Node]:
        rows = []
        for entry in load_fits_keywords():
            default = entry.default_atc or entry.default_spec
            comment = entry.comment
            if entry.enum_values:
                comment += " (" + ", ".join(f"``{v}``" for v in entry.enum_values) + ")"
            rows.append(
                [
                    _literal(entry.keyword),
                    _literal(entry.property),
                    entry.type,
                    _literal(default),
                    comment,
                ]
            )
        return self._render(["Keyword", "Property", "Type", "Default", "Comment"], rows)


def _validate(app) -> None:
    """Fail the build early when the source and the descriptions disagree."""
    load_commands()
    load_config_keys()
    load_fits_keywords()


def setup(app):
    app.add_directive("camerad-commands", CameradCommands)
    app.add_directive("camerad-config-keys", CameradConfigKeys)
    app.add_directive("camerad-fits-keywords", CameradFitsKeywords)
    app.connect("builder-inited", _validate)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
