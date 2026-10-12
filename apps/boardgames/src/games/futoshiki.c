/*
 * Futoshiki in RetroStone Board, for one player: the house plank with n x n cells inlaid in it, the signs burned into
 * the wood between them, the given digits in dark ink and the player's in blue ink, a row of wooden keys 1..n under
 * the board and four walnut buttons beside it (the rules: futoshiki_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a cell, then a key: its digit goes in (the same key again takes it out); or drag a key onto a cell. The cell's
 * row and column are lit faintly, and every cell holding its digit warmly (a key tapped with no empty cell picked
 * lights its digit's cells instead). The buttons, top to bottom: the pencil (notes: the keys then put small digits in
 * the cell, or take them out), the eraser (the cell's digit, or its notes), the lamp (a hint: a wrong digit put
 * right, else the easiest cell filled, in gold light) and the eye (clashes shown: a digit twice in a line turns red,
 * and so does a sign that does not hold; on at first). A key whose n digits are all on the board fades. Undo takes
 * back one action, all the way to the start. The bar: the moves. Solved: a wave of gold light from the last cell.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/futoshiki.h"
#include "games/logicgrid2_ui.h"
#include "games/puzzles.h"
#include "shell.h"

#define GAP 0.42f                               /* the room between two cells (the signs), a part of a cell */
#define MARGIN 0.32f                            /* round the cells, inside the frame */
#define POP_MS 200

static const uint8_t GIVEN[3] = {40, 34, 30}, NOTE[3] = {92, 70, 50};
static const float CELL_TONE[3] = {1.07f, 1.06f, 1.04f};

enum { T_NOTES, T_ERASE, T_HINT, T_CLASH, TOOLS };
static const int TOOL_PIC[TOOLS] = {LG2_PENCIL, LG2_ERASER, LG2_LAMP, LG2_EYE};

static fsk_game G;
static int sel = -1, lit_digit, notes_mode, show_clash = 1, serial;
static struct { int cell; Uint32 t0; } pop = {-1, 0}, hint = {-1, 0};
static lg2_wave win;
static struct { int on, key, moved; float x, y, x0, y0; } ptr;

static box last_box;
static float bx, by, bw, q, pitch, ox, oy, kx0, ky, kp, kw, kh, tx, ty[TOOLS], td;
static int fr, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_key, *t_small, *t_sign;
static puz_font f_given, f_entry, f_note;
static lg2_buttons buttons;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a sign: a chevron pointing at the lesser side; dir 0 left, 1 up, 2 right, 3 down (a unit square) */
static void sign_fig(art_fig *f, int dir)
{
    float p[3][2] = {{0.66f, 0.2f}, {0.3f, 0.5f}, {0.66f, 0.8f}};
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < dir; k++) {         /* a quarter turn clockwise round the middle */
            float x = p[i][0] - 0.5f, y = p[i][1] - 0.5f;
            p[i][0] = 0.5f - y;
            p[i][1] = 0.5f + x;
        }
    lg2_band(f, p[0][0], p[0][1], p[1][0], p[1][1], 0.2f);
    lg2_band(f, p[1][0], p[1][1], p[2][0], p[2][1], 0.2f);
    art_ellipse(f, ART_PART, p[1][0], p[1][1], 0.1f, 0.1f);
}

/* the cells' places: cell (x, y)'s top left corner in a board whose grid starts at (o, o), cells c wide */
static float place_of(int i, float o, float c) { return o + i * c * (1 + GAP); }

/* the board: the plank, each cell a lighter inlay */
static void paint_board(uint8_t *o, int w, int f, int n, float c, float org)
{
    lg2_plank(o, w, w, f);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            race_inlay(o, w, w, place_of(x, org, c), place_of(y, org, c), c, c, 0.08f * c, CELL_TONE,
                       fmaxf(1.5f, 0.07f * c));
}

/* the signs and the given digits drawn into a picture (the thumbnails) */
static void paint_clues(uint8_t *o, int w, const fsk_puz *p, float c, float org, const uint8_t *shown)
{
    int n = p->n;
    art_style st = race_style(PUZ_INK, PUZ_INK, 0, 0);
    for (int cell = 0; cell < n * n; cell++) {
        int x = cell % n, y = cell / n;
        float cx = place_of(x, org, c) + c / 2, cy = place_of(y, org, c) + c / 2, s = GAP * c * 1.3f;
        for (int way = 0; way < 2; way++) {
            int r = p->rel[way][cell];
            if (!r) continue;
            art_fig *f = art_fig_new();
            sign_fig(f, way ? (r == FSK_LT ? 1 : 3) : (r == FSK_LT ? 0 : 2));
            float sx = way ? cx : cx + c * (1 + GAP) / 2, sy = way ? cy + c * (1 + GAP) / 2 : cy;
            race_fig(o, w, w, f, &st, (int)(sx - s / 2), (int)(sy - s / 2), (int)s, 1);
            art_fig_free(f);
        }
        int d = shown ? shown[cell] : p->g[cell];
        if (d) puz_digits(o, w, w, d, cx, cy + 0.03f * c, 0.66f * c, p->g[cell] ? GIVEN : LG2_BLUE, 1, p->g[cell] != 0);
    }
}

