/*
 * Word Tiles in RetroStone Board: the crossword tile game for 2 to 4 players passing the device round (the rules:
 * wordtiles_rules.c, wordtiles.h). VIEW_HIDDEN: each rack is secret, a cover hides the screen whenever the device
 * passes.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The table: the board as wide as the table allows (15 x 15 squares of the house wood in its walnut frame, our own
 * premium squares inlaid in lacquer: jade for the letter premiums, amber and red for the word premiums, two or three
 * marks for double or triple, a gold star in the centre), the player's rack on a plank below it (beside it in
 * landscape), and a row of round walnut buttons: recall the tiles, shuffle the rack, exchange tiles, challenge the last
 * play, play. The bag shows how many tiles are left. The tiles: ivory, the letter in ink and its value small in a
 * corner; a blank's letter in a paler ink, no value.
 *
 * Placing tiles on a phone, where a square is small: every placement happens where the finger is lifted, and while
 * the finger is down on the board a loupe above it shows the squares under it magnified, the target square marked, so
 * the finger can slide to the right square before it lets go. Taps: tap a tile of the rack (it lifts), then a square;
 * or tap a square first (a cursor, with an arrow: across or down; tap it again to turn the arrow), then tap tiles one
 * after another: each goes to the cursor, which moves on along the arrow, as when typing. Drag: a tile from the rack
 * to a square, a tile just placed to another square or back to the rack, a tile of the rack along the rack to put it
 * elsewhere. A tile just placed, tapped, goes back to the rack. A blank: a grid of the set's letters opens; tap the
 * letter it stands for. While tiles are placed, the score they would make shows in a gold-rimmed disc at the end of
 * the main word (no disc: the play is not allowed there). The pass is the shell's pass button.
 * Undo: the last tile placed goes back to the rack; then a challenge made this turn; never past the hand-off.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/race.h"
#include "games/wordtiles.h"
#include "shell.h"
#include "text.h"

#define FLY_MS 240                              /* a tile flying (placed, recalled, drawn) */
#define DRAW_GAP 90                             /* the drawn tiles one after another */
#define REST_MS 300                             /* after the last tile lands: a moment before the device passes */
#define FLASH_MS 650                            /* the tiles of a play settling in a gold light */
#define LOUPE_Z 2.3f                            /* the loupe's magnification */
#define LOUPE_CELLS 3.3f                        /* the squares it shows across */

static wgame G;
static const wpos *P = &G.pos;
static int hand_n;                              /* the history's length when the turn began: undo stops there */

/* ---- the table's geometry ---------------------------------------------------------------------------------------- */
static struct {
    box b;
    int me, land;
    float bx, by, bs, fr, sq, ox, oy;           /* the board's picture (bs a side), its frame, a square, square 0's
                                                 * top left */
    float rs, rx[WT_RACK], ry[WT_RACK];         /* the rack: a tile's size, the slots' centres */
    float px, py, pw, ph;                       /* the rack's plank */
    float kd, kx[6], ky[6];                     /* the buttons (0 the bag), their diameter */
} T;

enum { K_BAG, K_RECALL, K_SHUFFLE, K_EXCHANGE, K_CHALLENGE, K_PLAY, KNOBS };

static void board_fit(float x, float y, float side)
{
    T.fr = floorf(side * 0.028f);
    T.sq = floorf((side - 2 * T.fr) / WT_N);
    T.bs = WT_N * T.sq + 2 * T.fr;
    T.bx = floorf(x + (side - T.bs) / 2);
    T.by = floorf(y);
    T.ox = T.bx + T.fr;
    T.oy = T.by + T.fr;
}

static void layout(box b)
{
    float u = ui.unit;
    T.b = b;
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= P->players) T.me = P->turn;
    T.land = b.w > b.h * 1.05f;
    if (!T.land) {                              /* portrait: the board, the rack, the buttons */
        T.rs = floorf(fminf(b.w * 0.92f / 7.6f, 1.05f * u));
        T.kd = floorf(fminf(0.9f * u, b.w / 6.8f));
        float below = 1.3f * T.rs + 1.35f * T.kd + 0.2f * u, side = fminf(b.w, b.h - below);
        board_fit(b.x + (b.w - side) / 2, b.y, side);
        float top = T.by + T.bs, rest = b.y + b.h - top, cx = b.x + b.w / 2;
        float ry = top + 0.1f * u + (rest - 0.1f * u) * 0.32f, ky = top + 0.1f * u + (rest - 0.1f * u) * 0.78f;
        for (int i = 0; i < WT_RACK; i++) T.rx[i] = cx + (i - 3) * 1.08f * T.rs, T.ry[i] = ry;
        T.pw = 7 * 1.08f * T.rs + 0.32f * T.rs, T.ph = 1.2f * T.rs;
        T.px = cx - T.pw / 2, T.py = ry - T.ph / 2 + 0.08f * T.rs;
        for (int k = 0; k < KNOBS; k++) T.kx[k] = b.x + (k + 0.5f) * b.w / KNOBS, T.ky[k] = ky;
    } else {                                    /* landscape: the board; the rack standing beside it; the buttons */
        T.rs = floorf(fminf(b.h * 0.9f / 7.7f, 1.05f * u));
        T.kd = floorf(fminf(0.9f * u, b.h / 7.4f));
        float side = fminf(b.h, b.w - (1.35f * T.rs + 1.35f * T.kd + 0.35f * u));
        board_fit(b.x, b.y + (b.h - side) / 2, side);
        float x0 = T.bx + T.bs, rest = b.x + b.w - x0, cy = b.y + b.h / 2;
        float rx = x0 + 0.15f * u + 0.62f * T.rs, kx = b.x + b.w - 0.62f * T.kd;
        if (kx - rx > 2.2f * T.rs) rx = x0 + rest * 0.3f, kx = x0 + rest * 0.75f;
        for (int i = 0; i < WT_RACK; i++) T.rx[i] = rx, T.ry[i] = cy + (i - 3) * 1.08f * T.rs;
        T.pw = 1.2f * T.rs, T.ph = 7 * 1.08f * T.rs + 0.32f * T.rs;
        T.px = rx - T.pw / 2 + 0.06f * T.rs, T.py = cy - T.ph / 2;
        for (int k = 0; k < KNOBS; k++) T.kx[k] = kx, T.ky[k] = b.y + (k + 0.5f) * b.h / KNOBS;
    }
}

static void cell_centre(int cell, float *x, float *y)
{
    *x = T.ox + (cell % WT_N + 0.5f) * T.sq;
    *y = T.oy + (cell / WT_N + 0.5f) * T.sq;
}

static int cell_at(float x, float y)
{
    int c = (int)floorf((x - T.ox) / T.sq), r = (int)floorf((y - T.oy) / T.sq);
    return c >= 0 && r >= 0 && c < WT_N && r < WT_N ? r * WT_N + c : -1;
}

static int on_board(float x, float y)
{
    return x >= T.bx && y >= T.by && x < T.bx + T.bs && y < T.by + T.bs;
}

static int on_rack(float x, float y)
{
    float m = 0.25f * T.rs;
    return x >= T.px - m && y >= T.py - m && x < T.px + T.pw + m && y < T.py + T.ph + m;
}

/* the rack slot under (x, y), -1: none */
static int slot_at(float x, float y)
{
    for (int i = 0; i < WT_RACK; i++)
        if (fabsf(x - T.rx[i]) <= 0.54f * T.rs && fabsf(y - T.ry[i]) <= 0.6f * T.rs) return i;
    return -1;
}

