/*
 * Lights in RetroStone Board, for one player: brass lamps set in the house plank, their glass domes glowing warm when
 * lit and smoky dark when not, in all the room the shell gives; the hint is one of the shell's tools (the rules:
 * lights_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a lamp: it and its neighbours turn over, the pressed one first, the others a moment later. The hint presses a
 * lamp of the fewest way (in gold light). Undo takes back one press, all the way to the start. The bar: the presses,
 * and at its right end the fewest possible from the start by a lit lamp (in gold when solved in that many). Solved:
 * a wave of gold light runs over the dark lamps from the last one pressed.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/lights.h"
#include "games/puzzles.h"
#include "games/pztools.h"
#include "shell.h"

#define FRAME 0.035f
#define FADE_MS 170
#define RIPPLE_MS 55                            /* the neighbours turn this much after the lamp pressed */
#define HINT_MS 1100
#define WAVE_STEP 70
#define WAVE_LEN 520

enum { B_HINT, BTNS };

static lmp_game G;
static Uint32 turn_at[LMP_CELLS];               /* when each lamp last turned over (its fade) */
static struct { int cell; Uint32 t0; } hint = {-1, 0};
static struct { int on, from; Uint32 t0; } win;
static int down = -1;                           /* the lamp under the finger */

static box last_box;
static float gx, gy, gw;
static int cs, fr, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_lit, *t_dark, *t_small, *t_par;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a glass dome seen from above (size x size): lit (warm, bright in the middle) or dark (smoky), a highlight at its
 * top left */
static void paint_glass(int size, int lit, uint8_t *o)
{
    float r = size / 2.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = (x + 0.5f - r) / r, dy = (y + 0.5f - r) / r, d = hypotf(dx, dy);
            uint8_t *p = o + 4 * (y * size + x);
            float a = clampf((1 - d) * r + 0.5f, 0, 1);
            if (a <= 0) { p[3] = 0; continue; }
            float c[3];
            if (lit) {
                float k = 1 - d * d;
                c[0] = 236 + 19 * k, c[1] = 150 + 94 * k, c[2] = 52 + 140 * k * k;
            } else {
                float k = 0.75f + 0.25f * (1 - d);
                c[0] = 74 * k, c[1] = 56 * k, c[2] = 40 * k;
            }
            float h = expf(-((dx + 0.34f) * (dx + 0.34f) + (dy + 0.38f) * (dy + 0.38f)) * 14);   /* the highlight */
            float rim = clampf((d - 0.82f) / 0.18f, 0, 1);                                    /* a darker rim */
            for (int i = 0; i < 3; i++) {
                float v = c[i] * (1 - 0.3f * rim) + (255 - c[i]) * h * (lit ? 0.75f : 0.45f);
                p[i] = (uint8_t)clampf(v, 0, 255);
            }
            p[3] = (uint8_t)(255 * a);
        }
}

/* a brass bezel set in a picture (w wide) round (cx, cy): a ring lit from the top left, its hole shaded */
static void paint_bezel(uint8_t *o, int w, float cx, float cy, float r_in, float r_out)
{
    for (int y = (int)(cy - r_out - 2); y <= (int)(cy + r_out + 2); y++)
        for (int x = (int)(cx - r_out - 2); x <= (int)(cx + r_out + 2); x++) {
            if (x < 0 || y < 0 || x >= w || y >= w) continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = hypotf(dx, dy);
            uint8_t *p = o + 4 * (y * w + x);
            float out = clampf(r_out - d + 0.5f, 0, 1), in = clampf(d - r_in + 0.5f, 0, 1);
            float lit = d > 0 ? -(dx + dy) / (1.414f * d) : 0, t = (d - r_in) / (r_out - r_in);
            if (out > 0 && in > 0) {            /* the ring: brass, its top left lit, a groove in the middle */
                float k = 1 + 0.32f * lit * (t < 0.5f ? -1 : 1) - 0.18f * expf(-(t - 0.5f) * (t - 0.5f) * 60);
                float c[3] = {PZT_BRASS[0] * k, PZT_BRASS[1] * k, PZT_BRASS[2] * k}, a = out * in;
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] + (c[i] - p[i]) * a, 0, 255);
            } else if (out > 0) {               /* the hole: dark under the glass */
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * 0.3f);
            } else if (d < r_out + 2.5f) {      /* the ring's shadow on the wood, to the lower right */
                float k = 1 - 0.25f * clampf((dx + dy) / (1.414f * d), 0, 1) * (1 - (d - r_out) / 2.5f);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
            }
        }
}

