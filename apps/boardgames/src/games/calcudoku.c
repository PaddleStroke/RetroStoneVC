/*
 * Calcudoku in RetroStone Board, for one player: the house plank ruled in n x n cells, the cages' thick lines, each
 * cage's target and operation burned into its first cell (the signs drawn, not written), the player's digits in blue
 * ink, a row of wooden keys 1..n under the board and four buttons beside it (the rules: calcudoku_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell, then a key: its digit goes in (the same key again takes it out); or drag a key onto a cell. The cell's
 * row and column are lit faintly, its cage a little more, and every cell holding its digit warmly (a key tapped with
 * no empty cell picked lights its digit's cells instead). The buttons, top to bottom: the pencil (notes: the keys then
 * put small digits in the cell, or take them out), the eraser, the lamp (a hint: a wrong digit put right, else a cell
 * that reasoning finds, in gold light) and the eye (clashes shown: a digit twice in a line, a full cage missing its
 * target, in red; on at first). Undo takes back one action, all the way to the start. The bar: the moves. Solved: a
 * wave of gold light runs over the board from the last cell filled.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/calcudoku.h"
#include "games/puzzles.h"
#include "games/pzbench_ui.h"
#include "shell.h"

#define FRAME 0.035f                            /* the board's frame, a part of its side */

enum { T_NOTES, T_ERASE, T_HINT, T_CLASH, TOOLS };

static cdk_game G;
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
/* an operation's sign, ink on the wood: a unit square's figure */
static void sign_fig(art_fig *f, int op)
{
    const float w = 0.17f;
    switch (op) {
    case CDK_ADD:
        art_band(f, ART_PART, 0.14f, 0.5f, 0.86f, 0.5f, w);
        art_band(f, ART_PART, 0.5f, 0.14f, 0.5f, 0.86f, w);
        break;
    case CDK_SUB: art_band(f, ART_PART, 0.14f, 0.5f, 0.86f, 0.5f, w); break;
    case CDK_MUL:
        art_band(f, ART_PART, 0.22f, 0.22f, 0.78f, 0.78f, w);
        art_band(f, ART_PART, 0.78f, 0.22f, 0.22f, 0.78f, w);
        break;
    default:
        art_band(f, ART_PART, 0.14f, 0.5f, 0.86f, 0.5f, w);
        art_ellipse(f, ART_PART, 0.5f, 0.2f, 0.1f, 0.1f);
        art_ellipse(f, ART_PART, 0.5f, 0.8f, 0.1f, 0.1f);
    }
}

/* a cage's label in a picture: the target, then the sign, from (x, y) its top left, dh the digits' height */
static void label(uint8_t *o, int w, int h, const cdk_cage *k, float x, float y, float dh)
{
    int digits = 1;
    for (int v = k->target; v >= 10; v /= 10) digits++;
    float tw = 0.66f * dh * (digits - 1) + 0.62f * dh;
    puz_digits(o, w, h, k->target, x + tw / 2, y + dh / 2, dh, PUZ_INK, 1, 1);
    if (k->op == CDK_ONE) return;
    art_fig *f = art_fig_new();
    sign_fig(f, k->op);
    art_style st = art_flat(PUZ_INK, PUZ_INK, 0, 0);
    int s = (int)(0.8f * dh + 0.5f);
    art_fig_blend(o, w, h, f, &st, (int)(x + tw + 0.12f * dh), (int)(y + (dh - s) / 2), s, 1);
    art_fig_free(f);
}

/* the plank ruled: thin lines between the cells, thick ones round the cages, the labels */
static void paint_board(uint8_t *o, const cdk_puzzle *p, int c, int f)
{
    int n = p->n, w = n * c + 2 * f;
    art_board_n(n, n, c, f, 0, o);
    float thin = fmaxf(1.0f, c * 0.024f), thick = fmaxf(2.0f, c * 0.075f), lo = (float)f, hi = (float)(w - f);
    for (int i = 1; i < n; i++) {               /* thin */
        float at = f + i * c;
        puz_ink_rect(o, w, w, at - thin / 2, lo, at + thin / 2, hi, PUZ_INK, 0.35f);
        puz_ink_rect(o, w, w, lo, at - thin / 2, hi, at + thin / 2, PUZ_INK, 0.35f);
    }
    for (int y = 0; y < n; y++)                 /* thick, where two cages meet */
        for (int x = 0; x < n; x++) {
            int cell = y * n + x;
            if (x + 1 < n && p->cage_of[cell] != p->cage_of[cell + 1]) {
                float at = f + (x + 1) * c;
                puz_ink_rect(o, w, w, at - thick / 2, fmaxf(lo, f + y * c - thick / 2), at + thick / 2,
                             fminf(hi, f + (y + 1) * c + thick / 2), PUZ_INK, 0.9f);
            }
            if (y + 1 < n && p->cage_of[cell] != p->cage_of[cell + n]) {
                float at = f + (y + 1) * c;
                puz_ink_rect(o, w, w, fmaxf(lo, f + x * c - thick / 2), at - thick / 2,
                             fminf(hi, f + (x + 1) * c + thick / 2), at + thick / 2, PUZ_INK, 0.9f);
            }
        }
    puz_ink_rect(o, w, w, lo, lo, hi, lo + thick, PUZ_INK, 0.9f);   /* round the grid */
    puz_ink_rect(o, w, w, lo, hi - thick, hi, hi, PUZ_INK, 0.9f);
    puz_ink_rect(o, w, w, lo, lo, lo + thick, hi, PUZ_INK, 0.9f);
    puz_ink_rect(o, w, w, hi - thick, lo, hi, hi, PUZ_INK, 0.9f);
    for (int k = 0; k < p->ncages; k++) {
        int cell = p->cage[k].cell[0];
        float lx = f + (cell % n) * c + 0.07f * c + thick / 2, ly = f + (cell / n) * c + 0.06f * c + thick / 2;
        label(o, w, w, &p->cage[k], lx, ly, 0.25f * c);
    }
}

