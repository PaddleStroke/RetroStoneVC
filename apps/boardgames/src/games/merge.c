/*
 * Merge in RetroStone Board, for one player: a walnut tray with sunk squares, the tiles cut from the house plank, their
 * numbers burned in, their wood deepening from pale maple to dark walnut as they grow, the target and beyond in deep
 * lacquer with gold numbers (the rules: merge_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Swipe on the tray, or tap one of the four round buttons under it, or use the arrow keys: every tile slides that
 * way, equal tiles meet and merge (a pop), and a new tile grows on an empty square. A way that moves nothing: its
 * button dimmed. The lamp: the hint (a short look ahead): its push's button in gold light, and a gold light along the
 * side the tiles would go. Undo takes back one push, all the way to the start (the new tiles too: the same push again
 * brings the same tile).
 * The target made: a gold light pulses on that tile and runs over the others, and the game goes on. No push left: the
 * game is over (never lost); the buttons give way to a wooden plaque with the score burned in (in gold light when it
 * is the best of the session), and the shell's card shows the stars and the score: three stars for the target made
 * (or beyond), two for the tile just below it, one otherwise. No designed levels: every new tile is drawn by chance,
 * so a level would only be a seed under another name, with no par to measure it by; the sizes are the difficulty.
 * The bar: the score, and at its right end the best score of the session (this size) by a gold star (narrow bars: the
 * two in two rows).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/merge.h"
#include "games/puzzles.h"
#include "games/swipepad.h"
#include "shell.h"

#define FRAME 0.035f
#define STRIP 0.16f                             /* the buttons' row, a part of the board's square */
#define SLIDE_MS 110
#define POP_MS 160
#define TARGET_MS 1500
#define WAVE_STEP 70
#define WAVE_LEN 560

static const uint8_t GOLD[3] = {240, 202, 112};

static mrg_game G;
static uint32_t best[MRG_VARIANTS], best_before;   /* the session's best scores (each size); when this game began */
static struct { int on; uint8_t prev[MRG_CELLS]; mrg_push how; Uint32 t0; } anim;
static struct { int on, end; Uint32 t0; } fx;   /* the target made, or the end: from t0 */
static int hint_way = -1;
static swp_swipe sw;
static swp_pad pad = {.down = -1};

static box last_box;
static float bx, by, side, tx, ty;              /* the board's square; the tray's top left */
static int fr, gap, tile, tw, made_key = -1, made_small = -1, made_tile = -1;
static SDL_Texture *t_tray, *t_tile[MRG_TOP + 1], *t_small, *t_best, *t_plaque;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* 0: pale maple .. 1: dark walnut, the step before the target */
static float tone(int p, int target) { return clampf((p - 1) / (float)(target - 2), 0, 1); }

/* a tile of power p (the grid's target: its tone), T x T: its own cut of the plank, toned, bevelled, its number burned
 * in (light numbers on the dark ones); the target and beyond in deep lacquer with gold numbers. digits 0: no number */
static void paint_tile(int p, int target, int T, int digits, uint8_t *o)
{
    int P = 2 * T;
    uint8_t *src = malloc((size_t)P * P * 4);
    if (!src) return;
    art_board_n(1, 1, P, 0, 0, src);
    int sx = p * 37 % T, sy = p * 53 % T;
    float k = tone(p, target), L = 1.14f - 0.8f * k, m[3] = {L, L * (1 - 0.16f * k), L * (1 - 0.32f * k)};
    if (p >= target) m[0] = 0.42f, m[1] = 0.25f, m[2] = 0.2f;   /* deep lacquer */
    for (int y = 0; y < T; y++)
        for (int x = 0; x < T; x++) {
            const uint8_t *s = src + 4 * ((size_t)(sy + y) * P + sx + x);
            uint8_t *d = o + 4 * ((size_t)y * T + x);
            for (int i = 0; i < 3; i++) d[i] = (uint8_t)clampf(s[i] * m[i], 0, 255);
            d[3] = 255;
        }
    free(src);
    puz_bevel(o, T, T, 0.08f * T, fmaxf(2, 0.065f * T), 0.34f);
    if (!digits) return;
    int value = 1 << p, n = 1;
    for (int v = value; v >= 10; v /= 10) n++;
    float dh = T * fminf(0.46f, 0.82f / (0.66f * n - 0.04f)), cx = T / 2.0f, cy = T * 0.52f;
    if (k < 0.5f) {
        puz_burn(o, T, T, value, cx, cy, dh);
        return;
    }
    float off = fmaxf(1, dh / 20);
    puz_digits(o, T, T, value, cx + 0.5f * off, cy + off, dh, PUZ_INK, 0.6f, 1);
    puz_digits(o, T, T, value, cx, cy, dh, p >= target ? GOLD : PUZ_CREAM, 1, 1);
}

