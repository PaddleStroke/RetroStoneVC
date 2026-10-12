/*
 * Untangle in RetroStone Board, for one player: brass pins on the house plank, dark cords between them (the rules:
 * untangle_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The shell's level screen chooses a level (150 in six chapters, untangle_levels.h: a few pins moved out of a layout
 * without crossings), or its die tab a puzzle made at random (the variants: the pins round a circle). Stars: the
 * moves against par (untangle.h), a hint caps them (pzkit.h).
 * Drag a pin anywhere on the board: its cords follow, and every cord that crosses another glows red, there and then.
 * When none crosses, they all turn gold. The board takes the room the shell gives (game.fill). The bar: the moves,
 * the crossings left (a small red cross of cords). The shell's tool: the hint (a pin moved where it lowers the
 * crossings most, or set where it sits in a layout without any, sliding there in gold light). Undo takes back one
 * move, all the way to the start.
 * Solved: the cords turn gold and a gold light runs over the pins from the last one moved.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/untangle.h"
#include "games/untangle_levels.h"
#include "shell.h"

#define FRAME 0.035f
#define SLIDE_MS 420                            /* a hint's pin sliding to its place */
#define HINT_MS 1300
#define WAVE_STEP 260                           /* the win's light: ms a board's width further on */
#define WAVE_LEN 560

static const uint8_t BRASS[3] = {218, 176, 88};

static ut_game G;
static struct { int on, v, moved; float dx, dy, x0, y0, ox, oy; } ptr;   /* a pin held: its offset, where it was */
static struct { int v; float fx, fy; Uint32 t0; } slide = {-1, 0, 0, 0};
static struct { int on, from; Uint32 t0; } win;

static box last_box;
static float bx, by, side, px0, py0, pw, dot;   /* the field: its corner, its side; a pin's size */
static int fr, made_key = -1, made_small = -1, made_dot = -1;
static SDL_Texture *t_board, *t_pin, *t_small, *t_knot;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a brass pin seen from above: its round head, lit from the top left, a dark rim */
static void paint_pin(uint8_t *o, int s)
{
    float c = s / 2.0f;
    pzk_ball(o, s, s, c, c, 0.46f * s, PZK_DARK, 1, 0);
    pzk_ball(o, s, s, c, c, 0.4f * s, BRASS, 1, 0.9f);
    const uint8_t white[3] = {255, 250, 230};
    pzk_ball(o, s, s, c - 0.13f * s, c - 0.14f * s, 0.09f * s, white, 0.7f, 0);
}

/* the token: three pins and their cords on a wooden tile */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(size * 0.9f), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.05f * w), 0.3f);
    float W = (float)w, p[4][2] = {{0.22f, 0.3f}, {0.78f, 0.24f}, {0.66f, 0.78f}, {0.26f, 0.7f}};
    const uint8_t cord[3] = {214, 168, 64};
    for (int i = 0; i < 4; i++)
        pzk_stroke(b, w, w, p[i][0] * W, p[i][1] * W, p[(i + 1) % 4][0] * W, p[(i + 1) % 4][1] * W, 0.035f * W, cord, 1);
    for (int i = 0; i < 4; i++) {
        pzk_ball(b, w, w, p[i][0] * W, p[i][1] * W, 0.1f * W, PZK_DARK, 1, 0);
        pzk_ball(b, w, w, p[i][0] * W, p[i][1] * W, 0.08f * W, BRASS, 1, 0.9f);
    }
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

