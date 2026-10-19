/*
 * Masyu in RetroStone Board, for one player: the house plank ruled in cells, ivory and ebony pearls set in some of
 * them, the loop a brass wire from cell middle to cell middle, gold once it is solved; a crossed link a small dark
 * cross (the rules: masyu_rules.c; what the logic grids share: bench.h, bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag from cell to cell to lay the wire (a drag that begins along a line takes the lines away instead); tap the
 * place between two cells to lay or lift one length of it, hold there to cross it (a note: no line). The shell's
 * tools: what a tap does (a line, or a cross: tap it to swap tap and hold), and the hint (a wrong mark put right,
 * else the next link reasoning finds, in gold light). A pearl whose rule the wire breaks glows red, and so does a
 * cell with more than two lines. Undo takes back one action (a whole drag), all the way to the start. The bar: the
 * moves. Solved: the wire turns gold and a wave of gold light runs over the grid; the stars count the hints and the
 * mistakes (bench.h).
 * The shell's level screen chooses a level (150 in six chapters, made by tools/masyu_gen.c: masyu_levels.h), or its
 * die tab a puzzle made at random (the variants).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "games/bench_ui.h"
#include "games/masyu.h"
#include "games/masyu_levels.h"

static const uint8_t BRASS[3] = {222, 176, 84}, BRASS_DARK[3] = {104, 70, 24};

enum { K_LINE, K_CROSS };                       /* the mode tool: what a tap puts */
enum { T_MODE, T_HINT, TOOLS };

static msy_game G;
static bench_geo geo;
static bench_flash hint = {-1, 0};             /* at: a link */
static bench_wave win;
static int crosses;                             /* taps put crosses (the mode tool) */
static int serial;
/* the finger: down at (x0, y0) on a link (e) in a cell (a); a drag from cell to cell (last: the cell it reached last,
 * erase: taking lines away, links: the links it has run along, stroke: those it changed) */
static struct { int on, moved, e, a, last, erase, links, stroke; float x0, y0; Uint32 t0; } fing;

static box last_box;
static int made_key = -1, made_small = -1, wire_th;
static SDL_Texture *t_board, *t_small, *t_wire, *t_cap, *t_cross, *t_pearl[2];

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a brass wire across (len x th), lit along its middle: a round wire */
static void wire_pic(uint8_t *o, int len, int th)
{
    for (int y = 0; y < th; y++) {
        float v = (y + 0.5f) / th * 2 - 1, k = sqrtf(fmaxf(0, 1 - v * v));
        float shade = 0.5f + 0.6f * k + 0.3f * powf(fmaxf(0, 1 - fabsf(v + 0.4f) * 3), 2);
        for (int x = 0; x < len; x++) {
            uint8_t *p = o + 4 * ((size_t)y * len + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(BRASS[i] * shade, 0, 255);
            p[3] = (uint8_t)(255 * clampf((1 - fabsf(v)) * th / 2 + 0.3f, 0, 1));
        }
    }
}

static void cap_pic(int d, uint8_t *o)          /* the wire's bend: a brass ball */
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.48f, 0.48f);
    art_fig_paint(o, d, f, BRASS, BRASS_DARK, 0.04f, 0.8f);
}

static void cross_pic(int d, uint8_t *o)
{
    art_fig *f = art_fig_new();
    art_band(f, ART_PART, 0.2f, 0.2f, 0.8f, 0.8f, 0.18f);
    art_band(f, ART_PART, 0.8f, 0.2f, 0.2f, 0.8f, 0.18f);
    art_fig_paint(o, d, f, PUZ_INK, NULL, 0, 0);
}

static void white_pic(int d, uint8_t *o) { art_counter(COUNTER_STONE, 0, d, o); }
static void black_pic(int d, uint8_t *o) { art_counter(COUNTER_STONE, 1, d, o); }

static SDL_Texture *pic_tex(void (*paint)(int, uint8_t *), int q)
{
    memset(ui_buf(q, q), 0, (size_t)q * q * 4);
    paint(q, ui_buf(q, q));
    return ui_tex(q, q, ui_buf(q, q));
}

