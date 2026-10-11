/*
 * Lines of Action in RetroStone Board: the board, the discs, tap or drag, the winning group lit up in gold (the rules:
 * loa_rules.c). A player with no move passes by itself.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The moves' notation (script: the screenshots, the tests): "b1-b3 a2xc4 ...", the squares as in chess from player 0's
 * side (the column a..h from its left, the row 1..8 from its side), "x" for a capture ("-" and "b1b3" are read too).
 */
#include <math.h>
#include <stdlib.h>

#include "games/loa.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define ANIM_MS 110                             /* the slide: this, and STEP_MS a square */
#define STEP_MS 35

static lgame G;
static lmove legal[LOA_MAX_MOVES];
static int nlegal;
static int sq, fr, made = -1;
static float bx, by;                            /* the a8 corner (h1 when flipped) */
static SDL_Texture *t_disc[2], *t_board;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, ms; lmove m; Uint32 t0; } anim;

/* player 0 plays ebony: the house's colour 1 */
static int colour(int player) { return !player; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    const lpos *p = loa_pos(&G);
    nlegal = p->result == LOA_PLAY ? loa_legal(p, p->side, legal) : 0;
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    loa_new(&G);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    static const int map[] = {[LOA_PLAY] = GS_PLAY, [LOA_WIN0] = GS_WIN0, [LOA_WIN1] = GS_WIN1, [LOA_DRAW] = GS_DRAW};
    return map[loa_pos(&G)->result];
}

static int turn(void) { return loa_pos(&G)->side; }

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static void play(int i, int animate)
{
    lmove m = legal[i];
    if (!loa_play(&G, m)) return;
    int dx = abs(m.to % 8 - m.from % 8), dy = abs(m.to / 8 - m.from / 8);
    anim.on = animate;
    anim.ms = ANIM_MS + STEP_MS * (dx > dy ? dx : dy);   /* a longer line, a longer slide */
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.capture ? SND_CAPTURE : SND_MOVE);
}

static int undo(void)
{
    if (!loa_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "b1-b3 a2xc4": the screenshots' and the tests' moves */
static int script(const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int used, k = loa_find(legal, nlegal, s, &used);
        if (k < 0) return 0;
        play(k, 0);
        s += used;
    }
    return 1;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    sq = (int)floorf(b.w / (8 + 2 * FRAME));
    fr = (int)(sq * FRAME);
    float bw = 8.0f * sq + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2) + fr;
    by = floorf(b.y + (b.h - bw) / 2) + fr;
    if (made == sq) return;
    made = sq;
    for (int c = 0; c < 2; c++) {
        ui_free(&t_disc[c]);
        art_counter(COUNTER_DISC, c, sq, ui_buf(sq, sq));
        t_disc[c] = ui_tex(sq, sq, ui_buf(sq, sq));
    }
    int n = 8 * sq + 2 * fr;
    ui_free(&t_board);
    art_board_n(8, 8, sq, fr, 1, ui_buf(n, n));
    t_board = ui_tex(n, n, ui_buf(n, n));
}

/* a square's top left corner */
static void sq_xy(int s, float *x, float *y)
{
    int f = s % 8, r = s / 8;
    if (ui.flipped) { f = 7 - f; r = 7 - r; }
    *x = bx + f * sq;
    *y = by + (7 - r) * sq;
}

static int xy_sq(float x, float y)
{
    int f = (int)floorf((x - bx) / sq), r = 7 - (int)floorf((y - by) / sq);
    if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
    return ui.flipped ? (7 - r) * 8 + 7 - f : r * 8 + f;
}

/* a player's disc lying on the wood, size x size from (x, y) (a disc has no top: never turned round face to face) */
static void disc_at(int player, float x, float y, float size)
{
    ui_shadow_flat(x + size / 2, y + size / 2, size, size, 1);
    ui_put(t_disc[colour(player)], x, y, size, size);
}

/* the winner's group in gold: a light on every piece and between every two that touch */
static void gold(const lpos *p, int who)
{
    static const int NEXT[4][2] = {{1, 0}, {0, 1}, {1, 1}, {-1, 1}};
    float x, y, h = sq / 2.0f, x1, y1;
    for (int s = 0; s < LOA_SQUARES; s++) {
        if (p->at[s] != who + 1) continue;
        sq_xy(s, &x, &y);
        ui_mark_gold(x + h, y + h, 1.7f * sq);
        for (int d = 0; d < 4; d++) {
            int nx = s % 8 + NEXT[d][0], ny = s / 8 + NEXT[d][1];
            if (nx < 0 || nx > 7 || ny > 7 || p->at[ny * 8 + nx] != who + 1) continue;
            sq_xy(ny * 8 + nx, &x1, &y1);
            ui_mark_gold((x + x1) / 2 + h, (y + y1) / 2 + h, 1.2f * sq);
        }
    }
}

