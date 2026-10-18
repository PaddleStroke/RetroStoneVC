/*
 * The Tower of Hanoi in RetroStone Board, for one player: the house plank standing as a backdrop, a walnut rail along
 * its foot with three turned pegs, and lacquered wooden discs graded from red lacquer (the smallest) through amber,
 * jade and lapis to plum (the largest) (the rules: hanoi_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a peg: its top disc lifts above the pegs; tap another peg: it glides over and drops there (a larger disc on a
 * smaller one is refused: it shakes, and stays up); tap its own peg: it goes back down. Or drag a disc from its peg to
 * another. Undo takes back one move, all the way to the start.
 * The bar: the moves made, and at its right end the fewest possible (2^n - 1) by a small gold-lit tower. Solved:
 * the discs light up one by one from the bottom; in the fewest moves, the whole tower glows gold at the end.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/hanoi.h"
#include "games/puzzles.h"
#include "shell.h"

#define FRAME 0.035f
#define LIFT_MS 140
#define GLIDE_MS 360
#define SHAKE_MS 280
#define WAVE_STEP 120
#define WAVE_LEN 520

static const uint8_t LACQUER[HN_MAX][3] = {     /* the smallest first */
    {176, 54, 40}, {204, 98, 44}, {222, 166, 64}, {74, 136, 92}, {46, 112, 126}, {54, 84, 154}, {132, 64, 124}};

static hn_game G;
static int lifted = -1;                         /* the peg whose top disc is up */
static Uint32 lift_t0, shake_t0;
static struct { int on, disc, from, to; float x0, y0; Uint32 t0; } fly;   /* a disc gliding to its peg */
static struct { int on; Uint32 t0; } win;
static struct { int on, peg, moved; float x, y, x0, y0, gx, gy; } drag;   /* gx, gy: the finger's grip on the disc */

static box last_box;
static float bx, by, bw, bh, pitch, dh, yb, yp, hover;
static int fr, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_disc[HN_MAX + 1], *t_small, *t_tower;

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static float disc_w(int n, int size, float p) { return p * (0.30f + 0.62f * (size - 1) / (float)(n - 1)); }

/* the scene in a W x H room (game.fill): as wide as it may be (no wider than half again its height), as tall as
 * 0.9 of its width; the discs as thick as its height allows */
static void fit(int n, float W, float H, float *o_bw, float *o_bh, float *o_dh)
{
    float w = floorf(fminf(W, 1.5f * H)), h = floorf(fminf(H, 0.9f * w)), f = (int)(fminf(w, h) * FRAME);
    *o_dh = fminf(0.085f * w, (h - 2 * f) / (n + 4.3f));
    *o_bw = w;
    *o_bh = h;
}

static float peg_x(int p) { return bx + fr + (bw - 2 * fr) * (p + 0.5f) / 3; }
static float slot_y(int k) { return yb - (k + 0.5f) * dh; }   /* the k-th disc from the bottom: its centre */

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void tint(uint8_t *p, const uint8_t rgb[3], float k)
{
    static const float plank[3] = {222, 184, 128};
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * rgb[i] / plank[i] * k, 0, 255);
}

/* a disc seen from the side: lacquered wood (the grain under the colour), rounded, lit along its top */
static void paint_disc(int w, int h, const uint8_t rgb[3], uint8_t *o)
{
    int q = w > h ? w : h;
    uint8_t *plank = malloc((size_t)q * q * 4);
    if (!plank) return;
    art_board_n(1, 1, q, 0, 0, plank);
    for (int y = 0; y < h; y++) {
        float t = (y + 0.5f) / h, k = 1.16f - 0.42f * t + 0.22f * expf(-(t - 0.22f) * (t - 0.22f) * 90);
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            memcpy(p, plank + 4 * (y * q + x), 4);
            tint(p, rgb, k);
        }
    }
    free(plank);
    puz_bevel(o, w, h, 0.32f * h, fmaxf(1.5f, 0.16f * h), 0.3f);
}

/* the backdrop: the plank in its frame (w x h), the rail along its foot, the three pegs (for n discs of height d) and
 * their shadows */
