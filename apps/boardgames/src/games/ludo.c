/*
 * Ludo in RetroStone Board: the cross-shaped board painted on one plank, the pawns, the die (the shell's), tap or
 * drag, the tokens hopping square by square (the rules and the notation: ludo_rules.c, ludo.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 15 x 15 cells. The player at the bottom has the lower left yard and throws in the lower right one (empty);
 * the player at the top, the upper right yard, its die in the upper left one. The track runs clockwise.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"                        /* PAWN: the tokens are chess pawns */
#include "games/ludo.h"
#include "shell.h"

#define N 15                                    /* cells across the board */
#define FRAME 0.55f                             /* the frame, in cells */
#define PAWN_SIZE 1.5f                          /* a pawn's picture, in cells (the pawn stands in its lower middle) */
#define DIE 1.9f                                /* the die, in cells */
#define HOP_MS 125
#define PI_F 3.14159265f

static lgame G;
static int cs, fr, ps, made = -1, made_flip = -1, made_small = -1;
static float bx, by;                            /* the cells' top left corner */
static SDL_Texture *t_board, *t_pawn[2], *t_small[2], *t_die[2][7];
static int fresh;                               /* the shell's die shows the last throw (not after an undo) */
static uint8_t rolls[LUDO_MAX_EVENTS];          /* the game's throws so far, kept through undo: undo never rethrows */
static int nrolls;
static struct { int on, token, moved; float x, y; } drag;
static struct { int on, player, token, from, hops, heard, caught; Uint32 t0; lpos before; } anim;
static int8_t track[LUDO_TRACK][2];             /* the track's cells, from the bottom player's start square */
static const uint8_t INK[3] = {84, 56, 36}, PALE[3] = {244, 226, 192}, TONE[2][3] = {{248, 240, 222}, {112, 84, 64}};
static const float TONE_A[2] = {0.9f, 0.82f};   /* the players' areas: ivory and ebony tones of the wood */

/* ---- the board's geometry, in cells ------------------------------------------------------------------------------ */
static void make_track(void)
{
    static const int8_t run[13][5] = {           /* x, y, dx, dy, squares: clockwise from (6, 13) */
        {6, 13, 0, -1, 5}, {5, 8, -1, 0, 6}, {0, 7, 0, 0, 1}, {0, 6, 1, 0, 6}, {6, 5, 0, -1, 6}, {7, 0, 0, 0, 1},
        {8, 0, 0, 1, 6}, {9, 6, 1, 0, 6}, {14, 7, 0, 0, 1}, {14, 8, -1, 0, 6}, {8, 9, 0, 1, 6}, {7, 14, 0, 0, 1},
        {6, 14, 0, 0, 1}};
    int i = 0;
    for (int r = 0; r < 13; r++)
        for (int k = 0; k < run[r][4]; k++, i++) {
            track[i][0] = (int8_t)(run[r][0] + k * run[r][2]);
            track[i][1] = (int8_t)(run[r][1] + k * run[r][3]);
        }
}

/* the centre of token k of a player at `at` (ludo.h), in cells; bottom: the player at the bottom */
static void place(int player, int k, int at, int bottom, float *u, float *v)
{
    static const float spot[2] = {1.9f, 4.1f};
    float x, y;
    if (at < 0) x = spot[k & 1], y = 9 + spot[k >> 1];
    else if (at < LUDO_COLUMN) x = track[at][0] + 0.5f, y = track[at][1] + 0.5f;
    else if (at < LUDO_HOME) x = 7.5f, y = 13.5f - (at - LUDO_COLUMN);
    else x = 6.75f + 0.5f * k, y = 8.55f;       /* home: side by side in the player's triangle */
    if (player != bottom) x = N - x, y = N - y; /* the other player: the board turned round */
    *u = x;
    *v = y;
}

static int arm(int x, int y) { return x >= 0 && y >= 0 && x < N && y < N && (x >= 6 && x <= 8) != (y >= 6 && y <= 8); }

/* the tokens as drawn: tokens sharing a square stand side by side, a little smaller */
typedef struct { int player, k; float u, v, scale; } tok;

