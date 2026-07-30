# ROG Kunai Gamepad 3 → Xbox 360 Controller Remapper

Reads input from your ROG Kunai Gamepad 3 via the Windows HID API and feeds it
to a virtual Xbox 360 controller via ViGEmBus. Games see a standard Xbox pad.

## Prerequisites

### 1. Install ViGEmBus Driver
Download and install the latest `.msi` from:
https://github.com/nefarius/ViGEmBus/releases

This is the only external dependency — it's an open-source kernel driver that
lets user-mode programs create virtual game controllers. Windows has no native
equivalent.

### 2. Download ViGEmClient SDK
Download the latest release `.zip` from:
https://github.com/nefarius/ViGEmClient/releases

Extract it so the folder structure looks like:
```
rog_kunai_xbox/
  deps/
    ViGEmClient/
      include/
        ViGEm/
          Client.h
          Common.h
      lib/
        release/
          x64/
            ViGEmClient.lib
  src/
    main.cpp
  rog_kunai_xbox.sln
  rog_kunai_xbox.vcxproj
```

Also copy `ViGEmClient.dll` from the SDK's `bin/release/x64/` folder next to
your built `.exe` (in `bin/Debug/` or `bin/Release/`).

### 3. Build
1. Open `rog_kunai_xbox.sln` in Visual Studio 2022
2. Select **x64** platform and **Debug** or **Release**
3. Build → Build Solution (Ctrl+Shift+B)

## Usage

```
rog_kunai_xbox.exe --list              # List connected HID gamepads
rog_kunai_xbox.exe --dump 0            # Dump raw HID bytes from device 0
rog_kunai_xbox.exe                     # Auto-detect ASUS gamepad and run
rog_kunai_xbox.exe --device 0          # Use device 0 explicitly
```

### Discovering Your Button Mapping

The default mapping matches the typical ASUS HID gamepad report layout. If
buttons are wrong, run `--dump <index>` and press each button one at a time to
see which bytes/bits change. Then edit the `KunaiReportLayout` struct in
`main.cpp` to match your device's actual report format.

Press **Ctrl+C** to stop the program and remove the virtual controller.

## Troubleshooting

- **"Failed to open HID device"** — Run as Administrator. Some HID devices
  require elevated access.
- **"Failed to connect to ViGEmBus"** — Make sure the ViGEmBus driver is
  installed and you've rebooted after installation.
- **No ASUS device found** — Use `--list` to find your device index, then
  `--device <N>` to select it manually.
- **Buttons are wrong** — Use `--dump` to see the raw report and adjust
  `KunaiReportLayout` byte offsets and bit masks.
