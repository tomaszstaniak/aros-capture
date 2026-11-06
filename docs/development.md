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
CAPTURE_TARGET=x86_64-v1 \
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

## Use

Without arguments (Workbench, Shell or a Dockbar program item) the program asks for an area and opens the editor. Drag to choose; Return takes the whole screen; Esc or the right mouse button cancel.

The editor's toolbar: new capture (N), copy to the clipboard (C), save to the capture folder (S), drag out (V: drag the picture into a Wanderer drawer), rectangle (R), ellipse (O), line (L), arrow (A), pencil (P), undo (Z) and settings. Tab copies the colour under the pointer as `#RRGGBB`. Keys 0 and 1 show the picture fitted or at 100 %.

The capture folder defaults to `SYS:Pictures/Captures` and is created on the first save. Change it in Settings; it is kept in `ENVARC:AROSCapture/Folder` and used by the editor and by the Shell commands below. If it cannot be stored (for example a write-protected `ENVARC:`), Settings say so and the previous folder stays in use.

Shell interface, for scripts and Dockbar command items:

```
AROSCapture SCREEN|REGION [DELAY=<seconds>] [TO=<file> [FORCE]|DIR=<folder>] [PUBSCREEN=<name>] [QUIET] [NOTIFY]
```

`SCREEN` saves the whole public screen, `REGION` asks for an area first. Without `TO` or `DIR` the file goes to the capture folder. `TO` names a file; an existing one is replaced only with `FORCE`, and the old file is kept until the new one is complete. `QUIET` prints nothing on success. `NOTIFY` shows a short notice at the top of the screen after saving or failing (for Dockbar command items, which have no Shell window); it does not change the output or the return code. Return codes: 0 saved, 5 cancelled, 10 not saved (the reason is printed), 20 wrong arguments or the editor could not start. Ctrl-C cancels a delay.

Dockbar: drop the AROSCapture icon on the dock for a program item (area, then editor); in its Edit shortcut window the icon can be set to `Icons/screenshot.svg` in the AROS Capture drawer. For one-click captures use Add command with, for example, `AROSCapture SCREEN NOTIFY` or `AROSCapture REGION DELAY=3 NOTIFY` and "Open a Shell window" off; the file goes to the capture folder.

`AROSCapture PROBE=<folder> [PUBSCREEN=<name>]` writes diagnostic data about composited windows (screen bitmap, each composited layer's surface, a report).

## Limits

- A capture is the picture of one screen. When several screens are shown at once (a screen dragged down), only that screen is captured.
- On mainline AROS (including the Raspberry Pi), see-through windows, such as Dockbar's, are not part of the screen picture. AROS Capture says how many were left out. While choosing an area they disappear for a moment.
- The mouse pointer is not captured.
- Window capture, image formats other than PNG, and video are not implemented.
