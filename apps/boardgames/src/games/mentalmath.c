/*
 * Mental Math in RetroStone Board, for one player: a drill of sums against the clock. The sum asked is set on a
 * plank in big ink digits, the number to find as a hollow where the digits typed come in blue; under it a row of
 * beads, one a sum (gold once answered, amber when it took a mistake or a hint). Under the plank (at its right on a
 * wide screen) a pad of big wooden keys 0..9 and a key taking back a digit (the rules: mentalmath_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap the digits: as soon as the answer has its digits it is checked; right, a gold light and the next sum; wrong, a
 * red glow, the hollow shakes and empties (a mistake). The shell's tool: the hint (the answer typed, counted). Undo
 * takes back a digit typed. The keys: the digits type, the arrows move a frame over the pad, Return presses it. The
 * bar: the seconds so far, and the best time of this drill (a medal) once there is one. The clock starts when the
 * first sum shows. Over: the stars (the time against par, capped by the hints and mistakes: mentalmath.h), the time
 * beside par on the end card; the best time is kept for each level and each kind of random drill.
 * The shell's level screen chooses a level (150 in six chapters: mentalmath_levels.h), or its die tab a random drill
 * of a kind (the variants).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/mathsigns.h"
#include "games/mentalmath.h"
#include "games/mentalmath_levels.h"
#include "shell.h"

enum { T_HINT, TOOLS };
enum { K_BACK = 10, KEYS = 12 };                /* the pad's places: 7 8 9 / 4 5 6 / 1 2 3 / - 0 back */
static const int PAD[KEYS] = {7, 8, 9, 4, 5, 6, 1, 2, 3, -1, 0, K_BACK};
#define FLASH_MS 380
#define BESTS (MMH_LEVELS + MMH_VARIANTS)
#define BEST_KEY "mentalmath/best"

static const uint8_t INK[3] = {52, 34, 22}, GOLD[3] = {236, 194, 96}, AMBER[3] = {214, 120, 64},
                     BEAD[3] = {120, 92, 66};

static mmh_game G;
static int which = -1;                          /* the best's slot: a level, or MMH_LEVELS + variant */
static Uint32 t0, t_end;
static int started, kept_best;
static uint16_t best[BESTS];                    /* tenths of a second; 0 none */

static box room;
static float px, py, pw, ph;                    /* the plank */
static float kx[KEYS], ky[KEYS], kd;            /* the keys' centres, a key's side */
static int made = -1, made_small = -1, made_medal = -1;
static SDL_Texture *t_plank, *t_key, *t_bead, *t_small, *t_medal, *t_sign[6];
static int sign_px;
static struct { int on, key; } held;
static struct { int kind, q; Uint32 t0; } flash;   /* kind MMH_RIGHT, MMH_WRONG (0: none); q the sum it was */
static int cur = -1;                            /* the keys' frame on the pad, -1 none */

/* ---- the best times ---------------------------------------------------------------------------------------------- */
static void bests_read(void)
{
    uint8_t b[2 * BESTS];
    memset(best, 0, sizeof best);
    int n = ui_save_get(BEST_KEY, b, sizeof b);
    for (int i = 0; 2 * i + 1 < n; i++) best[i] = (uint16_t)(b[2 * i] | b[2 * i + 1] << 8);
}

static void bests_write(void)
{
    uint8_t b[2 * BESTS];
    for (int i = 0; i < BESTS; i++) b[2 * i] = (uint8_t)(best[i] & 255), b[2 * i + 1] = (uint8_t)(best[i] >> 8);
    ui_save_set(BEST_KEY, b, sizeof b);
}

static Uint32 elapsed(void)
{
    if (!started) return 0;
    return (t_end ? t_end : ui_ms()) - t0;
}

static int tenths(void) { return (int)((elapsed() + 99) / 100); }

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
static SDL_Texture *pic(int w, int h, void (*paint)(uint8_t *, int, int, int), int k)
{
    uint8_t *o = ui_buf(w, h);
    memset(o, 0, (size_t)w * h * 4);
    paint(o, w, h, k);
    return ui_tex(w, h, o);
}

static void paint_sign(uint8_t *o, int w, int h, int sign) { msg_sign(o, w, h, w / 2.0f, h / 2.0f, (float)w, sign, INK); }

static void paint_bead(uint8_t *o, int w, int h, int k)
{
    (void)k;
    static const uint8_t white[3] = {255, 255, 255};
    pzk_ball(o, w, h, w / 2.0f, h / 2.0f, w * 0.45f, white, 1, 0.6f);
}

