/*
 * Marble Push in RetroStone Board: the hexagon of 61 holes carved into the plank, the glossy marbles, a tray in two
 * corners of the plank for the marbles each player pushed off (the rules: marblepush_rules.c, marblepush.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The hexagon lies with its rows across the board's square, player 0's side at the bottom: a hole's centre is
 * (diagonal - row / 2 - 2, (row - 4) * H) hole spacings from the middle hole, y up (turned half round when flipped).
 * The plank's corners left free by the hexagon hold the trays: six cups in a triangle, each player's at their right
 * hand (bottom right for the bottom player), for the six marbles they must push off; the picture is the same turned
 * half round, so one picture serves both ways.
 *
 * The input: tap one of your marbles to pick it, tap a second or a third in the same row (next to it, or one further
 * with yours between) to pick the row; or drag across them. Each way the picked marbles can go shows a dot on one hole
 * (in line: the hole ahead; sideways: the hole beside the row's leading end), or a ring round the marble a push would
 * move: tap it (the move's picture shows while the finger is down) and the marbles slide; a marble pushed off slides
 * over the edge and drops into its pusher's tray. A tap on a picked marble at an end of the row lets it go (on the
 * middle one: keeps it alone).
 */
#include <math.h>
#include <string.h>

#include "games/marblepush.h"
#include "shell.h"

#define FRAME 0.035f                            /* the board's frame, a part of its side */
#define H 0.8660254f                            /* from one row of holes to the next, in spacings */
#define EDGE 0.62f                              /* the hexagon's carved outline, this far beyond its outer holes */
#define FILL 0.95f                              /* how much of the plank's width the hexagon takes */
#define HOLE 0.2f                               /* a hole's radius (spacings) */
#define CUP 0.27f                               /* a tray cup's radius */
#define MARBLE 0.88f                            /* a marble's width (spacings) */
#define TRAY 0.66f                              /* a marble's width in its tray */
#define PITCH 0.74f                             /* from one cup to the next */
#define MARGIN 0.5f                             /* the tray's corner cup from the plank's edges */
#define SLIDE_MS 190
#define FLY_MS 430
#define GHOST_A 0.5f

static mpgame G;
static int q, fr, made_w = -1, made_small = -1;
static float bx, by, cx, cy, s;                 /* the board's corner, the hexagon's middle, the spacing (pixels) */
static SDL_Texture *t_board, *t_marble[2], *t_tray[2], *t_small[2];
static int8_t sel[3];
static int nsel;
static mpmove opts[MP_MAX_MOVES];               /* the picked marbles' moves, and the hole to tap for each */
static int8_t spot[MP_MAX_MOVES];
static int nopts;
enum { P_PICK, P_HELD, P_SPOT };                /* a press on a marble to pick, on a picked one, on a move's spot */
static struct { int on, mode, anchor, from, moved, target; float x, y; } ptr;
static struct {
    int on, n, off, cup;                        /* off: the last one moved leaves the board, for its cup */
    int8_t from[5], to[5];
    uint8_t who[5];
    mppos before;
    Uint32 t0;
} anim;

static int colour(int player) { return !player; } /* player 0 ebony: the house's second colour */

/* ---- the position ------------------------------------------------------------------------------------------------ */
static const mppos *now(void) { return mp_pos(&G); }

static void pick(const int8_t *cells, int n)
{
    nsel = n;
    if (n) memcpy(sel, cells, (size_t)n);
    nopts = n ? mp_moves_of(now(), sel, n, opts) : 0;
    for (int i = 0; i < nopts; i++) {           /* each move's own hole: no two moves share one */
        const mpmove *m = &opts[i];
        int d = m->dir, ax = -1;
        if (m->n > 1)
            for (int k = 0; k < 6; k++)
                if (mp_next(m->cell[0], k) == m->cell[1]) ax = k;
        if (m->n == 1 || d == ax || d == (ax + 3) % 6) spot[i] = (int8_t)mp_next(m->cell[m->n - 1], d);   /* ahead */
        else {                                  /* sideways: beside the end of the row on the side it leans to */
            int lean = (d - ax + 6) % 6;        /* 1 or 5: towards cell[n - 1]'s end; 2 or 4: towards cell[0]'s */
            spot[i] = (int8_t)mp_next(m->cell[lean == 1 || lean == 5 ? m->n - 1 : 0], d);
        }
    }
}

