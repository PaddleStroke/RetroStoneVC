/*
 * Fanorona in RetroStone Board: the 9 x 5 board of lines on one plank (lines.h's ink, its bands and dots, on a plank
 * longer than it is wide), the discs, tap or drag, captures by approach or withdrawal one step at a time (the rules
 * and the notation: fanorona_rules.c, fanorona.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A piece selected shows where it can go (dots) and every piece each of its steps would take (rings): a tap on a dot,
 * or on a ringed piece, plays. A step that could take either of two lines (approach or withdrawal) puts the piece on
 * its new point and rings both lines: a tap on one takes it (a tap anywhere else puts the piece back). In a chain of
 * captures the piece stays picked up; the pass button in the bar, or a tap on the piece itself, ends the turn.
 */
#include <math.h>

#include "games/fanorona.h"
#include "games/lines.h"
#include "games/trays.h"
#include "shell.h"

#define DISC 0.7f                               /* a disc's size, in cells */
#define STEP_MS 170                             /* the step's slide */
#define FADE_MS 220                             /* each piece taken: lifted, fading away */
#define FADE_GAP 55                             /* one after the other along the line */

/* the plank: 9 x 5 cells (a point in the middle of each), a frame of LINES_FRAME cells */
static lines_geo P = {.cells = FN_COLS, .rows = FN_ROWS};
static trays T;                                 /* the pieces each player took, by their side */
static fngame G;
static fnmove legal[FN_MAX_MOVES];
static int nlegal, made_small = -1;
static SDL_Texture *t_disc[2], *t_small[2], *t_board;
static int sel = -1;
static struct { int on, to, i[2]; } choice;     /* a step that approaches and withdraws: legal[i[0]], legal[i[1]] */
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, who, n; fnmove m; int8_t took[FN_MAX_TAKE]; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int status(void)
{
    switch (fn_status(&G)) {
    case FS_PLAY: return GS_PLAY;
    case FS_LOST: return fn_pos(&G)->side ? GS_WIN0 : GS_WIN1;
    default: return GS_DRAW;
    }
}

/* a piece in the middle of a chain of captures is already picked up */
static void refresh(void)
{
    nlegal = fn_legal(fn_pos(&G), legal);
    sel = fn_pos(&G)->chain;
    choice.on = drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    fn_new(&G);
    anim.on = 0;
    refresh();
}

static int turn(void) { return fn_pos(&G)->side; }

static int can_move(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from) return 1;
    return 0;
}

static int takes(int i, int pt)                 /* 1: legal[i] takes the piece on pt */
{
    int8_t t[FN_MAX_TAKE];
    int n = fn_taken(fn_pos(&G), legal[i], t);
    for (int k = 0; k < n; k++)
        if (t[k] == pt) return 1;
    return 0;
}

static void play(fnmove m, int animate)
{
    anim.who = fn_pos(&G)->side;
    anim.n = fn_taken(fn_pos(&G), m, anim.took);
    if (!fn_play(&G, m)) return;
    anim.on = animate && m.kind != FN_STOP;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    if (m.kind != FN_STOP) ui_sound(anim.n ? SND_CAPTURE : SND_MOVE);
}

/* the selected piece to pt (a tap, or dropped there): one step plays; approach or withdrawal: the choice; 0: none */
static int go(int pt, int animate)
{
    int found[2], n = 0;
    for (int i = 0; i < nlegal && n < 2; i++)
        if (legal[i].from == sel && legal[i].to == pt) found[n++] = i;
    if (n == 1) play(legal[found[0]], animate);
    if (n == 2) {
        choice.on = 1;
        choice.to = pt;
        choice.i[0] = found[0];
        choice.i[1] = found[1];
        ui_sound(SND_TAP);
    }
    return n;
}

static int can_pass(void) { return status() == GS_PLAY && fn_pos(&G)->chain >= 0; }

static void pass(void)                          /* the end of a chain of captures */
{
    const fnpos *p = fn_pos(&G);
    if (p->chain >= 0) play((fnmove){p->chain, p->chain, FN_STOP}, 0);
}

