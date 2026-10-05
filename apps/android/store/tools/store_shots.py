#!/usr/bin/env python3
"""The phone screenshots of both store listings, made headlessly from the apps themselves (the launcher and the games,
their bots playing), in 3 languages: apps/android/store/<free|full>/screenshots/<en|fr|de>/NN-<what>.png.

    make -f apps/launcher/bundle.mk                                    (the free app's headless runner)
    make -f apps/launcher/bundle.mk CONF=apps/android/app-full.conf    (the full app's)
    python3 apps/android/store/tools/store_shots.py

Each picture is the app's screen at a phone's width (480 x 240: 2:1, as an 18:9 phone in landscape), scaled to
2160 x 1080 (sharp pixels: x5, then smoothed down). Google Play accepts 16:9 to 2:1 (the longer side at most twice
the shorter). A game that has not opted in to widescreen (320 columns) gets the side borders the app draws: its own
picture, blurred and darkened.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageEnhance, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
STORE = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(STORE, "..", "..", ".."))
WIDTH, OUT_W, OUT_H = 480, 2160, 1080
LANGS = ("en", "fr", "de")
RUNNERS = {"free": "build/bundle/host/retrostonearcade_headless",
           "full": "build/bundle/host-full/retrostonearcade_headless"}
# what to show: (file name, game id or None for the launcher, frames, launcher touches)
SHOTS = {
    "free": [("01-menu", None, 40, ""),
             ("02-leadysquid", "leadysquid", 700, ""),
             ("03-pancaketower", "pancaketower", 900, ""),
             ("04-beaverrush", "beaverrush", 600, ""),
             ("05-full-version", None, 60, "10 tap-at 396 68\n")],      # a locked card: "Get the full version"
    "full": [("01-menu", None, 40, ""),
             ("02-duckparade", "duckparade", 900, ""),
             ("03-pogomamie", "pogomamie", 900, ""),
             ("04-molemarbles", "molemarbles", 900, ""),
             ("05-spiderswing", "spiderswing", 700, ""),
             ("06-otterrapids", "otterrapids", 900, ""),
             ("07-menu-more", None, 120, "10 touch 240 200\n14 move 240 150\n18 move 240 100\n22 move 240 60\n24 up\n")],
}


def compose(png, out):
    im = Image.open(png).convert("RGB")
    w, h = im.size
    if w < WIDTH:                                   # pillarbox: the frontend's borders
        bg = im.resize((WIDTH * 5 // 4, h * 5 // 4), Image.BILINEAR).filter(ImageFilter.GaussianBlur(10))
        bg = ImageEnhance.Brightness(bg).enhance(0.4).crop((WIDTH // 8, h // 8, WIDTH // 8 + WIDTH, h // 8 + h))
        bg.paste(im, ((WIDTH - w) // 2, 0))
        im = bg
    big = im.resize((im.width * 5, im.height * 5), Image.NEAREST)
    big.resize((OUT_W, OUT_H), Image.LANCZOS).save(out, optimize=True)


def main():
    tmp = tempfile.mkdtemp(prefix="rsa-shots-")
    try:
        for app, shots in SHOTS.items():
            runner = os.path.join(ROOT, RUNNERS[app])
            if not os.path.exists(runner):
                sys.exit(f"{runner} is missing: build it first (see the top of this file)")
            for lang in LANGS:
                d = os.path.join(STORE, app, "screenshots", lang)
                os.makedirs(d, exist_ok=True)
                for name, game, frames, script in shots:
                    saves = os.path.join(tmp, f"{app}-{lang}-{name}")
                    os.makedirs(saves, exist_ok=True)
                    raw = os.path.join(tmp, "raw.png")
                    cmd = [runner, "--saves", saves, "--width", str(WIDTH), "--lang", lang, "--frames", str(frames),
                           "--png", raw]
                    if game:
                        cmd += ["--start", game, "--fresh", "--opt", "bot=1"]
                    if script:
                        sf = os.path.join(tmp, "input.txt")
                        open(sf, "w").write(script)
                        cmd += ["--input", sf]
                    subprocess.run(cmd, check=True, cwd=ROOT, stdout=subprocess.DEVNULL)
                    compose(raw, os.path.join(d, name + ".png"))
                print(f"{d}: {len(shots)} screenshots")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
