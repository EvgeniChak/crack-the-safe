#!/usr/bin/env python3
"""
LZ4-compress the EEZ Studio image exports in main/ui/, in place.

The door frames, prizes and police lights are ~6.7 MB as raw LVGL pixel
arrays -- most of the app, and too much for two 7.94 MB update slots.
LZ4 brings them to ~2.6 MB. LVGL decompresses an image into PSRAM when it is
drawn (LV_BIN_DECODER_RAM_LOAD, LV_USE_LZ4 in sdkconfig.defaults) and keeps
recent ones in its image cache.

Run it after every EEZ Studio export (Ctrl+B), which writes the images out
uncompressed again:

    pip install lz4
    python tools/compress_images.py

Already-compressed files are left alone, so it is safe to run twice.
The build fails with a pointer to this script if an uncompressed image is
left in main/ui/ (see main/CMakeLists.txt).

    python tools/compress_images.py --check    # exit 1 if any is left
"""

import argparse
import glob
import os
import re
import struct
import sys

LZ4_METHOD = 2                      # lv_image_compress_t: LV_IMAGE_COMPRESS_LZ4
FLAG_COMPRESSED = "LV_IMAGE_FLAGS_COMPRESSED"
HERE = os.path.dirname(os.path.abspath(__file__))
UI_DIR = os.path.join(HERE, "..", "main", "ui")

ARRAY_RE = re.compile(r"(uint8_t\s+\w+_map\[\]\s*=\s*\{)(.*?)(\};)", re.S)
FLAGS_RE = re.compile(r"\.flags\s*=\s*[^,]+,")


def is_compressed(text: str) -> bool:
    return FLAG_COMPRESSED in text


def compress_file(path: str) -> tuple[int, int]:
    import lz4.block

    text = open(path, encoding="utf-8").read()
    m = ARRAY_RE.search(text)
    if not m or not FLAGS_RE.search(text):
        raise ValueError(f"{path}: not an LVGL image export")
    raw = bytes(int(h, 16) for h in re.findall(r"0x([0-9a-fA-F]{2})", m.group(2)))
    packed = lz4.block.compress(raw, store_size=False)
    if lz4.block.decompress(packed, uncompressed_size=len(raw)) != raw:
        raise ValueError(f"{path}: LZ4 round trip failed")

    # lv_image_compressed_t as the bin decoder reads it: method, compressed
    # size, decompressed size (uint32 LE each), then the LZ4 block.
    data = struct.pack("<III", LZ4_METHOD, len(packed), len(raw)) + packed
    rows = []
    for i in range(0, len(data), 24):
        rows.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 24]) + ",")
    body = "\n" + "\n".join(rows) + "\n"

    text = text[:m.start(2)] + body + text[m.end(2):]
    text = FLAGS_RE.sub(f".flags = {FLAG_COMPRESSED},", text, count=1)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    return len(raw), len(data)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true",
                    help="only report uncompressed images; exit 1 if any")
    ap.add_argument("--dir", default=UI_DIR, help="EEZ export folder (main/ui)")
    args = ap.parse_args()

    files = sorted(glob.glob(os.path.join(args.dir, "ui_image_*.c")))
    todo = [f for f in files if not is_compressed(open(f, encoding="utf-8").read())]

    if args.check:
        for f in todo:
            print(f"uncompressed: {os.path.basename(f)}")
        if todo:
            print("main/ui has uncompressed images (a fresh EEZ Studio export?).\n"
                  "Run:  pip install lz4  then  python tools/compress_images.py",
                  file=sys.stderr)
        return 1 if todo else 0

    if not todo:
        print(f"all {len(files)} images already compressed")
        return 0
    try:
        import lz4.block  # noqa: F401
    except ImportError:
        print("needs the lz4 package:  pip install lz4", file=sys.stderr)
        return 2

    before = after = 0
    for f in todo:
        a, b = compress_file(f)
        before += a
        after += b
        print(f"{os.path.basename(f):28s} {a:8d} -> {b:8d}  ({b * 100 // a}%)")
    print(f"{len(todo)} images: {before / 1e6:.2f} MB -> {after / 1e6:.2f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
