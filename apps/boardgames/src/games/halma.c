/*
 * Chinese checkers in RetroStone Board, for two, three, four or six: the six-pointed star carved into the plank, the
 * glossy marbles, tap a marble then a hole (a step, or a whole chain of jumps played hop by hop), or drag it there
 * (the rules: halma_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The star stands upright in the board's square, ivory's point at the bottom: a hole's centre is
 * (x2 / 2, (8 - row) * H) hole spacings from the middle hole (turned half round when flipped). The star's outline is a
 * carved line round its holes; the players' points are stained in their colours, the others left plain.
 * The marks the shell draws as squares (the last move, the selected marble) are drawn here as round tints of the same
 * colour, for round holes.
 */
#include <math.h>
#include <string.h>

#include "games/halma.h"
#include "shell.h"

#define FRAME 0.035f                            /* the board's frame, a part of its side */
#define H 0.8660254f                            /* from one row of holes to the next, in spacings */
#define EDGE 0.62f                              /* the star's outline, this far beyond its outer holes (spacings) */
#define FILL 0.97f                              /* how much of the plank the star takes */
#define HOLE 0.21f                              /* a hole's radius (spacings) */
#define MARBLE 0.86f                            /* a marble's width (spacings) */
#define CUT 3.897f                              /* the points' inner edges: half way from row 3 to row 4 (spacings) */
#define STEP_MS 150
#define HOP_MS 210

static hl_game G;
static int targets[HL_HOLES], ntargets;
static int q, fr, made_w = -1, made_flip = -1, made_np = -1, made_small = -1;
static float bx, by, cx, cy, s;                 /* the board's corner, the star's middle, the spacing (pixels) */
static SDL_Texture *t_board, *t_marble[HL_PLAYERS], *t_small[HL_PLAYERS], *t_empty;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, heard; hl_path p; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel = -1;
    ntargets = 0;
    drag.on = 0;
    anim.on = 0;
}

static void start(int variant)
{
    (void)variant;
    hl_new_players(&G, ui.players);
    calm();
}

static int status(void)
{
    int st = hl_status(&G), w = hl_winner(st);
    return w >= 0 ? gs_win(w) : st == HL_DRAW ? GS_DRAW : GS_PLAY;
}

static int turn(void) { return hl_turn(&G); }

static void select_hole(int h)
{
    sel = h;
    ntargets = h >= 0 ? hl_targets(&G, h, targets) : 0;
}

static int is_target(int h)
{
    for (int i = 0; i < ntargets; i++)
        if (targets[i] == h) return 1;
    return 0;
}

/* the move to a hole: animated hop by hop (a tap), or at once (a drop: the marble is already there) */
static void move_to(int from, int to, int animate)
{
    hl_path p;
    if (!hl_route(&G, from, to, &p) || !hl_play(&G, &p)) { ui_sound(SND_DENY); return; }
    select_hole(-1);
    anim.on = animate;
    anim.p = p;
    anim.heard = 0;
    anim.t0 = ui_ms();
    if (animate) ui_animate();
    else ui_sound(SND_MOVE);
}

static int undo(void)
{
    calm();
    return hl_undo(&G);
}

static int script(const char *s)
{
    calm();
    return hl_script(&G, s);
}

/* the bot's side (halma_bot.c): the game to copy, and its move ("c4-e6": the marble's hole and the hole it reaches)
 * played as a tap plays it, hop by hop */
const hl_game *halma_live(void) { return &G; }

int halma_show(const char *move)
{
    int la, lb, from = hl_hole(move, &la), to = from < 0 || move[la] != '-' ? -1 : hl_hole(move + la + 1, &lb);
    int reach[HL_HOLES], n = to < 0 || move[la + 1 + lb] || hl_status(&G) != HL_PLAY ? 0 : hl_targets(&G, from, reach);
    for (int i = 0; i < n; i++)
        if (reach[i] == to) {
            calm();
            move_to(from, to, 1);
            return 1;
        }
    return 0;
}

