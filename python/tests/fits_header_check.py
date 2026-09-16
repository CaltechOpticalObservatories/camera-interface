"""Assert the HISPEC ATC keywords in a FITS file the writer just produced.

Every keyword here is a row of the ATC header definition document. The values are
what the emulator produces from MODE_DEFAULT of hispecatc.acf, so a keyword that
silently stops being populated, or starts reporting a placeholder, fails the run.

Used by both emulator CI jobs, and standalone:
    python3 python/tests/fits_header_check.py /tmp/ci_fits_test/*.fits
"""

from __future__ import annotations

import argparse
import datetime
import math
import pathlib
import sys
from typing import Final

CARD_LENGTH: Final = 80
BLOCK_LENGTH: Final = 2880

# CHANVERT is absent by design: the document marks it N/A for the ATC
REQUIRED_KEYWORDS: Final = frozenset({
    "ACQTIME", "CAMD_VER", "DETBITS", "EXPMJDST", "EXPTIME", "EXPTUNIT", "FILENAME",
    "FILETYPE", "FIRMWARE", "FRAMETME", "LVLC_V1", "LVLC_V2", "LVLC_V3",
    "NCHANLS", "NREADS", "OPSMODE", "PIXTIME", "READMODE", "REFCHPOS",
    "REFPXAMP", "SKIPLNES", "SKIPROWS", "SUBFRAME",
})

EXPECTED_STRINGS: Final = {
    "EXPTUNIT": "sec",
    "READMODE": "UpTheRampRollingReset",
    "SUBFRAME": "fullframe",
    "OPSMODE": "DEFAULT",
    "REFCHPOS": "RIGHT",
    "FILETYPE": "FITS",
    "FREERUN": "F",
}

# NCHANLS and REFPXAMP come from MODE_DEFAULT's 5 taplines, FRAMETME from
# 2048 lines x 512 pixels per tap at PIXTIME
EXPECTED_NUMBERS: Final = {
    "DETBITS": 16.0,
    "NCHANLS": 4.0,
    "REFPXAMP": 5.0,
    "PIXTIME": 5.92,
    "FRAMETME": 6.20756992,
    "SKIPROWS": 0.0,
    "SKIPLNES": 0.0,
    "NREADS": 1.0,
}

# Any exposure this test writes is later than this, so a zeroed or unset date fails
MJD_2020: Final = 58849.0


class CheckFailed(Exception):
    """Raised when a header check does not hold."""


def check(condition: bool, message: str) -> None:
    """Raise CheckFailed with message unless condition holds."""
    if not condition:
        raise CheckFailed(message)


def card_value(field: str) -> str:
    """Strip quoting and the trailing comment from a card's value field."""
    stripped = field.strip()
    if not stripped.startswith("'"):
        return stripped.split("/", 1)[0].strip()
    end = stripped.find("'", 1)
    return stripped[1:end].strip() if end > 0 else stripped[1:].strip()


def read_primary_header(path: pathlib.Path) -> dict[str, str]:
    """Return the primary HDU's keyword/value pairs as strings.

    Parsed here rather than with astropy so the check has no dependency CI would
    have to install. Value cards only; COMMENT and HISTORY are skipped.
    """
    header: dict[str, str] = {}
    with path.open("rb") as handle:
        while len(block := handle.read(BLOCK_LENGTH)) == BLOCK_LENGTH:
            for offset in range(0, BLOCK_LENGTH, CARD_LENGTH):
                card = block[offset:offset + CARD_LENGTH].decode("ascii", errors="replace")
                if card[:8].strip() == "END":
                    return header
                if card[8:10] == "= ":
                    header[card[:8].strip()] = card_value(card[10:])
    raise CheckFailed(f"{path.name}: no END card in primary header")


def as_number(header: dict[str, str], keyword: str) -> float:
    """Return a keyword's value as a float, failing the check if it is not numeric."""
    try:
        return float(header[keyword])
    except ValueError as error:
        raise CheckFailed(f"{keyword}={header[keyword]!r} is not a number") from error


def check_values(path: pathlib.Path, header: dict[str, str], exptime: float) -> None:
    """Assert the keywords whose values the emulator determines."""
    # FREERUN is camerad's own keyword, not a document row, so it is only
    # checked when the build writing the file happens to emit it
    for keyword, expected in EXPECTED_STRINGS.items():
        if keyword in header:
            check(header[keyword] == expected,
                  f"{keyword}={header[keyword]!r}, expected {expected!r}")

    for keyword, expected in EXPECTED_NUMBERS.items():
        value = as_number(header, keyword)
        check(math.isclose(value, expected, rel_tol=1e-9),
              f"{keyword}={value}, expected {expected}")

    check(math.isclose(as_number(header, "EXPTIME"), exptime, rel_tol=1e-9),
          f"EXPTIME={header['EXPTIME']}, expected {exptime}")
    check(header["FILENAME"] == path.name,
          f"FILENAME={header['FILENAME']!r}, expected {path.name!r}")
    check(as_number(header, "EXPMJDST") > MJD_2020,
          f"EXPMJDST={header['EXPMJDST']} predates 2020")
    check(header["FIRMWARE"].endswith(".acf"),
          f"FIRMWARE={header['FIRMWARE']!r} is not an ACF path")
    check(bool(header["CAMD_VER"]), "CAMD_VER is empty")

    # Bias voltages are pulled from the ACF, so a parse that silently missed
    # them would otherwise leave a plausible-looking zero
    for keyword in ("LVLC_V1", "LVLC_V2", "LVLC_V3"):
        check(as_number(header, keyword) != 0.0, f"{keyword} is zero")

    try:
        datetime.datetime.fromisoformat(header["ACQTIME"])
    except ValueError as error:
        raise CheckFailed(f"ACQTIME={header['ACQTIME']!r} is not ISO 8601") from error


def check_file(path: pathlib.Path, exptime: float) -> None:
    """Assert every required keyword is present in path, with the expected value."""
    header = read_primary_header(path)
    missing = sorted(REQUIRED_KEYWORDS - header.keys())
    check(not missing, f"{path.name}: missing keywords {missing}")
    check_values(path, header, exptime)
    print(f"headers ok: {path.name} ({len(header)} cards)")


def main() -> int:
    """Parse arguments, check every file given, and report pass or fail."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=pathlib.Path, help="FITS files to check")
    parser.add_argument("--exptime", type=float, default=0.0,
                        help="exposure time the file was taken with, in sec")
    args = parser.parse_args()

    try:
        for path in args.paths:
            check_file(path, args.exptime)
    except CheckFailed as failure:
        print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
