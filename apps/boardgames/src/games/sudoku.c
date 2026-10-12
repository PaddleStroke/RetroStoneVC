/*
 * Sudoku in RetroStone Board, for one player: the house plank ruled in 9 x 9 cells, the boxes' thick lines, the
 * clues in dark ink and the player's digits in blue ink, a row of wooden keys 1..9 under the board and four walnut
 * buttons beside it (the rules: sudoku_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell, then a key: its digit goes in (the same key again takes it out); or drag a key onto a cell. The cell's
 * row, column and box are lit faintly, and every cell holding its digit warmly (a key tapped with no empty cell picked
 * lights its digit's cells instead). The buttons, top to bottom: the pencil (notes: the keys then put small digits in
 * the cell's corners, or take them out), the eraser (the cell's digit, or its notes), the lamp (a hint: a wrong digit
 * put right, else the easiest cell filled, in gold light) and the eye (clashes shown: a digit already in its row,
 * column or box turns red; on at first, as the house shows a king in check). A key whose nine digits are all on the
 * board fades. Undo takes back one action (a digit, a note, an erasure, a hint), all the way to the start.
 * The bar: the moves. Solved: a wave of gold light runs over the board from the last cell filled.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/sudoku.h"
#include "shell.h"

#define FRAME 0.035f                            /* the board's frame, a part of its side */
#define POP_MS 200                              /* a digit set down: it settles from a little larger */
#define HINT_MS 1500
#define WAVE_STEP 55                            /* the win's wave: ms a cell further on */
#define WAVE_LEN 560
#define WAVE_END (12 * WAVE_STEP + WAVE_LEN)

static const uint8_t GIVEN[3] = {40, 34, 30}, ENTRY[3] = {30, 64, 140}, NOTE[3] = {92, 70, 50},
                     CLASH[3] = {184, 44, 34};

enum { T_NOTES, T_ERASE, T_HINT, T_CLASH, TOOLS };

static su_game G;
static int sel = -1;                            /* the cell picked */
static int lit_digit;                           /* a key tapped with no empty cell picked: its digit's cells lit */
static int notes_mode, show_clash = 1;
static struct { int cell; Uint32 t0; } pop = {-1, 0}, hint = {-1, 0};
static struct { int on, from; Uint32 t0; } win;
static struct { int on, key, moved; float x, y, x0, y0; } ptr;

static box last_box;
static float gx, gy, gw, kx0, ky, kp, kw, kh, tx, ty[TOOLS], td;
static int cs, fr, made_w = -1, made_small = -1;
static SDL_Texture *t_board, *t_key, *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank ruled: thin lines between the cells, thick ones round the boxes */
static void paint_board(uint8_t *o, int q, int f)
{
    int w = 9 * q + 2 * f;
    art_board_n(9, 9, q, f, 0, o);
    for (int i = 0; i <= 9; i++) {
        int thick = i % 3 == 0;
        float th = thick ? fmaxf(2.0f, q * 0.07f) : fmaxf(1.0f, q * 0.026f), a = thick ? 0.88f : 0.5f;
        float p = f + i * q, lo = (float)f, hi = (float)(w - f);
        float c0 = fmaxf(lo, p - th / 2), c1 = fminf(hi, p + th / 2);
        if (i == 0) c1 = lo + th, c0 = lo;
        if (i == 9) c0 = hi - th, c1 = hi;
        puz_ink_rect(o, w, w, c0, lo, c1, hi, PUZ_INK, a);
        puz_ink_rect(o, w, w, lo, c0, hi, c1, PUZ_INK, a);
    }
}

