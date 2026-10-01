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

/* Deprecated calls (the "get ready" flow, replaced by the title lobby below) still work; build with
 * -DHU_WARN_DEPRECATED to have the compiler point at them. */
#if defined(HU_WARN_DEPRECATED) && defined(__GNUC__)
#define HU_DEPRECATED __attribute__((deprecated))
#else
#define HU_DEPRECATED
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
#define HU_OBJ_TILES  118       /* the kit sprites (110 before the D-pad glyph) */
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
/* The kit draws four arrows over glyphs that games do not use: put them in strings with these. */
#define HU_ARROW_LEFT  "{"
#define HU_ARROW_RIGHT "}"
#define HU_ARROW_UP    "^"
#define HU_ARROW_DOWN  "~"
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
/* DEPRECATED (no more "get ready": the title is the only menu, see hu_title_*): "GET READY", 2x cream, centred */
HU_DEPRECATED void hu_get_ready(int y);
/* the blinking prompt "PRESS A TO <VERB>": returns the pixel x where the A glyph goes (hu_glyph) */
int  hu_prompt(int y, const char *text, int blink_t);
void hu_copyright(int y);                              /* (C) 2026 8BCRAFT - RETROSTONE VC */
/* DEPRECATED (hu_title_draw shows the 4-player join line): "P2: PRESS A ON PAD 2 TO JOIN" or "P2 JOINED - <what>!" */
HU_DEPRECATED void hu_join_line(int y, int players, const char *joined);
/* the game-over score panel (below a hu_banner at row 4): labels on the left, values drawn as sprites by
 * hu_gameover_sprites(); 1 player: SCORE, BEST (NEW BEST), MEDAL; 2 players: PLAYER 1, PLAYER 2 and the winner.
 * 2-4 players: prefer the ranking (hu_results_*). */
void hu_gameover_panel(int players, int new_best, int score1, int score2);
void hu_gameover_sprites(int players, int score1, int score2, int best, int medal, int t, int slide_px);
void hu_retry_line(int t, int lock, const char *text); /* the blinking "A: <AGAIN>" line once t >= lock (row 19) */
void hu_retry_line_at(int row, int t, int lock, const char *text);

/* ---- players and the title (the only menu: no "get ready") -------------------------------------------------------
 * The title shows the logo, "PRESS <input> TO <VERB>" (P1 starts the game at once with one of its START INPUTS: the
 * game's natural play inputs, e.g. the D-pad and A), the player slots (P1 .. P4: the joined players' icons pop in),
 * the join line "P2 / P3 / P4: PRESS A TO JOIN", BEST and the credits. Pads 2-4 join with A (or Start) and leave
 * with B. Players are numbered in the order they joined (P1 is always pad 1): hu_player_pad() maps a player to
 * its pad. The lobby lives in the kit (saved by hu_state) and stays between runs: a retry keeps the players.
 * Rows (house layout): slots 18, prompt 21, BEST 24, join line 26, copyright 28. */
#define HU_MAX_PLAYERS 4
#define HU_IN_DPAD (RS_BTN_UP | RS_BTN_DOWN | RS_BTN_LEFT | RS_BTN_RIGHT)
#define HU_IN_LR   (RS_BTN_LEFT | RS_BTN_RIGHT)
#define HU_JOIN_BUTTONS  (RS_BTN_A | RS_BTN_START)
#define HU_LEAVE_BUTTONS RS_BTN_B
enum { HU_ROW_SLOTS = 18, HU_ROW_PROMPT = 21, HU_ROW_BEST = 24, HU_ROW_JOIN = 26, HU_ROW_COPYRIGHT = 28 };
typedef struct hu_title_cfg {
    uint16_t start;             /* P1's start inputs (RS_BTN_*): Leady Squid A|B|X|Y|UP|START, Duck Parade HU_IN_DPAD|A */
    int max_players;            /* 1..4 (1: no slots, no join line) */
    const char *verb;           /* "SWIM" -> PRESS A TO SWIM */
    const char *prompt;         /* NULL: made from start and verb (PRESS A / PRESS <-/-> / PRESS ANY ARROW TO <VERB>) */
    int slot_row;               /* 0: HU_ROW_SLOTS */
} hu_title_cfg;
void hu_title_setup(const hu_title_cfg *c);            /* after hu_init (which resets the lobby to P1 alone) */
void hu_players_set(int n);                            /* n players on pads 1..n (--opt players=N, tests) */
int  hu_players(void);                                 /* 1..4 */
int  hu_player_pad(int p);                             /* the pad (rs_pad_* port) of player p */
int  hu_player_age(int p);                             /* frames since player p joined (capped at 255) */
uint16_t hu_start_inputs(void);
int  hu_start_pressed(int p);                          /* player p pressed one of the start inputs this frame */
/* the default OBJ palette of player p (house rule, docs/art-direction.md: P1 OBJ 0, P2 OBJ 1, P3 OBJ 4, P4 OBJ 5) */
int  hu_player_pal(int p);
/* Call once per update on the title: joins (A / Start on a free pad), leaves (B on a joined pad 2-4), P1's start.
 * Returns HU_TITLE_* bits: play the join sound (house: HA_CONFIRM) on JOINED, a tick on LEFT; start on START. */
