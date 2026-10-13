/*
 * The Marienbad game (Nim) in RetroStone Board: rows of matches with red heads on a plank of the house wood (the rules
 * and the notation: nim_rules.c, nim.h). Tap matches of one row to lift them (or drag along the row), tap a lifted one
 * to put it back, then the round check button under the rows takes them; a match of another row while some are
 * lifted: refused. The matches taken slide off to the taker's side. Coordinates: fractions of the board's side, from
 * its top left (turned round when the board is flipped).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dicecup.h"
#include "games/nim.h"
#include "shell.h"

#define FR 0.035f                               /* the walnut frame, of the board's side */
#define ROWS_Y0 0.11f                           /* the rows' band */
#define ROWS_Y1 0.80f
#define STEP 0.088f                             /* from one match to the next in a row */
#define BTN_Y 0.895f                            /* the check button */
#define BTN 0.105f
#define TAKE_MS 420                             /* a match taken sliding off */
#define TAKE_GAP 45
#define LIFT 0.03f                              /* a lifted match, how high */

static nim_game G;
static int variant, S, made = -1, made_m = -1;
static float bx, by;
static SDL_Texture *t_board, *t_match, *t_small;
static int sel_row = -1, drag = -1;             /* drag: what a drag along the row does (1 lift, 0 put back) */
static unsigned sel;                            /* the lifted matches (bits of sel_row) */
static struct { int on, row, player; unsigned mask; Uint32 t0; } anim;

static const uint8_t STICK[3] = {238, 208, 150}, STICK_INK[3] = {150, 108, 58};
static const uint8_t HEAD[3] = {196, 44, 32}, HEAD_INK[3] = {96, 20, 14};

/* ---- where things are ------------------------------------------------------------------------------------------- */
static float row_y(int r)
{
    float band = (ROWS_Y1 - ROWS_Y0) / G.rows;
    return ROWS_Y0 + band * (r + 0.5f);
}

static float match_len(void) { return fminf(0.15f, 0.8f * (ROWS_Y1 - ROWS_Y0) / G.rows); }

static float match_x(int r, int i) { return 0.5f + (i - (G.start[r] - 1) / 2.0f) * STEP; }

/* a point of the board (fractions) on the screen: turned round when flipped */
static float SX(float x) { return bx + (ui.flipped ? 1 - x : x) * S; }
static float SY(float y) { return by + (ui.flipped ? 1 - y : y) * S; }

/* the match under (x, y) pixels: its row and place; 0: none */
static int hit_match(float x, float y, int *row, int *idx)
{
    float fx = (x - bx) / S, fy = (y - by) / S, L = match_len();
    if (ui.flipped) fx = 1 - fx, fy = 1 - fy;
    for (int r = 0; r < G.rows; r++) {
        if (fabsf(fy - row_y(r)) > 0.5f * L + 0.02f) continue;
        for (int i = 0; i < G.start[r]; i++)
            if (fabsf(fx - match_x(r, i)) < STEP / 2) {
                *row = r;
                *idx = i;
                return 1;
            }
    }
    return 0;
}

static int on_button(float x, float y)
{
    float fx = (x - bx) / S, fy = (y - by) / S;
    if (ui.flipped) fx = 1 - fx, fy = 1 - fy;
    return hypotf(fx - 0.5f, fy - BTN_Y) < 0.6f * BTN + 0.01f;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void unselect(void)
{
    sel = 0;
    sel_row = -1;
    drag = -1;
}

static void start(int v)
{
    variant = v >= 0 && v < NIM_VARIANTS ? v : NIM_MARIENBAD;
    nim_new(&G, variant, (uint32_t)ui_random(1 << 30));
    unselect();
    anim.on = 0;
    made_m = -1;                                /* the match's size follows the rows */
}

static int status(void)
{
    int s = nim_status(&G);
    return s == NIM_PLAY ? GS_PLAY : gs_win(s - NIM_WIN0);
}

static int turn(void) { return G.turn; }

static int busy(void) { return anim.on && ui_ms() - anim.t0 < (Uint32)(TAKE_MS + TAKE_GAP * NIM_LONG); }

static void take(int row, unsigned mask)
{
    int p = G.turn;
    if (!nim_play(&G, row, (int)mask)) { ui_sound(SND_DENY); return; }
    anim.on = 1;
    anim.row = row;
    anim.mask = mask;
    anim.player = p;
    anim.t0 = ui_ms();
    unselect();
    ui_sound(SND_MOVE);
}

/* the bot's side (bots.h): the game, read-only; a move in the notation, played as the take button does */
const nim_game *nim_live(void) { return &G; }

int nim_show(const char *move)
{
    nim_game t = G;                             /* the notation read by the rules, on a copy: the matches it takes */
    if (status() != GS_PLAY || !nim_script(&t, move) || t.n != G.n + 1) return 0;
    take(t.move[G.n].row, t.move[G.n].mask);
    return 1;
}

static int undo(void)
{
    unselect();
    anim.on = 0;
    return nim_undo(&G);
}

static int script(const char *s)
{
    int ok = nim_script(&G, s);
    anim.on = 0;
    unselect();
    return ok;
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a match standing up in a picture w x h (its length h): the stick, the red head at the top */
static void match_pic(int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    float k = (float)h, ox = (w - k) / 2;
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, 0.455f, 0.10f, 0.545f, 0.985f);
    art_style st = art_flat(STICK, STICK_INK, 0.012f, 0.35f);
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) { art_fig_free(f); return; }
    art_render(f, &st, w, h, k, ox, 0, o);
    art_fig_free(f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.105f, 0.076f, 0.1f);
    st = art_flat(HEAD, HEAD_INK, 0.012f, 0.6f);
    art_render(f, &st, w, h, k, ox, 0, b);
    art_blend(o, w, h, b, w, h, 0, 0);
    art_fig_free(f);
    free(b);
}

