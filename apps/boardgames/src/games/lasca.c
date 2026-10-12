/*
 * Lasca in RetroStone Board: the 7 x 7 checkered board (its 25 light squares used), the columns drawn as the house's
 * men and kings stacked (column.h, shared with draughts' bashni), tap or drag, a capture a jump at a time: the column
 * hops over, the top piece it takes slides in under it (the rules and the notation: lasca_rules.c, lasca.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>

#include "games/column.h"
#include "games/lasca.h"
#include "games/lines.h"
#include "shell.h"

#define PIECE 0.94f                             /* a piece's picture, in squares */
#define STEP_MS 160
#define JUMP_MS 300
#define PI_F 3.14159265f

static lsgame G;
static lsmove legal[LS_MAX_MOVES];
static int nlegal;
static lines_geo L = {.cells = LS_SIDE};        /* the squares: a grid of cells (lines.h's mapping) */
static int made_small = -1;
static SDL_Texture *t_pc[2][2], *t_small[2], *t_board;   /* [player][officer] */
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; lsmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int status(void)
{
    switch (ls_status(&G)) {
    case LS_PLAY: return GS_PLAY;
    case LS_STUCK: return ls_pos(&G)->side ? GS_WIN0 : GS_WIN1;
    default: return GS_DRAW;
    }
}

/* a column in the middle of a chain of captures is already picked up */
static void refresh(void)
{
    const lspos *p = ls_pos(&G);
    nlegal = ls_legal(p, legal);
    sel = p->chain;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    ls_new(&G);
    anim.on = 0;
    refresh();
}

static int turn(void) { return ls_pos(&G)->side; }

