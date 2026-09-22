import ctypes
import json
import sys
import time
from ctypes import wintypes

import pyautogui

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

VK_SPACE = 0x20
VK_RETURN = 0x0D
VK_ESCAPE = 0x1B

user32.GetForegroundWindow.restype = wintypes.HWND
user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
user32.GetWindowTextLengthW.restype = ctypes.c_int
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetWindowTextW.restype = ctypes.c_int
user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.GetWindowRect.restype = wintypes.BOOL
user32.GetAsyncKeyState.argtypes = [ctypes.c_int]
user32.GetAsyncKeyState.restype = ctypes.c_short
kernel32.GetConsoleWindow.restype = wintypes.HWND


class WindowInfo:
    __slots__ = ("left", "top", "title", "hwnd")

    def __init__(self, left, top, title, hwnd):
        self.left = left
        self.top = top
        self.title = title
        self.hwnd = hwnd


def foreground_window():
    hwnd = user32.GetForegroundWindow()
    if not hwnd:
        return None
    rect = wintypes.RECT()
    if not user32.GetWindowRect(hwnd, ctypes.byref(rect)):
        return None
    length = user32.GetWindowTextLengthW(hwnd)
    buf = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(hwnd, buf, length + 1)
    return WindowInfo(rect.left, rect.top, buf.value, hwnd)


def key_down(vk):
    return (user32.GetAsyncKeyState(vk) & 0x8000) != 0


def main():
    if len(sys.argv) != 2:
        print("usage: pick_coords.py output.json", file=sys.stderr)
        print("tracks the active window; click your target app, then hover buttons and press SPACE to capture, ESC to quit.", file=sys.stderr)
        sys.exit(2)

    out_path = sys.argv[1]

    print("tracking the active window.")
    print("click your target app to make it the active window, then hover buttons.")
    print("press SPACE or ENTER to capture, ESC to finish.")
    print()

    captured = []
    last_win = None
    last_len = 0

    while True:
        fg = foreground_window()
        if fg is not None:
            last_win = fg

        target = last_win
        p = pyautogui.position()
        if target is not None:
            rx, ry = p.x - target.left, p.y - target.top
            name = target.title if target.title else "(untitled)"
            disp = "rel=({:>4},{:>4})  abs=({:>4},{:>4})  win={}  [SPACE capture | ESC quit]".format(
                rx, ry, p.x, p.y, name)
        else:
            disp = "abs=({:>4},{:>4})  win=(none yet)  [SPACE capture | ESC quit]".format(p.x, p.y)

        pad = max(0, last_len - len(disp))
        sys.stdout.write("\r" + disp + " " * pad)
        sys.stdout.flush()
        last_len = len(disp)

        if key_down(VK_ESCAPE):
            break
        if key_down(VK_SPACE) or key_down(VK_RETURN):
            if target is None:
                sys.stdout.write("\n")
                sys.stdout.flush()
                print("(no active window to capture against; click your target app first)")
                last_len = 0
                while key_down(VK_SPACE) or key_down(VK_RETURN):
                    time.sleep(0.01)
                continue

            rx, ry = p.x - target.left, p.y - target.top
            action = {"window_title": target.title, "action": "click", "x": rx, "y": ry}
            captured.append(action)

            sys.stdout.write("\n")
            sys.stdout.flush()
            print("captured #{}: {} (window=\"{}\")".format(
                len(captured), json.dumps(action, ensure_ascii=False), target.title))
            last_len = 0

            while key_down(VK_SPACE) or key_down(VK_RETURN):
                time.sleep(0.01)

        time.sleep(0.02)

    sys.stdout.write("\n")
    print()
    print("=== captured {} action(s) ===".format(len(captured)))
    print(json.dumps(captured, indent=2, ensure_ascii=False))

    if captured:
        config = {"actions": captured}
        with open(out_path, "w", encoding="utf-8") as fh:
            json.dump(config, fh, indent=2, ensure_ascii=False)
        print("wrote config to {}".format(out_path))

    print("done")


if __name__ == "__main__":
    main()
