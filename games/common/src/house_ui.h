/*
 * The 8BCraft house UI kit for RetroStone VC games (games/common): the look of Leady Squid, shared.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The rules are in docs/art-direction.md; the Python half (sheets, palettes, the logo) is
 * games/common/tools/house_style.py. A game opts in with HOUSE_UI_<game> = 1 in its game.mk.
 *
 * BG side (one BG layer, usually BG1 = the front, priority 1):
 *   the house font (cream ink, 1-px navy shadow), the same font on the panel colour, a cache of 2x outlined
 *   glyphs (free: cream over the scene; banner: gold on the panel), the panel frame, the title logo.
 *   Tiles, relative to cfg.tile_base (the layer's tile base + tile_base must be free for HU_BG_TILES tiles):
 *     0..95 font, 96..191 font on the panel, 192..287 2x glyph cache, 288..296 panel frame,
 *     then the logo at cfg.logo_tile (up to HU_LOGO_TILES).
 * OBJ side (optional, cfg.obj_tile >= 0): the big score digits, the pad-button glyphs, the four medals and
 *   the sparkle, drawn at start-up into HU_OBJ_TILES sprite tiles, on sprite palette cfg.obj_pal.
 *
 * Every function is deterministic (no RNG, no clock): the games' determinism tests hold.
 */
#ifndef HOUSE_UI_H
#define HOUSE_UI_H

#include <stdint.h>
#include "rs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- the UI palette (a BG palette; house_style.py HOUSE "cream", "ui_navy", "ui_gold") ---------------------- */
enum {
    HU_CREAM = 1,       /* text ink            #fff6dc */
    HU_OUTLINE = 2,     /* glyph outline, shadow of the small font  #101838 */
    HU_FILL = 3,        /* panel fill          #2a4476 */
    HU_LIGHT = 4,       /* panel inner rim     #78aad2 */
    HU_DARK = 5,        /* panel rim, banner drop shadow  #0c142c */
    HU_GOLD = 6,        /* banner titles       #fad25a */
    HU_HILITE = 7,      /* the panel's top highlight line  #365488 */
    HU_RED = 8,         /* warnings, P1 accents  #e8584c */
    HU_GREEN = 9,       /* "ok", P2 accents    #6cc860 */
    HU_GREY = 10        /* disabled text       #9aa2b4 */
};
extern const rs_color hu_ui_palette[16];
/* the sprite palette of the kit sprites (digits, glyphs, medals, sparkle) */
extern const rs_color hu_obj_palette[16];

/* ---- set-up --------------------------------------------------------------------------------------------------- */
#define HU_BG_TILES   297       /* font, panel font, 2x cache, panel frame */
#define HU_LOGO_TILES 320       /* the logo, at most (40 x 8 tiles) */
#define HU_OBJ_TILES  110       /* the kit sprites */
typedef struct hu_config {
    int layer;                  /* the UI BG layer (RS_BG1) */
    int vram_base;              /* the layer's tile base (absolute VRAM tile, as given to rs_bg_setup) */
    int tile_base;              /* the kit's first tile, relative to the layer's base */
    int pal;                    /* the UI BG palette 0..7 (loaded by hu_init) */
    int logo_tile;              /* the logo's first tile, relative to the layer's base (-1: no logo) */
    int logo_pal;               /* its BG palette */
    int obj_tile;               /* the kit sprites' first tile, relative to the OBJ base (-1: none) */
    int obj_vram;               /* the OBJ base (absolute, as given to rs_obj_base) */
    int obj_pal;                /* their sprite palette 0..7 (loaded by hu_init) */
    const char *box_glyphs;     /* the glyphs of the panel font to upload (NULL: all 96) */
} hu_config;
/* The defaults: BG1 at VRAM 0, the kit at 0, palette 0, the logo at 320 on palette 5, the kit sprites at
 * OBJ tile 0 on sprite palette 3 (obj_vram 3072). */
hu_config hu_defaults(void);
void hu_init(const hu_config *cfg);
const hu_config *hu_cfg(void);
/* save states: registers the kit's own objects (call it from the game's state callback) */
void hu_state(void);

/* ---- text (tile coordinates, 40 x 30 on screen) -------------------------------------------------------------- */
void hu_text(int x, int y, const char *s);             /* cream, 1-px shadow, over the scene */
void hu_box_text(int x, int y, const char *s);         /* the same on the panel fill */
enum { HU_BIG_FREE, HU_BIG_BANNER };
void hu_big(int x, int y, const char *s, int style);   /* 2x glyphs, 2 tiles per character */
int  hu_center(const char *s, int big);                /* the column that centres s */
void hu_clear(void);                                   /* clears the UI layer and the 2x cache */
void hu_clear_rows(int y, int h);                      /* blank rows (the free font's space) */

