/*
 * Gomoku in RetroStone Board: go stones on a 15 x 15 go board, five in a row wins (the rules: gomoku_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * As in go (grid.h: the board, the stones, the ghost): a tap shows a ghost stone on the nearest point, a second tap on
 * it plays it; a drag moves the ghost, its release does not play. The new stone is set down from just above; the
 * winning five (or more, freestyle) light up in gold. The script's moves: "h8 i9 ..." (gomoku.h).
 */
#include "games/gomoku.h"
#include "games/grid.h"
#include "shell.h"

#define SET_MS 110                              /* a new stone set down */

static gkgame G;
static grid B = {.n = GK_N};
static struct { int on, pt; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    grid_calm(&B);
    anim.on = 0;
}

static void start(int variant)
{
    gk_new(&G, variant);
    calm();
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

static int legal(int pt) { return gk_status(&G) == GK_PLAY && G.pt[pt] == GK_EMPTY; }

static void play(int pt)
{
    if (!gk_play(&G, pt)) { ui_sound(SND_DENY); return; }
    B.ghost.on = 0;
    anim.on = 1;
    anim.pt = pt;
    anim.t0 = ui_ms();
    ui_sound(SND_PLACE);
}

static int undo(void)
{
    calm();
    return gk_undo(&G);
}

static int script(const char *s)
{
    calm();
    return gk_script(&G, s);
}

static int busy(void) { return anim.on; }     /* the end waits for the last stone to be down */

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b) { grid_layout(&B, b); }

static void draw(void)
{
    float x, y, t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)SET_MS;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    grid_board(&B);
    uint8_t line[GK_PTS];
    if (!anim.on && gk_line(&G, line))          /* the winning line: a gold light under its stones */
        for (int p = 0; p < GK_PTS; p++) {
            if (!line[p]) continue;
            grid_xy(&B, p, &x, &y);
            ui_mark_gold(x, y, 2.6f * B.sq);
        }
    grid_stones(&B, G.pt, NULL, anim.on ? anim.pt : -1);
    int last = gk_last(&G);
    if (anim.on) {                              /* the new stone, a little bigger, its shadow coming in */
        grid_xy(&B, last, &x, &y);
        ui_shadow_flat(x, y, (float)B.sq, (float)B.sq, t);
        grid_stone(&B, G.pt[last] - 1, x, y, (1.14f - 0.14f * t) * B.sq, 255);
    } else if (last >= 0) grid_last(&B, last, G.pt[last] - 1);
    if (gk_status(&G) == GK_PLAY) grid_ghost(&B, gk_side(&G));
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (gk_status(&G) == GK_PLAY) grid_press(&B, x, y, legal);
}

static void motion(float x, float y) { grid_motion(&B, x, y, legal); }

static void release(float x, float y)
{
    (void)x;
    (void)y;
    int pt = grid_release(&B);
    if (pt >= 0) play(pt);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the menu's card: a board's corner, five ebony stones in a row among ivory's; the picker's, closer in: freestyle six
 * in a row, standard exactly five */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int menu[][3] = {{1, 1, 0}, {2, 2, 0}, {3, 3, 0}, {4, 4, 0}, {5, 5, 0},
                                  {2, 1, 1}, {1, 3, 1}, {4, 3, 1}, {3, 4, 1}, {5, 2, 1}};
    static const int ivory[][2] = {{0, 4}, {2, 3}, {4, 5}, {5, 2}};
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
    .id = "gomoku", .order = 70, .variants = 2,
    .thumb = thumb, .token = grid_token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .script = script, .busy = busy,
};
REGISTER_GAME(gomoku_game)
