/*
 * The Royal Game of Ur in RetroStone Board: the board of twenty inlaid squares and its five rosettes, the pieces
 * (the house's discs), four pyramid dice thrown with a tap, the pieces hopping along their path (the rules and the
 * notation: ur_rules.c, ur.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board lies across the room the shell gives, the four-square end on the left (seen by player 0): each player's
 * own row next to them, the shared row in the middle. Next to each player, on the table: their pieces waiting (on the
 * left, by the path's start) and those borne off (on the right, by its end), and their four dice and the throw's
 * count. The board turns round when flipped. Where the room is tall (portrait) the board takes the whole width and
 * the racks and dice the height beyond its long sides: the racks under the board's two halves (in two rows of whole
 * pieces when there is room), the dice further out, bigger. Where it is wide (landscape) the racks stand at the
 * board's ends instead, each player's in their half (waiting at the left end, borne off at the right), the dice
 * alone beyond the long sides: the board grows with the height.
 * The dice: tetrahedra seen from above, a player's colour; a marked tip (a dot) pointing up counts one.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/lines.h"
#include "games/ur.h"
#include "shell.h"

#define FRAME 0.35f                             /* the board's frame, in cells */
#define WIDE (8 + 2 * FRAME)                    /* the board's width with its frame, in cells */
#define PIECE 0.98f                             /* a piece's picture, in cells (the disc fills 0.88 of it) */
#define RACK 0.82f                              /* the pieces off the board, a little smaller... */
#define RACK_STEP 0.5f                          /* ...and overlapping */
#define RACK_V 0.97f                            /* the racks' distance from the board's edge (cells) */
#define DIE 0.92f                               /* a die's picture, in cells (at the least) */
#define DIE_BIG 1.5f                            /* (at the most) */
#define DIE_STEP 1.05f
#define DICE_V 2.12f                            /* the dice's distance from the board's edge */
#define ENDS_GAP 0.2f                           /* landscape: the racks at the board's ends, so far from its frame */
#define HOP_MS 130
#define BACK_MS 320                             /* a piece sent back slides to its rack */
#define FLASH_MS 520                            /* a rosette's gold light when a piece lands on it */

static ugame G;
static int cs, fr, made = -1, made_die = -1, made_small = -1;
static float bx, by;                            /* the board's cells' top left corner (as drawn, not flipped) */
static box sq;                                  /* the room */
/* where the pieces off the board and the dice lie (cells; layout() chooses): the racks at the board's ends
 * (landscape) or beyond its long sides, in two rows of whole pieces or one row overlapping; the pieces' size there;
 * the dice's size and their row's distance from the board's edge; the racks' row (or the ends' columns: their step) */
static int ends, two;
static float rk_s = RACK, rk_v = RACK_V, rk_step = RACK_STEP, die_s = DIE, die_v = DICE_V;
static SDL_Texture *t_board, *t_piece[2], *t_small[2], *t_die[2][2];
static uint8_t rolls[UR_MAX_EVENTS];            /* each throw's dice (bit i: die i's marked tip up), kept through
                                                 * undo: undo never rethrows */
static int nrolls;
static struct { int on, k; Uint32 t0; } roll;   /* the throw that tumbles (k: the throws before it) */
static int pass_snd;                            /* a throw lost: its sound once the dice lie still */
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, player, from, to, hops, heard, caught; Uint32 t0; upos before; } anim;
static const uint8_t ROSE_WOOD[3] = {124, 62, 48};

/* ---- where things are -------------------------------------------------------------------------------------------- */
/* a place's centre, in cells from the board's top left corner, the board as player 0 sees it; k: the slot in a rack
 * (place 0: the pieces waiting, 15: those borne off) */
static void spot(int player, int place, int k, float *u, float *v)
{
    int c, r;
    if (ur_cell(player, place, &c, &r)) {
        *u = c + 0.5f;
        *v = r + 0.5f;
        return;
    }
    float rad = 0.44f * PIECE * rk_s;           /* a rack's piece's radius */
    if (ends) {                                 /* a column at the board's end, from the middle towards the player */
        *u = place ? 8 + FRAME + ENDS_GAP + rad : -FRAME - ENDS_GAP - rad;
        *v = 1.62f + rad + rk_step * k;
    } else if (two) {                           /* two rows of whole pieces, four and three, under a half */
        *u = (place ? 7.5f : 3.5f) - k % 4;
        *v = 3 + rk_v + (k / 4) * (2 * rad + 0.08f);
    } else {
        *u = (place ? 7.5f : 3.5f) - rk_step * k;
        *v = 3 + rk_v;
    }
    if (player) *v = 3 - *v;                    /* player 1's beyond the other side */
}