/* ---- panels and banners --------------------------------------------------------------------------------------- */
void hu_panel(int x, int y, int w, int h);             /* rounded navy panel, w x h tiles (>= 2 x 2) */
/* a titled banner: a 20 x 4 panel centred on row y with the title in gold 2x glyphs (GAME OVER, PAUSED...) */
void hu_banner(int y, const char *title);
void hu_get_ready(int y);                              /* "GET READY", 2x cream, centred */
/* the blinking prompt "PRESS A TO <VERB>": returns the pixel x where the A glyph goes (hu_glyph) */
int  hu_prompt(int y, const char *text, int blink_t);
void hu_copyright(int y);                              /* (C) 2026 8BCRAFT - RETROSTONE VC */
/* the 2-player line under "get ready": "P2: PRESS A ON PAD 2 TO JOIN" or "P2 JOINED - <what>!" */
void hu_join_line(int y, int players, const char *joined);
/* the game-over score panel (below a hu_banner at row 4): labels on the left, values drawn as sprites by
 * hu_gameover_sprites(); 1 player: SCORE, BEST (NEW BEST), MEDAL; 2 players: PLAYER 1, PLAYER 2 and the winner */
void hu_gameover_panel(int players, int new_best, int score1, int score2);
void hu_gameover_sprites(int players, int score1, int score2, int best, int medal, int t, int slide_px);
void hu_retry_line(int t, int lock, const char *text); /* the blinking "A: <AGAIN>" line once t >= lock */

/* ---- pause ------------------------------------------------------------------------------------------------------ */
/* Call every frame: dims the screen (brightness 9) and shows PAUSED on row y while paused. */
void hu_pause(int paused, int y);

/* ---- sprites (the kit's OBJ tiles; cfg.obj_tile >= 0) -------------------------------------------------------------- */
void hu_number(int n, int cx, int y, int prio);        /* big digits centred on cx (12-px advance) */
enum { HU_BTN_A, HU_BTN_B, HU_BTN_X, HU_BTN_Y };
void hu_glyph(int button, int x, int y, int pressed, int prio);   /* 16x16 round pad button */
void hu_medal(int tier, int x, int y, int prio);       /* tier 1 bronze .. 4 pearl, 24x24 */
void hu_sparkle(int x, int y, int frame, int prio);    /* 8x8 */
/* medal tier of a score: thresholds[0..3] = bronze, silver, gold, pearl (Leady Squid: 10, 20, 30, 40) */
int  hu_medal_of(int score, const int thresholds[4]);

/* ---- the title logo ----------------------------------------------------------------------------------------------- */
/* Draws `title` (capitals, digits, spaces) as the house logo: the font at 4x (3x when it would not fit in 304 px),
 * each word in a 3-shade ramp (light, mid, dark), a 1-px outline and a navy drop shadow (the 3D extrusion),
 * centred on the screen with its top at tile row `ty` (8 tile rows tall). ramps[w] is the ramp of word w
 * (the last ramp repeats); rivets != 0 puts a white rivet on each letter of the first word (Leady Squid).
 * Returns the tiles used. The same logo as house_style.logo(). */
int  hu_logo(const char *title, const rs_color (*ramps)[3], int nramps, int ty, int rivets);
void hu_logo_hide(void);

/* ---- motion --------------------------------------------------------------------------------------------------------- */
/* Integer tweens: t frames of dur, from 0 to dist (t clamped to 0..dur). */
int hu_ease_linear(int t, int dur, int dist);
int hu_ease_out(int t, int dur, int dist);    /* quadratic: fast start, soft landing (UI slide-in) */
int hu_ease_in(int t, int dur, int dist);     /* quadratic: soft start (UI slide-out) */
int hu_ease_inout(int t, int dur, int dist);
int hu_ease_back(int t, int dur, int dist);   /* out with a 10% overshoot (banners, medals popping in) */
/* the remaining offset of a slide-in: dist at t = 0, 0 at t = dur (Leady Squid's game-over panel: 200 px, 20
 * frames: (dur - t)^2 * dist / dur^2) */
int hu_slide_in(int t, int dur, int dist);
int hu_bob(int t, int period, int ampl);      /* a triangle bob, -ampl..ampl (the title hero, get ready) */
int hu_blink(int t);                          /* 1 for 30 frames, 0 for 30 frames */
/* screen shake: a deterministic decaying wobble; call hu_shake_step() once per update and add the offsets to
 * the playfield layers' scroll (never to the UI layer). amp <= 4 px, frames <= 16 (docs/art-direction.md). */
void hu_shake(int amp, int frames);
void hu_shake_step(void);
int  hu_shake_x(void);
int  hu_shake_y(void);

/* ---- small helpers ---------------------------------------------------------------------------------------------------- */
rs_color hu_lerp_color(rs_color a, rs_color b, int k256);   /* k256 = 0 -> a, 256 -> b */
rs_color hu_scale_color(rs_color c, int k256);              /* darken (k256 < 256) */

#ifdef __cplusplus
}
#endif
#endif
