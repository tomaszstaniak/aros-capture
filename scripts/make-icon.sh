#!/bin/sh
# Builds the Workbench icons with the AROS icon tool ilbmtoicon (AROS PNG
# icons with a normal and a selected image):
#   AROSCapture.info (tool) and drawer.info from assets/icon/AROSCapture.svg,
#   ReadMe.info (project, MultiView) from assets/icon/ReadMe.svg,
#   AddToDockbar.info (tool) from assets/icon/AddToDockbar.svg.
# The images are rendered by the svgicon renderer, the one that draws the
# toolbar icons at run time; the selected image has a blue glyph.
#   SVGICON_DIR  an aros-svg-icons checkout with its host build (build/host)
#   ILBMTOICON   ilbmtoicon from an AROS build tree (bin/<host>/tools)
#   ICON_SIZE    optional, default 64
# Needs a C++ compiler and netpbm's pamtopng. Output: build/icon/
set -eu
: "${SVGICON_DIR:?Set SVGICON_DIR to an aros-svg-icons checkout with build/host}"
: "${ILBMTOICON:?Set ILBMTOICON to the AROS ilbmtoicon tool}"
size=${ICON_SIZE:-64}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
src="$repo/assets/icon"
out="$repo/build/icon"
mkdir -p "$out"
objs=$(ls "$SVGICON_DIR"/build/host/svgicon.o "$SVGICON_DIR"/build/host/lu-*.o "$SVGICON_DIR"/build/host/pg-*.o)
# shellcheck disable=SC2086
c++ -o "$out/svgraster" -x c "$repo/scripts/host/svgraster.c" -x none -I"$SVGICON_DIR/src/svgicon" $objs
for v in AROSCapture ReadMe AddToDockbar; do
  sed 's/stroke="#2b2d31"/stroke="#1f5fbf"/' "$src/$v.svg" > "$out/$v-selected.svg"
  "$out/svgraster" "$src/$v.svg" "$size" "$out/$v.pam"
  "$out/svgraster" "$out/$v-selected.svg" "$size" "$out/$v-selected.pam"
  pamtopng "$out/$v.pam" > "$out/$v.png"
  pamtopng "$out/$v-selected.pam" > "$out/$v-selected.png"
done
"$ILBMTOICON" "$src/AROSCapture.info.src" "$out/AROSCapture.png" "$out/AROSCapture-selected.png" "$out/AROSCapture.info"
"$ILBMTOICON" "$src/drawer.info.src" "$out/AROSCapture.png" "$out/AROSCapture-selected.png" "$out/drawer.info"
"$ILBMTOICON" "$src/ReadMe.info.src" "$out/ReadMe.png" "$out/ReadMe-selected.png" "$out/ReadMe.info"
"$ILBMTOICON" "$src/AddToDockbar.info.src" "$out/AddToDockbar.png" "$out/AddToDockbar-selected.png" "$out/AddToDockbar.info"
(cd "$out" && shasum -a 256 AROSCapture.info drawer.info ReadMe.info AddToDockbar.info)
