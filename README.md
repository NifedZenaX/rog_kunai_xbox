# ROG Kunai Gamepad 3 → Xbox 360 Controller Remapper

Reads input from your ROG Kunai Gamepad 3 via the Windows HID API and feeds it
to a virtual Xbox 360 controller via ViGEmBus. Games see a standard Xbox pad.

## Prerequisites

### 1. Install ViGEmBus Driver (v1.22.0)

Download and install `ViGEmBus_Setup_x64.msi` from:
https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0

Reboot after installation. This is the only external dependency — an open-source
kernel driver that lets user-mode programs create virtual game controllers.

> **Note:** The project was archived in Nov 2023 but the driver still works on
> Windows 10/11. There is no maintained alternative.

### 2. Build ViGEmClient Library

The ViGEmClient SDK does **not** ship pre-built binaries — you must build it
from source.

1. Clone the repository:
   ```
   git clone https://github.com/nefarius/ViGEmClient.git
   ```

2. Open `ViGEmClient.sln` in Visual Studio 2022

3. Set the configuration to **Release_LIB | x64** (static library, no DLL needed)

4. Build the solution (Ctrl+Shift+B)

5. Copy the output into this project's `deps/` folder:
   ```
   rog_kunai_xbox/
     deps/
       ViGEmClient/
         include/          <-- copy from ViGEmClient/include/
           ViGEm/
             Client.h
             Common.h
             Util.h
         lib/
           x64/
             ViGEmClient.lib   <-- copy from ViGEmClient build output
   ```

   The build output location depends on your VS setup but is typically:
   `ViGEmClient/bin/Release_LIB/x64/ViGEmClient.lib`
   or check your Output window for the exact path.

### 3. Build This Project

1. Open `rog_kunai_xbox.sln` in Visual Studio 2022
2. Select **x64** platform and **Debug** or **Release**
3. Build → Build Solution (Ctrl+Shift+B)

The output `.exe` will be in `bin/Debug/` or `bin/Release/`. Since we link
statically (Release_LIB), no DLL is needed at runtime.

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
`src/ButtonMapping.h` to match your device's actual report format.

Press **Ctrl+C** to stop the program and remove the virtual controller.

## Troubleshooting

- **"Failed to open HID device"** — Run as Administrator. Some HID devices
  require elevated access.
- **"Failed to connect to ViGEmBus"** — Make sure the ViGEmBus driver is
  installed and you've rebooted after installation.
- **No ASUS device found** — Use `--list` to find your device index, then
  `--device <N>` to select it manually.
- **Buttons are wrong** — Use `--dump` to see the raw report and adjust
  `KunaiReportLayout` byte offsets and bit masks in `src/ButtonMapping.h`.
- **Linker errors about ViGEmClient** — Make sure you built with `Release_LIB`
  (not `Release_DLL`) and placed `ViGEmClient.lib` at `deps/ViGEmClient/lib/x64/`.
