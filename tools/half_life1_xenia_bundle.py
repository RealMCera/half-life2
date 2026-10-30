"""Stage one native Xbox 360 XEX for Windows Xenia and Xbox Series S Xenia UWP.

This does not compile the executable or prove either emulator can run it.
The second target (third-party Xenia UWP) has not been proven to load
xsdk homebrew XEXs. Keep assets and archives private and out of GitHub.

python -m tools.half_life1_xenia_bundle stage --xex local/build/hello.xex \
    --scene local/x360-remaster/c0a0 --out local/xenia-tests/first-build
python -m tools.half_life1_xenia_bundle verify --out local/xenia-tests/first-build
python -m tools.half_life1_xenia_bundle launch-pc \
    --xenia C:/Tools/Xenia/xenia_canary.exe --out local/xenia-tests/first-build
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile


class BundleError(ValueError):
    pass


MAX_ASSET_BYTES = 512 * 1024 * 1024
ALLOWED_TEXTURE_FILES = {".png", ".dds", ".tga"}


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(part)
    return digest.hexdigest()


def _private_file(src):
    if src.is_symlink() or not src.is_file():
        raise BundleError("Refusing symlink or non-file: " + str(src))
    if src.stat().st_size > MAX_ASSET_BYTES:
        raise BundleError("Single file is too large for prototype bundle: " + str(src))
    return src


def _scene_sources(scene):
    scene = Path(scene)
    if scene.is_symlink() or not scene.is_dir():
        raise BundleError("Scene must be a real directory")
    meshes = sorted(scene.glob("hl1_*_world.h360"))
    if len(meshes) != 1:
        raise BundleError("Scene needs exactly one hl1_*_world.h360 file")
    required = [meshes[0], scene / "material_manifest.json"]
    if (scene / "x360_scene_report.json").exists():
        required.append(scene / "x360_scene_report.json")
    for item in required:
        _private_file(item)
    textures = scene / "materials"
    if textures.exists():
        if textures.is_symlink() or not textures.is_dir():
            raise BundleError("materials is not a regular directory")
        for src in sorted(textures.rglob("*")):
            if src.is_symlink():
                raise BundleError("Refusing symlink in materials")
            if src.is_file():
                if src.suffix.lower() not in ALLOWED_TEXTURE_FILES:
                    raise BundleError("Unexpected material file type: " + str(src))
                required.append(_private_file(src))
    return [(p, "assets/" + p.relative_to(scene).as_posix()) for p in required]


def stage(xex, out, scene=None):
    xex = _private_file(Path(xex).resolve())
    if xex.suffix.lower() != ".xex":
        raise BundleError("Only a genuine Xbox 360 .xex should be staged")
    with xex.open("rb") as stream:
        signature = stream.read(4)
    if xex.stat().st_size < 4 or signature != b"XEX2":
        raise BundleError("XEX must start with the XEX2 file signature")
    out = Path(out).resolve()
    payload = out / "payload"
    if out.exists():
        raise BundleError("Output directory already exists; choose a new build name")
    sources = [(xex, "default.xex")]
    if scene is not None:
        sources.extend(_scene_sources(scene))
    for source, _ in sources:
        if out == source or out in source.parents:
            raise BundleError("Output directory cannot contain a source file")
    total = sum(source.stat().st_size for source, _ in sources)
    if total > MAX_ASSET_BYTES:
        raise BundleError("Prototype bundle exceeds 512 MiB limit")
    seen = set()
    for _, relative in sources:
        if relative.lower() in seen:
            raise BundleError("Case-insensitive duplicate payload path")
        seen.add(relative.lower())
    payload.mkdir(parents=True)
    entries = []
    for source, relative in sources:
        target = payload / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        entries.append({"path": relative, "bytes": target.stat().st_size, "sha256": _sha256(target)})
    manifest = {
        "schema": "hl1-xenia-dual-target-v1",
        "binary_type": "Xbox 360 XEX2",
        "test_targets": ["windows-xenia-canary", "xbox-series-s-xenia-uwp-unverified"],
        "source_xex_name": xex.name,
        "files": entries,
        "warning": "Packaging only. Neither emulator launch nor gameplay is verified. UWP custom homebrew support may differ.",
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    archive = out / "xenia_payload.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for entry in entries:
            zf.write(payload / entry["path"], arcname=entry["path"])
    verify(out)
    return manifest


def verify(out):
    out = Path(out).resolve()
    manifest = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != "hl1-xenia-dual-target-v1":
        raise BundleError("Unknown bundle manifest format")
    payload = out / "payload"
    files = manifest.get("files")
    if not isinstance(files, list) or not files:
        raise BundleError("Missing manifest entries")
    expected = set()
    for entry in files:
        relative = entry["path"]
        relpath = Path(relative)
        if relpath.is_absolute() or ".." in relpath.parts or "\\" in relative or not relative:
            raise BundleError("Unsafe relative payload path")
        expected.add(relative)
        file = payload / relpath
        _private_file(file)
        if file.stat().st_size != entry["bytes"] or _sha256(file) != entry["sha256"]:
            raise BundleError("Size/hash mismatch: " + relative)
    actual = {p.relative_to(payload).as_posix() for p in payload.rglob("*") if p.is_file()}
    if expected != actual:
        raise BundleError("Payload contents differ from manifest")
    if "default.xex" not in expected:
        raise BundleError("No default.xex in staged payload")
    return len(expected)


def launch_pc(out, xenia, additional_args=()):
    out = Path(out).resolve()
    verify(out)
    xenia = _private_file(Path(xenia).resolve())
    if xenia.suffix.lower() != ".exe":
        raise BundleError("Use an actual Windows Xenia executable")
    game = out / "payload" / "default.xex"
    # Startup is a smoke test only. Visible rendering/logs must be verified.
    command = [str(xenia), str(game), *additional_args]
    proc = subprocess.Popen(command, cwd=xenia.parent)
    return proc.pid, command


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p_stage = sub.add_parser("stage")
    p_stage.add_argument("--xex", required=True)
    p_stage.add_argument("--scene")
    p_stage.add_argument("--out", required=True)
    p_verify = sub.add_parser("verify")
    p_verify.add_argument("--out", required=True)
    p_launch = sub.add_parser("launch-pc")
    p_launch.add_argument("--out", required=True)
    p_launch.add_argument("--xenia", required=True)
    p_launch.add_argument("--gpu-readback", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.command == "stage":
            result = stage(args.xex, args.out, args.scene)
            print("Staged %d files for both Xenia test paths" % len(result["files"]))
            print("Windows: test payload/default.xex in Xenia Canary")
            print("Xbox Series S: copy payload contents for Xenia UWP compatibility testing (unverified)")
        elif args.command == "verify":
            print("Verified %d payload files and SHA-256 hashes" % verify(args.out))
        else:
            flags = ("--readback_resolve=full", "--async_shader_compilation=false") if args.gpu_readback else ()
            pid, cmd = launch_pc(args.out, args.xenia, flags)
            print("Started Xenia process %d; inspect Xenia logs and visible rendering before claiming success." % pid)
            print("Command:", subprocess.list2cmdline(cmd))
        return 0
    except (BundleError, OSError, KeyError, ValueError, json.JSONDecodeError) as exc:
        parser.exit(2, "Error: %s\n" % exc)


if __name__ == "__main__":
    sys.exit(main())