static void to_px(float u, float v, float *x, float *y)    /* cells to pixels, turned round when flipped */
{
    if (ui.flipped) u = 8 - u, v = 3 - v;
    *x = bx + u * cs;
    *y = by + v * cs;
}

static void place_px(int player, int place, int k, float *x, float *y)
{
    float u, v;
    spot(player, place, k, &u, &v);
    to_px(u, v, x, y);
}

/* the slot a piece leaving `place` takes in its rack (the last one waiting), or arriving there (the next free) */
static int slot(const upos *p, int player, int place)
{
    return place == 0 ? p->n[player][0] - 1 : place == UR_OFF ? p->n[player][UR_OFF] : 0;
}

/* a player's die i (4: the count after them), on their side of the screen, in a row read from their seat */
static void die_px(int player, int i, float *x, float *y)
{
    float u = 4 + (DIE_STEP * i + (i == 4 ? 0.15f : 0) - 2.0f) * die_s / DIE;
    if (ui_upside(player)) u = 8 - u;
    *x = bx + u * cs;
    *y = by + (player == ui_bottom() ? 3 + die_v : -die_v) * cs;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    anim.on = drag.on = pass_snd = roll.on = 0;
}

static void start(int variant)
{
    (void)variant;
    ur_new(&G);
    nrolls = 0;
    calm();
}

static int status(void) { return G.pos.winner < 0 ? GS_PLAY : G.pos.winner ? GS_WIN1 : GS_WIN0; }
static int turn(void) { return G.pos.turn; }
static int tumbling(void) { return roll.on && ui_ms() - roll.t0 < UI_DICE_MS; }
static int busy(void) { return anim.on || tumbling(); }   /* the end waits for the last piece */

static int count(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) + (m >> 3 & 1); }

/* the dice of a throw given by its count only (a script): one of the ways to throw it, always the same */
static int mask_of(int k, int value)
{
    static const uint8_t way[5][6] = {{0}, {1, 4, 2, 8}, {5, 10, 3, 12, 6, 9}, {7, 13, 11, 14}, {15}};
    static const int ways[5] = {1, 4, 6, 4, 1};
    return way[value][k % ways[value]];
}

static void throw_dice(void)
{
    int k = G.pos.throws, m = 0;
    if (G.n >= UR_MAX_EVENTS) return;
    if (k < nrolls) m = rolls[k];               /* thrown before an undo: the same dice again */
    else {
        for (int i = 0; i < 4; i++) m |= ui_random(2) << i;
        rolls[k] = (uint8_t)m;
        nrolls = k + 1;
    }
    int r = ur_throw(&G, count(m));
    if (!r) return;
    roll.on = 1;
    roll.k = k;
    roll.t0 = ui_ms();
    pass_snd = r == UT_PASS;
    ui_sound(SND_DICE);
    ui_animate();
}

static void play(int from, int animate)
{
    upos before = G.pos;
    int to = ur_dest(&before, from), r = ur_move(&G, from);
    if (!r) { ui_sound(SND_DENY); return; }
    anim.on = animate;
    anim.player = before.turn;
    anim.from = from;
    anim.to = to;
    anim.hops = to - from;
    anim.heard = 0;
    anim.caught = r > 1;
    anim.t0 = ui_ms();
    anim.before = before;
    drag.on = 0;
    if (!animate) ui_sound(r > 1 ? SND_CAPTURE : to == UR_OFF ? SND_PLACE : SND_MOVE);
    ui_animate();
}

static int undo(void)
{
    if (!ur_undo(&G)) return 0;
    calm();
    return 1;
}

