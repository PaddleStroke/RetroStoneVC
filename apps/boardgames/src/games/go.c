/*
 * Go in RetroStone Board: the board (9 x 9, 13 x 13, 19 x 19), the stones, the counting (the rules: go_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A tap shows a ghost stone on the nearest point (19 x 19 points are small on a phone), a second tap on it plays it;
 * a drag moves the ghost, its release does not play. Two passes: the counting. Each point shows its owner (a small
 * square), a tap on a group marks it dead (faded, its points the other player's) or alive again; the pass button then
 * means done. The script's moves: "d4 q16 pass" (go.h), a pass while counting is done. The board, the stones and the
 * ghost: grid.h (shared with gomoku).
 */
#include <math.h>
#include <string.h>

#include "games/go.h"
#include "games/grid.h"
#include "shell.h"

#define FADE_MS 260                             /* captured stones fade out */

static gogame G;
static grid B = {.n = 19};
static int made_u = -1;
static SDL_Texture *t_small[2], *t_half;
static struct { int on, k, pt[GO_PTS]; uint8_t who[GO_PTS]; Uint32 t0; } fade;

static const uint8_t WHITE[3] = {255, 255, 255};

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    grid_calm(&B);
    fade.on = 0;
}

static void start(int variant)
{
    B.n = variant == 0 ? 9 : variant == 1 ? 13 : 19;
    go_new(&G, B.n);
    calm();
}

static int status(void)
{
    if (G.phase != GO_OVER) return GS_PLAY;
    return go_winner(&G) == 0 ? GS_WIN0 : GS_WIN1;
}

static int turn(void) { return go_now(&G)->side; }

static int legal(int pt)
{
    gopos next;
    return go_try(&G, pt, &next) >= 0;
}

/* a stone on pt: the stones it takes fade out */
static void play(int pt)
{
    gopos before = *go_now(&G);
    int took = go_play(&G, pt);
    if (took < 0) { ui_sound(SND_DENY); return; }
    B.ghost.on = 0;
    fade.on = took > 0;
    fade.k = 0;
    fade.t0 = ui_ms();
    for (int p = 0; p < B.n * B.n; p++)
        if (before.pt[p] != GO_EMPTY && go_now(&G)->pt[p] == GO_EMPTY) {
            fade.pt[fade.k] = p;
            fade.who[fade.k++] = (uint8_t)(before.pt[p] - 1);
        }
    ui_sound(took ? SND_CAPTURE : SND_PLACE);
}

static int can_pass(void) { return G.phase != GO_OVER; }

static void pass(void)
{
    go_pass(&G);
    calm();
}

static int undo(void)
{
    calm();
    return go_undo(&G);
}

static int script(const char *s)
{
    calm();
    return go_script(&G, s);
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* the komi's half point: a ring, its left half filled */
static void paint_half(int size, uint8_t *o)
{
    float r = size * 0.5f, lw = fmaxf(1.2f, size * 0.14f);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = x + 0.5f - r, dy = y + 0.5f - r, d = hypotf(dx, dy);
            float disc = fminf(fmaxf(r - d, 0), 1), ring = disc * fminf(fmaxf(d - (r - lw), 0), 1);
            float half = disc * fminf(fmaxf(-dx + 0.5f, 0), 1);
            uint8_t *p = o + 4 * (y * size + x);
            memcpy(p, WHITE, 3);
            p[3] = (uint8_t)(255 * fmaxf(ring, half));
        }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    grid_layout(&B, b);
    int u = (int)ui.unit;
    if (made_u != u) {
        made_u = u;
        int s = (int)(0.6f * u), h = (int)(0.3f * u);
        for (int p = 0; p < 2; p++) {
            ui_free(&t_small[p]);
            t_small[p] = grid_stone_tex(p, s);
        }
        ui_free(&t_half);
        paint_half(h, ui_buf(h, h));
        t_half = ui_tex(h, h, ui_buf(h, h));
        SDL_SetTextureColorMod(t_half, 246, 238, 220);
    }
}

/* a point's owner while counting: a small square of the player's colour (ivory outlined on the wood, ebony on the
 * bar) */
static void owner_square(int player, float cx, float cy, float s, int on_wood)
{
    float e = fmaxf(1, roundf(0.12f * s));
    if (player == 1 && on_wood) ui_fill(cx - s / 2 - e, cy - s / 2 - e, s + 2 * e, s + 2 * e, 52, 40, 32, 255);
    if (player == 0 && !on_wood) ui_fill(cx - s / 2 - e, cy - s / 2 - e, s + 2 * e, s + 2 * e, 214, 204, 188, 255);
    if (player) ui_fill(cx - s / 2, cy - s / 2, s, s, 242, 234, 216, 255);
    else ui_fill(cx - s / 2, cy - s / 2, s, s, 40, 36, 34, 255);
}

