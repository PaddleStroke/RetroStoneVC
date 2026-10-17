/*
 * Carrom in RetroStone Board: the square board in its walnut frame, the coins sliding as the rules' physics runs them
 * (cuetable.h), the aiming line to the first contact. The striker slides along the shooter's baseline (drag it, or
 * tap the baseline); aim by pulling back from it and letting go, or tap where to aim and tap the power gauge. Two
 * players, or four in teams. The rules: carrom_rules.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The moves' notation (script, the bots): "pos:angle:power" (carrom.h), shots separated by spaces.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/carrom.h"
#include "games/cuetable_ui.h"
#include "shell.h"

#define FRAME 7.0f                              /* the frame round the playing surface, units */
#define OUT (CRM_SIZE + 2 * FRAME)
#define PULL_FULL 24.0f
#define INTRO_MS 650

static crm_game G;
static cue_table T;
static cue_view V;
static cue_gauge Gg;
static cue_replay R;
static SDL_Texture *t_board, *t_coin[3], *t_striker, *t_small[3];
static int made_s = -1, made_rot = -1, made_d = -1, made_small = -1;

enum { F_NONE, F_AIM, F_PULL, F_GAUGE, F_SLIDE, F_PENDING };
static struct {
    int mode;
    float ang, power, pull;
    int pos;                                    /* the striker's place along the baseline (-1000..1000) */
    float x0, y0;
    int moved;
} A;
static struct { int on; Uint32 t0; int pos, ang, pow, from; } intro;

static const crm_pos *P(void) { return crm_now(&G); }

/* ---- the board's picture ----------------------------------------------------------------------------------------- */
static const uint8_t INK[3] = {70, 40, 26}, LACQUER[3] = {170, 44, 36};

/* a ring of ink (cx, cy, radius r, width lw; pixels) on the board's picture */
static void ring(uint8_t *o, int w, int h, float cx, float cy, float r, float lw, const uint8_t c[3], float a,
                 int filled)
{
    for (int y = (int)(cy - r - lw - 2); y <= (int)(cy + r + lw + 2); y++)
        for (int x = (int)(cx - r - lw - 2); x <= (int)(cx + r + lw + 2); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float d = hypotf(x + 0.5f - cx, y + 0.5f - cy);
            float k = filled ? clampf(r - d + 0.5f, 0, 1) : clampf(lw / 2 - fabsf(d - r) + 0.5f, 0, 1);
            if (k <= 0) continue;
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] + (c[i] - p[i]) * k * a);
        }
}

/* a line of ink from (x0, y0) to (x1, y1), lw wide (pixels) */
static void line(uint8_t *o, int w, int h, float x0, float y0, float x1, float y1, float lw, const uint8_t c[3], float a)
{
    float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy;
    int xa = (int)fminf(x0, x1) - (int)lw - 2, xb = (int)fmaxf(x0, x1) + (int)lw + 2;
    int ya = (int)fminf(y0, y1) - (int)lw - 2, yb = (int)fmaxf(y0, y1) + (int)lw + 2;
    for (int y = ya; y <= yb; y++)
        for (int x = xa; x <= xb; x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float px = x + 0.5f - x0, py = y + 0.5f - y0, t = clampf((px * dx + py * dy) / L2, 0, 1);
            float d = hypotf(px - t * dx, py - t * dy), k = clampf(lw / 2 - d + 0.5f, 0, 1);
            if (k <= 0) continue;
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] + (c[i] - p[i]) * k * a);
        }
}

/* the board n x n pixels at s pixels a unit: light wood in a walnut frame, the baselines and their circles, the
 * arrows, the middle's circles, the holes */
