/*
 * Paths in RetroStone Board, for one player: square tiles cut in the house plank, pairs of lacquered dots on them (the
 * house's ivory, ebony, red lacquer, jade, amber and lapis, then plum, sky, rose, leaf, orange, teal, silver and
 * garnet), each with its own mark (a dot, a ring, a square, a diamond, triangles, a plus, a cross, a star, bars...)
 * so that no pair is told by its colour alone; the paths as lacquered bands from square to square (the rules:
 * paths_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag from a dot (its path starts again from it) or from a path's square (the path is cut back to it): the path
 * follows the finger square by square; over another path it cuts it (the cut part comes back if the finger backs
 * off); back along itself it shortens; onto its other dot the pair is linked (both dots ring). With taps alone: tap a
 * dot or a path's square (its end is marked), then a square in the same row or column: the path runs straight there.
 * The squares a path covers are washed in its colour. The lamp at the right end of the bar: a hint (one pair's path
 * laid as in the solution, in gold light). Undo takes back one stroke or hint, all the way to the start. The bar: the moves.
 * Solved: every pair linked and every square covered; a gold light runs along every path from its dots.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/paths.h"
#include "games/puzzles.h"
#include "games/routes.h"
#include "shell.h"

#define FRAME 0.035f
#define DOT 0.74f                               /* a dot's size, a part of a square */
#define BAND 0.34f                              /* a path's width */
#define HINT_MS 1400
#define LINK_MS 700
#define WAVE_STEP 45
#define WAVE_LEN 480

static const uint8_t NK_RGB[NK_COLOURS][3] = {
    {242, 234, 216}, {70, 64, 62}, {168, 52, 40}, {62, 128, 98}, {222, 168, 66}, {52, 84, 150},   /* the house's */
    {124, 66, 140}, {104, 176, 226}, {228, 122, 160}, {128, 178, 58}, {230, 118, 36}, {32, 148, 150},
    {176, 178, 188}, {116, 30, 52}};

static int light_colour(int c)                  /* 1: a light colour (its mark dark) */
{
    const uint8_t *k = NK_RGB[c];
    return 0.3f * k[0] + 0.59f * k[1] + 0.11f * k[2] > 150;
}