static void calm(void)
{
    pick(NULL, 0);
    ptr.on = 0;
    anim.on = 0;
}

static void start(int variant)
{
    mp_new(&G, variant);
    calm();
}

static int status(void)
{
    switch (mp_status(&G)) {
    case MP_PLAY: return GS_PLAY;
    case MP_WIN0: return GS_WIN0;
    case MP_WIN1: return GS_WIN1;
    default: return GS_DRAW;
    }
}

static int turn(void) { return now()->side; }

static int picked(int h)
{
    for (int i = 0; i < nsel; i++)
        if (sel[i] == h) return 1;
    return 0;
}

/* the move whose hole is h: its own spot, else the only move that puts a marble there or pushes the one there */
static int move_at(int h)
{
    if (h < 0 || now()->cell[h] == now()->side + 1) return -1;
    for (int i = 0; i < nopts; i++)
        if (spot[i] == h) return i;
    int found = -1;
    for (int i = 0; i < nopts; i++) {
        int8_t from[5], to[5];
        int hit = 0;
        for (int k = 0, n = mp_moving(&opts[i], from, to); k < n; k++)
            hit |= to[k] == h || (k >= opts[i].n && from[k] == h);
        if (!hit) continue;
        if (found >= 0) return -1;              /* two moves: not this hole */
        found = i;
    }
    return found;
}

/* a row of the player's own marbles from a to b (at most 3), into cells; its length, 0: none */
static int own_line(int a, int b, int8_t cells[3])
{
    int n = mp_line(a, b, cells);
    for (int i = 0; i < n; i++)
        if (now()->cell[cells[i]] != now()->side + 1) return 0;
    return n;
}

/* the picked row with h added (h one of the player's marbles): 1 and the new row, or 0 */
static int extend(int h, int8_t cells[3], int *n)
{
    int8_t all[3];
    if (nsel < 1 || nsel > 2) return 0;         /* three picked: no room for a fourth */
    memcpy(all, sel, (size_t)nsel);
    all[nsel] = (int8_t)h;
    for (int a = 0; a <= nsel; a++)             /* two of them as the ends of a row holding them all */
        for (int b = 0; b <= nsel; b++) {
            int k = own_line(all[a], all[b], cells), ok = k > nsel;
            for (int i = 0; i <= nsel && ok; i++) {
                int in = 0;
                for (int j = 0; j < k; j++) in |= cells[j] == all[i];
                ok = in;
            }
            if (ok) { *n = k; return 1; }
        }
    return 0;
}

static void play(const mpmove *m)
{
    mppos before = *now();
    if (!mp_play(&G, m)) { ui_sound(SND_DENY); return; }
    const mpmove *done = &G.move[G.ply - 1];
    anim.before = before;
    anim.n = mp_moving(done, anim.from, anim.to);
    for (int i = 0; i < anim.n; i++) anim.who[i] = (uint8_t)(before.cell[anim.from[i]] - 1);
    anim.off = done->off;
    anim.cup = before.lost[!before.side];
    anim.t0 = ui_ms();
    anim.on = 1;
    pick(NULL, 0);
    ui_sound(SND_MOVE);
    if (anim.off) ui_sound_later(SND_CAPTURE, SLIDE_MS + FLY_MS);
    ui_animate();
}

static int undo(void)
{
    calm();
    return mp_undo(&G);
}

static int script(const char *moves)
{
    calm();
    return mp_script(&G, moves);
}

/* the bot's side (marblepush_bot.c): the game to copy, and its move ("c3c5:ne", the two ends of the marbles and the
 * direction) played as taps play it */
const mpgame *marblepush_live(void) { return &G; }