/* the token: two cells of a cage, its label (6 and the sign x) and a blue digit */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int f = (int)fmaxf(1, size * 0.07f), c = (size - 2 * f) / 2, w = 2 * c + 2 * f;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(2, 2, c, f, 0, b);
    float thick = fmaxf(1.5f, c * 0.09f);
    puz_ink_rect(b, w, w, (float)f, f + c - thick / 2, (float)(w - f), f + c + thick / 2, PUZ_INK, 0.9f);
    puz_ink_rect(b, w, w, f + c - thick / 2, (float)f, f + c + thick / 2, (float)(f + c), PUZ_INK, 0.4f);
    cdk_cage k = {CDK_MUL, 2, {0, 1}, 6};
    label(b, w, w, &k, f + 0.1f * c, f + 0.08f * c, 0.36f * c);
    puz_digits(b, w, w, 3, f + 1.5f * c, f + 1.55f * c, 0.7f * c, PZB_BLUE, 1, 0);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int n = G.p.n ? G.p.n : 4;
    pzb_pad_layout(&pad, b, n, TOOLS);
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
        puz_font_make(&f_entry, (int)(0.6f * q), 0);
        puz_font_make(&f_note, (int)fmaxf(6, 0.22f * q), 1);
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
    cdk_new(&G, variant < 0 || variant >= CDK_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    notes_mode = 0;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return cdk_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return pzb_wave_busy(&win); }
static int score(void) { return G.hist.moves; }   /* the end card: the moves it took */

static void after(int cell)                     /* an action done: solved by it, the wave from that cell */
{
    if (cdk_solved(&G) && !win.on) {
        pzb_wave_start(&win, (float)(cell % G.p.n), (float)(cell / G.p.n), (float)G.p.n);
        sel = -1;
        lit_digit = 0;
    }
    ui_animate();
}

static int undo(void)
{
    int c = cdk_undo(&G);
    if (c < 0) return 0;
    calm();
    sel = c;
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = cdk_script(&G, moves);
    if (cdk_solved(&G)) after(G.p.n * G.p.n / 2);
    return ok;
}

static int cells(void) { return G.p.n * G.p.n; }

