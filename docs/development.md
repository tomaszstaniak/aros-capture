# Development

## Dependencies

- **zunekit**, https://github.com/tomaszstaniak/zunekit (MIT): the Zune
  classes `zk_toolbar`, `zk_imageview` and `zk_dragout`, compiled into the
  program from source. Point `ZUNEKIT_DIR` at a checkout; AROS Capture
  0.1.0 is built from revision `4b7f2a08395801b7ceed7303495131fbc6a4a7f3`.
- **svgicon.library** from aros-svg-icons,
  https://github.com/tomaszstaniak/aros-svg-icons (MIT, with LunaSVG and
  PlutoVG): draws the toolbar's SVG icons at run time. The 0.1.0 packages
  ship the library built from revision
  `86d408b4dc2d720a1964f4646bb0b2da4f24f053` (an ancestor of that
  repository's `main`); build it as its development guide describes and
  pass it with `SVGICON_LIBRARY`.
- **Dockbar header**: `src/dockbar_widgets.h` is a copy of
  `include/dockbar/widgets.h` from https://github.com/tomaszstaniak/aros-dockbar
  (MIT), tag `v0.6.0`.
- An AROS SDK and compiler for the target: GCC 10.5 with the ABIv11 2026.09
  SDK for `x86_64-abiv11`; the AROS aarch64 Clang 20.1 toolchain with the
  Raspberry Pi SDK for `aarch64-raspi-v1`.

## Building

`scripts/build.sh` builds one target into `build/<target>/`:

```sh
CAPTURE_TARGET=x86_64-abiv11 \
AROS_CC=/path/to/x86_64-aros-gcc \
AROS_SDK=/path/to/matching/Developer \
ZUNEKIT_DIR=/path/to/zunekit \
sh scripts/build.sh
```

Targets: `x86_64-v1` (mainline, builds but has no release package), `x86_64-abiv11` (ABIv11) and `aarch64-raspi-v1` (Raspberry Pi). The script refuses an x86_64 compiler and SDK of different ABIs. Pass the SDK with `AROS_SDK`: it is given to the compiler as `--sysroot`. The editor uses zunekit's `zk_toolbar`, `zk_imageview` and `zk_dragout`; `BUILDINFO` records the zunekit commit.

The result is `build/<target>/AROSCapture-drawer/`: the program, `AddToDockbar`, `Widget/org.aros.capture/` (the Dockbar widget package for this target, with its provider `CaptureWidget`; `VERSION` sets its version) and `Icons/` with the Tabler SVG files it draws at run time. The icons need `svgicon.library`, found in `LIBS:` or in the drawer's own `libs/`; set `SVGICON_LIBRARY` to the target's library to copy it there. Without it the toolbar shows text labels.

Host tests for the parts that do not need AROS:

```sh
cc -I src tests/test_names.c src/names.c -o /tmp/test_names && /tmp/test_names
cc -I src tests/test_replace.c src/replace.c -o /tmp/test_replace && /tmp/test_replace
cc -I src tests/test_annot.c src/annot.c -lm -o /tmp/test_annot && /tmp/test_annot
cc -I src tests/test_widgetreq.c src/widgetreq.c -o /tmp/test_widgetreq && /tmp/test_widgetreq
```

`test_widgetreq` can also read the provider's answer with Dockbar's own parser: add `-DWITH_DOCKBAR -I<dockbar>/src <dockbar>/src/applets/json.c <dockbar>/src/applets/snapshot.c -lm`.

## Dockbar widget

`AddToDockbar` asks a running Dockbar (0.6.0 or newer, port `Dockbar.Widgets`, header `src/dockbar_widgets.h` copied from Dockbar's `include/dockbar/widgets.h`) to install `PROGDIR:Widget/org.aros.capture` and offer it for a dock. Only when Dockbar answers that it installed or updated the widget does it store the full path of `AROSCapture` in `ENV:`/`ENVARC:AROSCapture/Program`; a skipped, refused or busy install leaves the stored path as it was. It sends nothing to older Dockbar versions (port `Dockbar` only). The widget is a Dockbar API 2 action widget (`refresh: none`): Dockbar starts `CaptureWidget` only when the user chooses an action; the provider starts `AROSCapture` (area: no arguments; screen: `SCREEN NOTIFY`; delay: `SCREEN DELAY=5 NOTIFY`) as a separate program and answers at once, or opens the capture folder in Wanderer. The child process must not inherit the provider's `PROGDIR:` lock, because the widget directory must remain replaceable after the action.

## Packages

`scripts/make-icon.sh` builds the Workbench icons (`AROSCapture.info`, `AddToDockbar.info`, the drawer icon, `ReadMe.info`) from `assets/icon/*.svg` with the AROS icon tool `ilbmtoicon`; the images are rendered on the host by the svgicon renderer (`SVGICON_DIR` with its host build, `ILBMTOICON` from an AROS build tree). `scripts/package.sh` then packs a drawer built with `SVGICON_LIBRARY` into `dist/AROSCapture-<version>-aros-<arch>-<abi>-<board>.zip` with ReadMe, NOTICES, the licences and BuildInfo, for `CAPTURE_TARGET=x86_64-abiv11` or `aarch64-raspi-v1`. It refuses uncommitted sources, test flags, an svgicon.library that does not match its BUILDINFO or comes from a modified tree, host paths, and an existing archive.

## Capture diagnostics

`AROSCapture PROBE=<folder> [PUBSCREEN=<name>]` writes the screen bitmap, composited layer surfaces and a report to the chosen folder.

User-facing commands and shortcuts are documented in the [user guide](usage.md).