enum { HU_TITLE_JOINED = 1, HU_TITLE_LEFT = 2, HU_TITLE_START = 4 };
int  hu_title_update(void);
/* BG, every title frame: the prompt (blinking), the slot labels, BEST n (if > 0), the join line, the credits */
void hu_title_draw(int t, int best);
/* sprites, every title frame: the prompt's glyph (A button or D-pad) and, for each joined player, icon(p, cx, cy, t,
 * user) at its slot (the game draws its hero icon in hu_player_pal(p); cy pops in with a bounce) and a sparkle as it
 * joins. icon may be NULL. Front to back: call it before the scene's sprites. */
typedef void (*hu_icon_fn)(int player, int cx, int cy, int t, void *user);
void hu_title_sprites(int t, hu_icon_fn icon, void *user);
/* where the title glyph goes (a game with its own glyph sprites): returns HU_BTN_A, HU_BTN_DPAD or -1 (none) */
int  hu_title_glyph(int *x, int *y);
void hu_slot_pos(int p, int *cx, int *cy);             /* the centre of player p's icon on the title */

/* ---- 4-player HUD ------------------------------------------------------------------------------------------------- */
/* Score chips: 1 player centred at the top (y 10), 2 players at x 80 and 240 (y 10), 3-4 players in the corners
 * (P1 top-left, P2 top-right, P3 bottom-left, P4 bottom-right) with a P1..P4 tag; bottom_y is the digits' top y of
 * the bottom chips (0: 220). */
typedef struct hu_chip {
    int x, y, align;            /* the digits: x (align -1: left edge, 0: centre, 1: right edge), top y */
    int tag_x, tag_y;           /* the tag's tile (-1: no tag) */
} hu_chip;
hu_chip hu_score_chip(int players, int p, int bottom_y);
void hu_score_tags(int players, int bottom_y);         /* BG: the tags (3-4 players), once when the run starts */
void hu_score_chips(int players, const int score[], int bottom_y);   /* the kit digits, every frame */
void hu_number_at(int n, int x, int y, int align, int prio);         /* big digits, aligned like a chip */
/* Split screen (rs_viewports): 1 player the full screen, 2 halves side by side; 3-4 players HU_SPLIT_QUAD (2 x 2
 * quadrants, 159 x 119; 3 players leave the 4th free) or HU_SPLIT_COLUMNS (n columns, 78 px wide for 4, the full
 * height). 2-px dividers. Layers 0x0f, sprites on, oam_first / oam_count 0: bracket each view's sprites with
 * hu_view_oam_begin / end. Returns the number of views. */
enum { HU_SPLIT_QUAD, HU_SPLIT_COLUMNS };
int  hu_split(int players, int mode, rs_viewport *out);
void hu_view_oam_begin(rs_viewport *v);                /* the next rs_spr() calls belong to v ... */
void hu_view_oam_end(rs_viewport *v);                  /* ... up to here */

/* ---- results for 2-4 players: a ranking with medals --------------------------------------------------------------- */
typedef struct hu_standing {
    int n;                      /* players */
    int order[HU_MAX_PLAYERS];  /* the players, first place first */
    int rank[HU_MAX_PLAYERS];   /* the rank of each place (0 = 1st; equal keys share a rank) */
    int value[HU_MAX_PLAYERS];  /* by player: the number shown */
} hu_standing;
/* rank n players by key (higher = better: the score, or the time a player lasted...); value[] is what is shown */
void hu_rank(hu_standing *s, int n, const int key[], const int value[]);
int  hu_winner(const hu_standing *s);                  /* the winning player, -1 when several share the 1st place */
/* BG: the banner on row 3 (title, or NULL: "P2 WINS!" / "DRAW!") and the panel from row 8: one row per place, 3 rows
 * apart: the rank (1ST..4TH) and the tag (P1..P4); the retry line goes on hu_results_retry_row() (hu_retry_line_at) */
void hu_results_panel(const hu_standing *s, const char *title);
/* sprites: the values (big digits) and the medals (1st gold, 2nd silver, 3rd bronze, ties alike), a sparkle by the
 * winner; slide_px as for hu_gameover_sprites */
void hu_results_sprites(const hu_standing *s, int t, int slide_px);
void hu_results_icon_pos(const hu_standing *s, int place, int *cx, int *cy);   /* where the game draws a hero icon */
int  hu_results_medal(const hu_standing *s, int place);                         /* its medal tier (0 none, 1..4) */
int  hu_results_row(const hu_standing *s, int place);                           /* its text row */
int  hu_results_retry_row(const hu_standing *s);

/* ---- pause ------------------------------------------------------------------------------------------------------ */
/* Call every frame: dims the screen (brightness 9) and shows PAUSED on row y while paused. */
void hu_pause(int paused, int y);

/* ---- sprites (the kit's OBJ tiles; cfg.obj_tile >= 0) -------------------------------------------------------------- */
void hu_number(int n, int cx, int y, int prio);        /* big digits centred on cx (12-px advance) */
enum { HU_BTN_A, HU_BTN_B, HU_BTN_X, HU_BTN_Y, HU_BTN_DPAD };
void hu_glyph(int button, int x, int y, int pressed, int prio);   /* 16x16 round pad button, or the D-pad cross */
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