/* the loop of a solution (lines[] 1: a line) and the pearls painted into a picture laid out by g */
static void paint_puzzle_into(const bench_geo *g, const msy_puzzle *p, const uint8_t *lines, uint8_t *o)
{
    int n = p->n, q = g->cs, W = g->w;
    float u = 1.0f / W, t = fmaxf(2.0f, 0.16f * q) * u;
    if (lines) {
        art_fig *f = art_fig_new();
        int parts = 0;
        for (int e = 0; e < msy_links(n); e++) {
            if (!lines[e]) continue;
            int a, b;
            msy_ends(n, e, &a, &b);
            float ax = (g->ox + (a % n + 0.5f) * q) * u, ay = (g->oy + (a / n + 0.5f) * q) * u;
            float bx = (g->ox + (b % n + 0.5f) * q) * u, by = (g->oy + (b / n + 0.5f) * q) * u;
            float ex = a / n == b / n ? t / 2 : 0, ey = a / n == b / n ? 0 : t / 2;
            art_band(f, ART_PART, ax - ex, ay - ey, bx + ex, by + ey, t);
            if (++parts == 30) {                /* a figure holds a few dozen shapes: painted in batches */
                art_fig_paint(o, W, f, BRASS, BRASS_DARK, q >= 12 ? 0.002f : 0, 0.5f);
                f = art_fig_new();
                parts = 0;
            }
        }
        art_fig_paint(o, W, f, BRASS, BRASS_DARK, q >= 12 ? 0.002f : 0, 0.5f);
    }
    int d = (int)fmaxf(3, 0.66f * q);
    uint8_t *pb = calloc((size_t)d * d, 4);
    if (!pb) return;
    for (int kind = MSY_WHITE; kind <= MSY_BLACK; kind++) {
        memset(pb, 0, (size_t)d * d * 4);
        (kind == MSY_WHITE ? white_pic : black_pic)(d, pb);
        for (int c = 0; c < n * n; c++)
            if (p->pearl[c] == kind)
                art_blend(o, W, g->h, pb, d, d, (int)(g->ox + (c % n + 0.5f) * q - d / 2.0f),
                          (int)(g->oy + (c / n + 0.5f) * q - d / 2.0f));
    }
    free(pb);
}

static void paint_board(const bench_geo *g, uint8_t *o) { bench_paint_grid(g, o); }

/* the token: a wooden tile, a white and a black pearl on a bend of wire */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.86f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    art_fig *f = art_fig_new();
    float t = fmaxf(2.0f, 0.1f * w) / w;
    art_band(f, ART_PART, 0.27f - t / 2, 0.27f, 0.73f, 0.27f, t);
    art_band(f, ART_PART, 0.73f, 0.27f - t / 2, 0.73f, 0.8f, t);
    art_fig_paint(b, w, f, BRASS, BRASS_DARK, 0.003f, 0.5f);
    int d = (int)(0.4f * w);
    uint8_t *pb = calloc((size_t)d * d, 4);
    if (pb) {
        white_pic(d, pb);
        art_blend(b, w, w, pb, d, d, (int)(0.3f * w - d / 2.0f), (int)(0.27f * w - d / 2.0f));
        memset(pb, 0, (size_t)d * d * 4);
        black_pic(d, pb);
        art_blend(b, w, w, pb, d, d, (int)(0.73f * w - d / 2.0f), (int)(0.27f * w - d / 2.0f));
        free(pb);
    }
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    geo.cols = geo.rows = G.p.n;
    int changed = bench_fit(&geo, b), key = serial % 4096 * 16 + G.p.n;
    if (changed || made_key != key) {
        made_key = key;
        int q = geo.cs;
        ui_free(&t_board);
        t_board = bench_tex(&geo, paint_board);
        wire_th = (int)fmaxf(3, 0.16f * q);
        int len = 6 * wire_th;
        ui_free(&t_wire);
        wire_pic(ui_buf(len, wire_th), len, wire_th);
        t_wire = ui_tex(len, wire_th, ui_buf(len, wire_th));
        ui_free(&t_cap);
        t_cap = pic_tex(cap_pic, (int)(1.15f * wire_th) | 1);
        ui_free(&t_cross);
        t_cross = pic_tex(cross_pic, (int)fmaxf(4, 0.26f * q));
        int d = (int)fmaxf(4, 0.66f * q);
        ui_free(&t_pearl[0]);
        t_pearl[0] = pic_tex(white_pic, d);
        ui_free(&t_pearl[1]);
        t_pearl[1] = pic_tex(black_pic, d);
    }
    bench_used(&geo);
    bench_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(&fing, 0, sizeof fing);
    hint.at = -1;
    win.on = 0;
}

