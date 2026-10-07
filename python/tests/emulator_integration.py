"""Drive a camera through the camera_interface module against the emulator.

The module owns the controller connection, so every wait here is on a condition
the camera reports rather than on a duration. Exits non-zero on the first
failure.
"""

from __future__ import annotations

import argparse
import pathlib
import sys
import time

import camera_interface

from fits_header_check import CheckFailed, check, check_file

EXPECTED_CONTROLLER = "archon"

# The FITS writer queues and drops by design, so a test that wants to see a
# file has to wait for it rather than expect it synchronously
WRITE_TIMEOUT_S = 30.0
POLL_INTERVAL_S = 0.1

# Nonzero so the header's EXPTIME is checked against a value something had to carry
EXPTIME_S = 1.5


def check_build_identity(expected_instrument: str) -> None:
    """Confirm the module was built for the instrument this run expects."""
    instrument = camera_interface.instrument_name()
    controller = camera_interface.controller_name()
    print(f"build: instrument={instrument} controller={controller}")
    check(instrument == expected_instrument,
          f"expected instrument {expected_instrument}, got {instrument}")
    check(controller == EXPECTED_CONTROLLER,
          f"expected controller {EXPECTED_CONTROLLER}, got {controller}")


def check_introspection(camera: camera_interface.Camera) -> None:
    """Confirm every enumerated instrument command resolves, and no other does."""
    commands = camera.instrument_commands()
    print(f"instrument commands: {commands}")
    for name in commands:
        check(camera.is_instrument_command(name),
              f"{name} was enumerated but is_instrument_command says no")
    check(not camera.is_instrument_command("nosuchcommand"),
          "an unknown command was accepted as an instrument command")


def check_exptime_units(camera: camera_interface.Camera) -> None:
    """Confirm an explicit unit overrides the config's default of seconds."""
    reply = camera.exptime("1500 ms").strip()
    check(reply.startswith("1.500"), f"expected 1.500 from \"1500 ms\", got {reply!r}")


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


def check_raw_needs_enabling(camera: camera_interface.Camera) -> None:
    """Confirm a read is refused until capture is actually enabled."""
    try:
        camera.controller_cmd("raw", "read")
    except RuntimeError:
        return
    raise CheckFailed("raw read was accepted with RAWENABLE=0")


def check_raw_streams_apart(camera: camera_interface.Camera,
                            fits_dir: pathlib.Path,
                            image_files: list[pathlib.Path]) -> None:
    """Confirm a raw capture lands beside the image rather than on top of it.

    RAW carries its own geometry, so sharing an output with the image would
    either overwrite the file or resize the stream.
    """
    before_raw = set(fits_dir.glob("*_raw.fits"))

    # The Archon captures raw alongside the frame, so enabling it has to
    # precede the exposure it should appear in
    camera.controller_cmd("raw", "set RAWENABLE 1")
    camera.expose("1")
    camera.controller_cmd("raw", "read")
    wait_for_writes(camera)

    raw_files = sorted(set(fits_dir.glob("*_raw.fits")) - before_raw)
    check(bool(raw_files), f"no raw FITS file appeared in {fits_dir}")
    for path in raw_files:
        print(f"raw: {path} ({path.stat().st_size} bytes)")
        check(path not in image_files, f"{path} overwrote an image file")


def check_shm_streams_apart(shm_dir: pathlib.Path, segment: str) -> None:
    """Confirm the raw frames went to their own segment, not the image's."""
    image = shm_dir / f"{segment}.im.shm"
    raw = shm_dir / f"{segment}_raw.im.shm"
    check(image.is_file(), f"no image segment at {image}")
    check(raw.is_file(), f"no raw segment at {raw}; raw shared the image stream")
    print(f"segments: {image.name} {image.stat().st_size}B, "
          f"{raw.name} {raw.stat().st_size}B")


def run(config_path: str, fits_dir: pathlib.Path, expected_instrument: str,
        shm_dir: pathlib.Path | None, segment: str | None) -> None:
    """Take exposures through the module and verify they reached disk."""
    check_build_identity(expected_instrument)

    before = set(fits_dir.glob("*.fits"))

    camera = camera_interface.Camera(config_path, log_to_stderr=True)
    camera.open()
    camera.load()
    camera.power("on")

    check_exptime_units(camera)

    camera.exptime(str(EXPTIME_S))
    print(f"power={camera.power()} exptime={camera.exptime()}")

    check_introspection(camera)
    check_raw_needs_enabling(camera)

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
        check_file(path, EXPTIME_S, instrument=expected_instrument != "none")

    check_raw_streams_apart(camera, fits_dir, written)
    if shm_dir is not None and segment is not None:
        check_shm_streams_apart(shm_dir, segment)

    camera.close()


def main() -> int:
    """Parse arguments, run the checks, and report pass or fail."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, help="camerad .cfg to use")
    parser.add_argument("--fits-dir", required=True,
                        help="directory the FITS writer is configured to use")
    parser.add_argument("--instrument", default="none",
                        help="instrument the module is expected to report")
    parser.add_argument("--shm-dir", help="directory the SHM writer is configured to use")
    parser.add_argument("--shm-segment", help="segment name the SHM writer is configured to use")
    args = parser.parse_args()

    try:
        run(args.config, pathlib.Path(args.fits_dir), args.instrument,
            pathlib.Path(args.shm_dir) if args.shm_dir else None,
            args.shm_segment)
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
