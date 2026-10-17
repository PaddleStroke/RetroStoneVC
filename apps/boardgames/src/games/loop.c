/*
 * Loop in RetroStone Board, for one player: the house plank with a brass pin at every dot, the numbers burned into
 * their cells, the loop a brass wire laid from pin to pin, gold once it is solved; a crossed side a small dark cross
 * (the rules: loop_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a side: a line, then a cross, then nothing again. Or drag along the pins: every side the finger runs along
 * becomes a line (a drag that begins along a line takes the lines away instead); a drag is undone in one go. A number
 * whose lines are all drawn fades (a 0 once crossed all round); one with too many turns red. The shell's tool, the
 * lamp: a hint (a wrong mark put right, else a side that reasoning finds, in gold light). Undo takes back one action
 * at a time, all the way to the start. The bar: the moves. Solved: the wire turns gold and a wave of gold light runs
 * round the board; the stars count the hints and the mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, loop_levels.h: 5 x 5 with the plainest reasoning
 * first, 15 x 15 with a side tried last), or its die tab a puzzle made at random (the four sizes at three levels).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/loop.h"
#include "games/loop_levels.h"
#include "shell.h"

static const uint8_t BRASS[3] = {222, 176, 84}, BRASS_DARK[3] = {104, 70, 24}, RED_INK[3] = {178, 40, 30};

static slk_game G;
static int serial;
static bench_flash hint = {-1, 0};             /* at: a side */
static bench_wave win;
/* the finger: down at (x0, y0); a stroke along the pins (last: the pin it reached last, erase: taking lines away) */
static struct { int on, moved, stroke, last, erase, sides; float x0, y0; } fing;

static box last_box;
static float bx, by;
static int q, fr, made_key = -1, made_small, wire_th;
static SDL_Texture *t_board, *t_small, *t_wire, *t_cap, *t_cross;
static puz_font f_clue;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a brass wire across (len x th): lit along its middle, dark at its edges, as a round wire */
static void paint_wire(uint8_t *o, int len, int th)
{
    for (int y = 0; y < th; y++) {
        float v = (y + 0.5f) / th * 2 - 1, k = sqrtf(fmaxf(0, 1 - v * v));
        float shade = 0.45f + 0.65f * k + 0.35f * powf(fmaxf(0, 1 - fabsf(v + 0.35f) * 3), 2);
        for (int x = 0; x < len; x++) {
            uint8_t *p = o + 4 * ((size_t)y * len + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(BRASS[i] * shade, 0, 255);
            p[3] = (uint8_t)(255 * clampf((1 - fabsf(v)) * th / 2 + 0.3f, 0, 1));
        }
    }
}

/* a pin's head or a wire's bend: a brass ball d across */
static void paint_cap(uint8_t *o, int d)
{
    memset(o, 0, (size_t)d * d * 4);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.48f, 0.48f);
    art_fig_paint(o, d, f, BRASS, BRASS_DARK, 0.04f, 0.8f);
}

static void paint_cross(uint8_t *o, int d)
{
    memset(o, 0, (size_t)d * d * 4);
    art_fig *f = art_fig_new();
    art_band(f, ART_PART, 0.18f, 0.18f, 0.82f, 0.82f, 0.2f);
    art_band(f, ART_PART, 0.82f, 0.18f, 0.18f, 0.82f, 0.2f);
    art_fig_paint(o, d, f, PUZ_INK, NULL, 0, 0);
}

/* a round, soft-edged spot of rgb (alpha a at its centre, a * edge at its rim) burned into the w x w plank */
static void ink_disc(uint8_t *o, int w, float cx, float cy, float r, const uint8_t rgb[3], float a, float edge)
{
    for (int y = (int)floorf(cy - r - 1); y <= (int)ceilf(cy + r + 1); y++)
        for (int x = (int)floorf(cx - r - 1); x <= (int)ceilf(cx + r + 1); x++) {
            if (x < 0 || y < 0 || x >= w || y >= w) continue;
            float d = hypotf(x + 0.5f - cx, y + 0.5f - cy), cov = clampf(r + 0.5f - d, 0, 1);
            if (cov <= 0) continue;
            float k = a * cov * (1 - (1 - edge) * clampf(d / r, 0, 1));
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] + (rgb[i] - p[i]) * k);
        }
}

