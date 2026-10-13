/*
 * Gomoku in RetroStone Board: go stones on a go board, a row of them wins (the rules: gomoku_rules.c): five in a row
 * on 15 x 15 (freestyle, standard), or six in a row on 19 x 19, two stones a turn.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * As in go (grid.h: the board, the stones, the ghost): a tap shows a ghost stone on the nearest point, a second tap on
 * it plays it; a drag moves the ghost, its release does not play. The new stone is set down from just above; the
 * winning row lights up in gold. Six in a row: the bar of the player to move shows the stones still to place this
 * turn (two, then one); the first stone of a pair half played is marked as selected; the rings show the other
 * player's last turn (its one or two stones). Undo takes back one stone. The script's moves: "h8 i9 ..." (gomoku.h).
 * Renju: while ebony is to move, its forbidden points carry a small red cross (a tap there is refused). Where the
 * points are small (loupe.h), a loupe beside the finger shows them magnified while it is down, and a second tap near
 * the ghost counts as on it. The board is a square in the middle of the room the shell gives (game.fill).
 */
#include <string.h>

#include "games/gomoku.h"
#include "games/grid.h"
#include "games/loupe.h"
#include "shell.h"

#define SET_MS 110                              /* a new stone set down */
#define LOUPE_PTS 4.2f                          /* the points the loupe shows across */

static gkgame G;
static grid B = {.n = GK_N};
static int made_small = -1, made_cross = -1, made_z = -1;
static SDL_Texture *t_small[2], *t_cross, *t_zstone[2], *t_zcross;   /* t_z*: the loupe's */
static box room;                                /* the shell's room: the board in its middle, the loupe in it */
static struct { float x, y, dx, dy; } fing;     /* the finger on the board, its offset to the ghost */
static struct { int on, pt; Uint32 t0; } anim;
static uint8_t forbid[GK_MAXPTS];               /* renju: ebony's forbidden points now (ebony to move) */

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    grid_calm(&B);
    anim.on = 0;
}

/* renju: the points ebony may not play, found once a position (the marks, the taps) */
static void find_forbidden(void)
{
    memset(forbid, 0, sizeof forbid);
    if (G.variant != GK_RENJU || gk_side(&G) != 0 || gk_status(&G) != GK_PLAY) return;
    for (int p = 0; p < G.n * G.n; p++) forbid[p] = (uint8_t)gk_forbidden(&G, p);
}

static void start(int variant)
{
    gk_new(&G, variant);
    B.n = G.n;
    calm();
    find_forbidden();
}

static int status(void)
{
    switch (gk_status(&G)) {
    case GK_PLAY: return GS_PLAY;
    case GK_WIN0: return GS_WIN0;
    case GK_WIN1: return GS_WIN1;
    default: return GS_DRAW;
    }
}

static int turn(void) { return gk_side(&G); }

static int legal(int pt)
{
    if (G.variant == GK_RENJU) return !forbid[pt] && gk_allowed(&G, pt);
    return gk_status(&G) == GK_PLAY && G.pt[pt] == GK_EMPTY;
}

static void play(int pt)
{
    if (!gk_play(&G, pt)) { ui_sound(SND_DENY); return; }
    B.ghost.on = 0;
    anim.on = 1;
    anim.pt = pt;
    anim.t0 = ui_ms();
    ui_sound(SND_PLACE);
    find_forbidden();
}

static int undo(void)
{
    calm();
    int done = gk_undo(&G);
    find_forbidden();
    return done;
}

static int script(const char *s)
{
    calm();
    int ok = gk_script(&G, s);
    find_forbidden();
    return ok;
}

static int busy(void) { return anim.on; }     /* the end waits for the last stone to be down */

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* renju's forbidden point: a small cross in red lacquer, its ends rounded (size x size) */
static void paint_cross(int size, uint8_t *o)
{
    float r = size * 0.5f, lw = fmaxf(1.5f, size * 0.17f), arm = size * 0.36f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = x + 0.5f - r, dy = y + 0.5f - r, u = (dx + dy) * 0.70710678f, v = (dx - dy) * 0.70710678f;
            float d1 = hypotf(fmaxf(fabsf(u) - arm, 0), v), d2 = hypotf(fmaxf(fabsf(v) - arm, 0), u);
            float a = fminf(fmaxf(lw / 2 - fminf(d1, d2) + 0.5f, 0), 1);
            uint8_t *p = o + 4 * (y * size + x);
            p[0] = 196, p[1] = 44, p[2] = 36, p[3] = (uint8_t)(235 * a);
        }
}