/* the token: a little tile with 2 < 5 */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.92f), h = (int)(size * 0.6f);
    uint8_t *b = calloc((size_t)w * h, 4);
    if (!b) return;
    lg2_tile(b, w, h, 0.16f * h, CELL_TONE);
    puz_digits(b, w, h, 2, w * 0.22f, h * 0.53f, 0.66f * h, GIVEN, 1, 1);
    puz_digits(b, w, h, 5, w * 0.78f, h * 0.53f, 0.66f * h, LG2_BLUE, 1, 0);
    art_fig *f = art_fig_new();
    sign_fig(f, 0);
    art_style st = race_style(PUZ_INK, PUZ_INK, 0, 0);
    race_fig(b, w, h, f, &st, (int)(w / 2.0f - 0.25f * h), (int)(h / 2.0f - 0.25f * h), (int)(0.5f * h), 1);
    art_fig_free(f);
    art_blend(rgba, size, size, b, w, h, (size - w) / 2, (size - h) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int n = G.n ? G.n : 5;
    float S = fminf(b.w, b.h), u = ui.unit;
    kh = floorf(1.0f * u);
    float avail = S - kh - 0.26f * u;
    fr = (int)(avail * 0.035f);
    bw = floorf(avail);
    q = floorf((bw - 2 * fr) / (n + GAP * (n - 1) + 2 * MARGIN));
    pitch = q * (1 + GAP);
    float grid = n * q + (n - 1) * GAP * q, org = floorf((bw - grid) / 2);
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    ox = bx + org;
    oy = by + org;
    kp = S / n;
    kw = floorf(fminf(1.3f * u, 0.86f * kp));
    kx0 = bx + kp / 2;
    ky = by + S - kh / 2;
    td = floorf(fminf(0.92f * u, 0.9f * (S - bw)));
    tx = bx + bw + (S - bw) / 2;
    for (int i = 0; i < TOOLS; i++) ty[i] = by + bw * (i + 0.5f) / TOOLS;
    int key = ((int)bw * 16 + n) * 64 + serial % 64;
    if (made_key != key) {
        made_key = key;
        int w = (int)bw;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, n, q, (float)org);
        t_board = ui_tex(w, w, ui_buf(w, w));
        int iw = (int)kw, ih = (int)kh;
        ui_free(&t_key);
        lg2_tile(ui_buf(iw, ih), iw, ih, 0.14f * iw, CELL_TONE);
        t_key = ui_tex(iw, ih, ui_buf(iw, ih));
        int s = (int)(GAP * q * 1.25f);
        ui_free(&t_sign);
        memset(ui_buf(s, s), 0, (size_t)s * s * 4);
        art_fig *f = art_fig_new();
        sign_fig(f, 0);
        art_style st = race_style(PUZ_WHITE, PUZ_WHITE, 0, 0);
        art_render(f, &st, s, s, (float)s, 0, 0, ui_buf(s, s));
        art_fig_free(f);
        t_sign = ui_tex(s, s, ui_buf(s, s));
        puz_font_make(&f_given, (int)(0.64f * q), 1);
        puz_font_make(&f_entry, (int)(0.64f * q), 0);
        puz_font_make(&f_note, (int)fmaxf(6, 0.28f * q), 1);
    }
    lg2_small(&t_small, &made_small, token);
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
    fsk_new(&G, variant < 0 || variant >= FSK_VARIANTS ? 3 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    notes_mode = 0;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return fsk_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return lg2_wave_busy(&win, G.n, G.n); }

static void after(int cell)                     /* an action done: solved by it, the wave from that cell */
{
    if (fsk_solved(&G) && !win.on) {
        lg2_wave_start(&win, (float)(cell % G.n), (float)(cell / G.n));
        sel = -1;
        lit_digit = 0;
    }
    ui_animate();
}

static int undo(void)
{
    int c = fsk_undo(&G);
    if (c < 0) return 0;
    calm();
    sel = c;
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = fsk_script(&G, moves);
    if (fsk_solved(&G)) after(G.n * G.n / 2);
    return ok;
}

static int editable(int c) { return c >= 0 && c < G.n * G.n && !G.p.g[c]; }

