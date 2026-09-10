"""Drive a camera through the camera_interface module against the emulator.

Mirrors the socket-based frame-outputs job so both control paths are covered by
the same assertions. Exits non-zero on the first failure.
"""

from __future__ import annotations

import argparse
import pathlib
import sys
import time

import camera_interface

EXPECTED_INSTRUMENT = "hispec_tracking_camera"
EXPECTED_CONTROLLER = "archon"

# The FITS writer queues and drops by design, so a test that wants to see a
# file has to wait for it rather than expect it synchronously
WRITE_TIMEOUT_S = 30.0
POLL_INTERVAL_S = 0.1


class CheckFailed(Exception):
    """Raised when an integration check does not hold."""


def check(condition: bool, message: str) -> None:
    """Raise CheckFailed with message unless condition holds."""
    if not condition:
        raise CheckFailed(message)


def check_build_identity() -> None:
    """Confirm the module was built for the instrument this test assumes."""
    instrument = camera_interface.instrument_name()
    controller = camera_interface.controller_name()
    print(f"build: instrument={instrument} controller={controller}")
    check(instrument == EXPECTED_INSTRUMENT,
          f"expected instrument {EXPECTED_INSTRUMENT}, got {instrument}")
    check(controller == EXPECTED_CONTROLLER,
          f"expected controller {EXPECTED_CONTROLLER}, got {controller}")


def check_introspection(camera: camera_interface.Camera) -> None:
    """Confirm instrument commands enumerate and each one resolves."""
    commands = camera.instrument_commands()
    print(f"instrument commands: {commands}")
    check(bool(commands), "instrument reported no commands")
    for name in commands:
        check(camera.is_instrument_command(name),
              f"{name} was enumerated but is_instrument_command says no")
    check(not camera.is_instrument_command("nosuchcommand"),
          "an unknown command was accepted as an instrument command")


def wait_for_writes(camera: camera_interface.Camera) -> list[dict]:
    """Return output status once every configured output has written, or time out."""
    deadline = time.monotonic() + WRITE_TIMEOUT_S
    while time.monotonic() < deadline:
        status = camera.output_status()
        if status and all(output["frames_written"] >= 1 for output in status):
            return status
        time.sleep(POLL_INTERVAL_S)
    raise CheckFailed(f"not every output wrote within {WRITE_TIMEOUT_S}s: "
                      f"{camera.output_status()}")


def run(config_path: str, fits_dir: pathlib.Path) -> None:
    """Take one exposure through the module and verify it reached disk."""
    check_build_identity()

    before = set(fits_dir.glob("*.fits"))

    camera = camera_interface.Camera(config_path, log_to_stderr=True)
    camera.open()
    camera.load()
    camera.power("on")
    camera.exptime("0")
    print(f"power={camera.power()} exptime={camera.exptime()}")

    check_introspection(camera)

    camera.expose("1")

    # Asserted in process because the SHM segment is destroyed when the writer
    # closes, so an external reader would race this script's exit
    for output in wait_for_writes(camera):
        print(f"output: {output}")
        check(output["frames_dropped"] == 0,
              f"{output['name']} dropped {output['frames_dropped']} frames")

    written = sorted(set(fits_dir.glob("*.fits")) - before)
    check(bool(written), f"no new FITS file appeared in {fits_dir}")
    for path in written:
        print(f"wrote: {path} ({path.stat().st_size} bytes)")

    camera.close()


def main() -> int:
    """Parse arguments, run the checks, and report pass or fail."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, help="camerad .cfg to use")
    parser.add_argument("--fits-dir", required=True,
                        help="directory the FITS writer is configured to use")
    args = parser.parse_args()

    try:
        run(args.config, pathlib.Path(args.fits_dir))
    except CheckFailed as failure:
        print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    except RuntimeError as failure:
        print(f"FAIL: camera command failed: {failure}", file=sys.stderr)
        return 1

    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
