<h1 align="center">ScreenOff Lock</h1>

<p align="center"><b>English</b> | <a href="README.md">简体中文</a></p>

<p align="center">A lightweight Windows program that <b>locks the screen when the display turns off or the lid is closed</b>.</p>

---
## Quick start

Download  `ScreenOffLock.exe`  from the [Releases page](https://github.com/AomiRaku/Screen-Off-Lock/releases/latest).

Just double-click `ScreenOffLock.exe` to run it — no administrator rights needed.

For autostart, right-click the tray icon and tick **Run at startup** (it writes the
registry Run key).

To remove completely: untick **Run at startup** → exit → delete the exe → delete
`HKCU\Software\ScreenOffLock`.

## Features

- Lives in the notification area with a single icon and no window
- **Only the right mouse button does anything**; left click is deliberately ignored
- Right-click menu:

  | Item | Description |
  | --- | --- |
  | Lock when screen turns off | Checkable; takes effect immediately and is saved |
  | Lock when lid is closed | Same; greyed out on desktops with no lid device |
  | Lock delay | Radio submenu: Immediately / 3 / 5 / 10 / 30 seconds. Locks that many seconds after the display turns off or the lid closes; waking the display or opening the lid in the meantime cancels it |
  | —— separator —— | |
  | Run at startup | Starts automatically at sign-in; unchecking **deletes** the registry value |
  | Power settings | Verifies the power button / lid actions against the recommended values, with one-click fix |
  | —— separator —— | |
  | Check for updates | Same as the button of that name in the About window: looks for a newer release |
  | About | Tagline, author, project link and license (both clickable), version and registry location; also a language button and a "Check for updates" button |
  | Exit | Removes the tray icon and quits |

- **Localization**: English / Simplified Chinese. On first run a language picker appears
  first; the choice is stored in the registry and never asked again. To change it later,
  use **Language / 语言** in the About window (open dialogs are closed so they can be
  rebuilt in the new language).
- Settings live in `HKCU\Software\ScreenOffLock`, all DWORDs:
  - `LockWhenScreenOff` — lock when the display turns off, `1` = on / `0` = off (default on)
  - `LockWhenLidClosed` — lock when the lid closes, `1` = on / `0` = off (default on)
  - `LockDelay` — lock delay in seconds, `0` = immediately (absent is treated as `0`);
    only `0` / `3` / `5` / `10` / `30` are accepted, anything else falls back to `0`
  - `Language` — `0` = English, `1` = Simplified Chinese (**absent** means first run)
  - `SkipSettingsWarning` — suppress the first-run settings notice (not written by default)
  - `SkippedVersion` — the remote version number remembered by "Skip this version"
    (not written by default)
- Autostart is stored as a `ScreenOffLock` value under
  `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, containing the quoted full path to
  the exe. **Unchecking deletes that value outright** — no `0` placeholder is left behind.
  The Run key itself belongs to Windows and is never created or removed, and other entries
  in it are untouched.
- Launching the exe twice shows "Already running." and exits.

## Building from source

```powershell
pwsh -File build.ps1
```

The output is `dist\ScreenOffLock.exe`.

The repository does not ship a toolchain. If `g++` is not found, the script calls
`scripts/fetch-toolchain.py` to download and unpack a portable **w64devkit**
(self-contained MinGW-w64, ~64 MB) into `.toolchain\` inside the repo. Nothing is written
to system directories and no administrator rights are needed; deleting `.toolchain\`
uninstalls the toolchain completely.

Python 3 is required for the download only — compiling does not depend on it. If you
already have MinGW-w64, putting `g++.exe` on PATH works too; the script prefers the copy
inside `.toolchain\`.

```powershell
pwsh -File build.ps1 -Clean       # rebuild from scratch
pwsh -File build.ps1 -Reinstall   # re-download the toolchain as well
```

## Power settings

For this program to work, **the screen must actually be turned off and the lid state must
be reported by the system**. The **Power settings** menu item reads two values from the
current power plan:

| Setting | Power setting GUID | Recommended (AC / battery) |
| --- | --- | --- |
| Power button action | `{7648EFA3-DD9C-4E3E-B566-50F929386280}` | Turn off display \| Turn off display |
| Lid close action | `{5CA83367-6E45-459F-A27B-476B1D01C936}` | Do nothing \| Sleep |

Both live in the subgroup `{4F971E89-EEBD-4455-A8DE-9E59040E7347}` (Power buttons and lid).
They are visible in Control Panel yet are **hidden** power settings — `powercfg /query`
does not list them, so they can only be read and written via the API by GUID. Values are
the indices in the power plan: `0` do nothing, `1` sleep, `2` hibernate, `3` shut down,
`4` turn off display.

Rules:

- On battery, a lid action that is not `Sleep` still counts as OK as long as it is
  `Do nothing` — under this program's semantics the two behave identically, since the
  software locks the screen itself.
- Everything else is compared strictly against the recommended value.
- Each power source gets its own verdict: both OK is "All good.", neither is "With these
  settings the lid and the power button cannot lock the screen; only an automatic display
  timeout can.", and a single mismatch is called out on its own.

**Reading needs no administrator rights** (`PowerReadACValueIndex` /
`PowerReadDCValueIndex` work for ordinary users — note that `SchemeGuid` must be the
active scheme GUID; passing `NULL` returns `ERROR_INVALID_PARAMETER`). The two
"Apply" buttons, however, **write** the power plan, which does require elevation, so they
launch an elevated copy of the program via `ShellExecuteEx` + `runas` (command-line flags
`--apply-ac` / `--apply-dc`) and refresh afterwards. Declining the UAC prompt simply does
nothing.

With `--apply-ac` or `--apply-dc` on the command line the program only writes the power
plan and exits — no window, no message loop, and it does not take part in the
single-instance check.

On first launch, if the settings are not all at their recommended values, a notice appears
once. Ticking "Don't show this again" writes `SkipSettingsWarning` and it stops appearing.

## Checking for updates

The number after "Build" in `APP_VERSION` is compared against the tag of the latest GitHub
Release (tags are plain numbers such as `1400`, so just keep the tag equal to the build
number when publishing).

- **A silent check runs at startup.** A prompt appears only when there really is a newer
  version. A failed check is retried, up to 3 times, and then dropped quietly — no error,
  no nagging. Ticking "Skip this version" records the remote version number in
  `SkippedVersion` so that version is not offered again; a newer release brings the prompt
  back.
- **"Check for updates" — in the tray menu and in the About window** — opens a small window
  that first says "Checking for updates..." (cancellable), then turns into "New version
  available: Build N / You already have the latest version / Update check failed", with the
  button changing to "Get update / Done / Close". "Get update" opens the Release page in the
  default browser.
- **Both windows show the release notes** — the text you write when publishing. Split them
  into Chinese and English with a line containing just `---` and the Chinese UI gets the
  Chinese half, the English UI the English one (decided by how many CJK characters each
  half has, so the order does not matter). Markdown markers are stripped: `#`, `**`, `*`
  and `` ` `` are removed, links keep only their text, and images become a short "see the
  release page" note since they cannot be shown here. The window grows and shrinks with
  the length of the notes.
- Requests go through WinHTTP, but the **functions are fetched from `winhttp.dll` at
  runtime**, so the static import table is still the same six system DLLs — no hard
  networking dependency, and if `winhttp.dll` is missing this feature simply does nothing
  while everything else keeps working. The request runs on a worker thread and never
  blocks the tray menu.
- Nothing else in the program touches the network: only these update actions do.

## Requirements

| OS | Runs | About icon | Rounded corners |
| --- | --- | --- | --- |
| Windows 11 | Yes | Segoe Fluent Icons glyph | Yes |
| Windows 10 | Yes | Segoe MDL2 Assets glyph (near-identical) | No, square |
| Windows 8.1 / 8 | Yes | System bitmap icon | No, square (matches the OS style) |
| Windows 7 | Yes | System bitmap icon | Aero rounded |
| Windows Vista | Theoretically, untested | System bitmap icon | — |
| Windows XP | No | — | — |

The hard floor for statically imported APIs is **Vista**: `RegisterPowerSettingNotification`
was introduced there, so on XP the exe fails to load with a missing entry point. The PE
subsystem version is 5.2 and the import table holds only six core system DLLs
(`ADVAPI32 / GDI32 / KERNEL32 / msvcrt / SHELL32 / USER32`); `dwmapi`, `shcore` and friends
are loaded dynamically at runtime with fallbacks, so they are not hard dependencies.

Fonts and icons are probed at runtime and fall back step by step, so no machine will ever
draw a tofu box because a font is missing: the UI font goes
Segoe UI Variable Text → Segoe UI, and the icon font goes
Segoe Fluent Icons → Segoe MDL2 Assets → system bitmap icon.

Note that **only Windows 11 has been verified on real hardware**; the other rows are
inferred from the API and font availability versions.

## How it works

The program does very little:

1. Creates an **invisible top-level window** to receive system messages;
2. Subscribes to two power settings with `RegisterPowerSettingNotification`, both
   delivered through `WM_POWERBROADCAST` / `PBT_POWERSETTINGCHANGE`:

   | Power setting GUID | Meaning | Locking edge |
   | --- | --- | --- |
   | `GUID_CONSOLE_DISPLAY_STATE`<br>`{6FE69556-704A-47A0-8F24-C28D936FDA47}` | Display power: `0` off / `1` on / `2` dimmed | `1 → 0` |
   | `GUID_LIDSWITCH_STATE_CHANGE`<br>`{BA3E0F4D-B817-4094-A2D1-D56379E6A0F3}` | Lid switch: `0` closed / `1` open | `1 → 0` |

3. Calls `LockWorkStation()` once when an edge is hit;
4. Declares **Per-Monitor V2 DPI awareness** at startup
   (`SetProcessDpiAwarenessContext`, falling back to `SetProcessDPIAware`). Without it,
   Windows treats the process as DPI-unaware and bitmap-stretches every window, context
   menu and dialog, which makes text blurry on high-DPI displays.

Both state machines act only on the open-to-closed edge, so repeated notifications never
lock twice; and if the display was already off or the lid already closed at startup, no
lock happens immediately. `LockWorkStation()` is idempotent, so both paths firing at once
is harmless.

With a lock delay set, hitting an edge only records a pending flag and starts a `SetTimer`
timer; the toggles and the current state are checked once more before locking. Waking the
display (dimming included) clears the "screen off" pending flag, and opening the lid clears
the "lid" one — the two are independent: closing a laptop lid usually raises both
notifications, so opening the lid without waking the display still lets the "screen off"
one lock on time.

Whether the machine has a lid is decided by `GetPwrCapabilities`' `LidPresent`, **not** by
the return value of `RegisterPowerSettingNotification` — measurements show the latter does
not validate the GUID at all: an invented GUID still yields a success handle, so using its
return value as the criterion would be wrong.

## Interface

Only the tray menu and the About window are visible, and both follow the Windows 11
visual language:

- The **tray context menu** gets its modern styling from the Common Controls 6.0
  dependency declared in `src\app.manifest`. Without that declaration Windows falls back
  to the classic Windows 95 control look — 3D borders, square corners, period colours.
- The **About window** does not use `MessageBox`: its icon can only be one of the old
  `IDI_*` bitmaps and its appearance is stuck in classic control styling. This one is
  custom-drawn — DWM supplies the rounded corners, the font is Segoe UI Variable Text
  (falling back to Segoe UI on older systems), the icon comes from a Segoe Fluent Icons
  glyph (`U+E946` ⓘ), and the buttons stay standard system buttons to keep the native feel.

`--about` on the command line opens the About window directly, which is handy for a quick look.

## Known limitations

- **Power-button screen-off cannot be distinguished from an automatic display timeout.**
  Windows sends an identical notification for both and never tells the application why the
  display was switched off. So once enabled, *any* reason for the display turning off will
  lock the screen. This is a Windows limitation, not an implementation shortcut.
- **Desktops that simply cut power to the monitor are not covered.** A physical power-down
  removes the display from the system topology and follows a different detection path; and
  many monitors keep HPD high while idle, so the system never notices at all. Left for a
  future version.
- The tray icon is generated by `scripts\make-icon.ps1` — two Segoe Fluent Icons glyphs
  (a monitor plus a closed lock) composed into a rounded square, committed as `src\app.ico`.
  It is still a placeholder and can be replaced at any time.

## Layout

```Tree
ScreenOffLock\
├─ src\main.cpp                all source (single file)
├─ src\app.rc                  icon, version info and manifest resources
├─ src\app.manifest            application manifest (Common Controls 6.0 / DPI awareness)
├─ src\app.ico                 application icon (generated, committed)
├─ scripts\make-icon.ps1       generates src\app.ico
├─ scripts\fetch-toolchain.py  downloads the portable toolchain
├─ build.ps1                   build script
├─ build\                      intermediate output (resource .o)
├─ dist\                       build output
└─ .toolchain\                 auto-downloaded toolchain (safe to delete)
```

## License

Released under the [MIT License](LICENSE).
