#!/usr/bin/env python3
import csv, os, re, struct, sys
from decimal import Decimal

HERE = os.path.dirname(os.path.abspath(__file__))
MODES = {"NONE":0,"FM":1,"AM":2,"SSB":3,"AFSK":4,"FSK":5,"GFSK":6,
         "GMSK":7,"MSK":8,"BPSK":9,"LORA":10,"CW":11,"APT":12,
         "SSTV":13,"DATA":14}
PROTOS = {"NONE":0,"VOICE":1,"APRS":2,"APT":3,"SSTV":4,
          "TELEMETRY":5,"CW":6,"DATA":7}

def define(header, name):
    pat = re.compile(r"^\s*#define\s+"+re.escape(name)+r"\s+(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\b")
    for line in open(os.path.join(HERE, header), encoding="ascii"):
        m = pat.match(line)
        if m:
            return int(m.group(1), 0)
    raise SystemExit("Missing define: "+name)

MAGIC = define("satid_cat.h", "CAT_MAGIC")
ASSET_MAX = define("../app_overlay.h", "APP_ASSET_MAX")

def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: gen_assets.py satid_catalog.csv satid_assets.bin")
    records, seen = [], set()
    with open(sys.argv[1], newline="", encoding="utf-8") as f:
        for n, r in enumerate(csv.DictReader(f), 2):
            name = r["name"].strip().upper()
            if not name or len(name) >= 12 or not name.isascii():
                raise SystemExit(f"row {n}: name must be under 12 ASCII characters")
            norad = int(r["norad"])
            if norad <= 0 or norad in seen:
                raise SystemExit(f"row {n}: invalid or duplicate NORAD ID {norad}")
            seen.add(norad)
            mode, proto = r["mode"].upper(), r["proto"].upper()
            if mode not in MODES or proto not in PROTOS:
                raise SystemExit(f"row {n}: unknown mode/protocol {mode}/{proto}")
            def hz(key):
                x = Decimal(r[key] or "0") * 100000
                if x < 0 or x > 0xFFFFFFFF or x != x.to_integral_value():
                    raise SystemExit(f"row {n}: invalid frequency")
                return int(x)
            src = r["source"].upper()
            flags = {"AMSAT":1,"TRACKER":2,"LEGACY":4}.get(src)
            if flags is None:
                raise SystemExit(f"row {n}: unknown source {src}")
            f1, f2 = hz("freq1_mhz"), hz("freq2_mhz")
            if src == "TRACKER" and (f1 or f2):
                raise SystemExit(f"row {n}: tracker-only record has RX frequency")
            records.append(struct.pack("<12sIIIBBBB", name.encode(), norad,
                f1, f2, MODES[mode], PROTOS[proto], flags, 0))
    if not records or len(records) > 255:
        raise SystemExit("catalog must contain 1..255 records")
    blob = struct.pack("<IBBH", MAGIC, len(records), 28, 0) + b"".join(records)
    if len(blob) > ASSET_MAX:
        raise SystemExit(f"catalog is {len(blob)} bytes; limit is {ASSET_MAX}")
    with open(sys.argv[2], "wb") as f:
        f.write(blob)
    print(f"{len(records)} records; {len(blob)} of {ASSET_MAX} asset bytes")

if __name__ == "__main__":
    main()
