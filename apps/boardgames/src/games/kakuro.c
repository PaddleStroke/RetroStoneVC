/*
 * Kakuro in RetroStone Board, for one player: the house plank ruled in n x n squares, the dark ones stained walnut
 * with a diagonal across them, a run's clue burned in cream on its side of it (the run across in the upper right, the
 * run down in the lower left), the player's digits in blue ink, a row of wooden keys 1..9 under the board and four
 * buttons beside it (the rules: kakuro_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a white square, then a key: its digit goes in (the same key again takes it out); or drag a key onto a square.
 * The square's two runs are lit faintly, their clues in gold, and every square holding its digit warmly (a key tapped
 * with nothing picked lights its digit's squares instead). The buttons, top to bottom: the pencil (notes), the
 * eraser, the lamp (a hint: a wrong digit put right, else a square that reasoning finds, in gold light) and the eye
 * (clashes shown: a digit twice in a run, a run adding up past its clue or full and wrong, in red; on at first). Undo
 * takes back one action, all the way to the start. The bar: the moves. Solved: a wave of gold light runs over the
 * board from the last square filled.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/kakuro.h"
#include "games/puzzles.h"
#include "games/pzbench_ui.h"
#include "shell.h"

#define FRAME 0.035f

enum { T_NOTES, T_ERASE, T_HINT, T_CLASH, TOOLS };

static const uint8_t CLUE_INK[3] = {244, 230, 200};

static kkr_game G;
static int sel = -1, lit_digit, notes_mode, show_clash = 1, serial;
static struct { int cell; Uint32 t0; } pop = {-1, 0}, hint = {-1, 0};
static pzb_wave win;
static pzb_pad pad;

static box last_box;
static float bx, by;
static int q, fr, made_key = -1, made_small;
static SDL_Texture *t_board, *t_small;
static puz_font f_entry, f_note;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a dark square: the plank stained walnut, a lighter diagonal, its clues in cream (across up right, down low left) */
static void dark_square(uint8_t *o, int w, int x0, int y0, int c, int across, int down)
{
    for (int y = y0; y < y0 + c; y++)
        for (int x = x0; x < x0 + c; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            p[0] = (uint8_t)(p[0] * 0.36f);
            p[1] = (uint8_t)(p[1] * 0.27f);
            p[2] = (uint8_t)(p[2] * 0.22f);
        }
    float th = fmaxf(1.0f, c * 0.035f);
    if (across || down)
        for (int y = y0; y < y0 + c; y++)       /* the diagonal, from the top left down to the bottom right */
            for (int x = x0; x < x0 + c; x++) {
                float d = fabsf((x + 0.5f - x0) - (y + 0.5f - y0)) / 1.4142f, a = clampf(th / 2 + 0.5f - d, 0, 1);
                uint8_t *p = o + 4 * ((size_t)y * w + x);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] + (CLUE_INK[i] - p[i]) * a * 0.55f);
            }
    float dh = 0.3f * c;
    if (across) puz_digits(o, w, w, across, x0 + 0.68f * c, y0 + 0.3f * c, dh, CLUE_INK, 1, 1);
    if (down) puz_digits(o, w, w, down, x0 + 0.32f * c, y0 + 0.72f * c, dh, CLUE_INK, 1, 1);
}