/* the buttons' pictures (the hint is the shell's lamp) */
static void paint_tool(int kind, int size, uint8_t *o)
{
    static const uint8_t red[3] = {196, 64, 48}, dark[3] = {52, 34, 22}, gold[3] = {232, 190, 92};
    art_fig *f = art_fig_new();
    switch (kind) {
    case T_NOTES: {                             /* a pencil */
        art_band(f, ART_PART, 0.40f, 0.60f, 0.72f, 0.28f, 0.15f);
        art_fig_paint(o, size, f, PUZ_CREAM, dark, 0.02f, 0.1f);
        f = art_fig_new();
        const float tip[] = {0.347f, 0.547f, 0.453f, 0.653f, 0.27f, 0.73f};
        art_poly(f, ART_PART, tip, 3);
        art_fig_paint(o, size, f, gold, dark, 0.02f, 0.1f);
        f = art_fig_new();
        const float lead[] = {0.30f, 0.66f, 0.34f, 0.70f, 0.27f, 0.73f};
        art_poly(f, ART_PART, lead, 3);
        art_fig_paint(o, size, f, dark, dark, 0.01f, 0.1f);
        return;
    }
    case T_ERASE:                               /* an eraser, half worn */
        art_band(f, ART_PART, 0.30f, 0.62f, 0.50f, 0.46f, 0.26f);
        art_fig_paint(o, size, f, red, dark, 0.02f, 0.1f);
        f = art_fig_new();
        art_band(f, ART_PART, 0.50f, 0.46f, 0.72f, 0.29f, 0.26f);
        art_fig_paint(o, size, f, PUZ_CREAM, dark, 0.02f, 0.1f);
        return;
    default: {                                  /* an eye: the clashes seen */
        float lens[2 * 26];
        for (int i = 0; i <= 12; i++) {
            float t = i / 12.0f;
            lens[2 * i] = 0.2f + 0.6f * t;
            lens[2 * i + 1] = 0.5f - 0.2f * sinf(PUZ_PI * t);
            lens[2 * (13 + i)] = 0.8f - 0.6f * t;
            lens[2 * (13 + i) + 1] = 0.5f + 0.2f * sinf(PUZ_PI * t);
        }
        art_poly(f, ART_PART, lens, 26);
        art_fig_paint(o, size, f, PUZ_CREAM, dark, 0.02f, 0.1f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.13f, 0.13f);
        art_fig_paint(o, size, f, red, dark, 0.015f, 0.1f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.055f, 0.055f);
        art_fig_paint(o, size, f, dark, dark, 0.0f, 0.1f);
        return;
    }
    }
}

/* a key: a wooden tile, bevelled */
static void paint_key(int w, int h, uint8_t *o)
{
    int q = w > h ? w : h;
    uint8_t *plank = malloc((size_t)q * q * 4);
    if (!plank) return;
    art_board_n(1, 1, q, 0, 0, plank);
    for (int y = 0; y < h; y++) memcpy(o + 4 * y * w, plank + 4 * y * q, (size_t)w * 4);
    free(plank);
    puz_bevel(o, w, h, 0.14f * w, fmaxf(2, 0.09f * w), 0.32f);
}

/* the token: a corner of a grid, a clue in dark ink and two digits in blue */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int f = (int)fmaxf(1, size * 0.07f), q = (size - 2 * f) / 3, w = 3 * q + 2 * f;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(3, 3, q, f, 0, b);
    for (int i = 1; i < 3; i++) {
        float p = f + i * q, th = fmaxf(1, q * 0.05f);
        puz_ink_rect(b, w, w, p - th / 2, (float)f, p + th / 2, (float)(w - f), PUZ_INK, 0.55f);
        puz_ink_rect(b, w, w, (float)f, p - th / 2, (float)(w - f), p + th / 2, PUZ_INK, 0.55f);
    }
    static const int cells[3][3] = {{0, 0, 1}, {1, 1, 5}, {2, 2, 9}};   /* x, y, digit */
    for (int i = 0; i < 3; i++)
        puz_digits(b, w, w, cells[i][2], f + (cells[i][0] + 0.5f) * q, f + (cells[i][1] + 0.52f) * q, 0.68f * q,
                   i == 1 ? GIVEN : ENTRY, 1, i == 1);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    float S = fminf(b.w, b.h), u = ui.unit;
    kh = floorf(0.98f * u);
    kw = floorf(0.9f * u);
    kp = S / 9;
    float avail = S - kh - 0.24f * u;
    fr = (int)(avail * FRAME);
    cs = (int)((avail - 2 * fr) / 9);
    gw = (float)(9 * cs + 2 * fr);
    gx = floorf(b.x + (b.w - S) / 2);
    gy = floorf(b.y + (b.h - S) / 2);
    td = floorf(fminf(0.92f * u, 0.9f * (S - gw)));
    tx = gx + gw + (S - gw) / 2;
    for (int i = 0; i < TOOLS; i++) ty[i] = gy + gw * (i + 0.5f) / TOOLS;
    kx0 = gx + kp / 2;
    ky = gy + S - kh / 2;
    if (made_w != (int)gw) {
        made_w = (int)gw;
        int w = (int)gw;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), cs, fr);
        t_board = ui_tex(w, w, ui_buf(w, w));
        int iw = (int)kw, ih = (int)kh;
        ui_free(&t_key);
        memset(ui_buf(iw, ih), 0, (size_t)iw * ih * 4);
        paint_key(iw, ih, ui_buf(iw, ih));
        t_key = ui_tex(iw, ih, ui_buf(iw, ih));
    }
    int small = (int)(0.6f * u);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        token(0, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel = -1;
    lit_digit = 0;
    pop.cell = hint.cell = -1;
    win.on = 0;
    memset(&ptr, 0, sizeof ptr);
}