static void draw(void)
{
    const gopos *p = go_now(&G);
    float x, y;
    grid_board(&B);
    int counting = G.phase != GO_PLAY;
    int8_t owner[GO_PTS];
    float score[2];
    if (counting) go_score(&G, owner, score);
    grid_stones(&B, p->pt, counting ? G.dead : NULL, -1);   /* the dead stones faded */
    if (fade.on) {
        float t = (ui_ms() - fade.t0) / (float)FADE_MS;
        if (t >= 1) fade.on = 0;
        else {
            ui_animate();
            for (int i = 0; i < fade.k; i++) {
                grid_xy(&B, fade.pt[i], &x, &y);
                grid_stone(&B, fade.who[i], x, y, (float)B.sq, (int)(255 * (1 - t) * (1 - t)));
            }
        }
    }
    int last = go_last(&G);
    if (last >= 0 && p->pt[last] != GO_EMPTY && !(counting && G.dead[last])) grid_last(&B, last, p->pt[last] - 1);
    if (counting)
        for (int pt = 0; pt < B.n * B.n; pt++) {
            if (owner[pt] < 0 || (p->pt[pt] != GO_EMPTY && !G.dead[pt])) continue;
            grid_xy(&B, pt, &x, &y);
            owner_square(owner[pt], x, y, roundf(0.22f * B.sq), 1);
        }
    if (G.phase == GO_PLAY) grid_ghost(&B, p->side);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (G.phase == GO_COUNT) {                  /* a group dead / alive */
        ui_sound(go_toggle(&G, grid_pt(&B, x, y)) ? SND_TAP : SND_DENY);
        return;
    }
    if (G.phase == GO_PLAY) grid_press(&B, x, y, legal);
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
/* playing: the stones the player took (a stone of the other colour, the count); counting: the player's score (a square
 * of the player's colour, the points; ivory's half point of the komi) */
static void bar(int player, box b)
{
    if (G.phase == GO_PLAY) {
        ui_bar_count(b, t_small[!player], go_now(&G)->caps[player]);
        return;
    }
    float u = ui.unit, cy = b.y + b.h / 2, x = b.x + 0.52f * u, h = 0.5f * u;
    int8_t owner[GO_PTS];
    float score[2];
    go_score(&G, owner, score);
    int value = (int)score[player], digits = value >= 100 ? 3 : value >= 10 ? 2 : 1;
    owner_square(player, b.x + 0.17f * u, cy, roundf(0.3f * u), 0);
    float w = 0.74f * h * (digits - 1) + 0.62f * h;
    ui_number(x + w / 2, cy, h, value, 1);
    if (G.phase != GO_PLAY && player == 1) {
        float s = 0.3f * u;
        ui_put(t_half, x + w + 0.12f * u, cy - s / 2, s, s);
    }
}

/* the menu's card: a board's corner, a few stones; the picker's: the whole board of each size */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int corner = variant < 0, nn = corner ? 5 : variant == 0 ? 9 : variant == 1 ? 13 : 19;
    grid_pic pic = grid_thumb(nn, corner, size, rgba);
    static const int corner_st[][3] = {{2, 2, 0}, {3, 1, 0}, {1, 3, 0}, {2, 1, 1}, {1, 2, 1}, {4, 3, 1}};
    static const float board_st[][3] = {{0.17f, 0.17f, 0}, {0.83f, 0.83f, 1}, {0.83f, 0.17f, 0}, {0.17f, 0.75f, 1},
                                        {0.5f, 0.5f, 0}, {0.75f, 0.42f, 1}};
    int k = corner ? (int)(sizeof corner_st / sizeof corner_st[0]) : (int)(sizeof board_st / sizeof board_st[0]);
    for (int i = 0; i < k; i++) {
        int c = corner ? corner_st[i][0] : (int)lroundf(board_st[i][0] * (nn - 1));
        int r = corner ? corner_st[i][1] : (int)lroundf(board_st[i][1] * (nn - 1));
        int who = corner ? corner_st[i][2] : (int)board_st[i][2];
        grid_thumb_stone(pic, c, r, who, size, rgba);
    }
}

static const game go_game = {
    .id = "go", .order = 60, .variants = 3,
    .thumb = thumb, .token = grid_token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script,
};
REGISTER_GAME(go_game)