/* the slot nearest to (x, y) along the rack */
static int slot_near(float x, float y)
{
    int best = 0;
    float d = 1e9f;
    for (int i = 0; i < WT_RACK; i++) {
        float e = T.land ? fabsf(y - T.ry[i]) : fabsf(x - T.rx[i]);
        if (e < d) d = e, best = i;
    }
    return best;
}

/* ---- the tiles placed this turn, the input's state --------------------------------------------------------------- */
static struct { int slot, cell, letter; } pl[WT_RACK];
static int npl;
static int sel = -1;                            /* a rack slot chosen (tap-tap) */
static int cur = -1, cur_down;                  /* the cursor on the board, its direction */
static int exch;                                /* exchanging: the tiles chosen (bit: slot), and +128: the mode on */
static struct { int on, slot, cell; } pick;     /* the blank's letter grid */
static struct { int down, src, slot, idx, moved; float x0, y0, x, y; } fing;   /* the finger: what it holds */
enum { SRC_NONE, SRC_RACK, SRC_BOARD, SRC_CELL };

static int placed_slot(int s)
{
    for (int i = 0; i < npl; i++)
        if (pl[i].slot == s) return i;
    return -1;
}

static int placed_cell(int c)
{
    for (int i = 0; i < npl; i++)
        if (pl[i].cell == c) return i;
    return -1;
}

static void build(wact *a)
{
    memset(a, 0, sizeof *a);
    a->kind = WA_PLAY;
    a->n = (uint8_t)npl;
    for (int i = 0; i < npl; i++) {
        a->slot[i] = (uint8_t)pl[i].slot;
        a->cell[i] = (uint8_t)pl[i].cell;
        a->letter[i] = (uint8_t)pl[i].letter;
    }
}

static int mine(void)                           /* the viewer may act now */
{
    return P->phase != WP_OVER && T.me == P->turn && !ui_handing();
}

/* ---- the tiles' pictures ----------------------------------------------------------------------------------------- */
static const uint8_t INK[3] = {44, 32, 26}, PALE_INK[3] = {150, 104, 64};

/* the set's letters all from one font: the boards' font (help/glyphs.txt) when it has them all; else (Polish, Czech,
 * Turkish, Ukrainian letters it lacks) the language's own font, which has them: its rules list the set */
static int own_font(void)
{
    static int set = -1, own;
    if (set != P->set) {
        uint8_t t[16 * 16 * 4];
        const wset *s = wt_set(P->set);
        own = 0;
        for (int i = 0; i < s->n && !own; i++) own = !text_glyph((int)s->l[i].cp, 8, 1, INK, t, 16, 16);
        set = P->set;
    }
    return own;
}

/* a character centred on its ink at (cx, cy): from the boards' font, or (own) drawn as a line of one letter in the
 * language's font and centred by its ink */
static void glyph_at(uint8_t *o, int s, uint32_t cp, float em, float cx, float cy, const uint8_t rgb[3], int own)
{
    int g = (int)(em * 1.6f) + 2, dx = 0, dy = 0;
    uint8_t *t = calloc((size_t)g * g, 4);
    if (!t) return;
    if (own || !text_glyph((int)cp, em, 1, rgb, t, g, g)) {
        char u[5] = {0};
        if (cp < 0x80) u[0] = (char)cp;
        else if (cp < 0x800) u[0] = (char)(0xc0 | cp >> 6), u[1] = (char)(0x80 | (cp & 63));
        else u[0] = (char)(0xe0 | cp >> 12), u[1] = (char)(0x80 | (cp >> 6 & 63)), u[2] = (char)(0x80 | (cp & 63));
        text_line(u, em, 1, rgb, t, g, g, g / 6, g / 8);
        int x0 = g, y0 = g, x1 = -1, y1 = -1;
        for (int y = 0; y < g; y++)
            for (int x = 0; x < g; x++)
                if (t[4 * ((size_t)y * g + x) + 3] > 40) {
                    x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1;
                    y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1;
                }
        if (x1 >= 0) dx = g / 2 - (x0 + x1 + 1) / 2, dy = g / 2 - (y0 + y1 + 1) / 2;
    }
    art_blend(o, s, s, t, g, g, (int)lroundf(cx - g / 2.0f) + dx, (int)lroundf(cy - g / 2.0f) + dy);
    free(t);
}

/* a tile s x s: an ivory face on a darker edge (thicker at the bottom), the letter, the value in a corner; TILE_BACK:
 * its back, plain wood */
#define TILE_BACK 127
static void tile_paint(int code, int s, uint8_t *o)
{
    static const uint8_t edge[4] = {164, 132, 92, 255}, ivory[4] = {244, 233, 208, 255}, wood[4] = {132, 96, 62, 255};
    const uint8_t *face = code == TILE_BACK ? wood : ivory;
    memset(o, 0, (size_t)s * s * 4);
    art_roundrect(s, s, 0.15f * s, edge, o);
    int in = s / 16 > 1 ? s / 16 : 1, fw = s - 2 * in, fh = s - 2 * in - in;
    uint8_t *f = malloc((size_t)fw * fh * 4);
    if (f) {
        art_roundrect(fw, fh, 0.12f * s, face, f);
        for (int y = 0; y < fh; y++)            /* a little light from above */
            for (int x = 0; x < fw; x++) {
                uint8_t *p = f + 4 * ((size_t)y * fw + x);
                float k = 1.05f - 0.1f * y / fh;
                for (int c = 0; c < 3; c++) p[c] = (uint8_t)clampf(p[c] * k, 0, 255);
            }
        art_blend(o, s, s, f, fw, fh, in, in / 2);
        free(f);
    }
    if (code == TILE_BACK) return;
    uint32_t cp = wt_char(P, code);
    if (!cp) return;                            /* a blank on a rack: plain */
    int blank = code & WT_BLANK, v = blank ? 0 : wt_value(P, code);
    float cx = blank ? 0.5f * s : 0.45f * s, cy = blank ? 0.47f * s : 0.45f * s;
    glyph_at(o, s, cp, (blank ? 0.66f : 0.62f) * s, cx, cy, blank ? PALE_INK : INK, own_font());
    if (v > 0) {
        float em = 0.27f * s, x = 0.81f * s, y = 0.79f * s;
        if (v >= 10) {
            glyph_at(o, s, '1', em, x - 0.15f * s, y, INK, 0);
            glyph_at(o, s, '0' + v % 10, em, x + 0.02f * s, y, INK, 0);
        } else glyph_at(o, s, '0' + v, em, x, y, INK, 0);
    }
}

/* the tiles' textures: a few sizes kept (the board, the rack, the loupe, the grid), each code made when needed */
#define TSIZES 5
static struct { int size, set; SDL_Texture *t[128]; Uint32 used; } TC[TSIZES];

static SDL_Texture *tile_tex(int code, int size)
{
    if (size < 4 || code <= 0 || code >= 128) return NULL;
    int k = 0;
    for (int i = 0; i < TSIZES; i++)
        if (TC[i].size == size && TC[i].set == P->set + 1) { k = i; goto found; }
    for (int i = 1; i < TSIZES; i++)            /* the size used least lately makes room */
        if (TC[i].used < TC[k].used) k = i;
    for (int i = 0; i < 128; i++) ui_free(&TC[k].t[i]);
    TC[k].size = size;
    TC[k].set = P->set + 1;
found:
    TC[k].used = ui_ms() + 1;
    if (!TC[k].t[code]) {
        tile_paint(code, size, ui_buf(size, size));
        TC[k].t[code] = ui_tex(size, size, ui_buf(size, size));
    }
    return TC[k].t[code];
}

