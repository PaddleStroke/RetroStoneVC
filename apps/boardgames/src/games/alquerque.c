/*
 * Alquerque in RetroStone Board: the 5 x 5 board of lines on one plank (lines.h), the discs, tap or drag, the
 * captures one jump at a time (the rules and the notation: alquerque_rules.c, alquerque.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>

#include "games/alquerque.h"
#include "games/lines.h"
#include "shell.h"

#define DISC 0.7f                               /* a disc's size, in cells */
#define STEP_MS 150
#define JUMP_MS 230
#define FADE_MS 240

static aqgame G;
static aqmove legal[AQ_MAX_MOVES];
static int nlegal;
static lines_geo L = {.cells = AQ_SIDE};        /* the plank: a point in the middle of each of its 5 x 5 cells */
static int made_small = -1;
static SDL_Texture *t_disc[2], *t_small[2], *t_board;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, who; aqmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int status(void)
{
    switch (aq_status(&G)) {
    case AS_PLAY: return GS_PLAY;
    case AS_STUCK: return aq_pos(&G)->side ? GS_WIN0 : GS_WIN1;
    default: return GS_DRAW;
    }
}

/* a piece in the middle of a chain of captures is already picked up */
static void refresh(void)
{
    const aqpos *p = aq_pos(&G);
    nlegal = aq_legal(p, legal);
    sel = p->chain;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    aq_new(&G);
    anim.on = 0;
    refresh();
}

static int turn(void) { return aq_pos(&G)->side; }

static int find(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static int find_over(int from, int over)        /* the capture of a piece (a tap on the piece itself) */
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].over == over) return i;
    return -1;
}

static int can_move(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from) return 1;
    return 0;
}

static void play(aqmove m, int animate)
{
    anim.who = aq_pos(&G)->side;
    if (!aq_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.over >= 0 ? SND_CAPTURE : SND_MOVE);
}

