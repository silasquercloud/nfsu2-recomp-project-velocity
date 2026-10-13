#!/usr/bin/env python3
"""Developer-only build of the complete port; players only install/import game data.

Use the signed Release APK path. Never replace absent lifted game C with
stubs, an official prebuilt game library, or a launcher-only native target.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

VERSION = "0.14.0-velocity.4"
VERSION_CODE = "18"
PACKAGE = "com.nfsu2.androidevolved"
APK_NAME = f"NFSU2-Project-Velocity-ARM64-v{VERSION}.apk"
LIBRARIES = {
    "libnfsu2_android.so", "libnfsu2_diagnostics.so", "libmain_hook.so",
    "libfile_redirect_hook.so", "libgsl_alloc_hook.so", "libhook_impl.so",
}


class BuildBlocked(RuntimeError):
    pass


def check_generated_sources(root):
    generated = root / "src/recomp/gen"
    required = [generated / name for name in
                ("recomp_funcs.h", "recomp_dispatch.c", "recomp_imports.c", "lift_stats.json")]
    chunks = sorted(generated.glob("recomp_[0-9]*.c"))
    missing = [str(path.relative_to(root)) for path in required
               if not path.is_file() or path.stat().st_size == 0]
    if not chunks or any(path.stat().st_size == 0 for path in chunks):
        missing.append("src/recomp/gen/recomp_[0-9]*.c (nonempty lifted functions)")
    if missing:
        raise BuildBlocked(
            "The complete upstream native target requires developer-generated game C.\n"
            "Missing: " + ", ".join(missing) + "\n"
            "The developer must obtain authorized generated inputs or run the upstream\n"
            "generation pipeline in their build workspace. This is not a player step.\n"
            "The launcher import workflow remains unchanged. No substitute APK is built.")
    stats = json.loads((generated / "lift_stats.json").read_text())
    if stats.get("lift_failures") != 0 or stats.get("failures"):
        raise BuildBlocked("The original generator reported lift failures; do not package its failure bodies")
    if stats.get("files") != len(chunks) or not stats.get("functions"):
        raise BuildBlocked("Generated source metadata does not match the lifted compilation units")
    return len(chunks)


def inspect_contents(apk, require_velocity=True):
    """Check packaging without claiming startup or gameplay functionality."""
    with zipfile.ZipFile(apk) as archive:
        corrupted = archive.testzip()
        if corrupted:
            raise BuildBlocked(f"Corrupt ZIP entry: {corrupted}")
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise BuildBlocked("Duplicate APK ZIP entries")
        native = {name.rsplit("/", 1)[-1]: name for name in names
                  if name.startswith("lib/arm64-v8a/") and name.endswith(".so")}
        if not LIBRARIES <= native.keys():
            raise BuildBlocked("Incomplete native packaging: " + ", ".join(sorted(LIBRARIES - native.keys())))
        for name in names:
            if name.startswith("lib/") and name.endswith(".so"):
                if not name.startswith("lib/arm64-v8a/"):
                    raise BuildBlocked("Unexpected ABI: " + name)
                elf = archive.read(name)
                if (elf[:4] != b"\x7fELF" or elf[4:6] != b"\x02\x01"
                        or int.from_bytes(elf[18:20], "little") != 183):
                    raise BuildBlocked("Native library is not little-endian ELF64 AArch64: " + name)
            if name.lower().endswith((".exe", ".dll", ".bun", ".big", ".vp6", ".mad", ".asf")):
                raise BuildBlocked("Game-file entry must not be packaged: " + name)
        if require_velocity:
            game = archive.read(native["libnfsu2_android.so"])
            if b"Project Velocity 0.14.0-velocity.4" not in game:
                raise BuildBlocked("Native Velocity profiling marker missing; refuse an unchanged official library")
    return sorted(native)


def verify_apk(apk, tools):
    libraries = inspect_contents(apk)
    badging = subprocess.check_output([str(tools / "aapt"), "dump", "badging", str(apk)], text=True)
    line = next((line for line in badging.splitlines() if line.startswith("package:")), "")
    fields = dict(re.findall(r"(\w+)='([^']*)'", line))
    if (fields.get("name") != PACKAGE or fields.get("versionName") != VERSION
            or fields.get("versionCode") != VERSION_CODE):
        raise BuildBlocked("Unexpected package/version: " + line)
    subprocess.run([str(tools / "zipalign"), "-c", "-P", "16", "4", str(apk)], check=True)
    subprocess.run(["bash", str(tools / "apksigner"), "verify", "--verbose", str(apk)], check=True)
    checksum = hashlib.sha256()
    with apk.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    digest = checksum.hexdigest()
    return libraries, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, help="Defaults to ignored build/android")
    parser.add_argument("--gradle-runner", type=Path,
                        help="Optional trusted Python wrapper for proxy/TLS setup; not for players")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    chunks = check_generated_sources(root)
    # The original ARM64 target includes adrenotools and its pinned nested
    # linker namespace dependency. Do not replace them with launcher stubs.
    dependencies = [root / "third_party/libadrenotools/CMakeLists.txt",
                    root / "third_party/libadrenotools/lib/linkernsbypass/CMakeLists.txt"]
    if not all(path.is_file() for path in dependencies):
        subprocess.run(["git", "submodule", "update", "--init", "--recursive"], cwd=root, check=True)
    sdk_value = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    if not sdk_value:
        raise BuildBlocked("Set ANDROID_HOME to the developer's prepared Android SDK")
    sdk = Path(sdk_value)
    tools = sdk / "build-tools/35.0.0"
    if not all((tools / name).is_file() for name in ("aapt", "apksigner", "zipalign")):
        raise BuildBlocked("Install Android SDK Build-Tools 35.0.0 for APK verification")
    print(f"Building the complete upstream target from {chunks} local lifted C units.", flush=True)
    built = root / "android/app/build/outputs/apk/release/app-release.apk"
    # AGP incremental ZIP updates can leave an entire old native payload as
    # unused archive space. Refresh only the generated APK, retaining C/LTO caches.
    # Preflight above runs before removing any previous generated package.
    built.unlink(missing_ok=True)
    command = ([sys.executable, str(args.gradle_runner.resolve())] if args.gradle_runner else
               ["bash", str(root / "android/gradlew"), "--no-daemon"])
    subprocess.run(command + [":app:assembleRelease", "-PnfsAbi=arm64-v8a", "-PnfsVelocityProfiling=true"],
                   cwd=root / "android", check=True)
    if not built.is_file():
        raise BuildBlocked("Gradle did not produce its Release APK")
    output = (args.output_dir or root / "build/android").resolve()
    output.mkdir(parents=True, exist_ok=True)
    # Verify the copied bytes before exposing the requested filename.
    with tempfile.NamedTemporaryFile(dir=output, suffix=".apk", delete=False) as handle:
        temporary = Path(handle.name)
    try:
        shutil.copyfile(built, temporary)
        libraries, digest = verify_apk(temporary, tools)
        destination = output / APK_NAME
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"APK: {destination}\nVersion: {VERSION}\nABI: arm64-v8a\n"
          f"Bytes: {destination.stat().st_size}\nSHA-256: {digest}\n"
          f"Native libraries: {', '.join(libraries)}\n"
          "Release build with development/test signature. Gameplay has not been verified by these packaging checks.")


if __name__ == "__main__":
    try:
        main()
    except (BuildBlocked, subprocess.CalledProcessError, OSError, zipfile.BadZipFile, json.JSONDecodeError) as error:
        print(f"Velocity APK build blocked: {error}", file=sys.stderr)
        sys.exit(1)