/* the bar's small mark for the crossings: two red cords crossed */
static void paint_knot(uint8_t *o, int s)
{
    float S = (float)s;
    pzk_stroke(o, s, s, 0.15f * S, 0.25f * S, 0.85f * S, 0.75f * S, 0.07f * S, PZK_RED, 1);
    pzk_stroke(o, s, s, 0.15f * S, 0.75f * S, 0.85f * S, 0.25f * S, 0.07f * S, PZK_RED, 1);
    pzk_ball(o, s, s, 0.15f * S, 0.25f * S, 0.1f * S, BRASS, 1, 0.6f);
    pzk_ball(o, s, s, 0.85f * S, 0.75f * S, 0.1f * S, BRASS, 1, 0.6f);
    pzk_ball(o, s, s, 0.15f * S, 0.75f * S, 0.1f * S, BRASS, 1, 0.6f);
    pzk_ball(o, s, s, 0.85f * S, 0.25f * S, 0.1f * S, BRASS, 1, 0.6f);
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h));
    side = S;
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    fr = (int)(S * FRAME);
    int nv = G.nv ? G.nv : 10;
    dot = floorf(fminf(0.52f * ui.unit, 0.3f * (S - 2 * fr) / sqrtf((float)nv)));
    pw = S - 2 * fr - dot;
    px0 = bx + fr + dot / 2;
    py0 = by + fr + dot / 2;
    int key = (int)S;
    if (made_key != key) {
        made_key = key;
        int w = (int)S;
        ui_free(&t_board);
        art_board_n(1, 1, w - 2 * fr, fr, 0, ui_buf(w, w));
        t_board = ui_tex(w, w, ui_buf(w, w));
    }
    if (made_dot != (int)dot) {
        made_dot = (int)dot;
        int s = (int)dot * 2;                   /* drawn at twice its size: sharp when lifted */
        ui_free(&t_pin);
        memset(ui_buf(s, s), 0, (size_t)s * s * 4);
        paint_pin(ui_buf(s, s), s);
        t_pin = ui_tex(s, s, ui_buf(s, s));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        token(0, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
        int k = (int)(0.5f * ui.unit);
        ui_free(&t_knot);
        memset(ui_buf(k, k), 0, (size_t)k * k * 4);
        paint_knot(ui_buf(k, k), k);
        t_knot = ui_tex(k, k, ui_buf(k, k));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(&ptr, 0, sizeof ptr);
    slide.v = -1;
    win.on = 0;
}

static void after(int v)
{
    if (ut_solved(&G) && !win.on) {
        win.on = 1;
        win.from = v;
        win.t0 = ui_ms();
    }
    ui_animate();
}

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!ut_decode(&G, ut_levels[level < 0 || level >= UT_LEVELS ? 0 : level])) ut_new(&G, 0, 1);
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    ut_new(&G, variant < 0 || variant >= UT_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int score(void) { return G.moves; }
static int par(void) { return G.par; }
static int level_stars(void) { return pzk_stars(ui_par_stars(G.moves, G.par), G.hints, 0); }

static int status(void) { return ut_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static Uint32 win_end(void) { return (Uint32)(1.5f * WAVE_STEP + WAVE_LEN); }
static int busy(void) { return win.on && ui_ms() - win.t0 < win_end(); }

static int undo(void)
{
    int v = ut_undo(&G);
    if (v < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = ut_script(&G, moves);
    after(0);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void at(int v, Uint32 now, float *x, float *y)   /* a pin on the screen (a hint's sliding there) */
{
    float fx = G.x[v], fy = G.y[v];
    if (v == slide.v) {
        float t = (now - slide.t0) / (float)SLIDE_MS;
        if (t < 1) {
            t = puz_ease(t);
            fx = slide.fx + (fx - slide.fx) * t;
            fy = slide.fy + (fy - slide.fy) * t;
            ui_animate();
        }
    }
    *x = px0 + fx * pw;
    *y = py0 + fy * pw;
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    int solved = ut_solved(&G);
    ui_put(t_board, bx, by, side, side);
    float X[UT_MAXV], Y[UT_MAXV], cw = fmaxf(2.5f, 0.15f * dot);
    for (int v = 0; v < G.nv; v++) at(v, now, &X[v], &Y[v]);
    uint8_t crossed[UT_MAXE];
    for (int i = 0; i < G.ne; i++) crossed[i] = (uint8_t)ut_crossed(&G, i);
    if (slide.v >= 0) {                         /* the hint's pin: a gold light on it */
        float p = puz_pulse((float)(now - slide.t0), 0, HINT_MS);
        if (p > 0) {
            ui_mark_gold(X[slide.v], Y[slide.v], 3.2f * dot * p);
            ui_animate();
        } else {
            slide.v = -1;
        }
    }
    /* the glow under the cords: red under the crossing ones, gold under all when solved */
    for (int i = 0; i < G.ne; i++) {
        int a = G.e[i][0], b = G.e[i][1];
        if (solved) pzk_line(X[a], Y[a], X[b], Y[b], 2 * cw, 2.5f * cw, (pzk_rgba){250, 200, 90, 120}, 1);
        else if (crossed[i]) pzk_line(X[a], Y[a], X[b], Y[b], 1.5f * cw, 2.2f * cw, (pzk_rgba){230, 50, 30, 90}, 0);
    }
    for (int pass = 0; pass < 2; pass++)        /* the cords: the held pin's last, on top */
        for (int i = 0; i < G.ne; i++) {
            int a = G.e[i][0], b = G.e[i][1], held = ptr.on && (a == ptr.v || b == ptr.v);
            if (held != pass) continue;
            pzk_rgba c = solved ? (pzk_rgba){236, 190, 86, 255}
                         : crossed[i] ? (pzk_rgba){204, 52, 38, 255}
                         : held ? (pzk_rgba){120, 84, 56, 255} : (pzk_rgba){62, 42, 30, 240};
            pzk_line(X[a], Y[a], X[b], Y[b], cw, 0, c, 0);
        }
    float wt = win.on ? (float)(now - win.t0) : -1;
    for (int v = 0; v < G.nv; v++) {            /* the pins */
        int held = ptr.on && v == ptr.v;
        float d = held ? 1.3f * dot : dot;
        if (wt >= 0) {
            int f = win.from >= 0 && win.from < G.nv ? win.from : 0;
            float dist = hypotf(G.x[v] - G.x[f], G.y[v] - G.y[f]);
            float p = puz_pulse(wt, dist * WAVE_STEP, WAVE_LEN);
            if (p > 0) ui_mark_gold(X[v], Y[v], 3.4f * dot * p);
        }
        if (held) ui_mark_gold(X[v], Y[v], 2.4f * dot);
        ui_shadow_flat(X[v] + (held ? 0.18f : 0.06f) * dot, Y[v] + (held ? 0.22f : 0.08f) * dot, d, d, held ? 0.6f : 0.5f);
        ui_put_c(t_pin, X[v], Y[v], d, d);
    }
    if (wt >= 0) {
        if (wt < win_end()) ui_animate();
        else win.on = 0;
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int pin_at(float x, float y)
{
    int best = -1;
    float bd = fmaxf(0.75f * dot, 0.42f * ui.unit);
    for (int v = 0; v < G.nv; v++) {
        float d = hypotf(px0 + G.x[v] * pw - x, py0 + G.y[v] * pw - y);
        if (d < bd) bd = d, best = v;
    }
    return best;
}

static void use_tool(int t);

static void press(float x, float y)
{
    int tool = ui_tool_hit(x, y);
    if (tool >= 0) {
        memset(&ptr, 0, sizeof ptr);
        use_tool(tool);
        return;
    }
    memset(&ptr, 0, sizeof ptr);
    if (ut_solved(&G)) return;
    int v = pin_at(x, y);
    if (v < 0) return;
    slide.v = -1;
    ptr.on = 1;
    ptr.v = v;
    ptr.ox = G.x[v];
    ptr.oy = G.y[v];
    ptr.dx = px0 + G.x[v] * pw - x;
    ptr.dy = py0 + G.y[v] * pw - y;
    ptr.x0 = x;
    ptr.y0 = y;
    ui_sound(SND_TAP);
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    if (!ptr.moved && hypotf(x - ptr.x0, y - ptr.y0) > 0.12f * dot) ptr.moved = 1;
    if (!ptr.moved) return;
    G.x[ptr.v] = clampf((x + ptr.dx - px0) / pw, UT_MIN, 1 - UT_MIN);   /* live: the crossings as it goes */
    G.y[ptr.v] = clampf((y + ptr.dy - py0) / pw, UT_MIN, 1 - UT_MIN);
    ui_animate();
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    motion(x, y);
    ptr.on = 0;
    ui_animate();
    float nx = G.x[ptr.v], ny = G.y[ptr.v];
    G.x[ptr.v] = ptr.ox;                        /* the move made as one, from where it was */
    G.y[ptr.v] = ptr.oy;
    if (!ptr.moved || !ut_move(&G, ptr.v, nx, ny)) return;
    ui_sound(SND_PLACE);
    after(ptr.v);
}

/* ---- the bar: the moves, the crossings left, the hint's lamp ----------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    ui_count c[2] = {{t_small, G.moves}, {t_knot, ut_crossings(&G)}};
    ui_bar_fit(b, c, 2, 0);
}

static void use_hint(void)
{
    float ox[UT_MAXV], oy[UT_MAXV];
    memcpy(ox, G.x, sizeof ox);
    memcpy(oy, G.y, sizeof oy);
    int v = ut_hint(&G);
    if (v < 0) { ui_sound(SND_DENY); return; }
    slide.v = v;
    slide.fx = ox[v];
    slide.fy = oy[v];
    slide.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after(v);
}

static void use_tool(int t)
{
    if (t == 0) use_hint();
}

/* ---- the menu's and the picker's cards: the picker's a puzzle of the size as it starts (its pins round a circle, its
 * cords crossing in red); the menu's one untangled, its cords gold -------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static ut_game T;
    ut_new(&T, variant < 0 ? 1 : variant, 777u + (uint32_t)(variant < 0 ? 1 : variant));
    if (variant < 0) {
        memcpy(T.x, T.hx, sizeof T.x);
        memcpy(T.y, T.hy, sizeof T.y);
    }
    int f = (int)(size * FRAME);
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    art_board_n(1, 1, size - 2 * f, f, 0, b);
    float d = fminf(0.07f * size, 0.36f * (size - 2 * f) / sqrtf((float)T.nv)), p = size - 2 * f - d, o = f + d / 2;
    const uint8_t gold[3] = {226, 178, 70}, ink[3] = {62, 42, 30};
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < T.ne; i++) {
            int a = T.e[i][0], c = T.e[i][1], x = ut_crossed(&T, i);
            if (x != pass) continue;
            pzk_stroke(b, size, size, o + T.x[a] * p, o + T.y[a] * p, o + T.x[c] * p, o + T.y[c] * p,
                       fmaxf(0.6f, 0.08f * d), variant < 0 ? gold : x ? PZK_RED : ink, 1);
        }
    for (int v = 0; v < T.nv; v++) {
        pzk_ball(b, size, size, o + T.x[v] * p, o + T.y[v] * p, 0.48f * d, PZK_DARK, 1, 0);
        pzk_ball(b, size, size, o + T.x[v] * p, o + T.y[v] * p, 0.4f * d, BRASS, 1, 0.9f);
    }
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
}

/* a chapter's tab: pins and their cords, more of them chapter by chapter, two of them crossing in red */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int n = 4 + (chapter < 0 ? 0 : chapter > 5 ? 5 : chapter);   /* pins round a ring, a cord to the next */
    float S = (float)size, c = S / 2, R = 0.36f * S, px[9], py[9];
    for (int i = 0; i < n; i++) {
        float a = -PUZ_PI / 2 + 2 * PUZ_PI * i / n;
        px[i] = c + R * cosf(a);
        py[i] = c + R * sinf(a);
    }
    const uint8_t ink[3] = {62, 42, 30};
    for (int i = 0; i < n; i++)
        pzk_stroke(rgba, size, size, px[i], py[i], px[(i + 1) % n], py[(i + 1) % n], fmaxf(0.8f, 0.03f * S), ink, 1);
    pzk_stroke(rgba, size, size, px[0], py[0], px[n / 2], py[n / 2], fmaxf(0.8f, 0.03f * S), PZK_RED, 1);
    pzk_stroke(rgba, size, size, px[1], py[1], px[n - 1], py[n - 1], fmaxf(0.8f, 0.03f * S), PZK_RED, 1);
    for (int i = 0; i < n; i++) {
        pzk_ball(rgba, size, size, px[i], py[i], 0.085f * S, PZK_DARK, 1, 0);
        pzk_ball(rgba, size, size, px[i], py[i], 0.068f * S, BRASS, 1, 0.9f);
    }
}

/* the board, then the shell's tools: the hint */
static void draw_all(void)
{
    draw();
    ui_tool(0, ICON_HINT, ut_solved(&G) ? UI_DIM : 0);
}

static const game untangle_game = {
    .id = "untangle", .order = 404, .variants = UT_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw_all,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 1,
    .levels = UT_LEVELS, .chapters = UT_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .level_stars = level_stars, .score = score, .par = par,
};
REGISTER_GAME(untangle_game)
