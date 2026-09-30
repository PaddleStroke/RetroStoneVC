/*
 * RetroStone VC SDK: the public game API.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft). See sdk/LICENSE.
 *
 * A game is a set of callbacks (rs_game) plus calls into this API. The
 * runtime calls update() 60 times per second and draw() once per frame, then
 * renders the picture with a scanline "PPU" and mixes the audio "APU".
 * The limits in docs/spec.md are GUIDELINES: most of them can be exceeded,
 * and strict mode (on in debug builds) logs one warning per kind of excess.
 */
#ifndef RS_H
#define RS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RS_VERSION "0.1.0"

/* ---- Screen and timing ------------------------------------------------ */
#define RS_SCREEN_W   320
#define RS_SCREEN_H   240
#define RS_FPS        60
#define RS_AUDIO_RATE 32000

/* ---- Game descriptor -------------------------------------------------- */
typedef struct rs_asset_entry {
    const char *name;           /* "levels/spring-1.txt" */
    const void *data;
    size_t size;
} rs_asset_entry;

typedef struct rs_game {
    const char *name;           /* display name: "Bomber Mole" */
    const char *id;             /* file-name safe id: "bombermole" */
    const char *version;
    void (*init)(void);         /* after the runtime is ready (sram loaded) */
    void (*update)(void);       /* fixed timestep: 60 calls per second */
    void (*draw)(void);         /* once per frame, after update() */
    void (*shutdown)(void);     /* may be NULL */
    const rs_asset_entry *assets; /* embedded asset pack, NULL-name terminated */
    /* Save states (docs/spec.md "Save states"). state == NULL: the game has none. */
    void (*state)(void);        /* registers the game's persistent state: rs_state_*() calls only */
    void (*state_loaded)(void); /* after a state was loaded: rebuild caches (may be NULL) */
    uint32_t state_version;     /* bump when the MEANING of the saved data changes (its layout is checked) */
} rs_game;

/* Implemented by the game (exactly one per binary). */
const rs_game *rs_game_main(void);

/* ---- Runtime ---------------------------------------------------------- */
void        rs_log(const char *fmt, ...);
uint32_t    rs_frame_count(void);            /* frames since start */
/* Frontend options ("--opt key=value" on desktop, core options later). */
const char *rs_option(const char *key);
int         rs_option_int(const char *key, int fallback);

/* Strict mode: warn once per kind when a guideline is exceeded. */
enum rs_warn_kind {
    RS_WARN_SPRITES_PER_LINE, RS_WARN_OAM_COUNT, RS_WARN_VRAM, RS_WARN_VOICES,
    RS_WARN_SAMPLE_MEM, RS_WARN_BANK, RS_WARN_CART, RS_WARN_SPRITE_SIZE,
    RS_WARN_MAP_SIZE, RS_WARN_KIND_COUNT
};
void rs_set_strict(int on);
int  rs_strict(void);
void rs_warn(int kind, const char *fmt, ...);   /* logs once per kind */
int  rs_warn_count(int kind);                   /* excess events seen */

/* ---- Assets ----------------------------------------------------------- */
/* Looks in the frontend's data directory first (desktop: ./data), then in
 * the game's embedded pack. Returns NULL if not found. The pointer stays
 * valid until rs_asset_free() or shutdown. */
const void *rs_asset(const char *name, size_t *size);
/* Development: forget the cached data-directory copy of an asset and look for it again (level
 * editing without a rebuild). Falls back to the embedded asset. */
const void *rs_asset_reload(const char *name, size_t *size);
/* Where an asset was loaded from: the frontend's folder for it, or "embedded". */
const char *rs_asset_origin(const char *name);

/* ---- Development aids (desktop frontends) --------------------------------- */
/* The function key pressed this frame (1..10 for F1..F10; F11/F12 belong to the frontend), 0 = none.
 * Consoles and the libretro core always return 0. */
int rs_dev_key(void);
/* Cost and sprite counts of the previous frame (host clock). */
typedef struct rs_perf_info { uint32_t update_us, render_us; int sprites, max_sprites_line; } rs_perf_info;
void rs_perf(rs_perf_info *out);

