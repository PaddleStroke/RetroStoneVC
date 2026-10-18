/*
 * Balance Scales in RetroStone Board, for one player: brass scales, each on its own plank, the shapes (painted wooden
 * figures in the house's colours, each kind its own form) and numbered brass weights on their pans; under them (at
 * their right on a wide screen) a row of the shapes, each with a hollow for its weight, and a pad of wooden keys
 * (the rules: balance_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a shape of the row (or let the frame move on by itself), then type its weight on the pad: the scales take the
 * weights guessed, and a scale whose shapes all have one tips towards its heavier pan when they do not balance it
 * (a wrong guess shows). Every shape guessed and some wrong is a mistake. The shell's tool: the hint (a wrong weight
 * put right, else the next weight reasoning finds). Undo takes back a digit. The keys: the digits type, the arrows
 * move the frame over the shapes, Return goes to the next one. Solved: every scale level, gold light along them;
 * the stars count the hints and the mistakes (ui_logic_stars).
 * The shell's level screen chooses a level (150 in six chapters: balance_levels.h), or its die tab a puzzle made at
 * random (three variants).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/balance.h"
#include "games/balance_levels.h"
#include "games/bench_ui.h"
#include "games/pzkit.h"
#include "shell.h"

enum { T_HINT, TOOLS };
enum { K_BACK = 10 };
#define TILT_DEG 11.0f
#define TILT_MS 320
#define WAVE_MS 900

static const int COLOUR[BAL_SHAPES] = {2, 3, 5, 4, 1};   /* red circle, jade triangle, lapis square, amber star,
                                                          * ebony diamond */
static const uint8_t BRASS[3] = {206, 160, 74}, BRASS_DARK[3] = {104, 72, 26}, STRING[3] = {70, 52, 36},
                     INK[3] = {52, 34, 22};

static bal_game G;
static int sel, fresh = 1;                      /* the shape the pad types into; fresh: the next digit starts anew */
static int serial;

static box room;
static int wide, ncol, nrow;
static float sx, sy, cw, ch;                    /* the scales' grid: its corner, a cell */
static float lx[BAL_SHAPES], ly, lw, lh;        /* the row of shapes: each one's centre x, the row's centre y; a
                                                 * cell's size */
static float ly_of[BAL_SHAPES];
static float kx[11], ky[11], kd;                /* the pad's keys (0..9, back) */
static int made = -1, made_item = -1, made_small = -1, made_legend = -1;
static float lpx, lpy, lpw, lph;                /* the plank under the row of shapes */
static SDL_Texture *t_legend;
static SDL_Texture *t_plank, *t_key, *t_pan, *t_shape[BAL_SHAPES], *t_big[BAL_SHAPES], *t_weight, *t_small;
static float item;                              /* an item's side on a pan */
static struct { float from, to, now; Uint32 t0; } beam[BAL_SCALES];
static struct { int on; Uint32 t0; } wave;
static bench_flash hint;
static int held = -1;

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a shape's figure in its unit square */
static art_fig *shape_fig(int kind)
{
    art_fig *f = art_fig_new();
    switch (kind) {
    case 0: art_ellipse(f, ART_PART, 0.5f, 0.52f, 0.4f, 0.4f); break;
    case 1: { const float t[] = {0.5f, 0.1f, 0.92f, 0.86f, 0.08f, 0.86f}; art_poly(f, ART_PART, t, 3); break; }
    case 2: art_rect(f, ART_PART, 0.14f, 0.16f, 0.86f, 0.88f); break;
    case 3: art_star(f, ART_PART, 0.5f, 0.55f, 0.46f); break;
    default: { const float d[] = {0.5f, 0.06f, 0.9f, 0.5f, 0.5f, 0.94f, 0.1f, 0.5f}; art_poly(f, ART_PART, d, 4); }
    }
    return f;
}

static void paint_shape(uint8_t *o, int pw, int ph, int kind, int x, int y, int size)
{
    art_fig *f = shape_fig(kind);
    art_style st = art_player[COLOUR[kind % BAL_SHAPES]];
    st.line = 0.045f;
    art_fig_blend(o, pw, ph, f, &st, x, y, size, 1);
    art_fig_free(f);
}

static SDL_Texture *shape_tex(int kind, int size)
{
    uint8_t *o = ui_buf(size, size);
    memset(o, 0, (size_t)size * size * 4);
    paint_shape(o, size, size, kind, 0, 0, size);
    return ui_tex(size, size, o);
}