static void enter(int d)
{
    if (!editable(sel)) {                       /* nothing to fill: the key lights its digit's cells */
        lit_digit = lit_digit == d ? 0 : d;
        sel = -1;
        ui_sound(SND_TAP);
        return;
    }
    lit_digit = 0;
    if (notes_mode) {
        if (G.val[sel] || !fsk_note(&G, sel, d)) { ui_sound(SND_DENY); return; }
        ui_sound(SND_TAP);
        return;
    }
    if (G.val[sel] == d) {                      /* the same digit again: taken out */
        fsk_put(&G, sel, 0);
        ui_sound(SND_TAP);
        return;
    }
    fsk_put(&G, sel, d);
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
        if (!editable(sel) || !fsk_erase(&G, sel)) { ui_sound(SND_DENY); break; }
        ui_sound(SND_TAP);
        break;
    default: {
        int c = fsk_hint(&G);
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
    *x = ox + (c % G.n) * pitch;
    *y = oy + (c / G.n) * pitch;
}

static int cell_at(float x, float y)
{
    int n = G.n;
    float ix = (x - ox + GAP * q / 2) / pitch, iy = (y - oy + GAP * q / 2) / pitch;   /* the gaps shared out */
    if (ix < 0 || iy < 0 || ix >= n || iy >= n) return -1;
    return (int)iy * n + (int)ix;
}

static int key_at(float x, float y)
{
    if (fabsf(y - ky) > 0.62f * kh || x < bx || x >= bx + G.n * kp) return 0;
    return (int)((x - bx) / kp) + 1;
}

static int tool_at(float x, float y)
{
    for (int i = 0; i < TOOLS; i++)
        if (lg2_hit(tx, ty[i], td, x, y)) return i;
    return -1;
}

static void draw_key(int d, float x, float y, float s, Uint8 a)
{
    SDL_SetTextureAlphaMod(t_key, a);
    ui_shadow_flat(x, y, kw * s, kh * s, a / 255.0f * 0.8f);
    ui_put_c(t_key, x, y, kw * s, kh * s);
    SDL_SetTextureAlphaMod(t_key, 255);
    if (notes_mode) {                           /* notes: the digit small, in its place in the cell */
        float nx = x + ((d - 1) % 3 - 1) * 0.24f * kw * s, ny = y + ((d - 1) / 3 - 1) * 0.25f * kh * s;
        puz_number(&f_given, nx, ny, 0.34f * kh * s, d, PUZ_INK, a);
    } else {
        puz_number(&f_given, x, y, 0.6f * kh * s, d, PUZ_INK, a);
    }
}

/* a sign broken by the digits on its two sides */
static int sign_broken(int way, int c)
{
    int r = G.p.rel[way][c], o = c + (way ? G.n : 1), a = G.val[c], b = G.val[o];
    return r && a && b && ((r == FSK_LT && a >= b) || (r == FSK_GT && a <= b));
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.n, solved = fsk_solved(&G);
    ui_put(t_board, bx, by, bw, bw);
    float x, y;
    int hd = sel >= 0 && G.val[sel] ? G.val[sel] : lit_digit;
    if (!solved) {
        for (int c = 0; c < n * n; c++) {
            cell_xy(c, &x, &y);
            if (sel >= 0 && c != sel && (c / n == sel / n || c % n == sel % n))
                ui_fill(x, y, q, q, 250, 214, 90, 38);
            if (hd && G.val[c] == hd && c != sel) ui_mark_last(x, y, q, q);
            if (show_clash && fsk_clash(&G, c)) ui_fill(x, y, q, q, 214, 60, 44, 56);
        }
        if (sel >= 0) {
            cell_xy(sel, &x, &y);
            ui_mark_select(x, y, q, q);
        }
    }
    float ss = GAP * q * 1.25f;                  /* the signs */
    for (int c = 0; c < n * n; c++)
        for (int way = 0; way < 2; way++) {
            int r = G.p.rel[way][c];
            if (!r) continue;
            cell_xy(c, &x, &y);
            float sx = x + q / 2 + (way ? 0 : pitch / 2), sy = y + q / 2 + (way ? pitch / 2 : 0);
            int dir = way ? (r == FSK_LT ? 1 : 3) : (r == FSK_LT ? 0 : 2);
            const uint8_t *ink = show_clash && !solved && sign_broken(way, c) ? LG2_CLASH : PUZ_INK;
            SDL_SetTextureColorMod(t_sign, ink[0], ink[1], ink[2]);
            ui_put_rot(t_sign, sx, sy, ss, ss, 90.0 * dir);
        }
    SDL_SetTextureColorMod(t_sign, 255, 255, 255);
    if (hint.cell >= 0) {
        cell_xy(hint.cell, &x, &y);
        if (!lg2_hint_glow(hint.t0, x + q / 2, y + q / 2, q)) hint.cell = -1;
    }
    Uint32 now = ui_ms();
    for (int c = 0; c < n * n; c++) {
        cell_xy(c, &x, &y);
        float mx = x + q / 2, my = y + q / 2;
        lg2_wave_cell(&win, (float)(c % n), (float)(c / n), mx, my, q);
        int v = G.val[c];
        if (v) {
            float k = 1;
            if (c == pop.cell) {
                float t = (now - pop.t0) / (float)POP_MS;
                if (t < 1) { k = 1 + 0.4f * (1 - puz_ease(t)); ui_animate(); }
            }
            const uint8_t *ink = G.p.g[c] ? GIVEN : show_clash && !solved && fsk_clash(&G, c) ? LG2_CLASH : LG2_BLUE;
            puz_number(G.p.g[c] ? &f_given : &f_entry, mx, my + 0.02f * q, 0.64f * q * k, v, ink, 255);
        } else if (G.notes[c]) {
            for (int d = 1; d <= n; d++) {
                if (!(G.notes[c] >> d & 1)) continue;
                float nx = mx + ((d - 1) % 3 - 1) * 0.31f * q, ny = my + ((d - 1) / 3 - 1) * 0.3f * q;
                if (d == hd) ui_mark_last(nx - 0.15f * q, ny - 0.15f * q, 0.3f * q, 0.3f * q);
                puz_number(&f_note, nx, ny, 0.28f * q, d, d == hd ? LG2_BLUE : NOTE, 255);
            }
        }
    }
    lg2_wave_tick(&win, n, n);
    for (int d = 1; d <= n; d++) {              /* the keys */
        float kx = kx0 + (d - 1) * kp;
        int full = fsk_count_digit(&G, d) >= n;
        if (d == hd && !solved) ui_mark_gold(kx, ky, 1.7f * kw);
        int held = ptr.on && ptr.key == d && ptr.moved;
        draw_key(d, kx, ky, 1, (Uint8)(held ? 90 : full ? 105 : 255));
    }
    int erasable = editable(sel) && (G.val[sel] || G.notes[sel]);
    int states[TOOLS] = {notes_mode ? LG2_ON : LG2_READY, erasable ? LG2_READY : LG2_DIM,
                         solved ? LG2_DIM : LG2_READY, show_clash ? LG2_ON : LG2_READY};
    for (int i = 0; i < TOOLS; i++) lg2_button(&buttons, TOOL_PIC[i], tx, ty[i], td, states[i]);
    if (ptr.on && ptr.key && ptr.moved) {       /* a key carried, above the finger */
        int c = cell_at(ptr.x, ptr.y - 0.6f * kh);
        if (editable(c)) {
            cell_xy(c, &x, &y);
            ui_mark_select(x, y, q, q);
        }
        draw_key(ptr.key, ptr.x, ptr.y - 0.6f * kh, 1.1f, 255);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (fsk_solved(&G)) return;
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
    sel = -1;
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

static void bar(int player, box b)
{
    (void)player;
    puz_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the size and level (its signs and digits), and as many ebony
 * stones as its level; the menu's a 5 x 5 puzzle half done ------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 4 : variant, n = fsk_size_of(v);
    fsk_puz p;
    uint8_t sol[FSK_CELLS], shown[FSK_CELLS];
    fsk_make(n, fsk_level_of(v), 2026u + (uint32_t)v, &p, sol);
    for (int c = 0; c < n * n; c++) shown[c] = p.g[c] ? p.g[c] : variant < 0 && c % 3 == 0 ? sol[c] : 0;
    int side = variant < 0 ? size : (int)(0.84f * size), f = (int)(side * 0.035f);
    float c = floorf((side - 2 * f) / (n + GAP * (n - 1) + 2 * MARGIN));
    float org = floorf((side - (n * c + (n - 1) * GAP * c)) / 2);
    uint8_t *b = calloc((size_t)side * side, 4);
    if (!b) return;
    paint_board(b, side, f, n, c, org);
    paint_clues(b, side, &p, c, org, shown);
    int o = (size - side) / 2;
    art_blend(rgba, size, size, b, side, side, o, variant < 0 ? o : 0);
    free(b);
    if (variant >= 0) lg2_stones(rgba, size, side + (size - side - (int)(0.1f * size)) / 2, fsk_level_of(v));
}

static const game futoshiki_game = {
    .id = "futoshiki", .order = 414, .variants = FSK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
};
REGISTER_GAME(futoshiki_game)