/* ---- Colour ----------------------------------------------------------- */
/* RGB555 in SNES CGRAM order: 0bbbbbgg gggrrrrr, 5 bits per channel. */
typedef uint16_t rs_color;
#define RS_RGB(r, g, b)  ((rs_color)((((b) & 31) << 10) | (((g) & 31) << 5) | ((r) & 31)))
#define RS_RGB8(r, g, b) RS_RGB((r) >> 3, (g) >> 3, (b) >> 3)
#define RS_HEX(h)        RS_RGB8(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255)

/* CGRAM: 256 entries. 0..127 = 8 BG palettes, 128..255 = 8 sprite palettes.
 * Entry 0 of each 16-colour palette is transparent; CGRAM[0] is the backdrop. */
#define RS_PAL_BG(n)   ((n) * 16)
#define RS_PAL_OBJ(n)  (128 + (n) * 16)
void     rs_pal_set(int index, rs_color c);
void     rs_pal_load(int first, const rs_color *colors, int count);
rs_color rs_pal_get(int index);
void     rs_backdrop(rs_color c);

/* ---- Tiles (VRAM) ------------------------------------------------------ */
/* One shared tile memory of 8x8 4bpp tiles, like SNES VRAM. BG layers and
 * sprites address it through a base (character base). Guideline: 64 KiB of
 * VRAM = 2048 4bpp tiles (maps count too). Hard limit: RS_TILE_MAX. */
#define RS_TILE_MAX   4096
#define RS_TILE_BYTES 32            /* packed 4bpp: 4 bytes per row, high nibble = left pixel */
void rs_tiles_load(int first, const uint8_t *packed4bpp, int count);
void rs_tiles_load8(int first, const uint8_t *pixels, int count); /* 64 bytes/tile, values 0..15 */
void rs_tile_pixel(int tile, int x, int y, int value);            /* poke one pixel */
int  rs_tiles_used(void);           /* number of distinct tiles written (VRAM accounting) */

/* ---- Background layers ---------------------------------------------------
 * Map entry = SNES format: vhopppcc cccccccc
 *   bits 0-9 tile (relative to the layer's tile base), 10-12 palette,
 *   13 priority, 14 h-flip, 15 v-flip. */
#define RS_BG_COUNT 4
#define RS_BG1 0
#define RS_BG2 1
#define RS_BG3 2
#define RS_BG4 3
#define RS_MAP(tile, pal, prio, hf, vf) ((uint16_t)(((tile) & 1023) | (((pal) & 7) << 10) | \
        (((prio) & 1) << 13) | (((hf) & 1) << 14) | (((vf) & 1) << 15)))
#define RS_MAP_TILE(e)  ((e) & 1023)
#define RS_MAP_PAL(e)   (((e) >> 10) & 7)
#define RS_MAP_PRIO     0x2000
#define RS_MAP_HFLIP    0x4000
#define RS_MAP_VFLIP    0x8000

/* map_w / map_h in tiles: 32, 64 or 128 each. Clears the map. */
void      rs_bg_setup(int layer, int map_w, int map_h, int tile_base);
void      rs_bg_enable(int layer, int on);
uint16_t *rs_bg_map(int layer);                 /* map_w * map_h entries, row-major */
int       rs_bg_map_w(int layer);
int       rs_bg_map_h(int layer);
void      rs_bg_put(int layer, int x, int y, uint16_t entry);
uint16_t  rs_bg_get(int layer, int x, int y);
void      rs_bg_fill(int layer, uint16_t entry);
/* A 16x16 metatile = 4 map entries (top-left, top-right, bottom-left, bottom-right). */
void      rs_bg_meta(int layer, int mx, int my, const uint16_t meta[4]);
void      rs_bg_scroll(int layer, int x, int y);
/* HDMA-like per-scanline scroll offsets (added to the layer scroll), 240
 * entries each, or NULL. The tables are read during rendering. */
void      rs_bg_line_scroll(int layer, const int16_t *dx, const int16_t *dy);

/* Affine (Mode-7-like) mode for one layer at a time. Matrix in 8.8 fixed
 * point; (cx, cy) is the centre in map pixels. SNES formula:
 *   X = a*(sx + hofs - cx) + b*(sy + vofs - cy) + cx, same for Y with c, d.
 * The raster callback may change the matrix per line (perspective). */