static void enter(int d)
{
    if (sel < 0) {                              /* nothing picked: the key lights its digit's cells */
        lit_digit = lit_digit == d ? 0 : d;
        ui_sound(SND_TAP);
        return;
    }
    lit_digit = 0;
    if (notes_mode) {
        if (G.val[sel] || !cdk_note(&G, sel, d)) { ui_sound(SND_DENY); return; }
        ui_sound(SND_TAP);
        return;
    }
    if (G.val[sel] == d) {                      /* the same digit again: taken out */
        cdk_put(&G, sel, 0);
        ui_sound(SND_TAP);
        return;
    }
    cdk_put(&G, sel, d);
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
        if (sel < 0 || !cdk_erase(&G, sel)) { ui_sound(SND_DENY); break; }
        ui_sound(SND_TAP);
        break;
    default: {
        int c = cdk_hint(&G);
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
static void cell_xy(int c, float *x, float *y)  /* its top left corner */
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

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, N = cells(), solved = cdk_solved(&G), w = n * q + 2 * fr;
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, (float)w, (float)w);
    float x, y;
    int hd = sel >= 0 && G.val[sel] ? G.val[sel] : lit_digit;   /* the digit lit */
    if (!solved) {
        for (int c = 0; c < N; c++) {
            cell_xy(c, &x, &y);
            if (sel >= 0 && c != sel) {
                if (G.p.cage_of[c] == G.p.cage_of[sel]) ui_fill(x, y, (float)q, (float)q, 250, 214, 90, 52);
                else if (c / n == sel / n || c % n == sel % n) ui_fill(x, y, (float)q, (float)q, 250, 214, 90, 30);
            }
            if (hd && G.val[c] == hd && c != sel) ui_mark_last(x, y, (float)q, (float)q);
            if (show_clash && cdk_clash(&G, c)) ui_fill(x, y, (float)q, (float)q, 214, 60, 44, 56);
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
        float mx = x + q / 2.0f, my = y + 0.62f * q;
        pzb_wave_at(&win, (float)(c % n), (float)(c / n), mx, my, 2.3f * q);
        int v = G.val[c];
        if (v) {
            float k = 1;
            if (c == pop.cell) {
                float t = (now - pop.t0) / (float)PZB_POP_MS;
                if (t < 1) { k = 1 + 0.4f * (1 - puz_ease(t)); ui_animate(); }
            }
            const uint8_t *ink = show_clash && !solved && cdk_clash(&G, c) ? PZB_CLASH : PZB_BLUE;
            puz_number(&f_entry, mx, my, 0.5f * q * k, v, ink, 255);
        } else if (G.notes[c]) {
            for (int d = 1; d <= n; d++) {
                if (!(G.notes[c] >> d & 1)) continue;
                float nx = x + (0.26f + 0.24f * ((d - 1) % 3)) * q, ny = y + (0.46f + 0.2f * ((d - 1) / 3)) * q;
                if (d == hd) ui_mark_last(nx - 0.11f * q, ny - 0.1f * q, 0.22f * q, 0.2f * q);
                puz_number(&f_note, nx, ny, 0.2f * q, d, d == hd ? PZB_BLUE : PZB_NOTE, 255);
            }
        }
    }
    pzb_wave_tick(&win);
    unsigned faded = 0;
    for (int d = 1; d <= n; d++)
        if (cdk_count_digit(&G, d) >= n) faded |= 1u << d;
    pzb_keys(&pad, solved ? 0 : hd, faded, notes_mode);
    int erasable = sel >= 0 && (G.val[sel] || G.notes[sel]);
    static const int kinds[TOOLS] = {PZB_PENCIL, PZB_ERASER, -1, PZB_EYE};
    int how[TOOLS] = {notes_mode ? UI_ON : 0, erasable ? 0 : UI_DIM, solved ? UI_DIM : 0, show_clash ? UI_ON : 0};
    pzb_tools(&pad, kinds, how);
    float kx, ky;
    int d = pzb_carried(&pad, &kx, &ky);
    if (d) {                                    /* a key carried, above the finger */
        int c = cell_at(kx, ky);
        if (c >= 0) {
            cell_xy(c, &x, &y);
            ui_mark_select(x, y, (float)q, (float)q);
        }
        pzb_key(&pad, d, kx, ky, 1.1f, 255, notes_mode);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (cdk_solved(&G)) return;
    int c = cell_at(x, y), t = pzb_tool_at(&pad, x, y);
    if (c >= 0) {
        if (sel != c) ui_sound(SND_TAP);
        sel = c;
        lit_digit = 0;
        ui_animate();
        return;
    }
    if (pzb_pad_press(&pad, x, y)) return;      /* a key: on release (a tap), or carried to a cell */
    if (t >= 0) { tool(t); return; }
    sel = -1;                                   /* elsewhere: nothing picked */
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
    int c = cell_at(dx, dy);                    /* dropped: on the cell under the key */
    if (c < 0) c = cell_at(x, y);
    if (c < 0) { ui_sound(SND_DENY); return; }
    sel = c;
    enter(d);
}

static void bar(int player, box b)
{
    (void)player;
    ui_bar_number(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the size and level, the level's ebony stones under it (one:
 * easy .. three: hard); the menu's a 5 x 5 puzzle half done ------------------------------------------------------ */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int n = variant < 0 ? 5 : cdk_size_of(variant), level = variant < 0 ? CDK_MEDIUM : cdk_level_of(variant);
    cdk_puzzle p;
    cdk_make(n, level, 2026u + (uint32_t)(variant + 1), &p);
    int side = variant < 0 ? size : (int)(0.84f * size);
    int f = (int)(side * FRAME), c = (side - 2 * f) / n, w = n * c + 2 * f, o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_board(b, &p, c, f);
    if (variant < 0)
        for (int k = 0; k < n * n; k++)
            if (k % 3 != 1 && k < 15)
                puz_digits(b, w, w, p.sol[k], f + (k % n + 0.5f) * c, f + (k / n + 0.58f) * c, 0.56f * c, PZB_BLUE, 1,
                           0);
    art_blend(rgba, size, size, b, w, w, o, variant < 0 ? o : 0);
    free(b);
    if (variant >= 0) pzb_stones(rgba, size, w + (size - w - (int)(0.1f * size)) / 2, level);
}

static const game calcudoku_game = {
    .id = "calcudoku", .order = 445, .variants = CDK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
};
REGISTER_GAME(calcudoku_game)