/* a brass weight: a block narrowing upwards, a knob on top, w wide */
static void paint_weight(uint8_t *o, int w, int h, float x0, float y0, float s)
{
    art_fig *f = art_fig_new();
    const float b[] = {0.08f, 0.95f, 0.92f, 0.95f, 0.8f, 0.3f, 0.2f, 0.3f};
    art_poly(f, ART_PART, b, 4);
    art_ellipse(f, ART_PART, 0.5f, 0.24f, 0.14f, 0.1f);
    art_style st = art_flat(BRASS, BRASS_DARK, 0.04f, 0.45f);
    art_fig_blend(o, w, h, f, &st, (int)x0, (int)y0, (int)s, 1);
    art_fig_free(f);
}

/* the pan: a shallow brass dish w x h */
static void paint_pan(uint8_t *o, int w, int h)
{
    art_fig *f = art_fig_new();
    float hh = 0.92f * h / w;                    /* the dish in units of its width */
    const float d[] = {0, 0, 1, 0, 0.86f, hh, 0.14f, hh};
    art_poly(f, ART_PART, d, 4);
    art_style st = art_flat(BRASS, BRASS_DARK, 0.012f, 0.5f);
    art_render(f, &st, w, h, (float)w, 0, 0, o);
    art_fig_free(f);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int s = (int)(0.9f * size), o = (size - s) / 2;
    uint8_t *b = calloc((size_t)s * s, 4);
    if (!b) return;
    pzk_stroke(b, s, s, 0.5f * s, 0.25f * s, 0.5f * s, 0.95f * s, 0.035f * s, BRASS, 1);
    pzk_stroke(b, s, s, 0.06f * s, 0.25f * s, 0.94f * s, 0.25f * s, 0.04f * s, BRASS, 1);
    pzk_stroke(b, s, s, 0.25f * s, 0.95f * s, 0.75f * s, 0.95f * s, 0.04f * s, BRASS, 1);
    paint_shape(b, s, s, 0, (int)(0.02f * s), (int)(0.27f * s), (int)(0.4f * s));
    paint_weight(b, s, s, 0.58f * s, 0.29f * s, 0.38f * s);
    art_blend(rgba, size, size, b, s, s, o, o);
    free(b);
}