/* ---- the board's picture ----------------------------------------------------------------------------------------- */
/* how far (in spacings) the point (x, y) is outside the star's triangle pointing down (its tip at ivory's point), < 0
 * inside; the one pointing up: the same with y turned */
static float outside(float x, float y)
{
    static const float N[3][2] = {{0, -1}, {-0.8660254f, 0.5f}, {0.8660254f, 0.5f}};   /* its sides' normals */
    static const float V[3][2] = {{0, 6.9282f}, {-6, -3.4641f}, {6, -3.4641f}};       /* its corners */
    float in = -1e9f;
    int out = 0;
    for (int i = 0; i < 3; i++) {
        float d = x * N[i][0] + y * N[i][1] - 3.4641f;
        if (d > in) in = d;
        if (d > 0) out = 1;
    }
    if (!out) return in;
    float best = 1e9f;                          /* outside: the distance to the nearest side (round the corners) */
    for (int i = 0; i < 3; i++) {
        const float *a = V[i], *b = V[(i + 1) % 3];
        float ex = b[0] - a[0], ey = b[1] - a[1];
        float t = clampf(((x - a[0]) * ex + (y - a[1]) * ey) / (ex * ex + ey * ey), 0, 1);
        best = fminf(best, hypotf(x - a[0] - t * ex, y - a[1] - t * ey));
    }
    return best;
}

/* (x, y) turned so that the star's point k comes to the bottom (point 0): turned by -60k degrees; the top point
 * mirrored (the same, the star being symmetric, and exact) */
static void to_bottom(int k, float x, float y, float *rx, float *ry)
{
    static const float C[HL_POINTS] = {1, 0.5f, -0.5f, -1, -0.5f, 0.5f};
    static const float S[HL_POINTS] = {0, 0.8660254f, 0.8660254f, 0, -0.8660254f, -0.8660254f};
    if (k == 0 || k == 3) {
        *rx = x;
        *ry = k ? -y : y;
        return;
    }
    *rx = x * C[k] + y * S[k];
    *ry = -x * S[k] + y * C[k];
}

/* the star on the plank o (w pixels wide, its frame f, the spacing sp): the stains of the players' points (their
 * colours), the outline, the holes carved in. flip: turned half round (ebony's point at the bottom) */
static void paint_star(uint8_t *o, int w, int f, float sp, int flip, int players)
{
    static const float STAIN[2][4] = {{238, 228, 206, 0.72f}, {56, 50, 48, 0.78f}}, INK[4] = {34, 24, 16, 0.8f};
    float stain[HL_PLAYERS][4];                 /* ivory, ebony; the others: their colour, a little thinner */
    for (int p = 0; p < HL_PLAYERS; p++)
        for (int i = 0; i < 4; i++) stain[p][i] = p < 2 ? STAIN[p][i] : i < 3 ? art_player[p].fill[i] : 0.62f;
    float mid = w / 2.0f, lw = 0.9f + 0.03f * sp, R = HOLE * sp, bevel = 0.07f * sp;
    for (int py = f; py < w - f; py++)
        for (int px = f; px < w - f; px++) {
            float x = (px + 0.5f - mid) / sp, y = (py + 0.5f - mid) / sp;
            if (flip) { x = -x; y = -y; }
            float down = outside(x, y), up = outside(x, -y), star = fminf(down, up);
            if (star > EDGE + lw / sp) continue;
            uint8_t *p = o + 4 * (py * w + px);
            float c[3] = {p[0], p[1], p[2]};
            for (int k = 0; k < players; k++) {   /* the players' points stained: ivory's (bottom), ebony's... */
                float rx, ry;
                to_bottom(hl_seat(players, k), x, y, &rx, &ry);
                float a = clampf((EDGE - outside(rx, ry)) * sp + 0.5f, 0, 1)
                        * clampf((ry - CUT) * sp + 0.5f, 0, 1) * stain[k][3];
                for (int i = 0; i < 3; i++) c[i] += (stain[k][i] - c[i]) * a;
            }
            float ink = clampf(lw / 2 - fabsf(star - EDGE) * sp + 0.5f, 0, 1) * INK[3];
            for (int i = 0; i < 3; i++) c[i] += (INK[i] - c[i]) * ink;
            /* the nearest hole: a recess, its far wall lit (the light from the upper left), a bevel round it */
            int row = (int)lroundf(8 - y / H), hole = -1;
            if (row >= 0 && row < HL_ROWS) {
                int x2 = (int)(2 * floorf((x * 2 - (row & 1)) / 2 + 0.5f)) + (row & 1);
                hole = hl_at(x2, row);
            }
            if (hole >= 0) {
                float hx = mid + (flip ? -1 : 1) * hl_x2(hole) / 2.0f * sp;
                float hy = mid + (flip ? -1 : 1) * (8 - hl_row(hole)) * H * sp;
                float k = art_recess(px + 0.5f - hx, py + 0.5f - hy, R, bevel, 0.42f, 0.2f, 0.28f);
                for (int i = 0; i < 3; i++) c[i] *= k;
            }
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(c[i] + 0.5f, 0, 255);
        }
}