int marblepush_show(const char *move)
{
    static const char *const DIR[6] = {"e", "ne", "nw", "w", "sw", "se"};
    int h[3], k = 0, len, dir = -1;
    while (k < 3 && (h[k] = mp_hole(move, &len)) >= 0) { k++; move += len; }
    if (!k || *move++ != ':') return 0;
    for (int d = 0; d < 6; d++)
        if (!strcmp(move, DIR[d])) dir = d;
    mpmove m = {.dir = (int8_t)dir}, all[MP_MAX_MOVES];
    m.n = (int8_t)mp_line(h[0], h[k - 1], m.cell);
    if (dir < 0 || !m.n || mp_status(&G) != MP_PLAY) return 0;
    for (int i = 0, n = mp_legal(now(), all); i < n; i++)
        if (mp_same(&all[i], &m)) {
            ptr.on = 0;
            play(&all[i]);
            return 1;
        }
    return 0;
}

/* the marbles player p has pushed off, as shown: the one still on its way to the tray not yet */
static int shown_taken(int p)
{
    return now()->lost[!p] - (anim.on && anim.off && anim.before.side == p);
}

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
/* a hole's place (row r, diagonal d, on the board or beyond it) in spacings from the middle, y up */
static void plane(int r, int d, float *x, float *y)
{
    *x = d - r / 2.0f - 2;
    *y = (r - 4) * H;
}

/* the cup i (0..5) of player p's tray (the plank's half side hs, spacings): bottom right for player 0, turned half
 * round for player 1. Three cups along the bottom from the corner, two above them, one on top. */
static void cup(int p, int i, float hs, float *x, float *y)
{
    static const float K[6][2] = {{0, 0}, {1, 0}, {2, 0}, {0.5f, 1}, {1.5f, 1}, {1, 2}};
    *x = hs - MARGIN - K[i][0] * PITCH;
    *y = -hs + MARGIN + K[i][1] * PITCH * H;
    if (p) { *x = -*x; *y = -*y; }
}

/* the spacing that fits the hexagon and its outline across a plank of inner side `inner` */
static float spacing(float inner) { return FILL * inner / (2 * (4 + EDGE)); }

static void to_screen(float px, float py, float *x, float *y)
{
    float k = ui.flipped ? -s : s;
    *x = cx + k * px;
    *y = cy - k * py;
}

static void hole_xy(int h, float *x, float *y)
{
    float px, py;
    plane(mp_row(h), mp_diag(h), &px, &py);
    to_screen(px, py, x, y);
}

static void cup_xy(int p, int i, float *x, float *y)
{
    float px, py;
    cup(p, i, 4.5f * q / s, &px, &py);
    to_screen(px, py, x, y);
}

/* the hole under (x, y), or -1 (a little room round each hole) */
static int xy_hole(float x, float y)
{
    int best = -1;
    float bd = 0.6f * s;
    for (int h = 0; h < MP_HOLES; h++) {
        float hx, hy;
        hole_xy(h, &hx, &hy);
        float d = hypotf(x - hx, y - hy);
        if (d < bd) bd = d, best = h;
    }
    return best;
}

/* ---- the board's picture ----------------------------------------------------------------------------------------- */
/* the hexagon on the plank o (w pixels wide, its frame f, the spacing sp): the playing field a shade darker, its carved
 * outline, the holes, the two trays' cups */
static void paint_field(uint8_t *o, int w, int f, float sp)
{
    static const float INK[3] = {34, 24, 16};
    float mid = w / 2.0f, lw = 0.9f + 0.03f * sp, hs = (w - 2 * f) / 2.0f / sp, cups[12][2];
    for (int i = 0; i < 12; i++) cup(i / 6, i % 6, hs, &cups[i][0], &cups[i][1]);
    for (int py = f; py < w - f; py++)
        for (int px = f; px < w - f; px++) {
            float x = (px + 0.5f - mid) / sp, y = (mid - py - 0.5f) / sp, ax = fabsf(x), ay = fabsf(y);
            float out = fmaxf(ay, H * ax + 0.5f * ay) - 4 * H;   /* beyond the outer holes' lines (spacings) */
            float k = 1 - 0.1f * clampf((EDGE - out) * sp + 0.5f, 0, 1);
            int r = (int)lroundf(y / H + 4), d = (int)lroundf(x + r / 2.0f + 2);
            if (mp_at(r, d) >= 0) {
                float hx, hy;
                plane(r, d, &hx, &hy);
                k *= art_recess((x - hx) * sp, (hy - y) * sp, HOLE * sp, 0.08f * sp, 0.4f, 0.24f, 0.22f);
            }
            for (int i = 0; i < 12; i++)
                if (fabsf(x - cups[i][0]) < 0.5f && fabsf(y - cups[i][1]) < 0.5f)
                    k *= art_recess((x - cups[i][0]) * sp, (cups[i][1] - y) * sp, CUP * sp, 0.07f * sp, 0.4f, 0.24f,
                                    0.22f);
            float ink = 0.8f * clampf(lw / 2 - fabsf(out - EDGE) * sp + 0.5f, 0, 1);
            uint8_t *p = o + 4 * (py * w + px);
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)clampf(p[j] * k * (1 - ink) + INK[j] * ink + 0.5f, 0, 255);
        }
}

