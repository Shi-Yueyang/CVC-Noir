import json
import os
import sys
import time

import pyautogui
import pygetwindow as gw

READY_DELAY = 3.0
SETTLE_DELAY = 0.5

ACTIONS = [
    {"window_title": "300C", "action": "click", "x": 120, "y": 80},
    {"action": "wait", "x": 1.0},
    {"window_title": "300C", "action": "click", "x": 340, "y": 200},
    {"action": "wait", "x": 0.3},
    {"window_title": "300C", "action": "type", "x": "hello"},
    {"action": "wait", "x": 0.3},
    {"window_title": "300C", "action": "press", "x": "enter"},
]


def load_config(path):
    with open(path, "r", encoding="utf-8-sig") as fh:
        data = json.load(fh)
    cfg = {}
    if "ready_delay" in data:
        cfg["READY_DELAY"] = float(data["ready_delay"])
    if "settle_delay" in data:
        cfg["SETTLE_DELAY"] = float(data["settle_delay"])
    if "actions" in data:
        cfg["ACTIONS"] = data["actions"]
    return cfg


def find_window(title):
    matches = [w for w in gw.getWindowsWithTitle(title) if title in w.title]
    return matches[0] if matches else None


def activate_window(win):
    if win.isMinimized:
        win.restore()
    try:
        win.activate()
    except Exception:
        pass


def run_action(action, origin_x, origin_y):
    kind = action["action"]
    if kind == "click":
        x = int(action["x"])
        y = int(action["y"])
        abs_x = origin_x + x
        abs_y = origin_y + y
        pyautogui.click(abs_x, abs_y)
        print("click rel=({}, {}) abs=({}, {})".format(x, y, abs_x, abs_y))
    elif kind == "wait":
        duration = float(action["x"])
        time.sleep(duration)
        print("wait {}s".format(duration))
    elif kind == "type":
        text = str(action["x"])
        pyautogui.typewrite(text, interval=0.02)
        print("type {!r}".format(text))
    elif kind == "press":
        key = str(action["x"])
        pyautogui.press(key)
        print("press {}".format(key))
    else:
        print("unknown action: {}".format(action), file=sys.stderr)


def resolve_origin(action, current_title, origin_x, origin_y):
    if action["action"] == "wait":
        return current_title, origin_x, origin_y

    target_title = action.get("window_title")
    if not target_title:
        print("action missing required 'window_title': {}".format(action), file=sys.stderr)
        return current_title, origin_x, origin_y

    win = find_window(target_title)
    if win is None:
        print("window '{}' not found; using last origin".format(target_title), file=sys.stderr)
        return current_title, origin_x, origin_y

    if target_title != current_title:
        activate_window(win)
        print("focus window '{}'".format(target_title))

    return target_title, win.left, win.top


def main():
    pyautogui.FAILSAFE = True
    pyautogui.PAUSE = 0.1

    ready_delay = READY_DELAY
    settle_delay = SETTLE_DELAY
    actions = ACTIONS

    if len(sys.argv) > 1:
        cfg_path = sys.argv[1]
        if not os.path.isfile(cfg_path):
            print("config not found: {}".format(cfg_path), file=sys.stderr)
            sys.exit(2)
        cfg = load_config(cfg_path)
        ready_delay = cfg.get("READY_DELAY", ready_delay)
        settle_delay = cfg.get("SETTLE_DELAY", settle_delay)
        actions = cfg.get("ACTIONS", actions)
        print("loaded config: {}".format(cfg_path))

    print("starting in {}s  (move mouse to a screen corner to abort)".format(ready_delay))
    time.sleep(ready_delay)

    current_title = None
    origin_x = 0
    origin_y = 0

    for action in actions:
        current_title, origin_x, origin_y = resolve_origin(action, current_title, origin_x, origin_y)
        run_action(action, origin_x, origin_y)
        time.sleep(settle_delay)

    print("done")


if __name__ == "__main__":
    main()
