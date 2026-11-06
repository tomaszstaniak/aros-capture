# Using AROS Capture

## Capture and save

Open AROS Capture and drag a rectangle around the area you want. Press Return to select the whole screen, or Esc to cancel. The selected image opens in the editor.

Use the toolbar to add shapes, arrows or freehand marks. Save the image as PNG, copy it to the clipboard, or select the drag tool and drag it into a Wanderer drawer.

Images are saved to `SYS:Pictures/Captures`. Choose another folder in Settings; the Shell commands use the same folder.

## Keyboard and mouse

| Key | Action |
|---|---|
| N | New capture |
| C | Copy image |
| S | Save PNG |
| V | Select and drag image out |
| R / O / L / A / P | Rectangle / ellipse / line / arrow / pencil |
| Z | Undo |
| Comma | Settings |
| + / − or Ctrl+wheel | Zoom |
| 0 / 1 | Fit image / 100% zoom |
| Tab | Copy the colour under the pointer as `#RRGGBB` |

Pan with the middle mouse button, or hold Space while dragging.

## Shell

```text
AROSCapture                         Select an area and open the editor
AROSCapture SCREEN                  Save the screen
AROSCapture REGION                  Select an area and save it
AROSCapture SCREEN DELAY=5           Save the screen after five seconds
AROSCapture SCREEN TO=RAM:shot.png   Save to a specific file
```

| Option | Meaning |
|---|---|
| `DELAY=<seconds>` | Wait 0–3600 seconds before capturing |
| `TO=<file>` | Save to this file |
| `DIR=<drawer>` | Save with a generated name in this folder |
| `FORCE` | Allow replacement of the file named by `TO` |
| `PUBSCREEN=<name>` | Capture the named public screen |
| `NOTIFY` | Show a brief success or error message on screen |
| `QUIET` | Suppress the Shell success message |

`TO`, `DIR`, `FORCE` and `NOTIFY` require `SCREEN` or `REGION`. Use `TO` or `DIR`, not both. `FORCE` requires `TO`. Press Ctrl-C to cancel a delay.

Return codes: **0** saved, **5** cancelled, **10** failed, **20** invalid arguments or editor startup failure.

## Dockbar

With [Dockbar](https://github.com/tomaszstaniak/aros-dockbar) 0.6.0 or newer, open **Add to Dockbar** in the Capture drawer. Choose a dock when prompted. The widget offers area capture, screen capture, capture after five seconds and opening the screenshot folder.

If Dockbar is stopped, you can select its program to start it. Run Add to Dockbar again after moving Capture or to update the widget.

From the Shell: `AddToDockbar [DOCKBAR=<program>] [QUIET]`.

Any dock can launch `AROSCapture` as a normal application. For a screen-capture shortcut, use `AROSCapture SCREEN NOTIFY`.

## Captured content

A capture contains one AROS screen: the front screen, or the screen selected with `PUBSCREEN`. If a screen is dragged down to reveal another behind it, only the selected screen is captured.

On mainline AROS, including Raspberry Pi, translucent windows are omitted. The program reports how many were left out. The mouse pointer is not captured.