static void tile_at(int code, float cx, float cy, float size, float alpha, int shadow)
{
    SDL_Texture *t = tile_tex(code, (int)(size + 0.5f));
    if (!t) return;
    if (shadow) ui_shadow_flat(cx, cy + 0.03f * size, size * 1.02f, size * 1.02f, 0.7f * alpha);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(alpha, 0, 1)));
    ui_put_c(t, cx, cy, size, size);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a gold rim round a tile (drawn under it): a tile placed this turn, a tile chosen */
static void tile_rim(float cx, float cy, float size)
{
    static struct { int size; SDL_Texture *t; } rim[3];
    static int next;
    int s = (int)(size * 1.14f + 0.5f), k;
    if (s < 4) return;
    for (k = 0; k < 3 && rim[k].size != s; k++) {}
    if (k == 3) {
        k = next++ % 3;
        static const uint8_t gold[4] = {236, 188, 70, 255};
        ui_free(&rim[k].t);
        art_roundrect(s, s, 0.2f * s, gold, ui_buf(s, s));
        rim[k].t = ui_tex(s, s, ui_buf(s, s));
        rim[k].size = s;
    }
    ui_put_c(rim[k].t, cx, cy, (float)s, (float)s);
}

/* ---- the board's picture ----------------------------------------------------------------------------------------- */
static const uint8_t PREM[6][3] = {{0, 0, 0}, {128, 176, 146}, {46, 118, 90}, {222, 162, 80}, {172, 58, 44},
                                   {222, 162, 80}};

/* a rectangle of a picture tinted (alpha a), its edges smoothed */
static void tint(uint8_t *o, int w, int h, float x0, float y0, float x1, float y1, const uint8_t rgb[3], float a)
{
    for (int y = (int)floorf(y0); y < (int)ceilf(y1); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x1); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float cx = fminf(x + 1.0f, x1) - fmaxf((float)x, x0), cy = fminf(y + 1.0f, y1) - fmaxf((float)y, y0);
            float k = a * clampf(cx, 0, 1) * clampf(cy, 0, 1);
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] + (rgb[c] - p[c]) * k);
        }
}

/* a premium square's marks: two or three, diamonds (a letter) or discs (a word); the centre's star */
static void marks(uint8_t *o, int n, int x, int y, int sq, int q)
{
    art_fig *f = art_fig_new();
    int k = q == WQ_DL || q == WQ_DW ? 2 : 3, letter = q == WQ_DL || q == WQ_TL;
    float r = 0.085f, gap = 0.27f;
    if (q == WQ_STAR) {
        float p[20];
        for (int i = 0; i < 10; i++) {
            float a = (-90 + 36 * i) * RACE_PI / 180, rr = i & 1 ? 0.17f : 0.4f;
            p[2 * i] = 0.5f + rr * cosf(a), p[2 * i + 1] = 0.53f + rr * sinf(a);
        }
        art_poly(f, ART_PART, p, 10);
    } else
        for (int i = 0; i < k; i++) {
            float mx = 0.5f + (i - (k - 1) / 2.0f) * gap;
            if (letter) {
                const float d[] = {mx, 0.5f - r * 1.25f, mx + r * 1.25f, 0.5f, mx, 0.5f + r * 1.25f, mx - r * 1.25f,
                                   0.5f};
                art_poly(f, ART_PART, d, 4);
            } else art_ellipse(f, ART_PART, mx, 0.5f, r, r);
        }
    static const uint8_t gold[3] = {240, 200, 96}, cream[3] = {250, 242, 222}, dark[3] = {70, 46, 30};
    art_style st = race_style(q == WQ_STAR ? gold : cream, dark, q == WQ_STAR ? 0.03f : 0.02f, 0.2f);
    uint8_t *b = calloc((size_t)sq * sq, 4);
    if (b) {
        art_render(f, &st, sq, sq, (float)sq, 0, 0, b);
        art_blend(o, n, n, b, sq, sq, x, y);
        free(b);
    }
    art_fig_free(f);
}

static void board_paint(int sq, int fr, uint8_t *o)
{
    int n = WT_N * sq + 2 * fr;
    art_board_n(WT_N, WT_N, sq, fr, 0, o);
    static const uint8_t line[3] = {70, 46, 30};
    for (int cell = 0; cell < WT_CELLS; cell++) {
        int q = wt_premium(cell);
        if (!q) continue;
        float x = fr + (float)(cell % WT_N) * sq, y = fr + (float)(cell / WT_N) * sq, e = 0.06f * sq;
        tint(o, n, n, x + e, y + e, x + sq - e, y + sq - e, PREM[q], 0.82f);
    }
    for (int i = 0; i <= WT_N; i++) {           /* the lines between the squares */
        float p = fr + (float)i * sq - 0.5f, wd = fmaxf(1, 0.04f * sq);
        tint(o, n, n, p, (float)fr, p + wd, (float)(n - fr), line, 0.45f);
        tint(o, n, n, (float)fr, p, (float)(n - fr), p + wd, line, 0.45f);
    }
    for (int cell = 0; cell < WT_CELLS; cell++)
        if (wt_premium(cell)) marks(o, n, fr + cell % WT_N * sq, fr + cell / WT_N * sq, sq, wt_premium(cell));
}

/* ---- the textures made for the layout ---------------------------------------------------------------------------- */
static SDL_Texture *t_board, *t_plank, *t_bag, *t_arrow, *t_disc;
static int made_board = -1, made_plank = -1, made_bag = -1, made_arrow = -1, made_disc = -1;
static const uint8_t CREAM[3] = {246, 238, 220}, DARK[3] = {52, 34, 22};

/* the buttons' pictures (play: the shell's tick) */
static void paint_knob(int kind, int s, uint8_t *o)
{
    art_fig *f = art_fig_new();
    switch (kind) {
    case K_RECALL:                              /* an arrow down to the rack */
        art_band(f, ART_PART, 0.5f, 0.24f, 0.5f, 0.56f, 0.09f);
        art_head(f, ART_PART, 0.5f, 0.6f, 90, 0.2f);
        art_rect(f, ART_PART, 0.27f, 0.7f, 0.73f, 0.77f);
        break;
    case K_SHUFFLE:                             /* two arrows crossing */
        art_band(f, ART_PART, 0.24f, 0.36f, 0.66f, 0.64f, 0.07f);
        art_band(f, ART_PART, 0.24f, 0.64f, 0.66f, 0.36f, 0.07f);
        art_head(f, ART_PART, 0.7f, 0.66f, 34, 0.17f);
        art_head(f, ART_PART, 0.7f, 0.34f, -34, 0.17f);
        break;
    case K_EXCHANGE:                            /* two arrows going round: tiles out, tiles in */
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.2f, 0.27f, 200, 340);
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.2f, 0.27f, 20, 160);
        art_head(f, ART_PART, 0.73f, 0.43f, 80, 0.17f);
        art_head(f, ART_PART, 0.27f, 0.57f, 260, 0.17f);
        break;
    default: {                                  /* the challenge: a red pennant on a pole */
        static const uint8_t red[3] = {196, 64, 48};
        static const float flag[] = {0.38f, 0.23f, 0.74f, 0.33f, 0.38f, 0.5f};
        art_rect(f, ART_PART, 0.33f, 0.2f, 0.38f, 0.8f);
        art_ellipse(f, ART_PART, 0.355f, 0.2f, 0.04f, 0.04f);
        art_fig_paint(o, s, f, CREAM, DARK, 0.01f, 0.1f);
        f = art_fig_new();
        art_poly(f, ART_PART, flag, 3);
        art_fig_paint(o, s, f, red, DARK, 0.015f, 0.1f);
        return;
    }
    }
    art_fig_paint(o, s, f, CREAM, NULL, 0, 0.1f);
}