/* ---- layout ------------------------------------------------------------------------------------------------------ */
static void layout(box b)
{
    room = b;
    int k = G.p.k > 0 ? G.p.k : 2, m = G.p.m > 0 ? G.p.m : 2;
    wide = b.w > 1.15f * b.h;
    float u = fminf(b.w, b.h);
    if (wide) {                                 /* the scales at the left; the shapes and the pad at the right */
        int lr = (k + 2) / 3;
        kd = floorf(fminf(b.h / (lr * 1.9f + 4.7f), 0.3f * b.w / 3.4f));
        float pw = 3.4f * kd, gap = 0.04f * u, aw = b.w - pw - gap;
        float px0 = b.x + aw + gap, top = b.y + (b.h - lr * 1.9f * kd - 4.6f * kd) / 2;
        lw = 1.1f * kd, lh = 1.9f * kd;
        for (int s = 0; s < k; s++) {
            int r = s / 3, in_row = r == lr - 1 ? k - r * 3 : 3;
            lx[s] = px0 + pw / 2 + (s % 3 - (in_row - 1) / 2.0f) * 1.12f * kd;
            ly_of[s] = top + (r + 0.5f) * lh;
        }
        for (int i = 0; i < 11; i++) {          /* 7 8 9 / 4 5 6 / 1 2 3 / 0 back */
            int d = i < 10 ? i : 10, r = d == 0 ? 3 : d == 10 ? 3 : 2 - (d - 1) / 3, c = d == 0 ? 1 : d == 10 ? 2 : (d - 1) % 3;
            kx[d] = px0 + pw / 2 + (c - 1) * 1.1f * kd;
            ky[d] = top + lr * lh + 0.2f * kd + (r + 0.5f) * 1.1f * kd;
        }
        ncol = m <= 2 ? m : m <= 4 ? 2 : 3;
        nrow = (m + ncol - 1) / ncol;
        cw = floorf(fminf(aw / ncol, 1.5f * b.h / nrow));
        ch = floorf(fminf(b.h / nrow, 0.9f * cw));
        sx = floorf(b.x + (aw - ncol * cw) / 2);
        sy = floorf(b.y + (b.h - nrow * ch) / 2);
    } else {                                    /* the scales above; the shapes, then the pad (two rows) */
        kd = floorf(fminf(b.w / 6.8f, 0.11f * b.h));
        lw = fminf(1.25f * kd, b.w / k), lh = 1.9f * kd;
        float padh = 2 * 1.1f * kd, low = b.y + b.h - padh - 0.1f * kd;
        float lyy = low - 0.25f * kd - lh / 2;
        for (int s = 0; s < k; s++) lx[s] = b.x + b.w / 2 + (s - (k - 1) / 2.0f) * 1.15f * lw, ly_of[s] = lyy;
        static const int ORDER[11] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 10};
        for (int i = 0; i < 11; i++) {
            int d = ORDER[i];
            kx[d] = b.x + b.w / 2 + (i % 6 - 2.5f) * 1.1f * kd;
            ky[d] = low + (i / 6 + 0.5f) * 1.1f * kd;
        }
        float ah = lyy - lh / 2 - 0.2f * kd - b.y;
        ncol = m <= 2 ? 1 : 2;
        nrow = (m + ncol - 1) / ncol;
        if (m == 2 && ah / 2 < 0.55f * b.w / 2) ncol = 2, nrow = 1;
        cw = floorf(fminf(b.w / ncol, 1.6f * ah / nrow));
        ch = floorf(fminf(ah / nrow, 0.9f * cw));
        sx = floorf(b.x + (b.w - ncol * cw) / 2);
        sy = floorf(b.y + (ah - nrow * ch) / 2);
    }
    ly = ly_of[0];
    lpx = 1e9f, lpy = 1e9f;                    /* the row of shapes on a plank of its own */
    float rx = -1e9f, ry = -1e9f;
    for (int s = 0; s < k; s++) {
        lpx = fminf(lpx, lx[s] - 0.6f * lw), rx = fmaxf(rx, lx[s] + 0.6f * lw);
        lpy = fminf(lpy, ly_of[s] - 0.55f * lh), ry = fmaxf(ry, ly_of[s] + 0.55f * lh);
    }
    lpx = floorf(lpx), lpy = floorf(lpy), lpw = floorf(rx - lpx), lph = floorf(ry - lpy);
    int lkey = (int)lpw * 4096 + (int)lph * 2 + art_frame_is_light();
    if (made_legend != lkey && lpw > 4 && lph > 4) {
        made_legend = lkey;
        int fr = (int)fmaxf(2, 0.05f * lph);
        uint8_t *o = ui_buf((int)lpw, (int)lph);
        art_plank((int)lpw - 2 * fr, (int)lph - 2 * fr, fr, o);
        ui_free(&t_legend);
        t_legend = ui_tex((int)lpw, (int)lph, o);
    }
    int key = ((int)cw * 4096 + (int)ch) * 2 + art_frame_is_light();
    if (made != key) {
        made = key;
        int w = (int)cw - 6, h = (int)ch - 6, fr = (int)fmaxf(2, 0.03f * fminf(w, h));
        uint8_t *o = ui_buf(w, h);
        art_plank(w - 2 * fr, h - 2 * fr, fr, o);
        ui_free(&t_plank);
        t_plank = ui_tex(w, h, o);
        int pw = (int)(0.32f * cw), ph = (int)fmaxf(3, 0.14f * pw);
        o = ui_buf(pw, ph);
        memset(o, 0, (size_t)pw * ph * 4);
        paint_pan(o, pw, ph);
        ui_free(&t_pan);
        t_pan = ui_tex(pw, ph, o);
    }
    item = floorf(fminf(0.32f * cw / 3.8f, 0.17f * ch));
    int ik = (int)item * 64 + (int)(0.8f * kd);
    if (made_item != ik && item > 2) {
        made_item = ik;
        for (int s = 0; s < BAL_SHAPES; s++) {
            ui_free(&t_shape[s]);
            t_shape[s] = shape_tex(s, (int)item);
            ui_free(&t_big[s]);
            t_big[s] = shape_tex(s, (int)(0.8f * kd));
        }
        int ws = (int)(1.2f * item);
        uint8_t *o = ui_buf(ws, ws);
        memset(o, 0, (size_t)ws * ws * 4);
        paint_weight(o, ws, ws, 0, 0, (float)ws);
        ui_free(&t_weight);
        t_weight = ui_tex(ws, ws, o);
        int k2 = (int)kd;
        o = ui_buf(k2, k2);
        bench_cut(o, k2, k2, 0.16f * k2, fmaxf(2, 0.08f * k2), 0.32f);
        ui_free(&t_key);
        t_key = ui_tex(k2, k2, o);
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int target(int i)                        /* a scale's tilt now (degrees, + its left pan down) */
{
    int t = bal_tilt(&G.p.sc[i], G.guess);
    return t == -1 ? 1 : t == 1 ? -1 : 0;
}

static void settle(void)                        /* the beams set at once */
{
    for (int i = 0; i < BAL_SCALES; i++) beam[i].from = beam[i].to = beam[i].now = i < G.p.m ? TILT_DEG * target(i) : 0;
}

static void begin(void)
{
    serial++;
    sel = 0;
    fresh = 1;
    wave.on = 0;
    hint.at = -1;
    held = -1;
    settle();
    if (room.w > 0) layout(room);
}

static void start(int level)
{
    bal_puzzle p;
    if (!bal_decode(bal_levels[level < 0 || level >= BAL_LEVELS ? 0 : level], &p)) bal_new(&G, 0, 1);
    else bal_load(&G, &p);
    begin();
}

static void start_random(int variant)
{
    bal_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return bal_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int moving(void)
{
    for (int i = 0; i < G.p.m; i++)
        if (beam[i].now != beam[i].to) return 1;
    return 0;
}
static int busy(void) { return moving() || (wave.on && ui_ms() - wave.t0 < WAVE_MS); }
static int score(void) { return G.moves; }
static int stars(void) { return ui_logic_stars(G.hints, G.mistakes); }

/* the guesses changed: the beams swing to their new tilt; solved: the light runs along the scales */
static void changed(void)
{
    Uint32 now = ui_ms();
    for (int i = 0; i < G.p.m; i++) {
        float t = TILT_DEG * target(i);
        if (t != beam[i].to) beam[i].from = beam[i].now, beam[i].to = t, beam[i].t0 = now;
    }
    if (bal_solved(&G) && !wave.on) {
        wave.on = 1;
        wave.t0 = now;
        ui_sound(SND_END);
    }
    ui_animate();
}

static int digits(int v) { return v >= 10 ? 2 : 1; }

static void next_shape(void)                    /* the frame on to the next shape still without its weight */
{
    for (int k = 1; k <= G.p.k; k++) {
        int s = (sel + k) % G.p.k;
        if (!G.guess[s]) { sel = s; break; }
    }
    fresh = 1;
}

static void type(int d)
{
    if (bal_solved(&G)) return;
    int v = fresh ? d : G.guess[sel] * 10 + d;
    if (v > 99) v = d;
    if (v == 0) { ui_sound(SND_DENY); return; }   /* no shape weighs 0 */
    bal_set(&G, sel, v);
    fresh = 0;
    int m = G.mistakes;
    if (digits(v) >= digits(bal_most(&G.p))) {  /* all its digits: judged, the frame moves on */
        bal_judge(&G);
        next_shape();
    }
    ui_sound(G.mistakes > m ? SND_DENY : SND_PLACE);
    changed();
}

static void back(void)
{
    if (!G.guess[sel] || bal_solved(&G)) { ui_sound(SND_DENY); return; }
    bal_set(&G, sel, G.guess[sel] / 10);
    fresh = 0;
    ui_sound(SND_TAP);
    changed();
}

static int undo(void)
{
    if (!bal_undo(&G)) return 0;
    fresh = 1;
    hint.at = -1;
    changed();
    return 1;
}

static int script(const char *moves)
{
    int ok = bal_script(&G, moves);
    settle();
    if (bal_solved(&G)) wave.on = 1, wave.t0 = ui_ms();
    return ok;
}

static void do_hint(void)
{
    int s = bal_hint(&G);
    if (s < 0) { ui_sound(SND_DENY); return; }
    sel = s;
    next_shape();
    bench_flash_start(&hint, s);
    ui_sound(SND_PLACE);
    changed();
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void brass_line(float ax, float ay, float bx, float by, float wd)
{
    pzk_line(ax, ay, bx, by, wd + 2, 1, (pzk_rgba){BRASS_DARK[0], BRASS_DARK[1], BRASS_DARK[2], 255}, 0);
    pzk_line(ax, ay, bx, by, wd, 1, (pzk_rgba){BRASS[0], BRASS[1], BRASS[2], 255}, 0);
}

/* a pan's items standing on it (its top centre at cx, cy) */
static void pan_items(const bal_scale *s, int side, float cx, float cy)
{
    int n = s->n[side] + (s->w[side] > 0), per = n < 4 ? n : 4;
    float step = item * 1.05f;
    for (int i = 0; i < n; i++) {
        int r = i / per, c = i % per, in_row = r == (n - 1) / per ? n - r * per : per;
        float x = cx + (c - (in_row - 1) / 2.0f) * step, y = cy - (r + 0.5f) * step;
        if (i < s->n[side]) ui_put_c(t_shape[s->s[side][i]], x, y, item, item);
        else {
            float ws = 1.2f * item;
            ui_put_c(t_weight, x, y - 0.1f * item, ws, ws);
            int v = s->w[side];
            ui_number_ex(x, y + 0.12f * item, (v >= 10 ? 0.42f : 0.5f) * item, v, INK, 255, UI_NUM_TIGHT);
        }
    }
}

static void draw_scale(int i, float x, float y)
{
    const bal_scale *s = &G.p.sc[i];
    ui_put(t_plank, x + 3, y + 3, cw - 6, ch - 6);
    float px = x + cw / 2, py = y + 0.3f * ch, L = 0.3f * cw, foot = y + ch - 0.1f * ch;
    Uint32 now = ui_ms();
    if (beam[i].now != beam[i].to) {
        float f = (now - beam[i].t0) / (float)TILT_MS;
        beam[i].now = f >= 1 ? beam[i].to : beam[i].from + (beam[i].to - beam[i].from) * ui_ease(f);
        ui_animate();
    }
    float a = beam[i].now * ART_PI / 180, cs = cosf(a), sn = sinf(a);
    brass_line(px, py, px, foot, 0.035f * cw);   /* the post and its foot */
    brass_line(px - 0.14f * cw, foot, px + 0.14f * cw, foot, 0.04f * ch);
    float ends[2][2] = {{px - L * cs, py + L * sn}, {px + L * cs, py - L * sn}};
    int pw = (int)(0.32f * cw), ph = (int)fmaxf(3, 0.14f * pw);
    float hang = 0.3f * ch;
    for (int side = 0; side < 2; side++) {      /* the strings, the pan, its load */
        float ex = ends[side][0], ey = ends[side][1], pcy = ey + hang;
        pzk_rgba str = {STRING[0], STRING[1], STRING[2], 230};
        pzk_line(ex, ey, ex - 0.46f * pw, pcy, fmaxf(1.5f, 0.006f * cw), 1, str, 0);
        pzk_line(ex, ey, ex + 0.46f * pw, pcy, fmaxf(1.5f, 0.006f * cw), 1, str, 0);
        pan_items(s, side, ex, pcy);
        ui_put(t_pan, ex - pw / 2.0f, pcy - 0.15f * ph, (float)pw, (float)ph);
    }
    brass_line(ends[0][0], ends[0][1], ends[1][0], ends[1][1], fmaxf(3, 0.035f * ch));   /* the beam */
    pzk_dot(px, py, 0.05f * ch, 1, (pzk_rgba){BRASS[0], BRASS[1], BRASS[2], 255}, 0);
    if (wave.on) {                              /* solved: a gold light along the scales */
        float p = puz_pulse((float)(now - wave.t0), i * 120.0f, 600);
        if (p > 0) ui_mark_gold(px, py, 0.5f * cw * p);
    } else if (bal_tilt(s, G.guess) == -1 || bal_tilt(s, G.guess) == 1) {
        ui_mark_glow(px, py, 0.12f * ch);       /* a wrong guess: the pivot glows red */
    }
}

static void draw(void)
{
    if (!t_plank) layout(room);
    if (!t_plank) return;
    int solved = bal_solved(&G);
    for (int i = 0; i < G.p.m; i++) {
        int r = i / ncol, c = i % ncol, in_row = r == nrow - 1 ? G.p.m - r * ncol : ncol;
        draw_scale(i, sx + (c + (ncol - in_row) / 2.0f) * cw, sy + r * ch);
    }
    if (t_legend) ui_put(t_legend, lpx, lpy, lpw, lph);
    for (int s = 0; s < G.p.k; s++) {           /* the row of shapes, each with its weight */
        float x = lx[s], y = ly_of[s], d = 0.8f * kd, bw = 1.0f * kd, bh = 0.8f * kd, by = y + 0.45f * lh - bh / 2 - 0.1f * kd;
        if (s == sel && !solved) ui_mark_select(x - 0.56f * lw, y - lh / 2, 1.12f * lw, lh);
        if (hint.at == s) bench_glow(&hint, x, by, 1.6f * kd);
        ui_put_c(t_big[s], x, y - 0.42f * lh + d / 2, d, d);
        ui_fill(x - bw / 2, by - bh / 2, bw, bh, 40, 26, 16, solved ? 40 : 90);
        ui_fill(x - bw / 2, by - bh / 2, bw, fmaxf(2, 0.12f * bh), 20, 12, 6, 80);
        if (G.guess[s]) {
            const uint8_t *ink = solved ? PZK_GOLD : BENCH_BLUE;
            ui_number_ex(x, by, 0.6f * bh, G.guess[s], ink, 255, UI_NUM_TIGHT);
        }
    }
    for (int d = 0; d <= 10; d++) {             /* the pad */
        float s = held == d ? 0.92f : 1, kw = kd * s;
        if (d == K_BACK) {
            ui_button(kx[d], ky[d], 0.8f * kw, ICON_UNDO, solved || !G.guess[sel] ? UI_DIM : 0);
            continue;
        }
        SDL_SetTextureAlphaMod(t_key, solved ? 120 : 255);
        ui_shadow_flat(kx[d], ky[d], kw, kw, 0.7f);
        ui_put_c(t_key, kx[d], ky[d], kw, kw);
        SDL_SetTextureAlphaMod(t_key, 255);
        ui_number_ex(kx[d], ky[d], 0.55f * kw, d, INK, solved ? 120 : 255, 0);
    }
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the finger and the keys ------------------------------------------------------------------------------------- */
static int key_at(float x, float y)
{
    for (int d = 0; d <= 10; d++)
        if (fabsf(x - kx[d]) <= 0.55f * kd && fabsf(y - ky[d]) <= 0.55f * kd) return d;
    return -1;
}

static void press_key(int d)
{
    if (d == K_BACK) back();
    else type(d);
}

static void press(float x, float y)
{
    held = -1;
    if (bal_solved(&G)) return;
    if (ui_tool_hit(x, y) == T_HINT) { do_hint(); return; }
    for (int s = 0; s < G.p.k; s++)             /* a shape of the row: the pad types its weight */
        if (fabsf(x - lx[s]) <= 0.56f * lw && fabsf(y - ly_of[s]) <= lh / 2) {
            if (sel != s) ui_sound(SND_TAP);
            sel = s;
            fresh = 1;
            ui_animate();
            return;
        }
    int d = key_at(x, y);
    if (d < 0) return;
    held = d;
    press_key(d);
    ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    held = -1;
    ui_animate();
}

static void key(SDL_Keycode k)
{
    if (bal_solved(&G)) return;
    if (k >= SDLK_0 && k <= SDLK_9) { type((int)(k - SDLK_0)); return; }
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) { type((int)(k - SDLK_KP_1) + 1); return; }
    if (k == SDLK_KP_0) { type(0); return; }
    int step = (k == SDLK_RIGHT || k == SDLK_DOWN || k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) -
               (k == SDLK_LEFT || k == SDLK_UP);
    if (!step) return;
    sel = (sel + step + G.p.k) % G.p.k;
    fresh = 1;
    ui_sound(SND_TAP);
    ui_animate();
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.moves);
}

/* ---- the pictures for the shell ---------------------------------------------------------------------------------- */
/* a scale painted level (its load on its pans) over a picture w x h, in the box x0, y0, bw x bh */
static void paint_scale(uint8_t *o, int w, int h, const bal_scale *s, float x0, float y0, float bw, float bh)
{
    float px = x0 + bw / 2, py = y0 + 0.3f * bh, L = 0.3f * bw, foot = y0 + 0.92f * bh, it = fminf(0.32f * bw / 3.8f, 0.17f * bh);
    pzk_stroke(o, w, h, px, py, px, foot, 0.018f * bw, BRASS, 1);
    pzk_stroke(o, w, h, px - 0.14f * bw, foot, px + 0.14f * bw, foot, 0.022f * bh, BRASS, 1);
    for (int side = 0; side < 2; side++) {
        float ex = side ? px + L : px - L, pcy = py + 0.3f * bh, pw = 0.32f * bw;
        pzk_stroke(o, w, h, ex, py, ex - 0.46f * pw, pcy, fmaxf(0.8f, 0.003f * bw), STRING, 1);
        pzk_stroke(o, w, h, ex, py, ex + 0.46f * pw, pcy, fmaxf(0.8f, 0.003f * bw), STRING, 1);
        int n = s->n[side] + (s->w[side] > 0), per = n < 4 ? n : 4;
        for (int i = 0; i < n; i++) {
            int r = i / per, c = i % per, in_row = r == (n - 1) / per ? n - r * per : per;
            float x = ex + (c - (in_row - 1) / 2.0f) * 1.05f * it, y = pcy - (r + 0.5f) * 1.05f * it;
            if (i < s->n[side]) paint_shape(o, w, h, s->s[side][i], (int)(x - it / 2), (int)(y - it / 2), (int)it);
            else {
                paint_weight(o, w, h, x - 0.6f * it, y - 0.7f * it, 1.2f * it);
                if (it >= 10) puz_digits(o, w, h, s->w[side], x, y + 0.12f * it, 0.45f * it, INK, 1, 1);
            }
        }
        pzk_stroke(o, w, h, ex - pw / 2, pcy, ex + pw / 2, pcy, 0.025f * pw + 1, BRASS, 1);
    }
    pzk_stroke(o, w, h, px - L, py, px + L, py, 0.016f * bh + 1, BRASS, 1);
    pzk_ball(o, w, h, px, py, 0.025f * bh + 1, BRASS, 1, 0.5f);
}

/* a puzzle's scales on planks, the whole card (size x size), the first n of them */
static void paint_card(const bal_puzzle *p, int n, int full, int size, uint8_t *rgba)
{
    n = n < p->m ? n : p->m;
    int cols = n <= 1 ? 1 : 2, rows = (n + cols - 1) / cols;
    float c = (float)size / cols, r = fminf((float)size / rows, 0.85f * c);
    for (int i = 0; i < n; i++) {
        int x = (int)((i % cols) * c), y = (int)((size - rows * r) / 2 + (i / cols) * r), w = (int)c - 2, h = (int)r - 2;
        uint8_t *o = malloc((size_t)w * h * 4);
        if (!o) return;
        int fr = (int)fmaxf(1, 0.03f * fminf(w, h));
        art_plank(w - 2 * fr, h - 2 * fr, fr, o);
        paint_scale(o, w, h, &p->sc[i], 0, 0, (float)w, (float)h);
        art_blend(rgba, full, full, o, w, h, x + 1 + (full - size) / 2, y + 1);
        free(o);
    }
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    bal_puzzle p;
    if (variant < 0) {                          /* the menu's: a level with shapes on both pans */
        if (bal_decode(bal_levels[60], &p)) paint_card(&p, 1, size, size, rgba);
        return;
    }
    bal_make m = bal_variant_make(variant);
    bal_make_puzzle(&m, 2026u + (uint32_t)variant, &p);
    paint_card(&p, variant + 1, size, (int)(0.84f * size), rgba);
    bench_stones(rgba, size, (int)(0.84f * size), variant);
}

/* a chapter's tab: the first scale of its first level */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    bal_puzzle p;
    int ch = chapter < 0 ? 0 : chapter >= BAL_CHAPTERS ? BAL_CHAPTERS - 1 : chapter;
    if (!bal_decode(bal_levels[ch * BAL_LEVELS / BAL_CHAPTERS + 6], &p)) return;
    uint8_t *o = calloc((size_t)size * size, 4);
    if (!o) return;
    paint_scale(o, size, size, &p.sc[p.m - 1], 0, -0.05f * size, (float)size, (float)size);
    art_blend(rgba, size, size, o, size, size, 0, 0);
    free(o);
}

static const game balance_game = {
    .id = "balance", .order = 493, .variants = BAL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = BAL_LEVELS, .chapters = BAL_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(balance_game)