static void start(int variant)
{
    su_new(&G, variant < 0 || variant >= SU_VARIANTS ? SU_EASY : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    calm();
    notes_mode = 0;
}

static int status(void) { return su_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return win.on && ui_ms() - win.t0 < (Uint32)WAVE_END; }

static void after(int cell)                     /* an action done: solved by it, the wave from that cell */
{
    if (su_solved(&G) && !win.on) {
        win.on = 1;
        win.from = cell;
        win.t0 = ui_ms();
        sel = -1;
        lit_digit = 0;
    }
    ui_animate();
}

static int undo(void)
{
    int c = su_undo(&G);
    if (c < 0) return 0;
    calm();
    sel = c;
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = su_script(&G, moves);
    if (su_solved(&G)) after(40);
    return ok;
}

static int editable(int c) { return c >= 0 && !G.given[c]; }

static void enter(int d)
{
    if (!editable(sel)) {                       /* nothing to fill: the key lights its digit's cells */
        lit_digit = lit_digit == d ? 0 : d;
        if (sel >= 0) sel = -1;
        ui_sound(SND_TAP);
        return;
    }
    lit_digit = 0;
    if (notes_mode) {
        if (G.val[sel] || !su_note(&G, sel, d)) { ui_sound(SND_DENY); return; }
        ui_sound(SND_TAP);
        return;
    }
    if (G.val[sel] == d) {                      /* the same digit again: taken out */
        su_put(&G, sel, 0);
        ui_sound(SND_TAP);
        return;
    }
    su_put(&G, sel, d);
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
        if (!editable(sel) || !su_erase(&G, sel)) { ui_sound(SND_DENY); break; }
        ui_sound(SND_TAP);
        break;
    default: {
        int c = su_hint(&G);
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
    *x = gx + fr + (c % 9) * cs;
    *y = gy + fr + (c / 9) * cs;
}

static int peers(int a, int b)
{
    return a / 9 == b / 9 || a % 9 == b % 9 || (a / 27 == b / 27 && a % 9 / 3 == b % 9 / 3);
}

static int cell_at(float x, float y)
{
    float ix = x - gx - fr, iy = y - gy - fr;
    if (ix < 0 || iy < 0 || ix >= 9 * cs || iy >= 9 * cs) return -1;
    return (int)(iy / cs) * 9 + (int)(ix / cs);
}

static int key_at(float x, float y)
{
    if (fabsf(y - ky) > 0.62f * kh || x < gx || x >= gx + 9 * kp) return 0;
    return (int)((x - gx) / kp) + 1;
}

static int tool_at(float x, float y)
{
    for (int i = 0; i < TOOLS; i++)
        if (ui_button_hit(tx, ty[i], td, x, y)) return i;
    return -1;
}

static void draw_key(int d, float x, float y, float s, Uint8 a)
{
    SDL_SetTextureAlphaMod(t_key, a);
    ui_shadow_flat(x, y, kw * s, kh * s, a / 255.0f * 0.8f);
    ui_put_c(t_key, x, y, kw * s, kh * s);
    SDL_SetTextureAlphaMod(t_key, 255);
    if (notes_mode) {                           /* notes: the digit small, in its corner of the cell */
        float nx = x + ((d - 1) % 3 - 1) * 0.26f * kw * s, ny = y + ((d - 1) / 3 - 1) * 0.25f * kh * s;
        ui_number_ex(nx, ny, 0.36f * kh * s, d, PUZ_INK, a, UI_NUM_TIGHT);
    } else {
        ui_number_ex(x, y, 0.58f * kh * s, d, PUZ_INK, a, UI_NUM_TIGHT);
    }
}

static void draw(void)
{
    if (made_w < 0) layout(last_box);
    Uint32 now = ui_ms();
    int solved = su_solved(&G);
    ui_put(t_board, gx, gy, gw, gw);
    float x, y;
    int hd = sel >= 0 && G.val[sel] ? G.val[sel] : lit_digit;   /* the digit lit */
    if (!solved) {
        for (int c = 0; c < SU_CELLS; c++) {
            cell_xy(c, &x, &y);
            if (sel >= 0 && c != sel && peers(c, sel)) ui_fill(x, y, (float)cs, (float)cs, 250, 214, 90, 38);
            if (hd && G.val[c] == hd && c != sel) ui_mark_last(x, y, (float)cs, (float)cs);
            if (show_clash && su_clash(&G, c)) ui_fill(x, y, (float)cs, (float)cs, 214, 60, 44, 56);
        }
        if (sel >= 0) {
            cell_xy(sel, &x, &y);
            ui_mark_select(x, y, (float)cs, (float)cs);
        }
    }
    if (hint.cell >= 0) {                       /* the hint's cell: a gold light, swelling and fading */
        float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
        if (p > 0) {
            cell_xy(hint.cell, &x, &y);
            ui_mark_gold(x + cs / 2.0f, y + cs / 2.0f, 2.6f * cs * p);
            ui_animate();
        } else {
            hint.cell = -1;
        }
    }
    float wt = win.on ? (float)(now - win.t0) : -1;
    for (int c = 0; c < SU_CELLS; c++) {
        cell_xy(c, &x, &y);
        float mx = x + cs / 2.0f, my = y + cs / 2.0f;
        if (wt >= 0) {                          /* the win: the wave of light */
            float dist = hypotf((float)(c % 9 - win.from % 9), (float)(c / 9 - win.from / 9));
            float p = puz_pulse(wt, dist * WAVE_STEP, WAVE_LEN);
            if (p > 0) ui_mark_gold(mx, my, 2.3f * cs * p);
        }
        int v = G.val[c];
        if (v) {
            float k = 1;
            if (c == pop.cell) {
                float t = (now - pop.t0) / (float)POP_MS;
                if (t < 1) { k = 1 + 0.4f * (1 - puz_ease(t)); ui_animate(); }
            }
            const uint8_t *ink = G.given[c] ? GIVEN : show_clash && !solved && su_clash(&G, c) ? CLASH : ENTRY;
            ui_number_ex(mx, my + 0.02f * cs, 0.64f * cs * k, v, ink, 255, G.given[c] ? UI_NUM_TIGHT : UI_NUM_TIGHT | UI_NUM_REGULAR);
        } else if (G.notes[c]) {
            for (int d = 1; d <= 9; d++) {
                if (!(G.notes[c] >> d & 1)) continue;
                float nx = mx + ((d - 1) % 3 - 1) * 0.31f * cs, ny = my + ((d - 1) / 3 - 1) * 0.30f * cs;
                if (d == hd) ui_mark_last(nx - 0.15f * cs, ny - 0.15f * cs, 0.3f * cs, 0.3f * cs);
                ui_number_ex(nx, ny, 0.27f * cs, d, d == hd ? ENTRY : NOTE, 255, UI_NUM_TIGHT);
            }
        }
    }
    if (wt >= 0) {
        if (wt < WAVE_END) ui_animate();
        else win.on = 0;
    }
    for (int d = 1; d <= 9; d++) {              /* the keys */
        float kx = kx0 + (d - 1) * kp;
        int full = su_count_digit(&G, d) >= 9;
        if (d == hd && !solved) ui_mark_gold(kx, ky, 1.7f * kw);
        int held = ptr.on && ptr.key == d && ptr.moved;
        draw_key(d, kx, ky, 1, (Uint8)(held ? 90 : full ? 105 : 255));
    }
    int erasable = editable(sel) && (G.val[sel] || G.notes[sel]);
    int states[TOOLS] = {notes_mode ? UI_ON : 0, erasable ? 0 : UI_DIM, solved ? UI_DIM : 0, show_clash ? UI_ON : 0};
    for (int i = 0; i < TOOLS; i++) {
        if (i == T_HINT) ui_button(tx, ty[i], td, ICON_HINT, states[i]);
        else ui_button_paint(tx, ty[i], td, paint_tool, i, 1, states[i]);
    }
    if (ptr.on && ptr.key && ptr.moved) {       /* a key carried, above the finger */
        int c = cell_at(ptr.x, ptr.y - 0.6f * kh);
        if (editable(c)) {
            cell_xy(c, &x, &y);
            ui_mark_select(x, y, (float)cs, (float)cs);
        }
        draw_key(ptr.key, ptr.x, ptr.y - 0.6f * kh, 1.1f, 255);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (su_solved(&G)) return;
    int c = cell_at(x, y), d = key_at(x, y), t = tool_at(x, y);
    if (c >= 0) {
        if (sel != c) ui_sound(SND_TAP);
        sel = c;
        lit_digit = 0;
        ui_animate();
        return;
    }
    if (d) {                                    /* a key: on release (a tap), or carried to a cell */
        ptr.on = 1;
        ptr.key = d;
        ptr.x = ptr.x0 = x;
        ptr.y = ptr.y0 = y;
        return;
    }
    if (t >= 0) { tool(t); return; }
    sel = -1;                                   /* elsewhere: nothing picked */
    lit_digit = 0;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    ptr.x = x;
    ptr.y = y;
    if (hypotf(x - ptr.x0, y - ptr.y0) > 0.3f * kw) ptr.moved = 1;
    ui_animate();
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    if (!ptr.moved) {
        if (key_at(x, y) == ptr.key) enter(ptr.key);
        return;
    }
    int c = cell_at(x, y - 0.6f * kh);          /* dropped: on the cell under the key */
    if (c < 0) c = cell_at(x, y);
    if (!editable(c)) { if (c >= 0) ui_sound(SND_DENY); return; }
    sel = c;
    enter(ptr.key);
}

/* ---- the bar: the moves ------------------------------------------------------------------------------------------ */
static void bar(int player, box b)
{
    (void)player;
    ui_bar_number(b, t_small, G.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the level (its clues) and as many ebony stones as its level
 * (one: easy .. four: expert); the menu's a puzzle half done ----------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int level = variant < 0 ? SU_MEDIUM : variant;
    uint8_t p[SU_CELLS], s[SU_CELLS];
    su_make(level, 2026u + (uint32_t)level, p, s);
    int side = variant < 0 ? size : (int)(0.84f * size);
    int f = (int)(side * 0.035f), q = (side - 2 * f) / 9, w = 9 * q + 2 * f, o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_board(b, q, f);
    for (int c = 0; c < SU_CELLS; c++) {
        float cx = f + (c % 9 + 0.5f) * q, cy = f + (c / 9 + 0.54f) * q;
        if (p[c]) puz_digits(b, w, w, p[c], cx, cy, 0.64f * q, GIVEN, 1, 1);
        else if (variant < 0 && c % 7 < 3 && c < 50) puz_digits(b, w, w, s[c], cx, cy, 0.64f * q, ENTRY, 1, 0);
    }
    art_blend(rgba, size, size, b, w, w, o, variant < 0 ? o : 0);
    free(b);
    if (variant < 0) return;
    int d = (int)(0.1f * size), y = w + (size - w - d) / 2;
    uint8_t *st = calloc((size_t)d * d, 4);
    if (!st) return;
    art_counter(COUNTER_STONE, 1, d, st);
    for (int i = 0; i <= level; i++)
        art_blend(rgba, size, size, st, d, d, (int)(size / 2.0f + (i - level / 2.0f) * 1.25f * d - d / 2.0f), y);
    free(st);
}

static const game sudoku_game = {
    .id = "sudoku", .order = 350, .variants = SU_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
};
REGISTER_GAME(sudoku_game)
