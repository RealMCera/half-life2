"""Inspect and locally stage an original-Xbox Half-LifeX disc image.

Runs inside the xboxrecomp repository; uses its bundled XDVDFS reader.
It does not compile a new XBE or modify a game image.

    py -3 -m tools.half_life1_xbox inspect Half-LifeX_Test_v2.iso --report report.json
    py -3 -m tools.half_life1_xbox prepare Half-LifeX_Test_v2.iso -o local/hl1
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys

from tools.xiso.xdvdfs import SECTOR, Xiso, XisoError


class HalfLifeImageError(ValueError):
    """ISO readable, but it cannot be treated as the expected Half-LifeX build."""


def parse_xbe_header(data):
    """Read only the fields relevant to identifying the user's Xbox executable."""
    if len(data) < 0x184 or data[:4] != b"XBEH":
        raise HalfLifeImageError("default.xbe is not an Xbox executable")
    base = struct.unpack_from("<I", data, 0x104)[0]
    cert_va = struct.unpack_from("<I", data, 0x118)[0]
    sections = struct.unpack_from("<I", data, 0x11C)[0]
    entry_xor = struct.unpack_from("<I", data, 0x128)[0]
    cert_offset = cert_va - base
    title = "(unknown)"
    if 0 <= cert_offset <= len(data) - 0x5C:
        title = data[cert_offset + 12:cert_offset + 92].decode("utf-16le", "replace").rstrip("\x00")
    return {"title": title, "section_count": sections,
            "entry_point_xor": "0x%08x" % entry_xor,
            "executable_sha256": hashlib.sha256(data).hexdigest()}


def read_pak_listing(iso, entry):
    """Read the PACK index without loading the 300 MB archive into RAM."""
    start = iso.base + entry.sector * SECTOR
    iso._f.seek(start)
    header = iso._f.read(12)
    if len(header) != 12 or header[:4] != b"PACK":
        raise HalfLifeImageError("valve/pak0.pak has no PACK header")
    directory_offset, directory_size = struct.unpack_from("<II", header, 4)
    if directory_size % 64 or directory_offset < 12 or directory_offset + directory_size > entry.size:
        raise HalfLifeImageError("valve/pak0.pak has invalid directory bounds")
    if directory_size > 10 * 1024 * 1024:
        raise HalfLifeImageError("valve/pak0.pak directory is unexpectedly large")
    iso._f.seek(start + directory_offset)
    table = iso._f.read(directory_size)
    if len(table) != directory_size:
        raise HalfLifeImageError("valve/pak0.pak is truncated")
    names = []
    for at in range(0, len(table), 64):
        name_bytes, file_offset, file_size = struct.unpack_from("<56sII", table, at)
        if file_offset + file_size > directory_offset:
            raise HalfLifeImageError("PAK entry points past the start of the directory")
        name = name_bytes.split(b"\x00", 1)[0].decode("latin1")
        names.append(name)
    return names


def safe_relative_path(value):
    """Reject traversal, absolute paths, and Windows drive syntax in images."""
    normalized = value.replace("\\", "/")
    parts = normalized.split("/")
    if not normalized or normalized.startswith("/") or any(
        x in ("", ".", "..") or ":" in x for x in parts
    ):
        raise HalfLifeImageError("Unsafe path in image: %r" % value)
    return Path(*parts)


def inspect_image(iso):
    root = iso.find("default.xbe")
    pak = iso.find("pak0.pak", "valve")
    if root is None or root.is_dir or pak is None or pak.is_dir:
        raise HalfLifeImageError("Image must contain default.xbe and valve/pak0.pak")
    if root.size > 64 * 1024 * 1024:
        raise HalfLifeImageError("Unexpectedly large Xbox executable")
    xbe = iso.read(root)
    header = parse_xbe_header(xbe)
    names = read_pak_listing(iso, pak)
    maps = sorted(name for name in names if name.lower().startswith("maps/")
                  and name.lower().endswith(".bsp"))
    all_files = []
    for directory, entry in iso.walk():
        relative = (directory + "/" + entry.name).lstrip("/")
        safe_relative_path(relative)
        all_files.append({"path": relative, "bytes": entry.size})
    for required in ("maps/c0a0.bsp", "maps/t0a0.bsp"):
        if required not in {name.lower() for name in maps}:
            raise HalfLifeImageError("Missing original Half-Life level %s in PAK" % required)
    return {
        "image": os.path.basename(iso.path),
        "partition_base": iso.base,
        "xbox_executable": header,
        "pak": {"bytes": pak.size, "files": len(names), "bsp_maps": len(maps),
                "has_campaign_start": "maps/c0a0.bsp" in {x.lower() for x in maps},
                "has_training_start": "maps/t0a0.bsp" in {x.lower() for x in maps}},
        "disc": {"files": len(all_files), "bytes": sum(x["bytes"] for x in all_files)},
        "file_list": all_files,
    }


def prepare_image(iso, output):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    count = 0
    for directory, entry in iso.walk():
        relative = safe_relative_path((directory + "/" + entry.name).lstrip("/"))
        destination = (output / relative).resolve()
        if not destination.is_relative_to(output):
            raise HalfLifeImageError("Unexpected extraction target: %s" % destination)
        iso.extract(entry, str(destination))
        count += 1
    return count


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    for command in ("inspect", "prepare"):
        p = sub.add_parser(command)
        p.add_argument("iso", help="Half-LifeX original-Xbox XISO path")
        p.add_argument("--report", help="Write a local JSON inspection report")
        if command == "prepare":
            p.add_argument("-o", "--output", required=True, help="Local extraction directory (do not commit)")
    args = ap.parse_args(argv)
    try:
        with Xiso(args.iso) as iso:
            result = inspect_image(iso)
            if args.command == "prepare":
                result["extracted_files"] = prepare_image(iso, args.output)
                result["extracted_to"] = str(Path(args.output).resolve())
        if args.report:
            path = Path(args.report)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            print("Inspection report:", path)
        print("Xbox title:", result["xbox_executable"]["title"])
        print("Disc files:", result["disc"]["files"], "| PAK files:", result["pak"]["files"])
        print("BSP maps:", result["pak"]["bsp_maps"])
        print("XBE SHA-256:", result["xbox_executable"]["executable_sha256"])
        if args.command == "prepare":
            print("Extracted files:", result["extracted_files"], "to", result["extracted_to"])
        return 0
    except (XisoError, HalfLifeImageError, OSError) as exc:
        ap.exit(2, "Error: %s\n" % exc)


if __name__ == "__main__":
    sys.exit(main())