static void paint_board(uint8_t *o, const kkr_puzzle *p, int c, int f)
{
    int n = p->n, w = n * c + 2 * f;
    art_board_n(n, n, c, f, 0, o);
    float thin = fmaxf(1.0f, c * 0.026f), lo = (float)f, hi = (float)(w - f);
    for (int i = 1; i < n; i++) {
        float at = f + i * c;
        puz_ink_rect(o, w, w, at - thin / 2, lo, at + thin / 2, hi, PUZ_INK, 0.45f);
        puz_ink_rect(o, w, w, lo, at - thin / 2, hi, at + thin / 2, PUZ_INK, 0.45f);
    }
    for (int k = 0; k < n * n; k++)
        if (!p->white[k]) dark_square(o, w, f + (k % n) * c, f + (k / n) * c, c, p->across[k], p->down[k]);
    for (int i = 1; i < n; i++) {               /* the lines again between dark squares: a seam */
        float at = f + i * c;
        for (int k = 0; k < n; k++) {
            if (!p->white[k * n + i - 1] && !p->white[k * n + i])
                puz_ink_rect(o, w, w, at - thin / 2, f + k * c, at + thin / 2, f + (k + 1) * c, PUZ_INK, 0.6f);
            if (!p->white[(i - 1) * n + k] && !p->white[i * n + k])
                puz_ink_rect(o, w, w, f + k * c, at - thin / 2, f + (k + 1) * c, at + thin / 2, PUZ_INK, 0.6f);
        }
    }
}

/* the token: a dark square with its clues, two white squares beside and under it, a blue digit */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int f = (int)fmaxf(1, size * 0.07f), c = (size - 2 * f) / 2, w = 2 * c + 2 * f;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(2, 2, c, f, 0, b);
    dark_square(b, w, f, f, c, 4, 9);
    dark_square(b, w, f + c, f + c, c, 0, 0);
    puz_digits(b, w, w, 3, f + 1.5f * c, f + 0.55f * c, 0.7f * c, PZB_BLUE, 1, 0);
    puz_digits(b, w, w, 8, f + 0.5f * c, f + 1.55f * c, 0.7f * c, PZB_BLUE, 1, 0);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int n = G.p.n ? G.p.n : 6;
    pzb_pad_layout(&pad, b, 9, TOOLS);
    fr = (int)(pad.gw * FRAME);
    q = (int)((pad.gw - 2 * fr) / n);
    int w = n * q + 2 * fr;
    bx = floorf(pad.gx + (pad.gw - w) / 2);
    by = floorf(pad.gy + (pad.gw - w) / 2);
    int key = (w * 16 + n) * 4096 + serial % 4096;
    if (made_key != key) {
        made_key = key;
        ui_free(&t_board);
        uint8_t *o = ui_buf(w, w);
        paint_board(o, &G.p, q, fr);
        t_board = ui_tex(w, w, o);
        puz_font_make(&f_entry, (int)(0.64f * q), 0);
        puz_font_make(&f_note, (int)fmaxf(6, 0.27f * q), 1);
    }
    pzb_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel = -1;
    lit_digit = 0;
    pop.cell = hint.cell = -1;
    win.on = 0;
    memset(&pad.drag, 0, sizeof pad.drag);
}

