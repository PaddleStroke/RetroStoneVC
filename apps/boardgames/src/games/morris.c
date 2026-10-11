/*
 * The men's morris games in RetroStone Board: nine men's (three squares drawn on one plank), twelve men's (the same and
 * the corners' diagonals), six men's (two squares); the discs, tap or drag, the mills and their removals (the rules,
 * the boards and the notation: morris_rules.c, morris.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <string.h>

#include "games/lines.h"
#include "games/morris.h"
#include "shell.h"

#define DISC 0.84f                              /* a disc's size, in cells (six men's larger cells: 0.72) */
#define ANIM_MS 150
#define FADE_MS 240

static mgame G;
static const mboard *B;                         /* the game's board */
static mmove legal[MORRIS_MAX_MOVES];
static int nlegal;
static lines_geo L = {.cells = 7};              /* the plank: cells x cells, a point in some of them */
static int made_small = -1, made_cells = -1;
static SDL_Texture *t_disc[2], *t_small[2], *t_board;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, color; mmove m; Uint32 t0; } anim;
static const uint8_t GOLD[3] = {255, 196, 64}, BLACK[3] = {0, 0, 0};

static float disc_of(const mboard *b) { return b->cells < 7 ? 0.72f : DISC; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = morris_legal(morris_pos(&G), legal);
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    morris_new(&G, variant);
    B = morris_board(morris_pos(&G)->variant);
    L.cells = B->cells;
    anim.on = 0;
    refresh();
}

static int status(void)
{
    switch (morris_status(&G)) {
    case MS_PLAY: return GS_PLAY;
    case MS_FEW: case MS_BLOCKED: return morris_pos(&G)->side ? GS_WIN0 : GS_WIN1;
    default: return GS_DRAW;
    }
}

static int turn(void) { return morris_pos(&G)->side; }

static int find(int type, int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].type == type && legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static int can_move(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].type == M_MOVE && legal[i].from == from) return 1;
    return 0;
}

static void play(mmove m, int animate)
{
    int color = morris_pos(&G)->side ^ (m.type == M_TAKE);
    if (!morris_play(&G, m)) return;
    anim.on = animate;
    anim.color = color;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.type == M_TAKE ? SND_CAPTURE : m.type == M_PLACE ? SND_PLACE : SND_MOVE);
    if (morris_pos(&G)->take) ui_sound(SND_CHECK);   /* a mill: a chime */
}

