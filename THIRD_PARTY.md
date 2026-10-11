# Third-party code

All third-party code is permissive and vendored under `sdk/third_party/`.

| Component | Version | Licence | Where | Used for |
|---|---|---|---|---|
| libxmp-lite (Claudio Matsuoka, Hipolito Carraro Jr and contributors) | 4.7.3 | MIT | `sdk/third_party/libxmp-lite/` (licence text in its `README`) | tracker music (MOD, S3M, XM, IT) |
| stb_image, stb_image_write (Sean Barrett) | stb_image 2.30, stb_image_write 1.16 | MIT or public domain (dual) | `sdk/third_party/stb/` (licence at the end of each file) | PNG reading (tests) and writing (screenshots) |
| stb_truetype (Sean Barrett) | 1.26 | MIT or public domain (dual) | `sdk/third_party/stb/stb_truetype.h` (licence at the end) | the board games' texts (apps/boardgames) |
| Noto Sans CJK (Adobe, Google) | 2.004 | SIL Open Font License 1.1 | not vendored: the system package `fonts-noto-cjk`, cut down at build time to the characters of the texts (`apps/boardgames/tools/gen_text.py`); licence: `apps/boardgames/fonts/LICENSE-NotoSansCJK-OFL.txt` | the board games' texts in 22 languages |
| libretro.h (The RetroArch team) | API v1 | MIT | `sdk/third_party/libretro/libretro.h` | libretro core API |
| SDL2 (Sam Lantinga and contributors) | 2.32.10 (Windows), system package (Linux) | zlib | not vendored: `tools/fetch_sdl2_mingw.sh` downloads the official mingw development package; linked statically into the Windows .exe | window, input, audio of the desktop runner |

The SDL2 licence (zlib): "This software is provided 'as-is', without any express or implied
warranty. [...] Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely", subject to not
misrepresenting its origin and keeping its notice in source distributions.

## Fonts

| Font | Version | Licence | Where | Used for |
|---|---|---|---|---|
| GNU Unifont (Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller, Johnnie Weaver, David Corbett, Nils Moskopp, Rebecca Bettencourt and contributors) | 13.0.06 | SIL Open Font License 1.1 (Unifont is dual-licensed GPL-2.0-or-later with the GNU font embedding exception, or OFL 1.1; it is used here under the OFL) | `games/common/fonts/unifont-subset.hex`: only the 16x16 glyphs the games' strings use (Japanese kana and kanji, Hangul, Simplified Chinese), from the copy in SDL2's source (`test/unifont-13.0.06.hex`); licence text `games/common/fonts/LICENSE-unifont-OFL.txt` | the wide characters of the translated texts (docs/i18n.md); each game embeds the subset of its own strings |

The house 5x7 font and its accented, Cyrillic and Greek capitals (`sdk/tools/gen_font.py`, `sdk/tools/gen_font_ext.py`)
are original, MIT. The OFL allows the Unifont glyphs to be embedded in the games and redistributed with them (not sold
by themselves); a modified version may not use the name "Unifont" as its own (the subset is a subset, not renamed).
