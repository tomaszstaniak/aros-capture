#!/bin/sh
# Builds AROSCapture for one target into build/<target>/.
#   CAPTURE_TARGET  x86_64-v1, x86_64-abiv11 or aarch64-raspi-v1
#   AROS_CC         the target's C compiler
#   AROS_SDK        the matching SDK sysroot (the Developer directory)
#   ZUNEKIT_DIR     a zunekit checkout (zk_toolbar, zk_imageview, zk_dragout)
#   SVGICON_LIBRARY optional: the target's svgicon.library, copied into the
#                   drawer's libs/ (the toolbar icons need it at run time)
#   CAPTURE_CFLAGS  optional extra flags, e.g. the test-only failure switches
#                   -DCAPTURE_TEST_FAIL_WRITE, -DCAPTURE_TEST_FAIL_PUBLISH and
#                   -DCAPTURE_TEST_FAIL_RESTORE (never for a delivered build)
set -eu
: "${ZUNEKIT_DIR:?Set ZUNEKIT_DIR to a zunekit checkout}"
: "${CAPTURE_TARGET:?Set CAPTURE_TARGET to x86_64-v1, x86_64-abiv11 or aarch64-raspi-v1}"
: "${AROS_CC:?Set AROS_CC to the target compiler}"
: "${AROS_SDK:?Set AROS_SDK to the matching SDK sysroot}"
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$repo" in *" "*) echo "The repository path must not contain spaces" >&2; exit 1 ;; esac
case "$CAPTURE_TARGET" in
  x86_64-v1|aarch64-raspi-v1) want_abi=v1 ;;
  x86_64-abiv11) want_abi=v11 ;;
  *) echo "Unknown CAPTURE_TARGET $CAPTURE_TARGET" >&2; exit 1 ;;
esac
# The x86_64 ABIs share compiler names and macros but not binaries: check the
# pair. Mainline's driver knows -noposixc and its SDK has libposixc.
if [ "$CAPTURE_TARGET" != aarch64-raspi-v1 ]; then
  if echo 'int p;' | "$AROS_CC" --sysroot="$AROS_SDK" -noposixc -x c -c -o /dev/null - 2>/dev/null; then cc_abi=v1; else cc_abi=v11; fi
  if [ -e "$AROS_SDK/lib/libposixc.a" ]; then sdk_abi=v1; else sdk_abi=v11; fi
  if [ "$cc_abi" != "$want_abi" ] || [ "$sdk_abi" != "$want_abi" ]; then
    echo "$CAPTURE_TARGET needs ABI$want_abi; AROS_CC is ABI$cc_abi, AROS_SDK is ABI$sdk_abi" >&2; exit 1
  fi
fi
out=${CAPTURE_BUILD_DIR:-"$repo/build/$CAPTURE_TARGET"}
mkdir -p "$out"
pmap="-ffile-prefix-map=$repo=aros-capture -ffile-prefix-map=$AROS_SDK=sdk"
src="$repo/src/main.c $repo/src/capture.c $repo/src/select.c $repo/src/probe.c $repo/src/names.c $repo/src/replace.c $repo/src/notice.c $repo/src/clip.c $repo/src/annot.c $repo/src/editor.c"
zk="$ZUNEKIT_DIR/src/zk_toolbar.c $ZUNEKIT_DIR/src/zk_imageview.c $ZUNEKIT_DIR/src/zk_imageview_core.c $ZUNEKIT_DIR/src/zk_dragout.c $ZUNEKIT_DIR/src/zk_dragout_core.c"
case "$ZUNEKIT_DIR" in *" "*) echo "ZUNEKIT_DIR must not contain spaces" >&2; exit 1 ;; esac
# shellcheck disable=SC2086
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
  -Werror=implicit-function-declaration $pmap -ffile-prefix-map=$ZUNEKIT_DIR=zunekit \
  ${CAPTURE_CFLAGS:-} -I"$repo/src" -I"$ZUNEKIT_DIR/src" $src $zk \
  -o "$out/AROSCapture" -lmuimaster -lcybergraphics -ldatatypes -llayers -lutility -lworkbench -lm
# The Dockbar widget provider and the program that installs the widget.
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
  -Werror=implicit-function-declaration $pmap ${CAPTURE_CFLAGS:-} -I"$repo/src" \
  "$repo/src/widget.c" "$repo/src/widgetreq.c" -o "$out/CaptureWidget"
"$AROS_CC" --sysroot="$AROS_SDK" -std=gnu99 -O2 -Wall -Wextra -Werror \
  -Werror=implicit-function-declaration $pmap ${CAPTURE_CFLAGS:-} -I"$repo/src" \
  "$repo/src/addtodockbar.c" -o "$out/AddToDockbar"
