#!/usr/bin/env python3
"""Package the Linux or macOS build of Interference into dist/ (archive + SHA-256).

Usage: python3 tools/package_release.py <linux|macos> [build_dir]

Mirrors tools/make_release.ps1 for the Windows ZIP: plug-in bundles, stand-alone app, user documentation and the
third-party licence texts (copied from the JUCE checkout). Binaries are NOT signed here; a macOS build is ad-hoc
signed in CI so Apple Silicon will load it, which is not notarisation (see SIGNATURE.txt in the archive).
"""
import hashlib, os, re, shutil, subprocess, sys, tarfile, tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
plat = sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__)
bd = Path(sys.argv[2]) if len(sys.argv) > 2 else root / "build"
if plat not in ("linux", "macos"):
    sys.exit("platform must be linux or macos")

version = re.search(r"project\(Interference VERSION (\d+\.\d+\.\d+)", (root / "CMakeLists.txt").read_text()).group(1)
name = f"Interference-{version}-{plat}"
art = bd / "Interference_artefacts" / "Release"
if not art.is_dir():
    art = bd / "Interference_artefacts"          # single-config generators
want = {"linux": [("VST3/Interference.vst3", "VST3/Interference.vst3"), ("Standalone/Interference", "Standalone/Interference")],
        "macos": [("VST3/Interference.vst3", "VST3/Interference.vst3"), ("AU/Interference.component", "AU/Interference.component"),
                  ("Standalone/Interference.app", "Standalone/Interference.app")]}[plat]

stage_root = Path(tempfile.mkdtemp(prefix="ifx-pkg-"))
stage = stage_root / name
stage.mkdir(parents=True)
for src, dst in want:
    s = art / src
    if not s.exists():
        sys.exit(f"missing build artifact: {s}")
    (stage / dst).parent.mkdir(parents=True, exist_ok=True)
    if s.is_dir():
        shutil.copytree(s, stage / dst, symlinks=True)
    else:
        shutil.copy2(s, stage / dst)

for f in ("README.md", "CHANGELOG.md", "LICENSE", "THIRD_PARTY.md"):
    shutil.copy2(root / f, stage / f)
(stage / "Docs").mkdir()
for f in sorted((root / "Docs").glob("*.md")):
    shutil.copy2(f, stage / "Docs" / f.name)

lic = stage / "ThirdPartyLicenses"
lic.mkdir()
juce = root / "JUCE"
texts = {"JUCE-LICENSE.md": "LICENSE.md",
         "HarfBuzz-COPYING.txt": "modules/juce_graphics/fonts/harfbuzz/COPYING",
         "SheenBidi-LICENSE.txt": "modules/juce_graphics/unicode/sheenbidi/LICENSE",
         "libpng-LICENSE.txt": "modules/juce_graphics/image_formats/pnglib/LICENSE",
         "IJG-README.txt": "modules/juce_graphics/image_formats/jpglib/README",
         "zlib-README.txt": "modules/juce_core/zip/zlib/README",
         "VST3-SDK-LICENSE.txt": "modules/audio_processors/format_types/VST3_SDK/LICENSE.txt".replace("audio_processors", "juce_audio_processors")}
for out, rel in texts.items():
    p = juce / rel
    if not p.exists():
        sys.exit(f"missing licence text: {p}")
    shutil.copy2(p, lic / out)

signed = os.environ.get("IFX_SIGNED_BY", "")
(stage / "SIGNATURE.txt").write_text(
    (f"Signed by: {signed}\n" if signed else
     ("UNSIGNED (ad-hoc signature only; not notarised). On first launch use right-click > Open, or run\n"
      "  xattr -dr com.apple.quarantine <bundle>\n" if plat == "macos" else "UNSIGNED. Linux builds are not signed.\n")))

dist = root / "dist"
dist.mkdir(exist_ok=True)
if plat == "linux":
    out = dist / f"{name}.tar.gz"
    with tarfile.open(out, "w:gz") as t:
        t.add(stage, arcname=name)
else:
    out = dist / f"{name}.zip"
    out.unlink(missing_ok=True)
    subprocess.run(["ditto", "-c", "-k", "--keepParent", str(stage), str(out)], check=True)   # keeps symlinks + modes
digest = hashlib.sha256(out.read_bytes()).hexdigest()
(dist / f"{out.name}.sha256").write_text(f"{digest}  {out.name}\n")
shutil.rmtree(stage_root, ignore_errors=True)
print(out, digest)
