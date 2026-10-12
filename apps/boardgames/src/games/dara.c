/*
 * Dara in RetroStone Board: a plank with 5 rows of 6 carved holes (the game is played in holes scooped in the sand),
 * the house's discs. Placing: tap a hole with a dot (the others would make a row of three). Moving: tap or drag a
 * disc to a dot; a dot in a gold light makes a row of three. The row then glows gold and the enemy discs glow red:
 * tap the one to take. The bars: the discs still in hand while placing, then the discs taken. The rules: dara_rules.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The script's notation: "c3" places (or takes, after a row of three; "xc3" too), "c3c4" moves; columns a..f from
 * player 0's left, rows 1..5 from their side.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dara.h"
#include "games/lines.h"
#include "shell.h"

#define DROP_MS 130
#define MOVE_MS 170
#define TAKE_MS 300
#define HOLE 0.36f                              /* a hole's radius, in cells */
#define DISC 0.84f                              /* a disc, in cells */

static dargame G;
static darmove legal[DAR_MAX_MOVES];
static int nlegal, sel = -1;
static lines_geo L = {.cells = DAR_W, .rows = DAR_H};
static int made = -1, made_small = -1;
static SDL_Texture *t_disc[2], *t_small[2], *t_board;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, who; darmove m; Uint32 t0; } anim;   /* who: the mover (a capture: the piece's owner) */

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = dar_status(&G) == DAR_PLAY ? dar_legal(dar_pos(&G), legal) : 0;
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    dar_new(&G);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    static const int map[] = {[DAR_PLAY] = GS_PLAY, [DAR_WIN0] = GS_WIN0, [DAR_WIN1] = GS_WIN1, [DAR_DRAW] = GS_DRAW};
    return map[dar_status(&G)];
}

static int turn(void) { return dar_pos(&G)->side; }

static int find(int kind, int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].kind == kind && legal[i].to == to && (kind != DAR_MOVE || legal[i].from == from)) return i;
    return -1;
}

static void play(darmove m, int animate)
{
    const darpos *p = dar_pos(&G);
    int who = m.kind == DAR_TAKE ? p->c[m.to] - 1 : p->side;
    if (!dar_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.who = who;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.kind == DAR_TAKE ? SND_CAPTURE : m.kind == DAR_PLACE ? SND_PLACE : SND_MOVE);
    if (dar_pos(&G)->phase == DAR_TAKE) ui_sound_later(SND_CHECK, animate ? MOVE_MS : 0);   /* a row of three */
}