strip=${AROS_CC%gcc}strip; [ -x "$strip" ] || strip=${AROS_CC%clang}strip; [ -x "$strip" ] || strip=${AROS_CC%cc}strip
if [ -x "$strip" ]; then for b in AROSCapture CaptureWidget AddToDockbar; do "$strip" --strip-debug "$out/$b"; done; fi
{
  echo "target=$CAPTURE_TARGET"
  echo "compiler=$("$AROS_CC" --version | head -n 1)"
  echo "source=$(git -C "$repo" rev-parse HEAD 2>/dev/null || echo none)$(git -C "$repo" diff --quiet HEAD 2>/dev/null || echo -dirty)"
  # Only the zunekit files built in: other uncommitted zunekit work does not
  # mark the build dirty, a change to these does.
  zkfiles="src/zk_toolbar.c src/zk_toolbar.h src/zk_imageview.c src/zk_imageview.h src/zk_imageview_core.c src/zk_imageview_core.h src/zk_dragout.c src/zk_dragout.h src/zk_dragout_core.c src/zk_dragout_core.h"
  # shellcheck disable=SC2086
  echo "zunekit=$(git -C "$ZUNEKIT_DIR" rev-parse HEAD 2>/dev/null || echo none)$(git -C "$ZUNEKIT_DIR" diff --quiet HEAD -- $zkfiles 2>/dev/null || echo -dirty)"
  # The SDK by its directory and a digest of its headers and link libraries.
  echo "sdk=$(basename "$(dirname "$AROS_SDK")")/$(basename "$AROS_SDK") $(cd "$AROS_SDK" && find include lib -type f 2>/dev/null | LC_ALL=C sort | xargs shasum -a 256 2>/dev/null | shasum -a 256 | cut -c1-16)"
  echo "cflags=${CAPTURE_CFLAGS:-}"
  echo "sha256=$(shasum -a 256 "$out/AROSCapture" | cut -d' ' -f1)"
  echo "widget=$(shasum -a 256 "$out/CaptureWidget" | cut -d' ' -f1)"
  echo "addtodockbar=$(shasum -a 256 "$out/AddToDockbar" | cut -d' ' -f1)"
} > "$out/BUILDINFO"
cat "$out/BUILDINFO"
# Package drawer: the program and the Tabler icons it draws at run time.
mkdir -p "$out/AROSCapture-drawer/Icons"
cp "$out/AROSCapture" "$out/AROSCapture-drawer/"
for f in "$repo"/assets/tabler/*.svg; do
  # file-text.svg is only the source of the ReadMe icon (assets/icon).
  case "$(basename "$f")" in file-text.svg|layout-bottombar.svg) ;; *) cp "$f" "$out/AROSCapture-drawer/Icons/" ;; esac
done
cp "$repo/assets/tabler/LICENSE" "$out/AROSCapture-drawer/Icons/"
# The widget package Dockbar copies into its widget drawer: this target only.
cp "$out/AddToDockbar" "$out/AROSCapture-drawer/"
w="$out/AROSCapture-drawer/Widget/org.aros.capture"
rm -rf "$out/AROSCapture-drawer/Widget"
mkdir -p "$w/assets" "$w/bin/$CAPTURE_TARGET"
sed -e "s/@TARGET@/$CAPTURE_TARGET/g" -e "s/@VERSION@/${VERSION:-0.1.0}/g" "$repo/widget/org.aros.capture/applet.json.in" > "$w/applet.json"
cp "$repo/widget/org.aros.capture/settings.json" "$repo/widget/org.aros.capture/views.json" "$w/"
cp "$repo/widget/org.aros.capture/assets/conditions.json" "$repo/assets/tabler/screenshot.svg" "$w/assets/"
cp "$repo/LICENSE" "$w/LICENSE"
mkdir -p "$w/licenses" && cp "$repo/assets/tabler/LICENSE" "$w/licenses/Tabler-Icons-LICENSE.txt"
cp "$out/CaptureWidget" "$w/bin/$CAPTURE_TARGET/"
if [ -n "${SVGICON_LIBRARY:-}" ]; then
  mkdir -p "$out/AROSCapture-drawer/libs"
  cp "$SVGICON_LIBRARY" "$out/AROSCapture-drawer/libs/svgicon.library"
  echo "svgicon=$(shasum -a 256 "$SVGICON_LIBRARY" | cut -d' ' -f1)" >> "$out/BUILDINFO"
fi