static void layout(box b)
{
    float side = fminf(b.w, b.h);               /* a square in the middle of the room */
    room = b;
    grid_layout(&B, (box){floorf(b.x + (b.w - side) / 2), floorf(b.y + (b.h - side) / 2), side, side});
    int z = loupe_small((float)B.sq) ? (int)lroundf(B.sq * LOUPE_Z) : 0;
    if (made_z != z) {                          /* the loupe's stones and cross, crisp */
        made_z = z;
        for (int p = 0; p < 2; p++) {
            ui_free(&t_zstone[p]);
            if (z) t_zstone[p] = grid_stone_tex(p, z);
        }
        ui_free(&t_zcross);
        int zc = (int)(0.42f * z);
        if (z) paint_cross(zc, ui_buf(zc, zc)), t_zcross = ui_tex(zc, zc, ui_buf(zc, zc));
    }
    int c = (int)fmaxf(7, 0.42f * B.sq);
    if (made_cross != c) {
        made_cross = c;
        ui_free(&t_cross);
        paint_cross(c, ui_buf(c, c));
        t_cross = ui_tex(c, c, ui_buf(c, c));
    }
    int s = (int)(0.6f * ui.unit);
    if (made_small != s) {
        made_small = s;
        for (int p = 0; p < 2; p++) {
            ui_free(&t_small[p]);
            t_small[p] = grid_stone_tex(p, s);
        }
    }
}

/* the turn being played starts at this stone (six in a row: a pair half played started one stone back) */
static int turn_now(void) { return G.nmoves < G.n * G.n ? gk_turn_start(&G, G.nmoves) : G.nmoves; }

/* the board and all on it, as the grid g places it (the board's own, or the loupe's); t: the new stone setting down
 * (0..1) */
static void draw_board(const grid *g, float t, SDL_Texture *cross)
{
    float x, y;
    grid_board(g);
    uint8_t line[GK_MAXPTS];
    if (!anim.on && gk_line(&G, line))          /* the winning line: a gold light under its stones */
        for (int p = 0; p < G.n * G.n; p++) {
            if (!line[p]) continue;
            grid_xy(g, p, &x, &y);
            ui_mark_gold(x, y, 2.6f * g->sq);
        }
    int last = gk_last(&G), playing = gk_status(&G) == GK_PLAY, now = playing ? turn_now() : G.nmoves;
    if (playing && now < G.nmoves) {            /* a pair half played: its first stone, selected */
        grid_xy(g, G.move[now], &x, &y);
        ui_mark_select_round(x, y, 1.12f * g->sq);
    }
    grid_stones(g, G.pt, NULL, anim.on ? anim.pt : -1);
    if (anim.on) {                              /* the new stone, a little bigger, its shadow coming in */
        grid_xy(g, last, &x, &y);
        ui_shadow_flat(x, y, (float)g->sq, (float)g->sq, t);
        grid_stone(g, G.pt[last] - 1, x, y, (1.14f - 0.14f * t) * g->sq, 255);
    }
    if (now > 0)                                /* the rings: the last whole turn (one stone, or a pair) */
        for (int i = gk_turn_start(&G, now - 1); i < now; i++)
            if (!(anim.on && i == G.nmoves - 1)) grid_last(g, G.move[i], G.pt[G.move[i]] - 1);
    if (playing && G.variant == GK_RENJU)       /* ebony to move: its forbidden points */
        for (int p = 0; p < G.n * G.n; p++)
            if (forbid[p]) {
                grid_xy(g, p, &x, &y);
                float c = fmaxf(7, 0.42f * g->sq);
                ui_put_c(cross, x, y, c, c);
            }
    if (playing) grid_ghost(g, gk_side(&G));
}

/* the loupe while a finger is down on small points: the board round it magnified, the ghost in it */
static void draw_loupe(float t)
{
    if (gk_status(&G) != GK_PLAY || !B.ghost.down || !t_zstone[0]) return;
    float fx = fing.x + fing.dx, fy = fing.y + fing.dy;
    loupe l = loupe_at(fx, fy, (float)B.sq, LOUPE_PTS, ui_upside(turn()), room);
    grid z = B;                                 /* the same board, its points larger round the finger */
    z.sq = made_z;
    z.fr = (int)lroundf(B.fr * (float)z.sq / B.sq);
    float k = (float)z.sq / B.sq;
    z.ox = l.cx + (B.ox - fx) * k;
    z.oy = l.cy + (B.oy - fy) * k;
    z.t_stone[0] = t_zstone[0];
    z.t_stone[1] = t_zstone[1];
    l.z = k;
    loupe_begin(&l);
    draw_board(&z, t, t_zcross);
    loupe_end();
}

