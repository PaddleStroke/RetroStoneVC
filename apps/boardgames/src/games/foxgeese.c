/*
 * Fox and geese in RetroStone Board: the cross of lines on one plank, the geese (ivory discs) and the fox (an ebony
 * disc with a fox's head), tap or drag, the jumps one at a time (the rules and the notation: foxgeese_rules.c,
 * foxgeese.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>

#include "games/foxgeese.h"
#include "games/lines.h"
#include "shell.h"

#define DISC 0.8f                               /* a piece's size, in cells */
#define STEP_MS 150
#define JUMP_MS 230
#define FADE_MS 240

static fggame G;
static fgmove legal[FG_MAX_MOVES];
static int nlegal;
static lines_geo L = {.cells = 7};              /* the plank: 7 x 7 cells, the cross's points in 33 of them */
static int made_small = -1;
static SDL_Texture *t_piece[2], *t_small, *t_board;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; fgmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int status(void)
{
    switch (fg_status(&G)) {
    case FS_PLAY: return GS_PLAY;
    case FS_TRAPPED: return GS_WIN0;
    case FS_FEW: case FS_STUCK: return GS_WIN1;
    default: return GS_DRAW;
    }
}

/* the fox has one piece: on its turn it is already picked up */
static void refresh(void)
{
    const fgpos *p = fg_pos(&G);
    nlegal = fg_legal(p, legal);
    sel = p->side == FG_FOX && status() == GS_PLAY ? p->fox : -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    fg_new(&G);
    anim.on = 0;
    refresh();
}

static int turn(void) { return fg_pos(&G)->side; }

static int find(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static int find_over(int from, int over)        /* the jump over a goose (a tap on the goose itself) */
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].over == over) return i;
    return -1;
}

static void play(fgmove m, int animate)
{
    if (!fg_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.over >= 0 ? SND_CAPTURE : SND_MOVE);
}

