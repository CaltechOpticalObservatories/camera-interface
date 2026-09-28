"""Capture a pre-CDS RAW waveform from each of several Archon channels.

The Archon captures raw samples for one channel per exposure, selected by
RAWSEL, so covering more than one channel means one exposure each. This script
loops that: set RAWSEL, expose, fetch, and record which file resulted.

RAWSEL is applied before the exposure because the controller acquires raw
samples while it fills the frame buffer, so enabling it afterwards cannot
populate that buffer.

No slot is inferred from RAWSEL. The manual maps four channels per slot while
the Archon GUI offers 72 selections, which is eighteen per slot, and the two
disagree on a mixed chassis. The module inventory is reported from SYSTEM
instead, and each file's header carries RAWSEL beside every candidate slot type.

    python raw_channel_sweep.py --config lris2.cfg --fits-dir /tmp/images \\
        --channels 0-3,18-21

Needs a camera_interface module built for an Archon controller, and an ACF that
defines the six RAW keys, since camerad can only write keys it already loaded.
It loads that ACF, so do not point it at a controller someone else is using.
"""

from __future__ import annotations

import argparse
import contextlib
import pathlib
import sys
import time
from types import MappingProxyType
from typing import Final

import camera_interface

# From archongui/src/archon.h, which carries the full table where the Archon
# manual stops at "16+: Unknown"
MODULE_NAMES: Final = MappingProxyType({
    0: "None", 1: "Driver", 2: "AD", 3: "LVBias", 4: "HVBias", 5: "Heater",
    6: "Atlas", 7: "HS", 8: "HVXBias", 9: "LVXBias", 10: "LVDS", 11: "HeaterX",
    12: "XVBias", 13: "ADF", 14: "ADX", 15: "ADLN", 16: "DriverX", 17: "ADM",
    18: "Unknown",
})

# Slots an AD or ADM module may occupy, and therefore the ones RAWSEL addresses
AD_SLOTS: Final = range(5, 9)

RAW_COMMAND: Final = "raw"

# The FITS writer queues and drops by design, so a caller wanting the file has
# to wait for it rather than expect it synchronously
WRITE_TIMEOUT_S: Final = 60.0
POLL_INTERVAL_S: Final = 0.2


class SweepError(Exception):
    """A step of the sweep failed in a way that should stop it."""


def parse_channels(spec: str) -> list[int]:
    """Expand a channel spec such as "0-3,7,18-21" into a sorted channel list."""
    channels: set[int] = set()
    for part in spec.split(","):
        piece = part.strip()
        if not piece:
            continue
        if "-" in piece:
            low, _, high = piece.partition("-")
            start, end = int(low), int(high)
            if end < start:
                raise ValueError(f"descending range: {piece}")
            channels.update(range(start, end + 1))
        else:
            channels.add(int(piece))
    if not channels:
        raise ValueError("no channels selected")
    return sorted(channels)


def read_module_types(camera: camera_interface.Camera) -> dict[int, int]:
    """Return MODn_TYPE for each AD-capable slot, parsed from the SYSTEM report."""
    report = camera.native("SYSTEM")
    types: dict[int, int] = {}
    for field in report.split():
        key, _, value = field.partition("=")
        for slot in AD_SLOTS:
            if key == f"MOD{slot}_TYPE":
                types[slot] = int(value)
    return types


def describe_modules(types: dict[int, int]) -> str:
    """Render the AD-capable slots as "slot 5: ADM" entries for logging."""
    if not types:
        return "no module types reported"
    return ", ".join(f"slot {slot}: {MODULE_NAMES.get(kind, kind)}"
                     for slot, kind in sorted(types.items()))


def wait_for_raw_file(fits_dir: pathlib.Path,
                      already_present: set[pathlib.Path]) -> pathlib.Path:
    """Return the RAW file that appears after a fetch, or raise on timeout."""
    deadline = time.monotonic() + WRITE_TIMEOUT_S
    while time.monotonic() < deadline:
        new = sorted(set(fits_dir.glob("*_raw.fits")) - already_present)
        if new:
            return new[-1]
        time.sleep(POLL_INTERVAL_S)
    raise SweepError(f"no new *_raw.fits appeared in {fits_dir} "
                     f"within {WRITE_TIMEOUT_S}s")