static void paint_medal(uint8_t *o, int w, int h, int k)
{
    (void)k;
    static const uint8_t ribbon[3] = {60, 90, 160}, rim[3] = {150, 110, 40};
    pzk_stroke(o, w, h, w * 0.32f, 0, w * 0.45f, h * 0.45f, w * 0.1f, ribbon, 1);
    pzk_stroke(o, w, h, w * 0.68f, 0, w * 0.55f, h * 0.45f, w * 0.1f, ribbon, 1);
    pzk_ball(o, w, h, w / 2.0f, h * 0.62f, w * 0.34f, rim, 1, 0.3f);
    pzk_ball(o, w, h, w / 2.0f, h * 0.62f, w * 0.27f, GOLD, 1, 0.6f);
}

/* a sum painted into a picture, centred at (cx, cy), its digits dh high; the number to find as a hollow (answered:
 * its number in blue in it) */
static void paint_sum(uint8_t *o, int w, int h, const mmh_q *q, float cx, float cy, float dh, int answered)
{
    float wid[5], all = 0, gap = 0.28f * dh;
    for (int i = 0; i < 5; i++) {
        if (i % 2) wid[i] = 0.9f * dh;
        else if (i / 2 == q->miss) wid[i] = mmh_digits(q->n[i / 2]) * 0.66f * dh + 0.5f * dh;
        else wid[i] = (mmh_digits(q->n[i / 2]) - 1) * 0.66f * dh + 0.62f * dh;
        all += wid[i] + (i ? gap : 0);
    }
    float x = cx - all / 2;
    for (int i = 0; i < 5; i++) {
        float mx = x + wid[i] / 2;
        if (i % 2) msg_sign(o, w, h, mx, cy, 0.9f * dh, i == 1 ? q->op : MSG_EQ, INK);
        else if (i / 2 == q->miss) {
            msg_hollow(o, w, h, x, cy - 0.7f * dh, wid[i], 1.4f * dh);
            if (answered) puz_digits(o, w, h, q->n[i / 2], mx, cy, dh, BENCH_BLUE, 1, 1);
        } else puz_digits(o, w, h, q->n[i / 2], mx, cy, dh, INK, 1, 1);
        x += wid[i] + gap;
    }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int s = (int)(0.86f * size), o = (size - s) / 2;
    uint8_t *b = calloc((size_t)s * s, 4);
    if (!b) return;
    bench_tile(b, s, s, 0.16f * s, BENCH_TONE[3]);
    msg_sign(b, s, s, 0.3f * s, 0.3f * s, 0.42f * s, MSG_ADD, INK);
    puz_digits(b, s, s, 9, 0.62f * s, 0.6f * s, 0.62f * s, BENCH_BLUE, 1, 1);
    art_blend(rgba, size, size, b, s, s, o, o);
    free(b);
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    room = b;
    float u = fminf(b.w, b.h);
    int wide = b.w > 1.25f * b.h;
    if (wide) {                                 /* the plank at the left, the pad at the right */
        kd = floorf(fminf(b.h / 4.4f, b.w * 0.42f / 3.3f));
        pw = floorf(fminf(b.w - 3.5f * kd * 1.1f - 0.06f * u, 1.6f * b.h));
        ph = floorf(fminf(0.75f * b.h, 0.7f * pw));
        float all = pw + 0.3f * kd + 3.3f * kd;
        px = floorf(b.x + (b.w - all) / 2);
        py = floorf(b.y + (b.h - ph) / 2);
        float x0 = px + pw + 0.3f * kd + 0.15f * kd, y0 = b.y + (b.h - 4 * 1.1f * kd) / 2;
        for (int i = 0; i < KEYS; i++) kx[i] = x0 + (i % 3 + 0.5f) * 1.1f * kd, ky[i] = y0 + (i / 3 + 0.5f) * 1.1f * kd;
    } else {                                    /* the plank above, the pad under it */
        kd = floorf(fminf(b.w / 3.5f, b.h * 0.6f / 4.5f));
        kd = fminf(kd, 1.6f * ui.unit);
        pw = floorf(fminf(b.w, 1.4f * b.h));
        ph = floorf(fminf(b.h - 4.6f * 1.1f * kd, 0.62f * pw));
        float all = ph + 0.4f * kd + 4 * 1.1f * kd;
        px = floorf(b.x + (b.w - pw) / 2);
        py = floorf(b.y + (b.h - all) / 2);
        float x0 = b.x + b.w / 2 - 1.5f * 1.1f * kd, y0 = py + ph + 0.4f * kd;
        for (int i = 0; i < KEYS; i++) kx[i] = x0 + (i % 3 + 0.5f) * 1.1f * kd, ky[i] = y0 + (i / 3 + 0.5f) * 1.1f * kd;
    }
    int key = (int)pw * 4096 + (int)ph * 2 + art_frame_is_light();
    if (made != key) {
        made = key;
        int w = (int)pw, h = (int)ph, fr = (int)fmaxf(2, 0.035f * fminf(w, h));
        uint8_t *o = ui_buf(w, h);
        art_plank(w - 2 * fr, h - 2 * fr, fr, o);
        ui_free(&t_plank);
        t_plank = ui_tex(w, h, o);
        int k = (int)kd;
        o = ui_buf(k, k);
        bench_cut(o, k, k, 0.16f * k, fmaxf(2, 0.08f * k), 0.32f);
        ui_free(&t_key);
        t_key = ui_tex(k, k, o);
        int bd = (int)fmaxf(4, 0.06f * ph);
        ui_free(&t_bead);
        t_bead = pic(bd, bd, paint_bead, 0);
    }
    bench_small(&t_small, &made_small, token);
    int m = (int)(0.6f * ui.unit);
    if (!t_medal || m != made_medal) {
        made_medal = m;
        ui_free(&t_medal);
        t_medal = pic(m, m, paint_medal, 0);
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void begin(int slot)
{
    which = slot;
    started = 0;
    t_end = 0;
    kept_best = 0;
    flash.kind = 0;
    held.on = 0;
    cur = -1;
    bests_read();
    if (room.w > 0) layout(room);
}

static void start(int level)
{
    level = level < 0 || level >= MMH_LEVELS ? 0 : level;
    if (!mmh_decode(mmh_levels[level], &G)) {
        mmh_spec s = mmh_variant_spec(0);
        mmh_drill(&s, 1, &G);
    }
    begin(level);
}

static void start_random(int variant)
{
    variant = variant < 0 || variant >= MMH_VARIANTS ? 0 : variant;
    mmh_spec s = mmh_variant_spec(variant);
    mmh_drill(&s, (uint32_t)ui_random(0x7fffffff) + 1u, &G);
    begin(MMH_LEVELS + variant);
}

static int status(void) { return mmh_done(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int flashing(void) { return flash.kind && ui_ms() - flash.t0 < FLASH_MS; }
static int busy(void) { return flash.kind == MMH_RIGHT && mmh_done(&G) && flashing(); }
static int score(void) { int s = (tenths() + 9) / 10; return s < 1 ? 1 : s; }
static int par(void) { return G.par; }
static int stars(void) { return mmh_stars(&G, tenths()); }

/* the drill over: the clock stops, a best time kept */
static void finish(void)
{
    if (t_end || !mmh_done(&G)) return;
    t_end = started ? ui_ms() : t0;
    int t = tenths();
    if (t < 1) t = 1;
    if (which >= 0 && which < BESTS && (!best[which] || t < best[which])) {
        kept_best = best[which] != 0;
        best[which] = (uint16_t)(t > 65535 ? 65535 : t);
        bests_write();
    }
}

static void react(int r, int q)
{
    if (r == MMH_MORE) { ui_sound(SND_TAP); return; }
    flash.kind = r;
    flash.q = q;
    flash.t0 = ui_ms();
    ui_sound(r == MMH_RIGHT ? SND_PLACE : SND_DENY);
    if (mmh_done(&G)) {
        finish();
        ui_sound(SND_END);
    }
    ui_animate();
}

static void type(int d)
{
    if (mmh_done(&G)) return;
    if (!started) started = 1, t0 = ui_ms();
    int q = G.cur;
    if (flash.kind == MMH_WRONG) flash.kind = 0;
    react(mmh_type(&G, d), q);
}

static int undo(void)
{
    if (!mmh_back(&G)) return 0;
    flash.kind = 0;
    return 1;
}

static int script(const char *moves)
{
    if (!started) started = 1, t0 = ui_ms();
    int ok = mmh_script(&G, moves);
    if (mmh_done(&G)) finish();
    return ok;
}

static void do_hint(void)
{
    if (mmh_done(&G)) { ui_sound(SND_DENY); return; }
    if (!started) started = 1, t0 = ui_ms();
    int q = G.cur;
    react(mmh_hint(&G), q);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *sign_tex(int sign, int d)
{
    if (sign_px != d) {
        for (int i = 0; i < 6; i++) ui_free(&t_sign[i]);
        sign_px = d;
    }
    if (!t_sign[sign]) {
        uint8_t *o = ui_buf(d, d);
        memset(o, 0, (size_t)d * d * 4);
        paint_sign(o, d, d, sign);
        t_sign[sign] = ui_tex(d, d, o);
    }
    return t_sign[sign];
}

/* a hollow on the screen: the wood darkened, its top and left walls in shade (its pictures kept, a few sizes); glow:
 * a light of that colour in it */
static void hollow(float x, float y, float w, float h, float glow, const uint8_t *col)
{
    static struct { int w, h; SDL_Texture *t; } kept[4];
    static int next;
    int iw = (int)w, ih = (int)h, k = 0;
    while (k < 4 && !(kept[k].t && kept[k].w == iw && kept[k].h == ih)) k++;
    if (k == 4 && iw > 2 && ih > 2) {
        k = next++ % 4;
        uint8_t *o = ui_buf(iw, ih);
        float rad = 0.16f * fminf(w, h);
        for (int py = 0; py < ih; py++)
            for (int px = 0; px < iw; px++) {
                float fx = px + 0.5f, fy = py + 0.5f, d;
                float dx = fmaxf(rad - fx, fx - (iw - rad)), dy = fmaxf(rad - fy, fy - (ih - rad));
                if (dx > 0 && dy > 0) d = rad - hypotf(dx, dy);
                else d = fminf(fminf(fx, iw - fx), fminf(fy, ih - fy));
                float a = clampf(d + 0.5f, 0, 1), wall = clampf(1 - fminf(fx, fy) / (0.2f * fminf(w, h)), 0, 1);
                uint8_t *p = o + 4 * (py * iw + px);
                p[0] = 40, p[1] = 26, p[2] = 16;
                p[3] = (uint8_t)(255 * a * (0.32f + 0.3f * wall));
            }
        ui_free(&kept[k].t);
        kept[k].w = iw, kept[k].h = ih, kept[k].t = ui_tex(iw, ih, o);
    }
    if (k < 4) ui_put(kept[k].t, x, y, (float)iw, (float)ih);
    if (glow <= 0) return;
    if (col == GOLD) ui_mark_gold_a(x + w / 2, y + h / 2, 1.4f * fmaxf(w, h), glow);
    else ui_mark_glow(x + w / 2, y + h / 2, 1.5f * fmaxf(w, h) * glow);   /* a wrong answer: red */
}

/* the sum q at (cx, cy), digits dh high: the number to find in its hollow (typed: the digits so far; shown: its
 * answer) */
static void draw_sum(const mmh_q *q, float cx, float cy, float dh, int show, float shake, float glow,
                     const uint8_t *glow_rgb)
{
    float wid[5], all = 0, gap = 0.28f * dh;
    for (int i = 0; i < 5; i++) {
        if (i % 2) wid[i] = 0.9f * dh;
        else if (i / 2 == q->miss) wid[i] = mmh_digits(q->n[i / 2]) * 0.66f * dh + 0.5f * dh;
        else wid[i] = ui_number_w(q->n[i / 2], dh);
        all += wid[i] + (i ? gap : 0);
    }
    float s = fminf(1, 0.9f * pw / all);        /* a long sum set smaller */
    dh *= s, gap *= s, all *= s;
    float x = cx - all / 2;
    for (int i = 0; i < 5; i++) {
        float w = wid[i] * s, mx = x + w / 2;
        if (i % 2) {
            int d = (int)(0.9f * dh);
            ui_put_c(sign_tex(i == 1 ? q->op : MSG_EQ, d), mx, cy, (float)d, (float)d);
        } else if (i / 2 == q->miss) {
            float sx = mx + shake * 0.15f * dh;
            hollow(sx - w / 2, cy - 0.72f * dh, w, 1.44f * dh, glow, glow_rgb);
            if (show) ui_number_ex(sx, cy, dh, q->n[i / 2], BENCH_BLUE, 255, 0);
            else if (G.ndig) ui_number_ex(sx, cy, dh, G.typed, BENCH_BLUE, 255, 0);
        } else {
            ui_number_ex(mx, cy, dh, q->n[i / 2], INK, 255, 0);
        }
        x += w + gap;
    }
}

static void draw(void)
{
    if (!t_plank) layout(room);
    if (!t_plank) return;
    if (!started && !mmh_done(&G)) started = 1, t0 = ui_ms();   /* the clock starts with the first sum shown */
    ui_put(t_plank, px, py, pw, ph);
    Uint32 now = ui_ms();
    float f = flash.kind ? (now - flash.t0) / (float)FLASH_MS : 1;
    float dh = 0.3f * ph, cy = py + 0.44f * ph;
    if (flash.kind == MMH_RIGHT && f < 1) {     /* the sum just answered, its answer in gold light */
        const mmh_q *q = &G.q[flash.q];
        draw_sum(q, px + pw / 2, cy, dh, 1, 0, sinf(ART_PI * f), GOLD);
        ui_animate();
    } else if (!mmh_done(&G)) {
        float shake = 0, glow = 0;
        if (flash.kind == MMH_WRONG && f < 1) {
            shake = sinf(f * 6 * ART_PI) * (1 - f);
            glow = 1 - f;
            ui_animate();
        }
        static const uint8_t red[3] = {214, 60, 44};
        draw_sum(&G.q[G.cur], px + pw / 2, cy, dh, 0, shake, glow, red);
    } else {
        draw_sum(&G.q[G.nq - 1], px + pw / 2, cy, dh, 1, 0, 0, GOLD);
    }
    if (flash.kind && f >= 1) flash.kind = 0;
    float bd = 0.06f * ph, step = fminf(1.5f * bd, 0.9f * pw / (G.nq ? G.nq : 1)), by = py + 0.83f * ph;   /* the beads */
    for (int i = 0; i < G.nq; i++) {
        float bx = px + pw / 2 + (i - (G.nq - 1) / 2.0f) * step;
        const uint8_t *c = i < G.cur ? (G.slips[i] ? AMBER : GOLD) : BEAD;
        if (i == G.cur && !mmh_done(&G)) ui_mark_select_round(bx, by, 1.5f * bd);
        SDL_SetTextureColorMod(t_bead, c[0], c[1], c[2]);
        ui_put_c(t_bead, bx, by, bd, bd);
    }
    SDL_SetTextureColorMod(t_bead, 255, 255, 255);
    int over = mmh_done(&G);
    for (int i = 0; i < KEYS; i++) {            /* the pad */
        if (PAD[i] < 0) continue;
        float s = held.on && held.key == i ? 0.92f : 1, d = kd * s;
        if (i == cur && !over) ui_mark_select(kx[i] - 0.55f * kd, ky[i] - 0.55f * kd, 1.1f * kd, 1.1f * kd);
        if (PAD[i] == K_BACK) {
            ui_button(kx[i], ky[i], 0.8f * d, ICON_UNDO, over || !G.ndig ? UI_DIM : 0);
            continue;
        }
        SDL_SetTextureAlphaMod(t_key, over ? 120 : 255);
        ui_shadow_flat(kx[i], ky[i], d, d, 0.7f);
        ui_put_c(t_key, kx[i], ky[i], d, d);
        SDL_SetTextureAlphaMod(t_key, 255);
        ui_number_ex(kx[i], ky[i], 0.55f * d, PAD[i], INK, over ? 120 : 255, 0);
    }
    ui_tool(T_HINT, ICON_HINT, over ? UI_DIM : 0);
    if (!over) ui_animate();                    /* the clock runs */
}

/* ---- the finger and the keys ------------------------------------------------------------------------------------- */
static int key_at(float x, float y)
{
    for (int i = 0; i < KEYS; i++)
        if (PAD[i] >= 0 && fabsf(x - kx[i]) <= 0.55f * kd && fabsf(y - ky[i]) <= 0.55f * kd) return i;
    return -1;
}

static void press_key(int i)
{
    if (PAD[i] == K_BACK) ui_sound(undo() ? SND_TAP : SND_DENY);
    else type(PAD[i]);
}

static void press(float x, float y)
{
    cur = -1;
    if (mmh_done(&G)) return;
    if (ui_tool_hit(x, y) == T_HINT) { do_hint(); return; }
    int i = key_at(x, y);
    if (i < 0) return;
    held.on = 1;
    held.key = i;
    press_key(i);                               /* at once: a drill is about speed */
    ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    held.on = 0;
    ui_animate();
}

static void key(SDL_Keycode k)
{
    if (mmh_done(&G)) return;
    if (k >= SDLK_0 && k <= SDLK_9) { cur = -1; type((int)(k - SDLK_0)); return; }
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) { cur = -1; type((int)(k - SDLK_KP_1) + 1); return; }
    if (k == SDLK_KP_0) { cur = -1; type(0); return; }
    int dx = (k == SDLK_RIGHT) - (k == SDLK_LEFT), dy = (k == SDLK_DOWN) - (k == SDLK_UP);
    if (dx || dy) {
        if (cur < 0) cur = 4;                   /* the 5 in the middle */
        else {
            int c = cur % 3, r = cur / 3;
            do {
                c = (c + dx + 3) % 3;
                r = (r + dy + 4) % 4;
            } while (PAD[r * 3 + c] < 0);
            cur = r * 3 + c;
        }
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && cur >= 0) press_key(cur);
}

static void bar(int player, box b)
{
    (void)player;
    ui_count c[2] = {{t_small, (tenths() + 9) / 10}, {t_medal, which >= 0 && which < BESTS ? (best[which] + 9) / 10 : 0}};
    ui_bar_fit(b, c, which >= 0 && which < BESTS && best[which] ? 2 : 1, 0);
}

/* ---- the pictures for the shell ---------------------------------------------------------------------------------- */
/* a plank holding a sum (the menu's card, the random tab's) */
static void card(const mmh_q *q, int answered, int size, uint8_t *rgba)
{
    int w = size, h = (int)(0.62f * size), fr = (int)fmaxf(2, 0.035f * h);
    uint8_t *o = malloc((size_t)w * h * 4);
    if (!o) return;
    art_plank(w - 2 * fr, h - 2 * fr, fr, o);
    float dh = 0.3f * h;
    for (int i = 0; i < 2; i++) {               /* set smaller when long */
        float all = 2 * 0.9f * dh + 4 * 0.28f * dh;
        for (int k = 0; k < 3; k++) all += mmh_digits(q->n[k]) * 0.66f * dh + (k == q->miss ? 0.5f * dh : 0);
        if (all > 0.9f * w) dh *= 0.9f * w / all;
    }
    paint_sum(o, w, h, q, w / 2.0f, h / 2.0f, dh, answered);
    art_blend(rgba, size, size, o, w, h, 0, (size - h) / 2);
    free(o);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    static const mmh_q SAMPLE[MMH_VARIANTS + 1] = {
        {{8, 6, 14}, MMH_ADD, 2}, {{13, 5, 8}, MMH_SUB, 2}, {{45, 27, 72}, MMH_ADD, 2},
        {{6, 7, 42}, MMH_MUL, 2}, {{56, 8, 7}, MMH_DIV, 2}, {{9, 4, 36}, MMH_MUL, 1}, {{7, 5, 12}, MMH_ADD, 2},
    };
    card(&SAMPLE[variant < 0 || variant >= MMH_VARIANTS ? MMH_VARIANTS : variant], variant < 0, size, rgba);
}

/* a chapter's tab: its signs on a tile (plus; minus; plus and minus to a hundred; times; divided; all four) */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int s = (int)(0.8f * size), o = (size - s) / 2;
    uint8_t *b = calloc((size_t)s * s, 4);
    if (!b) return;
    static const float TONE[3] = {0.84f, 0.76f, 0.66f};
    bench_tile(b, s, s, 0.16f * s, TONE);
    switch (chapter) {
    case 0: msg_sign(b, s, s, s / 2.0f, s / 2.0f, 1.1f * s, MSG_ADD, INK); break;
    case 1: msg_sign(b, s, s, s / 2.0f, s / 2.0f, 1.1f * s, MSG_SUB, INK); break;
    case 2: puz_digits(b, s, s, 100, s / 2.0f, s / 2.0f, 0.42f * s, INK, 1, 1); break;
    case 3: msg_sign(b, s, s, s / 2.0f, s / 2.0f, 1.1f * s, MSG_MUL, INK); break;
    case 4: msg_sign(b, s, s, s / 2.0f, s / 2.0f, 1.1f * s, MSG_DIV, INK); break;
    default:
        for (int i = 0; i < 4; i++)
            msg_sign(b, s, s, (0.3f + 0.4f * (i % 2)) * s, (0.3f + 0.4f * (i / 2)) * s, 0.5f * s, MSG_ADD + i, INK);
    }
    art_blend(rgba, size, size, b, s, s, o, o);
    free(b);
}

static const game mentalmath_game = {
    .id = "mentalmath", .order = 492, .variants = MMH_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = MMH_LEVELS, .chapters = MMH_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .par = par, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(mentalmath_game)