static SDL_Texture *tile_tex(int p)
{
    if (p < 1 || p > MRG_TOP || tile < 4) return NULL;
    if (!t_tile[p]) {
        uint8_t *o = ui_buf(tile, tile);
        paint_tile(p, G.target, tile, 1, o);
        t_tile[p] = ui_tex(tile, tile, o);
    }
    return t_tile[p];
}

/* the tray, w x w: the walnut frame f, its floor sunk in shadow, n x n squares sunk deeper (g between them, T each) */
static void paint_tray(int w, int f, int n, int g, int T, uint8_t *o)
{
    art_board_n(1, 1, w - 2 * f, f, 0, o);
    float sh = 0.03f * w, rad = 0.08f * T;
    for (int y = f; y < w - f; y++)
        for (int x = f; x < w - f; x++) {
            float e = fminf((float)(x - f), (float)(y - f)), k = 0.56f - 0.16f * clampf(1 - e / sh, 0, 1);
            int lx = x - f - g, ly = y - f - g, i = lx / (T + g), j = ly / (T + g);
            int sx = lx - i * (T + g), sy = ly - j * (T + g);
            if (lx >= 0 && ly >= 0 && i < n && j < n && sx < T && sy < T) {   /* in a square */
                float cx = fmaxf(rad - sx, sx - (T - rad)), cy = fmaxf(rad - sy, sy - (T - rad));
                if (cx <= 0 || cy <= 0 || hypotf(cx, cy) <= rad) {
                    float in = fminf((float)sx, (float)sy);
                    k *= 0.8f - 0.14f * clampf(1 - in / (0.12f * T), 0, 1);
                }
            }
            uint8_t *p = o + 4 * (y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] * k);
        }
}

static void tile_pic(int p, int digits, int size, int at_x, int at_y, int out, uint8_t *rgba)
{
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    paint_tile(p, 11, size, digits, b);
    art_blend(rgba, out, out, b, size, size, at_x, at_y);
    free(b);
}

static void token(int player, int size, uint8_t *rgba)   /* the first tile: 2 */
{
    (void)player;
    int T = (int)(0.84f * size);
    tile_pic(1, 1, T, (size - T) / 2, (size - T) / 2, size, rgba);
}

static void score_pic(int size, uint8_t *rgba)   /* the score: two tiles, one on the other */
{
    int T = (int)(0.66f * size);
    tile_pic(3, 0, T, 0, 0, size, rgba);
    tile_pic(7, 0, T, size - T, size - T, size, rgba);
}