static int undo(void)
{
    if (!dar_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

static int script(const char *s)
{
    int ok = dar_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank (cols x rows cells of s pixels, frame f) and a carved hole in each cell */
static void paint_board(uint8_t *o, int cols, int rows, int s, int f)
{
    int w = cols * s + 2 * f, h = rows * s + 2 * f;
    float r = HOLE * s, b = 0.07f * s;
    art_board_n(cols, rows, s, f, 0, o);
    for (int cy = 0; cy < rows; cy++)
        for (int cx = 0; cx < cols; cx++) {
            float hx = f + (cx + 0.5f) * s, hy = f + (cy + 0.5f) * s;
            for (int y = (int)(hy - r - 2); y <= (int)(hy + r + 2); y++)
                for (int x = (int)(hx - r - 2); x <= (int)(hx + r + 2); x++) {
                    if (x < 0 || y < 0 || x >= w || y >= h) continue;
                    float k = art_recess(x + 0.5f - hx, y + 0.5f - hy, r, b, 0.4f, 0.24f, 0.22f);
                    uint8_t *p = o + 4 * (y * w + x);
                    for (int j = 0; j < 3; j++) p[j] = (uint8_t)clampf(p[j] * k + 0.5f, 0, 255);
                }
        }
}

static SDL_Texture *disc_tex(int player, int size)
{
    art_counter(COUNTER_DISC, player, size, ui_buf(size, size));
    return ui_tex(size, size, ui_buf(size, size));
}

static void layout(box b)
{
    if (lines_fit(&L, b) || made != L.sq) {
        made = L.sq;
        int d = (int)(DISC * L.sq), w = (int)lines_side(&L), h = (int)lines_tall(&L);
        for (int c = 0; c < 2; c++) {
            ui_free(&t_disc[c]);
            t_disc[c] = disc_tex(c, d);
        }
        ui_free(&t_board);
        paint_board(ui_buf(w, h), DAR_W, DAR_H, L.sq, L.fr);
        t_board = ui_tex(w, h, ui_buf(w, h));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_small[c]);
            t_small[c] = disc_tex(c, small);
        }
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void hole_xy(int h, float *x, float *y) { lines_xy(&L, h % DAR_W, h / DAR_W, x, y); }

static int xy_hole(float x, float y)
{
    int gx, gy;
    return lines_at(&L, x, y, &gx, &gy) ? DAR_AT(gx, gy) : -1;
}

/* a disc of the player at (x, y): k its size (1: in its hole; lifted: bigger), alpha */
static void disc_at(int player, float x, float y, float k, float alpha)
{
    float d = DISC * L.sq * k, lift = (k - 1) * L.sq;
    ui_shadow_flat(x + 0.5f * lift, y + lift, d, d, alpha);
    SDL_SetTextureAlphaMod(t_disc[player], (Uint8)(255 * clampf(alpha, 0, 1)));
    ui_put_c(t_disc[player], x, y, d, d);
    SDL_SetTextureAlphaMod(t_disc[player], 255);
}

/* 1: moving from -> to makes a row of three */
static int makes_row(int from, int to)
{
    darpos q = *dar_pos(&G);
    darmove m = {DAR_MOVE, (uint8_t)from, (uint8_t)to};
    dar_make(&q, m);
    return q.phase == DAR_TAKE;
}

static void draw(void)
{
    const darpos *p = dar_pos(&G);
    float x, y, s = (float)L.sq;
    ui_put(t_board, L.x - L.fr, L.y - L.fr, lines_side(&L), lines_tall(&L));
    if (G.ply) {
        darmove m = G.move[G.ply - 1];
        if (m.kind == DAR_MOVE) lines_mark_last(&L, m.from % DAR_W, m.from / DAR_W);
        lines_mark_last(&L, m.to % DAR_W, m.to / DAR_W);
    }
    if (sel >= 0) lines_mark_select(&L, sel % DAR_W, sel / DAR_W);
    float t = 1;
    int ms = anim.m.kind == DAR_TAKE ? TAKE_MS : anim.m.kind == DAR_PLACE ? DROP_MS : MOVE_MS;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)ms;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    if (p->phase == DAR_TAKE && !(anim.on && anim.m.kind == DAR_MOVE))   /* the row of three; the discs to take */
        for (int h = 0; h < DAR_N; h++) {
            hole_xy(h, &x, &y);
            if (p->line >> h & 1) ui_mark_gold(x, y, 2.4f * s);
            else if (p->c[h] && p->c[h] != p->side + 1 && status() == GS_PLAY) ui_mark_glow(x, y, 1.45f * s);
        }
    for (int h = 0; h < DAR_N; h++) {
        if (!p->c[h] || (drag.on && drag.moved && h == drag.from) || (anim.on && h == anim.m.to)) continue;
        hole_xy(h, &x, &y);
        disc_at(p->c[h] - 1, x, y, 1, 1);
    }
    if (anim.on) {
        darmove m = anim.m;
        float e = 1 - (1 - t) * (1 - t) * (1 - t);
        hole_xy(m.to, &x, &y);
        if (m.kind == DAR_PLACE) disc_at(anim.who, x, y, 1.16f - 0.16f * e, 1);   /* set down from above */
        else if (m.kind == DAR_TAKE) disc_at(anim.who, x, y - 0.3f * s * e, 1 + 0.15f * e, 1 - e);   /* lifted off */
        else {
            float x0, y0;
            hole_xy(m.from, &x0, &y0);
            disc_at(anim.who, x0 + (x - x0) * e, y0 + (y - y0) * e, 1 + 0.08f * sinf(t * 3.14159265f), 1);
        }
    }
    if (anim.on || status() != GS_PLAY) return;
    if (p->phase == DAR_PLACE)                  /* where a disc may go */
        for (int i = 0; i < nlegal; i++) {
            hole_xy(legal[i].to, &x, &y);
            ui_mark_dot(x, y, 0.26f * s);
        }
    else if (sel >= 0)
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            hole_xy(legal[i].to, &x, &y);
            if (makes_row(sel, legal[i].to)) ui_mark_gold_a(x, y, 1.6f * s, 0.8f);
            ui_mark_dot(x, y, 0.3f * s);
        }
    if (drag.on && drag.moved) {                /* the dragged disc, a little bigger, above the finger */
        disc_at(p->side, drag.x, drag.y - 0.3f * s, 1.18f, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY || anim.on) return;
    const darpos *p = dar_pos(&G);
    int h = xy_hole(x, y), i;
    if (h < 0) { sel = -1; return; }
    if (p->phase != DAR_MOVE) {
        if ((i = find(p->phase, h, h)) >= 0) play(legal[i], 1);
        else ui_sound(SND_DENY);
        return;
    }
    if (sel >= 0 && (i = find(DAR_MOVE, sel, h)) >= 0) {
        play(legal[i], 1);
        return;
    }
    if (p->c[h] == p->side + 1) {
        int any = 0;
        for (i = 0; i < nlegal && !any; i++) any = legal[i].from == h;
        if (!any) { ui_sound(SND_DENY); sel = -1; return; }
        sel = h;
        drag.on = 1;
        drag.from = h;
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
    int h = xy_hole(x, y), i = h >= 0 && h != drag.from ? find(DAR_MOVE, drag.from, h) : -1;
    if (i >= 0) play(legal[i], 0);
    else if (h != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* while placing, the discs in hand; then the discs taken from the other player; the moves left before the draw once
 * they are few */
static void bar(int player, box b)
{
    const darpos *p = dar_pos(&G);
    float u = ui.unit, end = b.x + b.w;
    int left = DAR_QUIET - p->quiet;
    if (p->phase != DAR_PLACE && left <= 20 && status() == GS_PLAY && p->side == player) {
        box c = ui_bar_countdown(b, left);
        end = c.x + c.w;
    }
    box r = {b.x, b.y, end - b.x, b.h};
    if (p->phase == DAR_PLACE && p->hand[player]) {
        r.x = ui_bar_number(r, t_small[player], p->hand[player]) + 0.35f * u;
        r.w = end - r.x;
    }
    int taken = 0;
    for (int i = 0; i < G.ply; i++) taken += G.move[i].kind == DAR_TAKE && G.pos[i].side == player;
    if (r.w > 0) ui_bar_count(r, t_small[!player], taken);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card: a corner of the board, a row of three in its gold light, the discs about */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int n = 4, s = (int)(size / (n + 2 * LINES_FRAME)), f = (int)(s * LINES_FRAME), w = n * s + 2 * f,
        o = (size - w) / 2;
    uint8_t *pic = ui_buf(w, w);
    paint_board(pic, n, n, s, f);
    art_blend(rgba, size, size, pic, w, w, o, o);
    static const uint8_t gold[3] = {255, 214, 120};
    static const int place[][3] = {{0, 1, 0}, {1, 1, 0}, {2, 1, 0}, {3, 0, 1}, {1, 2, 1}, {2, 3, 1}, {0, 3, 0},
                                   {3, 2, 1}};
    int g = (int)(1.9f * s), d = (int)(DISC * s);
    uint8_t *one = malloc((size_t)g * g * 4);
    if (!one) return;
    art_disc(g, gold, 1, 1, one);
    for (int i = 0; i < 3; i++)
        art_blend(rgba, size, size, one, g, g, o + f + (int)((place[i][0] + 0.5f) * s) - g / 2,
                  o + f + (int)((place[i][1] + 0.5f) * s) - g / 2);
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        art_counter(COUNTER_DISC, place[i][2], d, one);
        art_flat_put(rgba, size, size, one, d, o + f + (place[i][0] + 0.5f) * s, o + f + (place[i][1] + 0.5f) * s);
    }
    free(one);
}

static int busy(void) { return anim.on; }

static const game dara_game = {
    .id = "dara", .order = 261, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(dara_game)