/* the score's disc: turned walnut */
static void disc_paint(int s, uint8_t *o)
{
    static const uint8_t walnut[3] = {104, 70, 46};
    memset(o, 0, (size_t)s * s * 4);
    art_fig *f = art_fig_new();
    art_style st = art_flat(walnut, DARK, 0.035f, 0.35f);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.47f, 0.47f);
    art_render(f, &st, s, s, (float)s, 0, 0, o);
    art_fig_free(f);
}

static void bag_paint(int s, uint8_t *o)        /* a cloth bag tied at its neck */
{
    static const uint8_t cloth[3] = {196, 160, 112}, ink[3] = {96, 66, 40}, cord[3] = {150, 52, 40};
    memset(o, 0, (size_t)s * s * 4);
    art_fig *f = art_fig_new();
    static const float neck[] = {0.36f, 0.12f, 0.64f, 0.12f, 0.58f, 0.3f, 0.42f, 0.3f};
    art_poly(f, ART_PART, neck, 4);
    art_ellipse(f, ART_PART, 0.5f, 0.62f, 0.38f, 0.33f);
    art_style st = art_flat(cloth, ink, 0.03f, 0.4f);
    art_render(f, &st, s, s, (float)s, 0, 0, o);
    art_fig_free(f);
    f = art_fig_new();
    art_rect(f, ART_PART, 0.38f, 0.27f, 0.62f, 0.32f);
    art_fig_paint(o, s, f, cord, ink, 0.01f, 0.1f);
}

static void textures(void)
{
    int sq = (int)T.sq, fr = (int)T.fr, n = WT_N * sq + 2 * fr;
    if (made_board != n * 1000 + sq) {
        made_board = n * 1000 + sq;
        ui_free(&t_board);
        board_paint(sq, fr, ui_buf(n, n));
        t_board = ui_tex(n, n, ui_buf(n, n));
    }
    int pw = (int)T.pw, ph = (int)T.ph, key = pw * 10000 + ph;
    if (made_plank != key) {                    /* the rack: a plank of the house wood in its frame (stretched a
                                                 * little across to the plank's size) */
        made_plank = key;
        int f2 = (int)(0.09f * T.rs), nx = T.land ? 1 : 7, ny = T.land ? 7 : 1;
        int cell = ((T.land ? ph : pw) - 2 * f2) / 7, w = nx * cell + 2 * f2, h = ny * cell + 2 * f2;
        art_board_n(nx, ny, cell, f2, 0, ui_buf(w, h));
        ui_free(&t_plank);
        t_plank = ui_tex(w, h, ui_buf(w, h));
    }
    int bs = (int)(0.9f * T.kd);
    if (made_bag != bs) {
        made_bag = bs;
        ui_free(&t_bag);
        bag_paint(bs, ui_buf(bs, bs));
        t_bag = ui_tex(bs, bs, ui_buf(bs, bs));
    }
    int as = (int)(0.5f * T.sq);
    if (made_arrow != as && as > 3) {           /* the cursor's arrow */
        made_arrow = as;
        ui_free(&t_arrow);
        uint8_t *o = ui_buf(as, as);
        memset(o, 0, (size_t)as * as * 4);
        art_fig *f = art_fig_new();
        static const float tri[] = {0.2f, 0.12f, 0.88f, 0.5f, 0.2f, 0.88f};
        art_poly(f, ART_PART, tri, 3);
        art_fig_paint(o, as, f, CREAM, DARK, 0.06f, 0.1f);
        t_arrow = ui_tex(as, as, o);
    }
    int ds = (int)(0.66f * T.sq + 6);
    if (made_disc != ds) {
        made_disc = ds;
        ui_free(&t_disc);
        disc_paint(ds, ui_buf(ds, ds));
        t_disc = ui_tex(ds, ds, ui_buf(ds, ds));
    }
}

/* ---- the flying tiles -------------------------------------------------------------------------------------------- */
enum { H_NONE, H_CELL, H_SLOT };
#define FLIES 32
static struct { int code, hide, id, fade; float x0, y0, s0, x1, y1, s1; Uint32 t0; int ms, delay; } fly[FLIES];
static int nfly;
static Uint32 quiet, flash_t0;                  /* busy until quiet; the play's gold light since flash_t0 */
static int flash_cells[WT_RACK], nflash;

static void calm(void)
{
    nfly = 0;
    quiet = 0;
    nflash = 0;
}

static int fly_on(int i) { return ui_ms() < fly[i].t0 + (Uint32)(fly[i].delay + fly[i].ms); }

static void fly_add(int code, float x0, float y0, float s0, float x1, float y1, float s1, int ms, int delay, int hide,
                    int id, int fade)
{
    if (nfly >= FLIES) {                        /* the oldest landed ones make room */
        int k = 0;
        for (int i = 0; i < nfly; i++)
            if (fly_on(i)) fly[k++] = fly[i];
        nfly = k;
        if (nfly >= FLIES) return;
    }
    fly[nfly].code = code, fly[nfly].hide = hide, fly[nfly].id = id, fly[nfly].fade = fade;
    fly[nfly].x0 = x0, fly[nfly].y0 = y0, fly[nfly].s0 = s0;
    fly[nfly].x1 = x1, fly[nfly].y1 = y1, fly[nfly].s1 = s1;
    fly[nfly].t0 = ui_ms(), fly[nfly].ms = ms, fly[nfly].delay = delay;
    nfly++;
    Uint32 end = ui_ms() + (Uint32)(delay + ms + REST_MS);
    if (end > quiet) quiet = end;
}

static int hidden(int kind, int id)
{
    for (int i = 0; i < nfly; i++)
        if (fly[i].hide == kind && fly[i].id == id && fly_on(i)) return 1;
    return 0;
}

static void fly_draw(void)
{
    for (int i = 0; i < nfly; i++) {
        if (!fly_on(i)) continue;
        int t = (int)(ui_ms() - fly[i].t0) - fly[i].delay;
        float f = race_ease(clampf(t / (float)fly[i].ms, 0, 1)), s = fly[i].s0 + (fly[i].s1 - fly[i].s0) * f;
        float x = fly[i].x0 + (fly[i].x1 - fly[i].x0) * f, y = fly[i].y0 + (fly[i].y1 - fly[i].y0) * f;
        float a = fly[i].fade & 1 ? 1 - f * f : 1;
        if (t < 0 && fly[i].fade & 2) continue;                 /* still in the bag */
        tile_at(fly[i].code, x, y - race_lift(f) * s * 0.4f, s * (1 + 0.08f * sinf(RACE_PI * f)), a, 1);
        ui_animate();
    }
}

static int busy(void)
{
    for (int i = 0; i < nfly; i++)
        if (fly_on(i)) return 1;
    return (quiet && ui_ms() < quiet) || (nflash && ui_ms() < flash_t0 + FLASH_MS);
}

/* ---- the turn ---------------------------------------------------------------------------------------------------- */
static void clear_turn(void)
{
    npl = 0;
    sel = -1;
    cur = -1;
    exch = 0;
    pick.on = 0;
    fing.down = 0;
    fing.src = SRC_NONE;
}

static void start(int variant)
{
    (void)variant;
    wt_new(&G, wt_set_for(text_lang()), ui.players, (uint32_t)ui_random(1 << 30));
    hand_n = 0;
    clear_turn();
    calm();
}

static int status(void)
{
    if (P->phase != WP_OVER) return GS_PLAY;
    int w = wt_winner(P);
    return w < 0 ? GS_DRAW : gs_win(w);
}