typedef struct rs_affine {
    int32_t a, b, c, d;         /* 0x100 = 1.0 */
    int32_t cx, cy;
    int wrap;                   /* 1 = map repeats, 0 = transparent outside */
} rs_affine;
void rs_bg_affine(int layer, const rs_affine *m); /* NULL turns affine off */

/* ---- Raster (per-scanline) callback -------------------------------------
 * Called before each visible line is rendered: change scroll, windows,
 * palettes, affine matrices... exactly like HDMA / H-IRQ code on the SNES. */
typedef void (*rs_raster_fn)(int line, void *user);
void rs_raster(rs_raster_fn fn, void *user);

/* ---- Windows, clipping and colour math ------------------------------- */
/* Two windows, each a horizontal span [left, right) that the raster
 * callback may change per line (a circle = iris transition). */
void rs_window(int w, int left, int right);
#define RS_WIN1      1          /* hide inside window 1 */
#define RS_WIN1_OUT  2          /* hide outside window 1 */
#define RS_WIN2      4
#define RS_WIN2_OUT  8
void rs_bg_window(int layer, int mask);
void rs_obj_window(int mask);
void rs_clip_black(int mask);   /* the final picture is black where the mask hides */
/* Fog: where the window mask hides, the given layers (RS_MATH_BG1..BG4, RS_MATH_BACK) are blended 3/4
 * towards the fog colour; sprites and the other layers stay clear. mask 0 = no fog. */
void rs_fog(int mask, rs_color colour, int layers);

#define RS_MATH_BG1   1
#define RS_MATH_BG2   2
#define RS_MATH_BG3   4
#define RS_MATH_BG4   8
#define RS_MATH_OBJ   16        /* sprite palettes 4-7 only, as on the SNES */
#define RS_MATH_BACK  32        /* backdrop */
#define RS_MATH_OFF   0
#define RS_MATH_ADD   1
#define RS_MATH_SUB   2
#define RS_MATH_HALF  4         /* halve the result (only when blending two pixels) */
#define RS_MATH_FIXED 8         /* operand = fixed colour instead of the pixel below */
/* Layers in `layers` are blended with what is below them (painter order) or
 * with the fixed colour. */
void rs_math(int mode, int layers, rs_color fixed);
void rs_brightness(int level);  /* 0 (black) .. 15 (full), like INIDISP */

/* ---- Sprites (OAM) ---------------------------------------------------------
 * A WxH sprite (multiples of 8, 8..64) uses (W/8)*(H/8) CONSECUTIVE tiles in
 * row-major order starting at obj_base + tile. Lower OAM index = in front of
 * other sprites. Priority 0..3 places the sprite between BG layers
 * (docs/spec.md, "Priorities"). Guideline: 128 sprites, 32 per line. */
#define RS_OAM_MAX    256       /* hard limit; guideline 128 */
#define RS_OAM_GUIDE  128
#define RS_SPR_HFLIP  1
#define RS_SPR_VFLIP  2
#define RS_SPR_HIDE   4
typedef struct rs_sprite {
    int16_t  x, y;
    uint16_t tile;
    uint8_t  w, h;
    uint8_t  pal;               /* 0..7 (sprite palettes) */
    uint8_t  prio;              /* 0..3 */
    uint8_t  flags;
    uint8_t  used;
} rs_sprite;
void       rs_obj_base(int tile_base);
void       rs_oam_clear(void);  /* hide all sprites */
rs_sprite *rs_oam(int index);
/* Convenience: returns the index used (next free slot), or -1 when full. */
int        rs_spr(int x, int y, int tile, int w, int h, int pal, int prio, int flags);