static void paint_scene(uint8_t *o, int w, int h, int f, int n, float d)
{
    /* the plank: drawn square (its grain as the boards'), its walnut frame round the w x h */
    int inner_w = w - 2 * f;
    uint8_t *sq = malloc((size_t)w * w * 4);
    if (!sq) return;
    art_board_n(1, 1, inner_w, f, 0, sq);
    for (int y = 0; y < h; y++) {               /* the top rows, then the bottom rows of the frame */
        int sy = y < h - f ? y : y + (w - h);
        memcpy(o + 4 * y * w, sq + 4 * sy * w, (size_t)w * 4);
    }
    free(sq);
    float P = (float)inner_w / 3, foot = h - f - 0.9f * d, top = foot - (n + 1.0f) * d, pw = fmaxf(3, 0.3f * d);
    static const uint8_t walnut[3] = {118, 78, 50};
    for (int y = (int)(foot); y < (int)(foot + 0.62f * d); y++)   /* the rail */
        for (int x = (int)(f + 0.12f * d); x < (int)(w - f - 0.12f * d); x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float t = (y - foot) / (0.62f * d), k = t < 0.18f ? 1.25f : t > 0.82f ? 0.7f : 1;
            tint(p, walnut, k * 1.25f);
        }
    for (int y = (int)(foot + 0.62f * d); y < (int)(foot + 0.82f * d) && y < h - f; y++)   /* its shadow */
        for (int x = (int)(f + 0.2f * d); x < (int)(w - f - 0.05f * d); x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float e = fminf(x - f - 0.2f * d, w - f - 0.05f * d - x) / (0.5f * d);   /* its ends fade */
            float k = 1 - 0.28f * (1 - (y - foot - 0.62f * d) / (0.2f * d)) * clampf(e, 0, 1);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
    for (int i = 0; i < 3; i++) {
        float cx = f + P * (i + 0.5f);
        for (int y = (int)(top - pw / 2); y < (int)foot; y++)
            for (int x = (int)(cx - pw); x < (int)(cx + 1.6f * pw); x++) {
                float dx = (x + 0.5f - cx) / (pw / 2), dy = y + 0.5f - top;
                uint8_t *p = o + 4 * (y * w + x);
                float in = dy >= 0 ? clampf(1 - fabsf(dx) * pw / 2 + pw / 2 - 0.5f, 0, 1)
                                   : clampf((pw / 2 - hypotf(dx * pw / 2, dy)) + 0.5f, 0, 1);
                if (dx > 1 && dy > -pw / 4) {   /* the shadow on the plank, to the right, soft at its top */
                    float k = 1 - 0.22f * clampf(1 - (dx - 1) / 1.2f, 0, 1)
                                      * clampf((dy + pw / 4) / (1.5f * pw), 0, 1);
                    for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] * k);
                    continue;
                }
                if (in <= 0) continue;
                float shade = 1.28f - 0.5f * (dx + 1) / 2 + 0.25f * expf(-(dx + 0.45f) * (dx + 0.45f) * 6);
                uint8_t c[4];
                memcpy(c, p, 4);
                tint(c, walnut, shade * 1.1f);
                for (int k = 0; k < 3; k++) p[k] = (uint8_t)(p[k] + (c[k] - p[k]) * in);
            }
    }
}

/* a small tower: three discs on a peg (the token, the bar) */
static void tower_pic(int size, int discs, uint8_t *rgba)
{
    float d = size / (discs + 1.6f), P = size * 0.96f;
    int pw = (int)fmaxf(2, 0.3f * d), top = (int)(size - 0.5f * d - (discs + 0.6f) * d);
    for (int y = top; y < size - (int)(0.5f * d); y++)   /* the peg */
        for (int x = size / 2 - pw / 2; x < size / 2 - pw / 2 + pw; x++) {
            uint8_t *p = rgba + 4 * (y * size + x);
            float k = 1.2f - 0.4f * (x - (size / 2 - pw / 2)) / pw;
            p[0] = (uint8_t)clampf(118 * k, 0, 255), p[1] = (uint8_t)clampf(78 * k, 0, 255);
            p[2] = (uint8_t)clampf(50 * k, 0, 255), p[3] = 255;
        }
    for (int i = 0; i < discs; i++) {
        int s = discs - i, w = (int)(P * (0.38f + 0.6f * (s - 1) / (float)(discs - 1))), h = (int)fmaxf(2, d);
        uint8_t *b = calloc((size_t)w * h, 4);
        if (!b) return;
        paint_disc(w, h, LACQUER[(s - 1) * 2 % HN_MAX], b);
        art_blend(rgba, size, size, b, w, h, (size - w) / 2, (int)(size - 0.5f * d - (i + 1) * d));
        free(b);
    }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    tower_pic(size, 3, rgba);
}

