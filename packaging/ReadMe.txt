AROS Capture @VERSION@
@TARGET_LINE@

Capture an area or a screen, add annotations, and save as PNG, copy to
the clipboard or drag the image into a Wanderer drawer.

INSTALLATION

Copy the AROSCapture drawer wherever you keep your applications.
Keep Icons, libs and Widget beside the program.

CAPTURE AND SAVE

Open AROS Capture and drag a rectangle around the area you want.
Return selects the whole screen; Esc or the right mouse button cancels.
The selected image opens in the editor.

Use the toolbar to add shapes, arrows or freehand marks. Save as PNG,
copy to the clipboard, or use the drag tool to drag the image into a
Wanderer drawer.

Images go to SYS:Pictures/Captures. Choose another folder in Settings;
the Shell commands use the same folder.

KEYBOARD AND MOUSE

  N       New capture             C       Copy image
  S       Save PNG                V       Select and drag image out
  R       Rectangle               O       Ellipse
  L       Line                    A       Arrow
  P       Pencil                  Z       Undo
  Comma   Settings                Tab     Copy colour as #RRGGBB

Zoom with +, - or Ctrl+wheel. Press 0 to fit the image or 1 for 100%.
Pan with the middle mouse button, or hold Space while dragging.

SHELL

  AROSCapture                         Select an area and open the editor
  AROSCapture SCREEN                  Save the screen
  AROSCapture REGION                  Select an area and save it
  AROSCapture SCREEN DELAY=5           Capture after five seconds
  AROSCapture SCREEN TO=RAM:shot.png   Save to a specific file

  DELAY=<seconds>     Wait 0-3600 seconds before capturing
  TO=<file>          Save to this file
  DIR=<drawer>       Save with a generated name in this folder
  FORCE              Allow replacement of the file named by TO
  PUBSCREEN=<name>   Capture the named public screen
  NOTIFY             Show a brief success or error message on screen
  QUIET              Suppress the Shell success message

TO, DIR, FORCE and NOTIFY require SCREEN or REGION. Use TO or DIR,
not both. FORCE requires TO. Ctrl-C cancels a delay.
Return codes: 0 saved, 5 cancelled, 10 failed, 20 invalid arguments
or editor startup failure.

DOCKBAR

With Dockbar 0.6.0 or newer, open Add to Dockbar and choose a dock.
The widget offers area capture, screen capture, capture after five
seconds and opening the screenshot folder.

If Dockbar is stopped, you can select its program to start it.
Run Add to Dockbar again after moving Capture or to update the widget.
From the Shell: AddToDockbar [DOCKBAR=<program>] [QUIET].

Any dock can launch AROSCapture as a normal application. For a screen
capture shortcut, use AROSCapture SCREEN NOTIFY.

CAPTURED CONTENT

@LIMITS@
The mouse pointer is not captured.

LICENCE

MIT; see LICENSE. Bundled components retain their own licences;
see NOTICES and the licences included in the package.
