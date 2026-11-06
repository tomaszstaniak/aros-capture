AROS Capture @VERSION@
@TARGET_LINE@

A screenshot tool: choose an area of the screen, mark it up and save it,
copy it or drag it into a drawer.

Installation
  Copy the AROSCapture drawer anywhere, for example to SYS:Utilities.
  Everything it needs is inside the drawer: the toolbar icons (Icons) and
  svgicon.library (libs), which it finds there by itself. No Assign and
  nothing in SYS:Libs is needed.

Use
  Double-click AROS Capture: drag a rectangle around the area you want
  (Return takes the whole screen, Esc or the right mouse button cancels).
  The editor opens with the picture.

  Editor: N new capture, C copy (as a picture in the clipboard), S save to
  the capture folder, V drag the picture out to a drawer, R rectangle,
  O ellipse, L line, A arrow, P pencil, Z undo, comma Settings.
  Ctrl+wheel or + and - zoom, 0 fits, 1 shows 100 %. Space+drag or the
  middle mouse button moves the picture. Tab copies the colour code under
  the pointer.

  The capture folder is SYS:Pictures/Captures until you choose another in
  Settings (also used by the Shell commands); it is created on the first
  save.

Shell
  AROSCapture SCREEN            save the whole screen without a window
  AROSCapture REGION            choose an area, then save it
  AROSCapture                   choose an area, then edit it

  Options: DELAY=<seconds> (0 to 3600), TO=<file> or DIR=<drawer>,
  FORCE (with TO: replace an existing file; the old one is kept until
  the new one is complete), QUIET (no message on success),
  NOTIFY (a short notice at the top of the screen after saving or
  failing, for Dockbar command items), PUBSCREEN=<name>.
  Return codes: 0 saved, 5 cancelled, 10 not saved, 20 wrong arguments.

Dockbar
  Double-click Add to Dockbar in this drawer to put an AROS Capture
  button on Dockbar (Dockbar 0.6.0 or newer). Dockbar installs it and asks
  whether to add it to the dock. A click on the button offers: capture an
  area, the screen, the screen in 5 seconds, or open the capture folder.
  If Dockbar is not running, you can choose the Dockbar program to start
  it, or skip. Running Add to Dockbar again updates the button and keeps
  its place. If you move the AROS Capture drawer, run it again.
  From the Shell: AddToDockbar [DOCKBAR=<Dockbar program>] [QUIET].

  With any Dockbar version you can also drop the AROS Capture icon on the
  dock for a program item (area choice and editor), or add a command item
  such as "AROSCapture SCREEN NOTIFY".

Limits
  @LIMITS@
  The mouse pointer is not captured.
  Video recording is not included.

Licence
  AROS Capture is under the MIT licence (LICENSE). The other components
  in this package and their licences are listed in NOTICES.