static int undo(void)
{
    if (!aq_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "c2-c3 b4-c3 d2xb4": the screenshots' and the tests' moves (alquerque.h) */
static int script(const char *s)
{
    int ok = aq_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank with its lines (each row, column and diagonal whole, from end to end) and its 25 points; cells of s
 * pixels in a frame of f; (5 s + 2 f) squared */
static void paint_board(int s, int f, uint8_t *o)
{
    int8_t seg[16][4], pt[AQ_POINTS][2];
    int nseg = 0;
    for (int i = 0; i < AQ_POINTS; i++) {
        pt[i][0] = (int8_t)aq_x(i), pt[i][1] = (int8_t)aq_y(i);
        for (int d = 0; d < 4; d++) {           /* N, NE, E, SE: a line starting here (nothing behind it) */
            if (aq_next(i, d) < 0 || aq_next(i, d + 4) >= 0) continue;
            int j = i;
            while (aq_next(j, d) >= 0) j = aq_next(j, d);
            seg[nseg][0] = (int8_t)aq_x(i), seg[nseg][1] = (int8_t)aq_y(i);
            seg[nseg][2] = (int8_t)aq_x(j), seg[nseg][3] = (int8_t)aq_y(j);
            nseg++;
        }
    }
    /* the lines and dots as wide as morris's and fox and geese's on their 7 x 7 planks */
    lines_paint(AQ_SIDE, s, f, (const int8_t (*)[4])seg, nseg, (const int8_t (*)[2])pt, AQ_POINTS,
                fmaxf(2.0f, 0.045f * s), fmaxf(1.6f, 0.082f * s), o);
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
            ui_free(&t_disc[c]);
            art_counter(COUNTER_DISC, c, d, ui_buf(d, d));
            t_disc[c] = ui_tex(d, d, ui_buf(d, d));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_small[c]);
            art_counter(COUNTER_DISC, c, small, ui_buf(small, small));
            t_small[c] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void pt_xy(int pt, float *x, float *y) { lines_xy(&L, aq_x(pt), aq_y(pt), x, y); }

static int xy_pt(float x, float y)              /* the point whose cell holds (x, y); -1 */
{
    int gx, gy;
    return lines_at(&L, x, y, &gx, &gy) ? gx + AQ_SIDE * gy : -1;
}

static void disc_at(int who, float cx, float cy, float d, float alpha) { ui_flat_at(t_disc[who], cx, cy, d, alpha, 0); }

static void draw(void)
{
    const aqpos *p = aq_pos(&G);
    float n = lines_side(&L), d = DISC * L.sq, x, y;
    ui_put(t_board, L.x - L.fr, L.y - L.fr, n, n);
    if (G.ply) {                                /* the last turn: a step, or every jump of its chain */
        int side = G.pos[G.ply - 1].side;
        for (int k = G.ply - 1; k >= 0 && G.pos[k].side == side; k--) {
            lines_mark_last(&L, aq_x(G.move[k].from), aq_y(G.move[k].from));
            lines_mark_last(&L, aq_x(G.move[k].to), aq_y(G.move[k].to));
        }
    }
    if (sel >= 0) lines_mark_select(&L, aq_x(sel), aq_y(sel));
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(anim.m.over >= 0 ? JUMP_MS : STEP_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    for (int i = 0; i < AQ_POINTS; i++) {
        if (p->at[i] < 0 || (drag.on && drag.moved && i == drag.from) || (anim.on && i == anim.m.to)) continue;
        pt_xy(i, &x, &y);
        disc_at(p->at[i], x, y, d, 1);
    }
    if (status() == GS_PLAY)
        for (int i = 0; i < nlegal; i++) {      /* the pieces that can be taken, where the selected piece can go */
            if (legal[i].over >= 0) {
                pt_xy(legal[i].over, &x, &y);
                ui_mark_ring(x, y, 0.92f * L.sq);
            }
            if (legal[i].from == sel) {
                pt_xy(legal[i].to, &x, &y);
                ui_mark_dot(x, y, 0.26f * L.sq);
            }
        }
    if (anim.on) {
        if (anim.m.over >= 0 && t < FADE_MS / (float)JUMP_MS) {   /* the piece taken: lifted, fading away */
            float k = t * JUMP_MS / FADE_MS;
            pt_xy(anim.m.over, &x, &y);
            disc_at(!anim.who, x, y, d * (1 + 0.2f * k), 1 - k);
        }
        float x0, y0, e = 1 - (1 - t) * (1 - t) * (1 - t);
        float hop = anim.m.over >= 0 ? 1 + 0.16f * sinf(3.14159265f * t) : 1;   /* a jump: up and over */
        pt_xy(anim.m.from, &x0, &y0);
        pt_xy(anim.m.to, &x, &y);
        disc_at(anim.who, x0 + (x - x0) * e, y0 + (y - y0) * e, d * hop, 1);
    }
    if (drag.on && drag.moved) {                /* the dragged disc, a little bigger, beside the finger */
        float s = 1.18f * d, off = ui_upside(p->side) ? -0.55f : 0.55f;
        disc_at(p->side, drag.x, drag.y - s * off, s, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    const aqpos *p = aq_pos(&G);
    int pt = xy_pt(x, y), i;
    if (pt < 0) {
        if (p->chain < 0) sel = -1;
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
    if (p->chain < 0) sel = -1;
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
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int pt = xy_pt(x, y), i = pt >= 0 ? find(drag.from, pt) : -1;
    if (i >= 0) play(legal[i], 0);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces the player took, side by side */
static void bar(int player, box b)
{
    float sz = 0.6f * ui.unit, x = b.x, cy = b.y + b.h / 2, end = b.x + b.w;
    int took = AQ_PIECES - aq_pos(&G)->left[!player];
    for (int i = 0; i < took && x + sz <= end; i++, x += 0.36f * ui.unit)
        ui_put(t_small[!player], x, cy - sz / 2, sz, sz);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card: the board at the start, its middle point empty */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (AQ_SIDE + 2 * LINES_FRAME)), f = (int)(s * LINES_FRAME), n = AQ_SIDE * s + 2 * f;
    int o = (size - n) / 2, d = (int)(DISC * s);
    uint8_t *board = ui_buf(n, n);
    paint_board(s, f, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    aqpos p;
    aq_start(&p);
    for (int i = 0; i < AQ_POINTS; i++) {
        if (p.at[i] < 0) continue;
        float cx = o + lines_px(aq_x(i), (float)s, (float)f), cy = o + lines_py(AQ_SIDE, aq_y(i), (float)s, (float)f);
        uint8_t *one = ui_buf(d, d);
        art_flat_shadow(d, one);
        art_blend(rgba, size, size, one, d, d, (int)(cx + 0.06f * d - d / 2.0f), (int)(cy + 0.1f * d - d / 2.0f));
        art_counter(COUNTER_DISC, p.at[i], d, one);
        art_blend(rgba, size, size, one, d, d, (int)(cx - d / 2.0f + 0.5f), (int)(cy - d / 2.0f + 0.5f));
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last move to finish */

static const game alquerque_game = {
    .id = "alquerque", .order = 240, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(alquerque_game)
