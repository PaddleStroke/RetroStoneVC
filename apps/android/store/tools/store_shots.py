#!/usr/bin/env python3
"""The phone screenshots of both store listings, made headlessly from the apps themselves (the launcher and the games,
their bots playing), in 3 languages: apps/android/store/<free|full>/screenshots/<en|fr|de>/NN-<what>.png.

    make -f apps/launcher/bundle.mk                                    (the free app's headless runner)
    make -f apps/launcher/bundle.mk CONF=apps/android/app-full.conf    (the full app's)
    python3 apps/android/store/tools/store_shots.py

Each picture is the app's screen at 16:9 (427 x 240, a phone in landscape), scaled to 1920 x 1080 (sharp pixels:
x5, then smoothed down): Google Play asks for 16:9 or 9:16, and at least 1080 px for promotion. A game's picture is
the moment of play its launcher card shows (apps/launcher/cards/cards.cfg: the frame and the options), played by its
own headless runner (make GAME=<id> build/host/<id>_headless) in the screenshot's language. A game that has not
opted in to widescreen (320 columns) gets the side borders the app draws: its own picture, blurred and darkened.
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
WIDTH, OUT_W, OUT_H = 427, 1920, 1080
LANGS = ("en", "fr", "de")
RUNNERS = {"free": "build/bundle/host/retrostonearcade_headless",
           "full": "build/bundle/host-full/retrostonearcade_headless"}
# what to show: (file name, game id or None for the launcher, frames, launcher touches); a game: its card's moment
SHOTS = {
    "free": [("01-menu", None, 40, ""),
             ("02-leadysquid", "leadysquid", 0, ""),
             ("03-pancaketower", "pancaketower", 0, ""),
             ("04-beaverrush", "beaverrush", 0, ""),
             ("05-full-version", None, 60, "10 tap-at %s\n")],       # a locked card: "Get the full version"
    "full": [("01-menu", None, 40, ""),
             ("02-duckparade", "duckparade", 0, ""),
             ("03-beaverrush", "beaverrush", 0, ""),
             ("04-pogomamie", "pogomamie", 0, ""),
             ("05-penguinslalom", "penguinslalom", 0, ""),
             ("06-nuttybranch", "nuttybranch", 0, ""),
             ("07-radishrascals", "radishrascals", 0, ""),
             ("08-spiderswing", "spiderswing", 0, "")],
}
LOCKED_CARD = "369 64"                                          # the free menu's first locked card (Biscuit Bandit)


def card_moment(game):
    """(frame, options) of the game's line in apps/launcher/cards/cards.cfg"""
    for line in open(os.path.join(ROOT, "apps", "launcher", "cards", "cards.cfg"), encoding="utf-8"):
        f = line.split("#")[0].split()
        if f and f[0] == game:
            return int(f[1]), [kv for kv in f[3:] if not kv.startswith(("sprite=", "panel="))]
    sys.exit(f"{game}: no line in cards.cfg")


def compose(png, out):
    im = Image.open(png).convert("RGB")
    w, h = im.size
    if w < 400:                                     # pillarbox (a 320-column game): the frontend's borders
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
                    if game:                        # the game's own runner, at its card's moment
                        frame, opts = card_moment(game)
                        cmd = [os.path.join(ROOT, "build", "host", f"{game}_headless"), "--width", str(WIDTH),
                               "--lang", lang, "--frames", str(frame + 1), "--shot", f"{frame}:{raw}"]
                        for kv in opts + ["bot=1", "ready=1", "seed=7", "music=0", "sound=0"]:
                            cmd += ["--opt", kv]    # the line's options first: the first --opt of a key wins
                    else:
                        cmd = [runner, "--saves", saves, "--width", str(WIDTH), "--lang", lang, "--frames",
                               str(frames), "--png", raw]
                    if script:
                        sf = os.path.join(tmp, "input.txt")
                        open(sf, "w").write(script.replace("%s", LOCKED_CARD))
                        cmd += ["--input", sf]
                    subprocess.run(cmd, check=True, cwd=ROOT, stdout=subprocess.DEVNULL)
                    compose(raw, os.path.join(d, name + ".png"))
                print(f"{d}: {len(shots)} screenshots")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
