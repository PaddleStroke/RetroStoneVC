/*
 * The Game of the Amazons in RetroStone Board: the 10 x 10 board, the amazons (chess queens), the burnt squares, the
 * turn in two steps: tap or drag an amazon to its new square, then tap where its arrow flies (the rules and the
 * notation: amazons_rules.c, amazons.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/amazons.h"
#include "games/chess.h"                        /* QUEEN, for art_piece */
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define MOVE_MS 170
#define FLY_MS 240                              /* the arrow in the air */
#define BURN_MS 260                             /* then the square catching fire */

static amgame G;
static int sq, fr, made = -1;
static float bx, by;                            /* the a10 corner (j1 when flipped) */
static SDL_Texture *t_queen[2], *t_board, *t_burn, *t_arrow;
static int sel = -1;                            /* the amazon picked up (on the arrow's step: the one that shoots) */
static uint8_t reach[AM_MAX_LINE];              /* where it can go, or where its arrow can fly */
static int nreach;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, stage, from, to; Uint32 t0; } anim;   /* stage: AM_MOVE a slide, AM_SHOOT an arrow */

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void pick(int s)
{
    sel = s;
    nreach = s >= 0 ? am_line(am_pos(&G), s, reach) : 0;
}

/* after each step: on the arrow's step the amazon that moved is picked up, its targets shown */
static void refresh(void)
{
    const ampos *p = am_pos(&G);
    pick(p->stage == AM_SHOOT ? p->at : -1);
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    am_new(&G);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    int w = am_winner(&G);
    return w < 0 ? GS_PLAY : w ? GS_WIN1 : GS_WIN0;
}

static int turn(void) { return am_pos(&G)->side; }

static int reaches(int s)
{
    for (int i = 0; i < nreach; i++)
        if (reach[i] == s) return 1;
    return 0;
}

static void animate(int stage, int from, int to)
{
    anim.on = 1;
    anim.stage = stage;
    anim.from = from;
    anim.to = to;
    anim.t0 = ui_ms();
}

static void move(int from, int to, int slide)
{
    if (!am_move(&G, from, to)) return;
    anim.on = 0;
    if (slide) animate(AM_MOVE, from, to);
    refresh();
    ui_sound(SND_MOVE);
}

static void shoot(int to)
{
    int from = am_pos(&G)->at;
    if (!am_shoot(&G, to)) return;
    animate(AM_SHOOT, from, to);
    refresh();
    ui_sound(SND_PLACE);
}