static void paint_board(float s, int n, uint8_t *o)
{
    int fr = (int)(FRAME * s);
    art_plank(n - 2 * fr, n - 2 * fr, fr, o);
    float f = FRAME * s, lw = fmaxf(1, 0.32f * s);
    for (int y = fr; y < n - fr; y++)           /* the playing surface: paler, polished */
        for (int x = fr; x < n - fr; x++) {
            uint8_t *p = o + 4 * ((size_t)y * n + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)fminf(255, p[i] * 0.86f + 46);
        }
    for (int side = 0; side < 4; side++) {
        float pts[8][2];
        static const float L[8][2] = {{20, CRM_BASE - 2.15f}, {80, CRM_BASE - 2.15f}, {20, CRM_BASE + 2.15f},
                                      {80, CRM_BASE + 2.15f}, {20, CRM_BASE}, {80, CRM_BASE}, {12.5f, 87.5f},
                                      {30, 70}};
        for (int k = 0; k < 8; k++) {
            crm_turn_pt(side, L[k][0], L[k][1], &pts[k][0], &pts[k][1]);
            pts[k][0] = f + pts[k][0] * s;
            pts[k][1] = f + pts[k][1] * s;
        }
        line(o, n, n, pts[0][0], pts[0][1], pts[1][0], pts[1][1], lw, INK, 0.85f);
        line(o, n, n, pts[2][0], pts[2][1], pts[3][0], pts[3][1], lw, INK, 0.85f);
        for (int e = 4; e < 6; e++) {           /* the baseline's end circles, red */
            ring(o, n, n, pts[e][0], pts[e][1], 2.15f * s, lw, INK, 0.85f, 0);
            ring(o, n, n, pts[e][0], pts[e][1], 1.6f * s, lw, LACQUER, 0.9f, 1);
        }
        /* the arrow from the corner towards the middle, its head at the baseline's end */
        line(o, n, n, pts[6][0], pts[6][1], pts[7][0], pts[7][1], lw, INK, 0.75f);
        float ax = pts[7][0] - pts[6][0], ay = pts[7][1] - pts[6][1], al = hypotf(ax, ay);
        ax /= al;
        ay /= al;
        float hl = 2.4f * s;
        line(o, n, n, pts[7][0], pts[7][1], pts[7][0] - hl * (ax - ay * 0.6f), pts[7][1] - hl * (ay + ax * 0.6f), lw,
             INK, 0.75f);
        line(o, n, n, pts[7][0], pts[7][1], pts[7][0] - hl * (ax + ay * 0.6f), pts[7][1] - hl * (ay - ax * 0.6f), lw,
             INK, 0.75f);
    }
    float c = f + 50 * s;
    ring(o, n, n, c, c, 11.5f * s, lw, INK, 0.85f, 0);
    ring(o, n, n, c, c, 2.4f * s, lw, INK, 0.7f, 0);
    ring(o, n, n, c, c, 1.6f * s, lw, LACQUER, 0.9f, 1);
    for (int k = 0; k < 4; k++) {               /* the holes, dark under a bevelled rim */
        float hx = f + (k & 1 ? CRM_SIZE - CRM_HOLE_AT : CRM_HOLE_AT) * s,
              hy = f + (k & 2 ? CRM_SIZE - CRM_HOLE_AT : CRM_HOLE_AT) * s, r = CRM_HOLE_R * s * 1.06f;
        for (int y = (int)(hy - r - 3); y <= (int)(hy + r + 3); y++)
            for (int x = (int)(hx - r - 3); x <= (int)(hx + r + 3); x++) {
                if (x < 0 || y < 0 || x >= n || y >= n) continue;
                float dx = x + 0.5f - hx, dy = y + 0.5f - hy;
                float kk = art_recess(dx, dy, r, 0.18f * r, 0.12f, 0.3f, 0.25f);
                uint8_t *p = o + 4 * ((size_t)y * n + x);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * kk, 0, 255);
            }
    }
}