static int undo(void)
{
    if (!morris_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "d2 f4 d1-d3 xb6": the screenshots' and the tests' moves (morris.h) */
static int script(const char *s)
{
    int ok = morris_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a board's plank with its lines (each from its first point to its last) and points: cells of s pixels in a frame of
 * f; (cells s + 2 f) squared */
static void paint_board(const mboard *b, int s, int f, uint8_t *o)
{
    int8_t seg[MORRIS_LINES][4], pt[MORRIS_POINTS][2];
    for (int i = 0; i < b->lines; i++) {
        const int8_t *l = b->line[i];
        int e = l[2] >= 0 ? l[2] : l[1];
        seg[i][0] = b->x[l[0]], seg[i][1] = b->y[l[0]], seg[i][2] = b->x[e], seg[i][3] = b->y[e];
    }
    for (int i = 0; i < b->points; i++) pt[i][0] = b->x[i], pt[i][1] = b->y[i];
    lines_paint(b->cells, s, f, (const int8_t (*)[4])seg, b->lines, (const int8_t (*)[2])pt, b->points,
                fmaxf(2.0f, 0.06f * s), fmaxf(1.6f, 0.11f * s), o);
}

static void layout(box b)
{
    if (!B) start(MORRIS_NINE);
    int changed = lines_fit(&L, b), n = (int)lines_side(&L);
    if (changed || made_cells != B->lines * 100 + B->cells) {   /* the board's own lines */
        made_cells = B->lines * 100 + B->cells;
        ui_free(&t_board);
        paint_board(B, L.sq, L.fr, ui_buf(n, n));
        t_board = ui_tex(n, n, ui_buf(n, n));
        int d = (int)(disc_of(B) * L.sq);
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
static void pt_xy(int pt, float *x, float *y)  /* a point's centre */
{
    lines_xy(&L, B->x[pt], B->y[pt], x, y);
}

static int xy_pt(float x, float y)              /* the point whose cell holds (x, y); -1 */
{
    int gx, gy;
    if (!lines_at(&L, x, y, &gx, &gy)) return -1;
    for (int i = 0; i < B->points; i++)
        if (B->x[i] == gx && B->y[i] == gy) return i;
    return -1;
}

static void disc_at(int c, float cx, float cy, float d, float alpha) { ui_flat_at(t_disc[c], cx, cy, d, alpha, 0); }

static void mark_last(int pt) { lines_mark_last(&L, B->x[pt], B->y[pt]); }

/* the mill just made: a gold band along it, a gold light under its pieces */
static void draw_mill(const mpos *p)
{
    int to = G.move[G.ply - 1].to;
    for (int i = 0; i < B->lines; i++) {
        const int8_t *m = B->line[i];
        if (m[2] < 0 || (m[0] != to && m[1] != to && m[2] != to) || p->at[m[0]] != p->side ||
            p->at[m[1]] != p->side || p->at[m[2]] != p->side)
            continue;
        float x0, y0, x1, y1, w = 0.17f * L.sq;
        pt_xy(m[0], &x0, &y0);
        pt_xy(m[2], &x1, &y1);
        if (x0 == x1 || y0 == y1)
            ui_fill(fminf(x0, x1) - w / 2, fminf(y0, y1) - w / 2, fabsf(x1 - x0) + w, fabsf(y1 - y0) + w,
                    250, 206, 80, 230);
        else                                    /* a diagonal (twelve men's): gold beads along it */
            for (int k = 0; k <= 16; k++)
                ui_mark_gold(x0 + (x1 - x0) * k / 16, y0 + (y1 - y0) * k / 16, 0.5f * L.sq);
        for (int j = 0; j < 3; j++) {
            pt_xy(m[j], &x0, &y0);
            ui_mark_gold(x0, y0, 1.7f * L.sq);
        }
    }
}

static void draw(void)
{
    const mpos *p = morris_pos(&G);
    float n = lines_side(&L), d = disc_of(B) * L.sq, x, y;
    int st = status();
    ui_put(t_board, L.x - L.fr, L.y - L.fr, n, n);
    for (int k = G.ply - 1; k >= 0 && k >= G.ply - 2; k--) {   /* the last turn: a removal, and the move before it */
        if (G.move[k].from >= 0) mark_last(G.move[k].from);
        if (G.move[k].to >= 0) mark_last(G.move[k].to);
        if (G.move[k].type != M_TAKE) break;
    }
    if (p->take && G.ply) draw_mill(p);
    if (sel >= 0) lines_mark_select(&L, B->x[sel], B->y[sel]);
    for (int i = 0; st == GS_PLAY && i < nlegal; i++) {          /* where to play, what can be removed */
        const mmove *m = &legal[i];
        if (m->type == M_TAKE) {
            pt_xy(m->from, &x, &y);
            ui_mark_glow(x, y, 1.5f * L.sq);
        } else if (m->type == M_PLACE || m->from == sel) {
            pt_xy(m->to, &x, &y);
            ui_mark_dot(x, y, 0.36f * L.sq);
        }
    }
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(anim.m.type == M_TAKE ? FADE_MS : ANIM_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    for (int i = 0; i < B->points; i++) {
        if (p->at[i] < 0 || (drag.on && drag.moved && i == drag.from) || (anim.on && anim.m.type != M_TAKE &&
                                                                          i == anim.m.to))
            continue;
        pt_xy(i, &x, &y);
        disc_at(p->at[i], x, y, d, 1);
    }
    if (anim.on) {
        float x0, y0;
        if (anim.m.type == M_MOVE) {            /* a slide along the line (or a flight) */
            pt_xy(anim.m.from, &x0, &y0);
            pt_xy(anim.m.to, &x, &y);
            disc_at(anim.color, x0 + (x - x0) * t, y0 + (y - y0) * t, d, 1);
        } else if (anim.m.type == M_PLACE) {    /* set down: from a little above the board */
            pt_xy(anim.m.to, &x, &y);
            disc_at(anim.color, x, y, d * (1.2f - 0.2f * t), 1);
        } else {                                /* removed: lifted and fading away */
            pt_xy(anim.m.from, &x, &y);
            disc_at(anim.color, x, y, d * (1 + 0.2f * t), 1 - t);
        }
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
    const mpos *p = morris_pos(&G);
    int pt = xy_pt(x, y), i;
    if (p->take || p->hand[p->side]) {          /* a removal or a placing: one tap */
        if (pt < 0) return;
        i = p->take ? find(M_TAKE, pt, -1) : find(M_PLACE, -1, pt);
        if (i < 0) ui_sound(SND_DENY);
        else play(legal[i], 1);
        return;
    }
    if (pt < 0) { sel = -1; return; }
    if (sel >= 0 && (i = find(M_MOVE, sel, pt)) >= 0) {
        play(legal[i], 1);
        return;
    }
    if (p->at[pt] == p->side && can_move(pt)) {
        sel = pt;
        drag.on = 1;
        drag.from = pt;
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
    if (hypotf(x - drag.x, y - drag.y) > L.sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the disc stays selected */
    int pt = xy_pt(x, y), i = pt >= 0 ? find(M_MOVE, drag.from, pt) : -1;
    if (i >= 0) play(legal[i], 0);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the player's pieces still to place (a stack), then the opponent's pieces it removed (as chess shows its captures) */
static void bar(int player, box b)
{
    const mpos *p = morris_pos(&G);
    float sz = 0.6f * ui.unit, x = b.x, cy = b.y + b.h / 2, end = b.x + b.w;
    int hand = p->hand[player], took = B->pieces - p->hand[!player] - p->on[!player];
    float step = hand > 9 ? 0.15f : 0.19f;      /* twelve in hand: a tighter stack */
    for (int i = 0; i < hand && x + sz <= end; i++, x += step * ui.unit)
        ui_put(t_small[player], x, cy - sz / 2, sz, sz);
    if (hand) x += sz + 0.16f * ui.unit;
    for (int i = 0; i < took && x + sz <= end; i++, x += 0.4f * ui.unit)
        ui_put(t_small[!player], x, cy - sz / 2, sz, sz);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the cards: the board, a few discs (ivory then ebony), a mill in gold: the menu's and nine men's the top row;
 * twelve men's along a corner's diagonal, on a crowded board; six men's the top row of its two squares */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const struct { int mill; const char *pieces[2]; } CARD[MORRIS_VARIANTS] = {   /* ivory's, ebony's */
        [MORRIS_NINE] = {7, {"a7 d7 g7 d2 b4", "e3 f4 d6 c3 e5 b2"}},
        [MORRIS_TWELVE] = {16, {"a1 b2 c3 g4 e5 d7 f2 d3", "d1 f6 a4 g1 b6 c5 f4 e3 g7"}},
        [MORRIS_SIX] = {3, {"a5 c5 e5 c2", "b3 d4 c1 e3 b4"}},
    };
    int v = variant < 0 ? MORRIS_NINE : variant;
    const mboard *b = morris_board(v);
    int c = b->cells, s = (int)(size / (c + 2 * LINES_FRAME)), f = (int)(s * LINES_FRAME), n = c * s + 2 * f;
    int o = (size - n) / 2, d = (int)(disc_of(b) * s), sh = (int)(1.25f * d);
    uint8_t *board = ui_buf(n, n);
    paint_board(b, s, f, board);
    const int8_t *m = b->line[CARD[v].mill];
    lines_band(board, n, lines_px(b->x[m[0]], (float)s, (float)f), lines_py(c, b->y[m[0]], (float)s, (float)f),
               lines_px(b->x[m[2]], (float)s, (float)f), lines_py(c, b->y[m[2]], (float)s, (float)f),
               0.17f * s, GOLD, 0.9f);
    art_blend(rgba, size, size, board, n, n, o, o);
    for (int k = 0; k < 2 * MORRIS_POINTS; k++) {   /* "a7 d7 ...": ivory's points, then ebony's */
        int who = k >= MORRIS_POINTS;
        const char *t = CARD[v].pieces[who] + 3 * (k % MORRIS_POINTS);
        if ((int)strlen(CARD[v].pieces[who]) < 3 * (k % MORRIS_POINTS) + 2) continue;
        int pt = morris_point(v, t);
        float cx = o + f + (b->x[pt] + 0.5f) * s, cy = o + f + (c - 1 - b->y[pt] + 0.5f) * s;
        uint8_t *one = ui_buf(sh, sh);
        art_disc(sh, BLACK, 0.5f, 1.6f, one);
        art_blend(rgba, size, size, one, sh, sh, (int)(cx + 0.05f * d - sh / 2.0f), (int)(cy + 0.09f * d - sh / 2.0f));
        art_counter(COUNTER_DISC, who, d, one);
        art_blend(rgba, size, size, one, d, d, (int)(cx - d / 2.0f + 0.5f), (int)(cy - d / 2.0f + 0.5f));
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last move to finish */

static const game morris_game = {
    .id = "morris", .order = 50, .variants = MORRIS_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(morris_game)