static int undo(void)                           /* one step: the arrow, then the move */
{
    if (!am_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "d1-d7/g7 g10-e8/e1": the screenshots' and the tests' steps (amazons.h) */
static int script(const char *s)
{
    int ok = am_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a burnt square: a scorch on the wood and, in it, a dark inlaid disc cracked by a black star, size x size */
static void paint_burn(int size, uint8_t *rgba)
{
    static const uint8_t SCORCH[3] = {58, 30, 14};
    art_disc(size, SCORCH, 0.85f, 0.8f, rgba);
    uint8_t *mark = malloc((size_t)size * size * 4);
    if (!mark) return;
    art_style st = {{46, 32, 24}, {16, 10, 6}, {16, 10, 6}, {10, 6, 4}, 0.45f, fmaxf(0.03f, 1.2f / size)};
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.31f, 0.31f);
    float star[16];
    for (int i = 0; i < 8; i++) {               /* four long spikes, four short */
        float a = i * 0.785398f + 0.39f, r = i & 1 ? 0.06f : 0.19f;
        star[2 * i] = 0.5f + r * cosf(a);
        star[2 * i + 1] = 0.5f + r * sinf(a);
    }
    art_poly(f, ART_MARK, star, 8);
    art_render(f, &st, size, size, (float)size, 0, 0, mark);
    art_blend(rgba, size, size, mark, size, size, 0, 0);
    art_fig_free(f);
    free(mark);
}

/* the arrow in flight, pointing right (to the east), its shaft across the middle of a size x size picture */
static void paint_arrow(int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    art_style wood = {{120, 82, 50}, {30, 18, 10}, {60, 38, 22}, {176, 52, 36}, 0.4f, fmaxf(0.02f, 1.0f / size)};
    art_fig *f = art_fig_new();
    const float fl_up[] = {0.06f, 0.40f, 0.22f, 0.40f, 0.30f, 0.485f, 0.14f, 0.485f};
    const float fl_dn[] = {0.06f, 0.60f, 0.22f, 0.60f, 0.30f, 0.515f, 0.14f, 0.515f};
    art_rect(f, ART_PART, 0.10f, 0.475f, 0.76f, 0.525f);
    art_poly(f, ART_MARK, fl_up, 4);
    art_poly(f, ART_MARK, fl_dn, 4);
    art_render(f, &wood, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    uint8_t *head = malloc((size_t)size * size * 4);
    if (!head) return;
    art_style iron = {{92, 94, 100}, {20, 20, 24}, {40, 40, 46}, {0, 0, 0}, 0.5f, fmaxf(0.02f, 1.0f / size)};
    f = art_fig_new();
    const float tip[] = {0.72f, 0.42f, 0.96f, 0.5f, 0.72f, 0.58f, 0.76f, 0.5f};
    art_poly(f, ART_PART, tip, 4);
    art_render(f, &iron, size, size, (float)size, 0, 0, head);
    art_blend(rgba, size, size, head, size, size, 0, 0);
    art_fig_free(f);
    free(head);
}

static SDL_Texture *tex(void (*paint)(int, uint8_t *), int size)
{
    paint(size, ui_buf(size, size));
    return ui_tex(size, size, ui_buf(size, size));
}

static void layout(box b)
{
    sq = (int)floorf(b.w / (AM_N + 2 * FRAME));
    fr = (int)(sq * FRAME);
    int n = AM_N * sq + 2 * fr;
    bx = floorf(b.x + (b.w - n) / 2) + fr;
    by = floorf(b.y + (b.h - n) / 2) + fr;
    if (made == sq) return;
    made = sq;
    ui_free(&t_board);
    art_board_n(AM_N, AM_N, sq, fr, 1, ui_buf(n, n));
    t_board = ui_tex(n, n, ui_buf(n, n));
    for (int c = 0; c < 2; c++) {
        ui_free(&t_queen[c]);
        art_piece(QUEEN, c, sq, ui_buf(sq, sq));
        t_queen[c] = ui_tex(sq, sq, ui_buf(sq, sq));
    }
    ui_free(&t_burn);
    t_burn = tex(paint_burn, sq);
    ui_free(&t_arrow);
    t_arrow = tex(paint_arrow, sq);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void sq_xy(int s, float *x, float *y)    /* a square's top left corner */
{
    int f = s % AM_N, r = s / AM_N;
    if (ui.flipped) { f = AM_N - 1 - f; r = AM_N - 1 - r; }
    *x = bx + f * sq;
    *y = by + (AM_N - 1 - r) * sq;
}

static int xy_sq(float x, float y)
{
    int f = (int)floorf((x - bx) / sq), r = AM_N - 1 - (int)floorf((y - by) / sq);
    if (f < 0 || f >= AM_N || r < 0 || r >= AM_N) return -1;
    return ui.flipped ? (AM_N - 1 - f) + (AM_N - 1 - r) * AM_N : f + r * AM_N;
}

static void queen_at(int c, float x, float y, float size)
{
    int up = ui_upside(c);
    ui_shadow(x, up ? y - 0.79f * size : y, size);
    if (up) ui_put_rot(t_queen[c], x + size / 2, y + size / 2, size, size, 180);
    else ui_put(t_queen[c], x, y, size, size);
}

static void mark_last(int s)
{
    float x, y;
    sq_xy(s, &x, &y);
    ui_mark_last(x, y, (float)sq, (float)sq);
}

static float ease(float t) { return 1 - (1 - t) * (1 - t) * (1 - t); }

static void draw(void)
{
    const ampos *p = am_pos(&G);
    float n = (float)(AM_N * sq + 2 * fr), h = sq / 2.0f, x, y;
    ui_put(t_board, bx - fr, by - fr, n, n);
    if (p->stage == AM_MOVE && G.ply >= 2) {    /* the last turn: from, to, the arrow */
        mark_last(G.step[G.ply - 2].from);
        mark_last(G.step[G.ply - 2].to);
        mark_last(G.step[G.ply - 1].to);
    } else if (p->stage == AM_SHOOT) mark_last(G.step[G.ply - 1].from);
    if (sel >= 0) {
        sq_xy(sel, &x, &y);
        ui_mark_select(x, y, (float)sq, (float)sq);
    }
    float t = 1;                                /* the animation's time, 0..1 */
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(anim.stage == AM_MOVE ? MOVE_MS : FLY_MS + BURN_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    int sliding = anim.on && anim.stage == AM_MOVE, flying = anim.on && anim.stage == AM_SHOOT;
    float fly = flying ? fminf(1, t * (FLY_MS + BURN_MS) / FLY_MS) : 1;          /* the flight, 0..1 */
    float burn = flying ? fmaxf(0, (t * (FLY_MS + BURN_MS) - FLY_MS) / BURN_MS) : 1;   /* then the fire */
    for (int s = 0; s < AM_SQUARES; s++) {      /* the burnt squares; the new one grows out of its fire */
        if (p->sq[s] != AM_ARROW || (flying && s == anim.to && fly < 1)) continue;
        sq_xy(s, &x, &y);
        float k = flying && s == anim.to ? 0.6f + 0.4f * ease(burn) : 1;
        if (k < 1) ui_mark_glow(x + h, y + h, (1.6f - 0.6f * burn) * sq);
        ui_put_c(t_burn, x + h, y + h, k * sq, k * sq);
    }
    for (int s = 0; s < AM_SQUARES; s++) {      /* the amazons */
        int c = p->sq[s] - AM_AMAZON;
        if (c < 0 || c > 1 || (drag.on && drag.moved && s == drag.from) || (sliding && s == anim.to)) continue;
        sq_xy(s, &x, &y);
        queen_at(c, x, y, (float)sq);
    }
    if (status() == GS_PLAY && sel >= 0 && !sliding)
        for (int i = 0; i < nreach; i++) {      /* where it goes: dots; where its arrow flies: gold */
            sq_xy(reach[i], &x, &y);
            if (p->stage == AM_MOVE) ui_mark_dot(x + h, y + h, 0.32f * sq);
            else {                              /* twice: bright enough on the light squares */
                ui_mark_gold(x + h, y + h, 0.8f * sq);
                ui_mark_gold(x + h, y + h, 0.5f * sq);
            }
        }
    if (sliding) {
        float x0, y0, e = ease(t);
        sq_xy(anim.from, &x0, &y0);
        sq_xy(anim.to, &x, &y);
        queen_at(p->sq[anim.to] - AM_AMAZON, x0 + (x - x0) * e, y0 + (y - y0) * e, (float)sq);
    }
    if (flying && fly < 1) {                    /* the arrow: off the amazon, up and over, down onto its square */
        float x0, y0, e = ease(fly), lift = 1 + 0.35f * sinf(3.14159265f * fly);
        sq_xy(anim.from, &x0, &y0);
        sq_xy(anim.to, &x, &y);
        double deg = atan2(y - y0, x - x0) * 57.29578;
        ui_put_rot(t_arrow, x0 + (x - x0) * e + h, y0 + (y - y0) * e + h - 0.3f * sq * (lift - 1), lift * sq,
                   lift * sq, deg);
    }
    if (drag.on && drag.moved) {                /* the dragged amazon, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? -0.25f : 0.75f;
        queen_at(p->side, drag.x - s / 2, drag.y - s * off, s);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    const ampos *p = am_pos(&G);
    int s = xy_sq(x, y);
    if (p->stage == AM_SHOOT) {                 /* the arrow: a tap on one of its squares */
        if (s >= 0 && reaches(s)) shoot(s);
        else if (s >= 0 && s != sel) ui_sound(SND_DENY);
        return;
    }
    if (s < 0) { pick(-1); return; }
    if (sel >= 0 && reaches(s)) {
        move(sel, s, 1);
        return;
    }
    if (p->sq[s] == AM_AMAZON + p->side) {
        pick(s);
        if (nreach) {
            drag.on = 1;
            drag.from = s;
            drag.moved = 0;
            drag.x = x;
            drag.y = y;
            return;
        }
    }
    ui_sound(SND_DENY);                         /* an empty square, the other's amazon, one walled in */
    pick(-1);
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
    if (!drag.moved) return;                    /* a tap: the amazon stays picked up */
    int s = xy_sq(x, y);
    if (s >= 0 && s != drag.from && reaches(s)) move(drag.from, s, 0);
    else if (s != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void token(int player, int size, uint8_t *rgba) { art_piece(QUEEN, player, size, rgba); }

/* the menu's card: a corner of the board, two amazons among burnt squares */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = ui_buf(n, n);
    art_board_n(4, 4, s, f, 1, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    static const int place[][3] = {{1, 0, 2}, {3, 0, 2}, {2, 1, 2}, {0, 2, 2}, {3, 3, 2}, {0, 1, 1}, {2, 3, 0}};
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        uint8_t *one = ui_buf(s, s);
        if (place[i][2] == 2) paint_burn(s, one);
        else art_piece(QUEEN, place[i][2], s, one);
        art_blend(rgba, size, size, one, s, s, o + f + place[i][0] * s, o + f + place[i][1] * s);
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last arrow to burn its square */

static const game amazons_game = {
    .id = "amazons", .order = 210, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .script = script, .busy = busy,
};
REGISTER_GAME(amazons_game)