/* "2 a 3 c 0": throws and the places moved from (ur.h) */
static int script(const char *s)
{
    int ok = ur_script(&G, s);
    for (int i = 0; i < G.n; i++)               /* its throws are the game's from now on */
        if (G.ev[i] < UR_MOVE_EV) rolls[G.past[i].throws] = (uint8_t)mask_of(G.past[i].throws, G.ev[i]);
    nrolls = G.pos.throws;
    calm();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a rosette, the board's jewel: eight cream petals round a heart, in a ring of dots, on a tile of rosewood */
static void paint_rosette(uint8_t *o, int pw, int ph, int x, int y, int w)
{
    art_fig *f = art_fig_new();
    float pt[2 * 26];
    for (int i = 0; i < 8; i++) {               /* a petal: a pointed leaf from the heart outwards */
        float a = (22.5f + 45 * i) * ART_PI / 180, ca = cosf(a), sa = sinf(a);
        int n = 0;
        for (int s = 0; s <= 12; s++) {
            float t = s / 12.0f, r = 0.07f + 0.31f * t, h = 0.075f * sinf(ART_PI * t) * (1.15f - 0.3f * t);
            pt[n++] = 0.5f + r * ca - h * sa;
            pt[n++] = 0.5f + r * sa + h * ca;
        }
        for (int s = 11; s >= 1; s--) {
            float t = s / 12.0f, r = 0.07f + 0.31f * t, h = 0.075f * sinf(ART_PI * t) * (1.15f - 0.3f * t);
            pt[n++] = 0.5f + r * ca + h * sa;
            pt[n++] = 0.5f + r * sa - h * ca;
        }
        art_poly(f, ART_PART, pt, n / 2);
    }
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.1f, 0.1f);
    art_ellipse(f, ART_MARK, 0.5f, 0.5f, 0.045f, 0.045f);
    for (int i = 0; i < 8; i++) {               /* the dots between the petals' tips */
        float a = 45 * i * ART_PI / 180;
        art_ellipse(f, ART_PART, 0.5f + 0.41f * cosf(a), 0.5f + 0.41f * sinf(a), 0.028f, 0.028f);
    }
    art_style st = art_flat(ART_PALE, ART_INK, fmaxf(0.018f, 1.0f / w), 0.25f);
    memcpy(st.mark, (float[3]){ROSE_WOOD[0], ROSE_WOOD[1], ROSE_WOOD[2]}, sizeof st.mark);
    art_fig_blend(o, pw, ph, f, &st, x, y, w, 1);
    art_fig_free(f);
}

/* the other squares' motifs, inked lightly: five dots, or an eye (a ring round a dot) */
static void paint_motif(uint8_t *o, int pw, int ph, float cx, float cy, float w, int eye)
{
    art_fig *f = art_fig_new();
    if (eye) {
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.17f, 0.23f, 0, 360);
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.07f, 0.07f);
        for (int i = 0; i < 4; i++) {
            float a = (45 + 90 * i) * ART_PI / 180;
            art_ellipse(f, ART_PART, 0.5f + 0.36f * cosf(a), 0.5f + 0.36f * sinf(a), 0.045f, 0.045f);
        }
    } else {
        for (int i = 0; i < 5; i++) {
            float dx = i == 4 ? 0 : (i & 1 ? 0.22f : -0.22f), dy = i == 4 ? 0 : (i & 2 ? 0.22f : -0.22f);
            art_ellipse(f, ART_PART, 0.5f + dx, 0.5f + dy, 0.075f, 0.075f);
        }
    }
    art_style st = art_flat(ART_INK, ART_INK, 0, 0);
    int s = (int)w;
    art_fig_blend(o, pw, ph, f, &st, (int)(cx - s / 2.0f), (int)(cy - s / 2.0f), s, 0.5f);
    art_fig_free(f);
}

/* the board for cells of c pixels in a frame of f: (8 c + 2 f) x (3 c + 2 f). The house plank and frame; the gap
 * between the two ends of each player's row in the frame's walnut; the twenty squares inlaid: light maple with their
 * motifs, the five rosettes on rosewood. */
