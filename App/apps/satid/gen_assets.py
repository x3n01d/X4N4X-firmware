#!/usr/bin/env python3
"""Build the Satellite ID catalog block (app assets) from a CSV file.

usage: gen_assets.py satid_catalog.csv satid_assets.bin
CSV columns: name,norad,freq1_mhz,freq2_mhz,mod,proto,verified
Frequencies are written in units of 10 Hz (what api->rx_freq() returns).
"""
import csv, os, re, struct, sys
from decimal import Decimal, InvalidOperation

HERE = os.path.dirname(os.path.abspath(__file__))
MODS = {"FM": 0, "AM": 1, "SSB": 2}
PROTOS = {"-": 0, "VOICE": 1, "APRS": 2, "APT": 3, "SSTV": 4}


def cdefine(header, name):
    pat = re.compile(r"^\s*#define\s+" + re.escape(name) +
                     r"\s+(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\b")
    with open(os.path.join(HERE, header)) as f:
        for line in f:
            m = pat.match(line)
            if m:
                return int(m.group(1), 0)
    sys.exit(f"{header}: #define {name} not found")


MAGIC = cdefine("satid_cat.h", "CAT_MAGIC")
HDR_SIZE = cdefine("satid_cat.h", "CAT_HDR_SIZE")
REC_SIZE = cdefine("satid_cat.h", "CAT_REC_SIZE")
NAME_LEN = cdefine("satid_cat.h", "CAT_NAME_LEN")
FLAG_TEST = cdefine("satid_cat.h", "CAT_F_TEST")
ASSET_MAX = cdefine("../app_overlay.h", "APP_ASSET_MAX")


def freq(text, row):
    try:
        v = Decimal(text) * 100000
    except InvalidOperation:
        sys.exit(f"row {row}: bad frequency {text!r}")
    if v != v.to_integral_value() or v < 0 or v > 0xFFFFFFFF:
        sys.exit(f"row {row}: frequency {text!r} MHz is not a multiple of 10 Hz")
    return int(v)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    records = []
    with open(src, newline="", encoding="utf-8") as f:
        for n, row in enumerate(csv.DictReader(f), start=2):
            name = row["name"].strip().upper()
            if not name or len(name) > NAME_LEN or not name.isascii():
                sys.exit(f"row {n}: name {name!r} must be 1..{NAME_LEN} ASCII chars")
            mod = MODS.get(row["mod"].strip().upper())
            proto = PROTOS.get(row["proto"].strip().upper())
            if mod is None or proto is None:
                sys.exit(f"row {n}: unknown mod/proto")
            flags = 0 if row["verified"].strip() == "1" else FLAG_TEST
            records.append(struct.pack(
                "<12sIIIBBBB", name.encode("ascii"), int(row["norad"]),
                freq(row["freq1_mhz"].strip(), n), freq(row["freq2_mhz"].strip(), n),
                mod, proto, flags, 0))
    if not records or len(records) > 255:
        sys.exit("catalog must hold 1..255 records")
    blob = struct.pack("<IBBH", MAGIC, len(records), REC_SIZE, 0) + b"".join(records)
    assert len(records[0]) == REC_SIZE and HDR_SIZE == 8
    if len(blob) > ASSET_MAX:
        sys.exit(f"catalog {len(blob)} B exceeds the {ASSET_MAX} B asset area")
    with open(dst, "wb") as f:
        f.write(blob)
    print(f"{dst}: {len(records)} records, {len(blob)} B of {ASSET_MAX} B")


if __name__ == "__main__":
    main()
