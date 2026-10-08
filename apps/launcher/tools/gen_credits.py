#!/usr/bin/env python3
"""The "Credits & licences" screen's data: build/bundle/gen/credits.c (apps/launcher/src/rsb.h, rsb_credit).

    gen_credits.py --out build/bundle/gen

Every component the apps are made of, with its licence's FULL text (English, as published; the screen's own words
are translated in apps/launcher/strings). The list follows THIRD_PARTY.md: the RetroStone VC games and this app
(code MIT, assets CC BY-NC-SA 4.0, (c) 8BCraft), SDL2 (zlib), libxmp-lite (MIT), GNU Unifont (SIL OFL 1.1),
stb_image_write (MIT, or public domain); the Android apps also list the Google Play In-App Review library and the
Play services and AndroidX libraries it brings (android_credits, compiled for Android only). Last, the apps' privacy
policy (privacy(): Google Play wants it inside the app too). libretro.h, also in THIRD_PARTY.md, is not part of the apps (it is the
libretro cores' header). The texts are read from the files that come with each component; they are made ASCII
(the house font) and their hard-wrapped lines joined into paragraphs (the launcher wraps them to the screen).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import argparse
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
OWNER = "RETROSTONE VC  (C) 2026 8BCRAFT"

ASCII = {"\u2018": "'", "\u2019": "'", "\u201c": '"', "\u201d": '"', "\u2013": "-", "\u2014": "--", "\u00a9": "(c)",
         "\u2026": "...", "\u00a0": " ", "\u00e9": "e", "\u00e8": "e", "\u00f6": "o", "\u00e4": "a", "\u00fc": "u",
         "\u2022": "-", "\u00b7": "-"}


def read(path):
    return open(os.path.join(ROOT, path), encoding="utf-8", errors="replace").read()


def section(text, start, end=None):
    """the part of text from the line matching start (a regex) to the line before end (or the end)"""
    lines = text.splitlines()
    i = next(k for k, l in enumerate(lines) if re.search(start, l))
    j = len(lines)
    if end:
        j = next((k for k in range(i + 1, len(lines)) if re.search(end, lines[k])), len(lines))
    return "\n".join(lines[i:j])


def paragraphs(text, stanzas=False, headings=False):
    """ASCII; the hard-wrapped lines of a paragraph joined; a paragraph keeps its first line's indent (2 at most).
    Lines of dashes separate paragraphs. stanzas: the text has no blank lines (stb): a line ending a sentence
    (. or :) ends its paragraph, and the headings (ALTERNATIVE, Copyright) are paragraphs of their own. headings:
    a line in capitals alone (OFL: PREAMBLE, DEFINITIONS) is a paragraph of its own."""
    for k, v in ASCII.items():
        text = text.replace(k, v)
    text = text.encode("ascii", "replace").decode("ascii").replace("\t", "    ").replace("\r", "")
    text = re.sub(r"(?m)^\s*[-=]{8,}\s*$", "", text)
    if headings:
        text = re.sub(r"(?m)^(\s*[A-Z][A-Z ]{3,38})$", r"\n\1\n", text)
    if stanzas:
        text = re.sub(r"(?m)^((?:ALTERNATIVE|Copyright).*|.*[.:])$", r"\1\n", text)
    out = []
    for block in re.split(r"\n\s*\n", text.strip("\n")):
        lines = [l.rstrip() for l in block.splitlines() if l.strip()]
        if not lines:
            continue
        # list items written one per line (a, b, 1., -): each its own paragraph
        items, cur = [], []
        for l in lines:
            if cur and re.match(r"\s*([a-z0-9ivx]{1,4}[.)]|-|\*)\s", l):
                items.append(cur)
                cur = []
            cur.append(l)
        items.append(cur)
        for it in items:
            indent = min(2, (len(it[0]) - len(it[0].lstrip())) // 2)
            out.append(" " * indent + " ".join(l.strip() for l in it))
    return "\n\n".join(out)


def c_str(s):
    def esc(p):
        return p.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    parts = [s[i:i + 96] for i in range(0, len(s), 96)] or [""]      # cut before escaping: no split escape
    return "\n        ".join('"' + esc(p) + '"' for p in parts)


def credits():
    sdl = sorted(glob.glob(os.path.join(ROOT, "build", "deps", "src", "SDL2-*", "LICENSE.txt")))
    if not sdl:
        raise SystemExit("gen_credits: SDL2's LICENSE.txt is missing (run tools/fetch_sdl2_src.sh build/deps)")
    sdl_v = re.search(r"SDL2-([\d.]+)", sdl[-1]).group(1)
    xmp_readme = read("sdk/third_party/libxmp-lite/README")
    xmp_v = re.search(r"libxmp-lite[^|]*\|\s*([\d.]+)", read("THIRD_PARTY.md"))
    stb = read("sdk/third_party/stb/stb_image_write.h")
    stb_v = re.search(r"stb_image_write - v([\d.]+)", stb)
    return [
        ("RETROSTONE VC", "RSB_ROLE_CODE", "MIT", "RetroStone VC: the games and this app (code)",
         "The code of the RetroStone VC games, of their SDK and of this app.\n\n" + read("LICENSE-MIT")),
        ("RETROSTONE VC", "RSB_ROLE_ART", "CC BY-NC-SA 4.0", "RetroStone VC: the games' art, music and sound",
         "Copyright (c) 2026 Pierre-Louis Boyer (8BCraft). The games' art, music, sound, levels and design documents "
         "are licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International licence. "
         "The licensor's own use (this app) is not limited by it.\n\n" + read("LICENSE-CC-BY-NC-SA-4.0.txt")),
        (f"SDL {sdl_v}", "RSB_ROLE_NONE", "ZLIB", f"Simple DirectMedia Layer {sdl_v} (zlib licence)",
         open(sdl[-1], encoding="utf-8").read()),
        ("LIBXMP-LITE " + (xmp_v.group(1) if xmp_v else ""), "RSB_ROLE_NONE", "MIT",
         "libxmp-lite (the music player, MIT licence)", section(xmp_readme, r"^Extended Module Player Lite",
                                                                  r"^\s*THE SOFTWARE\.\s*$") + "\nTHE SOFTWARE."),
        ("UNIFONT 13.0.06", "RSB_ROLE_NONE", "SIL OFL 1.1",
         "GNU Unifont 13.0.06 (the Japanese, Korean and Chinese glyphs; SIL Open Font License 1.1)",
         read("games/common/fonts/LICENSE-unifont-OFL.txt")),
        ("STB IMAGE WRITE " + (stb_v.group(1) if stb_v else ""), "RSB_ROLE_NONE", "MIT",
         "stb_image_write by Sean Barrett (MIT licence, or public domain)",
         section(stb, r"This software is available under 2 licenses", r"^\*/")),
    ]


# The Android apps only (apps/android/app/build.gradle): the Play In-App Review library and what it brings. Google's
# terms are not licence texts to copy: their names and addresses, as the libraries' Maven files give them.
PLAY_REVIEW = "2.0.2"
PLAY_TEXT = (
    "The Android apps ask Google Play for a rating through the Google Play In-App Review library "
    f"(com.google.android.play:review {PLAY_REVIEW}, with com.google.android.play:core-common 2.0.4) and the Google "
    "Play services libraries it uses (com.google.android.gms:play-services-basement 18.4.0 and play-services-tasks "
    "18.2.0). Copyright Google LLC. They are included unmodified, only as part of this app.\n\n"
    "The In-App Review and Play Core common libraries: Play Core Software Development Kit Terms of Service, "
    "https://developer.android.com/guide/playcore/license\n\n"
    "The Google Play services libraries: Android Software Development Kit License Agreement, "
    "https://developer.android.com/studio/terms")
ANDROIDX_TEXT = (
    "The Android apps include AndroidX libraries, used by the Google Play services libraries: androidx core, "
    "fragment, activity, lifecycle, savedstate, loader, viewpager, customview, collection, annotation, arch core "
    "and their dependencies. Copyright The Android Open Source Project. Licensed under the Apache License, "
    "Version 2.0:\n\n")


# The privacy policy, last in the list (Google Play wants it inside the app too, not only on the web): the same words as
# apps/android/store/privacy/privacy.html. CONTACT: the policy's e-mail (empty: that sentence is left out).
CONTACT = ""
POLICY_URL = ""
PRIVACY_TEXT = (
    "This policy covers the apps RetroStone Arcade and RetroStone Arcade Free (Android, and their Windows versions), "
    "published by 8BCraft SASU (SIREN 823 953 252, France).{contact}\n\n"
    "IN SHORT\n\n"
    "The apps collect no personal data at all. They have no ads, no analytics, no tracking, no account and no in-app "
    "purchases. They do not even ask for permission to use the internet: nothing you do in them can leave your device "
    "through the apps.\n\n"
    "WHAT STAYS ON YOUR DEVICE\n\n"
    "To let you pick up a game where you left it, the apps keep, in their private storage on your device: each game's "
    "best scores and medals; a saved state of each game you left; a few counters used to decide when to suggest rating "
    "the app. Nobody else can read these files, and we never receive them. They are deleted when you uninstall the "
    "app, or when you clear its data (Android: Settings > Apps > the app > Storage > Clear data). If you have turned "
    "on Android's own backup, Android may copy these files to your Google account so they can be restored on a new "
    "phone; that copy is handled by Google under its own privacy policy, and we have no access to it.\n\n"
    "PERMISSIONS\n\n"
    "Vibration: only to make a connected gamepad rumble. The touch screen and the Bluetooth and USB gamepads are read "
    "on the device to play; nothing is recorded.\n\n"
    "RATING THE APP\n\n"
    "From time to time, when you are back in the menu after playing, the Android apps may ask Google Play to show its "
    "rating window. That window is displayed by the Google Play app: if you rate or review the app, this happens in "
    "Google Play, under Google's privacy policy. The app never sees whether you rated it, nor what you wrote.\n\n"
    "LINKS TO GOOGLE PLAY\n\n"
    "RetroStone Arcade Free shows the other games of the full version in its menu; its \"Get it on Google Play\" "
    "button opens the Google Play page of RetroStone Arcade. The apps contain no other links, no ads and no "
    "third-party code that collects data.\n\n"
    "CHILDREN\n\n"
    "The games are made for all ages, children included. As the apps collect no data from anyone, they collect none "
    "from children either.\n\n"
    "CHANGES\n\n"
    "If this policy ever changes, the new version will be published with the app and on the web{url}, with its date. "
    "An app that starts collecting data would say so there first, and in its Google Play listing.")


def privacy():
    return [("PRIVACY POLICY", "RSB_ROLE_NONE", "NO DATA", "Privacy policy (effective 7 October 2026)",
             PRIVACY_TEXT.format(contact=f" Contact: {CONTACT}." if CONTACT else "",
                                 url=f" ({POLICY_URL})" if POLICY_URL else ""))]


def android_credits():
    return [
        ("PLAY REVIEW", "RSB_ROLE_NONE", "GOOGLE TERMS",
         f"Google Play In-App Review {PLAY_REVIEW} and Google Play services (Google's terms)", PLAY_TEXT),
        ("ANDROIDX", "RSB_ROLE_NONE", "APACHE 2.0", "AndroidX libraries (Apache License 2.0)",
         ANDROIDX_TEXT + read("apps/android/LICENSE-Apache-2.0.txt")),
    ]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    c = ["/* Generated by apps/launcher/tools/gen_credits.py: do not edit. */", '#include "rsb.h"', "",
         f"const char rsb_credits_owner[] = {c_str(OWNER)};", "const rsb_credit rsb_credits[] = {"]
    items = credits() + android_credits() + privacy()       # the Android ones: only in the Android build
    first_android, end_android = len(credits()), len(credits()) + len(android_credits())
    for i, (name, role, lic, title, text) in enumerate(items):
        if i == first_android:
            c.append("#ifdef __ANDROID__")
        if i == end_android:
            c.append("#endif")
        body = paragraphs(text, stanzas=name.startswith("STB"), headings=name.startswith(("UNIFONT", "PRIVACY")))
        c.append(f"    {{{c_str(name.strip())}, {role}, {c_str(lic)}, {c_str(title)},\n        {c_str(body)}}},")
    c += ["};", "const int rsb_ncredits = sizeof rsb_credits / sizeof rsb_credits[0];", ""]
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, "credits.c")
    text = "\n".join(c)
    if not os.path.exists(path) or open(path, encoding="utf-8").read() != text:
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
    print(f"{path}: {len(items)} credits, {sum(len(t) for *_, t in items)} characters of licence text")


if __name__ == "__main__":
    main()