static nk_game G;
static box last_box;
static float bx, by, side;
static int fr, cs, ox, oy, made_key = -1, made_small = -1, serial;
static SDL_Texture *t_board, *t_dot[NK_COLOURS], *t_small;
static rte_lamp lamp;
static int active = -1, anchor = -1;           /* the colour picked for taps (its path's end marked), its dot */
static struct { int on, cell, moved, line, was_active, from, linked; } ptr;
static int from_cell(void);
static struct { int colour; Uint32 t0; } hint = {-1, 0}, link = {-1, 0};
static struct { int on; Uint32 t0; } win;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a dot's mark: a figure in the unit square */
static void mark_fig(art_fig *f, int c)
{
    switch (c % NK_COLOURS) {
    case 0: art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.13f, 0.13f); break;           /* a dot */
    case 1: art_arc(f, ART_PART, 0.5f, 0.5f, 0.11f, 0.2f, 0, 360); break;         /* a ring */
    case 2: art_rect(f, ART_PART, 0.35f, 0.35f, 0.65f, 0.65f); break;              /* a square */
    case 3: {                                                                       /* a diamond */
        const float q[] = {0.5f, 0.29f, 0.71f, 0.5f, 0.5f, 0.71f, 0.29f, 0.5f};
        art_poly(f, ART_PART, q, 4);
        break;
    }
    case 4: { const float q[] = {0.5f, 0.3f, 0.71f, 0.67f, 0.29f, 0.67f}; art_poly(f, ART_PART, q, 3); } break;
    case 5: { const float q[] = {0.29f, 0.33f, 0.71f, 0.33f, 0.5f, 0.7f}; art_poly(f, ART_PART, q, 3); } break;
    case 6:                                                                         /* a plus */
        art_rect(f, ART_PART, 0.44f, 0.28f, 0.56f, 0.72f);
        art_rect(f, ART_PART, 0.28f, 0.44f, 0.72f, 0.56f);
        break;
    case 7: {                                                                       /* a cross */
        const float a[] = {0.33f, 0.25f, 0.75f, 0.67f, 0.67f, 0.75f, 0.25f, 0.33f};
        const float b[] = {0.67f, 0.25f, 0.75f, 0.33f, 0.33f, 0.75f, 0.25f, 0.67f};
        art_poly(f, ART_PART, a, 4);
        art_poly(f, ART_PART, b, 4);
        break;
    }
    case 8: {                                                                       /* a star */
        float q[20];
        for (int i = 0; i < 10; i++) {
            float a = (-90 + 36 * i) * PUZ_PI / 180, r = i & 1 ? 0.1f : 0.23f;
            q[2 * i] = 0.5f + r * cosf(a);
            q[2 * i + 1] = 0.52f + r * sinf(a);
        }
        art_poly(f, ART_PART, q, 10);
        break;
    }
    case 9: art_rect(f, ART_PART, 0.27f, 0.43f, 0.73f, 0.57f); break;             /* a bar across */
    case 10: art_rect(f, ART_PART, 0.43f, 0.27f, 0.57f, 0.73f); break;            /* a bar upright */
    case 11:                                                                        /* two dots */
        art_ellipse(f, ART_PART, 0.37f, 0.5f, 0.09f, 0.09f);
        art_ellipse(f, ART_PART, 0.63f, 0.5f, 0.09f, 0.09f);
        break;
    case 12: art_arc(f, ART_PART, 0.5f, 0.5f, 0.0f, 0.21f, 180, 360); break;      /* a half disc */
    default: {                                                                      /* a square frame */
        art_rect(f, ART_PART, 0.3f, 0.3f, 0.7f, 0.39f);
        art_rect(f, ART_PART, 0.3f, 0.61f, 0.7f, 0.7f);
        art_rect(f, ART_PART, 0.3f, 0.3f, 0.39f, 0.7f);
        art_rect(f, ART_PART, 0.61f, 0.3f, 0.7f, 0.7f);
    }
    }
}

/* a dot of colour c: a lacquered disc, its rim dark, its mark on it (size x size, cleared first) */
static void paint_dot(int c, int size, uint8_t *o)
{
    static const uint8_t INK[3] = {36, 26, 20}, CREAM[3] = {248, 240, 224};
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.46f, 0.46f);
    rte_paint(o, size, f, NK_RGB[c], INK, fmaxf(0.03f, 1.3f / size), 0.4f);
    f = art_fig_new();
    mark_fig(f, c);
    const uint8_t *m = light_colour(c) ? INK : CREAM;
    rte_paint(o, size, f, m, m, 0, 0);
}

/* the token: a wooden tile, two red dots joined by a path */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(0.86f * size), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    float q = w / 3.0f, h = BAND * q / 2;
    static const uint8_t DARK[3] = {40, 20, 14};
    puz_ink_rect(b, w, w, 0.5f * q - h - 1, 0.5f * q - h - 1, 2.5f * q + h + 1, 0.5f * q + h + 1, DARK, 1);
    puz_ink_rect(b, w, w, 2.5f * q - h - 1, 0.5f * q - h - 1, 2.5f * q + h + 1, 2.5f * q + h + 1, DARK, 1);
    puz_ink_rect(b, w, w, 0.5f * q - h, 0.5f * q - h, 2.5f * q + h, 0.5f * q + h, NK_RGB[2], 1);
    puz_ink_rect(b, w, w, 2.5f * q - h, 0.5f * q - h, 2.5f * q + h, 2.5f * q + h, NK_RGB[2], 1);
    int d = (int)(DOT * q);
    uint8_t *dot = malloc((size_t)d * d * 4);
    if (dot) {
        paint_dot(2, d, dot);
        art_blend(b, w, w, dot, d, d, (int)(0.5f * q - d / 2.0f), (int)(0.5f * q - d / 2.0f));
        art_blend(b, w, w, dot, d, d, (int)(2.5f * q - d / 2.0f), (int)(2.5f * q - d / 2.0f));
        free(dot);
    }
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h));
    side = S;
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    fr = (int)(S * FRAME);
    cs = (int)((S - 2 * fr) / G.n);
    ox = oy = (int)((S - G.n * cs) / 2);
    int key = ((int)S * 64 + serial % 64) * 16 + G.n;
    if (made_key != key) {
        made_key = key;
        int w = (int)S, d = (int)(DOT * cs);
        ui_free(&t_board);
        rte_tiles(ui_buf(w, w), w, fr, G.n, cs, ox, oy);
        t_board = ui_tex(w, w, ui_buf(w, w));
        for (int c = 0; c < NK_COLOURS; c++) {
            ui_free(&t_dot[c]);
            paint_dot(c, d, ui_buf(d, d));
            t_dot[c] = ui_tex(d, d, ui_buf(d, d));
        }
    }
    int small = (int)(0.6f * ui.unit);
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
    memset(&ptr, 0, sizeof ptr);
    hint.colour = link.colour = -1;
    win.on = 0;
    active = -1;
}