/* the board: the plank in its walnut frame (fw wide), the dots f from its edges, a small round pin hole at each
 * (its lit lower lip, then its dark well) */
static void paint_board(uint8_t *o, int n, int c, int f, int fw, int pins)
{
    int w = n * c + 2 * f;
    art_board_n(1, 1, w - 2 * fw, fw, 0, o);
    if (!pins) return;
    float r = fmaxf(1.5f, 0.07f * c), lip = fmaxf(0.6f, 0.012f * c);
    for (int y = 0; y <= n; y++)
        for (int x = 0; x <= n; x++) {
            float cx = f + x * c, cy = f + y * c;
            ink_disc(o, w, cx + lip, cy + lip, r, PUZ_CREAM, 0.35f, 0.6f);
            ink_disc(o, w, cx, cy, r, PUZ_INK, 0.7f, 0.35f);
        }
}

/* the token: a small loop of brass wire round a burned 3 */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int f = (int)fmaxf(1, size * 0.06f), c = (size - 2 * f) / 2, w = 2 * c + 2 * f;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(2, 2, c, f, 0, b);
    puz_burn(b, w, w, 3, f + 0.5f * c, f + 0.55f * c, 0.6f * c);
    art_fig *g = art_fig_new();
    float u = 1.0f / w, a = (f) * u, m = (f + c) * u, e = (f + 2 * c) * u, t = fmaxf(2.0f, 0.1f * c) * u;
    art_band(g, ART_PART, a, a, m, a, t);         /* round the top left cell, down the right, across the bottom */
    art_band(g, ART_PART, m, a, m, m, t);
    art_band(g, ART_PART, m, m, e, m, t);
    art_band(g, ART_PART, e, m, e, e, t);
    art_band(g, ART_PART, e, e, a, e, t);
    art_band(g, ART_PART, a, e, a, a, t);
    art_fig_paint(b, w, g, BRASS, BRASS_DARK, 0.003f, 0.6f);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int n = G.p.n ? G.p.n : 5;
    float S = fminf(b.w, b.h);
    int fw = (int)(S * BENCH_FRAME);                  /* the frame, then a margin of plank round the dots */
    q = (int)((S - 2 * fw) / (n + 0.6f));
    fr = fw + (int)(0.3f * q);
    int w = n * q + 2 * fr;
    bx = floorf(b.x + (b.w - w) / 2);
    by = floorf(b.y + (b.h - w) / 2);
    int key = (w * 32 + n) * 4096 + serial % 4096;
    if (made_key != key) {
        made_key = key;
        ui_free(&t_board);
        uint8_t *o = ui_buf(w, w);
        paint_board(o, n, q, fr, fw, 1);
        t_board = ui_tex(w, w, o);
        wire_th = (int)fmaxf(3, 0.13f * q);
        int len = 6 * wire_th;
        ui_free(&t_wire);
        o = ui_buf(len, wire_th);
        paint_wire(o, len, wire_th);
        t_wire = ui_tex(len, wire_th, o);
        int d = (int)(1.15f * wire_th) | 1;
        ui_free(&t_cap);
        o = ui_buf(d, d);
        paint_cap(o, d);
        t_cap = ui_tex(d, d, o);
        int x = (int)(0.24f * q);
        ui_free(&t_cross);
        o = ui_buf(x, x);
        paint_cross(o, x);
        t_cross = ui_tex(x, x, o);
        puz_font_make(&f_clue, (int)(0.56f * q), 1);
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    hint.at = -1;
    win.on = 0;
    memset(&fing, 0, sizeof fing);
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!slk_decode(&G, slk_levels[level < 0 || level >= SLK_NLEVELS ? 0 : level])) slk_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* the die tab: a puzzle made at random of the variant */
{
    slk_new(&G, variant < 0 || variant >= SLK_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return slk_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }   /* the end card: the moves it took */
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void dot_xy(int x, int y, float *px, float *py)
{
    *px = bx + fr + x * q;
    *py = by + fr + y * q;
}

static void after(int side)
{
    if (slk_solved(&G) && !win.on) {
        int x0, y0, x1, y1;
        slk_ends(G.p.n, side < 0 ? 0 : side, &x0, &y0, &x1, &y1);
        bench_wave_start(&win, (float)x0, (float)y0, G.p.n, G.p.n);
    }
    ui_animate();
}

static int undo(void)
{
    if (slk_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = slk_script(&G, moves);
    if (slk_solved(&G)) after(0);
    return ok;
}

static void lamp(void)
{
    int s = slk_hint(&G);
    if (s < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, s);
    ui_sound(SND_PLACE);
    after(s);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* the side nearest the point, if near enough (-1: none) */
static int side_at(float x, float y)
{
    int n = G.p.n, best = -1;
    float bd = 0.42f * q;
    for (int s = 0; s < slk_sides(n); s++) {
        int x0, y0, x1, y1;
        slk_ends(n, s, &x0, &y0, &x1, &y1);
        float ax, ay, cx, cy;
        dot_xy(x0, y0, &ax, &ay);
        dot_xy(x1, y1, &cx, &cy);
        float t = y0 == y1 ? clampf((x - ax) / q, 0, 1) : clampf((y - ay) / q, 0, 1);
        float d = hypotf(x - (ax + (cx - ax) * t), y - (ay + (cy - ay) * t));
        if (d < bd) bd = d, best = s;
    }
    return best;
}

/* the dot under the finger (within r of it): x + y * (n + 1), or -1 */
static int dot_at(float x, float y, float r)
{
    int n = G.p.n;
    float fx = (x - bx - fr) / q, fy = (y - by - fr) / q;
    int dx = (int)lroundf(fx), dy = (int)lroundf(fy);
    if (dx < 0 || dy < 0 || dx > n || dy > n || hypotf(fx - dx, fy - dy) * q > r) return -1;
    return dy * (n + 1) + dx;
}

static void wire(int s, Uint8 alpha, int gold)
{
    int x0, y0, x1, y1;
    slk_ends(G.p.n, s, &x0, &y0, &x1, &y1);
    float ax, ay, cx, cy;
    dot_xy(x0, y0, &ax, &ay);
    dot_xy(x1, y1, &cx, &cy);
    float mx = (ax + cx) / 2, my = (ay + cy) / 2, len = (float)q;
    if (gold) SDL_SetTextureColorMod(t_wire, 255, 236, 140);
    /* its shadow: the wire itself in dark brown, soft-edged, a little down and right */
    SDL_SetTextureColorMod(t_wire, 20, 12, 6);
    SDL_SetTextureAlphaMod(t_wire, (Uint8)(alpha * 0.3f));
    float sh = 0.045f * q;
    if (y0 == y1) ui_put_c(t_wire, mx + 0.5f * sh, my + sh, len, 1.15f * wire_th);
    else ui_put_rot(t_wire, mx + sh, my + 0.5f * sh, len, 1.15f * wire_th, 90);
    if (gold) SDL_SetTextureColorMod(t_wire, 255, 236, 140);
    else SDL_SetTextureColorMod(t_wire, 255, 255, 255);
    SDL_SetTextureAlphaMod(t_wire, alpha);
    if (y0 == y1) ui_put_c(t_wire, mx, my, len, (float)wire_th);
    else ui_put_rot(t_wire, mx, my, len, (float)wire_th, 90);
    SDL_SetTextureAlphaMod(t_wire, 255);
    SDL_SetTextureColorMod(t_wire, 255, 255, 255);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, solved = slk_solved(&G), w = n * q + 2 * fr;
    ui_put(t_board, bx, by, (float)w, (float)w);
    float px, py;
    if (hint.at >= 0) {
        int x0, y0, x1, y1;
        slk_ends(n, hint.at, &x0, &y0, &x1, &y1);
        dot_xy(x0, y0, &px, &py);
        float qx, qy;
        dot_xy(x1, y1, &qx, &qy);
        bench_glow(&hint, (px + qx) / 2, (py + qy) / 2, 1.8f * q);
    }
    for (int c = 0; c < n * n; c++) {           /* the numbers: faded once met, red when passed */
        float cx = bx + fr + (c % n + 0.5f) * q, cy = by + fr + (c / n + 0.5f) * q;
        bench_wave_at(&win, c % n + 0.5f, c / n + 0.5f, cx, cy, 2.2f * q);
        int want = G.p.clue[c];
        if (want < 0) continue;
        int has = slk_lines(&G, c), x = c % n, y = c / n, crossed = 0;
        const int round[4] = {slk_across(n, x, y), slk_across(n, x, y + 1), slk_down(n, x, y), slk_down(n, x + 1, y)};
        for (int e = 0; e < 4; e++) crossed += G.side[round[e]] == SLK_CROSS;
        int met = !solved && has == want && (want > 0 || crossed == 4);   /* a 0 fades once crossed round */
        const uint8_t *ink = !solved && has > want ? RED_INK : PUZ_INK;
        puz_number(&f_clue, cx + 0.02f * q, cy + 0.03f * q, 0.5f * q, want, PUZ_CREAM, (Uint8)(met ? 50 : 120));
        puz_number(&f_clue, cx, cy, 0.5f * q, want, ink, (Uint8)(met ? 105 : 255));
    }
    for (int s = 0; s < slk_sides(n); s++) {
        if (G.side[s] == SLK_LINE) wire(s, 255, solved);
        else if (G.side[s] == SLK_CROSS && !solved) {
            int x0, y0, x1, y1;
            slk_ends(n, s, &x0, &y0, &x1, &y1);
            float ax, ay, cx, cy;
            dot_xy(x0, y0, &ax, &ay);
            dot_xy(x1, y1, &cx, &cy);
            float d = 0.24f * q;
            ui_put_c(t_cross, (ax + cx) / 2, (ay + cy) / 2, d, d);
        }
    }
    float cap = 1.15f * wire_th;
    for (int y = 0; y <= n; y++)                /* the pins, and the wire's bends round them */
        for (int x = 0; x <= n; x++) {
            int lines = slk_dot_lines(&G, x, y);
            dot_xy(x, y, &px, &py);
            if (lines > 2 && !solved) ui_mark_glow(px, py, 0.7f * q);
            if (solved) SDL_SetTextureColorMod(t_cap, 255, 236, 140);
            float d = lines ? cap : 0.62f * cap;
            SDL_SetTextureAlphaMod(t_cap, lines ? 255 : 200);
            ui_put_c(t_cap, px, py, d, d);
            SDL_SetTextureAlphaMod(t_cap, 255);
            SDL_SetTextureColorMod(t_cap, 255, 255, 255);
        }
    bench_wave_tick(&win);
    ui_tool(0, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the stroke reached a pin: the sides from the last pin to it along a line drawn (or taken away) */
static void reach(int dot)
{
    int n = G.p.n, ax = fing.last % (n + 1), ay = fing.last / (n + 1), cx = dot % (n + 1), cy = dot / (n + 1);
    if (dot == fing.last || (ax != cx && ay != cy)) return;
    while (ax != cx || ay != cy) {
        int nx = ax + (cx > ax) - (cx < ax), ny = ay + (cy > ay) - (cy < ay);
        int s = ny == ay ? slk_across(n, ax < nx ? ax : nx, ay) : slk_down(n, ax, ay < ny ? ay : ny);
        if (!fing.sides) fing.erase = G.side[s] == SLK_LINE;   /* the first side says which */
        int state = fing.erase ? SLK_EMPTY : SLK_LINE;
        if (G.side[s] != state && (!fing.erase || G.side[s] == SLK_LINE)) {
            slk_stroke(&G, s, state, fing.stroke == 0);
            fing.stroke++;
            ui_sound_soft(fing.erase ? SND_TAP : SND_PLACE, 0.6f);
            after(s);
        }
        fing.sides++;
        ax = nx, ay = ny;
    }
    fing.last = dot;
}

static void press(float x, float y)
{
    memset(&fing, 0, sizeof fing);
    if (slk_solved(&G)) return;
    if (ui_tool_hit(x, y) == 0) {               /* the shell's tool: the hint */
        lamp();
        return;
    }
    fing.on = 1;
    fing.x0 = x;
    fing.y0 = y;
    fing.last = dot_at(x, y, 0.45f * q);
}

static void motion(float x, float y)
{
    if (!fing.on || slk_solved(&G)) return;
    if (!fing.moved && hypotf(x - fing.x0, y - fing.y0) > 0.3f * q) fing.moved = 1;
    if (!fing.moved) return;
    int d = dot_at(x, y, 0.3f * q);
    if (d < 0) return;
    if (fing.last < 0) fing.last = d;
    else reach(d);
    ui_animate();
}

static void release(float x, float y)
{
    if (!fing.on) return;
    fing.on = 0;
    ui_animate();
    if (fing.moved) return;                     /* a stroke: done as it went */
    int s = side_at(x, y);
    if (s < 0 || !slk_cycle(&G, s)) { ui_sound(SND_DENY); return; }
    ui_sound(G.side[s] == SLK_LINE ? SND_PLACE : SND_TAP);
    after(s);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the size and level with its loop, the level's ebony stones
 * under it ------------------------------------------------------------------------------------------------------ */
static void card_of(const slk_puzzle *pp, int variant, int size, int bare, uint8_t *rgba)
{
    const slk_puzzle p = *pp;
    int n = p.n, level = variant < 0 ? SLK_MEDIUM : slk_level_of(variant);
    int side = variant < 0 || bare ? size : (int)(0.84f * size);
    int fw = (int)(side * BENCH_FRAME), c = (int)((side - 2 * fw) / (n + 0.6f)), f = fw + (int)(0.3f * c);
    int w = n * c + 2 * f, o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_board(b, n, c, f, fw, c >= 12);   /* the pins and the numbers only where they show */
    for (int k = 0; k < n * n && c >= 10; k++)
        if (p.clue[k] >= 0) puz_burn(b, w, w, p.clue[k], f + (k % n + 0.5f) * c, f + (k / n + 0.52f) * c, 0.52f * c);
    {                                           /* its loop: a card that reads at a glance */
        static const uint8_t DEEP[3] = {150, 104, 40};   /* a small card: a darker brass, to stand out */
        const uint8_t *fill = c >= 12 ? BRASS : DEEP;
        float t = fmaxf(2.0f, 0.14f * c) / w, line = c >= 12 ? 0.002f : 0, light = c >= 12 ? 0.6f : 0.1f;
        art_fig *g = art_fig_new();
        int parts = 0;
        for (int s = 0; s < slk_sides(n); s++) {
            if (!p.sol[s]) continue;
            int x0, y0, x1, y1;
            slk_ends(n, s, &x0, &y0, &x1, &y1);
            float ax = (f + x0 * c) / (float)w, ay = (f + y0 * c) / (float)w;
            float cx = (f + x1 * c) / (float)w, cy = (f + y1 * c) / (float)w;
            float ex = x0 == x1 ? 0 : t / 2, ey = y0 == y1 ? 0 : t / 2;   /* to the corners' middles */
            art_band(g, ART_PART, ax - ex, ay - ey, cx + ex, cy + ey, t);
            if (++parts == 30) {                /* a figure holds a few dozen shapes: painted in batches */
                art_fig_paint(b, w, g, fill, BRASS_DARK, line, light);
                g = art_fig_new();
                parts = 0;
            }
        }
        art_fig_paint(b, w, g, fill, BRASS_DARK, line, light);
    }
    art_blend(rgba, size, size, b, w, w, o, variant < 0 ? o : 0);
    free(b);
    if (variant >= 0 && !bare) bench_stones(rgba, size, w, level);
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    static slk_puzzle p;
    slk_make(variant < 0 ? 5 : slk_size_of(variant), variant < 0 ? SLK_MEDIUM : slk_level_of(variant),
             2026u + (uint32_t)(variant + 1), &p);
    card_of(&p, variant, size, bare, rgba);
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* a chapter's tab: its last level with its loop, on the whole tab (the cells more and smaller as they go) */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static slk_game T;
    int ch = chapter < 0 ? 0 : chapter >= SLK_CHAPTERS ? SLK_CHAPTERS - 1 : chapter;
    if (slk_decode(&T, slk_levels[(ch + 1) * (SLK_NLEVELS / SLK_CHAPTERS) - 1])) card_of(&T.p, -1, size, 1, rgba);
}

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

static const game loop_game = {
    .id = "loop", .order = 448, .variants = SLK_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {SLK_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .score = score, .level_stars = stars, .fill = 1, .tools = 1,
    .levels = SLK_NLEVELS, .chapters = SLK_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
};
REGISTER_GAME(loop_game)
