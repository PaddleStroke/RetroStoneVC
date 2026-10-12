/*
 * Bagh-Chal in RetroStone Board: the 5 x 5 board of lines on one plank (lines.h), the goats (a goat's head on an
 * ivory disc) and the tigers (a tiger's head on an ebony disc); a tap sets a goat down, tap or drag to move, the
 * tigers' jumps (the rules and the notation: baghchal_rules.c, baghchal.h). The goats' bar: the goats still to come;
 * the tigers' bar: the goats taken.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/baghchal.h"
#include "games/lines.h"
#include "shell.h"

#define DISC 0.78f                              /* a piece's size, in cells */
#define STEP_MS 150
#define JUMP_MS 230
#define FADE_MS 240
#define PLACE_MS 160

static bcgame G;
static bcmove legal[BC_MAX_MOVES];
static int nlegal;
static lines_geo L = {.cells = BC_SIDE};        /* the plank: a point in the middle of each of its 5 x 5 cells */
static int made_small = -1;
static SDL_Texture *t_piece[2], *t_small[2], *t_board;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, who; bcmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int status(void)
{
    switch (bc_status(&G)) {
    case BS_PLAY: return GS_PLAY;
    case BS_TRAPPED: return GS_WIN0;
    case BS_FIVE: case BS_STUCK: return GS_WIN1;
    default: return GS_DRAW;
    }
}

static void refresh(void)
{
    nlegal = bc_legal(bc_pos(&G), legal);
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    bc_new(&G);
    anim.on = 0;
    refresh();
}

static int turn(void) { return bc_pos(&G)->side; }

static int placing(void) { const bcpos *p = bc_pos(&G); return p->side == BC_GOAT && bc_to_place(p) > 0; }

static int find(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

/* the jump over a goat (a tap on the goat itself): from the selected tiger, or the only tiger that can take it */
static int find_over(int from, int over)
{
    int k = -1, n = 0;
    for (int i = 0; i < nlegal; i++)
        if (legal[i].over == over && (from < 0 || legal[i].from == from)) k = i, n++;
    return n == 1 ? k : -1;
}

static int can_move(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from) return 1;
    return 0;
}

static void play(bcmove m, int animate)
{
    anim.who = bc_pos(&G)->side;
    if (!bc_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.from < 0 ? SND_PLACE : m.over >= 0 ? SND_CAPTURE : SND_MOVE);
}