/* a coin (0 white, 1 black, 2 the queen) or the striker (3: ivory, a gold ring), d x d */
static void paint_disc(int kind, int d, uint8_t *o)
{
    art_counter(COUNTER_DISC, kind == 3 ? 0 : kind, d, o);
    if (kind == 3) {
        static const uint8_t gold[3] = {204, 160, 64};
        uint8_t *r = malloc((size_t)d * d * 4);
        if (!r) return;
        art_ring(d, 0.60f, gold, 0.9f, r);
        for (int i = 0; i < d * d; i++) {       /* the ring only where the counter is */
            float a = r[4 * i + 3] / 255.0f * (o[4 * i + 3] / 255.0f);
            float rr = sqrtf((float)((i % d - d / 2) * (i % d - d / 2) + (i / d - d / 2) * (i / d - d / 2))) / (d / 2.0f);
            if (rr > 0.78f) a = 0;
            for (int c = 0; c < 3; c++) o[4 * i + c] = (uint8_t)(o[4 * i + c] * (1 - a) + r[4 * i + c] * a);
        }
        free(r);
    }
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static int side_now(void) { return crm_side(P(), P()->turn); }

static void aim_default(void)
{
    A.ang = (270 - 90 * side_now()) * CUE_PI / 180;
    A.pos = crm_free_pos(P(), P()->turn, 0);
}

static void start(int v)
{
    (void)v;
    crm_table(&T);
    crm_game_new(&G, ui.players == 4 ? 4 : 2);
    R.on = intro.on = 0;
    A.mode = F_NONE;
    A.power = 0.6f;
    aim_default();
}

static int turn(void) { return P()->turn; }

static int status(void)
{
    int s = P()->status;
    return s == CRM_WIN0 ? GS_WIN0 : s == CRM_WIN1 ? GS_WIN1 : GS_PLAY;
}

static unsigned winners(void)
{
    int s = P()->status;
    if (s == CRM_PLAY) return 0;
    int team = s == CRM_WIN1;
    return P()->players == 4 ? (team ? 0xAu : 0x5u) : 1u << team;
}

static void home(int player, float *fx, float *fy)
{
    if (P()->players != 4) { *fx = 0.5f; *fy = player ? 0 : 1; return; }
    static const float H[4][2] = {{0.3f, 1}, {1, 0.6f}, {0.7f, 0}, {0, 0.4f}};
    *fx = H[player & 3][0];
    *fy = H[player & 3][1];
}

static int busy(void) { return R.on || intro.on; }

static int shoot(int pos, int ang, int pow, int animate)
{
    cue_world w;
    if (!crm_ready(P(), pos, ang, pow, &w)) return 0;
    if (!crm_play(&G, pos, ang, pow, NULL)) return 0;
    if (animate) {
        cue_replay_start(&R, &T, &w);
        ui_sound(SND_MOVE);
    }
    A.mode = F_NONE;
    aim_default();
    return 1;
}

static int undo(void)
{
    if (!crm_undo(&G)) return 0;
    R.on = intro.on = 0;
    A.mode = F_NONE;
    aim_default();
    return 1;
}

static int script(const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int pos, ang, pow, n;
        if (!crm_parse(s, &pos, &ang, &pow, &n) || !shoot(pos, ang, pow, 0)) return 0;
        s += n;
    }
    return 1;
}

const crm_game *carrom_live(void) { return &G; }

int carrom_show(const char *move)
{
    int pos, ang, pow, n;
    cue_world w;
    if (busy() || !crm_parse(move, &pos, &ang, &pow, &n) || !crm_ready(P(), pos, ang, pow, &w)) return 0;
    intro.on = 1;
    intro.t0 = ui_ms();
    intro.pos = pos;
    intro.ang = ang;
    intro.pow = pow;
    intro.from = A.pos;
    A.mode = F_NONE;
    return 1;
}

