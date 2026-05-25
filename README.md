# Roblox Handler

A lightweight Windows utility for Roblox: auto-crash on a timer, automatic
crash detection & rejoin, anti-AFK, and multi-instance support — wrapped in a
native Dear ImGui + DirectX 11 interface over a pure-C backend.

> 🤖 **Vibecoded with [Claude](https://claude.com/claude-code).**
> The whole thing — C backend, ImGui/DX11 frontend, the MinGW build, and this
> README — was built collaboratively with Claude.

---

## Features

| Tab | What it does |
| --- | --- |
| **Auto-Crash** | Crashes the Roblox client on a fixed interval (seconds / minutes / hours). |
| **Crash Detect** | Watches the Roblox process, its `*Player*.log`, and error dialogs; auto-relaunches on a real disconnect. Rejoins a specific `placeId` (with live game thumbnail) and optionally a private server via link code, after a configurable cooldown. |
| **Anti-AFK** | Keeps the session alive with periodic input — three modes: F-key, WASD, or mouse nudge. |
| **Multi-Instance** | Closes the `ROBLOX_singletonEvent` / mutex so several Roblox clients can run at once. |

Plus:

- **Favorites** — save up to 64 games (name + placeId + link code) for quick rejoin.
- **Settings persistence** — everything is stored as JSON and can be imported/exported.
  Default location: `%LOCALAPPDATA%\RobloxHandler\settings.json`.

## Screenshots

> _Add screenshots here, e.g. `docs/screenshot.png`._

## Usage

Build `RobloxHandler.exe` (see [Build](#build)) or grab a release, then just run
it — it's a single portable executable, no install. Your settings are saved
automatically to `%LOCALAPPDATA%\RobloxHandler\settings.json` and reloaded on
the next launch.

### Auto-Crash
1. Set **Crash every** and the **Unit** (seconds / minutes / hours).
2. Click **Start auto-crash** — Roblox is force-closed every interval.
3. Click **Stop** to halt. _(The kill is instant — save your progress first.)_

### Crash Detect
1. Set the **Restart delay (seconds)** — how long to wait before relaunching.
2. _(Optional)_ Tick **Open with a specific Place ID** and enter the **Place ID**
   (the number in a game's URL, e.g. `roblox.com/games/`**`1818`**`/...`).
3. _(Optional)_ Tick **Private server (VIP) link code** and paste the **Code** to
   rejoin a private server.
4. Click **Start watching**. The tool tails the Roblox log + process and, on a
   real disconnect/crash, waits the delay and relaunches (rejoining the place /
   private server if set). **Status** shows the restart count and last event.

**Favorites:** type a **Name** and click **Add** to save the current Place ID
(+ link code). Select an entry and **Load** it — or double-click it — to fill the
fields. **Remove** deletes the selected one.

### Anti-AFK
1. Tick **Enable Anti-AFK**.
2. Set the **Interval (seconds)** (minimum 5).
3. Pick a **Mode**:
   - **Random F-key (F22..F24)** — silent, no in-game effect. _Recommended._
   - **WASD jiggle** — taps a movement key (avatar may twitch).
   - **Mouse jiggle** — nudges the cursor by 1 pixel.

   Input only fires while the Roblox window is in the foreground, so it never
   disturbs other apps.

### Multi-Instance
1. Launch your **first** Roblox window normally.
2. Click **Run once now**, or tick **Enable continuous multi-instance closer
   (every 2 s)** if you plan to open more sessions later.
3. Launch each **additional** Roblox window.

   This closes the `ROBLOX_singletonEvent` / `ROBLOX_singletonMutex` objects that
   normally limit Roblox to one window per session.

### Settings
Everything is saved automatically. Use **File → Import settings… (Ctrl+I)** and
**File → Export settings… (Ctrl+E)** to move a JSON config between machines.

## Requirements

- **Windows** (Windows-only by design).
- Runtime: `d3d11.dll` and `d3dcompiler_47.dll` (present on any modern Windows).
- Build: a MinGW toolchain (`gcc`, `g++`, `windres`, `dlltool`) on your `PATH`.

> Built and tested against **MinGW.org GCC 6.3.0** (32-bit). Because that old
> toolchain ships no DirectX/WIC headers or import libraries, the repo carries
> small compatibility shims (`src/third_party/stub_includes/`) and generates the
> needed import libs at build time. It should also build cleanly on
> **MinGW-w64**, where most of those shims become unnecessary.

## Build

```bat
build.bat
```

The script:

1. Generates import libraries for `d3d11` / `d3dcompiler_47` / `SetProcessDPIAware`
   from the `.def` files in `libs\` (via `dlltool`).
2. Compiles the C core, the Dear ImGui sources + DX11/Win32 backends, and the GUI.
3. Links everything into **`RobloxHandler.exe`**.

No external dependencies to install — Dear ImGui is vendored under
`src/third_party/imgui/`.

## How it works

- **Backend** — pure C modules, no C++ runtime needed:
  `process`, `uri`, `json`, `favorites`, `settings`, `anti_afk`, `crash_detect`,
  `singleton`, `thumbnail`.
- **Frontend** — Dear ImGui rendered through a DirectX 11 swapchain on a native
  Win32 host window. Image thumbnails are decoded with WIC.
- DXGI and WIC are reached purely through COM (`CoCreateInstance`), so no extra
  import libraries are required for them.

## Project layout

```
src/
  core/         pure-C backend modules
  gui/          App.cpp (host window + D3D11 + main loop), Tabs.cpp, Texture.cpp
  third_party/
    imgui/      vendored Dear ImGui + Win32/DX11 backends
    stub_includes/  MinGW.org compatibility shims (DX/WIC headers, uuid glue)
libs/           .def files -> import libs generated by dlltool
build.bat       one-shot build script
```

## Disclaimer

This is a personal/educational automation tool. Automating a game client may be
against Roblox's Terms of Service — **use it at your own risk**. The authors
take no responsibility for account actions or any other consequences.

## License

Released under the [MIT License](LICENSE).