static int gather(const lpos *p, int bottom, int skip_player, int skip_k, tok *out)
{
    int n = 0;
    for (int pl = 0; pl < 2; pl++)
        for (int k = 0; k < LUDO_TOKENS; k++) {
            if (pl == skip_player && k == skip_k) continue;
            out[n] = (tok){pl, k, 0, 0, p->at[pl][k] == LUDO_HOME ? 0.62f : 1};
            place(pl, k, p->at[pl][k], bottom, &out[n].u, &out[n].v);
            n++;
        }
    float du[2 * LUDO_TOKENS] = {0};
    for (int i = 0; i < n; i++) {
        int same = 0, rank = 0;
        for (int j = 0; j < n; j++)
            if (fabsf(out[j].u - out[i].u) < 0.01f && fabsf(out[j].v - out[i].v) < 0.01f) rank += j < i, same++;
        if (same > 1) {
            du[i] = (rank - (same - 1) / 2.0f) * 0.62f / same;
            out[i].scale = 0.8f;
        }
    }
    for (int i = 0; i < n; i++) out[i].u += du[i];
    for (int i = 1; i < n; i++)                 /* back to front */
        for (int j = i; j > 0 && out[j].v < out[j - 1].v; j--) {
            tok t = out[j];
            out[j] = out[j - 1];
            out[j - 1] = t;
        }
    return n;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int variant)
{
    (void)variant;
    ludo_new(&G);
    nrolls = 0;
    fresh = 0;
    anim.on = 0;
    drag.on = 0;
}

static int status(void) { return G.pos.winner < 0 ? GS_PLAY : G.pos.winner ? GS_WIN1 : GS_WIN0; }
static int turn(void) { return G.pos.turn; }
static int busy(void) { return anim.on || (fresh && ui_dice_busy()); }   /* the end waits for the last hop */

static void throw_die(void)
{
    int k = G.pos.throws, me = G.pos.turn, v;
    if (G.n >= LUDO_MAX_EVENTS) return;
    if (k < nrolls) v = rolls[k];               /* thrown before an undo: the same number again */
    else {
        v = 1 + ui_random(6);
        rolls[k] = (uint8_t)v;
        nrolls = k + 1;
    }
    if (!ludo_throw(&G, v)) return;
    ui_dice_throw(1, &v, me);
    fresh = 1;
}

static void play(int token, int animate)
{
    lpos before = G.pos;
    int r = ludo_move(&G, token);
    if (!r) { ui_sound(SND_DENY); return; }
    int from = before.at[before.turn][token];
    anim.on = animate;
    anim.player = before.turn;
    anim.token = token;
    anim.from = from;
    anim.hops = from < 0 ? 1 : G.pos.at[before.turn][token] - from;
    anim.heard = 0;
    anim.caught = r > 1;
    anim.t0 = ui_ms();
    anim.before = before;
    if (!animate) ui_sound(r > 1 ? SND_CAPTURE : SND_MOVE);
}

static int undo(void)
{
    if (!ludo_undo(&G)) return 0;
    anim.on = 0;
    drag.on = 0;
    fresh = 0;
    return 1;
}

