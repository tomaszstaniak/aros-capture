#!/bin/sh
# Packs a built drawer (scripts/build.sh with SVGICON_LIBRARY) and the
# Workbench icons (scripts/make-icon.sh) into dist/.
#   CAPTURE_TARGET  x86_64-abiv11 or aarch64-raspi-v1 (the user targets)
#   SVGICON_DIR     the aros-svg-icons checkout the library came from
#                   (licences, THIRD-PARTY.md, the library's BUILDINFO)
#   SVGICON_BUILD   the aros-svg-icons build directory of that library
#   VERSION         e.g. 0.1.0-rc1
# Output: dist/AROSCapture-<VERSION>-aros-<arch>-<abi>-<board>.zip and
# its line in dist/SHA256SUMS. An existing archive is never replaced.
set -eu
: "${CAPTURE_TARGET:?}" "${SVGICON_DIR:?}" "${SVGICON_BUILD:?}" "${VERSION:?}"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$CAPTURE_TARGET" in
  x86_64-abiv11)
    suffix=aros-x86_64-abiv11-pc
    target_line="For AROS x86_64 ABIv11 (AROS One and other ABIv11 systems)."
    runtime="the GCC 10.5.0 runtime (libstdc++, libgcc; GPL-3.0 with the GCC Runtime Library Exception)"
    lic="AROS-APL-1.1.txt GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt GPL-3.0.txt LunaSVG-PlutoVG-MIT.txt"
    limits="A capture is the picture of one screen: the front screen, or the one
  named with PUBSCREEN. When several screens are shown at once (a screen
  dragged down), only that screen is captured."
    ;;
  aarch64-raspi-v1)
    suffix=aros-aarch64-abiv1-raspi
    target_line="For AROS on the Raspberry Pi (aarch64, ABIv1)."
    runtime="the LLVM 20.1.0 runtime (libc++, libc++abi, libunwind; Apache-2.0 with LLVM exception)"
    lic="AROS-APL-1.1.txt LLVM-Apache-2.0-with-LLVM-exception.txt LunaSVG-PlutoVG-MIT.txt"
    limits="A capture is the picture of one screen: the front screen, or the one
  named with PUBSCREEN. See-through (translucent) windows are not
  included; the program says so when it happens."
    ;;
  *) echo "CAPTURE_TARGET must be x86_64-abiv11 or aarch64-raspi-v1" >&2; exit 1 ;;
esac
build="$repo/build/$CAPTURE_TARGET"
drawer="$build/AROSCapture-drawer"
icons="$repo/build/icon"
for f in "$drawer/AROSCapture" "$drawer/AddToDockbar" "$drawer/Widget/org.aros.capture/applet.json" "$drawer/libs/svgicon.library" "$build/BUILDINFO" "$icons/AROSCapture.info" "$icons/drawer.info" "$icons/ReadMe.info" "$icons/AddToDockbar.info" "$SVGICON_BUILD/BUILDINFO"; do
  [ -e "$f" ] || { echo "missing $f" >&2; exit 1; }
done
lib_sha=$(shasum -a 256 "$drawer/libs/svgicon.library" | cut -d' ' -f1)
grep -q "^sha256=$lib_sha$" "$SVGICON_BUILD/BUILDINFO" || { echo "libs/svgicon.library is not the library of $SVGICON_BUILD" >&2; exit 1; }
lib_rev=$(sed -n 's/^revision=//p' "$SVGICON_BUILD/BUILDINFO")
case "$lib_rev" in *modified*) echo "svgicon.library comes from a modified tree ($lib_rev)" >&2; exit 1 ;; esac
src_rev=$(sed -n 's/^source=//p' "$build/BUILDINFO")
zk_rev=$(sed -n 's/^zunekit=//p' "$build/BUILDINFO")
case "$src_rev$zk_rev" in *dirty*) echo "the build is from uncommitted sources ($src_rev, $zk_rev)" >&2; exit 1 ;; esac
grep -q "^cflags=$" "$build/BUILDINFO" || { echo "the build has test flags" >&2; exit 1; }

name="AROSCapture-$VERSION-$suffix"
dist="$repo/dist"
[ -e "$dist/$name.zip" ] && { echo "$dist/$name.zip exists; never replaced" >&2; exit 1; }
stage=$(mktemp -d)
app="$stage/AROSCapture"
mkdir -p "$app/licenses/svgicon"
cp -R "$drawer/." "$app/"
cp "$icons/AROSCapture.info" "$app/AROSCapture.info"
cp "$icons/AddToDockbar.info" "$app/AddToDockbar.info"
grep -q "\"version\": \"$VERSION\"" "$app/Widget/org.aros.capture/applet.json" || { echo "the widget package was built with another VERSION" >&2; exit 1; }
cp "$icons/drawer.info" "$stage/AROSCapture.info"
cp "$repo/LICENSE" "$app/LICENSE"
cp "$SVGICON_DIR/LICENSE" "$SVGICON_DIR/THIRD-PARTY.md" "$app/licenses/svgicon/"
for l in $lic; do cp "$SVGICON_DIR/licenses/$l" "$app/licenses/svgicon/"; done
fill() { # template output KEY=VALUE...
  python3 - "$@" <<'PY'
import sys
src, dst, pairs = sys.argv[1], sys.argv[2], sys.argv[3:]
text = open(src).read()
for p in pairs:
    k, v = p.split("=", 1)
    text = text.replace("@" + k + "@", v)
if "@" in "".join(l for l in text.splitlines() if l.strip().startswith("@")):
    sys.exit("unfilled placeholder in " + src)
open(dst, "w").write(text)
PY
}
fill "$repo/packaging/ReadMe.txt" "$app/ReadMe" "VERSION=$VERSION" "TARGET_LINE=$target_line" "LIMITS=$limits"
cp "$icons/ReadMe.info" "$app/ReadMe.info"
cp "$icons/ReadMe.info" "$app/LICENSE.info"
cp "$icons/ReadMe.info" "$app/NOTICES.info"
fill "$repo/packaging/NOTICES.txt" "$app/NOTICES" "SOURCE=$src_rev" "ZUNEKIT=$zk_rev" "SVGICON_REV=$lib_rev" "SVGICON_SHA=$lib_sha" "RUNTIME=$runtime"
grep -n "@[A-Z_]*@" "$app/ReadMe" "$app/NOTICES" && { echo "unfilled placeholder" >&2; exit 1; }
{ cat "$build/BUILDINFO"; echo "package=$name"; } > "$app/BuildInfo"
# No host paths may leave the build machine.
if grep -r -l -I -e "/Users/" -e "/Volumes/" -e "/private/" "$stage" >/dev/null; then
  echo "host path found in the package:" >&2; grep -r -l -I -e "/Users/" -e "/Volumes/" -e "/private/" "$stage" >&2; exit 1
fi
for f in "$app/AROSCapture" "$app/AddToDockbar" "$app/Widget/org.aros.capture/bin/$CAPTURE_TARGET/CaptureWidget" "$app/libs/svgicon.library"; do
  if strings "$f" | grep -q -e "/Users/" -e "/Volumes/"; then echo "host path in $f" >&2; exit 1; fi
done
mkdir -p "$dist"
(cd "$stage" && find . -exec touch -t 202611060000 {} + && zip -q -X -r "$dist/$name.zip" AROSCapture AROSCapture.info)
rm -rf "$stage"
(cd "$dist" && shasum -a 256 "$name.zip" >> SHA256SUMS && tail -n 1 SHA256SUMS)