static int find(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static int find_over(int from, int over)        /* the capture of a column (a tap on the column itself) */
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

static void play(lsmove m, int animate)
{
    if (!ls_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.over >= 0 ? SND_CAPTURE : SND_MOVE);
}

static int undo(void)
{
    if (!ls_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "c3-d4 e5xc3 b2xd4": the screenshots' and the tests' moves (lasca.h) */
static int script(const char *s)
{
    int ok = ls_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void layout(box b)
{
    if (lines_fit(&L, b)) {
        int n = (int)lines_side(&L), d = (int)(PIECE * L.sq);
        ui_free(&t_board);
        art_board_n(LS_SIDE, LS_SIDE, L.sq, L.fr, 1, ui_buf(n, n));
        t_board = ui_tex(n, n, ui_buf(n, n));
        for (int c = 0; c < 2; c++)
            for (int k = 0; k < 2; k++) {
                ui_free(&t_pc[c][k]);
                art_counter(k ? COUNTER_KING : COUNTER_MAN, c, d, ui_buf(d, d));
                t_pc[c][k] = ui_tex(d, d, ui_buf(d, d));
            }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_small[c]);
            art_counter(COUNTER_MAN, c, small, ui_buf(small, small));
            t_small[c] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void sq_xy(int s, float *x, float *y) { lines_xy(&L, ls_x(s), ls_y(s), x, y); }

/* the square under (x, y): a used one; a tap on a dark square goes to the light one below it on the screen (a tall
 * column rises into it); -1 */
static int xy_sq(float x, float y)
{
    int gx, gy;
    for (int k = 0; k < 2; k++, y += 0.5f * L.sq)
        if (lines_at(&L, x, y, &gx, &gy) && ls_used(gx + LS_SIDE * gy)) return gx + LS_SIDE * gy;
    return -1;
}

/* a column standing on (cx, cy) (its foot), size its pieces' picture, lifted (pixels) */
static void column_on(lscol c, float cx, float cy, float size, float lift, float alpha)
{
    column_at(c.pc, c.h, t_pc, cx, cy, (float)L.sq, size, lift, alpha);
}

static void draw(void)
{
    const lspos *p = ls_pos(&G);
    float n = lines_side(&L), d = PIECE * L.sq, x, y;
    ui_put(t_board, L.x - L.fr, L.y - L.fr, n, n);
    if (G.ply) {                                /* the last turn: a step, or every jump of its chain */
        int side = G.pos[G.ply - 1].side;
        for (int k = G.ply - 1; k >= 0 && G.pos[k].side == side; k--) {
            lines_mark_last(&L, ls_x(G.move[k].from), ls_y(G.move[k].from));
            lines_mark_last(&L, ls_x(G.move[k].to), ls_y(G.move[k].to));
        }
    }
    if (sel >= 0) lines_mark_select(&L, ls_x(sel), ls_y(sel));
    float t = 1;
    if (anim.on) {
        int jump = anim.m.over >= 0;
        t = (ui_ms() - anim.t0) / (float)(jump ? JUMP_MS : STEP_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    const lspos *before = anim.on ? &G.pos[G.ply - 1] : p;
    float handover = 0.3f;                      /* a capture: the piece taken leaves its column then */
    for (int i = 0; i < LS_SQUARES; i++) {      /* the rows from the top of the screen: a column rises over the one
                                                 * behind it */
        int sq = i % LS_SIDE + LS_SIDE * (ui.flipped ? i / LS_SIDE : LS_SIDE - 1 - i / LS_SIDE);
        lscol c = p->col[sq];
        if (anim.on && sq == anim.m.to) continue;
        if (anim.on && sq == anim.m.over && t < handover) c = before->col[sq];
        if (drag.on && drag.moved && sq == drag.from) continue;
        sq_xy(sq, &x, &y);
        column_on(c, x, y, d, 0, 1);
    }
    if (status() == GS_PLAY)
        for (int i = 0; i < nlegal; i++) {      /* the columns that can be taken, where the selected one can go */
            if (legal[i].over >= 0) {
                sq_xy(legal[i].over, &x, &y);
                ui_mark_ring(x, y, 1.1f * L.sq);
            }
            if (legal[i].from == sel) {
                sq_xy(legal[i].to, &x, &y);
                ui_mark_dot(x, y, 0.3f * L.sq);
            }
        }
    if (anim.on) {
        lscol mover = before->col[anim.m.from];
        float x0, y0, x1, y1, e = 1 - (1 - t) * (1 - t) * (1 - t);
        sq_xy(anim.m.from, &x0, &y0);
        sq_xy(anim.m.to, &x1, &y1);
        float cx = x0 + (x1 - x0) * e, cy = y0 + (y1 - y0) * e;
        float lift = anim.m.over >= 0 ? 0.42f * L.sq * sinf(PI_F * t) : 0;
        if (anim.m.over >= 0 && t >= handover) {   /* the piece taken slides in under the column */
            lscol v = before->col[anim.m.over], one = {v.pc & 3, 1};
            float k = (t - handover) / (1 - handover), ox, oy;
            k = k * k * (3 - 2 * k);
            sq_xy(anim.m.over, &ox, &oy);
            oy -= column_rise(v.h, 0) * L.sq;
            column_on(one, ox + (cx - ox) * k, oy + (cy - lift - oy) * k, d, 0, 1);
        }
        column_on(mover, cx, cy, d * (1 + 0.06f * sinf(PI_F * t)), lift, 1);
    }
    if (drag.on && drag.moved) {                /* the dragged column, a little bigger, beside the finger */
        float s = 1.12f * d, off = ui_upside(p->side) ? -0.45f : 0.55f;
        column_on(p->col[drag.from], drag.x, drag.y - s * off, s, 0, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    const lspos *p = ls_pos(&G);
    int s = xy_sq(x, y), i;
    if (s < 0) {
        if (p->chain < 0) sel = -1;
        return;
    }
    if (sel >= 0 && ((i = find(sel, s)) >= 0 || (i = find_over(sel, s)) >= 0)) {
        play(legal[i], 1);
        return;
    }
    if (p->col[s].h && LS_OWNER(ls_top(p->col[s])) == p->side && can_move(s)) {
        sel = s;
        drag.on = 1;
        drag.from = s;
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
    if (!drag.moved) return;                    /* a tap: the column stays selected */
    int s = xy_sq(x, y), i = s >= 0 ? find(drag.from, s) : -1;
    if (i >= 0) play(legal[i], 0);
    else if (s != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the other player's pieces held prisoner in the player's columns */
static void bar(int player, box b) { ui_bar_count(b, t_small[!player], ls_prisoners(ls_pos(&G), player)); }

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_MAN, player, size, rgba); }

/* the menu's card: a game going on, columns of every height */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (LS_SIDE + 2 * LINES_FRAME)), f = (int)(s * LINES_FRAME), n = LS_SIDE * s + 2 * f;
    int o = (size - n) / 2, d = (int)(PIECE * s);
    uint8_t *board = ui_buf(n, n);
    art_board_n(LS_SIDE, LS_SIDE, s, f, 1, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    lspos p;
    ls_setup(&p, 0, "a1:w e1:w b2:w f2:wbb c3:bw g3:w d4:wbbw a5:b e5:Bwb g5:b b6:bww f6:b c7:b g7:B");
    for (int r = LS_SIDE - 1; r >= 0; r--)      /* the back row first */
        for (int c = 0; c < LS_SIDE; c++) {
            lscol col = p.col[c + LS_SIDE * r];
            float cx = o + lines_px(c, (float)s, (float)f), cy = o + lines_py(LS_SIDE, r, (float)s, (float)f);
            column_thumb(rgba, size, col.pc, col.h, cx, cy, s, d);
        }
}

static int busy(void) { return anim.on; }     /* the end waits for the last move to finish */

static const game lasca_game = {
    .id = "lasca", .order = 25, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(lasca_game)