/* ---- Viewports (split screen) ---------------------------------------------
 * Up to RS_VIEW_MAX screen rectangles. Each draws the BG layers of its mask with ITS OWN scroll (sx, sy: the
 * layer pixel shown at the viewport's top-left corner) and the sprites of its OAM range [oam_first,
 * oam_first + oam_count), whose coordinates are relative to the viewport's top-left corner, all clipped to
 * the rectangle. Later viewports are drawn over earlier ones (an overlay box is a viewport); screen pixels
 * that no viewport covers show the divider colour. The raster callback runs for each line AND each viewport
 * crossing it (rs_viewport_current() tells which); windows stay in screen columns. Line-scroll tables still
 * apply (by screen line); an affine layer ignores the viewports' scroll. n = 0 (the default) = one full screen
 * with the layers' global scroll and every sprite. Cost: about the same pixels as one screen, plus one 8-px
 * tile per layer and viewport on each line (docs/spec.md, "Viewports"). */
#define RS_VIEW_MAX      12
#define RS_VIEW_DIVIDER  2          /* px between the standard layouts' viewports */
typedef struct rs_viewport {
    int16_t  x, y, w, h;            /* screen rectangle */
    int16_t  sx[4], sy[4];          /* per BG layer: the layer pixel at the viewport's top-left */
    uint8_t  layers;                /* bit l: BG layer l drawn here (if enabled) */
    uint8_t  objs;                  /* 1: this viewport's sprites are drawn */
    uint16_t oam_first, oam_count;  /* its sprites */
} rs_viewport;
void rs_viewports(int n, const rs_viewport *views, rs_color divider);
int  rs_viewport_count(void);
int  rs_viewport_current(void);     /* in the raster callback: the viewport being drawn, -1 = none */
int  rs_oam_next(void);             /* the first free OAM slot: where the next rs_spr() goes */
/* The standard layouts (rectangles only; layers 0x0f, sprites on): 1 = full screen; 2 = left/right halves
 * (RS_LAYOUT_HSPLIT: top/bottom); 3 = a top half and two bottom quarters (RS_LAYOUT_MAP: 4 quadrants, the
 * 4th free for a map); 4 = quadrants. Returns the number of rectangles written. */
#define RS_LAYOUT_HSPLIT 1
#define RS_LAYOUT_MAP    2
int  rs_viewport_layout(int players, int flags, rs_viewport *out);

/* ---- Input ------------------------------------------------------------ */
/* SNES pad layout, SNES bit order (JOY1 register). */
#define RS_PAD_MAX   4
#define RS_BTN_B      0x8000
#define RS_BTN_Y      0x4000
#define RS_BTN_SELECT 0x2000
#define RS_BTN_START  0x1000
#define RS_BTN_UP     0x0800
#define RS_BTN_DOWN   0x0400
#define RS_BTN_LEFT   0x0200
#define RS_BTN_RIGHT  0x0100
#define RS_BTN_A      0x0080
#define RS_BTN_X      0x0040
#define RS_BTN_L      0x0020
#define RS_BTN_R      0x0010
uint16_t rs_pad(int port);           /* held */
uint16_t rs_pad_pressed(int port);   /* went down this frame */
uint16_t rs_pad_released(int port);  /* went up this frame */
int      rs_pad_connected(int port);
/* What drives a port, so a game can name its buttons ("PRESS A (X KEY)"): a pad, or one of the
 * desktop keyboard's two key sets (docs/spec.md "Input"). Set by the frontend; a pad by default. */
#define RS_DEVICE_PAD       0
#define RS_DEVICE_KEYBOARD  1        /* first key set: arrows, X = A, Z = B, Enter = Start */
#define RS_DEVICE_KEYBOARD2 2        /* second key set: WASD, H = A, G = B, T = Start */
int      rs_pad_device(int port);

/* ---- Save RAM (battery) ------------------------------------------------ */
#define RS_SRAM_SIZE 32768
uint8_t *rs_sram(void);
void     rs_sram_commit(void);       /* tell the frontend to persist it */

/* ---- Save states ---------------------------------------------------------
 * A state is the whole console (video and audio memory, registers, input edges, frame counter, the global
 * RNG, the music track and its position) plus what the game registers here, from its rs_game.state
 * callback only (called once, before init() may even have run: register static objects, do nothing else).
 * Everything is saved byte for byte, so it must not contain pointers, except those declared with
 * rs_state_ptr(): they are saved as (registered object, offset). Save RAM is NOT part of a state: keep
 * progress (unlocks, best scores) in rs_sram() and do not register its in-memory copy.
 * Details and the file format: docs/spec.md "Save states". */
