/*
 * Rithmomachia in RetroStone Board: the long checkered board (8 x 16; a line across the middle parts the two halves),
 * the pieces in ivory and ebony (rounds, triangles pointing at the enemy, squares, and the stepped pyramid seen from
 * above), their numbers burned in. Nothing is secret (VIEW_OPEN): the board turns round with the device, the army of
 * the player to move at the bottom (portrait) or on the left (landscape), so it takes the whole table. Tap or drag a
 * piece: dots where it can go, rings where the move would take something. After a move the capturers glow gold and
 * the pieces taken glow red as they vanish; a harmony glows gold. The rules: rithmomachia_rules.c, rithmomachia.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The script's notation: a move is its two squares, "c4d5", "b14b12" (files a..h from the evens' left, ranks 1..16
 * from their side).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/grid.h"
#include "games/rithmomachia.h"
#include "shell.h"
#include "text.h"

#define FRAME 0.26f                             /* the board's frame, in squares */
#define MOVE_MS 220
#define TAKE_MS 760                             /* the capturers' light, the pieces taken fading */
#define PI_F 3.14159265f

static rhmgame G;
static rhmmove legal[RHM_MAX_MOVES];
static uint8_t takes[RHM_MAX_MOVES];            /* 1: the legal move would take something */
static int nlegal, sel = -1, variant;
static struct { int land, rot, cols, rows, s, f; float x, y; } T;   /* as shown; (x, y): the squares' top left */
static int made = -1, made_land = -1, made_small = -1;
static SDL_Texture *t_piece[2][5], *t_small[2], *t_board;
static struct { int on, from, moved; float x, y; } drag;
static struct {
    int on, n, mover;
    rhmmove m;
    uint8_t gone[RHM_PIECES], id[RHM_PIECES];   /* the squares taken, the pieces that were there */
    uint8_t by[RHM_N];                          /* 1: a capturer */
    Uint32 t0;
} anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    const rhmpos *p = rhm_pos(&G);
    nlegal = rhm_status(&G) == RHM_PLAY ? rhm_legal(p, legal) : 0;
    for (int i = 0; i < nlegal; i++) {
        rhmpos q = *p;
        rhm_make(&q, legal[i], G.variant);
        takes[i] = q.taken[p->side] > p->taken[p->side];
    }
    sel = -1;
    drag.on = 0;
}

static void start(int v)
{
    variant = v;
    rhm_new(&G, v);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    static const int map[] = {[RHM_PLAY] = GS_PLAY, [RHM_WIN0] = GS_WIN0, [RHM_WIN1] = GS_WIN1, [RHM_DRAW] = GS_DRAW};
    return map[rhm_status(&G)];
}