static void layout(box b)
{
    last_box = b;
    fit(G.n, b.w, b.h, &bw, &bh, &dh);
    fr = (int)(fminf(bw, bh) * FRAME);
    bx = floorf(b.x + (b.w - bw) / 2);
    by = floorf(b.y + (b.h - bh) / 2);
    pitch = (bw - 2 * fr) / 3;
    yb = by + bh - fr - 0.9f * dh;
    yp = yb - (G.n + 1.0f) * dh;
    hover = yp - 0.95f * dh;
    int key = ((int)bw * 4096 + (int)bh) * 16 + G.n;
    if (made_key != key) {
        made_key = key;
        int w = (int)bw, h = (int)bh;
        ui_free(&t_board);
        paint_scene(ui_buf(w, w), w, h, fr, G.n, dh);
        t_board = ui_tex(w, h, ui_buf(w, w));
        for (int s = 1; s <= HN_MAX; s++) {
            ui_free(&t_disc[s]);
            if (s > G.n) continue;
            int dw = (int)disc_w(G.n, s, pitch), hh = (int)(0.96f * dh);
            memset(ui_buf(dw, hh), 0, (size_t)dw * hh * 4);
            paint_disc(dw, hh, LACQUER[(s - 1) * (HN_MAX - 1) / (G.n - 1)], ui_buf(dw, hh));
            t_disc[s] = ui_tex(dw, hh, ui_buf(dw, hh));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        tower_pic(small, 2, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
        ui_free(&t_tower);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        tower_pic(small, 3, ui_buf(small, small));
        t_tower = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    lifted = -1;
    fly.on = win.on = 0;
    memset(&drag, 0, sizeof drag);
    shake_t0 = 0;
}

static void begin(void)
{
    calm();
    made_key = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a designed level */
{
    if (!hn_level(&G, level)) hn_level(&G, 0);
    begin();
}

static void start_random(int variant)           /* 4 to 7 discs scattered by chance, a goal peg */
{
    hn_random(&G, 4 + (variant < 0 || variant >= HN_VARIANTS ? 0 : variant), (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return hn_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par; }
static int level_stars(void) { return ui_par_stars(G.moves, G.par); }

static Uint32 win_end(void) { return (Uint32)(G.n * WAVE_STEP + WAVE_LEN + 500); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (fly.on && now - fly.t0 < GLIDE_MS) || (win.on && now - win.t0 < win_end());
}

static int undo(void)
{
    calm();
    return hn_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = hn_script(&G, moves);
    if (hn_solved(&G)) { win.on = 1; win.t0 = ui_ms(); }
    return ok;
}

/* the moves since the start, in that notation (the shell's game.history) */
static int history(int i, char *move, int cap)
{
    if (move) hn_move_text(&G, i, move, cap);
    return G.moves;
}

/* the disc of `from` (lifted, or carried from x0, y0) goes to `to` */
static void drop(int from, int to, float x0, float y0)
{
    int disc = hn_top(&G, from);
    if (!hn_move(&G, from, to)) return;
    fly.on = 1;
    fly.disc = disc;
    fly.from = from;
    fly.to = to;
    fly.x0 = x0;
    fly.y0 = y0;
    fly.t0 = ui_ms();
    lifted = -1;
    ui_sound(SND_MOVE);
    ui_sound_later(SND_PLACE, GLIDE_MS - 40);
    if (hn_solved(&G)) { win.on = 1; win.t0 = fly.t0 + GLIDE_MS; }
    ui_animate();
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void disc_at(int s, float x, float y, float alpha)
{
    SDL_Texture *t = t_disc[s];
    if (!t) return;
    float w = disc_w(G.n, s, pitch), h = 0.96f * dh;
    float up = clampf((slot_y(0) - y) / (4 * dh), 0, 1);   /* its soft shadow, further off the higher it is */
    ui_shadow_flat(x + (0.06f + 0.3f * up) * dh, y + (0.47f + 0.15f * up) * dh, 1.02f * w, (0.34f + 0.2f * up) * dh,
                   alpha * (0.34f - 0.14f * up));
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    ui_put_c(t, x, y, w, h);
    SDL_SetTextureAlphaMod(t, 255);
}

/* where a gliding disc is: up from (x0, y0) to the hover line, across, down onto its slot */
static void glide_xy(float t, float *x, float *y)
{
    float x1 = peg_x(fly.to), y1 = slot_y(G.h[fly.to] - 1);
    float up = fmaxf(0, fly.y0 - hover), across = fabsf(x1 - fly.x0), down = y1 - hover, all = up + across + down;
    float d = puz_ease(t) * all;
    if (d < up) { *x = fly.x0; *y = fly.y0 - d; return; }
    d -= up;
    if (d < across) { *x = fly.x0 + (x1 > fly.x0 ? d : -d); *y = fminf(hover, fly.y0); return; }
    *x = x1;
    *y = fminf(hover, fly.y0) + (d - across) * (y1 - fminf(hover, fly.y0)) / fmaxf(1, down);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, bw, bh);
    if (G.goal >= 0 && !hn_solved(&G))         /* the goal peg: a soft gold light at its foot */
        ui_mark_gold_a(peg_x(G.goal), yb - 0.2f * dh, 0.95f * pitch, 0.6f);
    float ft = fly.on ? (now - fly.t0) / (float)GLIDE_MS : 2;
    if (fly.on && ft >= 1) fly.on = 0;
    float wt = win.on ? (float)(now - win.t0) : -1;
    for (int p = 0; p < 3; p++)
        for (int k = 0; k < G.h[p]; k++) {
            int s = G.disc[p][k];
            int top = k == G.h[p] - 1;
            if (top && fly.on && p == fly.to) continue;
            if (top && (p == lifted || (drag.on && drag.moved && p == drag.peg))) continue;
            float x = peg_x(p), y = slot_y(k);
            if (wt >= 0) {                      /* the win: the discs light up from the bottom */
                float q = puz_pulse(wt, k * WAVE_STEP, WAVE_LEN);
                if (q > 0) ui_mark_gold(x, y, disc_w(G.n, s, pitch) * 1.1f * q);
            }
            disc_at(s, x, y, 1);
        }
    if (wt >= 0) {
        float q = puz_pulse(wt, G.n * WAVE_STEP, WAVE_LEN + 500);
        if (q > 0 && G.moves == G.par) {        /* the fewest moves: the whole tower in gold light */
            int p = G.h[0] == G.n ? 0 : G.h[1] == G.n ? 1 : 2;
            ui_mark_gold(peg_x(p), slot_y(G.n / 2), 2.6f * pitch * q);
        }
        if (wt < win_end()) ui_animate();
        else win.on = 0;
    }
    if (lifted >= 0) {                          /* the disc up, rising to the hover line; refused: it shakes */
        int s = hn_top(&G, lifted);
        float t = clampf((now - lift_t0) / (float)LIFT_MS, 0, 1), y0 = slot_y(G.h[lifted] - 1);
        float x = peg_x(lifted), y = y0 + (hover - y0) * puz_ease(t);
        float st = (float)(now - shake_t0);
        if (shake_t0 && st < SHAKE_MS) x += sinf(st / SHAKE_MS * 6 * PUZ_PI) * 0.12f * dh * (1 - st / SHAKE_MS);
        if (t < 1 || (shake_t0 && st < SHAKE_MS)) ui_animate();
        ui_mark_last_round(x, y, 1.2f * dh);
        disc_at(s, x, y, 1);
    }
    if (fly.on) {
        float x, y;
        glide_xy(ft, &x, &y);
        disc_at(fly.disc, x, y, 1);
        ui_animate();
    }
    if (drag.on && drag.moved) {                /* a disc carried by the finger */
        int s = hn_top(&G, drag.peg);
        disc_at(s, drag.x - drag.gx, drag.y - drag.gy, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int peg_at(float x, float y)
{
    if (x < bx || x >= bx + bw || y < by - 0.5f * dh || y >= by + bh + 0.5f * dh) return -1;
    int p = (int)((x - bx - fr) / pitch);
    return p < 0 ? 0 : p > 2 ? 2 : p;
}

static void tap(int p)
{
    if (lifted < 0) {
        if (!G.h[p]) { ui_sound(SND_DENY); return; }
        lifted = p;
        lift_t0 = ui_ms();
        shake_t0 = 0;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (p == lifted) {                          /* back down */
        lifted = -1;
        ui_sound(SND_PLACE);
        ui_animate();
        return;
    }
    if (!hn_can(&G, lifted, p)) {
        shake_t0 = ui_ms();
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    drop(lifted, p, peg_x(lifted), hover);
}

static void press(float x, float y)
{
    memset(&drag, 0, sizeof drag);
    if (hn_solved(&G)) return;
    fly.on = 0;
    int p = peg_at(x, y);
    if (p < 0) return;
    drag.on = 1;
    drag.peg = p;
    drag.x = drag.x0 = x;
    drag.y = drag.y0 = y;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (!drag.moved && hypotf(x - drag.x0, y - drag.y0) > 0.45f * dh && G.h[drag.peg] &&
        (lifted < 0 || lifted == drag.peg)) {   /* the top disc taken in hand */
        drag.moved = 1;
        float dx = peg_x(drag.peg), dy = lifted == drag.peg ? hover : slot_y(G.h[drag.peg] - 1);
        drag.gx = drag.x0 - dx;
        drag.gy = drag.y0 - dy;
        if (fabsf(drag.gx) > pitch / 2) drag.gx = 0;
        if (fabsf(drag.gy) > 2 * dh) drag.gy = 0;
        lifted = -1;
        ui_sound(SND_TAP);
    }
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    ui_animate();
    int p = peg_at(x, y);
    if (!drag.moved) {
        if (p == drag.peg) tap(p);
        return;
    }
    if (p == drag.peg) {                        /* back down on its own peg */
        ui_sound(SND_PLACE);
        return;
    }
    if (p >= 0 && hn_can(&G, drag.peg, p)) {
        drop(drag.peg, p, x - drag.gx, y - drag.gy);
        return;
    }
    lifted = drag.peg;                          /* up over its own peg (refused: it shakes there) */
    lift_t0 = ui_ms() - LIFT_MS;
    if (p >= 0) {
        shake_t0 = ui_ms();
        ui_sound(SND_DENY);
    }
}

/* ---- the bar: the moves; the fewest possible at the right ---------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    float u = ui.unit, dh2 = 0.46f * u, cy = b.y + b.h / 2, sz = 0.6f * u;
    int best = G.par;
    int digits = best >= 100 ? 3 : best >= 10 ? 2 : 1;
    float nw = (0.74f * (digits - 1) + 0.62f) * dh2, x = b.x + b.w - nw;
    /* a narrow bar (landscape): the moves without their picture, so the fewest still fit */
    float need = (0.74f * (digits - 1) + 0.62f) * dh2 + 0.35f * u + sz + 0.12f * u + nw;
    int room = b.w >= need + sz + 0.12f * u;
    if (b.w < need) {
        ui_bar_number(b, NULL, G.moves);
        return;
    }
    ui_bar_number(b, room ? t_small : NULL, G.moves);
    int perfect = hn_solved(&G) && G.moves == best;
    ui_mark_gold(x - 0.12f * u - sz / 2, cy, (perfect ? 2.4f : 1.5f) * sz);
    ui_put(t_tower, x - 0.12f * u - sz, cy - sz / 2, sz, sz);
    ui_number(x + nw / 2, cy, dh2, best, 1);
}

/* ---- the menu's and the picker's cards --------------------------------------------------------------------------- */
/* a position on the pegs (the goal peg's foot in gold light), the discs as large as the picture allows */
static void paint_position(const hn_game *T, int size, uint8_t *rgba)
{
    int n = T->n, f = (int)(size * FRAME), w = size, h = (int)(0.84f * size), oy = (size - h) / 2;
    float d = (h - 2.0f * f) / (n + 2.4f);
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_scene(b, w, h, f, n, d);
    art_blend(rgba, size, size, b, w, h, 0, oy);
    float P = (w - 2.0f * f) / 3, foot = oy + h - f - 0.9f * d;
    if (T->goal >= 0) {
        static const uint8_t gold[3] = {255, 200, 90};
        int g = (int)(0.95f * P);
        memset(b, 0, (size_t)g * g * 4);
        art_disc(g, gold, 0.6f, 1.6f, b);
        art_blend(rgba, size, size, b, g, g, (int)(f + P * (T->goal + 0.5f) - g / 2.0f), (int)(foot - 0.2f * d - g / 2.0f));
    }
    for (int p = 0; p < 3; p++)
        for (int k = 0; k < T->h[p]; k++) {
            int s = T->disc[p][k], dw = (int)disc_w(n, s, P), hh = (int)(0.96f * d);
            memset(b, 0, (size_t)dw * hh * 4);
            paint_disc(dw, hh, LACQUER[(s - 1) * (HN_MAX - 1) / (n - 1)], b);
            art_blend(rgba, size, size, b, dw, hh, (int)(f + P * (p + 0.5f) - dw / 2.0f),
                      (int)(foot - (k + 0.5f) * d - hh / 2.0f));
        }
    free(b);
}

/* the menu's card (the classic tower under way) and the picker's (that many discs scattered, a goal peg) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    hn_game T;
    if (variant < 0) {
        hn_new(&T, 4);
        hn_script(&T, "=6");
    } else hn_random(&T, 4 + variant, 5);
    paint_position(&T, size, rgba);
}

/* the level screen: a level's position; a chapter's tab: the tower of its discs */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    hn_game T;
    if (hn_level(&T, level)) paint_position(&T, size, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba) { tower_pic(size, 3 + chapter, rgba); }

static const game hanoi_game = {
    .id = "hanoi", .order = 365, .variants = HN_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = HN_LEVELS, .chapter_of = hn_chapter_of, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1,
    .history = history,
};
REGISTER_GAME(hanoi_game)