static void start(int variant)
{
    kkr_new(&G, variant < 0 || variant >= KKR_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    notes_mode = 0;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return kkr_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return pzb_wave_busy(&win); }
static int score(void) { return G.hist.moves; }   /* the end card: the moves it took */

static void after(int cell)
{
    if (kkr_solved(&G) && !win.on) {
        pzb_wave_start(&win, (float)(cell % G.p.n), (float)(cell / G.p.n), (float)G.p.n);
        sel = -1;
        lit_digit = 0;
    }
    ui_animate();
}

static int undo(void)
{
    int c = kkr_undo(&G);
    if (c < 0) return 0;
    calm();
    sel = c;
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = kkr_script(&G, moves);
    if (kkr_solved(&G)) after(G.p.n * G.p.n / 2);
    return ok;
}

static void enter(int d)
{
    if (sel < 0) {
        lit_digit = lit_digit == d ? 0 : d;
        ui_sound(SND_TAP);
        return;
    }
    lit_digit = 0;
    if (notes_mode) {
        if (G.val[sel] || !kkr_note(&G, sel, d)) { ui_sound(SND_DENY); return; }
        ui_sound(SND_TAP);
        return;
    }
    if (G.val[sel] == d) {
        kkr_put(&G, sel, 0);
        ui_sound(SND_TAP);
        return;
    }
    kkr_put(&G, sel, d);
    pop.cell = sel;
    pop.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after(sel);
}

static void tool(int i)
{
    switch (i) {
    case T_NOTES: notes_mode = !notes_mode; ui_sound(SND_TAP); break;
    case T_CLASH: show_clash = !show_clash; ui_sound(SND_TAP); break;
    case T_ERASE:
        if (sel < 0 || !kkr_erase(&G, sel)) { ui_sound(SND_DENY); break; }
        ui_sound(SND_TAP);
        break;
    default: {
        int c = kkr_hint(&G);
        if (c < 0) { ui_sound(SND_DENY); break; }
        sel = c;
        lit_digit = 0;
        hint.cell = pop.cell = c;
        hint.t0 = pop.t0 = ui_ms();
        ui_sound(SND_PLACE);
        after(c);
    }
    }
    ui_animate();
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)
{
    *x = bx + fr + (c % G.p.n) * q;
    *y = by + fr + (c / G.p.n) * q;
}

static int cell_at(float x, float y)
{
    int n = G.p.n;
    float ix = x - bx - fr, iy = y - by - fr;
    if (ix < 0 || iy < 0 || ix >= n * q || iy >= n * q) return -1;
    return (int)(iy / q) * n + (int)(ix / q);
}

static int in_run(int c, int r)
{
    const kkr_run *k = &G.run[r];
    for (int i = 0; i < k->len; i++)
        if (k->cell[i] == c) return 1;
    return 0;
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, N = n * n, solved = kkr_solved(&G), w = n * q + 2 * fr;
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, (float)w, (float)w);
    float x, y;
    int hd = sel >= 0 && G.val[sel] ? G.val[sel] : lit_digit;
    if (!solved) {
        if (sel >= 0)
            for (int t = 0; t < 2; t++) {       /* its two runs, their clues in gold */
                int r = G.run_of[sel][t];
                cell_xy(G.run[r].clue, &x, &y);
                ui_mark_gold_a(t ? x + 0.32f * q : x + 0.68f * q, t ? y + 0.72f * q : y + 0.3f * q, 0.9f * q, 0.8f);
            }
        for (int c = 0; c < N; c++) {
            if (!G.p.white[c]) continue;
            cell_xy(c, &x, &y);
            if (sel >= 0 && c != sel && (in_run(c, G.run_of[sel][0]) || in_run(c, G.run_of[sel][1])))
                ui_fill(x, y, (float)q, (float)q, 250, 214, 90, 40);
            if (hd && G.val[c] == hd && c != sel) ui_mark_last(x, y, (float)q, (float)q);
            if (show_clash && kkr_clash(&G, c)) ui_fill(x, y, (float)q, (float)q, 214, 60, 44, 56);
        }
        if (sel >= 0) {
            cell_xy(sel, &x, &y);
            ui_mark_select(x, y, (float)q, (float)q);
        }
    }
    if (hint.cell >= 0) {
        cell_xy(hint.cell, &x, &y);
        if (!pzb_hint_glow(hint.t0, x + q / 2.0f, y + q / 2.0f, 2.6f * q)) hint.cell = -1;
    }
    for (int c = 0; c < N; c++) {
        cell_xy(c, &x, &y);
        float mx = x + q / 2.0f, my = y + q / 2.0f;
        pzb_wave_at(&win, (float)(c % n), (float)(c / n), mx, my, 2.3f * q);
        if (!G.p.white[c]) continue;
        int v = G.val[c];
        if (v) {
            float k = 1;
            if (c == pop.cell) {
                float t = (now - pop.t0) / (float)PZB_POP_MS;
                if (t < 1) { k = 1 + 0.4f * (1 - puz_ease(t)); ui_animate(); }
            }
            const uint8_t *ink = show_clash && !solved && kkr_clash(&G, c) ? PZB_CLASH : PZB_BLUE;
            puz_number(&f_entry, mx, my + 0.02f * q, 0.64f * q * k, v, ink, 255);
        } else if (G.notes[c]) {
            for (int d = 1; d <= 9; d++) {
                if (!(G.notes[c] >> d & 1)) continue;
                float nx = mx + ((d - 1) % 3 - 1) * 0.31f * q, ny = my + ((d - 1) / 3 - 1) * 0.3f * q;
                if (d == hd) ui_mark_last(nx - 0.15f * q, ny - 0.15f * q, 0.3f * q, 0.3f * q);
                puz_number(&f_note, nx, ny, 0.27f * q, d, d == hd ? PZB_BLUE : PZB_NOTE, 255);
            }
        }
    }
    pzb_wave_tick(&win);
    pzb_keys(&pad, solved ? 0 : hd, 0, notes_mode);
    int erasable = sel >= 0 && (G.val[sel] || G.notes[sel]);
    static const int kinds[TOOLS] = {PZB_PENCIL, PZB_ERASER, -1, PZB_EYE};
    int how[TOOLS] = {notes_mode ? UI_ON : 0, erasable ? 0 : UI_DIM, solved ? UI_DIM : 0, show_clash ? UI_ON : 0};
    pzb_tools(&pad, kinds, how);
    float kx, ky;
    int d = pzb_carried(&pad, &kx, &ky);
    if (d) {
        int c = cell_at(kx, ky);
        if (c >= 0 && G.p.white[c]) {
            cell_xy(c, &x, &y);
            ui_mark_select(x, y, (float)q, (float)q);
        }
        pzb_key(&pad, d, kx, ky, 1.1f, 255, notes_mode);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (kkr_solved(&G)) return;
    int c = cell_at(x, y), t = pzb_tool_at(&pad, x, y);
    if (c >= 0) {
        if (!G.p.white[c]) {                    /* a dark square: nothing to pick */
            ui_sound(SND_DENY);
            sel = -1;
            ui_animate();
            return;
        }
        if (sel != c) ui_sound(SND_TAP);
        sel = c;
        lit_digit = 0;
        ui_animate();
        return;
    }
    if (pzb_pad_press(&pad, x, y)) return;
    if (t >= 0) { tool(t); return; }
    sel = -1;
    lit_digit = 0;
    ui_animate();
}

static void motion(float x, float y) { pzb_pad_motion(&pad, x, y); }

static void release(float x, float y)
{
    int tapped;
    float dx, dy;
    int d = pzb_pad_release(&pad, x, y, &tapped, &dx, &dy);
    if (!d) return;
    if (tapped) { enter(d); return; }
    int c = cell_at(dx, dy);
    if (c < 0 || !G.p.white[c]) c = cell_at(x, y);
    if (c < 0 || !G.p.white[c]) { ui_sound(SND_DENY); return; }
    sel = c;
    enter(d);
}

static void bar(int player, box b)
{
    (void)player;
    ui_bar_number(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the size and level, the level's ebony stones under it; the
 * menu's an 8 x 8 puzzle half done -------------------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int n = variant < 0 ? 8 : kkr_size_of(variant), level = variant < 0 ? KKR_MEDIUM : kkr_level_of(variant);
    kkr_puzzle p;
    kkr_make(n, level, 2026u + (uint32_t)(variant + 1), &p);
    int side = variant < 0 ? size : (int)(0.84f * size);
    int f = (int)(side * FRAME), c = (side - 2 * f) / n, w = n * c + 2 * f, o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_board(b, &p, c, f);
    if (variant < 0)
        for (int k = 0; k < n * n / 2; k++)
            if (p.white[k] && k % 3)
                puz_digits(b, w, w, p.sol[k], f + (k % n + 0.5f) * c, f + (k / n + 0.54f) * c, 0.62f * c, PZB_BLUE, 1,
                           0);
    art_blend(rgba, size, size, b, w, w, o, variant < 0 ? o : 0);
    free(b);
    if (variant >= 0) pzb_stones(rgba, size, w + (size - w - (int)(0.1f * size)) / 2, level);
}

static const game kakuro_game = {
    .id = "kakuro", .order = 446, .variants = KKR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
};
REGISTER_GAME(kakuro_game)