def capture_channel(camera: camera_interface.Camera, channel: int,
                    geometry: list[str],
                    fits_dir: pathlib.Path) -> pathlib.Path:
    """Expose with RAWSEL on one channel, fetch the capture, return its file."""
    settings = ["RAWENABLE", "1", "RAWSEL", str(channel), *geometry]
    camera.controller_cmd(RAW_COMMAND, "set " + " ".join(settings))

    already_present = set(fits_dir.glob("*_raw.fits"))
    camera.expose("1")
    summary = camera.controller_cmd(RAW_COMMAND, "read")
    print(f"  RAWSEL={channel}: {summary}")
    return wait_for_raw_file(fits_dir, already_present)


def geometry_settings(args: argparse.Namespace) -> list[str]:
    """Build the RAW geometry key and value pairs the caller chose to override."""
    overrides = {
        "RAWSAMPLES": args.samples,
        "RAWSTARTLINE": args.start_line,
        "RAWENDLINE": args.end_line,
        "RAWSTARTPIXEL": args.start_pixel,
    }
    return [token
            for key, value in overrides.items() if value is not None
            for token in (key, str(value))]


def sweep(camera: camera_interface.Camera, channels: list[int],
          geometry: list[str],
          fits_dir: pathlib.Path) -> tuple[dict[int, pathlib.Path], dict[int, str]]:
    """Capture each channel in turn, returning the files and any failures."""
    captured: dict[int, pathlib.Path] = {}
    failures: dict[int, str] = {}
    for channel in channels:
        try:
            captured[channel] = capture_channel(camera, channel, geometry, fits_dir)
        except (SweepError, RuntimeError) as failure:
            print(f"  RAWSEL={channel}: FAILED {failure}", file=sys.stderr)
            failures[channel] = str(failure)
    return captured, failures


def run(args: argparse.Namespace) -> int:
    """Sweep the requested channels and report where each capture landed."""
    controller = camera_interface.controller_name()
    if controller != "archon":
        raise SweepError(f"RAW capture is Archon-only, but this module is "
                         f"built for {controller}")

    channels = parse_channels(args.channels)
    fits_dir = pathlib.Path(args.fits_dir)
    if not fits_dir.is_dir():
        raise SweepError(f"not a directory: {fits_dir}")

    # The binding exposes no __enter__, so closing() is what guarantees the
    # controller is released even when a capture raises
    with contextlib.closing(camera_interface.Camera(args.config,
                                                    log_to_stderr=True)) as camera:
        camera.open()
        camera.load()
        camera.power("on")
        if args.exptime is not None:
            camera.exptime(str(args.exptime))

        print(f"instrument={camera_interface.instrument_name()} "
              f"exptime={camera.exptime()}")
        print(f"modules: {describe_modules(read_module_types(camera))}")
        print(f"sweeping RAWSEL {channels}")

        try:
            captured, failures = sweep(camera, channels,
                                       geometry_settings(args), fits_dir)
        finally:
            # Leave capture off so a later exposure does not silently carry raw data
            camera.controller_cmd(RAW_COMMAND, "set RAWENABLE 0")

    print(f"\ncaptured {len(captured)} of {len(channels)} channels")
    for channel, path in captured.items():
        print(f"  RAWSEL={channel} -> {path.name} ({path.stat().st_size} bytes)")
    return 1 if failures else 0


def main() -> int:
    """Parse arguments, run the sweep, and report pass or fail."""
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True, help="camerad .cfg to use")
    parser.add_argument("--fits-dir", required=True,
                        help="directory the FITS writer is configured to use")
    parser.add_argument("--channels", default="0-3",
                        help="RAWSEL values, e.g. \"0-3,18-21\" (default: 0-3)")
    parser.add_argument("--exptime", type=float,
                        help="exposure time, in the unit the config selects")
    parser.add_argument("--samples", type=int, help="RAWSAMPLES override")
    parser.add_argument("--start-line", type=int, help="RAWSTARTLINE override")
    parser.add_argument("--end-line", type=int, help="RAWENDLINE override")
    parser.add_argument("--start-pixel", type=int, help="RAWSTARTPIXEL override")
    args = parser.parse_args()

    try:
        return run(args)
    except (SweepError, ValueError) as failure:
        print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    except RuntimeError as failure:
        print(f"FAIL: camera command failed: {failure}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