/* "6 a 4 a 3 6 b": throws and the tokens moved (ludo.h) */
static int script(const char *s)
{
    int ok = ludo_script(&G, s);
    for (int i = 0; i < G.n; i++)               /* its throws are the game's from now on */
        if (G.ev[i] < LUDO_MOVE_EV) rolls[G.past[i].throws] = G.ev[i];
    nrolls = G.pos.throws;
    anim.on = 0;
    drag.on = 0;
    fresh = 0;
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a rounded rectangle of one colour painted over the board (n pixels wide) */
static void paint_rect(uint8_t *o, int n, int x, int y, int w, int h, float rad, const uint8_t rgb[3], float a)
{
    if (w < 1 || h < 1) return;
    const uint8_t fill[4] = {rgb[0], rgb[1], rgb[2], (uint8_t)(255 * a)};
    uint8_t *b = malloc((size_t)w * h * 4);
    if (!b) return;
    art_roundrect(w, h, rad, fill, b);
    art_blend(o, n, n, b, w, h, x, y);
    free(b);
}

/* the bare plank again inside a rounded rectangle (a yard's inner square) */
static void wood_back(uint8_t *o, const uint8_t *wood, int n, int x, int y, int w, int h, float rad)
{
    static const uint8_t white[4] = {255, 255, 255, 255};
    uint8_t *m = malloc((size_t)w * h * 4);
    if (!m) return;
    art_roundrect(w, h, rad, white, m);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            if (x + i < 0 || y + j < 0 || x + i >= n || y + j >= n) continue;
            size_t at = 4 * ((size_t)(y + j) * n + x + i);
            float a = m[4 * (j * w + i) + 3] / 255.0f;
            for (int k = 0; k < 3; k++) o[at + k] = (uint8_t)(o[at + k] + (wood[at + k] - o[at + k]) * a + 0.5f);
        }
    free(m);
}

/* a flat figure (a star, a triangle), w x h pixels for its unit square, painted at (x, y) with alpha a */
static void paint_fig(uint8_t *o, int n, const art_fig *f, const uint8_t rgb[3], float line, int x, int y, int w,
                      float a)
{
    art_style st = {{0}, {0}, {0}, {0}, 0, line};
    for (int k = 0; k < 3; k++) {
        st.fill[k] = rgb[k];
        st.outer[k] = st.inner[k] = INK[k];
    }
    uint8_t *b = malloc((size_t)w * w * 4);
    if (!b) return;
    art_render(f, &st, w, w, (float)w, 0, 0, b);
    for (int i = 0; i < w * w; i++) b[4 * i + 3] = (uint8_t)(b[4 * i + 3] * a);
    art_blend(o, n, n, b, w, w, x, y);
    free(b);
}

static void paint_disc(uint8_t *o, int n, float cx, float cy, int d, const uint8_t rgb[3], float a, float ring)
{
    uint8_t *b = malloc((size_t)d * d * 4);
    if (!b) return;
    if (ring > 0) art_ring(d, ring, rgb, a, b);
    else art_disc(d, rgb, a, 0, b);
    art_blend(o, n, n, b, d, d, (int)(cx - d / 2.0f), (int)(cy - d / 2.0f));
    free(b);
}