static void draw(void)
{
    const lpos *p = loa_pos(&G), *shown = p;
    float x, y, h = sq / 2.0f;
    ui_put(t_board, bx - fr, by - fr, 8.0f * sq + 2 * fr, 8.0f * sq + 2 * fr);
    if (G.ply) {
        lmove m = G.move[G.ply - 1];
        sq_xy(m.from, &x, &y);
        ui_mark_last(x, y, sq, sq);
        sq_xy(m.to, &x, &y);
        ui_mark_last(x, y, sq, sq);
    }
    if (sel >= 0) {
        sq_xy(sel, &x, &y);
        ui_mark_select(x, y, sq, sq);
    }
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)anim.ms;
        if (t >= 1) t = 1, anim.on = 0;
        else {
            ui_animate();
            shown = &G.pos[G.ply - 1];          /* the piece taken stays until the taker arrives */
        }
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    if (!anim.on && (p->result == LOA_WIN0 || p->result == LOA_WIN1)) gold(p, p->result - LOA_WIN0);
    int skip = anim.on ? anim.m.from : drag.on && drag.moved ? drag.from : -1;
    for (int s = 0; s < LOA_SQUARES; s++) {    /* the shadows first: they fall on the wood only */
        if (!shown->at[s] || s == skip) continue;
        sq_xy(s, &x, &y);
        ui_shadow_flat(x + h, y + h, (float)sq, (float)sq, 1);
    }
    for (int s = 0; s < LOA_SQUARES; s++) {
        if (!shown->at[s] || s == skip) continue;
        sq_xy(s, &x, &y);
        ui_put(t_disc[colour(shown->at[s] - 1)], x, y, (float)sq, (float)sq);
    }
    if (sel >= 0)                               /* where the selected piece can go */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            sq_xy(legal[i].to, &x, &y);
            if (legal[i].capture) ui_mark_ring(x + h, y + h, 1.12f * sq);   /* round the disc it takes */
            else ui_mark_dot(x + h, y + h, 0.32f * sq);
        }
    if (anim.on) {
        float x0, y0, x1, y1;
        sq_xy(anim.m.from, &x0, &y0);
        sq_xy(anim.m.to, &x1, &y1);
        disc_at(p->at[anim.m.to] - 1, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, (float)sq);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? -0.25f : 0.75f;
        disc_at(p->side, drag.x - s / 2, drag.y - s * off, s);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    int s = xy_sq(x, y);
    if (s < 0) { sel = -1; return; }
    int k = sel >= 0 ? target(sel, s) : -1;
    if (k >= 0) {
        play(k, 1);
        return;
    }
    const lpos *p = loa_pos(&G);
    if (p->at[s] == p->side + 1) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    if (sel >= 0) ui_sound(SND_DENY);
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int s = xy_sq(x, y), k = s >= 0 && s != drag.from ? target(drag.from, s) : -1;
    if (k >= 0) play(k, 0);
    else if (s != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the player's pieces left on the board */
static void bar(int player, box b)
{
    const lpos *p = loa_pos(&G);
    int n = 0;
    for (int s = 0; s < LOA_SQUARES; s++) n += p->at[s] == player + 1;
    ui_number(b.x + 0.45f * ui.unit, b.y + b.h / 2, 0.46f * ui.unit, n, 1);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, colour(player), size, rgba); }

/* the menu's card: a corner of the board (a8 at the top left), a game under way: ebony from the top row, ivory from
 * the left column */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = ui_buf(n, n);
    art_board_n(4, 4, s, f, 1, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    static const int place[][3] = {{1, 0, 0}, {3, 0, 0}, {2, 1, 0}, {2, 3, 0}, {0, 1, 1}, {0, 2, 1}, {1, 2, 1},
                                   {3, 2, 1}};   /* column, row from the top, player */
    for (int pass = 0; pass < 2; pass++)        /* the shadows, then the discs */
        for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
            int x = o + f + place[i][0] * s, y = o + f + place[i][1] * s;
            uint8_t *one = ui_buf(s, s);
            if (pass) art_counter(COUNTER_DISC, colour(place[i][2]), s, one);
            else art_flat_shadow(s, one);
            art_blend(rgba, size, size, one, s, s, pass ? x : x + s / 20, pass ? y : y + s * 8 / 100);
        }
}

static int busy(void) { return anim.on; }     /* the end waits for the joining piece to arrive */

static const game loa_game = {
    .id = "loa", .order = 230, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(loa_game)