void rs_state_var(const char *name, void *data, size_t size);            /* saved and restored */
void rs_state_ptr(const char *name, void *pointer_variable);             /* a pointer: (object, offset) */
void rs_state_ref(const char *name, const void *data, size_t size);      /* not saved; pointers may aim here */
void rs_state_raster(const char *name, rs_raster_fn fn);                 /* a raster callback the game installs */
#define RS_STATE(v)        rs_state_var(#v, &(v), sizeof(v))
#define RS_STATE_PTR(p)    rs_state_ptr(#p, &(p))
#define RS_STATE_REF(v)    rs_state_ref(#v, &(v), sizeof(v))
#define RS_STATE_RASTER(f) rs_state_raster(#f, f)

/* ---- Deterministic RNG (xorshift32) ------------------------------------ */
typedef struct rs_rng { uint32_t s; } rs_rng;
void     rs_rng_seed(rs_rng *r, uint32_t seed);
uint32_t rs_rng_next(rs_rng *r);
int      rs_rng_range(rs_rng *r, int n);     /* 0..n-1 */
/* The global RNG, seeded with a constant at start (deterministic runs). */
void     rs_srand(uint32_t seed);
uint32_t rs_rand(void);
int      rs_rand_range(int n);

/* ---- Text (8x8 bitmap font as tiles) ------------------------------------
 * rs_text_load uploads the font (96 glyphs, ASCII 32..127) at tile `first`:
 * colour 1 = ink, colour 2 = shadow. With big != 0 it also uploads the 2x
 * font (4 tiles per glyph, 384 tiles) right after. Returns tiles used. */
int  rs_text_load(int first, int big);
/* Which layer/palette text goes to; tile_first is RELATIVE to the layer's
 * tile base. prio sets the map priority bit. */
void rs_text_setup(int layer, int tile_first, int pal, int prio);
void rs_text(int x, int y, const char *s);           /* tile coordinates */
void rs_textf(int x, int y, const char *fmt, ...);
void rs_text_big(int x, int y, const char *s);       /* 2x2 tiles per glyph */
void rs_text_pal(int pal);
void rs_text_clear(int x, int y, int w, int h);

/* ---- Audio ------------------------------------------------------------ */
/* 8 voices (guideline; hard max RS_VOICE_MAX), sample based, 32 kHz stereo.
 * Pitch 0x1000 = the sample's own rate. Volume 0..127, pan 0 (left)..127. */
#define RS_VOICES     8
#define RS_VOICE_MAX  16
#define RS_SAMPLE_MAX 64
#define RS_PITCH_1    0x1000
typedef struct rs_adsr {
    uint16_t attack_ms, decay_ms;
    uint8_t  sustain;           /* 0..127 */
    uint16_t release_ms;
} rs_adsr;
/* loop_start < 0 = one-shot. Data is copied. Returns 0 on success. */
int  rs_sample_pcm16(int slot, const int16_t *data, int frames, int rate, int loop_start);
/* IMA ADPCM, 4 bits per sample, decoded on load (counts as 4-bit in the
 * sample-memory guideline). */
int  rs_sample_adpcm(int slot, const uint8_t *data, int frames, int rate, int loop_start);
int  rs_voice_play(int voice, int slot, int pitch, int vol, int pan, const rs_adsr *env);
void rs_voice_set(int voice, int pitch, int vol, int pan);
void rs_voice_release(int voice);
void rs_voice_echo(int voice, int on);
int  rs_voice_active(int voice);
/* One-shot sound effect on a free voice (steals the oldest). Returns the voice. */
int  rs_sfx(int slot, int vol, int pan, int pitch);
/* Global echo: delay 0..240 ms, feedback and volume 0..127. */
void rs_echo(int delay_ms, int feedback, int volume);
/* Tracker music (MOD/S3M/XM/IT) via libxmp-lite. The data must stay valid. */
int  rs_music_play(const void *module, size_t size, int loop);
void rs_music_stop(void);
void rs_music_volume(int vol);  /* 0..127 */
int  rs_music_playing(void);
void rs_sound_volume(int vol);  /* sfx master 0..127 */

#ifdef __cplusplus
}
#endif
#endif
