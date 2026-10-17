/*
 * Breakthrough in RetroStone Board: the checkered board, two armies of pawns, tap or drag, the race to the far row
 * (the rules and the notation: breakthrough_rules.c, breakthrough.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <string.h>

#include "games/breakthrough.h"
#include "games/chess.h"                        /* PAWN: the house's chess pawn is the piece */
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define ANIM_MS 150

static btgame G;
static btmove legal[BT_MAX_MOVES];
static int nlegal;
static int sq, fr, made = -1, made_small = -1;
static float bx, by;                            /* the a8 corner (h1 when flipped) */
static SDL_Texture *t_piece[2], *t_small[2], *t_board;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; btmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = bt_legal(bt_pos(&G), legal);
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    bt_new(&G);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    int w = bt_winner(bt_pos(&G));
    return w < 0 ? GS_PLAY : w ? GS_WIN1 : GS_WIN0;   /* no draws in breakthrough */
}

static int turn(void) { return bt_pos(&G)->side; }

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static int can_move(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from) return 1;
    return 0;
}

static void play(btmove m, int animate)
{
    if (!bt_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.capture ? SND_CAPTURE : SND_MOVE);
}

static int undo(void)
{
    if (!bt_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "a2a3 b7b6 a3a4": the screenshots' and the tests' moves (breakthrough.h) */
static int script(const char *s)
{
    int ok = bt_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the bot (breakthrough_bot.c) -------------------------------------------------------------------------------- */
const btgame *breakthrough_live(void) { return &G; }

/* "a2-a3", "a2xb3" (or "a2a3"): one move, played as a tap plays it (animated); 0: not a legal move now */
int breakthrough_show(const char *move)
{
    int from = bt_square(move), sep = from >= 0 && (move[2] == '-' || move[2] == 'x');
    int to = from >= 0 ? bt_square(move + 2 + sep) : -1;
    int k = to >= 0 && !move[4 + sep] ? target(from, to) : -1;
    if (k < 0 || status() != GS_PLAY) return 0;
    play(legal[k], 1);
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
    if (made != sq) {
        made = sq;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_piece[c]);
            art_piece(PAWN, c, sq, ui_buf(sq, sq));
            t_piece[c] = ui_tex(sq, sq, ui_buf(sq, sq));
        }
        int n = 8 * sq + 2 * fr;
        ui_free(&t_board);
        art_board_n(8, 8, sq, fr, 1, ui_buf(n, n));
        t_board = ui_tex(n, n, ui_buf(n, n));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_small[c]);
            art_piece(PAWN, c, small, ui_buf(small, small));
            t_small[c] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

static void sq_xy(int s, float *x, float *y)    /* a square's top left corner */
{
    int f = s & 7, r = s >> 3;
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

/* a pawn on the square at (x, y), lifted off its shadow by lift (0 standing .. 1 a little up and bigger), faded */
static void piece_lift(int player, float x, float y, float size, float lift, float alpha)
{
    int up = ui_upside(player);
    if (alpha >= 1) ui_shadow(x, up ? y - 0.79f * size : y, size);
    float s = size * (1 + 0.1f * lift), dy = 0.12f * size * lift;
    x -= (s - size) / 2;
    y -= (s - size) / 2 + (up ? -dy : dy);
    SDL_SetTextureAlphaMod(t_piece[player], (Uint8)(255 * fmaxf(0, fminf(1, alpha))));
    if (up) ui_put_rot(t_piece[player], x + s / 2, y + s / 2, s, s, 180);
    else ui_put(t_piece[player], x, y, s, s);
    SDL_SetTextureAlphaMod(t_piece[player], 255);
}

static void piece_at(int player, float x, float y, float size) { piece_lift(player, x, y, size, 0, 1); }

static void draw(void)
{
    const btpos *p = bt_pos(&G);
    ui_put(t_board, bx - fr, by - fr, 8.0f * sq + 2 * fr, 8.0f * sq + 2 * fr);
    float x, y;
    if (G.ply) {
        btmove m = G.move[G.ply - 1];
        sq_xy(m.from, &x, &y);
        ui_mark_last(x, y, sq, sq);
        sq_xy(m.to, &x, &y);
        ui_mark_last(x, y, sq, sq);
    }
    if (sel >= 0) {
        sq_xy(sel, &x, &y);
        ui_mark_select(x, y, sq, sq);
    }
    float t = 1, lin = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)ANIM_MS;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        lin = t;
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    if (G.ply && status() != GS_PLAY && !anim.on) {   /* the winning piece: through, or the last one taken */
        sq_xy(G.move[G.ply - 1].to, &x, &y);
        ui_mark_gold(x + sq / 2.0f, y + sq / 2.0f, 1.5f * sq);
    }
    for (int s = 0; s < BT_SQUARES; s++) {
        if (p->at[s] < 0 || (drag.on && drag.moved && s == drag.from)) continue;
        sq_xy(s, &x, &y);
        if (anim.on && s == anim.m.to) {        /* the piece taken fades as the taker comes down on it */
            if (anim.m.capture) piece_lift(!p->at[s], x, y, (float)sq, 0, t < 0.5f ? 1 : 2 - 2 * t);
            continue;
        }
        piece_lift(p->at[s], x, y, (float)sq, s == sel && !drag.moved ? 1 : 0, 1);   /* the one picked lifts */
    }
    if (sel >= 0)                               /* where the selected piece can go */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            sq_xy(legal[i].to, &x, &y);
            if (legal[i].capture) ui_mark_ring(x + sq / 2.0f, y + sq / 2.0f, sq);
            else ui_mark_dot(x + sq / 2.0f, y + sq / 2.0f, 0.32f * sq);
        }
    if (anim.on) {
        float x0, y0, x1, y1;
        sq_xy(anim.m.from, &x0, &y0);
        sq_xy(anim.m.to, &x1, &y1);
        piece_lift(p->at[anim.m.to], x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, (float)sq, sinf(3.14159265f * lin), 1);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? -0.25f : 0.75f;
        piece_at(p->side, drag.x - s / 2, drag.y - s * off, s);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    int s = xy_sq(x, y);
    if (s < 0) { sel = -1; return; }
    if (sel >= 0 && target(sel, s) >= 0) {
        play(legal[target(sel, s)], 1);
        return;
    }
    const btpos *p = bt_pos(&G);
    if (p->at[s] == p->side && can_move(s)) {
        sel = s;
        drag.on = 1;
        drag.from = s;
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
    int s = xy_sq(x, y), i = s >= 0 ? target(drag.from, s) : -1;
    if (i >= 0) play(legal[i], 0);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pawns a player took from the other, closer together when there are many */
static void bar(int player, box b)
{
    int c = !player, n = BT_PIECES - bt_pos(&G)->left[c];
    float sz = 0.6f * ui.unit, step = 0.36f * ui.unit, cy = b.y + b.h / 2;
    if (n > 1 && sz + (n - 1) * step > b.w) step = fmaxf(0.12f * ui.unit, (b.w - sz) / (n - 1));
    for (int i = 0; i < n && b.x + i * step + sz <= b.x + b.w; i++)
        ui_put(t_small[c], b.x + i * step, cy - sz / 2, sz, sz);
}

static void token(int player, int size, uint8_t *rgba) { art_piece(PAWN, player, size, rgba); }

/* the menu's card: a corner of a board, an ivory pawn through the ebony line onto the far row, in a gold light */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = ui_buf(n, n);
    art_board_n(4, 4, s, f, 1, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    int g = (int)(1.6f * s);
    const uint8_t gold[3] = {255, 210, 96};
    uint8_t *glow = ui_buf(g, g);
    art_disc(g, gold, 1.0f, 1.0f, glow);
    art_blend(rgba, size, size, glow, g, g, o + f + 2 * s + s / 2 - g / 2, o + f + s / 2 - g / 2);
    static const int place[][3] = {{2, 0, 0}, {0, 0, 1}, {3, 0, 1}, {1, 1, 1}, {3, 1, 1},
                                   {0, 2, 1}, {2, 2, 0}, {0, 3, 0}, {1, 3, 0}, {3, 3, 0}};
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        uint8_t *one = ui_buf(s, s);
        art_piece(PAWN, place[i][2], s, one);
        art_blend(rgba, size, size, one, s, s, o + f + place[i][0] * s, o + f + place[i][1] * s);
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the winning pawn to arrive */

static const game breakthrough_game = {
    .id = "breakthrough", .order = 200, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(breakthrough_game)