static void paint_board(uint8_t *o, int n, int q, int f)
{
    int w = n * q + 2 * f;
    art_board_n(n, n, q, f, 0, o);
    for (int c = 0; c < n * n; c++)
        paint_bezel(o, w, f + (c % n + 0.5f) * q, f + (c / n + 0.5f) * q, 0.34f * q, 0.43f * q);
}

/* a lamp, lit, in its bezel (the token, the bar's pictures) */
static void lamp_pic(int size, int lit, uint8_t *rgba)
{
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    for (int i = 0; i < size * size; i++) b[4 * i + 3] = 0;
    paint_bezel(b, size, size / 2.0f, size / 2.0f, 0.36f * size, 0.48f * size);
    for (int i = 0; i < size * size; i++) {    /* the bezel alone: no wood round it */
        float dx = i % size + 0.5f - size / 2.0f, dy = i / size + 0.5f - size / 2.0f;
        b[4 * i + 3] = (uint8_t)(255 * clampf(0.48f * size - hypotf(dx, dy) + 0.5f, 0, 1));
    }
    int g = (int)(0.74f * size);
    uint8_t *gl = calloc((size_t)g * g, 4);
    if (gl) {
        paint_glass(g, lit, gl);
        art_blend(b, size, size, gl, g, g, (size - g) / 2, (size - g) / 2);
        free(gl);
    }
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    lamp_pic(size, 1, rgba);
}