static void begin(void)
{
    serial++;
    calm();
    crosses = 0;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    msy_puzzle p;
    if (!msy_decode(msy_levels[level < 0 || level >= MSY_LEVELS ? 0 : level], &p) || !msy_load(&G, &p))
        msy_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    msy_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return msy_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int e)
{
    if (msy_solved(&G) && !win.on) {
        int a, b;
        msy_ends(G.p.n, e < 0 ? 0 : e, &a, &b);
        bench_wave_from(&win, a, G.p.n, G.p.n);
        hint.at = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (msy_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = msy_script(&G, moves);
    after(0);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void link_mid(int e, float *x, float *y)
{
    int a, b;
    msy_ends(G.p.n, e, &a, &b);
    *x = (bench_mid_x(&geo, a) + bench_mid_x(&geo, b)) / 2;
    *y = (bench_mid_y(&geo, a) + bench_mid_y(&geo, b)) / 2;
}

/* the link nearest the point, if near enough (-1: none) */
static int link_at(float x, float y)
{
    int best = -1;
    float bd = 0.5f * geo.cs;
    for (int e = 0; e < msy_links(G.p.n); e++) {
        int a, b;
        msy_ends(G.p.n, e, &a, &b);
        float ax = bench_mid_x(&geo, a), ay = bench_mid_y(&geo, a);
        float cx = bench_mid_x(&geo, b), cy = bench_mid_y(&geo, b);
        float t = ay == cy ? clampf((x - ax) / geo.cs, 0, 1) : clampf((y - ay) / geo.cs, 0, 1);
        float d = hypotf(x - (ax + (cx - ax) * t), y - (ay + (cy - ay) * t));
        if (d < bd) bd = d, best = e;
    }
    return best;
}

static void wire(int e, int gold)
{
    int a, b;
    msy_ends(G.p.n, e, &a, &b);
    float mx, my, len = (float)geo.cs, th = (float)wire_th;
    link_mid(e, &mx, &my);
    int across = a / G.p.n == b / G.p.n;
    if (gold) SDL_SetTextureColorMod(t_wire, 255, 236, 140);
    ui_fill(across ? mx - len / 2 : mx - th / 2 + 0.04f * len, across ? my - th / 2 + 0.05f * len : my - len / 2,
            across ? len : th, across ? th : len, 20, 12, 6, 64);   /* its shadow */
    if (across) ui_put_c(t_wire, mx, my, len, th);
    else ui_put_rot(t_wire, mx, my, len, th, 90);
    SDL_SetTextureColorMod(t_wire, 255, 255, 255);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, q = geo.cs, solved = msy_solved(&G);
    ui_put(t_board, geo.bx, geo.by, (float)geo.w, (float)geo.h);
    float x, y;
    for (int e = 0; e < msy_links(n); e++) {
        if (G.st[e] == MSY_LINE) wire(e, solved);
        else if (G.st[e] == MSY_CROSS && !solved) {
            link_mid(e, &x, &y);
            ui_put_c(t_cross, x, y, 0.26f * q, 0.26f * q);
        }
    }
    float cap = 1.15f * wire_th, d = 0.66f * q;
    for (int c = 0; c < n * n; c++) {           /* the bends, the faults, the pearls */
        float mx = bench_mid_x(&geo, c), my = bench_mid_y(&geo, c);
        int lines = msy_lines(&G, c), pl = G.p.pearl[c];
        if (!solved && (lines > 2 || msy_broken(&G, c))) ui_mark_glow(mx, my, 0.95f * q);
        if (lines && !pl) {
            if (solved) SDL_SetTextureColorMod(t_cap, 255, 236, 140);
            ui_put_c(t_cap, mx, my, cap, cap);
            SDL_SetTextureColorMod(t_cap, 255, 255, 255);
        }
        if (pl) ui_put_c(t_pearl[pl - 1], mx, my, d, d);
    }
    if (fing.on && !fing.moved && fing.e >= 0) {   /* a press growing into a cross (or a line): a ring */
        float f = (ui_ms() - fing.t0) / (float)BENCH_HOLD_MS;
        if (f > 0.25f && f < 1) {
            link_mid(fing.e, &x, &y);
            ui_mark_select_round(x, y, q * (0.4f + 0.6f * f));
        }
        if (f < 1) ui_animate();
    }
    if (hint.at >= 0) {
        link_mid(hint.at, &x, &y);
        bench_glow(&hint, x, y, 1.8f * q);
    }
    bench_wave_cells(&win, &geo, 2.3f);
}

/* the mode tool's picture: a length of wire, or a cross */
static void paint_button(int kind, int size, uint8_t *o)
{
    if (kind == K_CROSS) {
        cross_pic(size, o);
        return;
    }
    art_fig *f = art_fig_new();
    art_band(f, ART_PART, 0.12f, 0.5f, 0.88f, 0.5f, 0.2f);
    art_fig_paint(o, size, f, BRASS, BRASS_DARK, 0.02f, 0.6f);
}

static void draw_all(void)
{
    draw();
    int solved = msy_solved(&G);
    ui_tool_paint(T_MODE, paint_button, crosses ? K_CROSS : K_LINE, 0.72f, solved ? UI_DIM : crosses ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void tool(int t)
{
    if (t == T_MODE) {
        crosses = !crosses;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    int e = msy_hint(&G);
    if (e < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, e);
    ui_sound(SND_PLACE);
    after(e);
}

/* the drag reached a cell: the links from the last cell to it, along a row or a column, drawn (or taken away) */
static void reach(int c)
{
    int n = G.p.n, a = fing.last;
    if (c == a || (c % n != a % n && c / n != a / n)) return;
    int step = c % n == a % n ? (c > a ? n : -n) : (c > a ? 1 : -1);
    for (; a != c; a += step) {
        int e = msy_link(n, a, a + step);
        if (!fing.links) fing.erase = G.st[e] == MSY_LINE;   /* the first link says which */
        int to = fing.erase ? MSY_EMPTY : MSY_LINE;
        if (G.st[e] != to && (!fing.erase || G.st[e] == MSY_LINE) && msy_stroke(&G, e, to, fing.stroke == 0)) {
            fing.stroke++;
            ui_sound_soft(fing.erase ? SND_TAP : SND_PLACE, 0.6f);
            after(e);
        }
        fing.links++;
    }
    fing.last = c;
}

static void press(float x, float y)
{
    memset(&fing, 0, sizeof fing);
    if (msy_solved(&G)) return;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { tool(t); return; }
    fing.e = link_at(x, y);
    fing.a = fing.last = bench_cell_at(&geo, x, y);
    if (fing.e < 0 && fing.a < 0) return;
    fing.on = 1;
    fing.x0 = x;
    fing.y0 = y;
    fing.t0 = ui_ms();
    ui_animate();
}

static void motion(float x, float y)
{
    if (!fing.on) return;
    if (!fing.moved && hypotf(x - fing.x0, y - fing.y0) > 0.3f * geo.cs) fing.moved = 1;
    if (!fing.moved) return;
    int c = bench_cell_at(&geo, x, y);
    if (c < 0) return;
    if (fing.last < 0) fing.last = c;
    else reach(c);
    ui_animate();
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!fing.on) return;
    fing.on = 0;
    ui_animate();
    if (fing.moved) return;                     /* a drag: done as it went */
    int e = fing.e, held = ui_ms() - fing.t0 >= BENCH_HOLD_MS;
    if (e < 0) { ui_sound(SND_DENY); return; }
    int to = held != crosses ? msy_hold_state(&G, e) : msy_tap_state(&G, e);
    if (!msy_set(&G, e, to)) { ui_sound(SND_DENY); return; }
    ui_sound(to == MSY_LINE ? SND_PLACE : SND_TAP);
    after(e);
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the pictures of puzzles: the menu's and the picker's cards, the chapters' tabs ----------------------------- *
 * A puzzle with its loop (lines; NULL: none), side x side, at the top of the card (centred: mid) */
static void paint_puzzle(const msy_puzzle *p, const uint8_t *lines, int size, int side, int mid, uint8_t *rgba)
{
    bench_geo g;
    memset(&g, 0, sizeof g);
    g.cols = g.rows = p->n;
    bench_fit(&g, (box){0, 0, (float)side, (float)side});
    uint8_t *b = calloc((size_t)g.w * g.h, 4);
    if (!b) return;
    bench_paint_grid(&g, b);
    paint_puzzle_into(&g, p, lines, b);
    int ox = (size - side) / 2 + (int)g.bx, oy = (mid ? (size - side) / 2 : 0) + (int)g.by;
    art_blend(rgba, size, size, b, g.w, g.h, ox, oy);
    free(b);
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant, side = variant < 0 || bare ? size : (int)(0.84f * size);
    msy_puzzle p;
    uint8_t sol[MSY_LINKS];
    msy_make(msy_size(v), msy_level(v), 2026u + (uint32_t)v, &p, sol);
    paint_puzzle(&p, sol, size, side, variant < 0, rgba);
    if (variant >= 0 && !bare) bench_stones(rgba, size, side, msy_level(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest size), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= MSY_CHAPTERS ? MSY_CHAPTERS - 1 : chapter;
    static msy_game T;
    msy_puzzle p;
    if (!msy_decode(msy_levels[(ch + 1) * MSY_LEVELS / MSY_CHAPTERS - 1], &p) || !msy_load(&T, &p)) return;
    paint_puzzle(&p, T.sol, size, (int)(0.92f * size), 1, rgba);
}

static const game masyu_game = {
    .id = "masyu", .order = 421, .variants = MSY_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw_all,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {MSY_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .levels = MSY_LEVELS, .chapters = MSY_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(masyu_game)