static int turn(void) { return rhm_pos(&G)->side; }

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static void play(rhmmove m, int animate)
{
    rhmpos q = *rhm_pos(&G);
    int me = q.side;
    rhm_step(&q, m);                            /* what the move takes, and by whom */
    anim.n = 0;
    memset(anim.by, 0, sizeof anim.by);
    for (int s = 0; s < RHM_N; s++) {
        int att[2];
        int how = q.sq[s] && rhm_piece[q.sq[s] - 1].owner != me ? rhm_threat(&q, me, s, att) : 0;
        if (!how) continue;
        anim.gone[anim.n] = (uint8_t)s;
        anim.id[anim.n++] = (uint8_t)(q.sq[s] - 1);
        if (how == RHM_SIEGE) {
            int x = s % RHM_W, y = s / RHM_W;
            if (x > 0) anim.by[s - 1] = 1;
            if (x < RHM_W - 1) anim.by[s + 1] = 1;
            if (y > 0) anim.by[s - RHM_W] = 1;
            if (y < RHM_H - 1) anim.by[s + RHM_W] = 1;
        } else
            for (int k = 0; k < 2; k++)
                if (att[k] >= 0) anim.by[att[k]] = 1;
    }
    if (!rhm_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.mover = me;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(SND_MOVE);
    if (anim.n) ui_sound_later(SND_CAPTURE, animate ? MOVE_MS : 0);
    ui_handoff(turn());                         /* the board turns round once the move is over */
}

static int undo(void)
{
    if (!rhm_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    ui_handoff(turn());
    return 1;
}

static int script(const char *s)
{
    int ok = rhm_script(&G, s);
    anim.on = 0;
    refresh();
    ui_handoff(turn());
    return ok;
}

/* the bot's side (rithmomachia_bot.c): the game to copy, and its move played as a tap plays it */
const rhmgame *rithmomachia_live(void) { return &G; }

int rithmomachia_show(const char *move)
{
    int la, lb, a = rhm_square(move, &la), b = a < 0 ? -1 : rhm_square(move + la, &lb);
    if (b < 0 || move[la + lb] || target(a, b) < 0) return 0;
    play((rhmmove){(uint8_t)a, (uint8_t)b}, 1);
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static const uint8_t BURNT[3] = {86, 46, 22}, INLAY[3] = {236, 220, 186};   /* the numbers: on ivory, on ebony */

/* a piece's shape (a triangle pointing up), size x size, in its player's ivory or ebony: a rim round a raised face */
static void paint_piece(int shape, int owner, int size, uint8_t *rgba)
{
    art_style st = art_player[owner];
    st.line = fmaxf(0.02f, 1.2f / size);
    art_fig *f = art_fig_new();
    if (shape == RHM_ROUND) {
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.44f, 0.44f);
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.37f, 0.37f);
    } else if (shape == RHM_TRIANGLE) {
        const float cy = 0.60f, out[] = {0.5f, 0.07f, 0.96f, 0.865f, 0.04f, 0.865f};
        float in[6];
        for (int i = 0; i < 3; i++) {
            in[2 * i] = 0.5f + (out[2 * i] - 0.5f) * 0.8f;
            in[2 * i + 1] = cy + (out[2 * i + 1] - cy) * 0.8f;
        }
        art_poly(f, ART_PART, out, 3);
        art_poly(f, ART_PART, in, 3);
    } else if (shape == RHM_SQUARE) {
        art_rect(f, ART_PART, 0.09f, 0.09f, 0.91f, 0.91f);
        art_rect(f, ART_PART, 0.16f, 0.16f, 0.84f, 0.84f);
    } else {                                    /* the pyramid, stepped, seen from above */
        art_rect(f, ART_PART, 0.05f, 0.05f, 0.95f, 0.95f);
        art_rect(f, ART_PART, 0.15f, 0.15f, 0.85f, 0.85f);
        art_rect(f, ART_PART, 0.25f, 0.25f, 0.75f, 0.75f);
        const float edges[4][4] = {{0.05f, 0.05f, 0.25f, 0.25f}, {0.95f, 0.05f, 0.75f, 0.25f},
                                   {0.05f, 0.95f, 0.25f, 0.75f}, {0.95f, 0.95f, 0.75f, 0.75f}};
        for (int i = 0; i < 4; i++) art_band(f, ART_MARK, edges[i][0], edges[i][1], edges[i][2], edges[i][3], 0.012f);
        memcpy(st.mark, st.outer, sizeof st.mark);
    }
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

static void make_board(void)
{
    static const uint8_t INK[3] = {40, 26, 16};
    int w = T.cols * T.s + 2 * T.f, h = T.rows * T.s + 2 * T.f, st = w > h ? w : h;
    uint8_t *o = ui_buf(w, h);
    art_board_n(T.cols, T.rows, T.s, T.f, 1, o);
    float *cov = calloc((size_t)st * st, sizeof *cov);
    if (cov) {                                  /* the middle line: the two halves */
        float lw = 1.2f + 0.035f * T.s, m = T.f + 8.0f * T.s;
        if (T.land) grid_cover_rect(cov, st, m - lw / 2, (float)T.f, m + lw / 2, (float)(T.f + T.rows * T.s));
        else grid_cover_rect(cov, st, (float)T.f, m - lw / 2, (float)(T.f + T.cols * T.s), m + lw / 2);
        grid_ink(o, w, h, cov, st, INK, 0.75f);
        free(cov);
    }
    ui_free(&t_board);
    t_board = ui_tex(w, h, o);
}

static void layout(box b)
{
    float kp = fminf(b.w / (RHM_W + 2 * FRAME), b.h / (RHM_H + 2 * FRAME)),
          kl = fminf(b.w / (RHM_H + 2 * FRAME), b.h / (RHM_W + 2 * FRAME));
    T.land = kl > kp;
    T.cols = T.land ? RHM_H : RHM_W;
    T.rows = T.land ? RHM_W : RHM_H;
    T.s = (int)floorf(fmaxf(kp, kl));
    T.f = (int)(T.s * FRAME);
    int w = T.cols * T.s + 2 * T.f, h = T.rows * T.s + 2 * T.f;
    T.x = floorf(b.x + (b.w - w) / 2) + T.f;
    T.y = floorf(b.y + (b.h - h) / 2) + T.f;
    if (made != T.s) {
        made = T.s;
        for (int o = 0; o < 2; o++)
            for (int k = RHM_ROUND; k <= RHM_PYRAMID; k++) {
                ui_free(&t_piece[o][k]);
                paint_piece(k, o, T.s, ui_buf(T.s, T.s));
                t_piece[o][k] = ui_tex(T.s, T.s, ui_buf(T.s, T.s));
            }
        made_land = -1;
    }
    if (made_land != T.land) {
        made_land = T.land;
        make_board();
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int o = 0; o < 2; o++) {
            ui_free(&t_small[o]);
            paint_piece(RHM_ROUND, o, small, ui_buf(small, small));
            t_small[o] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* the board turns round for the odds: the army of the player whose view it is at the bottom, or on the left */
static int turned(void) { return ui.viewer == 1; }

static void sq_xy(int s, float *x, float *y)    /* a square's centre */
{
    int fx = s % RHM_W, fy = s / RHM_W, r = turned(), col, row;
    if (T.land) col = r ? RHM_H - 1 - fy : fy, row = r ? RHM_W - 1 - fx : fx;
    else col = r ? RHM_W - 1 - fx : fx, row = r ? fy : RHM_H - 1 - fy;
    *x = T.x + (col + 0.5f) * T.s;
    *y = T.y + (row + 0.5f) * T.s;
}

static int xy_sq(float x, float y)
{
    int col = (int)floorf((x - T.x) / T.s), row = (int)floorf((y - T.y) / T.s), r = turned();
    if (col < 0 || row < 0 || col >= T.cols || row >= T.rows) return -1;
    if (T.land) return RHM_SQ(r ? RHM_W - 1 - row : row, r ? RHM_H - 1 - col : col);
    return RHM_SQ(r ? RHM_W - 1 - col : col, r ? row : RHM_H - 1 - row);
}

static void mark_sq(int s, void (*mark)(float, float, float, float))
{
    float x, y, h = T.s / 2.0f;
    sq_xy(s, &x, &y);
    mark(x - h, y - h, (float)T.s, (float)T.s);
}

static int digits(int v) { return v >= 100 ? 3 : v >= 10 ? 2 : 1; }

/* a piece (its id) centred on (x, y), d across, faded (alpha 0..1), its shadow under it; its number burned in */
static void piece_at(int id, float x, float y, float d, float alpha)
{
    const rhmpiece *k = &rhm_piece[id];
    SDL_Texture *t = t_piece[k->owner][k->shape];
    float deg = 0, nx = 0, ny = 0, a = clampf(alpha, 0, 1);
    if (k->shape == RHM_TRIANGLE) {             /* pointing at the enemy: up (right in landscape) for the viewer's */
        int ahead = (k->owner == 0) != turned();
        deg = (T.land ? 90.0f : 0.0f) + (ahead ? 0 : 180);
        float c = 0.1f * d, r = deg * PI_F / 180;   /* the number at the triangle's centroid */
        nx = -sinf(r) * c;
        ny = cosf(r) * c;
    }
    float sh = k->shape == RHM_TRIANGLE ? 0.62f : 0.88f;   /* a triangle's shadow: under its middle */
    ui_shadow_flat(x + nx / 2, y + ny / 2, sh * d, sh * d, a);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * a));
    ui_put_rot(t, x, y, d, d, deg);
    SDL_SetTextureAlphaMod(t, 255);
    static const float H[5][4] = {{0}, {0, 0.40f, 0.34f, 0.27f}, {0, 0.34f, 0.29f, 0.22f}, {0, 0.42f, 0.36f, 0.27f},
                                  {0, 0.34f, 0.29f, 0.235f}};
    float h = H[k->shape][digits(k->value)] * d;
    ui_number_ex(x + nx, y + ny, h, k->value, k->owner ? INLAY : BURNT, (Uint8)(255 * a), UI_NUM_TIGHT);
}

static void draw(void)
{
    const rhmpos *p = rhm_pos(&G);
    float x, y, s = (float)T.s;
    ui_put(t_board, T.x - T.f, T.y - T.f, T.cols * s + 2 * T.f, T.rows * s + 2 * T.f);
    if (G.ply) {
        mark_sq(G.move[G.ply - 1].from, ui_mark_last);
        mark_sq(G.move[G.ply - 1].to, ui_mark_last);
    }
    if (sel >= 0) mark_sq(sel, ui_mark_select);
    int el = (int)(ui_ms() - anim.t0), total = MOVE_MS + (anim.n ? TAKE_MS : 0);
    if (anim.on && el >= total) anim.on = 0;
    if (anim.on) ui_animate();
    float tk = anim.on ? clampf((el - MOVE_MS) / (float)TAKE_MS, 0, 1) : 1;
    if (anim.on && el >= MOVE_MS)               /* the capturers' light, the pieces taken in red */
        for (int q = 0; q < RHM_N; q++) {
            if (!anim.by[q]) continue;
            sq_xy(q, &x, &y);
            ui_mark_gold_a(x, y, 2.1f * s, sinf(tk * PI_F));
        }
    if (p->proper && !anim.on)                  /* the harmony */
        for (int i = 0; i < 3; i++) {
            sq_xy(p->harmony[i], &x, &y);
            ui_mark_gold(x, y, 1.4f * s);
        }
    for (int q = 0; q < RHM_N; q++) {
        if (!p->sq[q] || (drag.on && drag.moved && q == drag.from) || (anim.on && q == anim.m.to)) continue;
        sq_xy(q, &x, &y);
        piece_at(p->sq[q] - 1, x, y, s, 1);
    }
    if (anim.on) {
        for (int i = 0; i < anim.n; i++) {      /* the pieces taken: still there while the move slides, then gone */
            sq_xy(anim.gone[i], &x, &y);
            if (el >= MOVE_MS) ui_mark_glow(x, y, 1.2f * s * (1 - 0.4f * tk));
            piece_at(anim.id[i], x, y - 0.25f * s * tk * tk, s, 1 - tk);
        }
        float t = clampf(el / (float)MOVE_MS, 0, 1), e = 1 - (1 - t) * (1 - t) * (1 - t), x0, y0;
        sq_xy(anim.m.from, &x0, &y0);
        sq_xy(anim.m.to, &x, &y);
        piece_at(p->sq[anim.m.to] - 1, x0 + (x - x0) * e, y0 + (y - y0) * e, s * (1 + 0.1f * sinf(t * PI_F)), 1);
    }
    if (sel >= 0)                               /* where the selected piece can go; a ring where it would take */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            sq_xy(legal[i].to, &x, &y);
            if (takes[i]) ui_mark_ring(x, y, s);
            ui_mark_dot(x, y, 0.3f * s);
        }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, above the finger */
        piece_at(p->sq[drag.from] - 1, drag.x, drag.y - 0.4f * s, 1.25f * s, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY || anim.on) return;
    const rhmpos *p = rhm_pos(&G);
    int q = xy_sq(x, y), i;
    if (q < 0) { sel = -1; return; }
    if (sel >= 0 && (i = target(sel, q)) >= 0) {
        play(legal[i], 1);
        return;
    }
    if (p->sq[q] && rhm_piece[p->sq[q] - 1].owner == p->side) {
        int any = 0;
        for (i = 0; i < nlegal && !any; i++) any = legal[i].from == q;
        if (!any) { ui_sound(SND_DENY); sel = -1; return; }   /* hemmed in */
        sel = q;
        drag.on = 1;
        drag.from = q;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    ui_sound(SND_DENY);
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > T.s * 0.15f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int q = xy_sq(x, y), i = q >= 0 && q != drag.from ? target(drag.from, q) : -1;
    if (i >= 0) play(legal[i], 0);
    else if (q != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces the player took (a round of the other colour and how many); the moves left before the draw once few */
static void bar(int player, box b)
{
    const rhmpos *p = rhm_pos(&G);
    int left = RHM_QUIET - p->quiet;
    if (left <= 20 && p->side == player && status() == GS_PLAY) b = ui_bar_countdown(b, left);
    ui_bar_number(b, t_small[!player], p->taken[player]);
}

static void token(int player, int size, uint8_t *rgba) { paint_piece(RHM_PYRAMID, player, size, rgba); }

/* a piece with its number into a picture (a thumbnail): shape, owner, value, centred on (cx, cy), d across */
static void thumb_piece(uint8_t *rgba, int size, int shape, int owner, int value, float cx, float cy, int d, int down)
{
    uint8_t *one = malloc((size_t)d * d * 4);
    if (!one) return;
    paint_piece(shape, owner, d, one);
    if (down) {                                 /* turned round: an enemy triangle */
        for (int y = 0; y < d / 2; y++)
            for (int x = 0; x < d; x++) {
                uint32_t a, b;
                memcpy(&a, one + 4 * (y * d + x), 4);
                memcpy(&b, one + 4 * ((d - 1 - y) * d + d - 1 - x), 4);
                memcpy(one + 4 * (y * d + x), &b, 4);
                memcpy(one + 4 * ((d - 1 - y) * d + d - 1 - x), &a, 4);
            }
    }
    art_flat_put(rgba, size, size, one, d, cx, cy);
    free(one);
    if (!value) return;
    char num[8];
    snprintf(num, sizeof num, "%d", value);
    static const float H[5] = {0, 0.40f, 0.34f, 0.40f, 0.30f};
    float px = H[shape] * d * (value >= 100 ? 0.75f : 1) * 1.3f;
    int w = text_line(num, px, 1, owner ? INLAY : BURNT, NULL, size, size, 0, 0);
    float oy = shape == RHM_TRIANGLE ? (down ? -0.1f : 0.1f) * d : 0;
    text_line(num, px, 1, owner ? INLAY : BURNT, rgba, size, size, (int)(cx - w / 2.0f),
              (int)(cy + oy - 0.66f * px));
}

/* the menu's card: a corner of the board, pieces of both armies; the picker's: the pieces to take (8 or 12), or three
 * pieces in harmony in their gold light */
static void thumb(int v, int size, uint8_t *rgba)
{
    int n = 4, s = (int)(size / (n + 2 * FRAME)), f = (int)(s * FRAME), w = n * s + 2 * f, o = (size - w) / 2;
    uint8_t *pic = ui_buf(w, w);
    art_board_n(n, n, s, f, 1, pic);
    art_blend(rgba, size, size, pic, w, w, o, o);
#define CX(c) (o + f + ((c) + 0.5f) * s)
    if (v < 0) {
        static const int place[][5] = {{0, 0, RHM_SQUARE, 1, 121}, {2, 0, RHM_TRIANGLE, 1, 36},
                                       {1, 1, RHM_ROUND, 1, 9}, {3, 1, RHM_PYRAMID, 1, 190},
                                       {0, 2, RHM_ROUND, 0, 16}, {2, 2, RHM_TRIANGLE, 0, 25},
                                       {1, 3, RHM_PYRAMID, 0, 91}, {3, 3, RHM_SQUARE, 0, 289}};
        for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++)
            thumb_piece(rgba, size, place[i][2], place[i][3], place[i][4], CX(place[i][0]), CX(place[i][1]),
                        (int)(0.92f * s), place[i][3] == 1);
    } else if (v == RHM_PROPER) {
        static const uint8_t gold[3] = {255, 214, 120};
        int g = (int)(2.0f * s);
        uint8_t *glow = malloc((size_t)g * g * 4);
        if (glow) {
            art_disc(g, gold, 1, 1, glow);
            for (int i = 0; i < 3; i++)
                art_blend(rgba, size, size, glow, g, g, (int)(CX(i) + 0.5f * s) - g / 2, (int)CX(1.5f) - g / 2);
            free(glow);
        }
        for (int i = 0; i < 3; i++)
            thumb_piece(rgba, size, RHM_ROUND, 0, 4 + 2 * i, CX(i) + 0.5f * s, CX(1.5f), (int)(0.92f * s), 0);
    } else {
        int k = rhm_goal(v), d = (int)(0.8f * s);
        for (int i = 0; i < k; i++) {           /* the enemy pieces to take, in rows */
            int c = i % 4, r = i / 4, rows = (k + 3) / 4;
            thumb_piece(rgba, size, RHM_ROUND, 1, 0, CX(c), CX(r + (4 - rows) / 2.0f), d, 0);
        }
    }
#undef CX
}

static int busy(void) { return anim.on; }     /* the board turns, and the end waits, once the move is over */

static const game rithmomachia_game = {
    .id = "rithmomachia", .order = 262, .variants = RHM_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .view = VIEW_OPEN,
};
REGISTER_GAME(rithmomachia_game)