static unsigned winners(void) { return wt_winners(P); }
static int turn(void) { return P->turn; }

/* the bag's place on the screen, and the top of the table (where a challenged play's tiles go) */
static void bag_xy(float *x, float *y) { *x = T.kx[K_BAG], *y = T.ky[K_BAG]; }

/* an action of the player to move, its tiles flying */
static void act(const wact *a)
{
    wpos before = G.pos;
    int me = P->turn;
    if (!wt_play(&G, a)) { ui_sound(SND_DENY); return; }
    calm();
    float bx, by;
    bag_xy(&bx, &by);
    switch (a->kind) {
    case WA_PLAY:                               /* the tiles settle; new ones come out of the bag */
        nflash = a->n;
        flash_t0 = ui_ms();
        for (int i = 0; i < a->n; i++) flash_cells[i] = a->cell[i];
        ui_sound(SND_PLACE);
        for (int i = 0, k = 0; i < a->n; i++) {
            int s = a->slot[i], t = P->rack[me][s];
            if (!t) continue;
            fly_add(t, bx, by, 0.5f * T.rs, T.rx[s], T.ry[s], T.rs, FLY_MS, 260 + DRAW_GAP * k, H_SLOT, s, 2);
            ui_sound_later(SND_MOVE, 260 + DRAW_GAP * k + FLY_MS - 40);
            k++;
        }
        if (P->phase == WP_FINAL) quiet = ui_ms() + FLASH_MS + REST_MS;
        break;
    case WA_EXCHANGE:                           /* the old tiles into the bag, the new ones out */
        for (int i = 0; i < a->n; i++) {
            int s = a->slot[i];
            fly_add(before.rack[me][s], T.rx[s], T.ry[s], T.rs, bx, by, 0.4f * T.rs, FLY_MS, 50 * i, H_NONE, 0, 1);
            fly_add(P->rack[me][s], bx, by, 0.5f * T.rs, T.rx[s], T.ry[s], T.rs, FLY_MS, FLY_MS + 120 + 70 * i,
                    H_SLOT, s, 2);
        }
        ui_sound(a->n ? SND_MOVE : SND_TAP);
        if (!a->n) quiet = ui_ms() + REST_MS;
        break;
    case WA_CHALLENGE:                          /* the play's tiles fly off the board, back to their owner */
        for (int i = 0; i < before.last.n; i++) {
            float x, y;
            int c = before.last.cell[i];
            cell_centre(c, &x, &y);
            fly_add(before.board[c], x, y, T.sq, T.b.x + T.b.w / 2, T.b.y - 0.4f * ui.unit, 0.8f * T.sq, 420,
                    60 * i, H_NONE, 0, 1);
        }
        ui_sound(SND_CAPTURE);
        break;
    case WA_ACCEPT:
        ui_sound(SND_TAP);
        break;
    }
    clear_turn();
    if (P->phase != WP_OVER && P->turn != me) {
        hand_n = G.n;
        ui_handoff(P->turn);
    }
}

static void play_now(void)
{
    if (exch & 128) {                           /* the exchange chosen */
        wact a;
        memset(&a, 0, sizeof a);
        a.kind = WA_EXCHANGE;
        for (int s = 0; s < WT_RACK; s++)
            if (exch >> s & 1) a.slot[a.n++] = (uint8_t)s;
        if (!a.n) { ui_sound(SND_DENY); return; }
        act(&a);
        return;
    }
    if (P->phase == WP_FINAL) {
        wact a = {WA_ACCEPT, 0, {0}, {0}, {0}};
        act(&a);
        return;
    }
    wact a;
    build(&a);
    if (!npl || wt_eval(P, &a, NULL) < 0) { ui_sound(SND_DENY); return; }
    act(&a);
}

/* a tile of the rack set on a square (a blank: its letter first) */
static void advance_cursor(void)
{
    if (cur < 0) return;
    int r = cur / WT_N, c = cur % WT_N;
    for (;;) {
        if (cur_down) r++;
        else c++;
        if (r >= WT_N || c >= WT_N) { cur = -1; return; }
        int cell = r * WT_N + c;
        if (!P->board[cell] && placed_cell(cell) < 0) { cur = cell; return; }
    }
}

static void put(int slot, int cell, int letter, float fx, float fy, float fs)
{
    if (npl >= WT_RACK) return;
    pl[npl].slot = slot, pl[npl].cell = cell, pl[npl].letter = letter;
    npl++;
    float x, y;
    cell_centre(cell, &x, &y);
    int code = P->rack[T.me][slot];
    if (code == WT_BLANK) code = WT_BLANK | letter;
    if (fs > 0) fly_add(code, fx, fy, fs, x, y, T.sq, 160, 0, H_CELL, cell, 0);
    ui_sound(SND_PLACE);
    if (cur == cell) advance_cursor();
}

static void place(int slot, int cell, float fx, float fy, float fs)
{
    if (slot < 0 || cell < 0 || P->board[cell] || placed_cell(cell) >= 0 || placed_slot(slot) >= 0 ||
        !P->rack[T.me][slot]) {
        ui_sound(SND_DENY);
        return;
    }
    sel = -1;
    if (P->rack[T.me][slot] == WT_BLANK) {      /* which letter: the grid */
        pick.on = 1;
        pick.slot = slot;
        pick.cell = cell;
        ui_sound(SND_TAP);
        return;
    }
    put(slot, cell, 0, fx, fy, fs);
}

static void unplace(int i, int fly_back)
{
    if (i < 0 || i >= npl) return;
    float x, y;
    cell_centre(pl[i].cell, &x, &y);
    int s = pl[i].slot, code = P->rack[T.me][s];
    if (fly_back) fly_add(code, x, y, T.sq, T.rx[s], T.ry[s], T.rs, FLY_MS, 0, H_SLOT, s, 0);
    memmove(&pl[i], &pl[i + 1], sizeof pl[0] * (size_t)(npl - i - 1));
    npl--;
}

static void recall(void)
{
    if (!npl) { ui_sound(SND_DENY); return; }
    while (npl) unplace(npl - 1, 1);
    ui_sound(SND_MOVE);
}

/* the rack in a new order: new slot i holds old slot perm[i]; the tiles fly to their new places */
static void arrange(const uint8_t perm[WT_RACK])
{
    uint8_t inv[WT_RACK];
    for (int i = 0; i < WT_RACK; i++) inv[perm[i]] = (uint8_t)i;
    for (int i = 0; i < WT_RACK; i++) {
        int o = perm[i], t = P->rack[T.me][o];
        if (t && placed_slot(o) < 0 && o != i)
            fly_add(t, T.rx[o], T.ry[o], T.rs, T.rx[i], T.ry[i], T.rs, 200, 0, H_SLOT, i, 0);
    }
    for (int k = 0; k < npl; k++) pl[k].slot = inv[pl[k].slot];
    if (sel >= 0) sel = inv[sel];
    int e = 0;
    for (int s = 0; s < WT_RACK; s++)
        if (exch >> s & 1) e |= 1 << inv[s];
    exch = (exch & 128) | e;
    wt_arrange(&G.pos, T.me, perm);
}

static void shuffle(void)
{
    uint8_t free_s[WT_RACK], perm[WT_RACK];
    int n = 0;
    for (int i = 0; i < WT_RACK; i++) {
        perm[i] = (uint8_t)i;
        if (placed_slot(i) < 0) free_s[n++] = (uint8_t)i;
    }
    if (n < 2) { ui_sound(SND_DENY); return; }
    uint8_t to[WT_RACK];
    memcpy(to, free_s, (size_t)n);
    for (int tries = 0; tries < 8; tries++) {   /* a new order (not the same again) */
        for (int i = n - 1; i > 0; i--) {
            int j = ui_random(i + 1);
            uint8_t t = to[i];
            to[i] = to[j];
            to[j] = t;
        }
        if (memcmp(to, free_s, (size_t)n)) break;
    }
    for (int i = 0; i < n; i++) perm[free_s[i]] = to[i];
    arrange(perm);
    ui_sound(SND_MOVE);
}