/* the plank in its frame (n x n) */
static void board_pic(int n, uint8_t *o)
{
    int fr = (int)(FR * n);
    art_board_n(1, 1, n - 2 * fr, fr, 0, o);
}

static void layout(box b)
{
    S = (int)fminf(b.w, b.h);
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    if (made != S) {
        made = S;
        board_pic(S, ui_buf(S, S));
        ui_free(&t_board);
        t_board = ui_tex(S, S, ui_buf(S, S));
        int q = (int)(0.6f * ui.unit);
        match_pic(q, q, ui_buf(q, q));
        ui_free(&t_small);
        t_small = ui_tex(q, q, ui_buf(q, q));
    }
    int h = (int)(match_len() * S * 1.06f), w = h / 4;
    if (made_m != h) {
        made_m = h;
        match_pic(w, h, ui_buf(w, h));
        ui_free(&t_match);
        t_match = ui_tex(w, h, ui_buf(w, h));
    }
}

/* a match on the board at (x, y) (fractions), lifted by lift (fractions), faded by a */
static void match_at(float x, float y, float lift, float a)
{
    float L = match_len() * S, w = L / 4, cx = SX(x), cy = SY(y), up = ui.flipped ? -1 : 1;
    float k = 1 + 1.6f * lift;
    ui_shadow_flat(cx + (0.004f + 0.6f * lift) * S, cy + 0.6f * lift * S, 0.16f * L, 0.98f * L, 0.55f * a);
    SDL_SetTextureAlphaMod(t_match, (Uint8)(255 * a));
    ui_put_rot(t_match, cx, cy - up * lift * S, w * k, L * 1.06f * k, ui.flipped ? 180 : 0);
    SDL_SetTextureAlphaMod(t_match, 255);
}

static void draw(void)
{
    if (made_m != (int)(match_len() * S * 1.06f)) layout((box){bx, by, (float)S, (float)S});
    ui_put(t_board, bx, by, (float)S, (float)S);
    float L = match_len();
    Uint32 now = ui_ms();
    int moving = busy();
    if (moving) ui_animate();
    /* the last take: where its matches lay */
    if (G.n && !moving) {
        nim_move m = G.move[G.n - 1];
        for (int i = 0; i < G.start[m.row]; i++)
            if (m.mask >> i & 1) ui_mark_last_round(SX(match_x(m.row, i)), SY(row_y(m.row)), 0.55f * L * S);
    }
    for (int r = 0; r < G.rows; r++)
        for (int i = 0; i < G.start[r]; i++) {
            if (!(G.mask[r] >> i & 1)) continue;
            int up = r == sel_row && (sel >> i & 1);
            if (up) ui_mark_gold(SX(match_x(r, i)), SY(row_y(r) - LIFT), 0.9f * L * S);
            match_at(match_x(r, i), row_y(r), up ? LIFT : 0, 1);
        }
    if (moving) {                               /* the matches taken, sliding off to the taker's side */
        int bottom = anim.player == ui_bottom(), k = 0;
        for (int i = 0; i < G.start[anim.row]; i++) {
            if (!(anim.mask >> i & 1)) continue;
            float f = ((float)(int)(now - anim.t0) - TAKE_GAP * k++) / TAKE_MS;
            f = clampf(f, 0, 1);
            if (f >= 1) continue;
            float y0 = row_y(anim.row), y1 = ui.flipped ? (bottom ? 0.02f : 0.98f) : (bottom ? 0.98f : 0.02f);
            float y = cup_mix(y0, y1, f), lift = LIFT * (1 - f) + 0.02f * sinf(ART_PI * f);
            match_at(match_x(anim.row, i) + 0.02f * f * (i - 3), y, lift, 1 - f * f);
        }
    }
    /* the check button: lit while matches are lifted, turned to the player to move face to face */
    int lit = sel && status() == GS_PLAY && !moving;
    if (status() == GS_PLAY)
        cup_button_at(CUP_STOP, SX(0.5f), SY(BTN_Y), BTN * S, lit,
                      ui_upside(G.turn) ? 180 : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY || busy()) return;
    int r, i;
    if (on_button(x, y)) {
        if (sel) take(sel_row, sel);
        else ui_sound(SND_DENY);
        return;
    }
    if (!hit_match(x, y, &r, &i) || !(G.mask[r] >> i & 1)) { ui_sound(SND_DENY); return; }
    if (sel && r != sel_row) { ui_sound(SND_DENY); return; }   /* one row at a time */
    sel_row = r;
    sel ^= 1u << i;
    drag = (int)(sel >> i & 1);
    if (!sel) sel_row = -1;
    ui_sound(SND_TAP);
}