static void layout(box b)
{
    last_box = b;
    box r = b;                                  /* the whole room (game.fill); the hint is the shell's tool */
    float S = floorf(fminf(r.w, r.h) * 0.98f);
    fr = (int)(S * FRAME);
    cs = (int)((S - 2 * fr) / G.n);
    gw = (float)(G.n * cs + 2 * fr);
    gx = floorf(r.x + (r.w - gw) / 2);
    gy = floorf(r.y + (r.h - gw) / 2);
    int key = (int)gw * 8 + G.n;
    if (made_key != key) {
        made_key = key;
        int w = (int)gw, g = (int)(0.7f * cs);
        ui_free(&t_board);
        paint_board(ui_buf(w, w), G.n, cs, fr);
        t_board = ui_tex(w, w, ui_buf(w, w));
        ui_free(&t_lit);
        ui_free(&t_dark);
        paint_glass(g, 1, ui_buf(g, g));
        t_lit = ui_tex(g, g, ui_buf(g, g));
        paint_glass(g, 0, ui_buf(g, g));
        t_dark = ui_tex(g, g, ui_buf(g, g));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        ui_free(&t_par);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        lamp_pic(small, 0, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        lamp_pic(small, 1, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(turn_at, 0, sizeof turn_at);
    hint.cell = down = -1;
    win.on = 0;
}

static void begin(void)
{
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a designed level */
{
    if (!lmp_level(&G, level)) lmp_level(&G, 0);
    begin();
}

static void start_random(int variant)           /* a board made at random, of the variant's size */
{
    lmp_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int par(void) { return G.best > 0 ? G.best : 0; }
static int level_stars(void) { return ui_par_stars(G.moves, par()); }

static int status(void) { return lmp_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }

static Uint32 win_end(void) { return (Uint32)(2 * G.n * WAVE_STEP + WAVE_LEN); }

static int busy(void)
{
    Uint32 now = ui_ms();
    for (int c = 0; c < G.n * G.n; c++)
        if (turn_at[c] && now < turn_at[c] + FADE_MS) return 1;
    return win.on && now - win.t0 < win_end();
}

/* the lamps a press on c turned: the fade of each, c's first */
static void fades(int c)
{
    Uint32 now = ui_ms();
    int n = G.n, x = c % n, y = c / n;
    turn_at[c] = now;
    if (x > 0) turn_at[c - 1] = now + RIPPLE_MS;
    if (x < n - 1) turn_at[c + 1] = now + RIPPLE_MS;
    if (y > 0) turn_at[c - n] = now + RIPPLE_MS;
    if (y < n - 1) turn_at[c + n] = now + RIPPLE_MS;
    ui_animate();
}

static void pressed(int c)
{
    lmp_press(&G, c);
    fades(c);
    ui_sound(SND_PLACE);
    if (lmp_solved(&G)) {
        win.on = 1;
        win.from = c;
        win.t0 = ui_ms() + RIPPLE_MS + FADE_MS;
    }
}

static int undo(void)
{
    int c = lmp_undo(&G);
    if (c < 0) return 0;
    win.on = 0;
    hint.cell = -1;
    fades(c);
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = lmp_script(&G, moves);
    if (lmp_solved(&G) && G.nh) {
        win.on = 1;
        win.from = G.hist[G.nh - 1];
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void lamp_xy(int c, float *x, float *y)
{
    *x = gx + fr + (c % G.n + 0.5f) * cs;
    *y = gy + fr + (c / G.n + 0.5f) * cs;
}

static int lamp_at(float x, float y)
{
    float ix = x - gx - fr, iy = y - gy - fr;
    if (ix < 0 || iy < 0 || ix >= G.n * cs || iy >= G.n * cs) return -1;
    return (int)(iy / cs) * G.n + (int)(ix / cs);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_board, gx, gy, gw, gw);
    float g = 0.7f * cs, wt = win.on && now >= win.t0 ? (float)(now - win.t0) : -1;
    if (win.on && wt < 0) ui_animate();
    for (int c = 0; c < G.n * G.n; c++) {
        float x, y, lit = G.on[c], sz = g;
        lamp_xy(c, &x, &y);
        if (turn_at[c]) {                       /* fading over: before its time, still the other way */
            float t = now < turn_at[c] ? 0 : (now - turn_at[c]) / (float)FADE_MS;
            if (t < 1) {
                float e = puz_ease(t);
                lit = G.on[c] ? e : 1 - e;
                sz = g * (1 - 0.07f * sinf(3.14159265f * t));   /* it clicks down and springs back as it turns */
                ui_animate();
            }
        }
        if (c == down && !lmp_solved(&G)) sz = g * 0.93f;   /* held under the finger: pressed in */
        if (lit > 0) ui_mark_gold(x, y, 1.45f * cs * lit);
        if (wt >= 0) {                          /* the win: the wave of light */
            float dist = hypotf((float)(c % G.n - win.from % G.n), (float)(c / G.n - win.from / G.n));
            float p = puz_pulse(wt, dist * WAVE_STEP, WAVE_LEN);
            if (p > 0) ui_mark_gold(x, y, 2.2f * cs * p);
        }
        if (lit < 1) ui_put_c(t_dark, x, y, sz, sz);
        if (lit > 0) {
            SDL_SetTextureAlphaMod(t_lit, (Uint8)(255 * lit));
            ui_put_c(t_lit, x, y, sz, sz);
            SDL_SetTextureAlphaMod(t_lit, 255);
        }
        if (c == down && !lmp_solved(&G)) ui_mark_select_round(x, y, 0.9f * cs);
    }
    if (hint.cell >= 0) {
        float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
        if (p > 0) {
            float x, y;
            lamp_xy(hint.cell, &x, &y);
            ui_mark_gold(x, y, 2.4f * cs * p);
            ui_animate();
        } else {
            hint.cell = -1;
        }
    }
    if (wt >= 0) {
        if (wt < win_end()) ui_animate();
        else win.on = 0;
    }
    int solved = lmp_solved(&G);
    ui_tool(B_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    down = -1;
    if (lmp_solved(&G)) return;
    if (ui_tool_hit(x, y) == B_HINT) {
        int c = lmp_hint(&G);
        if (c < 0) { ui_sound(SND_DENY); return; }
        hint.cell = c;
        hint.t0 = ui_ms();
        pressed(c);
        return;
    }
    down = lamp_at(x, y);
    ui_animate();
}

static void motion(float x, float y)
{
    if (down >= 0 && lamp_at(x, y) != down) { down = -1; ui_animate(); }
}

static void release(float x, float y)
{
    int c = down;
    down = -1;
    ui_animate();
    if (c >= 0 && lamp_at(x, y) == c) pressed(c);
}

/* ---- the bar: the presses; the fewest possible at the right ------------------------------------------------------ */
static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two(b, t_small, G.moves, t_par, G.best, lmp_solved(&G) && G.moves == G.best);
}

/* ---- the pictures of the shell's screens: a board of n x n lamps, those of `on` lit ---------------------------- */
static void paint_lamps(int n, const uint8_t *on, int size, uint8_t *rgba)
{
    int f = (int)(size * FRAME), q = (size - 2 * f) / n, w = n * q + 2 * f;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_board(b, n, q, f);
    int g = (int)(0.7f * q);
    uint8_t *lit = calloc((size_t)g * g, 4), *dark = calloc((size_t)g * g, 4), *halo = calloc((size_t)q * 2 * q * 2, 4);
    if (lit && dark && halo) {
        paint_glass(g, 1, lit);
        paint_glass(g, 0, dark);
        static const uint8_t gold[3] = {255, 200, 90};
        art_disc(2 * q, gold, 0.55f, 1.6f, halo);
        for (int c = 0; c < n * n; c++) {
            float cx = f + (c % n + 0.5f) * q, cy = f + (c / n + 0.5f) * q;
            if (on[c]) art_blend(b, w, w, halo, 2 * q, 2 * q, (int)(cx - q), (int)(cy - q));
        }
        for (int c = 0; c < n * n; c++) {
            float cx = f + (c % n + 0.5f) * q, cy = f + (c / n + 0.5f) * q;
            art_blend(b, w, w, on[c] ? lit : dark, g, g, (int)(cx - g / 2.0f), (int)(cy - g / 2.0f));
        }
    }
    free(lit);
    free(dark);
    free(halo);
    art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    free(b);
}

/* the menu's and the picker's cards: the board of the size, some lamps lit */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 1 : variant;
    uint8_t on[LMP_CELLS];
    lmp_make(v, 11, on);
    paint_lamps(lmp_size(v), on, size, rgba);
}

/* the level screen: a level's board; a chapter's tab: a board of its size, a cross of lamps lit in its middle */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    lmp_game T;
    if (lmp_level(&T, level)) paint_lamps(T.n, T.on, size, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    lmp_game T;
    if (!lmp_level(&T, chapter * (LMP_LEVELS / LMP_CHAPTERS))) return;
    uint8_t on[LMP_CELLS] = {0};
    lmp_toggle(T.n, on, T.n / 2 + T.n / 2 * T.n);
    paint_lamps(T.n, on, size, rgba);
}

static int score(void) { return G.moves; }   /* the end card's number */

static const game lights_game = {
    .id = "lights", .order = 443, .variants = LMP_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
    .levels = LMP_LEVELS, .chapters = LMP_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .par = par, .fill = 1, .tools = BTNS,
};
REGISTER_GAME(lights_game)
