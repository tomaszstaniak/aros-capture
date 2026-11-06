# AROS Capture

Screenshots and annotations for AROS. Select an area or capture a screen, add arrows, shapes or freehand marks, then save as PNG, copy to the clipboard or drag the image into a Wanderer drawer.

[Download 0.1.0](https://github.com/tomaszstaniak/aros-capture/releases/tag/v0.1.0) for **AROS One / x86_64 ABIv11** or **Raspberry Pi / aarch64 ABIv1**.

## Install

Unpack the archive and copy the `AROSCapture` drawer wherever you keep your applications. Keep its contents together.

Or install with [arospkg](https://github.com/tomaszstaniak/arospkg):

```sh
apkg update
apkg install aroscapture
```

## Use

Open AROS Capture and select an area. The editor lets you annotate, save or copy the image. Screenshots go to `SYS:Pictures/Captures`; choose another folder in Settings.

From the Shell:

```text
AROSCapture SCREEN
AROSCapture REGION
AROSCapture SCREEN DELAY=5
```

With [Dockbar](https://github.com/tomaszstaniak/aros-dockbar) 0.6.0 or newer, open `Add to Dockbar` to add a widget with capture actions and a shortcut to your screenshots. Dockbar is optional. Other docks can launch AROS Capture as a normal application.

Captures contain one AROS screen and omit the mouse pointer. On Raspberry Pi, translucent windows are omitted from the image.

See the included ReadMe for keyboard shortcuts and Shell options. Build instructions are in the [development guide](docs/development.md).

## Licence

[MIT](LICENSE). [Tabler Icons](https://github.com/tabler/tabler-icons) and bundled libraries retain their own licences; see `NOTICES` in the download.
