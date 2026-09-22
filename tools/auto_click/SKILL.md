---
name: auto_click
description: Automate GUI button clicks and ordered mouse/keyboard sequences on Windows using a pyautogui script. Use when the user wants to script repetitive clicks on one or more GUI apps via window-relative coordinates, or drive Windows GUIs programmatically in a defined order.
---

# auto_click

Drive one or more Windows GUI apps by clicking window-relative coordinates and
typing keys in a deterministic order, using `pyautogui`. One config can operate
multiple windows; coordinates are resolved against each window's current
position at run time, so clicks stay correct if a window moves.

## Location

- Script: `skills/auto_click/auto_click.py`
- Coordinate picker: `skills/auto_click/pick_coords.py`
- Dependencies: `skills/requirements.txt` (shared by all scripts under `skills/`)
- Python venv (shared, gitignored): `skills/.venv/`

## When to use

- You need a deterministic sequence of clicks / waits / typing / key presses.
- The target window(s) can be focused by title.
- You want one config to drive multiple windows in sequence.

Do NOT use when the app resizes between runs (coordinates are based on the
window origin, not scaled to its size), or when control-identity automation
(`pywinauto` / UI Automation) is feasible — prefer that for robustness.

## Setup (one-time, per machine)

```powershell
python -m venv skills/.venv
skills/.venv/Scripts/python.exe -m pip install -r skills/requirements.txt
```

The venv lives at `skills/.venv/` and is shared by all scripts under `skills/`.

## Configuration

Two ways to configure the run:

1. **Edit constants** at the top of `auto_click.py`
   (`READY_DELAY`, `SETTLE_DELAY`, `ACTIONS`).
2. **Pass a JSON config file** (preferred for agent use — no source edits):
   ```powershell
   skills/.venv/Scripts/python.exe skills/auto_click/auto_click.py path/to/config.json
   ```

### JSON schema

Each action is an object with four fields: `window_title`, `action`, `x`, `y`.

```json
{
  "ready_delay": 3.0,
  "settle_delay": 0.5,
  "actions": [
    {"window_title": "300C", "action": "click", "x": 120, "y": 80},
    {"action": "wait", "x": 1.0},
    {"window_title": "OtherWindow", "action": "click", "x": 340, "y": 200},
    {"action": "wait", "x": 0.3},
    {"window_title": "300C", "action": "type", "x": "hello"},
    {"action": "wait", "x": 0.3},
    {"window_title": "300C", "action": "press", "x": "enter"}
  ]
}
```

- `window_title` (required for click/type/press): the window to focus before the
  action. One config can drive multiple windows by setting a different title per
  action. If omitted on a non-`wait` action, a warning is logged and the last
  known origin is reused.
- `wait` actions ignore `window_title`.
- Top-level `ready_delay` and `settle_delay` are optional; omitted values fall
  back to the script defaults.

### Action types

| Action object                                | Meaning                                          |
|----------------------------------------------|--------------------------------------------------|
| `{"window_title":W,"action":"click","x":X,"y":Y}` | Left-click at window-relative pixel (X,Y)   |
| `{"action":"wait","x":S}`                    | Sleep `S` seconds                                |
| `{"window_title":W,"action":"type","x":"text"}`   | Type a string (per-key interval 0.02s)      |
| `{"window_title":W,"action":"press","x":"key"}`   | Press a single key (e.g. `"enter"`)         |

For non-click actions the `y` field is unused; `x` carries the value (seconds,
text, or key name).

## How to find the window title

```powershell
skills/.venv/Scripts/python.exe -c "import pygetwindow as gw; [print(w) for w in gw.getAllTitles() if w.strip()]"
```

Pick the substring that uniquely matches the target window and set
`window_title` to it.

## How to find click coordinates

The easiest way is the bundled interactive picker:

```powershell
skills/.venv/Scripts/python.exe skills/auto_click/pick_coords.py output.json
```

It tracks the currently active window and prints the window-relative coordinate
live (`rel=(x,y)`). Run it, click your target app to bring it to the foreground,
hover over a button, and press **SPACE** or **ENTER** to capture it. Switch focus
to another window at any time to capture from it too. Press **ESC** to finish.

Captured actions are written to the output path as a ready-to-use config:

```json
{
  "actions": [
    {"window_title": "window title", "action": "click", "x": 120, "y": 80},
    {"window_title": "other window",  "action": "click", "x": 340, "y": 200}
  ]
}
```

Each capture records the window it was taken from, so switching focus between
windows while picking naturally produces a multi-window config.

Notes about the picker:
- Origin is the window's outer top-left corner (includes the title bar), matching
  how the click script resolves coordinates — so captured values drop straight in.
- Uses Win32 `GetAsyncKeyState` for global key polling, so SPACE/ENTER work even
  while the target window has focus (not the console). No extra dependencies.
- Windows-only.

### Manual alternatives

- Spy++ (VS Tools → Spy++, Window Finder crosshair) reports coordinates relative
  to the selected window.
- AutoHotkey WindowSpy shows both screen and client-relative coordinates.
- Snapshot with `pyautogui` + `pygetwindow` to compute the offset:
  ```powershell
  skills/.venv/Scripts/python.exe -c "import pyautogui, time, pygetwindow as gw; time.sleep(3); w=gw.getActiveWindow(); p=pyautogui.position(); print('rel', p.x-w.left, p.y-w.top, 'abs', p.x, p.y, 'win', w.left, w.top)"
  ```
  Run it, then move the mouse onto the button before the 3s timer expires.

Note: Windows DPI scaling can shift pixel coordinates. If clicks land off-target,
verify the app's DPI-awareness / display scaling setting.

## Running

```powershell
skills/.venv/Scripts/python.exe skills/auto_click/auto_click.py [config.json]
```

## Failsafe

`pyautogui.FAILSAFE = True`. To abort a run, slam the mouse to any screen
corner — the script raises `FailSafeException` and stops immediately.

## Constraints

- Windows-only (uses `pygetwindow` window activation).
- Click coordinates are relative to the target window's top-left corner; if a
  window cannot be found, the script prints a warning and reuses the last known
  origin. Window size still matters: coordinates are based on the window origin,
  not scaled to its size.
- Do not use this skill to make functional edits in `Source/ATP_CODE`; keep
  automation tooling under `skills/`.
