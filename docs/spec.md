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
| Resolution | 320x240 (widescreen: 320..560 x 240) | same |
| Frame rate | 60 fps, fixed timestep | same |
| Output | RGB565 framebuffer | same |

- The game's `update()` runs exactly 60 times per second and `draw()` once per frame after it. Frontends
  keep the pace: libretro reports 60 fps; the desktop runner uses a fixed-step accumulator with vsync.
- 320x240 is exactly 2x on the RetroStone2's 640x480 4:3 LCD, so pixels stay square and sharp.

### Widescreen
A guideline extension for 16:9 screens and phones. **The height stays 240; the width is flexible, from 320 to 560
columns, chosen by the frontend at start-up**: 320 = 4:3 (the default: the RetroStone2, RetroStoneOS), 427 = 16:9,
520 = 19.5:9 and 533 = 20:9 phones. Even widths are recommended (a 320-px stage then centres exactly); odd ones work.
**A wider screen shows more of the decor; the gameplay is unchanged.**

- **Opting in.** A game sets `rs_game.max_width` (usually `RS_SCREEN_W_MAX`) and draws whatever `rs_screen_width()`
  says; its `game.mk` has `WIDESCREEN_<game> = 1`, which runs its checks at 320 and at every width of `WIDTHS`
  (427 and 520). A game that does not opt in always runs at 320: wide frontends pillarbox it.
- **The API.** `rs_screen_width()` (fixed for a run, set before `init()`), `rs_screen_margin()` = (width - 320) / 2.
  `rs_screen_origin(x)` sets the **stage origin**, the screen column of stage x 0: sprites, viewports, windows and
  the BG scroll are in stage coordinates. Origin 0 (the default) suits a side-scroller, which reveals more of its
  world on the right; `rs_screen_origin(rs_screen_margin())` centres a game designed for 320 columns, and the screen
  shows stage x `rs_screen_left()` .. `rs_screen_right()` (-margin .. 320 + margin): the decor layers draw wider, the
  320 columns of the stage stay the same pixels. Everything else follows the width: the scanline renderer and its
  line buffers, BG layers (a map wider than the screen avoids visible repeats: 64 tiles up to 512 columns, 128
  beyond), the raster callback (its lines are unchanged, `rs_screen_width()` gives the columns), the affine layer,
  sprite clipping (OAM x is signed: -64..width), the standard viewport layouts (they split the whole width) and the
  PNG dumps. The house UI kit (games/common) centres the title, panels, banners, prompt, results and pause on the
  screen and puts the score chips on its halves and corners.
- **Frontends.** SDL2: the width follows the window's shape (the first window has the desktop's aspect; resizing it
  restarts the game at the new width from a save state, where it was); `--width N`, `--aspect 4:3|16:9|wide` or the
  environment variable `RS_WIDTH=N` fix it (`--aspect 4:3` is the classic 320x240). libretro: the core option
  "Aspect" (`rsvc_aspect`: 4:3, 16:9, Wide), 4:3 by default so RetroStoneOS is unaffected; the geometry follows it
  and a change restarts the game from a save state. Headless and the state tests: `--width N` (or `RS_WIDTH`).
  A host embedding the runtime (an Android app) calls `rs_host_set_width(rs_host_width_for_aspect(w, h))` with the
  device's landscape size before `rs_host_init()`, reads `rs_screen_width()` (a game that has not opted in stays at
  320) and shows the framebuffer, `rs_screen_width()` x 240 with that pitch, at the screen's height;
  `rs_host_change_width()` switches live.
