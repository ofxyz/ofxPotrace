#!/usr/bin/env python3
"""Download libpotrace sources into libs/potrace (GPL). Run from addon root or anywhere."""
from __future__ import annotations

import ssl
import urllib.request
from pathlib import Path

ADDON_ROOT = Path(__file__).resolve().parent.parent
DEST = ADDON_ROOT / "libs" / "potrace"
BASE = "https://raw.githubusercontent.com/skyrpex/potrace/master"

SRC_FILES = [
    "potracelib.h",
    "potracelib.c",
    "curve.h",
    "curve.c",
    "trace.h",
    "trace.c",
    "decompose.h",
    "decompose.c",
    "lists.h",
    "auxiliary.h",
    "bitmap.h",
    "progress.h",
]


def fetch(url: str) -> bytes:
    ctx = ssl.create_default_context()
    req = urllib.request.Request(url, headers={"User-Agent": "ofxPotrace-vendor/1.0"})
    with urllib.request.urlopen(req, context=ctx, timeout=120) as resp:
        return resp.read()


def main() -> None:
    DEST.mkdir(parents=True, exist_ok=True)
    for name in SRC_FILES:
        url = f"{BASE}/src/{name}"
        data = fetch(url)
        out = DEST / name
        out.write_bytes(data)
        print(f"{name}\t{len(data)}")

    copying = fetch(f"{BASE}/COPYING")
    (DEST / "COPYING").write_bytes(copying)
    print(f"COPYING\t{len(copying)}")
    print(f"Done -> {DEST}")


if __name__ == "__main__":
    main()
