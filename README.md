# Generic External Launcher for RetroArch by PickOne (Windows only) - Based on [libretro-dolphin-launcher](https://github.com/RobLoach/libretro-dolphin-launcher) by Rob Loach (MIT). 

A tiny libretro core that doesn't emulate anything itself. When you load a game in RetroArch, it starts an **external emulator** (RPCS3, PCSX2, Dolphin, Cemu, TeknoParrot, so on) with that game. When you close the game/emulator, you are back in RetroArch.

This lets you keep your whole library inside RetroArch (playlists, thumbnails, controller navigation, frontends like Big Picture setups) while the real emulation is done by the standalone emulator.

**One DLL, any emulator.** All the details (which program to run, with which arguments) come from a plain text `.cfg` file placed next to the DLL, with the same nane as the DLL file. To support a new emulator, copy and rename the DLL, write a new `.cfg` in cores folder, then write a new `.info` file in info folder or Retroarch (See "Sample Switch Core/Retroarch")

---

## How it works

1. You pick a game in RetroArch with this core.
2. The core reads `<core name>.cfg`, builds the command line (`exe` + `args`), and starts the emulator.
3. While the emulator is running, RetroArch's window goes away and stays out of the way (if `wait=1`, the core waits for the emulator to finish).
4. When you close the emulator, the core shows one blank frame and tells RetroArch it is done, then RetroArch comes back so you can pick another game.

> RetroArch disappearing while the emulator runs is expected: the core itself has nothing to display. The exact behavior can vary slightly depending on how RetroArch is launched (menu, command line, or a frontend).

---

## Installation

You need **three files**, all with the **same base name**, which must end in `_libretro`:

| File | Goes in | Purpose |
|------|---------|---------|
| `rpcs3_launcher_libretro.dll` | `RetroArch\cores\` | The core itself |
| `rpcs3_launcher_libretro.cfg` | `RetroArch\cores\` (**same folder as the DLL**) | Tells the core which emulator to run |
| `rpcs3_launcher_libretro.info` | `RetroArch\info\` | Lets RetroArch show the core's name and supported file types |

```
RetroArch\
├── cores\
│   ├── rpcs3_launcher_libretro.dll
│   └── rpcs3_launcher_libretro.cfg
└── info\
    └── rpcs3_launcher_libretro.info
```

Notes:

- The name in front of `_libretro` can be anything (`rpcs3_launcher_libretro`, `ps4_libretro`, `my_emu_libretro`, ...). What matters is that the `.dll`, `.cfg` and `.info` share exactly the same base name.
- If your RetroArch uses custom folders, check **Settings → Directory → Cores** and **Core Info** for the real paths.
- Use a **64-bit** DLL with 64-bit RetroArch (the normal case on modern Windows).
- Restart RetroArch after copying the files.

---

## The `.cfg` file

A simple `key=value` text file. Lines starting with `#` are comments.

```ini
# rpcs3_launcher_libretro.cfg
name=RPCS3
extensions=bin|ps3
exe=C:\Emulators\rpcs3\rpcs3.exe
args="{target}"
workdir=C:\Emulators\rpcs3
wait=1
```

| Key | Required | Description |
|-----|----------|-------------|
| `name` | no | Name shown in RetroArch. Default: the DLL name without `_libretro`. |
| `extensions` | no | File extensions the core accepts, separated by `\|` (e.g. `iso\|bin\|gcm`). |
| `exe` | **yes** | Full path to the emulator executable. Quotes are optional. |
| `args` | no | Command-line arguments. Supports the placeholders below. |
| `workdir` | no | Working directory. Default: the folder containing `exe`. |
| `wait` | no | `1` (default): wait until the emulator closes. `0`: don't wait. |

### Placeholders for `args`

Given the game `D:\Games\PS2\Gran Turismo 4.iso`:

| Placeholder | Value |
|-------------|-------|
| `{rom}` | `D:\Games\PS2\Gran Turismo 4.iso` |
| `{rom_dir}` | `D:\Games\PS2` |
| `{rom_file}` | `Gran Turismo 4.iso` |
| `{rom_name}` | `Gran Turismo 4` |
| `{target}` | See "Stub files" below |

> **Always wrap paths in quotes** inside `args` (e.g. `"{rom}"`), because game paths often contain spaces. The core does not add quotes for you.

### Stub files with `{target}`

Some games aren't a single file (for example an installed PS3 game, or a title launched by ID). For those, create a tiny text file (a *stub*) with an extension you choose, e.g. `God of War III.ps3`, containing **one line**: the real path or ID to launch:

```
C:\Emulators\rpcs3\dev_hdd0\game\BCES00510\USRDIR\EBOOT.BIN
```

With `args="{target}"`, the core reads the first non-empty line of the stub and passes it to the emulator. Then you can scan the stubs into a RetroArch playlist like normal ROMs.