- **Save states load at any width.** The width is a setting of the frontend, not part of a state (the format is
  unchanged): a state saved at 427 loads at 320 or 520. A widescreen game keeps its saved objects independent of the
  width, or rebuilds what depends on it in `state_loaded` (maps drawn for the width, a course generated up to the
  screen's edge); the next frame is drawn at the new width.
- **Strict mode.** The sprites-per-line guideline scales with the width: 32 per 320 columns (42 at 427, 52 at 520).
  VRAM: on a screen wider than 320, the map columns past 64 tiles (the 128-tile maps a wide screen needs) are not
  counted, so a game that fits the 64 KiB at 4:3 still fits; the other guidelines are unchanged.
- **Performance.** The cost grows with the columns: the PPU stress scene costs 0.79 ms at 320, 0.92 ms at 427 and
  1.01 ms at 520 on the build host (x1.28 for x1.63 columns); see section 11 for the games.

**SNES mapping.** None: the SNES is 4:3. A port runs a widescreen game at 320 (256) columns, its 4:3 stage.

### Tall screen
The same idea for a phone held upright (portrait). **The width stays 320; the height is flexible, from 240 to 720
lines (`RS_SCREEN_H_MAX`), chosen by the frontend**: 692 on a 1080x2340 phone. A game that does not opt in always
runs at 240: a tall frontend shows its 4:3 picture across the width, centred, the lines above and below filled with a
dim blurred copy of its edges, and the touch buttons in the space below it (apps/launcher/src/rsb_sdl.c).

There are two ways to opt in. **The zoomed screen (v2, the one to use)**: a screen of a moderate height that fills the
display's height, its sides cropped, so the game is big. **The plain tall screen (v1)**: the display's whole height at
320 columns (a 320 x 700 strip: small, and the stage's 240 lines float in the middle); it still works, unchanged.

- **The zoomed screen.** A game calls `rs_screen_zoom(H)` first in its `init()` (a call, not a descriptor field: the
  games' descriptors stay as they are), H of about 400..520 lines. On a phone held upright it gets H lines at 320
  columns, and the frontend scales them to FILL the display's height: the sides are cropped equally (fill, not fit).
  `rs_screen_visible_x0()` .. `rs_screen_visible_x1()` - 1 are the screen columns shown (an even count, rounded down:
  the frontend may show a column more on each side): 202 at H 440 on a 1080x2340 phone (9:19.5), 188 on a 1080x2520
  one (9:21), 246 on a 16:9 one, every column on a display less tall than 320 x H (it then gets a shorter screen, as
  with `rs_screen_tall`). The taller H, the more lines and the fewer columns. Landscape, 4:3 and the frontends without
  a tall screen: 240 lines and every column, the same pixels as before.
  - **Pan**: `rs_screen_focus_x(x)` centres the display on screen column x (clamped so that the columns shown stay
    inside the screen; -1: the middle, the default). **Fill or fit**: `rs_screen_fill(0)` shows every column for a
    while (the frontend fits the width, the blurred bands above and below), `rs_screen_fill(1)` zooms again: for a
    title, a panel or a split screen that needs the whole width. Both are settings, not saved in states: set them
    each frame from the game's state; the frontend follows at once (the picture, the touches, the buttons).
  - **The HUD at the screen's top**: `rs_bg_pin(layer, y, lines, screen_y)` draws the layer's stage lines y .. y +
    lines - 1 at screen line screen_y (0: the display's top), and nothing in their old place: e.g. `rs_bg_pin(UI, 0,
    48, 0)` puts the UI layer's top 6 rows (the score, the timer bar) at the top of the screen while the stage sits
    lower. One full screen only (not in viewports); no effect on a 240-line screen; a setting like the origin, not
    saved in states. HUD sprites go at stage y `rs_screen_top()` + their y. Keep the HUD inside the visible columns,
    centred, clear of the frontend's back and pause buttons in the top corners (about 30 px of the screen each).
- **The plain tall screen.** `rs_screen_tall(RS_SCREEN_H_MAX)` first in `init()`: the display's height at 320 columns,
  every column shown (the frontend fits the width).
- **The API, both ways.** `rs_screen_height()` (fixed for a run), `rs_screen_vmargin()` = (height - 240) / 2.
  `rs_screen_origin_v(y)` sets the stage's vertical origin, the screen line of stage y 0; **by default the stage is
  centred** (origin = vmargin). The screen shows stage y `rs_screen_top()` .. `rs_screen_bottom()` - 1. Sprites,
  viewports (and the standard layouts) and the BG scroll are in stage coordinates, and so are the raster callback's
  lines: they run from `rs_screen_top()` (negative) to `rs_screen_bottom()` - 1, so a raster that indexes a 240-entry
  table must clamp the line. The line-scroll tables keep 240 entries (stage lines): the lines past the stage get no
  line scroll. Touches are in screen pixels (y 0 .. height - 1; the stage y is y - `rs_screen_origin_y()`), wherever
  the frontend puts the picture.
- **Frontends.** The RetroStone app's Arcade tab and the Arcade app (rsb_sdl.c) ask for the height of the display at
  320 columns, uncapped (`rs_host_set_height()` before `rs_host_init()`: 746 on a 9:21 phone, the zoomed screen's
  columns come from it; `rs_host_change_size()` when the phone turns: the game restarts from a save state, like a
  width change, and also when a zoomed game's visible columns change). A zoomed picture: the display's height, centred
  on the visible columns; the touch buttons (games.cfg button=) in the band below the stage, or over the bottom of the
  picture (at least 45% of the display's width tall); back and pause in the top corners, as always. The other
  frontends never ask: 240. The headless runner: `--display 1080x2340` (as the apps size the screen; `--height N`,
  `RS_HEIGHT`), `--visible` (the PNG holds the visible columns only), PNGs of the whole height.
- **Save states load at any height**, as at any width (the height is not part of a state).

**The zoomed recipe** (games/beaverrush is the example: DESIGN.md "Tall screen").
1. **Choose H** so the play is big and fits the columns shown: H 440 shows 188 to 202 columns on today's phones;
   check what the play needs (Beaver Rush: the trunk, its branches and the beaver, 140 columns).
2. **Keep the play and the HUD inside the visible columns** (read `rs_screen_visible_x0()` / `x1()` each frame); what
   is outside is decor that may or may not be seen. A title or a panel wider than that: `rs_screen_fill(0)` while it
   shows. Split screens: `rs_screen_fill(0)`, or lay the views out in the visible columns.
3. **The HUD at the screen's top**: `rs_bg_pin` for a BG layer's HUD rows, `rs_screen_top()` for HUD sprites (the
   house kit's sprites are drawn anywhere on the screen).
4. **Place the stage**: `rs_screen_origin_v()`, usually low (the ground near the bottom, a little of it below the
   stage), the extra lines above it showing more of the play when the play can grow there (Beaver Rush: the whole
   trunk kept, its branches up to the screen's top from the start), else more decor.
5. **Decor fills the whole visible area, without colour seams**: the raster's colours carried on from the stage's
   first and last lines (sky, earth), a BG layer's map taller than 240 lines; layers that would repeat or wrap there
   hidden (windows), put back at line 0.
6. **Things that come and go appear at the visible edges, not the stage's**: weather falls from `rs_screen_top()`
   across the visible columns, the sun and moon inside them, a sprite culled at `rs_screen_top()` / `bottom()`, not
   at 0 / 240.
7. **Check at 1080x2340 and 1080x2520** (`<game>_headless --display 1080x2340 --visible --png out.png`), the title,
   play and the panels; and landscape (2340x1080, `--width 520`) and 4:3 stay the same pixels.
8. **The house UI, the same on every game's screen** (the 8BCraft games: `games/common/src/house_tall.h`, `ht_*`).
   On a tall screen the title, the banners, the game over, the results and the pause use only the standard calls
   and rows: the logo on row `HT_ROW_LOGO` (two words always on two lines, each 4x, 3x when too wide for the
   columns shown), the slots on `HT_ROW_SLOTS`, then the hint, the prompt, BEST, the join line and the credits
   bottom-aligned on row 29; the GAME OVER banner on row 4 and its panel from row 8 or 9; the ranking's banner on
   row 3. Each game places its stage at its own height, so these rows are **anchored on the screen, not the
   stage**: while the title or a panel shows, the game pins its whole UI layer (`ht_pin(HT_PIN_TITLE)` /
   `ht_pin(HT_PIN_PANEL)`: `rs_bg_pin` of its 240 lines) so that the logo's top lands at `HT_LOGO_AT` % (20) of the
   screen's height and the banner's at `HT_PANEL_AT` % (24), and its raster shows the UI layer on those lines
   (`ht_pin_line`) instead of the stage's. The helpers' sprites (the glyph, the slots, the digits, the medals, the
   icon positions) follow by themselves (`ht_pin_dy`); a sprite a game draws at a UI position adds `ht_pin_dy`. The
   scene stays where the game draws it; a title mascot goes between the slots and the text under them
   (`ht_title_out` `gap_y0` .. `gap_y1`) or, above the logo, moves with it. A game with a UI layer of 64 rows (nothing
   to hide past its 30) may scroll it down by `ht_pin_dy` instead (its viewports too). Landscape and 4:3: no pin.

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

### Viewports (split screen)
- Up to **12 viewports** (`rs_viewports`), each a screen rectangle that draws the BG layers of its mask with
  **its own scroll** (the layer pixel at its top-left corner) and **its own sprites** (a range of OAM, with
  coordinates relative to the viewport), clipped to the rectangle. Later viewports are drawn over earlier ones
  (an overlay box is a viewport); pixels no viewport covers show a **divider colour**.
- The raster callback runs for each line **and each viewport** crossing it (`rs_viewport_current()`), so
  windows, palette changes and math can differ per viewport (a lamp circle in each player's view).
- Standard layouts (`rs_viewport_layout`): 1 player = full screen; 2 = left/right halves of 159x240 (or
  top/bottom 320x119); 3 = a top half and two bottom quarters (or 4 quadrants, the 4th free for a map);
  4 = quadrants of 159x119, with a 2-px divider.
- Cost: the same pixels as one screen, plus one 8-px tile per layer and viewport on each line, and each
  viewport scans only its own sprites. Measured: the PPU stress scene in 4 quadrants costs +4% (0.86 ms against
  0.83 ms on the host); a 4-CPU Bomber Mole battle in 4 views costs the same as on one screen (0.51 ms).
- Off (`rs_viewports(0, ...)`, the default): one full screen, the layers' global scroll, every sprite.

**SNES mapping.** Two windows with per-layer masks and the colour window are the SNES registers W12SEL..WOBJLOG
and CGWSEL. Colour math add/sub/half with a fixed colour is CGADSUB/COLDATA. Viewports have no register of
their own on the SNES: a horizontal split (top/bottom) is an HDMA scroll change at the split line (cheap); a
vertical split needs the two halves drawn into one layer each or a window per half with two layers per view,
and sprites sorted per half by the game (the OAM range already does that). "Blend with the pixel below"
maps to the sub screen: a port puts the layers under a translucent layer on the sub screen (TS register).
One difference: our painter blends each translucent layer with what is under it, where the SNES blends only
the front-most main-screen pixel with the sub screen.

## 6. Sprites
| | Guideline | Hard limit |
|---|---|---|
| Sprites (OAM) | **128** | 256 |
| Size | 8x8 to 64x64, multiples of 8 | any multiple of 8, up to 248 |
| Per scanline | **32** (widescreen: 32 per 320 columns) | none |
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
- Desktop: keyboard pad 1 = arrows, Z/X = B/A, C/V = Y/X, Q/E = L/R, Enter = Start, Right Shift/Backspace/Esc =
  Select; keyboard pad 2 = W A S D, G/H = B/A, T = Start, R = Select; game controllers by position (bottom = B,
  right = A, left = Y, top = X), on pads 1-4 in the order they are plugged in.
- `rs_pad_device(port)` tells what drives a port, so a game can name its buttons: `RS_DEVICE_PAD`,
  `RS_DEVICE_KEYBOARD` (the first key set: "A (X KEY)", "START (ENTER)") or `RS_DEVICE_KEYBOARD2` (the second:
  H, G, T). The desktop runner reports the last one used on pads 1-2 (the keyboard until a controller is
  used); the libretro core always reports a pad; the headless runner takes `--device P1=pad|kb|kb2`
  (default: pad 1 the keyboard, pad 2 the second key set, pads 3-4 pads).
- libretro: ports 1-4 are pads 1-4 (a multitap or four controllers in RetroArch / RetroStoneOS).

**SNES mapping.** Identical bit layout (JOY1-JOY4 with a multitap).

### Touch
An extension for phones (the RetroStone Arcade apps) and the desktop's mouse. **Touch is player 1**; players 2-4 are
pads. A game that does not read it is unchanged, and a game that reads it keeps its pad controls.

- **Up to 4 fingers** (`RS_TOUCH_MAX`), each in a slot it keeps while it is down: `rs_touch(i)` (NULL when the slot
  is free), `rs_touch_count()`, `rs_touch_first()` (the finger down the longest). A point (`rs_touch_point`) has its
  position `x, y` in **screen pixels of the current width** (0..`rs_screen_width()`-1 x 0..239; the stage x is
  x - `rs_screen_origin_x()`), where it went down (`x0, y0`), the frames since, and its edges: `down`, `pressed`
  (went down this frame), `released` (went up this frame; x, y = where it was last).
- **Gestures**, worked out by the runtime at the start of each frame: a **tap** (`tap`, `rs_touch_tap()`): released
  within `RS_TOUCH_TAP_FRAMES` (18, 0.3 s) without ever going further than `RS_TOUCH_SLOP` (8 px) from where it went
  down (then `moved` is set); a **swipe** (`swipe` on the frame it is recognised, once per touch, `swiped` after,
  `rs_touch_swipe()`): `RS_TOUCH_SWIPE_DIST` (20 px) from where the finger was at most `RS_TOUCH_SWIPE_FRAMES` (24)
  frames before (a flick, also by a finger that has been down for a while), mostly along one axis (the other at most
  half as far): `RS_SWIPE_UP`, `DOWN`, `LEFT`, `RIGHT`; a **hold**
  (`rs_touch_hold()`): down `RS_TOUCH_HOLD_FRAMES` (19) or more without moving; the **screen halves**:
  `rs_touch_half(x)`, `rs_touch_halves()` (the halves with a finger down, `RS_TOUCH_LEFT | RS_TOUCH_RIGHT`).
- **Declaring it.** `rs_game.touch_native = 1`: the game reads the touches itself for player 1, **its menus
  included** (a tap starts the title, goes on after a game over...). The apps (apps/launcher) then pass it the
  touches, and no longer turn them into pad buttons (games.cfg's zones and drawn buttons are for the other games);
  the app's back button (top left) and pause button (top right, which presses the game's pause input) keep the
  fingers that go down on them.
- **Frontends.** The host gives the fingers down before each frame: `rs_host_set_touches(n, {id, x, y})`, in screen
  pixels (clamped). SDL2: a touch screen's fingers, and the **mouse's left button as one finger** (test on the
  desktop). The apps: the phone's touches (and the mouse on the desktop) for a touch-native game. Headless:
  `touch`, `move`, `up`, `tap-at`, `swipe` lines in the input script (fingers T1-T4; coordinates in pixels, `N%`, or
  `@N` = stage coordinate). libretro: `RETRO_DEVICE_POINTER` on port 1 when the frontend has one (a touch screen,
  RetroArch's mouse); without one the game is played with the pad.
- **Save states** do not hold touches (the format is unchanged): a load forgets the fingers down, and one still down
  is seen as a new touch. A game keeps what it derives from touches (a target) in its own registered state.

**SNES mapping.** None (the SNES Mouse aside): a port is played with the pad.

## 9. Save
- **32 KiB of battery RAM** (`rs_sram()`), exposed as libretro `SAVE_RAM`; the desktop runner saves it as
  `<game>.srm`. The game calls `rs_sram_commit()` after a change.

**SNES mapping.** Battery SRAM of 2-32 KiB (32 KiB = 256 kbit) on the cartridge.

### Save states
A state is the whole console at a frame boundary, like an emulator's: the runtime saves its own "hardware"
(VRAM as packed 4bpp, the four maps in full, CGRAM, OAM, every PPU register, the viewports, the raster callback
and line-scroll tables, the voices with their envelopes and positions, the echo buffer, the music, the text
set-up, the frame counter, the global RNG, the pads and their previous state for the edges), and the game
registers **its** objects from the `rs_game.state` callback:

| Call | |
|---|---|
| `rs_state_var(name, ptr, size)`, `RS_STATE(v)` | an object saved and restored byte for byte |
| `rs_state_ptr(name, &p)`, `RS_STATE_PTR(p)` | a pointer (alone, or inside a saved object): saved as (object, offset) |
| `rs_state_ref(name, ptr, size)` | a constant table that saved pointers may point into (not saved itself) |
| `rs_state_raster(name, fn)` | a raster callback the game installs (saved by number) |

Rules:
- `state` is called once, possibly before `init()` (a libretro frontend asks for the size first): register
  static objects only. The size of a state is fixed for a build (libretro requires it): about 350 KB.
- No raw pointers: a pointer the game keeps, or gives the runtime (raster user data, line-scroll tables), must
  point into a registered object or reference, else saving fails and logs which one. Convert pointers into
  constant tables to indices, or register the table with `rs_state_ref`.
- Samples are not saved (a voice names its slot): load them in `init()`, the same at every start; a state
  whose voices' samples differ is refused. The music is saved as the asset's name, its order position and row:
  a load restarts the same module there (libxmp's voices are not restored: the notes held at the save start
  again on the next row).
- `rs_game.state_loaded` (may be NULL) runs after a load, to rebuild caches (Leady Squid's bot clears its
  memo there).
- **Save RAM is not in states.** Keep progress (unlocks, best times) in `rs_sram()` and do not register its
  in-memory copy: loading an old state never takes progress back.
- Name objects `<file>.<name>` (the source file's stem). `tools/state_audit.py` (in `make check`) lists every
  writable static of the game's object files and fails when one is neither registered nor listed with a reason
  in `games/<game>/state_audit.txt` (scratch buffers, caches rebuilt the same).
- Bump `rs_game.state_version` when the meaning of saved data changes without its layout changing.
- The screen width is not part of a state (section 1, "Widescreen"): a state loads at any width.

**Format** (little-endian). A 64-byte header: `"RSVC"`, format version (u16, 1), header size (u16, 64), game id
(20 bytes), game version (16 bytes), the game's `state_version` (u32), the **build hash** (u64: format, pointer
size, endianness, id, version, every registered name and size, and the asset pack's contents), the payload size
(u32) and an FNV-1a checksum of the payload (u32). Then tagged sections, always in this order, each a 4-char
tag, a u32 size and the payload: `CORE`, `PPU `, `APU `, `MUS `, `TEXT`, `GAME` (the registered objects in
order, with the pointers inside them zeroed), `PTRS` (each registered pointer as object number + offset),
`END `. The rest of the fixed-size buffer is zero. A load checks the header, the checksum and every section
(sizes, map sizes, voice positions, pointers, the music asset, the samples) before it changes anything: a
state of another game, another version or build, truncated or corrupted is refused and the game goes on
untouched. libretro: `retro_serialize_size` / `retro_serialize` / `retro_unserialize`; a state loaded before
the first frame (resume at start-up) starts the game first; a save before the first frame returns false; the
core declares the endian- and platform-dependent serialization quirks (the game's objects are raw).

Tests: `sdk/tests/test_states.c` runs a game to a point, saves, plays on while hashing the state and the
picture of every frame, loads, replays the same frames and compares; a fresh process loads the state (and the
battery save of that moment) before its first frame and must match too; bad states (truncated, another game,
version or build, a corrupted byte, invalid sections, fuzzed sections) must be refused with nothing changed.
`games/<game>/tests/state_test.sh` runs it at many points of each game; `make SAN=1 check` runs everything
under ASan and UBSan.

**SNES mapping.** None: states are a frontend feature (a port keeps the battery save only).

## 10. Runtime services
- Deterministic RNG (xorshift32), a global one seeded at start, and private ones.
- Text: a built-in 5x7 font (96 ASCII glyphs) uploaded as tiles (ink = colour 1, shadow = colour 2), plus a
  2x font (4 tiles per glyph) for titles.
- Languages (docs/i18n.md): the games show their texts in the 22 languages of RetroStoneOS, picked from the system
  (libretro: the core option Language, RetroStoneOS's `rsos_language`, `GET_LANGUAGE`; SDL's preferred locales;
  `--lang`). Strings are UTF-8 tables compiled into the game (`tools/i18n_build.py`); the house font draws accented
  Latin, Cyrillic and Greek capitals, and Japanese, Chinese and Korean use 16x16 Unifont glyphs, a per-game subset.
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
| Bomber Mole summer: a corn field burning, 2 bee swarms, 2 harvesters (test arena), average | 0.41 ms | 6.1-8.2 ms |
| Bomber Mole summer, same scene, worst frame | 0.77 ms | 11.6-15.4 ms |
| PPU stress in 4 viewports (quadrants, 32 sprites each) | 0.86 ms | 12.9-17.2 ms |
| Bomber Mole battle, 4 CPUs in 4 viewports (Mine Cart Mayhem), average | 0.51 ms | 7.6-10.2 ms |
| Bomber Mole battle, same, worst frame | 0.96 ms | 14.4-19.3 ms |

The frame budget is 16.7 ms. Audio (libxmp + 8 voices) costs about 0.01 ms per frame on the host.