static void best_pic(int size, uint8_t *rgba)    /* the best: a dark tile with a gold star */
{
    int T = (int)(0.9f * size);
    uint8_t *b = calloc((size_t)T * T, 4);
    if (!b) return;
    uint8_t *src = malloc((size_t)T * T * 4);
    if (src) {
        art_board_n(1, 1, T, 0, 0, src);
        for (int i = 0; i < T * T; i++) {
            b[4 * i] = (uint8_t)(src[4 * i] * 0.42f), b[4 * i + 1] = (uint8_t)(src[4 * i + 1] * 0.25f);
            b[4 * i + 2] = (uint8_t)(src[4 * i + 2] * 0.2f), b[4 * i + 3] = 255;
        }
        free(src);
    }
    puz_bevel(b, T, T, 0.12f * T, fmaxf(1.5f, 0.08f * T), 0.34f);
    art_fig *f = art_fig_new();
    float star[20];
    for (int i = 0; i < 10; i++) {
        float a = (-90 + 36 * i) * PUZ_PI / 180, r = i & 1 ? 0.15f : 0.36f;
        star[2 * i] = 0.5f + r * cosf(a);
        star[2 * i + 1] = 0.53f + r * sinf(a);
    }
    art_poly(f, ART_PART, star, 10);
    art_style st = swp_style(GOLD, GOLD, 0, 0.3f);
    uint8_t *s = calloc((size_t)T * T, 4);
    if (s) {
        art_render(f, &st, T, T, (float)T, 0, 0, s);
        art_blend(b, T, T, s, T, T, 0, 0);
        free(s);
    }
    art_fig_free(f);
    art_blend(rgba, size, size, b, T, T, (size - T) / 2, (size - T) / 2);
    free(b);
}

static void fit(float S, int n, int *f, int *g, int *T, int *w)
{
    *f = (int)(S * FRAME);
    float in = S * (1 - STRIP) - 2 * *f;
    *g = (int)fmaxf(2, in * 0.024f);
    *T = (int)((in - (n + 1) * *g) / n);
    *w = n * *T + (n + 1) * *g + 2 * *f;
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h)), strip = floorf(S * STRIP);
    side = S;
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    fit(S, G.n, &fr, &gap, &tile, &tw);
    tx = floorf(bx + (S - tw) / 2);
    ty = floorf(by + (S - strip - tw) / 2);
    int key = tw * 8 + G.n;
    if (made_key != key) {
        made_key = key;
        ui_free(&t_tray);
        uint8_t *o = malloc((size_t)tw * tw * 4);
        if (o) {
            paint_tray(tw, fr, G.n, gap, tile, o);
            t_tray = ui_tex(tw, tw, o);
            free(o);
        }
    }
    if (made_tile != tile * 32 + G.target) {
        made_tile = tile * 32 + G.target;
        for (int p = 0; p <= MRG_TOP; p++) ui_free(&t_tile[p]);
    }
    swp_place(&pad, (box){bx, by + S - strip, S, strip});
    int pw = (int)(0.62f * S), ph = (int)(0.78f * strip);
    ui_free(&t_plaque);
    uint8_t *o = malloc((size_t)pw * pw * 4);
    if (o) {
        art_board_n(1, 1, pw, 0, 0, o);         /* the plank cut to the plaque (its first rows) */
        puz_bevel(o, pw, ph, 0.18f * ph, fmaxf(2, 0.08f * ph), 0.4f);
        t_plaque = ui_tex(pw, ph, o);
        free(o);
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        score_pic(small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
        ui_free(&t_best);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        best_pic(small, ui_buf(small, small));
        t_best = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int status(void) { return mrg_over(&G) ? GS_WIN0 : GS_PLAY; }   /* over: done, never lost */

/* the stars, by the largest tile made: the target or beyond 3, the one below it 2, else 1 */
static int level_stars(void)
{
    int top = mrg_top(&G);
    return top >= G.target ? 3 : top == G.target - 1 ? 2 : 1;
}

static int turn(void) { return 0; }
static int score(void) { return (int)G.p.score; }   /* the end card shows it */

static Uint32 fx_len(void) { return fx.end ? (Uint32)(2 * G.n * WAVE_STEP + WAVE_LEN + 200) : TARGET_MS; }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && now - anim.t0 < SLIDE_MS + POP_MS) || (fx.on && (int)(now - fx.t0) < (int)fx_len());
}