/* the whole board: cells of c pixels in a frame of f, (15 c + 2 f) squared; bottom: the player at the bottom */
static void paint_board(uint8_t *o, int c, int f, int bottom)
{
    int n = N * c + 2 * f, lw = c / 16 > 1 ? c / 16 : 1;
    art_board_n(N, N, c, f, 0, o);
    uint8_t *wood = malloc((size_t)n * n * 4);
    if (!wood) return;
    memcpy(wood, o, (size_t)n * n * 4);
    int col[2] = {bottom, !bottom};             /* the colour of each seat: 0 the bottom, 1 the top */
    /* the cross: pale squares; each player's start square and home column in its tone; lines between */
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            if (!arm(x, y)) continue;
            int s = (x == 6 && y == 13) || (x == 7 && y >= 9 && y <= 13) ? 0
                  : (x == 8 && y == 1) || (x == 7 && y >= 1 && y <= 5) ? 1 : -1;
            if (s >= 0) paint_rect(o, n, f + x * c, f + y * c, c, c, 0, TONE[col[s]], TONE_A[col[s]]);
            else paint_rect(o, n, f + x * c, f + y * c, c, c, 0, PALE, 0.42f);
        }
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            if (!arm(x, y)) continue;
            int px = f + x * c - lw / 2, py = f + y * c - lw / 2;
            paint_rect(o, n, px, py, c + lw, lw, 0, INK, 0.6f);
            paint_rect(o, n, px, py, lw, c + lw, 0, INK, 0.6f);
            if (!arm(x + 1, y)) paint_rect(o, n, px + c, py, lw, c + lw, 0, INK, 0.6f);
            if (!arm(x, y + 1)) paint_rect(o, n, px, py + c, c + lw, lw, 0, INK, 0.6f);
        }
    /* the yards: the two players' in their tones (four spots on bare wood), the two empty ones plain (the dice) */
    for (int q = 0; q < 4; q++) {
        int ox = f + (q == 1 || q == 3 ? 9 : 0) * c, oy = f + (q == 0 || q == 3 ? 9 : 0) * c, m = c / 6;
        if (q < 2) {
            const uint8_t *t = TONE[col[q]];
            paint_rect(o, n, ox + m, oy + m, 6 * c - 2 * m, 6 * c - 2 * m, 0.5f * c, t, TONE_A[col[q]]);
            int in = (int)(0.75f * c);
            wood_back(o, wood, n, ox + in, oy + in, 6 * c - 2 * in, 6 * c - 2 * in, 0.4f * c);
            for (int k = 0; k < 4; k++) {
                float u, v;
                place(col[q], k, -1, bottom, &u, &v);
                paint_disc(o, n, f + u * c, f + v * c, (int)(1.45f * c), t, 0.55f, 0);
                paint_disc(o, n, f + u * c, f + v * c, (int)(1.45f * c), INK, 0.45f, 0.9f);
            }
        } else {                                /* a groove round a plain tray */
            paint_rect(o, n, ox + m, oy + m, 6 * c - 2 * m, 6 * c - 2 * m, 0.5f * c, INK, 0.2f);
            wood_back(o, wood, n, ox + m + lw, oy + m + lw, 6 * c - 2 * (m + lw), 6 * c - 2 * (m + lw),
                      0.5f * c - lw);
        }
    }
    /* the centre: four triangles, the players' homes in their tones */
    for (int s = 0; s < 4; s++) {
        static const float tri[4][6] = {{0, 1, 1, 1, 0.5f, 0.5f}, {0, 0, 1, 0, 0.5f, 0.5f},
                                        {0, 0, 0, 1, 0.5f, 0.5f}, {1, 0, 1, 1, 0.5f, 0.5f}};
        art_fig *fig = art_fig_new();
        art_poly(fig, ART_PART, tri[s], 3);
        paint_fig(o, n, fig, s < 2 ? TONE[col[s]] : PALE, (float)lw / (3 * c), f + 6 * c, f + 6 * c, 3 * c,
                  s < 2 ? TONE_A[col[s]] + 0.05f : 0.6f);
        art_fig_free(fig);
    }
    /* the safe squares: a star (light on the ebony tone) */
    art_fig *star = art_fig_new();
    float pts[20];
    for (int i = 0; i < 10; i++) {
        float a = (-90 + 36 * i) * PI_F / 180, r = i & 1 ? 0.15f : 0.36f;
        pts[2 * i] = 0.5f + r * cosf(a);
        pts[2 * i + 1] = 0.52f + r * sinf(a);
    }
    art_poly(star, ART_PART, pts, 10);
    for (int i = 0; i < LUDO_TRACK; i++) {
        if (!ludo_safe(i)) continue;
        int dark = (i == 0 && col[0] == 1) || (i == LUDO_TRACK / 2 && col[1] == 1);
        paint_fig(o, n, star, dark ? TONE[0] : INK, 0, f + track[i][0] * c, f + track[i][1] * c, c, dark ? 0.8f : 0.5f);
    }
    art_fig_free(star);
    free(wood);
}