static void paint_board(uint8_t *o, int c, int f)
{
    static const uint8_t walnut[3] = {92, 60, 40};
    static const float maple[3] = {1.07f, 1.1f, 1.18f}, rose[3] = {0.6f, 0.4f, 0.36f};
    int pw = 8 * c + 2 * f, ph = 3 * c + 2 * f;
    art_board_n(8, 3, c, f, 0, o);
    for (int side = 0; side < 2; side++) {      /* the gaps: walnut, the frame reaching into the board */
        int x0 = f + 4 * c, x1 = f + 6 * c, rim = (int)(0.2f * f);
        int y0 = side ? f + 2 * c : rim, y1 = side ? ph - rim : f + c;
        float groove = fmaxf(1, 0.14f * f);
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) {
                float g = sinf((x + y * 0.15f) * 0.09f + 1.7f * sinf(y * 0.013f)) * 0.5f + 0.5f;
                float k = 1 + 0.06f * (g - 0.5f);
                float d = fminf(fminf(x - x0 + 0.5f, x1 - x - 0.5f), side ? y - y0 + 0.5f : y1 - y - 0.5f);
                if (d < groove) k *= 0.62f;     /* the groove along the squares, as on the frame */
                uint8_t *p = o + 4 * ((size_t)y * pw + x);
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(walnut[i] * k, 0, 255);
            }
    }
    float m = fmaxf(1, 0.045f * c), seam = fmaxf(1, 0.05f * c);
    for (int r = 0; r < 3; r++)
        for (int col = 0; col < 8; col++) {
            if (r != 1 && (col == 4 || col == 5)) continue;
            int ros = (r != 1 && (col == 0 || col == 6)) || (r == 1 && col == 3);
            float x = f + col * c + m, y = f + r * c + m, w = c - 2 * m;
            art_inlay(o, pw, ph, x, y, w, w, 0.08f * c, ros ? rose : maple, seam);
            if (ros) paint_rosette(o, pw, ph, (int)x, (int)y, (int)w);
            else paint_motif(o, pw, ph, x + w / 2, y + w / 2, 0.8f * w, r == 1 ? (col & 1) == 0 : col == 7 || col == 2);
        }
}

/* a pyramid die seen from above: three faces round its top, lit from the upper left; the tip marked or not */
static void paint_die(int player, int marked, int size, uint8_t *o)
{
    static const float top[2] = {0.5f, 0.56f}, corner[3][2] = {{0.5f, 0.06f}, {0.95f, 0.84f}, {0.05f, 0.84f}};
    static const float light[2][3] = {{1.04f, 0.86f, 0.72f}, {1.75f, 1.25f, 0.9f}};   /* left, right, front */
    static const int face[3][2] = {{2, 0}, {0, 1}, {1, 2}};
    art_style st = art_player[player];
    st.line = fmaxf(0.03f, 1.3f / size);
    st.light = 0;
    art_fig *f = art_fig_new();
    float tri[6] = {corner[0][0], corner[0][1], corner[1][0], corner[1][1], corner[2][0], corner[2][1]};
    art_poly(f, ART_PART, tri, 3);
    art_render(f, &st, size, size, (float)size, 0, 0, o);   /* the outline */
    art_fig_free(f);
    for (int j = 0; j < 3; j++) {               /* the faces, each its own light */
        art_style fs = art_player[player];
        fs.line = 0;
        fs.light = 0;
        for (int k = 0; k < 3; k++) fs.fill[k] = fminf(255, fs.fill[k] * light[player][j]);
        art_fig *g = art_fig_new();
        const float *a = corner[face[j][0]], *b = corner[face[j][1]];
        float pt[6] = {top[0], top[1], a[0], a[1], b[0], b[1]};
        art_poly(g, ART_PART, pt, 3);
        art_fig_blend(o, size, size, g, &fs, 0, 0, size, 1);
        art_fig_free(g);
    }
    uint8_t ridge[3], mark[3];
    for (int k = 0; k < 3; k++) {
        ridge[k] = (uint8_t)art_player[player].inner[k];
        mark[k] = (uint8_t)art_player[player].mark[k];
    }
    for (int j = 0; j < 3; j++)
        lines_band(o, size, top[0] * size, top[1] * size, corner[j][0] * size, corner[j][1] * size,
                   fmaxf(1, 0.025f * size), ridge, 0.45f);
    if (marked) {                               /* the tip's inlay: a dot of the other colour in a thin ring */
        lines_dot(o, size, top[0] * size, top[1] * size, 0.125f * size, ridge);
        lines_dot(o, size, top[0] * size, top[1] * size, 0.1f * size, mark);
    }
}

