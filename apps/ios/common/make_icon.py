#!/usr/bin/env python3
"""The iOS apps' icon: an asset catalog (Assets.xcassets/AppIcon.appiconset) from the same artwork as the Android
icon (apps/android/tools/make_icon.py: store_icon(), the Google Play picture: the gold frame round the app's tile, on
the tile's colour, opaque).

    make_icon.py --conf apps/android/app-board.conf --art build/ios/board/icon-art --out build/ios/board/Assets.xcassets
    make_icon.py --conf apps/android/app-full.conf --out build/ios/arcade/Assets.xcassets

One 1024 x 1024 picture, RGB without alpha (the App Store refuses an icon with transparency), as Xcode 14 and later
take it ("single size": Xcode makes every smaller size the iPhone, the iPad, Settings and Spotlight need from it).
The artwork is drawn 512 px wide (TILE: make_icon.py's own drawing size), so the 1024 picture is that drawing scaled
up smoothly; the icons on the devices (180 px and smaller) come out sharp.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import json
import os
import sys

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "apps", "android", "tools"))
import make_icon  # noqa: E402  (the Android icons: the same artwork)

INFO = {"author": "xcode", "version": 1}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--conf", required=True, help="the app's .conf (its ICON_STYLE, ICON_GAMES, PROFILE)")
    ap.add_argument("--art", help="ICON_STYLE=board: the app's pictures (make -f apps/boardgames/Makefile icon-art)")
    ap.add_argument("--out", required=True, help="the asset catalog to write (Assets.xcassets)")
    a = ap.parse_args()
    games, full = make_icon.conf_games(a.conf)
    style = make_icon.read_conf(a.conf).get("ICON_STYLE")
    board = style in ("board", "retrostone")
    if board and not a.art:
        ap.error(f"ICON_STYLE={style} needs --art")
    icon = make_icon.store_icon(games, full, a.art if board else None, style).convert("RGB")
    icon = icon.resize((1024, 1024), Image.LANCZOS)
    d = os.path.join(a.out, "AppIcon.appiconset")
    os.makedirs(d, exist_ok=True)
    icon.save(os.path.join(d, "AppIcon-1024.png"))
    with open(os.path.join(d, "Contents.json"), "w") as f:
        json.dump({"images": [{"filename": "AppIcon-1024.png", "idiom": "universal", "platform": "ios",
                               "size": "1024x1024"}], "info": INFO}, f, indent=2)
    with open(os.path.join(a.out, "Contents.json"), "w") as f:
        json.dump({"info": INFO}, f, indent=2)
    print("make_icon: %s (1024 x 1024)" % os.path.join(d, "AppIcon-1024.png"))


if __name__ == "__main__":
    main()
