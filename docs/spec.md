# RetroStone VC: the virtual console specification

RetroStone VC is a fantasy console with the feel of a Super Nintendo (and a bit of Game Boy Advance). Its
games run as libretro cores on RetroStoneOS (and RetroArch), and as desktop programs on Linux and Windows.
The SDK in `sdk/` is the reference implementation (MIT licence).

Everything below is a **guideline**, not a hard limit. A game may break a guideline on purpose (a special
effect, a big boss). The runtime has a **strict mode** (on in debug builds, `make DEBUG=1`, or `--opt strict=1`)
that logs **one warning per kind of excess** (`rs_warn()`): sprites per line, OAM count, VRAM, voices,
sample memory, asset bank, cartridge size, sprite size and map size. The hard limits are the array sizes of
the implementation and are listed with each item.

Each section ends with **SNES mapping**: what a later port to real hardware would have to change.

## 1. Screen and timing
| | Guideline | Hard limit |
|---|---|---|
| Resolution | 320x240 | same |
| Frame rate | 60 fps, fixed timestep | same |
| Output | RGB565 framebuffer | same |

- The game's `update()` runs exactly 60 times per second and `draw()` once per frame after it. Frontends
  keep the pace: libretro reports 60 fps; the desktop runner uses a fixed-step accumulator with vsync.
- 320x240 is exactly 2x on the RetroStone2's 640x480 4:3 LCD, so pixels stay square and sharp.

**SNES mapping.** The SNES shows 256x224 (NTSC). A port keeps 60 Hz but loses 64 columns and 16 lines: a
Bomber Mole playfield of 20x14 cells becomes 16x14 visible cells with horizontal scrolling, and the 16-px HUD
fits in the 224 lines.

## 2. Colour
- Master palette: **RGB555** (32768 colours), stored in the SNES CGRAM format `0bbbbbgggggrrrrr`.
- **CGRAM: 256 entries** = 8 BG palettes (entries 0-127) + 8 sprite palettes (128-255) of 16 colours each.
- Tiles are **4 bpp**: colour 0 of every palette is transparent. CGRAM[0] is the backdrop colour.
- Master brightness 0-15 (`rs_brightness`), applied last, for fades.

**SNES mapping.** Identical: same colour format, same CGRAM size and split, same 4-bpp palette rule. The
SNES backdrop is also CGRAM[0]; master brightness is INIDISP.

## 3. Video memory and tiles
| | Guideline | Hard limit |
|---|---|---|
| VRAM (tiles + maps) | **64 KiB** = 2048 4-bpp tiles if nothing else | 4096 tiles of 8x8 + 4 maps of 128x128 |
| Assets per bank | **512 KiB** | none |
| Cartridge (all assets) | **8 MiB** | none |

- One shared tile memory of 8x8 tiles, like SNES VRAM. BG layers and sprites address it through a base
  ("character base"), so a map entry holds a 10-bit tile number relative to its layer's base.
- Tiles are uploaded packed (4 bpp, 32 bytes, high nibble = left pixel) or as one byte per pixel.
- VRAM accounting counts the distinct tiles written (32 bytes each) plus the maps of the enabled layers.