static int undo(void)
{
    if (!fg_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "c3-c4 d4-c5 c4-b4 d6xd4xd2": the screenshots' and the tests' moves (foxgeese.h) */
static int script(const char *s)
{
    int ok = fg_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank with the cross: its lines (between neighbours, each once) and points; cells of s pixels in a frame of f;
 * (7 s + 2 f) squared */
static void paint_board(int s, int f, uint8_t *o)
{
    int8_t seg[4 * FG_POINTS][4], pt[FG_POINTS][2];
    int nseg = 0;
    for (int i = 0; i < FG_POINTS; i++) {
        pt[i][0] = fg_x[i], pt[i][1] = fg_y[i];
        for (int d = 0; d < 4; d++) {           /* N, NE, E, SE */
            int j = fg_next[i][d];
            if (j < 0) continue;
            seg[nseg][0] = fg_x[i], seg[nseg][1] = fg_y[i], seg[nseg][2] = fg_x[j], seg[nseg][3] = fg_y[j];
            nseg++;
        }
    }
    lines_paint(7, s, f, (const int8_t (*)[4])seg, nseg, (const int8_t (*)[2])pt, FG_POINTS,
                2 * fmaxf(1.0f, 0.03f * s), fmaxf(1.6f, 0.11f * s), o);
}

/* the fox: an ebony disc with a copper fox's head on it, its muzzle pale, size x size */
static void paint_fox(int size, uint8_t *rgba)
{
    art_counter(COUNTER_DISC, 1, size, rgba);
    uint8_t *head = malloc((size_t)size * size * 4);
    if (!head) return;
    art_style st = {{214, 118, 50}, {36, 20, 12}, {120, 58, 24}, {44, 22, 12}, 0.35f, fmaxf(0.016f, 1.1f / size)};
    art_fig *f = art_fig_new();
    const float ear_l[] = {0.29f, 0.47f, 0.30f, 0.23f, 0.46f, 0.36f}, ear_r[] = {0.71f, 0.47f, 0.70f, 0.23f,
                                                                                  0.54f, 0.36f};
    const float in_l[] = {0.32f, 0.40f, 0.325f, 0.29f, 0.40f, 0.35f}, in_r[] = {0.68f, 0.40f, 0.675f, 0.29f,
                                                                               0.60f, 0.35f};
    const float face[] = {0.25f, 0.43f, 0.37f, 0.35f, 0.50f, 0.335f, 0.63f, 0.35f, 0.75f, 0.43f,
                          0.67f, 0.56f, 0.56f, 0.66f, 0.50f, 0.74f, 0.44f, 0.66f, 0.33f, 0.56f};
    art_poly(f, ART_PART, ear_l, 3);
    art_poly(f, ART_PART, ear_r, 3);
    art_poly(f, ART_MARK, in_l, 3);
    art_poly(f, ART_MARK, in_r, 3);
    art_poly(f, ART_PART, face, 10);
    art_render(f, &st, size, size, (float)size, 0, 0, head);
    art_blend(rgba, size, size, head, size, size, 0, 0);
    art_fig_free(f);
    art_style pale = {{244, 232, 210}, {36, 20, 12}, {120, 58, 24}, {30, 18, 12}, 0.2f, 0};
    f = art_fig_new();                          /* the pale muzzle, the eyes and the nose */
    const float muzzle[] = {0.34f, 0.565f, 0.43f, 0.53f, 0.50f, 0.56f, 0.57f, 0.53f, 0.66f, 0.565f,
                            0.56f, 0.655f, 0.50f, 0.725f, 0.44f, 0.655f};
    art_poly(f, ART_PART, muzzle, 8);
    art_ellipse(f, ART_MARK, 0.405f, 0.475f, 0.035f, 0.024f);
    art_ellipse(f, ART_MARK, 0.595f, 0.475f, 0.035f, 0.024f);
    art_ellipse(f, ART_MARK, 0.50f, 0.705f, 0.035f, 0.028f);
    art_render(f, &pale, size, size, (float)size, 0, 0, head);
    art_blend(rgba, size, size, head, size, size, 0, 0);
    art_fig_free(f);
    free(head);
}

static void paint_piece(int who, int size, uint8_t *rgba)
{
    if (who == FG_FOX) paint_fox(size, rgba);
    else art_counter(COUNTER_DISC, 0, size, rgba);
}

static void layout(box b)
{
    int changed = lines_fit(&L, b), n = (int)lines_side(&L);
    if (changed) {
        fg_init();
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
        ui_free(&t_small);
        art_counter(COUNTER_DISC, 0, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void pt_xy(int pt, float *x, float *y)  /* a point's centre */
{
    lines_xy(&L, fg_x[pt], fg_y[pt], x, y);
}

static int xy_pt(float x, float y)              /* the point whose cell holds (x, y); -1 */
{
    int gx, gy;
    if (!lines_at(&L, x, y, &gx, &gy)) return -1;
    for (int i = 0; i < FG_POINTS; i++)
        if (fg_x[i] == gx && fg_y[i] == gy) return i;
    return -1;
}

static void piece_at(int who, float cx, float cy, float d, float alpha)
{
    ui_flat_at(t_piece[who], cx, cy, d, alpha, who == FG_FOX && ui_upside(FG_FOX));
}

static void mark_last(int pt) { lines_mark_last(&L, fg_x[pt], fg_y[pt]); }

static void draw(void)
{
    const fgpos *p = fg_pos(&G);
    float n = lines_side(&L), d = DISC * L.sq, x, y;
    ui_put(t_board, L.x - L.fr, L.y - L.fr, n, n);
    if (G.ply) {                                /* the last turn: a step, or every jump of the fox's chain */
        int side = G.pos[G.ply - 1].side;
        for (int k = G.ply - 1; k >= 0 && G.pos[k].side == side; k--) {
            mark_last(G.move[k].from);
            mark_last(G.move[k].to);
        }
    }
    if (sel >= 0) lines_mark_select(&L, fg_x[sel], fg_y[sel]);
    float t = 1;
    if (anim.on) {
        int ms = anim.m.over >= 0 ? JUMP_MS : STEP_MS;
        t = (ui_ms() - anim.t0) / (float)ms;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    float e = 1 - (1 - t) * (1 - t) * (1 - t);
    for (int i = 0; i < FG_POINTS; i++) {
        if (p->at[i] < 0 || (drag.on && drag.moved && i == drag.from) || (anim.on && i == anim.m.to)) continue;
        pt_xy(i, &x, &y);
        piece_at(p->at[i], x, y, d, 1);
    }
    if (status() == GS_PLAY)
        for (int i = 0; i < nlegal; i++) {      /* the geese the fox can take, where the picked-up piece can go */
            const fgmove *m = &legal[i];
            if (m->over >= 0) {
                pt_xy(m->over, &x, &y);
                ui_mark_ring(x, y, (float)L.sq);
            }
            if (m->from == sel) {
                pt_xy(m->to, &x, &y);
                ui_mark_dot(x, y, 0.32f * L.sq);
            }
        }
    if (anim.on) {
        float x0, y0;
        if (anim.m.over >= 0 && t < FADE_MS / (float)JUMP_MS) {   /* the goose taken: lifted and fading away */
            float f = t * JUMP_MS / FADE_MS;
            pt_xy(anim.m.over, &x, &y);
            piece_at(FG_GOOSE, x, y, d * (1 + 0.2f * f), 1 - f);
        }
        pt_xy(anim.m.from, &x0, &y0);
        pt_xy(anim.m.to, &x, &y);
        float hop = anim.m.over >= 0 ? 1 + 0.16f * sinf(3.14159265f * t) : 1;   /* a jump: up and over */
        piece_at(p->at[anim.m.to], x0 + (x - x0) * e, y0 + (y - y0) * e, d * hop, 1);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * d, off = ui_upside(p->side) ? -0.55f : 0.55f;
        piece_at(p->at[drag.from], drag.x, drag.y - s * off, s, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int can_move(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from) return 1;
    return 0;
}

static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    const fgpos *p = fg_pos(&G);
    int pt = xy_pt(x, y), i;
    if (pt < 0) {
        if (p->side == FG_GOOSE) sel = -1;
        return;
    }
    if (sel >= 0 && ((i = find(sel, pt)) >= 0 || (i = find_over(sel, pt)) >= 0)) {
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
    if (p->side == FG_GOOSE) sel = -1;
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
/* the geese: those left, a stack; the fox: the geese it took, side by side */
static void bar(int player, box b)
{
    const fgpos *p = fg_pos(&G);
    float sz = 0.6f * ui.unit, x = b.x, cy = b.y + b.h / 2, end = b.x + b.w;
    int n = player == FG_GOOSE ? p->geese : FG_GEESE - p->geese;
    float step = (player == FG_GOOSE ? 0.19f : 0.4f) * ui.unit;
    for (int i = 0; i < n && x + sz <= end; i++, x += step) ui_put(t_small, x, cy - sz / 2, sz, sz);
}

static void token(int player, int size, uint8_t *rgba) { paint_piece(player, size, rgba); }

/* the menu's card: the cross, the fox among the geese */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    fg_init();
    int s = (int)(size / (7 + 2 * LINES_FRAME)), f = (int)(s * LINES_FRAME), n = 7 * s + 2 * f, o = (size - n) / 2;
    int d = (int)(DISC * s), sh = d;
    uint8_t *board = ui_buf(n, n);
    paint_board(s, f, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    static const char place[][3] = {"d4", "c2", "d2", "e2", "a3", "b3", "c3", "f3", "g3", "c4", "e4", "b5", "f4"};
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        int pt = fg_point(place[i]), who = i ? FG_GOOSE : FG_FOX;
        float cx = o + f + (fg_x[pt] + 0.5f) * s, cy = o + f + (6 - fg_y[pt] + 0.5f) * s;
        uint8_t *one = ui_buf(d, d);
        art_flat_shadow(sh, one);
        art_blend(rgba, size, size, one, sh, sh, (int)(cx + 0.06f * d - sh / 2.0f), (int)(cy + 0.1f * d - sh / 2.0f));
        paint_piece(who, d, one);
        art_blend(rgba, size, size, one, d, d, (int)(cx - d / 2.0f + 0.5f), (int)(cy - d / 2.0f + 0.5f));
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last move to finish */

static const game foxgeese_game = {
    .id = "foxgeese", .order = 120, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(foxgeese_game)