static void start(int variant)
{
    nk_new(&G, variant < 0 || variant >= NK_VARIANTS ? 1 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return nk_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }

static Uint32 win_end(void)
{
    int longest = 0;
    for (int c = 0; c < G.pairs; c++)
        if (G.len[c] > longest) longest = G.len[c];
    return (Uint32)((longest / 2 + 1) * WAVE_STEP + WAVE_LEN);
}

static int busy(void) { return win.on && ui_ms() - win.t0 < win_end(); }

/* an action done: a pair newly linked rings, the win's light */
static void after(int linked_before)
{
    int linked = 0;
    for (int c = 0; c < G.pairs; c++) linked |= nk_linked(&G, c) << c;
    for (int c = 0; c < G.pairs; c++)
        if ((linked >> c & 1) && !(linked_before >> c & 1)) {
            link.colour = c;
            link.t0 = ui_ms();
        }
    if (nk_solved(&G) && !win.on) {
        win.on = 1;
        win.t0 = ui_ms();
        active = -1;
    }
    ui_animate();
}

static int linked_now(void)
{
    int m = 0;
    for (int c = 0; c < G.pairs; c++) m |= nk_linked(&G, c) << c;
    return m;
}

static int undo(void)
{
    calm();
    return nk_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int before = linked_now(), ok = nk_script(&G, moves);
    after(before);
    link.colour = -1;
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_centre(int s, float *x, float *y)
{
    *x = bx + ox + (s % G.n + 0.5f) * cs;
    *y = by + oy + (s / G.n + 0.5f) * cs;
}

static int cell_at(float x, float y)
{
    float ix = x - bx - ox, iy = y - by - oy;
    if (ix < 0 || iy < 0 || ix >= G.n * cs || iy >= G.n * cs) return -1;
    return (int)(iy / cs) * G.n + (int)(ix / cs);
}

/* a band from square a to square b (beside each other), w wide, its ends reaching w / 2 past the centres */
static void band(int a, int b, float w, const uint8_t rgb[3], Uint8 alpha)
{
    float ax, ay, bx2, by2;
    cell_centre(a, &ax, &ay);
    cell_centre(b, &bx2, &by2);
    float x0 = fminf(ax, bx2) - w / 2, y0 = fminf(ay, by2) - w / 2;
    ui_fill(x0, y0, fabsf(bx2 - ax) + w, fabsf(by2 - ay) + w, rgb[0], rgb[1], rgb[2], alpha);
}

static void draw_path(int c)
{
    static const uint8_t RIM[3] = {38, 24, 16};
    int L = G.len[c];
    if (L < 2) return;
    float w = BAND * cs, rim = fmaxf(1.5f, 0.045f * cs);
    const uint8_t *k = NK_RGB[c];
    uint8_t hi[3];
    for (int i = 0; i < 3; i++) hi[i] = (uint8_t)(k[i] + (255 - k[i]) * 0.2f);
    for (int i = 0; i + 1 < L; i++) band(G.path[c][i], G.path[c][i + 1], w + 2 * rim, RIM, 255);
    for (int i = 0; i + 1 < L; i++) band(G.path[c][i], G.path[c][i + 1], w, k, 255);
    for (int i = 0; i + 1 < L; i++) band(G.path[c][i], G.path[c][i + 1], 0.28f * w, hi, 255);   /* its gleam */
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    int nc = G.n * G.n, solved = nk_solved(&G);
    ui_put(t_board, bx, by, side, side);
    for (int s = 0; s < nc; s++) {              /* the squares covered: washed in their colour */
        int o = G.own[s];
        if (!o || G.dot[s]) continue;
        const uint8_t *k = NK_RGB[o - 1];
        float x, y;
        cell_centre(s, &x, &y);
        float in = fmaxf(1, 0.05f * cs);
        ui_fill(x - cs / 2.0f + in, y - cs / 2.0f + in, cs - 2 * in, cs - 2 * in, k[0], k[1], k[2], 70);
    }
    if (active >= 0 && !solved) {               /* the picked path's end */
        int h = from_cell();
        float x, y;
        cell_centre(h, &x, &y);
        ui_mark_select_round(x, y, 0.98f * cs);
    }
    for (int c = 0; c < G.pairs; c++) draw_path(c);
    float d = DOT * cs;
    for (int c = 0; c < G.pairs; c++)
        for (int e = 0; e < 2; e++) {
            float x, y;
            cell_centre(G.end[c][e], &x, &y);
            if (nk_linked(&G, c) && !solved) ui_mark_gold(x, y, 1.25f * d);
            ui_shadow_flat(x, y, d, d, 0.7f);
            ui_put_c(t_dot[c], x, y, d, d);
        }
    if (link.colour >= 0) {                     /* a pair just linked: its dots ring */
        float p = puz_pulse((float)(now - link.t0), 0, LINK_MS);
        if (p > 0) {
            for (int e = 0; e < 2; e++) {
                float x, y;
                cell_centre(G.end[link.colour][e], &x, &y);
                ui_mark_gold(x, y, (1.2f + 1.2f * p) * d);
            }
            ui_animate();
        } else {
            link.colour = -1;
        }
    }
    if (hint.colour >= 0) {                     /* the hint's path in gold light */
        float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
        if (p > 0) {
            for (int i = 0; i < G.len[hint.colour]; i++) {
                float x, y;
                cell_centre(G.path[hint.colour][i], &x, &y);
                ui_mark_gold(x, y, 1.6f * cs * p);
            }
            ui_animate();
        } else {
            hint.colour = -1;
        }
    }
    if (win.on) {                               /* the win: a light along every path from both its dots */
        float wt = (float)(now - win.t0);
        for (int c = 0; c < G.pairs; c++)
            for (int i = 0; i < G.len[c]; i++) {
                int from_end = i < G.len[c] - 1 - i ? i : G.len[c] - 1 - i;
                float q = puz_pulse(wt, from_end * WAVE_STEP, WAVE_LEN), x, y;
                if (q <= 0) continue;
                cell_centre(G.path[c][i], &x, &y);
                ui_mark_gold(x, y, 2.0f * cs * q);
            }
        if (wt < win_end()) ui_animate();
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the square the picked path goes on from: its end, or the dot tapped when it has none */
static int from_cell(void)
{
    if (active < 0) return -1;
    int h = nk_head(&G, active);
    return h >= 0 ? h : anchor;
}

/* 1: a tap on s runs the picked path straight there (s in its end's row or column: a free square, another path's
 * square but its end, or the path's other dot) */
static int line_to(int s)
{
    if (active < 0 || nk_linked(&G, active)) return 0;
    int h = from_cell(), n = G.n;
    if (h < 0 || s == h || (h % n != s % n && h / n != s / n)) return 0;
    if (G.dot[s]) return G.dot[s] == active + 1 && (!G.len[active] || s != G.path[active][0]);
    int o = G.own[s] - 1;
    if (o == active) return 0;
    if (o >= 0 && nk_head(&G, o) == s) return 0;
    return 1;
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (nk_solved(&G)) return;
    int s = cell_at(x, y);
    if (s < 0) return;
    ptr.on = 1;
    ptr.cell = s;
    ptr.was_active = active;
    if (line_to(s)) {                           /* on release: the picked path runs straight there */
        ptr.line = 1;
        return;
    }
    ptr.linked = linked_now();
    int c = nk_begin(&G, s);
    if (c < 0) {
        ptr.on = 0;
        ui_sound(SND_DENY);
        return;
    }
    ptr.from = active >= 0 ? from_cell() : -1;
    active = c;
    anchor = s;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on || ptr.line || G.stroke < 0) return;
    int s = cell_at(x, y);
    if (s < 0 || s == nk_head(&G, G.stroke)) return;
    if (s != ptr.cell) ptr.moved = 1;
    if (nk_toward(&G, s)) {
        ui_sound_soft(SND_TAP, 0.35f);
        ui_animate();
    }
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    int s = cell_at(x, y);
    if (ptr.line) {
        if (s != ptr.cell) return;
        int before = linked_now(), c = active;
        nk_begin(&G, from_cell());
        int k = nk_toward(&G, s);
        if (!nk_end(&G) || !k) { ui_sound(SND_DENY); return; }
        ui_sound(SND_PLACE);
        if (nk_linked(&G, c)) active = -1;
        after(before);
        return;
    }
    if (G.stroke < 0) return;
    int c = G.stroke, changed = nk_end(&G);
    if (changed) ui_sound(SND_PLACE);
    if (!ptr.moved && !changed && ptr.was_active == c && ptr.from == ptr.cell) active = -1;   /* tapped again: let go */
    if (nk_linked(&G, c)) active = -1;
    if (changed) after(ptr.linked);
    ui_animate();
}

/* ---- the bar: the moves, the hint ------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    puz_bar_moves(b, t_small, G.moves);
    rte_lamp_draw(&lamp, b, !nk_solved(&G));
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (!rte_lamp_hit(&lamp, x, y)) return 0;
    int before = linked_now(), c = nk_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return 1; }
    active = -1;
    hint.colour = c;
    hint.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after(before);
    return 1;
}

/* ---- the menu's and the picker's cards: a puzzle of the size, part of its paths drawn ---------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const uint8_t RIM[3] = {38, 24, 16};
    int v = variant < 0 ? 1 : variant;
    nk_game *T = malloc(sizeof *T);
    if (!T) return;
    nk_new(T, v, 2026u + (uint32_t)v);
    int n = T->n, f = (int)(size * FRAME), q = (size - 2 * f) / n, o = (size - n * q) / 2;
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) { free(T); return; }
    rte_tiles(b, size, f, n, q, o, o);
    for (int c = 0; c < T->pairs; c++) {
        if (c % 3 == 2) continue;               /* a third of the pairs still to link */
        float w = BAND * q, r = fmaxf(0.6f, 0.045f * q);
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i + 1 < T->slen[c]; i++) {
                int a = T->spath[c][i], e = T->spath[c][i + 1];
                float ax = o + (a % n + 0.5f) * q, ay = o + (a / n + 0.5f) * q, ex = o + (e % n + 0.5f) * q,
                      ey = o + (e / n + 0.5f) * q, h = pass ? w / 2 : w / 2 + r;
                puz_ink_rect(b, size, size, fminf(ax, ex) - h, fminf(ay, ey) - h, fmaxf(ax, ex) + h, fmaxf(ay, ey) + h,
                             pass ? NK_RGB[c] : RIM, 1);
            }
    }
    int d = (int)(DOT * q);
    uint8_t *dot = d > 2 ? malloc((size_t)d * d * 4) : NULL;
    for (int c = 0; dot && c < T->pairs; c++) {
        paint_dot(c, d, dot);
        for (int e = 0; e < 2; e++) {
            int s = T->end[c][e];
            art_blend(b, size, size, dot, d, d, (int)(o + (s % n + 0.5f) * q - d / 2.0f),
                      (int)(o + (s / n + 0.5f) * q - d / 2.0f));
        }
    }
    free(dot);
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
    free(T);
}

static const game paths_game = {
    .id = "paths", .order = 401, .variants = NK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(paths_game)