/* a glossy marble of colour c (0 ivory, 1 ebony), d x d: a lit sphere (the light up to the left), a sharp highlight
 * and a soft one, a thin darker rim, its soft shadow down to the right; each pixel sampled 3 x 3 */
static void paint_marble(int c, int d, uint8_t *o)
{
    static const float BASE[2][3] = {{240, 230, 208}, {58, 52, 49}}, RIM[2] = {0.5f, 0.7f};
    static const float AMB[2] = {0.52f, 0.32f}, DIF[2] = {0.58f, 0.85f};
    float R = 0.45f * d, mx = 0.475f * d, my = 0.47f * d, L[3] = {-0.45f, -0.6f, 0.66f}, HV[3] = {-0.45f, -0.6f, 1.66f};
    float ln = sqrtf(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]), hn = sqrtf(HV[0] * HV[0] + HV[1] * HV[1] + HV[2] * HV[2]);
    for (int i = 0; i < 3; i++) L[i] /= ln, HV[i] /= hn;
    for (int py = 0; py < d; py++)
        for (int px = 0; px < d; px++) {
            float col[3] = {0, 0, 0}, cov = 0;
            for (int sy = 0; sy < 3; sy++)
                for (int sx = 0; sx < 3; sx++) {
                    float x = (px + (sx + 0.5f) / 3 - mx) / R, y = (py + (sy + 0.5f) / 3 - my) / R, r2 = x * x + y * y;
                    if (r2 >= 1) continue;
                    float z = sqrtf(1 - r2), nl = x * L[0] + y * L[1] + z * L[2], nh = x * HV[0] + y * HV[1] + z * HV[2];
                    float k = AMB[c] + DIF[c] * fmaxf(0, nl) + 0.1f * fmaxf(0, y);   /* the light, and the table's below */
                    float sp = 0.9f * powf(fmaxf(0, nh), 70) + 0.14f * powf(fmaxf(0, nh), 6);
                    float rim = clampf((sqrtf(r2) - 1) * R / 1.6f + 1, 0, 1) * RIM[c];   /* the last 1.6 pixels */
                    for (int j = 0; j < 3; j++) {
                        float v = clampf(BASE[c][j] * k, 0, 255);
                        v += (255 - v) * sp;
                        col[j] += v * (1 - rim) + BASE[c][j] * 0.4f * rim;
                    }
                    cov += 1;
                }
            uint8_t *p = o + 4 * ((size_t)py * d + px);
            float sx = (px + 0.5f - mx - 0.06f * d) / R, sy = (py + 0.5f - my - 0.08f * d) / R;   /* the shadow */
            float sa = 0.42f * clampf((1.04f - sqrtf(sx * sx + sy * sy)) / 0.22f, 0, 1), a = cov / 9;
            float out = a + sa * (1 - a);
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)(out > 0 ? clampf(col[j] / 9 / out, 0, 255) : 0);
            p[3] = (uint8_t)(255 * out + 0.5f);
        }
}

static void paint_board(uint8_t *o, int w, int f)
{
    int qq = (w - 2 * f) / 9;
    art_board_n(9, 9, qq, f, 0, o);
    paint_field(o, w, f, spacing(9.0f * qq));
}