**SNES mapping.** 64 KiB is exactly SNES VRAM. SNES 4-bpp tiles are bitplane-interleaved; the asset tool
would write that format instead of packed nibbles. Bank: a LoROM bank is 32 KiB and a HiROM bank 64 KiB; the
512 KiB "bank" guideline is a loading unit (one season's art, one music set) that maps to several ROM banks.
Cartridges: 4 MiB for plain LoROM/HiROM, 6-8 MiB with ExHiROM.

## 4. Background layers
- **4 tile layers** (BG1-BG4) of 8x8 tiles; maps of 32, 64 or 128 tiles per side (128x128 max).
- Map entry = **SNES format**: `vhopppcc cccccccc` (tile 10 bits, palette 3 bits, priority 1 bit, h/v flip).
- 16x16 **metatiles** as a convenience (`rs_bg_meta`): 4 map entries.
- Per-layer scroll, wrap-around maps.
- **Per-scanline scroll** tables (240 entries, dx and/or dy, `rs_bg_line_scroll`): HDMA-like raster effects
  (waves, parallax bands, heat haze).
- **Raster callback** (`rs_raster`): called before each visible line; it may change any register (scroll,
  windows, palette entries, affine matrix, layer enable), exactly like HDMA/H-IRQ code.
- **Affine layer** (one at a time, `rs_bg_affine`): Mode-7-like rotation and scaling with an 8.8 matrix and a
  centre, wrap or transparent outside; per-line matrix changes give perspective floors.

### Priorities (back to front)
The sprite priority (0-3) and each map entry's priority bit place the pixels in this order (SNES Mode 0/1):

`backdrop, BG4 low, BG3 low, OBJ 0, BG4 high, BG3 high, OBJ 1, BG2 low, BG1 low, OBJ 2, BG2 high, BG1 high, OBJ 3`

Between sprites, the lower OAM index is in front, whatever the priorities (as on the SNES).

**SNES mapping.** Mode 0 has 4 layers but only 2 bpp (4 colours) each; Mode 1 has two 4-bpp layers and one
2-bpp layer. Four 4-bpp layers is the one deliberate step beyond the SNES: a port puts the HUD and weather on
2-bpp layers (Mode 1 BG3 with its high-priority bit) and merges the rest. Maps: the SNES tops out at 64x64
tiles per layer, so bigger maps need streaming. The map entry format, 16x16 tile mode, scroll registers, HDMA
and H-IRQ map directly. The affine layer is Mode 7, which on the SNES is 8 bpp (256 colours), 128x128 tiles of
8x8 in a fixed map, and replaces all other layers; a port converts the affine layer's art to Mode 7 and keeps
the other layers off while it shows.

## 5. Windows and colour math
- **Two windows**, each a horizontal span `[left, right)` that the raster callback may change per line
  (a circle = an iris transition; a moving circle = a lamp in the dark).
- Each layer, and the sprites, can be hidden inside or outside window 1 and/or 2; `rs_clip_black` blacks
  out the final picture inside or outside the windows.
- **Colour math** on chosen layers (BG1-4, sprites with palettes 4-7, backdrop): add or subtract, optionally
  halved, with the pixel already drawn below (painter order) or with a fixed colour. Used for translucent
  weather, shadows, flashes and fades.

**SNES mapping.** Two windows with per-layer masks and the colour window are the SNES registers W12SEL..WOBJLOG
and CGWSEL. Colour math add/sub/half with a fixed colour is CGADSUB/COLDATA. "Blend with the pixel below"
maps to the sub screen: a port puts the layers under a translucent layer on the sub screen (TS register).
One difference: our painter blends each translucent layer with what is under it, where the SNES blends only
the front-most main-screen pixel with the sub screen.

## 6. Sprites
| | Guideline | Hard limit |
|---|---|---|
| Sprites (OAM) | **128** | 256 |
| Size | 8x8 to 64x64, multiples of 8 | any multiple of 8, up to 248 |
| Per scanline | **32** | none |
| Palettes | 8 (CGRAM 128-255) | same |

- Each sprite: x, y (signed), tile, width, height, palette, priority 0-3, H/V flip, hide.
- A WxH sprite uses (W/8)x(H/8) **consecutive** tiles (row-major) from the sprite base.
- Colour math on sprites only applies to palettes 4-7, as on the SNES.

**SNES mapping.** 128 sprites, 32 per line (and 34 8-pixel slivers per line), priorities, palettes and the
math rule are SNES OAM behaviour. The SNES allows only **two sizes at once** (OBSEL: 8/16, 8/32, 16/32,
16/64, 32/64...), all square: a port picks a pair (16/32 for Bomber Mole) and builds other sizes from
several sprites. The SNES lays multi-tile sprites out in a 16-tile-wide grid in VRAM, not consecutively; the
asset tool would reorder.

## 7. Audio
| | Guideline | Hard limit |
|---|---|---|
| Voices | **8** (music channels + sound effects) | 16 |
| Output | 32 kHz stereo, 16-bit | same |
| Samples | 16-bit PCM or 4-bit IMA ADPCM | 64 slots |
| Sample memory | **64 KiB counted as 4-bit ADPCM** (131072 samples) | RAM |

- Each voice: sample (looping or not), pitch (`0x1000` = the sample's rate, linear interpolation),
  volume, pan, **ADSR** envelope, echo send.
- One global **echo**: delay up to 240 ms, feedback, volume, with a low-pass in the loop.
- **Music**: tracker modules (MOD, S3M, XM, IT) played by **libxmp-lite 4.7.3** (MIT, vendored, about 700 KB of
  source, integer mixer). A module's channels count toward the 8-voice guideline: a 4-channel MOD leaves 4
  voices for sound effects.
- Sound effects: `rs_sfx()` takes a free voice or steals the oldest.

**SNES mapping.** The S-DSP has exactly 8 voices at 32 kHz with ADSR, per-voice echo send, one echo unit with
an 8-tap FIR, pitch in the same 4.12 format (`0x1000` = 1.0) and 64 KiB of audio RAM. SNES samples are BRR
(4-bit ADPCM, 9 bytes per 16 samples), so the guideline counts sample memory at 4 bits per sample. Music: a
port converts the modules for an SPC700 sound driver (IT/XM converters exist); keep modules within 8 channels
minus the sound-effect voices, and the samples within ARAM.

## 8. Input
- Up to **4 pads**, SNES layout: D-pad, A B X Y, L R, Start Select.
- `rs_pad(port)` returns the held buttons in the **SNES JOY1 bit order**
  (`B Y Select Start Up Down Left Right A X L R`); `rs_pad_pressed`/`released` give the edges.
- Desktop: keyboard on pad 1, game controllers by position (bottom = B, right = A, left = Y, top = X).

**SNES mapping.** Identical bit layout (JOY1-JOY4 with a multitap).

## 9. Save
- **32 KiB of battery RAM** (`rs_sram()`), exposed as libretro `SAVE_RAM`; the desktop runner saves it as
  `<game>.srm`. The game calls `rs_sram_commit()` after a change.

**SNES mapping.** Battery SRAM of 2-32 KiB (32 KiB = 256 kbit) on the cartridge.

## 10. Runtime services
- Deterministic RNG (xorshift32), a global one seeded at start, and private ones.
- Text: a built-in 5x7 font (96 ASCII glyphs) uploaded as tiles (ink = colour 1, shadow = colour 2), plus a
  2x font (4 tiles per glyph) for titles.
- Assets: a game embeds its data (`rs_game.assets`); `rs_asset(name)` first looks in the frontend's data
  directory (desktop: `data/` next to the executable), so levels and music can be replaced without rebuilding.
- Options: `--opt key=value` on the desktop runners (`rs_option()`).

## 11. Performance
The renderer is a scanline "PPU": each line decodes the enabled layers into byte lines (SWAR, 8 pixels per
map entry), rasterises the sprites of the line, then paints the planes back to front straight into RGB565.
No floating point in any per-pixel path. Target: **60 fps on one 1 GHz Cortex-A7 core** (the RetroStone2's
Allwinner A20) with 4 layers and 128 sprites.

Measured on the build host (WSL2, x86-64) with `make bench`:

| Scene | Host, per frame | A20 estimate (x15-x20) |
|---|---|---|
| PPU stress: 4 layers 64x64 (2 with line scroll, colour math) + 128 sprites of 32x32 | 0.78 ms | 11.7-15.6 ms |
| Bomber Mole spring 5, explosion chain (heaviest game scene), average | 0.42 ms | 6.3-8.4 ms |
| Bomber Mole spring 5, worst frame | 0.86 ms | 12.9-17.2 ms |

The frame budget is 16.7 ms. Audio (libxmp + 8 voices) costs about 0.01 ms per frame on the host.