static void intro_tick(void)
{
    if (!intro.on) return;
    ui_animate();
    if (ui_ms() - intro.t0 < INTRO_MS) return;
    intro.on = 0;
    if (shoot(intro.pos, intro.ang, intro.pow, 1)) R.t0 = intro.t0 + INTRO_MS;   /* on time */
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void layout(box b)
{
    cue_view v = {.tw = CRM_SIZE, .th = CRM_SIZE};
    cue_fit(b, OUT, OUT, 0, ui.flipped ? 2 : 0, &v, &Gg);
    V = v;
    if (made_s != (int)(V.s * 8) || made_rot != V.rot) {
        made_s = (int)(V.s * 8);
        made_rot = V.rot;
        int n = (int)lroundf(OUT * V.s);
        uint8_t *src = malloc((size_t)n * n * 4);
        if (src) {
            paint_board(V.s, n, src);
            uint8_t *out = ui_buf(n, n);
            cue_turn_pic(src, n, n, V.rot, out);
            ui_free(&t_board);
            t_board = ui_tex(n, n, out);
            free(src);
        }
    }
    int d = (int)ceilf(2 * CRM_COIN_R * V.s);
    if (made_d != d) {
        made_d = d;
        for (int k = 0; k < 3; k++) {
            ui_free(&t_coin[k]);
            paint_disc(k, d, ui_buf(d, d));
            t_coin[k] = ui_tex(d, d, ui_buf(d, d));
        }
        int ds = (int)ceilf(2 * CRM_STRIKER_R * V.s);
        ui_free(&t_striker);
        paint_disc(3, ds, ui_buf(ds, ds));
        t_striker = ui_tex(ds, ds, ui_buf(ds, ds));
    }
}

static void disc_at(int i, float x, float y, float k, float alpha)
{
    float sx, sy, d = 2 * (i == CRM_STRIKER ? CRM_STRIKER_R : CRM_COIN_R) * V.s * k;
    cue_to_screen(&V, x, y, &sx, &sy);
    SDL_Texture *t = i == CRM_STRIKER ? t_striker : t_coin[i == CRM_QUEEN ? 2 : crm_colour(i)];
    ui_shadow_flat(sx, sy, d, d, 0.9f * alpha);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    ui_put_c(t, sx, sy, d, d);
}

static int human_turn(void) { return !(ui_bot_seats() >> P()->turn & 1); }

static int good_first(int j) { (void)j; return 1; }

static void draw(void)
{
    intro_tick();
    cue_replay_tick(&R, 100);
    const crm_pos *p = P();
    ui_put_c(t_board, V.cx, V.cy, OUT * V.s, OUT * V.s);
    if (R.on) {
        for (int i = 0; i < R.w.n; i++) {
            const cue_ball *b = &R.w.b[i];
            float x, y, k;
            if (b->down < 0) disc_at(i, b->x, b->y, 1, 1);
            else if (cue_replay_falling(&R, i, T.hx[b->down], T.hy[b->down], &x, &y, &k)) disc_at(i, x, y, k, k);
        }
        return;
    }
    for (int i = 1; i < p->w.n; i++)
        if (p->w.b[i].down < 0) disc_at(i, p->w.b[i].x, p->w.b[i].y, 1, 1);
    if (p->status != CRM_PLAY) return;
    int pos = A.pos;
    float ang = A.ang;
    if (intro.on) {
        float f = ui_ease(clampf((ui_ms() - intro.t0) / (INTRO_MS * 0.5f), 0, 1)), dx, dy;
        pos = (int)lroundf(intro.from + (intro.pos - intro.from) * f);
        cue_dir(intro.ang, &dx, &dy);
        ang = atan2f(dy, dx);
    }
    cue_world w = p->w;
    crm_spot(p, p->turn, pos, &w.b[0].x, &w.b[0].y);
    w.b[0].down = -1;
    float sx, sy;
    cue_to_screen(&V, w.b[0].x, w.b[0].y, &sx, &sy);
    int free_spot = crm_spot_free(p, p->turn, pos);
    if (!free_spot) ui_mark_glow(sx, sy, 2.6f * CRM_STRIKER_R * V.s);
    int show = (human_turn() && A.mode != F_SLIDE && A.mode != F_PENDING) ||
               (intro.on && ui_ms() - intro.t0 > INTRO_MS / 2);
    if (show) cue_aim_draw(&V, &T, &w, 0, cosf(ang), sinf(ang), good_first);
    if (A.mode == F_PULL && A.pull > 0) {       /* the finger's pull, a faint line behind the striker */
        float bx = w.b[0].x - cosf(ang) * (CRM_STRIKER_R + A.pull * PULL_FULL),
              by = w.b[0].y - sinf(ang) * (CRM_STRIKER_R + A.pull * PULL_FULL);
        cue_dots(&V, w.b[0].x, w.b[0].y, bx, by, 1.4f, fmaxf(3, 0.5f * V.s), 0.45f);
    }
    if (human_turn() && !intro.on) ui_mark_gold_a(sx, sy, 2.7f * CRM_STRIKER_R * V.s, 0.35f + 0.15f * sinf(ui_ms() / 300.0f));
    disc_at(0, w.b[0].x, w.b[0].y, A.mode == F_SLIDE ? 1.08f : 1, 1);
    if (human_turn() && !intro.on) {
        cue_gauge_draw(&Gg, A.mode == F_PULL ? A.pull : A.power, A.mode == F_NONE || A.mode == F_GAUGE);
        ui_animate();
    }
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
/* a board point in the shooter's own frame (their baseline along the bottom) */
static void local(float x, float y, float *lx, float *ly)
{
    float tx, ty;
    cue_from_screen(&V, x, y, &tx, &ty);
    crm_turn_pt((4 - side_now()) & 3, tx, ty, lx, ly);
}

static int pos_at(float lx) { return (int)lroundf(clampf((lx - CRM_SIZE / 2) / CRM_BASE_HALF, -1, 1) * 1000); }

static void fire(float power)
{
    int pow = (int)lroundf(clampf(power, 0.01f, 1) * 1000), ang = cue_angle(cosf(A.ang), sinf(A.ang));
    if (!shoot(A.pos, ang, pow, 1)) ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    const crm_pos *p = P();
    if (busy() || p->status != CRM_PLAY) return;
    A.x0 = x;
    A.y0 = y;
    A.moved = 0;
    if (cue_gauge_hit(&Gg, x, y)) {
        A.mode = F_GAUGE;
        A.power = cue_gauge_at(&Gg, x, y);
        return;
    }
    float lx, ly, sx, sy, slack = 0.4f * ui.unit / V.s;
    local(x, y, &lx, &ly);
    crm_spot(p, p->turn, A.pos, &sx, &sy);
    float bx, by;                               /* the striker in the shooter's frame */
    crm_turn_pt((4 - side_now()) & 3, sx, sy, &bx, &by);
    float d = hypotf(lx - bx, ly - by);
    if (d < CRM_STRIKER_R + slack) { A.mode = F_PENDING; return; }
    if (fabsf(ly - CRM_BASE) < 2.6f + slack && fabsf(lx - CRM_SIZE / 2) < CRM_BASE_HALF + 4) {
        A.mode = F_SLIDE;
        A.pos = pos_at(lx);
        return;
    }
    if (d < 4 * CRM_STRIKER_R + slack) { A.mode = F_PULL; A.pull = 0; return; }
    if (lx < -FRAME || ly < -FRAME || lx > CRM_SIZE + FRAME || ly > CRM_SIZE + FRAME) { A.mode = F_NONE; return; }
    A.mode = F_AIM;
    float tx, ty;
    cue_from_screen(&V, x, y, &tx, &ty);
    A.ang = atan2f(ty - sy, tx - sx);
}

static void motion(float x, float y)
{
    const crm_pos *p = P();
    if (A.mode == F_NONE) return;
    float lx, ly, tx, ty, sx, sy;
    local(x, y, &lx, &ly);
    cue_from_screen(&V, x, y, &tx, &ty);
    crm_spot(p, p->turn, A.pos, &sx, &sy);
    if (fabsf(x - A.x0) + fabsf(y - A.y0) > 0.18f * ui.unit) A.moved = 1;
    if (A.mode == F_PENDING && A.moved) {       /* along the baseline: it slides; else a pull */
        float l0x, l0y;
        local(A.x0, A.y0, &l0x, &l0y);
        A.mode = fabsf(lx - l0x) > 1.4f * fabsf(ly - l0y) ? F_SLIDE : F_PULL;
        A.pull = 0;
    }
    switch (A.mode) {
    case F_GAUGE: A.power = cue_gauge_at(&Gg, x, y); break;
    case F_AIM: A.ang = atan2f(ty - sy, tx - sx); break;
    case F_SLIDE: A.pos = pos_at(lx); break;
    case F_PULL: {
        float dx = sx - tx, dy = sy - ty, l = hypotf(dx, dy);
        if (l > CRM_STRIKER_R) {
            A.ang = atan2f(dy, dx);
            A.pull = clampf((l - CRM_STRIKER_R) / PULL_FULL, 0, 1);
        } else A.pull = 0;
        break;
    }
    }
    ui_animate();
}

static void release(float x, float y)
{
    motion(x, y);
    int m = A.mode;
    A.mode = F_NONE;
    switch (m) {
    case F_GAUGE:
        if (A.power >= 0.02f) fire(A.power);
        break;
    case F_PULL:
        if (A.pull >= 0.03f) { A.power = A.pull; fire(A.pull); }
        break;
    case F_SLIDE:
        if (!crm_spot_free(P(), P()->turn, A.pos)) { ui_sound(SND_DENY); A.pos = crm_free_pos(P(), P()->turn, A.pos); }
        else ui_sound(SND_PLACE);
        break;
    }
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the side's coins pocketed, and the queen once covered (pocketed and waiting for its cover: faded) */
static void bar(int player, box b)
{
    const crm_pos *p = P();
    int d = (int)(0.6f * ui.unit), team = crm_team(p, player);
    if (d != made_small) {
        made_small = d;
        for (int k = 0; k < 3; k++) {
            ui_free(&t_small[k]);
            paint_disc(k, d, ui_buf(d, d));
            t_small[k] = ui_tex(d, d, ui_buf(d, d));
        }
    }
    float x = ui_bar_number(b, t_small[team], crm_pocketed(p, team));
    if (p->queen != CRM_Q_BOARD && p->queen_team == team) {
        float a = p->queen == CRM_Q_COVERED ? 1 : 0.45f;
        SDL_SetTextureAlphaMod(t_small[2], (Uint8)(255 * a));
        ui_put_c(t_small[2], x + 0.6f * ui.unit, b.y + b.h / 2, d, d);
        SDL_SetTextureAlphaMod(t_small[2], 255);
    }
}

static void token(int player, int size, uint8_t *rgba)
{
    int team = P()->players == 4 ? player & 1 : player;
    paint_disc(team, size, rgba);
}

/* the menu's card: a corner of the board, its hole, a baseline's circle, coins, the queen and the striker */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    float s = size / 52.0f;
    int n = (int)(OUT * s);
    uint8_t *b = malloc((size_t)n * n * 4);
    if (!b) return;
    paint_board(s, n, b);
    int ox = n - size, oy = n - size;           /* the bottom right corner */
    for (int y = 0; y < size; y++)
        memcpy(rgba + 4 * (size_t)y * size, b + 4 * ((size_t)(y + oy) * n + ox), 4 * (size_t)size);
    free(b);
    int d = (int)(2 * CRM_COIN_R * s * 1.15f), ds = (int)(2 * CRM_STRIKER_R * s * 1.15f);
    uint8_t *c = malloc((size_t)ds * ds * 4);
    if (!c) return;
    static const float C[][3] = {{70, 70, 0}, {76.5f, 73.5f, 1}, {66, 62, 2}, {78, 64, 0}, {89, 90, 1}, {71, 77.5f, 0}};
    float bx = FRAME * s - ox, by = FRAME * s - oy;
    for (unsigned k = 0; k < sizeof C / sizeof C[0]; k++) {
        paint_disc((int)C[k][2], d, c);
        art_flat_put(rgba, size, size, c, d, bx + C[k][0] * s, by + C[k][1] * s);
    }
    paint_disc(3, ds, c);
    art_flat_put(rgba, size, size, c, ds, bx + 58 * s, by + CRM_BASE * s);
    free(c);
}

static const game carrom_game = {
    .id = "carrom", .order = 481, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .fill = 1,
    .players = 1u << 2 | 1u << 4, .home = home, .winners = winners,
};
REGISTER_GAME(carrom_game)
