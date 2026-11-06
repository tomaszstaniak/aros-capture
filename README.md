# AROS Capture

A screenshot tool for AROS. Drag a rectangle around part of the screen, mark
it up in the editor, then save it as PNG, copy it to the clipboard or drag it
into a Wanderer drawer.

## Get AROS Capture

[Download AROS Capture 0.1.0](https://github.com/tomaszstaniak/aros-capture/releases/tag/v0.1.0)
for AROS x86_64 ABIv11 (AROS One and other ABIv11 systems) or AROS on the
Raspberry Pi (aarch64).

Unpack the archive and copy the AROSCapture drawer anywhere, for example to
SYS:Utilities. The toolbar icons and svgicon.library are inside the drawer;
nothing needs to go into SYS:Libs and no Assign is needed.

You can also install it with [arospkg](https://github.com/tomaszstaniak/arospkg):

```sh
apkg update
apkg install aroscapture
```

## What it does

- **Area capture.** Double-click AROS Capture and drag a rectangle; Return
  takes the whole screen, Esc or the right mouse button cancels.
- **Editor.** Rectangle, ellipse, line, arrow and pencil, undo, zoom
  (Ctrl+wheel, + and -, 0 fits, 1 shows 100 %). Tab copies the colour code
  under the pointer as `#RRGGBB`.
- **Save, copy, drag.** Save into the capture folder (SYS:Pictures/Captures
  until you choose another in Settings), copy as a picture to the clipboard,
  or drag the picture into a drawer.
- **Shell.** `AROSCapture SCREEN` saves the whole screen without a window,
  `AROSCapture REGION` asks for an area and saves it. `DELAY`, `TO`, `DIR`,
  `FORCE`, `QUIET`, `NOTIFY` and `PUBSCREEN` cover scripts and dock items.
  Return codes: 0 saved, 5 cancelled, 10 not saved, 20 wrong arguments.
- **Dockbar button (optional).** With [Dockbar](https://github.com/tomaszstaniak/aros-dockbar)
  0.6.0 or newer running, double-click Add to Dockbar in the AROS Capture
  drawer. Dockbar installs the button and asks which dock to put it on. A
  click on it offers: capture an area, the screen, the screen in 5 seconds,
  or open the capture folder. AROS Capture works the same without Dockbar.

## Limits

- A capture is the picture of one screen: the front screen, or the one
  named with `PUBSCREEN`.
- On the Raspberry Pi, see-through (translucent) windows are not part of the
  picture; AROS Capture says how many were left out.
- The mouse pointer is not captured. Window capture, formats other than PNG
  and video recording are not included.
- The Raspberry Pi package was tested in QEMU, not on a physical Raspberry Pi.

## Development

[Development guide](docs/development.md): building, dependencies, packaging,
host tests and the Dockbar widget protocol.

## Licence

AROS Capture is under the [MIT License](LICENSE). The toolbar and button
icons are [Tabler Icons](https://github.com/tabler/tabler-icons) (MIT),
shipped unmodified in `assets/tabler/` with their licence. The packages also
contain svgicon.library and AROS link libraries under their own licences,
listed in each package's NOTICES.