/* a tile of the rack moved along it, to slot `to` (the others make room) */
static void move_in_rack(int from, int to)
{
    if (from == to) return;
    uint8_t perm[WT_RACK];
    int k = 0, order[WT_RACK];
    for (int i = 0; i < WT_RACK; i++)
        if (i != from) order[k++] = i;
    memmove(&order[to + 1], &order[to], sizeof order[0] * (size_t)(WT_RACK - 1 - to));
    order[to] = from;
    for (int i = 0; i < WT_RACK; i++) perm[i] = (uint8_t)order[i];
    arrange(perm);
    ui_sound_soft(SND_MOVE, 0.5f);
}

/* ---- undo, the script -------------------------------------------------------------------------------------------- */
static int undo(void)
{
    if (pick.on) { pick.on = 0; return 1; }
    if (npl) { unplace(npl - 1, 1); return 1; }
    if (exch & 127) { exch &= 128; return 1; }
    if (G.n <= hand_n || !wt_undo(&G)) return 0;
    clear_turn();
    calm();
    return 1;
}

static int script(const char *s)
{
    int ok = wt_script(&G, s);
    clear_turn();
    calm();
    hand_n = G.n;
    ui_handoff(P->turn);
    return ok;
}

static int can_pass(void) { return mine() && P->phase == WP_PLAY && !npl && !(exch & 128); }