static void layout(box b)
{
    q = (int)(b.w * (1 - 2 * FRAME) / 9);
    fr = (int)((b.w - 9 * q) / 2);
    int w = 9 * q + 2 * fr;
    bx = floorf(b.x + (b.w - w) / 2);
    by = floorf(b.y + (b.h - w) / 2);
    cx = bx + w / 2.0f;
    cy = by + w / 2.0f;
    s = spacing(9.0f * q);
    if (made_w != w) {                          /* the same picture flipped or not */
        made_w = w;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr);
        t_board = ui_tex(w, w, ui_buf(w, w));
        int d = (int)lroundf(MARBLE * s), t = (int)lroundf(TRAY * s);
        for (int c = 0; c < 2; c++) {
            ui_free(&t_marble[c]);
            paint_marble(c, d, ui_buf(d, d));
            t_marble[c] = ui_tex(d, d, ui_buf(d, d));
            ui_free(&t_tray[c]);
            paint_marble(c, t, ui_buf(t, t));
            t_tray[c] = ui_tex(t, t, ui_buf(t, t));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_small[c]);
            paint_marble(c, small, ui_buf(small, small));
            t_small[c] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void marble(int player, float x, float y, float d, float alpha)
{
    SDL_Texture *t = d < 0.8f * MARBLE * s ? t_tray[colour(player)] : t_marble[colour(player)];
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(alpha, 0, 1) + 0.5f));
    ui_put_c(t, x, y, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

/* where the animated marble i is at time t (ms since the move): on the board, over the edge, or flying to its cup;
 * lift 0..1 (how high above the wood) */
static void anim_at(int i, float t, float *x, float *y, float *lift)
{
    float f = clampf(t / SLIDE_MS, 0, 1), x0, y0, x1, y1;
    f = f * f * (3 - 2 * f);
    *lift = 0;
    hole_xy(anim.from[i], &x0, &y0);
    if (anim.to[i] >= 0) hole_xy(anim.to[i], &x1, &y1);
    else {                                      /* over the edge: one step on, the step from the marble behind it */
        float fx, fy, bx0, by0;
        plane(mp_row(anim.from[i]), mp_diag(anim.from[i]), &fx, &fy);
        plane(mp_row(anim.from[i - 1]), mp_diag(anim.from[i - 1]), &bx0, &by0);
        to_screen(2 * fx - bx0, 2 * fy - by0, &x1, &y1);
    }
    *x = x0 + (x1 - x0) * f;
    *y = y0 + (y1 - y0) * f;
    if (anim.to[i] >= 0 || t <= SLIDE_MS) return;
    float g = clampf((t - SLIDE_MS) / FLY_MS, 0, 1), cx1, cy1;   /* off: up, over and down into the tray */
    cup_xy(anim.before.side, anim.cup, &cx1, &cy1);
    float e = g * g * (3 - 2 * g);
    *x = x1 + (cx1 - x1) * e;
    *y = y1 + (cy1 - y1) * e;
    *lift = sinf(3.14159265f * g);
}

static void draw(void)
{
    const mppos *p = now();
    float w = 9.0f * q + 2 * fr, d = MARBLE * s, td = TRAY * s, x, y;
    ui_put(t_board, bx, by, w, w);
    float t = anim.on ? (float)(ui_ms() - anim.t0) : 0;
    if (anim.on && t >= SLIDE_MS + (anim.off ? FLY_MS : 0)) anim.on = 0;
    if (anim.on) ui_animate();
    const mppos *shown = anim.on ? &anim.before : p;   /* the marbles standing still: the position before the move */
    if (G.ply && !anim.on) {                    /* the last move: where its marbles left, and where they went */
        int8_t from[5], to[5];
        const mpmove *m = &G.move[G.ply - 1];
        mp_moving(m, from, to);
        for (int i = 0; i < m->n; i++) {
            hole_xy(from[i], &x, &y);
            ui_mark_last_round(x, y, 0.98f * s);
            hole_xy(to[i], &x, &y);
            ui_mark_last_round(x, y, 0.98f * s);
        }
    }
    for (int i = 0; i < nsel; i++) {
        hole_xy(sel[i], &x, &y);
        ui_mark_select_round(x, y, 1.0f * s);
    }
    int still[MP_HOLES];
    for (int h = 0; h < MP_HOLES; h++) still[h] = shown->cell[h];
    if (anim.on)
        for (int i = 0; i < anim.n; i++) still[anim.from[i]] = 0;
    for (int h = 0; h < MP_HOLES; h++)          /* the shadows first: they fall on the wood only */
        if (still[h]) {
            hole_xy(h, &x, &y);
            ui_shadow_flat(x, y, d, d, 1);
        }
    for (int pl = 0; pl < 2; pl++)
        for (int i = 0; i < shown_taken(pl); i++) {
            cup_xy(pl, i, &x, &y);
            ui_shadow_flat(x, y, td, td, 1);
        }
    for (int h = 0; h < MP_HOLES; h++)
        if (still[h]) {
            hole_xy(h, &x, &y);
            marble(still[h] - 1, x, y, d, 1);
        }
    for (int pl = 0; pl < 2; pl++)              /* each tray: the opponent's marbles this player pushed off */
        for (int i = 0; i < shown_taken(pl); i++) {
            cup_xy(pl, i, &x, &y);
            marble(!pl, x, y, td, 1);
        }
    int st = status();
    if ((st == GS_WIN0 || st == GS_WIN1) && !anim.on)   /* the winner's full tray in gold */
        for (int i = 0; i < MP_WIN; i++) {
            cup_xy(st == GS_WIN1, i, &x, &y);
            ui_mark_gold(x, y, 1.2f * td);
        }
    if (anim.on)                                /* the marbles moving: sliding, the one pushed off flying */
        for (int i = 0; i < anim.n; i++) {
            float lift, k;
            anim_at(i, t, &x, &y, &lift);
            k = anim.to[i] >= 0 ? d : d + (td - d) * clampf((t - SLIDE_MS) / FLY_MS, 0, 1);
            ui_shadow_flat(x, y, k, k, 1 - 0.5f * lift);
            k *= 1 + 0.2f * lift;
            marble(anim.who[i], x, y - 0.3f * k * lift, k, 1);
        }
    if (st != GS_PLAY || anim.on) return;
    for (int i = 0; i < nopts; i++) {           /* the picked marbles' moves: a dot, or a ring round a marble pushed */
        hole_xy(spot[i], &x, &y);
        if (p->cell[spot[i]]) ui_mark_ring(x, y, 1.1f * s);
        else ui_mark_dot(x, y, 0.56f * s);
    }
    if (ptr.on && ptr.target >= 0) {            /* the finger on a move: where the marbles would go */
        int8_t from[5], to[5];
        const mpmove *m = &opts[ptr.target];
        for (int k = 0, n = mp_moving(m, from, to); k < n; k++) {
            if (to[k] < 0) continue;
            hole_xy(to[k], &x, &y);
            marble(p->cell[from[k]] - 1, x, y, d, GHOST_A);
        }
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    const mppos *p = now();
    int h = xy_hole(x, y), n, i;
    int8_t cells[3];
    memset(&ptr, 0, sizeof ptr);
    ptr.on = 1;
    ptr.from = h;
    ptr.target = -1;
    ptr.x = x;
    ptr.y = y;
    if (h >= 0 && p->cell[h] == p->side + 1) {  /* one's own marble: pick it, or add it to the row */
        if (picked(h)) { ptr.mode = P_HELD; return; }
        ptr.mode = P_PICK;
        if (nsel && extend(h, cells, &n)) {
            pick(cells, n);
            ptr.anchor = cells[0] == h ? cells[n - 1] : cells[0];
        } else {
            cells[0] = (int8_t)h;
            pick(cells, 1);
            ptr.anchor = h;
        }
        ui_animate();
        return;
    }
    if ((i = move_at(h)) >= 0) {                /* a move's hole: shown while the finger is down, played on release */
        ptr.mode = P_SPOT;
        ptr.target = i;
        ui_animate();
        return;
    }
    if (nsel || (h >= 0 && p->cell[h])) ui_sound(SND_DENY);
    pick(NULL, 0);
    ptr.on = 0;
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    if (hypotf(x - ptr.x, y - ptr.y) > 0.25f * s) ptr.moved = 1;
    int h = xy_hole(x, y), n;
    int8_t cells[3];
    if (ptr.mode == P_PICK && h >= 0 && (n = own_line(ptr.anchor, h, cells)) > 0 && (n != nsel || !picked(h)))
        pick(cells, n);                         /* dragging across one's marbles: the row from the first */
    ptr.target = move_at(h);
    ui_animate();
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    int h = xy_hole(x, y);
    if (ptr.target >= 0 && move_at(h) == ptr.target) {
        mpmove m = opts[ptr.target];
        play(&m);
        return;
    }
    if (ptr.mode == P_HELD && !ptr.moved && h == ptr.from) {   /* a tap on a picked marble lets it go */
        int8_t cells[3];
        int n = 0;
        if (nsel == 3 && sel[1] == h) cells[n++] = (int8_t)h;   /* the middle one: it alone */
        if (nsel > 1 && (sel[0] == h || sel[nsel - 1] == h))
            for (int i = 0; i < nsel; i++)
                if (sel[i] != h) cells[n++] = sel[i];
        pick(cells, n);
        return;
    }
    if (ptr.moved && h != ptr.from && ptr.mode != P_PICK) ui_sound(SND_DENY);
    ui_animate();
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the marbles the player pushed off: a marble of the other colour, how many */
static void bar(int player, box b) { ui_bar_count(b, t_small[colour(!player)], shown_taken(player)); }

static void token(int player, int size, uint8_t *rgba) { paint_marble(colour(player), size, rgba); }

/* a marble painted on a card at (x, y) pixels, d across (its shadow with it: the shadows' pass paints nothing) */
static void thumb_marble(uint8_t *rgba, int size, int c, float x, float y, int d, int shadow)
{
    if (shadow) return;
    uint8_t *one = ui_buf(d, d);
    paint_marble(c, d, one);
    art_blend(rgba, size, size, one, d, d, (int)lroundf(x - d / 2.0f), (int)lroundf(y - d / 2.0f));
}

/* the menu's card: a game under way, a marble in each tray; the picker's: each starting layout */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const char *const MID[2] = {"a3 a4 b2 b3 b4 b5 c3 c4 c5 d4 d5 e5 e6",
                                       "i6 i7 i8 h5 h6 h7 h8 g5 g6 g7 g8 f6 f7"};
    mppos p;
    mp_start(&p, variant < 0 ? MP_STANDARD : variant);
    if (variant < 0) {
        memset(p.cell, 0, sizeof p.cell);
        for (int c = 0; c < 2; c++)
            for (const char *t = MID[c]; *t;) {
                int len, h = mp_hole(t, &len);
                if (h < 0) break;
                p.cell[h] = (uint8_t)(c + 1);
                t += len;
                while (*t == ' ') t++;
            }
        p.lost[0] = p.lost[1] = 1;
    }
    int f = (int)(size * 0.04f), qq = (size - 2 * f) / 9, w = 9 * qq + 2 * f, o = (size - w) / 2;
    float sp = spacing(9.0f * qq), mid = o + w / 2.0f, hs = 4.5f * qq / sp;
    uint8_t *board = ui_buf(w, w);
    paint_board(board, w, f);
    art_blend(rgba, size, size, board, w, w, o, o);
    int d = (int)lroundf(MARBLE * sp), td = (int)lroundf(TRAY * sp);
    for (int pass = 1; pass >= 0; pass--) {     /* the shadows, then the marbles */
        for (int h = 0; h < MP_HOLES; h++) {
            if (!p.cell[h]) continue;
            float x, y;
            plane(mp_row(h), mp_diag(h), &x, &y);
            thumb_marble(rgba, size, colour(p.cell[h] - 1), mid + x * sp, mid - y * sp, d, pass);
        }
        for (int pl = 0; pl < 2; pl++)
            for (int i = 0; i < p.lost[!pl]; i++) {
                float x, y;
                cup(pl, i, hs, &x, &y);
                thumb_marble(rgba, size, colour(!pl), mid + x * sp, mid - y * sp, td, pass);
            }
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last marble to land in its tray */

static const game marblepush_game = {
    .id = "marblepush", .order = 95, .variants = MP_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(marblepush_game)