static void layout(box b)
{
    if (!track[1][1]) make_track();
    cs = (int)floorf(b.w / (N + 2 * FRAME));
    fr = (int)(cs * FRAME);
    float bw = (float)N * cs + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2) + fr;
    by = floorf(b.y + (b.h - bw) / 2) + fr;
    if (made != cs || made_flip != ui_bottom()) {
        made = cs;
        made_flip = ui_bottom();
        int n = N * cs + 2 * fr;
        uint8_t *o = malloc((size_t)n * n * 4);
        if (o) {
            paint_board(o, cs, fr, ui_bottom());
            ui_free(&t_board);
            t_board = ui_tex(n, n, o);
            free(o);
        }
        ps = (int)(PAWN_SIZE * cs);
        int d = (int)(DIE * cs);
        for (int p = 0; p < 2; p++) {
            ui_free(&t_pawn[p]);
            art_piece(PAWN, p, ps, ui_buf(ps, ps));
            t_pawn[p] = ui_tex(ps, ps, ui_buf(ps, ps));
            for (int v = 1; v <= 6; v++) {
                ui_free(&t_die[p][v]);
                art_die(v, p, d, ui_buf(d, d));
                t_die[p][v] = ui_tex(d, d, ui_buf(d, d));
            }
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int p = 0; p < 2; p++) {
            ui_free(&t_small[p]);
            art_piece(PAWN, p, small, ui_buf(small, small));
            t_small[p] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

static float px_x(float u) { return bx + u * cs; }
static float px_y(float v) { return by + v * cs; }

/* a pawn standing on (x, y) (pixels), its foot a little below; turned round for the top player, face to face */
static void pawn_at(int player, float x, float y, float scale)
{
    float s = ps * scale, left = x - s / 2, top = y + 0.36f * cs * scale - 0.86f * s;
    if (ui_upside(player)) {
        top = 2 * y - top - s;
        ui_shadow(left, top - 0.79f * s, s);
        ui_put_rot(t_pawn[player], left + s / 2, top + s / 2, s, s, 180);
    } else {
        ui_shadow(left, top, s);
        ui_put(t_pawn[player], left, top, s, s);
    }
}

/* where a player throws: the centre of the empty yard on its right */
static void tray(int player, float *x, float *y)
{
    float u = player == ui_bottom() ? 12 : 3;
    *x = px_x(u);
    *y = px_y(u);
}

static int last_throw(int player)               /* the player's last throw (0: none yet) */
{
    for (int i = G.n - 1; i >= 0; i--)
        if (G.ev[i] < LUDO_MOVE_EV && G.past[i].turn == player) return G.ev[i];
    return 0;
}

/* each player's die in its tray: the one to throw lit in gold; a throw that could not move dimmed, in a red glow */
static void draw_dice(void)
{
    const lpos *p = &G.pos;
    int latest = -1, rolling = fresh && ui_dice_busy();
    for (int i = G.n - 1; i >= 0 && latest < 0; i--)
        if (G.ev[i] < LUDO_MOVE_EV) latest = G.past[i].turn;
    int lost = G.n && G.ev[G.n - 1] < LUDO_MOVE_EV && p->phase == LUDO_THROW && p->turn != latest;
    for (int q = 0; q < 2; q++) {
        int v = last_throw(q), active = p->phase != LUDO_OVER && p->turn == q;
        int invite = active && p->phase == LUDO_THROW && !rolling, dim = !active && !(rolling && q == latest);
        if (!v && !invite) continue;
        float x, y, d = (float)(int)(DIE * cs);
        tray(q, &x, &y);
        if (invite) ui_mark_gold(x, y, 4.4f * cs), ui_mark_gold(x, y, 3 * cs);   /* throw me */
        if (lost && q == latest && !rolling) ui_mark_glow(x, y, 3.2f * cs);
        if (fresh && q == latest) {
            ui_dice_draw(x, y, d, (unsigned)dim);
        } else {
            SDL_Texture *t = t_die[q][v ? v : 6];
            Uint8 a = dim ? 110 : 255;
            ui_shadow_flat(x, y, d * 1.1f, d * 1.1f, a / 255.0f);
            SDL_SetTextureAlphaMod(t, a);
            ui_put_c(t, x, y, d, d);
            SDL_SetTextureAlphaMod(t, 255);
        }
    }
}

/* where the moving token is, t hops after it set off: square by square, a little leap each */
static void hop_at(float t, float *u, float *v)
{
    int h = (int)t;
    float f = t - h;
    if (h >= anim.hops) h = anim.hops - 1, f = 1;
    int a0 = anim.from < 0 ? -1 : anim.from + h, a1 = anim.from < 0 ? 0 : anim.from + h + 1;
    float u0, v0, u1, v1;
    place(anim.player, anim.token, a0, ui_bottom(), &u0, &v0);
    place(anim.player, anim.token, a1, ui_bottom(), &u1, &v1);
    *u = u0 + (u1 - u0) * f;
    *v = v0 + (v1 - v0) * f - 0.4f * sinf(f * PI_F);
}

/* where the tokens that can move go: dots (under the pawns), or rings round a token sent home (over them) */
static void draw_targets(int rings)
{
    const lpos *p = &G.pos;
    int moves = p->phase == LUDO_MOVE && !busy() ? ludo_movable(p) : 0;
    for (int k = 0; k < LUDO_TOKENS; k++) {
        if (!(moves >> k & 1)) continue;
        int to = ludo_dest(p, k), sq = ludo_square(p->turn, to), hit = 0;
        for (int e = 0; e < LUDO_TOKENS && sq >= 0 && !ludo_safe(sq); e++)
            hit |= ludo_square(!p->turn, p->at[!p->turn][e]) == sq;
        float u, v;
        place(p->turn, k, to, ui_bottom(), &u, &v);
        if (hit && rings) ui_mark_ring(px_x(u), px_y(v), 1.25f * cs);
        else if (!hit && !rings) ui_mark_dot(px_x(u), px_y(v), 0.42f * cs);
    }
}

static void draw(void)
{
    const lpos *p = &G.pos;
    float n = (float)N * cs + 2 * fr, u, v;
    ui_put(t_board, bx - fr, by - fr, n, n);
    for (int i = G.n - 1; i >= 0; i--) {        /* the last move: where it came from, where it went */
        if (G.ev[i] < LUDO_MOVE_EV) continue;
        const lpos *b = &G.past[i];
        int t = G.ev[i] - LUDO_MOVE_EV, from = b->at[b->turn][t], to = ludo_dest(b, t);
        for (int k = 0; k < 2; k++) {
            int at = k ? to : from;
            if (at < 0 || at >= LUDO_HOME) continue;
            place(b->turn, t, at, ui_bottom(), &u, &v);
            ui_mark_last(px_x(u - 0.5f), px_y(v - 0.5f), (float)cs, (float)cs);
        }
        break;
    }
    draw_dice();
    float t = 0;
    if (anim.on) {                              /* a hop a beat; the capture's sound on the last */
        t = (ui_ms() - anim.t0) / (float)HOP_MS;
        int landed = t >= anim.hops ? anim.hops : (int)t;
        while (anim.heard < landed) ui_sound(++anim.heard == anim.hops && anim.caught ? SND_CAPTURE : SND_MOVE);
        if (t >= anim.hops) anim.on = 0;
        else ui_animate();
    }
    tok all[2 * LUDO_TOKENS];                   /* the tokens standing (not the one hopping, nor the one dragged) */
    int n_tok = anim.on ? gather(&anim.before, ui_bottom(), anim.player, anim.token, all)
                        : gather(p, ui_bottom(), drag.on && drag.moved ? p->turn : -1, drag.token, all);
    for (int i = 0; i < n_tok && drag.on && !drag.moved; i++)   /* the token pressed */
        if (all[i].player == p->turn && all[i].k == drag.token)
            ui_mark_select(px_x(all[i].u - 0.5f), px_y(all[i].v - 0.5f), (float)cs, (float)cs);
    draw_targets(0);
    for (int i = 0; i < n_tok; i++) pawn_at(all[i].player, px_x(all[i].u), px_y(all[i].v), all[i].scale);
    draw_targets(1);
    if (anim.on) {
        hop_at(t, &u, &v);
        int home = anim.from + anim.hops >= LUDO_HOME && t >= anim.hops - 1;
        pawn_at(anim.player, px_x(u), px_y(v), home ? 0.62f + 0.38f * (anim.hops - t) : 1);
    }
    if (drag.on && drag.moved) {                /* the dragged token, a little bigger, above the finger */
        float off = ui_upside(p->turn) ? -0.5f : 0.5f;
        pawn_at(p->turn, drag.x, drag.y - off * ps, 1.15f);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the token of the player to move that can go, under the point (u, v) (cells), or going there (dest); -1 */
static int token_near(float u, float v, int dest)
{
    const lpos *p = &G.pos;
    tok all[2 * LUDO_TOKENS];
    int n = gather(p, ui_bottom(), -1, -1, all), best = -1, moves = ludo_movable(p);
    float near = dest ? 0.6f : 0.75f;
    for (int i = 0; i < n; i++) {
        tok *t = &all[i];
        if (t->player != p->turn || !(moves >> t->k & 1)) continue;
        float tu = t->u, tv = t->v;
        if (dest) place(t->player, t->k, ludo_dest(p, t->k), ui_bottom(), &tu, &tv);
        else if (p->at[t->player][t->k] < 0) tv -= 0.1f;
        float d = hypotf(u - tu, v - tv) / (!dest && p->at[t->player][t->k] < 0 ? 1.4f : 1);
        if (d < near) near = d, best = t->k;
    }
    return best;
}

static void press(float x, float y)
{
    const lpos *p = &G.pos;
    if (p->phase == LUDO_OVER || busy()) return;
    if (p->phase == LUDO_THROW) {               /* a tap on the die (its tray) throws it */
        float tx, ty;
        tray(p->turn, &tx, &ty);
        if (fabsf(x - tx) < 3 * cs && fabsf(y - ty) < 3 * cs) throw_die();
        else ui_sound(SND_DENY);
        return;
    }
    float u = (x - bx) / cs, v = (y - by) / cs;
    int t = token_near(u, v, 0);
    if (t >= 0) {
        drag.on = 1;
        drag.token = t;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    t = token_near(u, v, 1);
    if (t >= 0) play(t, 1);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > cs * 0.25f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) { play(drag.token, 1); return; }   /* a tap: the token goes, hopping */
    float u, v;
    place(G.pos.turn, drag.token, ludo_dest(&G.pos, drag.token), ui_bottom(), &u, &v);
    if (hypotf((x - bx) / cs - u, (y - by) / cs - v) < 0.8f) play(drag.token, 0);
    else ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)               /* the player's tokens home */
{
    float sz = 0.6f * ui.unit, step = 0.42f * ui.unit, cy = b.y + b.h / 2;
    for (int i = 0; i < ludo_home(&G.pos, player); i++) ui_put(t_small[player], b.x + i * step, cy - sz / 2, sz, sz);
}

static void token(int player, int size, uint8_t *rgba) { art_piece(PAWN, player, size, rgba); }

/* the menu's card: the lower left part of the board, pawns out and home, a die */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    if (!track[1][1]) make_track();
    int c = (int)(size / 9.4f), f = (int)(c * FRAME), n = N * c + 2 * f, oy = n - size;
    uint8_t *o = malloc((size_t)n * n * 4);
    if (!o) return;
    paint_board(o, c, f, 0);
    for (int y = 0; y < size; y++)
        memcpy(rgba + 4 * (size_t)y * size, o + 4 * ((size_t)(y + oy) * n), 4 * (size_t)size);
    free(o);
    static const int put[][3] = {{0, 0, -1}, {0, 2, -1}, {0, 1, 3}, {1, 0, 34}, {1, 1, 23}};
    int s = (int)(PAWN_SIZE * c), d = (int)(1.5f * c);
    uint8_t *one = malloc((size_t)s * s * 4);   /* the pawn's picture; the die is smaller */
    if (!one) return;
    art_die(5, 0, d, one);
    art_blend(rgba, size, size, one, d, d, f + 3 * c - d / 2, f + 12 * c - oy - d / 2);
    for (unsigned i = 0; i < sizeof put / sizeof put[0]; i++) {
        float u, v;
        place(put[i][0], put[i][1], put[i][2], 0, &u, &v);
        art_piece(PAWN, put[i][0], s, one);
        art_blend(rgba, size, size, one, s, s, (int)(f + u * c - s / 2.0f),
                  (int)(f + v * c - oy + 0.36f * c - 0.86f * s));
    }
    free(one);
}

static const game ludo_game = {
    .id = "ludo", .order = 140, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(ludo_game)