static void pass(void)
{
    wact a;
    memset(&a, 0, sizeof a);
    a.kind = WA_EXCHANGE;
    act(&a);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* the board drawn with square 0's top left at (ox, oy), squares sq wide: the wood, the tiles, the marks; `held`: a
 * tile shown at the target square (the loupe) */
static void draw_board(float ox, float oy, float sq, int target, int held)
{
    float k = sq / T.sq;
    ui_put(t_board, ox - T.fr * k, oy - T.fr * k, T.bs * k, T.bs * k);
    int last = P->last.kind == WA_PLAY && P->phase != WP_OVER;
    for (int cell = 0; cell < WT_CELLS; cell++) {
        int t = P->board[cell], i = placed_cell(cell);
        float x = ox + (cell % WT_N) * sq, y = oy + (cell / WT_N) * sq;
        if (i >= 0) {
            if (hidden(H_CELL, cell) || (fing.down && fing.src == SRC_BOARD && fing.moved && fing.idx == i)) continue;
            if (ui.viewer < 0) continue;        /* placed this turn, still the player's: not under the cover */
            int s = pl[i].slot, code = P->rack[T.me][s];
            if (code == WT_BLANK) code = WT_BLANK | pl[i].letter;
            tile_rim(x + sq / 2, y + sq / 2, sq * 0.9f);   /* placed this turn, not played yet: a gold rim */
            tile_at(code, x + sq / 2, y + sq / 2 - 0.03f * sq, sq * 0.94f, 1, 1);
            continue;
        }
        if (!t) continue;
        tile_at(t, x + sq / 2, y + sq / 2, sq * 0.96f, 1, 0);
        int lp = 0;
        for (int j = 0; last && j < P->last.n; j++) lp |= P->last.cell[j] == cell;
        if (lp) ui_mark_last(x, y, sq, sq);
    }
    if (cur >= 0 && mine()) {                   /* the cursor and its arrow */
        float x = ox + (cur % WT_N) * sq, y = oy + (cur / WT_N) * sq;
        ui_mark_select(x, y, sq, sq);
        if (t_arrow) ui_put_rot(t_arrow, x + sq / 2, y + sq / 2, 0.5f * sq, 0.5f * sq, cur_down ? 90 : 0);
    }
    if (target >= 0) {
        float x = ox + (target % WT_N) * sq, y = oy + (target / WT_N) * sq;
        ui_mark_select(x, y, sq, sq);
        if (held > 0) tile_at(held, x + sq / 2, y + sq / 2, sq * 0.96f, 0.8f, 1);
    }
}

/* the score the placed tiles would make: a walnut disc in a gold light past the end of the main word */
static void draw_preview(void)
{
    if (!npl || !mine()) return;
    wact a;
    wscore s;
    build(&a);
    if (wt_eval(P, &a, &s) < 0 || !s.words) return;
    int at = s.w[0].at, len = s.w[0].len, down = s.w[0].down, step = down ? WT_N : 1;
    float x, y, d = 0.66f * T.sq + 6;          /* a badge on the corner of the main word's last tile */
    cell_centre(at + (len - 1) * step, &x, &y);
    x += 0.48f * T.sq, y -= 0.48f * T.sq;
    ui_mark_gold(x, y, 1.5f * d);
    ui_put_c(t_disc, x, y, d, d);
    int digits = s.score >= 100 ? 3 : s.score >= 10 ? 2 : 1;
    ui_number(x, y, fminf(0.42f * d, 0.85f * d / (0.74f * digits)), s.score, 1);
}

/* the rack: the plank, the tiles not placed (the one held by the finger left out) */
static void draw_rack(void)
{
    if (t_plank) ui_put(t_plank, T.px, T.py, T.pw, T.ph);
    if (T.me < 0 || T.me >= P->players) return;
    int over = P->phase == WP_OVER, can = mine() && P->phase == WP_PLAY;
    for (int s = 0; s < WT_RACK; s++) {
        int t = P->rack[T.me][s];
        if (ui.viewer < 0) {                    /* the hand-off cover: every tile face down */
            if (t) tile_at(TILE_BACK, T.rx[s], T.ry[s], T.rs, 1, 1);
            continue;
        }
        if (!t || placed_slot(s) >= 0 || hidden(H_SLOT, s)) continue;
        if (fing.down && fing.src == SRC_RACK && fing.moved && fing.slot == s) continue;
        int up = sel == s || (exch >> s & 1 && exch & 128);
        float y = T.ry[s] - (up ? 0.16f * T.rs : 0);
        if (up) ui_mark_gold(T.rx[s], y, 1.5f * T.rs), tile_rim(T.rx[s], y, T.rs);
        tile_at(t, T.rx[s], y, T.rs, over || (!can && mine()) ? 0.6f : 1, 1);
    }
}

static void draw_knobs(void)
{
    float d = T.kd;
    int m = mine(), play = P->phase == WP_PLAY, ok = 0;
    if (m && play && npl && !(exch & 128)) {
        wact a;
        build(&a);
        ok = wt_eval(P, &a, NULL) >= 0;
    }
    int st[KNOBS] = {0};
    st[K_RECALL] = m && npl ? 0 : UI_DIM;
    st[K_SHUFFLE] = m && play ? 0 : UI_DIM;
    st[K_EXCHANGE] = exch & 128 ? UI_ON : m && play && wt_can_exchange(P) ? 0 : UI_DIM;
    st[K_CHALLENGE] = m && wt_can_challenge(P) ? UI_LIT : UI_DIM;
    st[K_PLAY] = !m ? UI_DIM : (exch & 128) ? (exch & 127 ? UI_LIT : UI_DIM)
                 : P->phase == WP_FINAL ? UI_LIT : ok ? UI_LIT : UI_DIM;
    for (int k = K_RECALL; k < KNOBS; k++) {
        if (k == K_PLAY) ui_button(T.kx[k], T.ky[k], d, ICON_TICK, st[k]);
        else ui_button_paint(T.kx[k], T.ky[k], d, paint_knob, k, 1, st[k]);
    }
    /* the bag and the tiles still in it */
    float bs = 0.9f * T.kd;
    ui_shadow_flat(T.kx[K_BAG], T.ky[K_BAG] + 0.1f * bs, bs * 0.8f, bs * 0.7f, 0.6f);
    ui_put_c(t_bag, T.kx[K_BAG], T.ky[K_BAG], bs, bs);
    ui_number(T.kx[K_BAG], T.ky[K_BAG] + 0.14f * bs, 0.3f * bs, P->nbag, 0);
}

/* the loupe: the squares round the finger, magnified, above it (below it near the top of the screen) */
static void draw_loupe(void)
{
    if (!fing.down || !mine() || pick.on) return;
    if (!(fing.src == SRC_CELL || (fing.moved && (fing.src == SRC_RACK || fing.src == SRC_BOARD)))) return;
    if (!on_board(fing.x, fing.y)) return;
    float z = LOUPE_Z, side = LOUPE_CELLS * T.sq * z, gap = 0.75f * T.rs;
    float cx = clampf(fing.x, side / 2 + 4, ui.w - side / 2 - 4), cy = fing.y - gap - side / 2;
    if (cy - side / 2 < 4) cy = fing.y + gap + side / 2;
    int target = cell_at(fing.x, fing.y), held = 0;
    if (target >= 0 && (P->board[target] || placed_cell(target) >= 0)) {
        if (!(fing.src == SRC_BOARD && placed_cell(target) == fing.idx)) held = -1;
    }
    if (held == 0 && fing.src == SRC_RACK) held = P->rack[T.me][fing.slot];
    if (held == 0 && fing.src == SRC_BOARD) {
        int s = pl[fing.idx].slot;
        held = P->rack[T.me][s] == WT_BLANK ? WT_BLANK | pl[fing.idx].letter : P->rack[T.me][s];
    }
    if (held == WT_BLANK) held = 0;             /* a blank: its letter comes after */
    float x0 = cx - side / 2, y0 = cy - side / 2, e = fmaxf(3, 0.08f * T.sq);
    ui_shadow_flat(cx + 0.03f * side, cy + 0.05f * side, side * 1.08f, side * 1.08f, 0.8f);
    ui_fill(x0 - e, y0 - e, side + 2 * e, side + 2 * e, 92, 60, 40, 255);   /* a walnut frame */
    ui_fill(x0 - e * 0.4f, y0 - e * 0.4f, side + 0.8f * e, side + 0.8f * e, 52, 34, 22, 255);
    SDL_Rect clip = {(int)x0, (int)y0, (int)side, (int)side};
    SDL_RenderSetClipRect(ui.r, &clip);
    ui_fill(x0, y0, side, side, 30, 60, 44, 255);
    draw_board(cx + (T.ox - fing.x) * z, cy + (T.oy - fing.y) * z, T.sq * z, target, held > 0 ? held : 0);
    SDL_RenderSetClipRect(ui.r, NULL);
}

/* the blank's letters: a grid of tiles on a dark panel */
static void pick_geom(int *cols, float *cell, float *x0, float *y0)
{
    const wset *s = wt_set(P->set);
    box b = T.b;
    *cols = T.land ? 8 : 6;
    if (s->n > 36) *cols = T.land ? 10 : 7;
    int rows = (s->n + *cols - 1) / *cols;
    *cell = floorf(fminf(fminf(b.w * 0.9f / *cols, b.h * 0.85f / rows), 1.25f * ui.unit));
    *x0 = b.x + (b.w - *cols * *cell) / 2;
    *y0 = b.y + (b.h - rows * *cell) / 2;
}

static int pick_at(float x, float y)
{
    int cols;
    float cell, x0, y0;
    pick_geom(&cols, &cell, &x0, &y0);
    int c = (int)floorf((x - x0) / cell), r = (int)floorf((y - y0) / cell), i = r * cols + c;
    return x >= x0 && y >= y0 && c < cols && i < wt_set(P->set)->n ? i + 1 : 0;
}

static void draw_pick(void)
{
    if (!pick.on) return;
    int cols;
    float cell, x0, y0;
    pick_geom(&cols, &cell, &x0, &y0);
    const wset *s = wt_set(P->set);
    int rows = (s->n + cols - 1) / cols;
    ui_fill(0, 0, (float)ui.w, (float)ui.h, 0, 0, 0, 110);
    float m = 0.25f * cell;
    ui_fill(x0 - m, y0 - m, cols * cell + 2 * m, rows * cell + 2 * m, 28, 34, 32, 245);
    for (int i = 0; i < s->n; i++)
        tile_at(WT_BLANK | (i + 1), x0 + (i % cols + 0.5f) * cell, y0 + (i / cols + 0.5f) * cell, 0.86f * cell, 1, 1);
}

static void draw_summary(void)
{
    box b = T.b;
    float u = ui.unit, rh = fminf(1.05f * u, b.h * 0.8f / P->players), pw = fminf(b.w * 0.92f, 7.6f * u);
    float ph = rh * P->players + 0.5f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 70);
    ct_panel(x0, y0, pw, ph, 250);
    unsigned w = wt_winners(P);
    for (int q = 0; q < P->players; q++) {      /* a row each: the token, the tiles left (lost or gained), the score */
        float cy = y0 + 0.25f * u + (q + 0.5f) * rh, tx = x0 + 0.75f * u, d = fminf(0.72f * rh, 0.8f * u);
        if (w >> q & 1) ui_mark_gold(tx, cy, 2.0f * d);
        ui_put_c(ct_token(q, (int)(0.6f * ui.unit)), tx, cy, d, d);
        int k = 0;                              /* the tiles left on the rack, small */
        for (int s = 0; s < WT_RACK; s++)
            if (P->rack[q][s]) tile_at(P->rack[q][s], x0 + 1.55f * u + k++ * 0.42f * u, cy, 0.38f * u, 1, 0);
        if (P->left[q]) ui_number_signed(x0 + pw * 0.62f, cy, 0.36f * rh, P->left[q], 1, 1);
        ui_number_signed(x0 + pw * 0.84f, cy, 0.5f * rh, P->score[q], 1, 0);
        if (q < P->players - 1)
            ct_rule(x0 + 0.3f * u, cy + rh / 2, pw - 0.6f * u);
    }
}