static int undo(void)
{
    if (!fn_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "d3-e3w d4-d3": the screenshots' and the tests' moves (fanorona.h) */
static int script(const char *s)
{
    int ok = fn_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank with its lines (each row, column and diagonal whole, from end to end) and its 45 points; cells of s
 * pixels in a frame of f: (9 s + 2 f) x (5 s + 2 f) */
static void paint_board(int s, int f, uint8_t *o)
{
    int8_t seg[48][4];
    int nseg = 0, w = FN_COLS * s + 2 * f;
    float S = (float)s, F = (float)f;
    art_board_n(FN_COLS, FN_ROWS, s, f, 0, o);
    for (int i = 0; i < FN_POINTS; i++)
        for (int d = 0; d < 4; d++) {           /* N, NE, E, SE: a line starting here (nothing behind it) */
            if (fn_next(i, d) < 0 || fn_next(i, d + 4) >= 0) continue;
            int j = i;
            while (fn_next(j, d) >= 0) j = fn_next(j, d);
            seg[nseg][0] = (int8_t)fn_x(i), seg[nseg][1] = (int8_t)fn_y(i);
            seg[nseg][2] = (int8_t)fn_x(j), seg[nseg][3] = (int8_t)fn_y(j);
            nseg++;
        }
    /* lines.h's bands and dots on the plank (wider than tall: everything lies inside it, the row length w its
     * stride); a little bolder than on alquerque's bigger cells */
    float lw = fmaxf(2.0f, 0.05f * s), r = fmaxf(1.6f, 0.09f * s);
    for (int i = 0; i < nseg; i++)
        lines_band(o, w, lines_px(seg[i][0], S, F), lines_py(FN_ROWS, seg[i][1], S, F), lines_px(seg[i][2], S, F),
                   lines_py(FN_ROWS, seg[i][3], S, F), lw, LINES_INK, 1);
    for (int i = 0; i < FN_POINTS; i++)
        lines_dot(o, w, lines_px(fn_x(i), S, F), lines_py(FN_ROWS, fn_y(i), S, F), r, LINES_INK);
}

/* the plank as big as the room lets it be (the room's width in portrait, its height in landscape); the pieces taken
 * in the room left above and below it */
static void layout(box b)
{
    int changed = lines_fit(&P, b), sq = P.sq, w = (int)lines_side(&P), h = (int)lines_tall(&P);
    float x, y;
    trays_place(&T, b, (float)w, (float)h, FN_PIECES, 0.8f * sq, 0.7f * ui.unit, &x, &y);
    P.x = x + P.fr;
    P.y = y + P.fr;
    if (changed) {
        ui_free(&t_board);
        paint_board(sq, P.fr, ui_buf(w, h));
        t_board = ui_tex(w, h, ui_buf(w, h));
        int d = (int)(DISC * sq);
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
static void pt_xy(int pt, float *x, float *y)   /* a point's centre (turned round when the board is flipped) */
{
    lines_xy(&P, fn_x(pt), fn_y(pt), x, y);
}

static int xy_pt(float x, float y)              /* the point whose cell holds (x, y); -1 */
{
    int gx, gy;
    return lines_at(&P, x, y, &gx, &gy) ? gx + FN_COLS * gy : -1;
}

static void cell_mark(int pt, void (*mark)(float, float, float, float))
{
    float x, y;
    pt_xy(pt, &x, &y);
    mark(x - P.sq / 2.0f, y - P.sq / 2.0f, (float)P.sq, (float)P.sq);
}

static void disc_at(int who, float cx, float cy, float d, float alpha) { ui_flat_at(t_disc[who], cx, cy, d, alpha, 0); }

/* the pieces each player took, by their side: the last turn's join them once they have faded from the board */
static void draw_trays(void)
{
    if (!trays_on(&T)) return;
    for (int c = 0; c < 2; c++) {
        int n = FN_PIECES - fn_pos(&G)->left[!c];
        if (anim.on && anim.who == c) n -= anim.n;
        for (int i = 0; i < n; i++) {
            float x, y;
            trays_slot(&T, c, i, &x, &y);
            disc_at(!c, x, y, 0.86f * T.pitch, 1);
        }
    }
}

static void ring(int pt)
{
    float x, y;
    pt_xy(pt, &x, &y);
    ui_mark_ring(x, y, 0.92f * P.sq);
}

/* the rings: in a choice, both lines; a piece selected, every piece its steps take; else the first piece of every
 * line a capture could take. The dots: where the selected piece can go */
static void draw_marks(void)
{
    const fnpos *p = fn_pos(&G);
    int8_t t[FN_MAX_TAKE];
    float x, y;
    for (int i = 0; i < nlegal; i++) {
        if (choice.on && i != choice.i[0] && i != choice.i[1]) continue;
        if (!choice.on && sel >= 0 && legal[i].from != sel) continue;
        int n = fn_taken(p, legal[i], t);
        for (int k = 0; k < n && (k == 0 || sel >= 0); k++) ring(t[k]);
        if (!choice.on && sel >= 0) {
            pt_xy(legal[i].to, &x, &y);
            ui_mark_dot(x, y, 0.26f * P.sq);
        }
    }
}

static void draw(void)
{
    const fnpos *p = fn_pos(&G);
    float w = (float)FN_COLS * P.sq + 2 * P.fr, h = (float)FN_ROWS * P.sq + 2 * P.fr, d = DISC * P.sq, x, y;
    ui_put(t_board, P.x - P.fr, P.y - P.fr, w, h);
    draw_trays();
    if (G.ply) {                                /* the last turn: a step, or every step of its chain */
        int side = G.pos[G.ply - 1].side;
        for (int k = G.ply - 1; k >= 0 && G.pos[k].side == side; k--) {
            cell_mark(G.move[k].from, ui_mark_last);
            cell_mark(G.move[k].to, ui_mark_last);
        }
    }
    if (sel >= 0) cell_mark(choice.on ? choice.to : sel, ui_mark_select);
    /* the step slides; the pieces taken start fading half-way through it */
    float t = 1, total = anim.n ? STEP_MS * 0.5f + (anim.n - 1) * FADE_GAP + FADE_MS : STEP_MS;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / total;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    int st = status();
    if ((st == GS_WIN0 || st == GS_WIN1) && G.ply && G.pos[G.ply - 1].side == st - GS_WIN0 && !anim.on) {
        pt_xy(G.move[G.ply - 1].to, &x, &y);    /* the winner's last capture: its piece in a gold light */
        ui_mark_gold(x, y, 1.5f * P.sq);
    }
    for (int i = 0; i < FN_POINTS; i++) {
        if (p->at[i] < 0 || (drag.on && drag.moved && i == drag.from) || (anim.on && i == anim.m.to)) continue;
        if (choice.on && i == sel) continue;
        pt_xy(i, &x, &y);
        disc_at(p->at[i], x, y, d, 1);
    }
    if (choice.on) {                            /* the piece on its new point, waiting for the line to take; a pale */
        pt_xy(sel, &x, &y);                     /* one where it came from */
        disc_at(p->side, x, y, d, 0.3f);
        pt_xy(choice.to, &x, &y);
        disc_at(p->side, x, y, d, 1);
    }
    if (st == GS_PLAY) draw_marks();
    if (anim.on) {
        float ms = t * total;
        for (int k = 0; k < anim.n; k++) {      /* the pieces taken: lifted, fading away, nearest first */
            float f = (ms - STEP_MS * 0.5f - k * FADE_GAP) / FADE_MS;
            if (f >= 1) continue;
            f = fmaxf(f, 0);
            pt_xy(anim.took[k], &x, &y);
            disc_at(!anim.who, x, y, d * (1 + 0.2f * f), 1 - f);
        }
        float x0, y0, s = fminf(ms / STEP_MS, 1), e = 1 - (1 - s) * (1 - s) * (1 - s);
        pt_xy(anim.m.from, &x0, &y0);
        pt_xy(anim.m.to, &x, &y);
        disc_at(anim.who, x0 + (x - x0) * e, y0 + (y - y0) * e, d, 1);
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
    const fnpos *p = fn_pos(&G);
    int pt = xy_pt(x, y);
    if (choice.on) {                            /* a tap on one of the two lines takes it; elsewhere: back */
        for (int k = 0; k < 2; k++)
            if (pt >= 0 && takes(choice.i[k], pt)) {
                play(legal[choice.i[k]], 1);
                return;
            }
        choice.on = 0;
        if (pt == choice.to) return;
    }
    if (pt < 0) {
        if (p->chain < 0) sel = -1;
        return;
    }
    if (sel >= 0 && pt != sel) {
        if (go(pt, 1)) return;
        for (int i = 0; i < nlegal; i++)        /* a ringed piece: the step that takes it */
            if (legal[i].from == sel && takes(i, pt)) {
                play(legal[i], 1);
                return;
            }
    }
    if ((pt == p->chain) || (p->chain < 0 && p->at[pt] == p->side && can_move(pt))) {
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
    if (hypotf(x - drag.x, y - drag.y) > P.sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) {                          /* a tap: the piece stays selected; the chain's piece: it stops */
        if (drag.from == fn_pos(&G)->chain) pass();
        return;
    }
    int pt = xy_pt(x, y);
    if (pt >= 0 && pt != drag.from && !go(pt, 0)) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the pieces the player took */
{
    ui_bar_count(b, t_small[!player], FN_PIECES - fn_pos(&G)->left[!player]);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card: the board at the start, its centre empty */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (FN_COLS + 2 * LINES_FRAME)), f = (int)(s * LINES_FRAME);
    int w = FN_COLS * s + 2 * f, h = FN_ROWS * s + 2 * f, ox = (size - w) / 2, oy = (size - h) / 2, d = (int)(DISC * s);
    uint8_t *board = ui_buf(w, h);
    paint_board(s, f, board);
    art_blend(rgba, size, size, board, w, h, ox, oy);
    fnpos p;
    fn_start(&p);
    for (int c = 0; c < 2; c++) {
        uint8_t *one = ui_buf(d, d);
        art_flat_shadow(d, one);
        for (int layer = 0; layer < 2; layer++) {   /* the shadows (a little to the lower right), then the discs */
            float dx = layer ? 0.5f : 0.06f * d, dy = layer ? 0.5f : 0.1f * d;
            for (int i = 0; i < FN_POINTS; i++) {
                if (p.at[i] != c) continue;
                float cx = ox + lines_px(fn_x(i), (float)s, (float)f) - d / 2.0f;
                float cy = oy + lines_py(FN_ROWS, fn_y(i), (float)s, (float)f) - d / 2.0f;
                art_blend(rgba, size, size, one, d, d, (int)(cx + dx), (int)(cy + dy));
            }
            if (!layer) art_counter(COUNTER_DISC, c, d, one);
        }
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last capture to finish */

static const game fanorona_game = {
    .id = "fanorona", .order = 245, .variants = 1, .fill = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy,
};
REGISTER_GAME(fanorona_game)
