"""Read frames from the camerad ImageStreamIO shared-memory segment.

camerad's SharedMemoryWriter publishes every frame into an ImageStreamIO stream
named by SHM_SEGMENT_NAME, carrying FRAMENO, TIMESTMP and SEQNUM as stream
keywords. This script attaches to that stream, blocks on its semaphore, and
reports each frame as it arrives.

Needs numpy and the ImageStreamIOWrap module, the latter built from
milk-org/ImageStreamIO with -DPYTHON_WRAPPER=ON since it is not on PyPI, and
camerad running with SHM_ENABLED=yes.

    python shm_read_frames.py --segment hispec_tracking_camera --count 10
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from ImageStreamIOWrap import Image

DEFAULT_SEGMENT = "hispec_tracking_camera"
DEFAULT_SEGMENT_TIMEOUT_S = 30.0
IMAGESTREAMIO_SUCCESS = 0
SEGMENT_POLL_INTERVAL_S = 0.2
UNSET_KEYWORD_TYPE = "N"

# TIMESTMP is the Archon counter, which ticks every 0.01 us
ARCHON_TICK_SECONDS = 1e-8


@dataclass(frozen=True)
class Frame:
    """One frame copied out of the stream, with the keywords that came with it."""

    pixels: np.ndarray
    counter: int
    frameno: int
    timestamp: int
    seqnum: int


class SegmentUnavailable(Exception):
    """Raised when the named stream never appears."""


def open_segment(name: str, timeout_s: float) -> Image:
    """Attach to the named stream, waiting for camerad to create it."""
    # The writer sizes the stream from the first frame it publishes, so the
    # segment does not exist until camerad has actually exposed once
    deadline = time.monotonic() + timeout_s
    image = Image()
    while image.open(name) != IMAGESTREAMIO_SUCCESS:
        if time.monotonic() >= deadline:
            raise SegmentUnavailable(f"stream {name!r} did not appear within {timeout_s}s")
        time.sleep(SEGMENT_POLL_INTERVAL_S)
    return image


def keyword_values(image: Image) -> dict[str, int | float | str]:
    """Return the stream's populated keywords, keyed by name."""
    return {keyword.name.strip(): keyword.value
            for keyword in image.kw
            if keyword.type != UNSET_KEYWORD_TYPE}


def read_frames(image: Image, count: int | None) -> Iterator[Frame]:
    """Yield frames as the writer publishes them, blocking in between."""
    semaphore = image.getsemwaitindex(0)
    image.semflush(semaphore)

    yielded = 0
    while count is None or yielded < count:
        image.semwait(semaphore)

        # Nothing pins the live buffer, so a reader slower than the writer gets
        # overtaken mid-copy; the cnt0 gap in report() is how that shows up
        pixels = image.copy()
        keywords = keyword_values(image)

        yield Frame(
            pixels=pixels,
            counter=int(image.md.cnt0),
            frameno=int(keywords.get("FRAMENO", -1)),
            timestamp=int(keywords.get("TIMESTMP", -1)),
            seqnum=int(keywords.get("SEQNUM", -1)),
        )
        yielded += 1


def report(frame: Frame, previous: Frame | None) -> None:
    """Print one line per frame, with the cadence and any gap since the last one."""
    pixels = frame.pixels
    fields = [f"cnt0={frame.counter}",
              f"FRAMENO={frame.frameno}",
              f"SEQNUM={frame.seqnum}",
              f"shape={pixels.shape}",
              f"dtype={pixels.dtype}",
              f"min={pixels.min()}",
              f"max={pixels.max()}",
              f"mean={pixels.mean():.1f}"]

    if previous is not None:
        period_s = (frame.timestamp - previous.timestamp) * ARCHON_TICK_SECONDS
        if period_s > 0:
            fields.append(f"period={period_s * 1e3:.3f}ms ({1.0 / period_s:.1f} Hz)")
        missed = frame.counter - previous.counter - 1
        if missed > 0:
            fields.append(f"MISSED={missed}")

    print(" ".join(fields), flush=True)


def describe_segment(name: str, image: Image) -> None:
    """Print the stream geometry the writer created."""
    metadata = image.md
    print(f"segment={name} size={tuple(metadata.size[:metadata.naxis])} "
          f"datatype={metadata.datatype} cnt0={metadata.cnt0}", flush=True)


def parse_arguments() -> argparse.Namespace:
    """Parse the command line."""
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--segment", default=DEFAULT_SEGMENT,
                        help="stream name, matching SHM_SEGMENT_NAME in the camerad .cfg")
    parser.add_argument("--dir",
                        help="ImageStreamIO base directory, matching SHM_DIR in the .cfg")
    parser.add_argument("--count", type=int,
                        help="stop after this many frames (default: until interrupted)")
    parser.add_argument("--timeout", type=float, default=DEFAULT_SEGMENT_TIMEOUT_S,
                        help="seconds to wait for the segment to appear")
    parser.add_argument("--save-dir", type=Path,
                        help="write each frame here as frame_<FRAMENO>.npy")
    return parser.parse_args()


def main() -> int:
    """Attach to the segment and report frames until the count or an interrupt."""
    args = parse_arguments()

    if args.dir:
        # ImageStreamIO reads this when resolving the segment path, so it has to
        # be set before the first open
        os.environ["MILK_SHM_DIR"] = args.dir

    try:
        image = open_segment(args.segment, args.timeout)
    except SegmentUnavailable as failure:
        print(f"ERROR {failure}", file=sys.stderr)
        return 1

    describe_segment(args.segment, image)

    if args.save_dir:
        args.save_dir.mkdir(parents=True, exist_ok=True)

    previous: Frame | None = None
    try:
        for frame in read_frames(image, args.count):
            report(frame, previous)
            if args.save_dir:
                np.save(args.save_dir / f"frame_{frame.frameno:08d}.npy", frame.pixels)
            previous = frame
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
    finally:
        image.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