static void motion(float x, float y)
{
    int r, i;
    if (drag < 0 || !hit_match(x, y, &r, &i) || !(G.mask[r] >> i & 1)) return;
    if (sel_row >= 0 && r != sel_row) return;
    if (sel_row < 0) {
        if (!drag) return;
        sel_row = r;
    }
    unsigned was = sel;
    if (drag) sel |= 1u << i;
    else sel &= ~(1u << i);
    if (!sel) sel_row = -1;
    if (sel != was) ui_sound_soft(SND_TAP, 0.5f);
}

static void release(float x, float y)
{
    (void)x, (void)y;
    drag = -1;
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ui_bar_count(b, t_small, G.taken[player]); }   /* the matches taken */

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card and the picker's: the plank and its rows (a random layout: other rows, and a die); misere: the last
 * match in a red glow, normal play: in a gold light */
static void thumb(int v, int size, uint8_t *rgba)
{
    static const int marienbad[4] = {1, 3, 5, 7}, random[4] = {1, 4, 6, 0};
    int random_rows = v == NIM_RANDOM || v == NIM_RANDOM_NORMAL;
    const int *rows = random_rows ? random : marienbad;
    int nr = random_rows ? 3 : 4;
    board_pic(size, rgba);
    float L = 0.17f, step = 0.105f;
    if (v >= 0) {                               /* the last match, alone on the top row: lost or won */
        int d = (int)(0.3f * size);
        uint8_t *g = malloc((size_t)d * d * 4);
        if (g) {
            int misere = v == NIM_MARIENBAD || v == NIM_RANDOM;
            const uint8_t red[3] = {214, 52, 36}, gold[3] = {255, 204, 72};
            art_disc(d, misere ? red : gold, 0.85f, 1.2f, g);
            art_blend(rgba, size, size, g, d, d, size / 2 - d / 2, (int)(0.2f * size) - d / 2);
            free(g);
        }
    }
    int h = (int)(L * size), w = h / 4 + 1;
    uint8_t *m = malloc((size_t)w * h * 4);
    if (!m) return;
    uint8_t *sh = malloc((size_t)w * h * 4);
    if (!sh) { free(m); return; }
    match_pic(w, h, m);
    for (int i = 0; i < w * h * 4; i += 4) sh[i] = sh[i + 1] = sh[i + 2] = 40, sh[i + 3] = (uint8_t)(m[i + 3] * 0.3f);
    int off = size / 90 + 1;
    for (int r = 0; r < nr; r++)
        for (int i = 0; i < rows[r]; i++) {
            float cx = 0.5f + (i - (rows[r] - 1) / 2.0f) * step, cy = 0.2f + r * 0.215f;
            int x = (int)(cx * size) - w / 2, y = (int)(cy * size) - h / 2;
            art_blend(rgba, size, size, sh, w, h, x + off, y + off / 2);
            art_blend(rgba, size, size, m, w, h, x, y);
        }
    free(sh);
    free(m);
    if (random_rows) {
        int d = (int)(0.2f * size);
        art_die_blend(rgba, size, size, 0.8f * size, 0.82f * size, d, 0, 3, 14);
    }
}

static const game nim_game_def = {
    .id = "nim", .order = 255, .variants = NIM_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(nim_game_def)