static void calm(void)
{
    anim.on = fx.on = 0;
    hint_way = -1;
    sw.on = 0;
    pad.down = -1;
}

static void keep_best(void)
{
    if (G.p.score > best[G.variant]) best[G.variant] = G.p.score;
}

/* the gold that follows a push (or a script): the target made, or the end */
static void light(Uint32 at)
{
    fx.on = 0;
    if (mrg_over(&G)) fx.on = fx.end = 1;
    else if (mrg_just_reached(&G)) fx.on = 1, fx.end = 0;
    fx.t0 = at;
}

static void push(int way)
{
    if (mrg_over(&G)) { ui_sound(SND_DENY); return; }
    uint8_t prev[MRG_CELLS];
    memcpy(prev, G.p.c, sizeof prev);
    mrg_push how;
    if (!mrg_move(&G, way, &how)) { ui_sound(SND_DENY); return; }
    memcpy(anim.prev, prev, sizeof prev);
    anim.how = how;
    anim.on = 1;
    anim.t0 = ui_ms();
    hint_way = -1;
    keep_best();
    ui_sound(SND_MOVE);
    if (how.gain) ui_sound_later(SND_PLACE, SLIDE_MS - 10);
    light(anim.t0 + SLIDE_MS);
    ui_animate();
}

static void key(SDL_Keycode k)                  /* the arrow keys */
{
    int w = swp_key_way(k);
    if (w >= 0) push(w);
}

static void start(int variant)
{
    mrg_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    best_before = best[G.variant];
    calm();
    made_key = -1;
    if (last_box.w > 0) layout(last_box);
}