/* the spacing that fits the star in a plank of inner side `inner` */
static float spacing(float inner) { return FILL * inner / (2 * (8 * H + EDGE)); }

static void layout(box b)
{
    q = (int)(b.w * (1 - 2 * FRAME) / 9);
    fr = (int)((b.w - 9 * q) / 2);
    int w = 9 * q + 2 * fr;
    bx = floorf(b.x + (b.w - w) / 2);
    by = floorf(b.y + (b.h - w) / 2);
    cx = bx + w / 2.0f;
    cy = by + w / 2.0f;
    s = spacing(9.0f * q);
    if (made_w != w || made_flip != ui.flipped || made_np != G.players) {
        made_w = w;
        made_flip = ui.flipped;
        made_np = G.players;
        ui_free(&t_board);
        art_board_n(9, 9, q, fr, 0, ui_buf(w, w));
        paint_star(ui_buf(w, w), w, fr, s, ui.flipped, G.players);
        t_board = ui_tex(w, w, ui_buf(w, w));
        int d = (int)lroundf(MARBLE * s);
        for (int p = 0; p < HL_PLAYERS; p++) {
            ui_free(&t_marble[p]);
            art_counter(COUNTER_STONE, p, d, ui_buf(d, d));
            t_marble[p] = ui_tex(d, d, ui_buf(d, d));
        }
    }
    int small = (int)(0.17f * ui.unit);
    if (made_small != small) {
        made_small = small;
        static const uint8_t dark[3] = {20, 14, 8};
        for (int p = 0; p < HL_PLAYERS; p++) {
            ui_free(&t_small[p]);
            art_counter(COUNTER_STONE, p, small, ui_buf(small, small));
            t_small[p] = ui_tex(small, small, ui_buf(small, small));
        }
        ui_free(&t_empty);
        art_disc(small, dark, 0.3f, 0, ui_buf(small, small));
        t_empty = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void hole_xy(int h, float *x, float *y)
{
    float k = ui.flipped ? -s : s;
    *x = cx + k * hl_x2(h) / 2.0f;
    *y = cy + k * (8 - hl_row(h)) * H;
}

/* the hole under (x, y), or -1 (a little room round each hole) */
static int xy_hole(float x, float y)
{
    int best = -1;
    float bd = 0.62f * s;
    for (int h = 0; h < HL_HOLES; h++) {
        float hx, hy;
        hole_xy(h, &hx, &hy);
        float d = hypotf(x - hx, y - hy);
        if (d < bd) bd = d, best = h;
    }
    return best;
}

static void marble_at(int player, float x, float y, float size)
{
    ui_shadow_flat(x, y, size, size, 1);
    ui_put_c(t_marble[player], x, y, size, size);
}

/* the hop under way: where the moving marble is, and how high (0..1) it is lifted over the marble it jumps */
static int hop_at(float *x, float *y, float *lift)
{
    float x0, y0, x1, y1;
    hole_xy(anim.p.hole[0], &x0, &y0);
    hole_xy(anim.p.hole[1], &x1, &y1);
    int hops = anim.p.n - 1, step = hops == 1 && hypotf(x1 - x0, y1 - y0) < 1.5f * s;
    float t = (ui_ms() - anim.t0) / (float)(step ? STEP_MS : HOP_MS);
    int k = (int)t;
    while (anim.heard < k && anim.heard < hops) {   /* each landing heard, the last one louder */
        if (++anim.heard == hops) ui_sound(SND_MOVE);
        else ui_sound_soft(SND_MOVE, 0.55f);
    }
    if (k >= hops) return 0;
    float f = t - k;
    hole_xy(anim.p.hole[k], &x0, &y0);
    hole_xy(anim.p.hole[k + 1], &x1, &y1);
    f = step ? 1 - (1 - f) * (1 - f) * (1 - f) : f * f * (3 - 2 * f);
    *x = x0 + (x1 - x0) * f;
    *y = y0 + (y1 - y0) * f;
    *lift = step ? 0 : sinf(3.14159265f * f);
    return 1;
}

static void draw(void)
{
    float w = 9.0f * q + 2 * fr, d = MARBLE * s, m = 0.98f * s, x, y, ax = 0, ay = 0, lift = 0;
    ui_put(t_board, bx, by, w, w);
    if (anim.on && !hop_at(&ax, &ay, &lift)) anim.on = 0;
    if (anim.on) ui_animate();
    const hl_path *last = hl_last(&G);
    if (last && !anim.on)                       /* the last move's way: where it left, each landing, where it ended */
        for (int i = 0; i < last->n; i++) {
            hole_xy(last->hole[i], &x, &y);
            ui_mark_last_round(x, y, m);
        }
    if (sel >= 0) {
        hole_xy(sel, &x, &y);
        ui_mark_select_round(x, y, m);
    }
    int moving = anim.on ? anim.p.hole[anim.p.n - 1] : -1, held = drag.on && drag.moved ? drag.from : -1;
    for (int pass = 0; pass < 2; pass++)        /* the shadows first: they fall on the wood only */
        for (int h = 0; h < HL_HOLES; h++) {
            if (!G.cell[h] || h == moving || h == held) continue;
            hole_xy(h, &x, &y);
            if (pass) ui_put_c(t_marble[G.cell[h] - 1], x, y, d, d);
            else ui_shadow_flat(x, y, d, d, 1);
        }
    int won = gs_winner(status());
    if (won >= 0 && !anim.on)                   /* the winner's full point lit in gold, once it has landed */
        for (int h = 0; h < HL_HOLES; h++) {
            if (!hl_dest(&G, won, h)) continue;
            hole_xy(h, &x, &y);
            ui_mark_gold(x, y, 1.15f * s);
        }
    for (int i = 0; i < ntargets; i++) {        /* where the selected marble can go */
        hole_xy(targets[i], &x, &y);
        ui_mark_dot(x, y, 0.62f * s);           /* wider than the hole: its rim round it (inside, it hides) */
    }
    if (anim.on) {                              /* hopping: lifted over the marble it jumps, its shadow left below */
        int p = G.cell[moving] - 1;
        float k = 1 + 0.16f * lift;
        ui_shadow_flat(ax, ay, d, d, 1 - 0.45f * lift);
        ui_put_c(t_marble[p], ax, ay - 0.22f * d * lift, d * k, d * k);
    }
    if (held >= 0) {                            /* the dragged marble, a little bigger, beside the finger */
        float k = 1.15f * d, off = ui_upside(turn()) ? 0.3f : -0.3f;
        marble_at(G.cell[held] - 1, drag.x, drag.y + off * k, k);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    int h = xy_hole(x, y);
    if (h < 0) { select_hole(-1); return; }
    if (sel >= 0 && is_target(h)) {
        move_to(sel, h, 1);
        return;
    }
    if (G.cell[h] == turn() + 1) {
        select_hole(h);
        drag.on = 1;
        drag.from = h;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    if (sel >= 0 || G.cell[h]) ui_sound(SND_DENY);
    select_hole(-1);
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > s * 0.2f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the marble stays selected */
    int h = xy_hole(x, y);
    if (h >= 0 && is_target(h)) move_to(drag.from, h, 0);
    else if (h != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the marbles already home: the point they fill (a small triangle of ten, its tip first), then their number */
static void bar(int player, box b)
{
    int n = hl_arrived(&G, player);
    float u = ui.unit, t = 0.17f * u, gap = 0.2f * u, mid = b.y + b.h / 2, x0 = b.x + 2 * gap;
    for (int r = 0, i = 0; r < 4; r++)
        for (int c = 0; c <= r; c++, i++) {
            float x = x0 + (c - r / 2.0f) * gap, y = mid + (r - 1.5f) * gap * H;
            ui_put_c(i < n ? t_small[player] : t_empty, x, y, i < n ? t : 0.6f * t, i < n ? t : 0.6f * t);
        }
    ui_number(x0 + 2 * gap + 0.25f * u + 0.3f * u, mid, 0.46f * u, n, 1);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_STONE, player, size, rgba); }

/* where each player sits: the tip of their point of the star (the side points: the bar of their side, top or bottom) */
static void home(int player, float *fx, float *fy)
{
    static const float DIR[HL_POINTS][2] = {{0, 1}, {-0.866f, 0.5f}, {-0.866f, -0.5f}, {0, -1}, {0.866f, -0.5f},
                                            {0.866f, 0.5f}};
    int k = hl_seat(ui.players, player);
    k = k < 0 ? 0 : k;
    *fx = 0.5f + 0.4f * DIR[k][0];
    *fy = 0.5f + 0.4f * DIR[k][1];
}

/* the menu's card: the whole star, a game under way */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    static hl_game T;
    hl_new(&T);
    hl_script(&T, "d4-i5 a14-e13 c3-d4 a15-a14 b3-h5 b15-f13 d4-i6 a14-d12");
    int f = (int)(size * 0.04f), qq = (size - 2 * f) / 9, w = 9 * qq + 2 * f, o = (size - w) / 2;
    float sp = spacing(9.0f * qq), mid = w / 2.0f;
    uint8_t *board = ui_buf(w, w);
    art_board_n(9, 9, qq, f, 0, board);
    paint_star(board, w, f, sp, 0, 2);
    art_blend(rgba, size, size, board, w, w, o, o);
    int d = (int)lroundf(MARBLE * sp);
    for (int pass = 0; pass < 2; pass++)       /* the shadows, then the marbles */
        for (int h = 0; h < HL_HOLES; h++) {
            if (!T.cell[h]) continue;
            float x = o + mid + hl_x2(h) / 2.0f * sp, y = o + mid + (8 - hl_row(h)) * H * sp;
            uint8_t *one = ui_buf(d, d);
            if (pass) art_counter(COUNTER_STONE, T.cell[h] - 1, d, one);
            else art_flat_shadow(d, one);
            int ox = (int)lroundf(x - d / 2.0f), oy = (int)lroundf(y - d / 2.0f);
            art_blend(rgba, size, size, one, d, d, pass ? ox : ox + d / 20, pass ? oy : oy + d * 8 / 100);
        }
}

static int busy(void) { return anim.on; }     /* the end waits for the last marble to land */

static const game halma_game = {
    .id = "halma", .order = 250, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
    .players = 1u << 2 | 1u << 3 | 1u << 4 | 1u << 6, .home = home,
};
REGISTER_GAME(halma_game)
