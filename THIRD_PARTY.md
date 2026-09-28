# Third-party code

All third-party code is permissive and vendored under `sdk/third_party/`.

| Component | Version | Licence | Where | Used for |
|---|---|---|---|---|
| libxmp-lite (Claudio Matsuoka, Hipolito Carraro Jr and contributors) | 4.7.3 | MIT | `sdk/third_party/libxmp-lite/` (licence text in its `README`) | tracker music (MOD, S3M, XM, IT) |
| stb_image, stb_image_write (Sean Barrett) | stb_image 2.30, stb_image_write 1.16 | MIT or public domain (dual) | `sdk/third_party/stb/` (licence at the end of each file) | PNG reading (tests) and writing (screenshots) |
| libretro.h (The RetroArch team) | API v1 | MIT | `sdk/third_party/libretro/libretro.h` | libretro core API |
| SDL2 (Sam Lantinga and contributors) | 2.32.10 (Windows), system package (Linux) | zlib | not vendored: `tools/fetch_sdl2_mingw.sh` downloads the official mingw development package; linked statically into the Windows .exe | window, input, audio of the desktop runner |

The SDL2 licence (zlib): "This software is provided 'as-is', without any express or implied
warranty. [...] Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely", subject to not
misrepresenting its origin and keeping its notice in source distributions.