static int undo(void)
{
    if (!bc_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "c3 a1-b2 c2 b2xd2": the screenshots' and the tests' moves (baghchal.h) */
static int script(const char *s)
{
    int ok = bc_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank with its 16 lines, each drawn whole from end to end (the 5 rows, the 5 columns, the two long diagonals and
 * the diamond joining the middles of the sides), and its 25 points; cells of s pixels in a frame of f; (5 s + 2 f)
 * squared */
static void paint_board(int s, int f, uint8_t *o)
{
    static const int8_t DIAG[6][4] = {{0, 0, 4, 4}, {0, 4, 4, 0},                       /* the long ones */
                                      {0, 2, 2, 4}, {2, 4, 4, 2}, {4, 2, 2, 0}, {2, 0, 0, 2}};   /* the diamond */
    int8_t seg[16][4], pt[BC_POINTS][2];
    for (int i = 0; i < BC_SIDE; i++) {
        const int8_t row[4] = {0, (int8_t)i, 4, (int8_t)i}, col[4] = {(int8_t)i, 0, (int8_t)i, 4};
        memcpy(seg[i], row, 4);
        memcpy(seg[BC_SIDE + i], col, 4);
    }
    memcpy(seg[2 * BC_SIDE], DIAG, sizeof DIAG);
    for (int i = 0; i < BC_POINTS; i++) pt[i][0] = (int8_t)bc_x(i), pt[i][1] = (int8_t)bc_y(i);
    lines_paint(BC_SIDE, s, f, (const int8_t (*)[4])seg, 16, (const int8_t (*)[2])pt, BC_POINTS,
                fmaxf(2.0f, 0.045f * s), fmaxf(1.6f, 0.082f * s), o);
}

/* mirrors a polygon's points left to right (the other half of a face) */
static void mirror(const float *xy, int n, float *out)
{
    for (int i = 0; i < n; i++) out[2 * i] = 1 - xy[2 * i], out[2 * i + 1] = xy[2 * i + 1];
}

static void poly2(art_fig *f, int role, const float *xy, int n)   /* a polygon and its mirror image */
{
    float m[2 * 24];
    art_poly(f, role, xy, n);
    mirror(xy, n, m);
    art_poly(f, role, m, n);
}

/* a tiger: an ebony disc, a tiger's head on it, seen from the front: orange, its stripes black, its cheeks and
 * muzzle white; size x size */
static void paint_tiger(int size, uint8_t *rgba)
{
    art_counter(COUNTER_DISC, 1, size, rgba);
    float line = fmaxf(0.014f, 1.0f / size);
    art_style fur = {{226, 128, 44}, {28, 16, 10}, {120, 60, 22}, {30, 18, 12}, 0.35f, line};
    art_fig *f = art_fig_new();
    const float ear[] = {0.285f, 0.375f, 0.27f, 0.29f, 0.30f, 0.235f, 0.355f, 0.225f, 0.40f, 0.27f};
    const float ear_in[] = {0.30f, 0.33f, 0.295f, 0.28f, 0.33f, 0.255f, 0.365f, 0.265f};
    const float head[] = {0.50f, 0.285f, 0.60f, 0.295f, 0.68f, 0.34f, 0.72f, 0.42f, 0.775f, 0.50f,
                          0.735f, 0.53f, 0.765f, 0.585f, 0.70f, 0.60f, 0.66f, 0.67f, 0.58f, 0.725f,
                          0.50f, 0.74f, 0.42f, 0.725f, 0.34f, 0.67f, 0.30f, 0.60f, 0.235f, 0.585f,
                          0.265f, 0.53f, 0.225f, 0.50f, 0.28f, 0.42f, 0.32f, 0.34f, 0.40f, 0.295f};
    poly2(f, ART_PART, ear, 5);
    poly2(f, ART_MARK, ear_in, 4);
    art_poly(f, ART_PART, head, 20);
    /* the stripes: three on the forehead, two on each cheek */
    const float s1[] = {0.485f, 0.305f, 0.515f, 0.305f, 0.505f, 0.385f, 0.495f, 0.385f};
    const float s2[] = {0.425f, 0.315f, 0.45f, 0.31f, 0.465f, 0.37f, 0.455f, 0.375f};
    const float c1[] = {0.245f, 0.50f, 0.30f, 0.475f, 0.355f, 0.49f, 0.30f, 0.505f};
    const float c2[] = {0.255f, 0.565f, 0.31f, 0.54f, 0.35f, 0.55f, 0.305f, 0.57f};
    art_poly(f, ART_MARK, s1, 4);
    poly2(f, ART_MARK, s2, 4);
    poly2(f, ART_MARK, c1, 4);
    poly2(f, ART_MARK, c2, 4);
    art_fig_blend(rgba, size, size, f, &fur, 0, 0, size, 1);
    art_fig_free(f);
    art_style white = {{246, 238, 222}, {28, 16, 10}, {150, 120, 96}, {24, 14, 10}, 0.2f, 0};
    f = art_fig_new();                          /* the white round the eyes and the muzzle; the eyes, the nose */
    const float brow[] = {0.355f, 0.43f, 0.40f, 0.405f, 0.445f, 0.43f, 0.43f, 0.465f, 0.37f, 0.465f};
    poly2(f, ART_PART, brow, 5);
    art_ellipse(f, ART_PART, 0.455f, 0.615f, 0.062f, 0.05f);
    art_ellipse(f, ART_PART, 0.545f, 0.615f, 0.062f, 0.05f);
    art_ellipse(f, ART_PART, 0.50f, 0.665f, 0.05f, 0.04f);
    art_ellipse(f, ART_MARK, 0.405f, 0.47f, 0.03f, 0.022f);
    art_ellipse(f, ART_MARK, 0.595f, 0.47f, 0.03f, 0.022f);
    const float nose[] = {0.455f, 0.555f, 0.545f, 0.555f, 0.50f, 0.605f};
    art_poly(f, ART_MARK, nose, 3);
    art_fig_blend(rgba, size, size, f, &white, 0, 0, size, 1);
    art_fig_free(f);
    art_style amber = {{240, 196, 60}, {0, 0, 0}, {0, 0, 0}, {18, 12, 8}, 0.1f, 0};
    f = art_fig_new();                          /* the eyes' amber, their pupils */
    art_ellipse(f, ART_PART, 0.405f, 0.47f, 0.022f, 0.016f);
    art_ellipse(f, ART_PART, 0.595f, 0.47f, 0.022f, 0.016f);
    art_ellipse(f, ART_MARK, 0.405f, 0.47f, 0.008f, 0.014f);
    art_ellipse(f, ART_MARK, 0.595f, 0.47f, 0.008f, 0.014f);
    art_fig_blend(rgba, size, size, f, &amber, 0, 0, size, 1);
    art_fig_free(f);
}

/* a goat: an ivory disc, a goat's head on it, seen from the front: a long brown face, the horns curving up and out,
 * the ears hanging sideways, a beard; size x size */
static void paint_goat(int size, uint8_t *rgba)
{
    art_counter(COUNTER_DISC, 0, size, rgba);
    float line = fmaxf(0.014f, 1.0f / size);
    art_style horn = {{126, 106, 86}, {34, 24, 16}, {80, 64, 50}, {0, 0, 0}, 0.4f, line};
    art_fig *f = art_fig_new();
    const float h[] = {0.445f, 0.355f, 0.415f, 0.285f, 0.375f, 0.225f, 0.325f, 0.19f, 0.30f, 0.20f,
                       0.345f, 0.24f, 0.375f, 0.30f, 0.39f, 0.365f};
    poly2(f, ART_PART, h, 8);
    art_fig_blend(rgba, size, size, f, &horn, 0, 0, size, 1);
    art_fig_free(f);
    art_style coat = {{186, 140, 96}, {34, 22, 14}, {110, 76, 48}, {28, 18, 12}, 0.3f, line};
    f = art_fig_new();
    const float ear[] = {0.40f, 0.395f, 0.33f, 0.40f, 0.255f, 0.43f, 0.22f, 0.47f, 0.26f, 0.49f,
                         0.33f, 0.47f, 0.405f, 0.445f};
    const float beard[] = {0.455f, 0.69f, 0.545f, 0.69f, 0.53f, 0.765f, 0.50f, 0.81f, 0.47f, 0.765f};
    const float face[] = {0.50f, 0.325f, 0.575f, 0.335f, 0.615f, 0.38f, 0.62f, 0.45f, 0.595f, 0.55f,
                          0.575f, 0.64f, 0.56f, 0.70f, 0.50f, 0.725f, 0.44f, 0.70f, 0.425f, 0.64f,
                          0.405f, 0.55f, 0.38f, 0.45f, 0.385f, 0.38f, 0.425f, 0.335f};
    poly2(f, ART_PART, ear, 7);
    art_poly(f, ART_PART, beard, 5);
    art_poly(f, ART_PART, face, 14);
    art_ellipse(f, ART_MARK, 0.44f, 0.475f, 0.034f, 0.026f);   /* the eyes */
    art_ellipse(f, ART_MARK, 0.56f, 0.475f, 0.034f, 0.026f);
    art_ellipse(f, ART_MARK, 0.47f, 0.665f, 0.013f, 0.01f);
    art_ellipse(f, ART_MARK, 0.53f, 0.665f, 0.013f, 0.01f);
    art_fig_blend(rgba, size, size, f, &coat, 0, 0, size, 1);
    art_fig_free(f);
    art_style pale = {{236, 216, 184}, {0, 0, 0}, {0, 0, 0}, {28, 18, 12}, 0.15f, 0};
    f = art_fig_new();                          /* a pale blaze down the face */
    const float blaze[] = {0.50f, 0.36f, 0.525f, 0.42f, 0.52f, 0.58f, 0.50f, 0.62f, 0.48f, 0.58f, 0.475f, 0.42f};
    art_poly(f, ART_PART, blaze, 6);
    art_fig_blend(rgba, size, size, f, &pale, 0, 0, size, 1);
    art_fig_free(f);
    art_style amber = {{232, 184, 70}, {0, 0, 0}, {0, 0, 0}, {20, 14, 10}, 0.1f, 0};
    f = art_fig_new();                          /* the eyes' amber, a goat's flat pupils */
    art_ellipse(f, ART_PART, 0.44f, 0.475f, 0.024f, 0.017f);
    art_ellipse(f, ART_PART, 0.56f, 0.475f, 0.024f, 0.017f);
    art_rect(f, ART_MARK, 0.418f, 0.47f, 0.462f, 0.48f);
    art_rect(f, ART_MARK, 0.538f, 0.47f, 0.582f, 0.48f);
    art_fig_blend(rgba, size, size, f, &amber, 0, 0, size, 1);
    art_fig_free(f);
}

static void paint_piece(int who, int size, uint8_t *rgba)
{
    if (who == BC_TIGER) paint_tiger(size, rgba);
    else paint_goat(size, rgba);
}

static void layout(box b)
{
    int changed = lines_fit(&L, b), n = (int)lines_side(&L);
    if (changed) {
        ui_free(&t_board);
        paint_board(L.sq, L.fr, ui_buf(n, n));
        t_board = ui_tex(n, n, ui_buf(n, n));
        int d = (int)(DISC * L.sq);
        for (int c = 0; c < 2; c++) {
            ui_free(&t_piece[c]);
            paint_piece(c, d, ui_buf(d, d));
            t_piece[c] = ui_tex(d, d, ui_buf(d, d));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small[BC_GOAT]);
        paint_goat(small, ui_buf(small, small));
        t_small[BC_GOAT] = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void pt_xy(int pt, float *x, float *y) { lines_xy(&L, bc_x(pt), bc_y(pt), x, y); }

static int xy_pt(float x, float y)              /* the point whose cell holds (x, y); -1 */
{
    int gx, gy;
    return lines_at(&L, x, y, &gx, &gy) ? gx + BC_SIDE * gy : -1;
}

/* the heads look at their owner: face to face, the top player's are turned round */
static void piece_at(int who, float cx, float cy, float d, float alpha)
{
    ui_flat_at(t_piece[who], cx, cy, d, alpha, ui_upside(who));
}

static void draw(void)
{
    const bcpos *p = bc_pos(&G);
    float n = lines_side(&L), d = DISC * L.sq, x, y;
    int playing = status() == GS_PLAY;
    ui_put(t_board, L.x - L.fr, L.y - L.fr, n, n);
    if (G.ply) {
        bcmove m = G.move[G.ply - 1];
        if (m.from >= 0) lines_mark_last(&L, bc_x(m.from), bc_y(m.from));
        lines_mark_last(&L, bc_x(m.to), bc_y(m.to));
    }
    if (sel >= 0) lines_mark_select(&L, bc_x(sel), bc_y(sel));
    float t = 1;
    if (anim.on) {
        int ms = anim.m.from < 0 ? PLACE_MS : anim.m.over >= 0 ? JUMP_MS : STEP_MS;
        t = (ui_ms() - anim.t0) / (float)ms;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    for (int i = 0; i < BC_POINTS; i++) {
        if (p->at[i] < 0 || (drag.on && drag.moved && i == drag.from) || (anim.on && i == anim.m.to)) continue;
        pt_xy(i, &x, &y);
        piece_at(p->at[i], x, y, d, 1);
    }
    for (int i = 0; playing && i < nlegal; i++) {   /* where a goat can be set down, the goats the tigers can take,
                                                     * where the picked-up piece can go */
        const bcmove *m = &legal[i];
        if (m->over >= 0) {
            pt_xy(m->over, &x, &y);
            ui_mark_ring(x, y, 0.92f * L.sq);
        }
        if (m->from < 0 || m->from == sel) {
            pt_xy(m->to, &x, &y);
            ui_mark_dot(x, y, (m->from < 0 ? 0.22f : 0.26f) * L.sq);
        }
    }
    if (anim.on) {
        float e = 1 - (1 - t) * (1 - t) * (1 - t), x0, y0;
        pt_xy(anim.m.to, &x, &y);
        if (anim.m.from < 0) {                  /* a goat set down: from a little above the board */
            piece_at(anim.who, x, y, d * (1.2f - 0.2f * e), 1);
        } else {
            if (anim.m.over >= 0 && t < FADE_MS / (float)JUMP_MS) {   /* the goat taken: lifted, fading away */
                float k = t * JUMP_MS / FADE_MS, ox, oy;
                pt_xy(anim.m.over, &ox, &oy);
                piece_at(BC_GOAT, ox, oy, d * (1 + 0.2f * k), 1 - k);
            }
            float hop = anim.m.over >= 0 ? 1 + 0.16f * sinf(3.14159265f * t) : 1;   /* a jump: up and over */
            pt_xy(anim.m.from, &x0, &y0);
            piece_at(anim.who, x0 + (x - x0) * e, y0 + (y - y0) * e, d * hop, 1);
        }
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * d, off = ui_upside(p->side) ? -0.55f : 0.55f;
        piece_at(p->side, drag.x, drag.y - s * off, s, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    const bcpos *p = bc_pos(&G);
    int pt = xy_pt(x, y), i;
    if (placing()) {                            /* a goat set down: one tap */
        if (pt < 0) return;
        if ((i = find(-1, pt)) >= 0) play(legal[i], 1);
        else ui_sound(SND_DENY);
        return;
    }
    if (pt < 0) { sel = -1; return; }
    if (sel >= 0 && (i = find(sel, pt)) >= 0) {
        play(legal[i], 1);
        return;
    }
    if (p->side == BC_TIGER && p->at[pt] == BC_GOAT && (i = find_over(sel, pt)) >= 0) {
        play(legal[i], 1);
        return;
    }
    if (p->at[pt] == p->side && can_move(pt)) {
        sel = pt;
        drag.on = 1;
        drag.from = pt;
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
    if (hypotf(x - drag.x, y - drag.y) > L.sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays picked up */
    int pt = xy_pt(x, y), i = pt >= 0 ? find(drag.from, pt) : -1;
    if (i >= 0) play(legal[i], 0);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the goats: how many are still to come; the tigers: how many goats they took (5 win) */
static void bar(int player, box b)
{
    const bcpos *p = bc_pos(&G);
    ui_bar_count(b, t_small[BC_GOAT], player == BC_GOAT ? bc_to_place(p) : p->taken);
}

static void token(int player, int size, uint8_t *rgba) { paint_piece(player, size, rgba); }

/* the menu's card: the board, the tigers among the goats, one about to jump */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (BC_SIDE + 2 * LINES_FRAME)), f = (int)(s * LINES_FRAME), n = BC_SIDE * s + 2 * f;
    int o = (size - n) / 2, d = (int)(0.86f * s);   /* a little larger than on the board: a small card */
    uint8_t *board = ui_buf(n, n);
    paint_board(s, f, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    static const char place[][3] = {"a5", "e5", "c3", "e1", "b2", "c2", "d2", "b3", "d3", "c4", "a1", "b1"};
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        int pt = bc_point(place[i]), who = i < 4 ? BC_TIGER : BC_GOAT;
        float cx = o + lines_px(bc_x(pt), (float)s, (float)f), cy = o + lines_py(BC_SIDE, bc_y(pt), (float)s, (float)f);
        uint8_t *one = ui_buf(d, d);
        paint_piece(who, d, one);
        art_flat_put(rgba, size, size, one, d, cx, cy);
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last move to finish */

static const game baghchal_game = {
    .id = "baghchal", .order = 125, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(baghchal_game)