static void draw(void)
{
    T.me = ui_bottom();
    if (T.me < 0 || T.me >= P->players) T.me = P->turn;
    textures();
    int target = -1;
    if (fing.down && mine() && !pick.on && on_board(fing.x, fing.y) &&
        (fing.src == SRC_CELL || (fing.moved && fing.src != SRC_NONE)))
        target = cell_at(fing.x, fing.y);
    draw_board(T.ox, T.oy, T.sq, target, 0);
    if (exch & 128) ui_fill(T.bx, T.by, T.bs, T.bs, 0, 0, 0, 90);    /* exchanging: the board set aside */
    for (int i = 0; i < nflash && ui_ms() < flash_t0 + FLASH_MS; i++) {   /* a play settling */
        float x, y, f = (ui_ms() - flash_t0) / (float)FLASH_MS;
        cell_centre(flash_cells[i], &x, &y);
        ui_mark_gold(x, y, T.sq * (1.2f + 0.8f * f) * (1 - f));
        ui_animate();
    }
    draw_preview();
    draw_rack();
    draw_knobs();
    fly_draw();
    if (fing.down && fing.moved && (fing.src == SRC_RACK || fing.src == SRC_BOARD) && mine()) {
        int code = fing.src == SRC_RACK ? P->rack[T.me][fing.slot] : P->rack[T.me][pl[fing.idx].slot];
        if (fing.src == SRC_BOARD && code == WT_BLANK) code = WT_BLANK | pl[fing.idx].letter;
        float s = on_board(fing.x, fing.y) ? T.sq : T.rs;
        tile_at(code, fing.x, fing.y - (on_board(fing.x, fing.y) ? 0 : 0.3f * T.rs), s,
                on_board(fing.x, fing.y) ? 0.45f : 1, 1);
    }
    draw_loupe();
    if (ui.viewer >= 0) draw_pick();
    if (P->phase == WP_OVER && !busy()) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    fing.down = 1;
    fing.moved = 0;
    fing.src = SRC_NONE;
    fing.x0 = fing.x = x;
    fing.y0 = fing.y = y;
    if (P->phase == WP_OVER) return;
    if (!mine()) { ui_sound(SND_DENY); return; }
    if (pick.on) {                              /* the blank's letter */
        int l = pick_at(x, y);
        pick.on = 0;
        fing.down = 0;
        if (l) {
            float fx = T.rx[pick.slot], fy = T.ry[pick.slot];
            put(pick.slot, pick.cell, l, fx, fy, T.rs);
        } else ui_sound(SND_TAP);
        return;
    }
    for (int k = K_RECALL; k < KNOBS; k++) {
        if (!ui_button_hit(T.kx[k], T.ky[k], T.kd, x, y)) continue;
        fing.down = 0;
        switch (k) {
        case K_RECALL: recall(); break;
        case K_SHUFFLE: if (P->phase == WP_PLAY) shuffle(); else ui_sound(SND_DENY); break;
        case K_EXCHANGE:
            if (exch & 128) { exch = 0; ui_sound(SND_TAP); }
            else if (P->phase == WP_PLAY && wt_can_exchange(P)) {
                while (npl) unplace(npl - 1, 1);
                exch = 128;
                sel = cur = -1;
                ui_sound(SND_TAP);
            } else ui_sound(SND_DENY);
            break;
        case K_CHALLENGE: {
            wact a = {WA_CHALLENGE, 0, {0}, {0}, {0}};
            if (wt_can_challenge(P)) act(&a);
            else ui_sound(SND_DENY);
            break;
        }
        case K_PLAY: play_now(); break;
        }
        return;
    }
    if (P->phase != WP_PLAY) { ui_sound(SND_DENY); fing.down = 0; return; }
    int s = slot_at(x, y);
    if (s >= 0 && P->rack[T.me][s] && placed_slot(s) < 0) {
        fing.src = SRC_RACK;
        fing.slot = s;
        return;
    }
    if (on_board(x, y) && !(exch & 128)) {
        int c = cell_at(x, y), i = c >= 0 ? placed_cell(c) : -1;
        if (i >= 0) {
            fing.src = SRC_BOARD;
            fing.idx = i;
        } else fing.src = SRC_CELL;
        return;
    }
}

static void motion(float x, float y)
{
    if (!fing.down) return;
    fing.x = x;
    fing.y = y;
    if (!fing.moved && hypotf(x - fing.x0, y - fing.y0) > 0.18f * T.rs) fing.moved = 1;
    if (fing.src != SRC_NONE) ui_animate();
}

static void release(float x, float y)
{
    if (!fing.down) return;
    fing.down = 0;
    fing.x = x;
    fing.y = y;
    if (!mine() || P->phase != WP_PLAY) return;
    int src = fing.src, moved = fing.moved || hypotf(x - fing.x0, y - fing.y0) > 0.18f * T.rs;
    fing.src = SRC_NONE;
    int c = on_board(x, y) ? cell_at(x, y) : -1;
    if (src == SRC_RACK) {
        int s = fing.slot;
        if (!moved) {                           /* a tap on a tile of the rack */
            if (exch & 128) { exch ^= 1 << s; ui_sound(SND_TAP); }
            else if (cur >= 0) place(s, cur, T.rx[s], T.ry[s], T.rs);
            else { sel = sel == s ? -1 : s; ui_sound(SND_TAP); }
            return;
        }
        if (c >= 0 && !(exch & 128)) place(s, c, 0, 0, 0);
        else if (on_rack(x, y)) move_in_rack(s, slot_near(x, y));
        return;
    }
    if (src == SRC_BOARD) {
        int i = fing.idx;
        if (!moved) { unplace(i, 1); ui_sound(SND_MOVE); return; }
        if (c >= 0 && c != pl[i].cell && !P->board[c] && placed_cell(c) < 0) {
            pl[i].cell = c;                     /* moved to another square */
            ui_sound(SND_PLACE);
        } else if (c < 0) { unplace(i, 0); ui_sound(SND_MOVE); }
        return;
    }
    if (src == SRC_CELL) {
        if (c < 0) return;
        if (P->board[c]) { cur = -1; sel = -1; ui_sound(SND_DENY); return; }
        if (sel >= 0) { int s = sel; place(s, c, T.rx[s], T.ry[s], T.rs); return; }
        if (cur == c) cur_down = !cur_down;
        else cur = c;
        ui_sound(SND_TAP);
    }
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the score */
{
    int v = G.pos.score[player], digits = 1;
    for (int a = abs(v); a >= 10; a /= 10) digits++;
    float w = 0.74f * (digits - 1) + 0.62f + (v < 0 ? 0.56f : 0), h = fminf(0.46f * ui.unit, b.w / w * 0.95f);
    ui_number_signed(b.x + w * h / 2, b.y + b.h / 2, h, v, 1, 0);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's card: a corner of the board, a word across and one down through it */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    int sq = size / 6, fr = (size - 5 * sq) / 2, n = 5 * sq + 2 * fr;
    uint8_t *b = malloc((size_t)n * n * 4), *t = malloc((size_t)sq * sq * 4);
    if (!b || !t) { free(b); free(t); return; }
    memset(rgba, 0, (size_t)size * size * 4);
    art_board_n(5, 5, sq, fr, 0, b);
    for (int cell = 0; cell < 25; cell++) {     /* the squares round the centre star, as on the board */
        int q = wt_premium((5 + cell / 5) * WT_N + 5 + cell % 5), x = fr + cell % 5 * sq, y = fr + cell / 5 * sq;
        if (!q) continue;
        float e = 0.06f * sq;
        tint(b, n, n, x + e, y + e, x + sq - e, y + sq - e, PREM[q], 0.82f);
        marks(b, n, x, y, sq, q);
    }
    art_blend(rgba, size, size, b, n, n, (size - n) / 2, (size - n) / 2);
    int set = G.pos.set, ts = (int)(sq * 0.94f), o = (size - n) / 2 + fr + (sq - ts) / 2;
    G.pos.set = 0;                              /* English letters: WORDS across, TIDES down through its D */
    static const char across[] = "WORDS", down[] = "TIDES";
    for (int i = 0; i < 10; i++) {
        int dn = i >= 5, k = i % 5, l = 0;
        char c = dn ? down[k] : across[k];
        if (dn && k == 2) continue;
        for (int j = 0; j < wt_set(0)->n; j++)
            if (wt_set(0)->l[j].cp == (uint32_t)c) l = j + 1;
        tile_paint(l, ts, t);
        art_blend(rgba, size, size, t, ts, ts, o + (dn ? 3 : k) * sq, o + (dn ? k : 2) * sq);
    }
    G.pos.set = (uint8_t)set;
    free(t);
    free(b);
}

static const game wordtiles_game = {
    .id = "wordtiles", .order = 302, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(wordtiles_game)