static void layout(box b)
{
    sq = b;
    /* the racks and the dice beyond the long sides (at the least as in the 9-unit square), or the racks at the ends
     * and the dice alone beyond the sides: whichever gives the bigger board */
    float r1 = 0.44f * PIECE, rr = r1 * RACK;  /* a piece's radius, whole and in a rack */
    float c_stack = fminf(b.w / WIDE, b.h / (3 + 2 * (DICE_V + DIE / 2)));
    float c_ends = fminf(b.w / (WIDE + 2 * (ENDS_GAP + 2 * rr) + 0.1f), b.h / (3 + 2 * (FRAME + 0.25f + DIE)));
    ends = c_ends > 1.02f * c_stack;
    cs = (int)floorf(ends ? c_ends : c_stack);
    float side = (b.h / cs - 3) / 2;            /* the room beyond each long side, cells from the board's squares */
    two = 0;
    rk_s = RACK;
    rk_step = RACK_STEP;
    rk_v = RACK_V;
    if (ends) {                                 /* the columns as long as the room lets them, the dice beyond */
        rk_step = clampf((3 + side - rr - 0.05f - (1.62f + rr)) / (UR_PIECES - 1), 0.3f, RACK_STEP);
        die_s = clampf(side - FRAME - 0.3f, DIE, 1.2f);
        die_v = FRAME + 0.25f + die_s / 2;
    } else if (side >= FRAME + 0.15f + 4 * r1 + 0.08f + 0.3f + 1.0f + 0.05f) {
        two = 1;                                /* tall: two rows of whole pieces, big dice further out */
        rk_s = 1;
        rk_v = FRAME + 0.15f + r1;
        float racks = rk_v + 3 * r1 + 0.08f;    /* the racks' outer edge */
        die_s = clampf(side - racks - 0.35f, 1.0f, DIE_BIG);
        die_v = racks + 0.3f + fminf(0.6f, (side - racks - 0.35f - die_s) / 2) + die_s / 2;
    } else {                                    /* one row of pieces overlapping, the dice as big as the room lets */
        die_s = clampf(side - RACK_V - rr - 0.33f - 0.04f, DIE, 1.3f);
        die_v = RACK_V + rr + 0.33f + die_s / 2;
    }
    fr = (int)(cs * FRAME);
    bx = floorf(b.x + (b.w - 8.0f * cs) / 2);
    by = floorf(b.y + (b.h - 3.0f * cs) / 2);
    int ds = (int)(die_s * cs);
    if (made_die != ds) {
        made_die = ds;
        for (int p = 0; p < 2; p++)
            for (int k = 0; k < 2; k++) {
                ui_free(&t_die[p][k]);
                paint_die(p, k, ds, ui_buf(ds, ds));
                t_die[p][k] = ui_tex(ds, ds, ui_buf(ds, ds));
            }
    }
    if (made != cs) {
        made = cs;
        int pw = 8 * cs + 2 * fr, ph = 3 * cs + 2 * fr;
        uint8_t *o = malloc((size_t)pw * ph * 4);
        if (o) {
            paint_board(o, cs, fr);
            ui_free(&t_board);
            t_board = ui_tex(pw, ph, o);
            free(o);
        }
        int ps = (int)(PIECE * cs);
        for (int p = 0; p < 2; p++) {
            ui_free(&t_piece[p]);
            art_counter(COUNTER_DISC, p, ps, ui_buf(ps, ps));
            t_piece[p] = ui_tex(ps, ps, ui_buf(ps, ps));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int p = 0; p < 2; p++) {
            ui_free(&t_small[p]);
            art_counter(COUNTER_DISC, p, small, ui_buf(small, small));
            t_small[p] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void piece_at(int player, float x, float y, float scale, float alpha)
{
    ui_flat_at(t_piece[player], x, y, PIECE * cs * scale, alpha, ui_upside(player));
}

/* the latest throw of a player: its event (-1: none yet) */
static int last_throw(int player)
{
    for (int i = G.n - 1; i >= 0; i--)
        if (G.ev[i] < UR_MOVE_EV && G.past[i].turn == player) return i;
    return -1;
}

/* a player's four dice: waiting to be thrown (in gold light), tumbling, the throw to play, or the last one (dimmed;
 * in a red glow when it was lost); the count next to them */
static void draw_dice(int q)
{
    const upos *p = &G.pos;
    int e = last_throw(q), k = e >= 0 ? G.past[e].throws : -1, rolling = tumbling() && roll.k == k && k >= 0;
    int active = p->phase != UR_OVER && p->turn == q, invite = active && p->phase == UR_THROW && !tumbling();
    if (e < 0 && !invite) return;
    int mask = e >= 0 ? rolls[k] : 0, lost = e == G.n - 1 && p->turn != q && !rolling;
    float alpha = active || rolling ? 1 : 0.45f, s = die_s * cs, k_s = die_s / DIE, x, y;
    if (invite && e >= 0) alpha = 0.8f;
    float rot = ui_upside(q) ? 180 : 0;
    if (lost) {
        die_px(q, 1, &x, &y);
        float x2, y2;
        die_px(q, 2, &x2, &y2);
        ui_mark_glow((x + x2) / 2, y, 4.6f * k_s * cs);
    }
    for (int i = 0; i < 4; i++) {
        die_px(q, i, &x, &y);
        if (invite) ui_mark_gold(x, y, 2.1f * k_s * cs);
    }
    Uint32 t = ui_ms() - roll.t0;
    for (int i = 0; i < 4; i++) {
        die_px(q, i, &x, &y);
        int marked = mask >> i & 1;
        float tilt = (art_hash(k, i) - 0.5f) * 24, turn_by = 0, lift = 0;
        if (rolling) {                          /* tumbling: the tip flickers, the die spins and bounces */
            ui_die_tumble((int)t, i, &turn_by, &lift);
            marked = (int)((t / 70 + (unsigned)i * 3 + (unsigned)mask) % 2);
        }
        ui_shadow_flat(x, y + 0.05f * s, s * 1.0f, s * 0.92f, alpha);
        SDL_Texture *d = t_die[q][marked];
        SDL_SetTextureAlphaMod(d, (Uint8)(255 * alpha));
        ui_put_rot(d, x, y - lift * s, s, s, tilt + turn_by + rot);
        SDL_SetTextureAlphaMod(d, 255);
    }
    if (rolling) ui_animate();
    else if (e >= 0) {
        die_px(q, 4, &x, &y);
        ui_number_rot(x, y, 0.62f * k_s * cs, G.ev[e], 1, rot);
    }
}

/* the last move: the squares it left and reached */
static void mark_last(void)
{
    for (int i = G.n - 1; i >= 0; i--) {
        if (G.ev[i] < UR_MOVE_EV) continue;
        const upos *b = &G.past[i];
        int from = G.ev[i] - UR_MOVE_EV, ends[2] = {from, from + b->roll};
        for (int k = 0; k < 2; k++) {
            if (ends[k] < 1 || ends[k] > 14) continue;
            float x, y;
            place_px(b->turn, ends[k], 0, &x, &y);
            ui_mark_last(x - cs / 2.0f, y - cs / 2.0f, (float)cs, (float)cs);
        }
        return;
    }
}

/* where the pieces that can move go: dots (under the pieces), or rings round a piece sent back (over them) */
static void draw_targets(int moves, int rings)
{
    const upos *p = &G.pos;
    for (int f = 0; f < UR_OFF; f++) {
        if (!(moves >> f & 1)) continue;
        if (drag.on && !drag.moved && drag.from != f) continue;   /* a piece pressed: its own target only */
        int to = ur_dest(p, f), hit = ur_takes(p, f);
        float x, y;
        place_px(p->turn, to, slot(p, p->turn, to), &x, &y);
        if (hit && rings) ui_mark_ring(x, y, 1.18f * cs);
        else if (!hit && !rings) ui_mark_dot(x, y, 0.36f * cs);
    }
}

/* the pieces of a position: the racks and the board; skip: a place of the player to move not drawn (moving),
 * hide_back: the other player's piece on that place not drawn (sliding back) */
static void draw_pieces(const upos *p, int mover, int skip, int hide)
{
    for (int pl = 0; pl < 2; pl++) {
        for (int k = 0; k < p->n[pl][0]; k++) { /* waiting: from the far end, so the next to go lies on top */
            if (pl == mover && skip == 0 && k == p->n[pl][0] - 1) continue;
            float x, y;
            place_px(pl, 0, k, &x, &y);
            piece_at(pl, x, y, rk_s, 1);
        }
        for (int k = 0; k < p->n[pl][UR_OFF]; k++) {
            float x, y;
            place_px(pl, UR_OFF, k, &x, &y);
            piece_at(pl, x, y, rk_s, 1);
        }
        for (int s = 1; s < UR_OFF; s++) {
            if (!p->n[pl][s] || (pl == mover && s == skip) || (pl != mover && s == hide)) continue;
            float x, y;
            place_px(pl, s, 0, &x, &y);
            piece_at(pl, x, y, 1, 1);
        }
    }
}

static void draw(void)
{
    const upos *p = &G.pos;
    float pw = 8.0f * cs + 2 * fr, ph = 3.0f * cs + 2 * fr, x, y, t = 0;
    ui_put_rot(t_board, bx - fr + pw / 2, by - fr + ph / 2, pw, ph, ui.flipped ? 180 : 0);
    if (anim.on) {                              /* a hop a beat, the sound as it lands on each square */
        t = (ui_ms() - anim.t0) / (float)HOP_MS;
        int landed = t >= anim.hops ? anim.hops : (int)t;
        while (anim.heard < landed) {
            int last = ++anim.heard == anim.hops;
            ui_sound(last && anim.caught ? SND_CAPTURE : last && anim.to == UR_OFF ? SND_PLACE : SND_MOVE);
        }
        float end = anim.hops + (anim.caught ? (float)BACK_MS / HOP_MS : 0) +
                    (ur_rosette(anim.to) ? (float)FLASH_MS / HOP_MS : 0);
        if (t >= end) anim.on = 0;
        else ui_animate();
    }
    mark_last();
    draw_dice(0);
    draw_dice(1);
    if (pass_snd && !tumbling()) {
        pass_snd = 0;
        ui_sound(SND_DENY);
    }
    if (anim.on && t >= anim.hops && ur_rosette(anim.to)) {   /* a rosette reached: a gold light, another throw */
        place_px(anim.player, anim.to, 0, &x, &y);
        ui_mark_gold(x, y, 2.0f * cs);
    }
    int moves = p->phase == UR_MOVE && !busy() ? ur_movable(p) : 0, me = p->turn;
    int one = __builtin_popcount((unsigned)moves) == 1;
    int sel = drag.on && !drag.moved ? drag.from : one ? __builtin_ctz((unsigned)moves) : -1;
    if (sel >= 0 && moves) {                    /* the piece pressed, or the only one that can move */
        place_px(me, sel, slot(p, me, sel), &x, &y);
        if (sel == 0) ui_mark_select_round(x, y, rk_s * PIECE * cs * 1.1f);
        else ui_mark_select(x - cs / 2.0f, y - cs / 2.0f, (float)cs, (float)cs);
    }
    draw_targets(moves, 0);
    if (anim.on) draw_pieces(&anim.before, anim.player, anim.from, anim.caught && t >= anim.hops ? anim.to : -1);
    else draw_pieces(p, me, drag.on && drag.moved ? drag.from : -1, -1);
    draw_targets(moves, 1);
    if (anim.on && anim.caught && t >= anim.hops) {   /* the piece sent back slides to its rack */
        float f = ui_ease(clampf((t - anim.hops) * HOP_MS / BACK_MS, 0, 1)), x1, y1;
        int other = !anim.player;
        place_px(other, anim.to, 0, &x, &y);
        place_px(other, 0, anim.before.n[other][0], &x1, &y1);
        piece_at(other, x + (x1 - x) * f, y + (y1 - y) * f, 1 - (1 - rk_s) * f, 1);
    }
    if (anim.on) {                              /* the piece hopping square by square */
        int h = (int)t;
        float f = t - h, x1, y1;
        if (h >= anim.hops) h = anim.hops - 1, f = 1;
        int a = anim.from + h, b = a + 1;
        place_px(anim.player, a, slot(&anim.before, anim.player, a), &x, &y);
        place_px(anim.player, b, slot(&anim.before, anim.player, b), &x1, &y1);
        float e = ui_ease(f), up = (ui_upside(anim.player) ? -1 : 1) * ui_hop(f) * cs;
        float sc = (a == 0 ? rk_s + (1 - rk_s) * e : 1) * (b == UR_OFF ? 1 - (1 - rk_s) * e : 1);
        piece_at(anim.player, x + (x1 - x) * e, y + (y1 - y) * e - up, sc * (1 + 0.1f * sinf(ART_PI * f)), 1);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, above the finger */
        float off = (ui_upside(me) ? -0.5f : 0.5f) * cs;
        piece_at(me, drag.x, drag.y - off, 1.15f, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the place of the player to move's piece that can go, under (x, y): 1, or its target there: 2; else 0 */
static int hit(float x, float y, int *from)
{
    const upos *p = &G.pos;
    int me = p->turn, moves = ur_movable(p), best = -1;
    float near = 0.62f * cs, px, py, d;
    for (int f = 0; f < UR_OFF; f++) {
        if (!(moves >> f & 1)) continue;
        for (int k = 0; k < (f ? 1 : p->n[me][0]); k++) {   /* any piece waiting will do */
            place_px(me, f, k, &px, &py);
            d = hypotf(x - px, y - py) * (f ? 1 : 1.3f);
            if (d < near) near = d, best = f;
        }
    }
    if (best >= 0) {
        *from = best;
        return 1;
    }
    for (int f = 0; f < UR_OFF; f++) {
        if (!(moves >> f & 1)) continue;
        int to = ur_dest(p, f);
        place_px(me, to, slot(p, me, to), &px, &py);
        d = hypotf(x - px, y - py);
        if (d < near) near = d, best = f;
    }
    if (best < 0) return 0;
    *from = best;
    return 2;
}

static void press(float x, float y)
{
    const upos *p = &G.pos;
    if (p->phase == UR_OVER || busy()) return;
    if (p->phase == UR_THROW) {                 /* a tap anywhere on the board's square throws */
        if (x >= sq.x && y >= sq.y && x < sq.x + sq.w && y < sq.y + sq.h) throw_dice();
        return;
    }
    int from, h = hit(x, y, &from);
    if (h == 1) {
        drag.on = 1;
        drag.from = from;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
    } else if (h == 2) {
        play(from, 1);
    } else {
        ui_sound(SND_DENY);
    }
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > cs * 0.25f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) { play(drag.from, 1); return; }   /* a tap: the piece goes, hopping */
    const upos *p = &G.pos;
    int to = ur_dest(p, drag.from);
    float px, py;
    place_px(p->turn, to, slot(p, p->turn, to), &px, &py);
    if (hypotf(x - px, y - py) < 0.8f * cs) play(drag.from, 0);
    else ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)               /* the pieces the player has borne off */
{
    int n = G.pos.n[player][UR_OFF];
    float sz = 0.6f * ui.unit, step = fminf(0.42f * ui.unit, (b.w - sz) / (UR_PIECES - 1)), cy = b.y + b.h / 2;
    for (int i = 0; i < n; i++) ui_put(t_small[player], b.x + i * step, cy - sz / 2, sz, sz);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card: the board's four-square end with its rosettes, pieces on it, two dice */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int c = (int)(size / 5.2f), f = (int)(c * FRAME), pw = 8 * c + 2 * f, ph = 3 * c + 2 * f;
    uint8_t *o = malloc((size_t)pw * ph * 4);
    if (!o) return;
    paint_board(o, c, f);
    int w = 5 * c + f + f / 2, ox = (size - w) / 2 + c / 3, oy = (size - ph - c / 3 - (int)(DIE * c)) / 2;
    if (w > size) w = size;
    for (int y = 0; y < ph; y++)               /* the left end: cols 0..4 */
        memmove(o + 4 * (size_t)y * w, o + 4 * (size_t)y * pw, 4 * (size_t)w);
    art_blend(rgba, size, size, o, w, ph, ox - c / 3, oy);
    free(o);
    static const int put[][3] = {{0, 2, 2}, {1, 1, 0}, {1, 2, 1}};
    int s = (int)(PIECE * c), d = (int)(DIE * c);
    uint8_t *one = malloc((size_t)(s > d ? s : d) * (s > d ? s : d) * 4);
    if (!one) return;
    for (unsigned i = 0; i < sizeof put / sizeof put[0]; i++) {
        int cx = ox - c / 3 + f + put[i][1] * c + c / 2, cy = oy + f + put[i][2] * c + c / 2;
        art_flat_shadow(s, one);
        art_blend(rgba, size, size, one, s, s, cx - s / 2 + s / 20, cy - s / 2 + s * 8 / 100);
        art_counter(COUNTER_DISC, put[i][0], s, one);
        art_blend(rgba, size, size, one, s, s, cx - s / 2, cy - s / 2);
    }
    for (int i = 0; i < 3; i++) {
        paint_die(0, i != 1, d, one);
        art_blend(rgba, size, size, one, d, d, size / 2 - d / 2 + (i - 1) * (int)(1.15f * d), oy + ph + c / 3);
    }
    free(one);
}

static const game ur_game = {
    .id = "ur", .order = 135, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .fill = 1,
};
REGISTER_GAME(ur_game)