If the content file is large or binary (for example a real `EBOOT.BIN` or an `.iso`), `{target}` simply falls back to the file's own path, so you can use `{target}` for both cases.

---

## The `.info` file

This is the standard RetroArch core info file. Example:

```ini
# rpcs3_launcher_libretro.info
display_name = "Sony - PlayStation 3 (RPCS3 Launcher)"
authors = "Rob Loach, your name"
supported_extensions = "bin|ps3"
corename = "RPCS3 Launcher"
manufacturer = "Sony"
categories = "Emulator"
systemname = "PlayStation 3"
systemid = "playstation3"
database = "Sony - PlayStation 3"
license = "MIT"
permissions = ""
display_version = "2.0"
supports_no_game = "false"
```

Keep `supported_extensions` in sync with `extensions=` from the `.cfg`.

---

## Examples

Always check your emulator's documentation, because command-line options can change between versions.

### RPCS3 (PlayStation 3)
```ini
name=RPCS3
extensions=bin|ps3
exe=C:\Emulators\rpcs3\rpcs3.exe
args="{target}"
```

### PCSX2 (PlayStation 2)
```ini
name=PCSX2
extensions=iso|chd|bin|cue
exe=C:\Emulators\PCSX2\pcsx2-qt.exe
args=-batch -fullscreen -- "{rom}"
```

### Dolphin (GameCube / Wii)
```ini
name=Dolphin
extensions=iso|gcm|rvz|wbfs|gcz
exe=C:\Emulators\Dolphin\Dolphin.exe
args=-b -e "{rom}"
```

### Cemu (Wii U)
```ini
name=Cemu
extensions=wua|wud|wux|rpx
exe=C:\Emulators\Cemu\Cemu.exe
args=-g "{rom}" -f
```

---

## Usage

**From the menu**
1. **Load Core** → choose your launcher core.
2. **Load Content** → choose a game.
3. The emulator starts and RetroArch's window goes away. When you close the emulator, you are back in RetroArch.

**With playlists**
1. **Import Content → Scan Directory** (or "Manual Scan"), set the *Default Core* to your launcher core, and set the file extensions to match your `.cfg`.
2. Launch games from the playlist as usual.

**From the command line**
```
retroarch.exe -L cores\rpcs3_launcher_libretro.dll "D:\Games\PS3\MyGame.ps3"
```

**Using the same core for multiple emulators**

Copy the DLL, rename it (keeping `_libretro` at the end), and make a matching `.cfg` and `.info`:

```
cores\pcsx2_launcher_libretro.dll
cores\pcsx2_launcher_libretro.cfg
info\pcsx2_launcher_libretro.info
```

---

## Building from source using Visual Studio 2022 with the **Desktop development with C++** workload.

**From the IDE**
1. Create a **Dynamic-Link Library (DLL)** project (or an Empty Project and set *Configuration Type* to *Dynamic Library*).
2. Add `libretro-generic-launcher.c` and `libretro.h`.
3. Project Properties (all configurations, platform **x64**):
   - *C/C++ → Precompiled Headers*: **Not Using Precompiled Headers**
   - *C/C++ → Advanced → Compile As*: **Compile as C Code (/TC)**
   - *C/C++ → Preprocessor*: add `_CRT_SECURE_NO_WARNINGS`
   - *C/C++ → Code Generation → Runtime Library* (Release): **Multi-threaded (/MT)**
4. Set the project name (or *Target Name*) to something ending in `_libretro`, build in **Release | x64**.

**From the command line** (x64 Native Tools Command Prompt for VS 2022)
```
cl /nologo /O2 /MT /LD /TC /D_CRT_SECURE_NO_WARNINGS libretro-generic-launcher.c /Fe:rpcs3_launcher_libretro.dll
```

`/MT` links the C runtime statically, so the DLL has no dependency on the Visual C++ Redistributable.

---

## Troubleshooting

- **The core doesn't show up in RetroArch**: the DLL name must end in `_libretro.dll`, and the architecture (x64/x86) must match RetroArch.
- **The emulator doesn't start (RetroArch just returns right away)**: make sure `exe=` is set and the `.cfg` has exactly the same base name as the DLL, in the same folder.
- **Game not accepted or not listed**: check that its extension appears in `extensions=` (and in the `.info`).
- **Need more details**: enable logging in RetroArch (**Settings → Logging**) and look for lines starting with `[Launcher]`. They show the exact command line that was run, any "Could not start" errors, and the emulator's exit code.

---

## License

MIT. Based on [libretro-dolphin-launcher](https://github.com/RobLoach/libretro-dolphin-launcher) by Rob Loach. `libretro.h` is part of the [libretro API](https://github.com/libretro/libretro-common) and is included unmodified. See `LICENSE` for details.