static void hint(void)
{
    int d = mrg_hint(&G);
    anim.on = 0;
    if (d < 0) { ui_sound(SND_DENY); return; }
    hint_way = d;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    calm();
    return mrg_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = mrg_script(&G, moves);
    keep_best();
    light(ui_ms());
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_xy(int i, float *x, float *y)
{
    *x = tx + fr + gap + (i % G.n) * (tile + gap) + tile / 2.0f;
    *y = ty + fr + gap + (i / G.n) * (tile + gap) + tile / 2.0f;
}

static void tile_at(int p, float x, float y, float s, float alpha)
{
    SDL_Texture *t = tile_tex(p);
    if (!t) return;
    ui_shadow_flat(x + 0.03f * tile, y + 0.05f * tile, 0.96f * tile * s, 0.96f * tile * s, 0.5f * alpha);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    ui_put_c(t, x, y, tile * s, tile * s);
    SDL_SetTextureAlphaMod(t, 255);
}

/* the gold of the target or the end: the light round a tile at time wt (ms after fx.t0) */
static float glow(int i, float wt)
{
    if (!fx.on || wt < 0) return 0;
    int ti = 0;                                 /* the largest tile: where the light starts */
    for (int k = 0; k < G.n * G.n; k++)
        if (G.p.c[k] > G.p.c[ti]) ti = k;
    float dist = hypotf((float)(i % G.n - ti % G.n), (float)(i / G.n - ti / G.n));
    if (!fx.end) {
        if (i == ti) return puz_pulse(wt, 0, 700) + puz_pulse(wt, 600, 800);
        return 0.7f * puz_pulse(wt, 250 + dist * WAVE_STEP, WAVE_LEN);
    }
    if (!mrg_reached(&G)) return 0;
    return puz_pulse(wt, dist * WAVE_STEP, WAVE_LEN);
}

static void draw_plaque(void)
{
    float cx = bx + side / 2, cy = pad.cy[SWP_UP];
    float pw = 0.62f * side, ph = 0.78f * floorf(side * STRIP);
    int newbest = G.p.score > best_before && G.p.score >= best[G.variant];
    for (int i = -1; newbest && i <= 1; i++) ui_mark_gold(cx + i * 0.3f * pw, cy, 0.62f * pw);
    ui_shadow_flat(cx + 0.02f * ph, cy + 0.06f * ph, pw, ph, 0.7f);
    ui_put_c(t_plaque, cx, cy, pw, ph);
    float dh = 0.5f * ph;
    ui_number_ex(cx + 0.03f * dh, cy + 0.05f * dh, dh, (int)G.p.score, PUZ_CREAM, 140, UI_NUM_TIGHT);
    ui_number_ex(cx, cy, dh, (int)G.p.score, PUZ_INK, 255, UI_NUM_TIGHT);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_tray, tx, ty, (float)tw, (float)tw);
    float t = anim.on ? (float)(now - anim.t0) : 1e9f, wt = fx.on ? (float)(int)(now - fx.t0) : -1;
    if (anim.on && t >= SLIDE_MS + POP_MS) anim.on = 0;
    if (hint_way >= 0 && !anim.on) {            /* the hint: a gold light along the side the tiles would go */
        for (int k = 0; k < G.n; k++) {
            int i = hint_way == MRG_UP ? k : hint_way == MRG_DOWN ? k + (G.n - 1) * G.n
                    : hint_way == MRG_LEFT ? k * G.n : k * G.n + G.n - 1;
            float x, y;
            cell_xy(i, &x, &y);
            static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
            ui_mark_gold(x + DX[hint_way] * 0.3f * tile, y + DY[hint_way] * 0.3f * tile, 1.3f * tile);
        }
    }
    if (anim.on && t < SLIDE_MS) {              /* the tiles sliding from their old squares */
        float f = puz_ease(t / SLIDE_MS);
        for (int i = 0; i < G.n * G.n; i++) {
            if (!anim.prev[i] || anim.how.to[i] < 0) continue;
            float x0, y0, x1, y1;
            cell_xy(i, &x0, &y0);
            cell_xy(anim.how.to[i], &x1, &y1);
            tile_at(anim.prev[i], x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, 1, 1);
        }
    } else {
        float f = anim.on ? (t - SLIDE_MS) / POP_MS : 2;
        for (int i = 0; i < G.n * G.n; i++) {
            int p = G.p.c[i];
            if (!p) continue;
            float x, y, s = 1, a = 1;
            cell_xy(i, &x, &y);
            if (f < 1 && anim.how.merged[i]) s = 1 + 0.16f * sinf(PUZ_PI * f);   /* a merge pops */
            if (f < 1 && i == anim.how.spawn) { s = 0.3f + 0.7f * puz_ease(f); a = puz_ease(f); }   /* a new tile */
            float q = glow(i, wt);
            if (q > 0) ui_mark_gold(x, y, 1.9f * tile * q);
            tile_at(p, x, y, s, a);
            if (q > 0) ui_mark_gold(x, y, 1.1f * tile * q);
        }
    }
    if (anim.on || (fx.on && wt < (float)fx_len())) ui_animate();
    if (mrg_over(&G) && !anim.on) {
        draw_plaque();
        return;
    }
    unsigned can = 0;
    for (int w = 0; w < 4; w++)
        if (mrg_can(&G, w)) can |= 1u << w;
    swp_draw(&pad, can, anim.on ? -1 : hint_way, can != 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    sw.on = 0;
    pad.down = mrg_over(&G) ? -1 : swp_hit(&pad, x, y);
    if (pad.down >= 0) { ui_animate(); return; }
    if (x >= bx && y >= by && x < bx + side && y < by + side) swp_begin(&sw, x, y);   /* anywhere on the board */
}

static void motion(float x, float y)
{
    int w = swp_follow(&sw, x, y, 0.6f * ui.unit);
    if (w >= 0) push(w);
}

static void release(float x, float y)
{
    if (pad.down >= 0) {
        int k = pad.down;
        pad.down = -1;
        ui_animate();
        if (swp_hit(&pad, x, y) != k) return;
        if (k == SWP_HINT) hint();
        else push(k);
        return;
    }
    int tap, w = swp_end(&sw, x, y, 0.3f * ui.unit, &tap);
    if (w >= 0) push(w);
    else if (tap) ui_sound(SND_DENY);           /* the tiles go by pushes: a swipe, a button */
}

/* ---- the bar: the score; the session's best at the right ------------------------------------------------------- */
static float digits_w(uint32_t v, float dh)
{
    int n = 1;
    for (; v >= 10; v /= 10) n++;
    return (0.74f * (n - 1) + 0.62f) * dh;
}

static void bar(int player, box b)
{
    (void)player;
    uint32_t score = G.p.score, top = best[G.variant] > score ? best[G.variant] : score;
    float u = ui.unit, cy = b.y + b.h / 2, sz = 0.6f * u, dh = 0.46f * u;
    int over = mrg_over(&G) && !anim.on, newbest = over && score > best_before && score >= top;
    float bw_ = digits_w(top, dh), need = sz + 0.12f * u + digits_w(score, dh) + 0.35f * u + sz + 0.12f * u + bw_;
    if (b.w >= need) {
        if (newbest) ui_mark_gold(b.x + sz / 2, cy, 2.2f * sz);
        puz_bar_moves(b, t_small, (int)score);
        float x = b.x + b.w - bw_;
        ui_mark_gold(x - 0.12f * u - sz / 2, cy, (newbest ? 2.4f : 1.5f) * sz);
        ui_put(t_best, x - 0.12f * u - sz, cy - sz / 2, sz, sz);
        ui_number(x + bw_ / 2, cy, dh, (int)top, 1);
        return;
    }
    /* narrow: the score above, the best under it by a small star */
    float h2 = 0.34f * u, s2 = 0.38f * u, y1 = cy - 0.22f * u, y2 = cy + 0.24f * u;
    ui_number(b.x + digits_w(score, h2) / 2, y1, h2, (int)score, 1);
    ui_mark_gold(b.x + s2 / 2, y2, 1.6f * s2);
    ui_put(t_best, b.x, y2 - s2 / 2, s2, s2);
    ui_number(b.x + s2 + 0.1f * u + digits_w(top, h2) / 2, y2, h2, (int)top, 1);
}

/* ---- the menu's and the picker's cards: the tray of that size with tiles on it ------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const uint8_t SHOW[MRG_VARIANTS][MRG_CELLS] = {
        {1, 0, 0, 2, 0, 3, 0, 0, 0, 4, 5, 0, 11, 8, 7, 6},
        {0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 1, 0, 0, 4, 0, 5, 0, 0, 6, 0, 11, 9, 8, 7, 1},
        {0, 1, 0, 2, 0, 3, 8, 5, 4}};
    static const uint8_t MENU[MRG_CELLS] = {1, 0, 0, 2, 0, 3, 0, 0, 0, 4, 5, 0, 10, 9, 8, 6};
    int v = variant < 0 ? 0 : variant, n, target;
    mrg_variant(v, &n, &target);
    int f = (int)(size * FRAME), in = size - 2 * f, g = (int)fmaxf(1, in * 0.024f), T = (in - (n + 1) * g) / n;
    int w = n * T + (n + 1) * g + 2 * f;
    uint8_t *o = malloc((size_t)w * w * 4), *one = calloc((size_t)T * T, 4);
    if (o && one) {
        paint_tray(w, f, n, g, T, o);
        for (int i = 0; i < n * n; i++) {
            int p = variant < 0 ? MENU[i] : SHOW[v][i];
            if (!p) continue;
            memset(one, 0, (size_t)T * T * 4);
            paint_tile(p, target, T, 1, one);
            art_blend(o, w, w, one, T, T, f + g + (i % n) * (T + g), f + g + (i / n) * (T + g));
        }
        art_blend(rgba, size, size, o, w, w, (size - w) / 2, (size - w) / 2);
    }
    free(o);
    free(one);
}

static const game merge_game = {
    .id = "merge", .order = 424, .variants = MRG_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key, .score = score,
    .level_stars = level_stars,
};
REGISTER_GAME(merge_game)