static void draw(void)
{
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)SET_MS;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    draw_board(&B, t, t_cross);
    draw_loupe(t);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (gk_status(&G) != GK_PLAY) return;
    fing.x = x, fing.y = y, fing.dx = fing.dy = 0;
    if (B.ghost.on && loupe_small((float)B.sq)) {   /* small points: a tap near the ghost is on it */
        float gx, gy;
        grid_xy(&B, B.ghost.pt, &gx, &gy);
        loupe_snap(x, y, gx, gy, 0.8f * B.sq, &fing.dx, &fing.dy);
    }
    grid_press(&B, x + fing.dx, y + fing.dy, legal);
}

static void motion(float x, float y)
{
    fing.x = x, fing.y = y;
    grid_motion(&B, x + fing.dx, y + fing.dy, legal);
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int pt = grid_release(&B);
    if (pt >= 0) play(pt);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* six in a row: the stones the player to move still places this turn (two, then one) */
static void bar(int player, box b)
{
    if (G.variant != GK_SIX || gk_status(&G) != GK_PLAY || gk_side(&G) != player) return;
    float sz = 0.6f * ui.unit, cy = b.y + b.h / 2;
    for (int i = 0; i < gk_left(&G); i++)
        ui_flat_at(t_small[player], b.x + sz / 2 + i * 0.72f * ui.unit, cy, sz, 1, 0);
}

/* the menu's card: a board's corner, five ebony stones in a row among ivory's; the picker's, closer in: freestyle six
 * in a row, standard exactly five; six in a row: the whole large board, a diagonal six among pairs of stones */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int menu[][3] = {{1, 1, 0}, {2, 2, 0}, {3, 3, 0}, {4, 4, 0}, {5, 5, 0},
                                  {2, 1, 1}, {1, 3, 1}, {4, 3, 1}, {3, 4, 1}, {5, 2, 1}};
    static const int ivory[][2] = {{0, 4}, {2, 3}, {4, 5}, {5, 2}};
    static const int six[][3] = {{4, 4, 0}, {5, 5, 0}, {6, 6, 0}, {7, 7, 0}, {8, 8, 0}, {9, 9, 0}, {11, 7, 0},
                                 {12, 7, 0}, {5, 9, 1}, {6, 9, 1}, {9, 5, 1}, {10, 5, 1}, {3, 12, 1}, {4, 12, 1},
                                 {13, 11, 0}, {13, 13, 1}, {14, 13, 1}, {7, 14, 0}};
    if (variant == GK_RENJU) {                  /* renju: ebony's double three, its point crossed out in red */
        static const int st[][3] = {{3, 4, 0}, {4, 4, 0}, {5, 2, 0}, {5, 3, 0}, {1, 1, 1}, {6, 6, 1}, {6, 1, 1},
                                    {2, 6, 1}};
        grid_pic pic = grid_thumb(8, 1, size, rgba);
        for (unsigned i = 0; i < sizeof st / sizeof st[0]; i++)
            grid_thumb_stone(pic, st[i][0], st[i][1], st[i][2], size, rgba);
        int c = (int)(0.9f * pic.s);
        paint_cross(c, ui_buf(c, c));
        art_blend(rgba, size, size, ui_buf(c, c), c, c, pic.x0 + 5 * pic.s + (pic.s - c) / 2,
                  pic.y0 + 4 * pic.s + (pic.s - c) / 2);
        return;
    }
    if (variant == GK_SIX) {
        grid_pic pic = grid_thumb(GK_MAX, 0, size, rgba);
        for (unsigned i = 0; i < sizeof six / sizeof six[0]; i++)
            grid_thumb_stone(pic, six[i][0], six[i][1], six[i][2], size, rgba);
        return;
    }
    grid_pic pic = grid_thumb(variant < 0 ? 7 : 8, 1, size, rgba);
    if (variant < 0) {
        for (unsigned i = 0; i < sizeof menu / sizeof menu[0]; i++)
            grid_thumb_stone(pic, menu[i][0], menu[i][1], menu[i][2], size, rgba);
        return;
    }
    for (int i = 0; i < (variant == GK_FREESTYLE ? 6 : 5); i++) grid_thumb_stone(pic, 1 + i, 4, 0, size, rgba);
    for (unsigned i = 0; i < sizeof ivory / sizeof ivory[0]; i++)
        grid_thumb_stone(pic, ivory[i][0], ivory[i][1], 1, size, rgba);
}

static const game gomoku_game = {
    .id = "gomoku", .order = 70, .variants = GK_VARIANTS,
    .thumb = thumb, .token = grid_token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .fill = 1,
};
REGISTER_GAME(gomoku_game)
