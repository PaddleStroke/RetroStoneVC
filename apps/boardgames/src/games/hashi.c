/*
 * Bridges in RetroStone Board, for one player: the house plank, faint burned dots where islands may lie, the islands
 * brass rings round a disc of wood with their numbers burned in, the bridges wooden planks laid between them (one, or
 * two side by side) (the rules: hashi_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag from an island towards another: a bridge (a second one, then none again). Or tap an island, then another in
 * line with it; or tap a bridge, or the water between two islands that see each other: the same. A bridge that would
 * cross another is refused. An island with all its bridges glows gold, one with too many red. The lamp at the bar's
 * end: a hint (a wrong bridge taken away, else one that reasoning finds, in gold light). Undo takes back one bridge
 * at a time, all the way to the start. The bar: the moves. Solved: the planks turn gold and a wave of gold light runs
 * over the islands.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/hashi.h"
#include "games/puzzles.h"
#include "games/pzbench_ui.h"
#include "shell.h"

#define FRAME 0.035f

static const uint8_t BRASS[3] = {214, 168, 82}, BRASS_DARK[3] = {112, 78, 30};

static hsh_game G;
static int sel = -1, serial;
static struct { int link; Uint32 t0; } hint = {-1, 0}, pop = {-1, 0};
static pzb_wave win;
static pzb_bar barl;
static struct { int on, from, moved; float x, y, x0, y0; } drag;

static box last_box;
static float bx, by;
static int q, fr, made_key = -1, made_small, isl_d;
static SDL_Texture *t_board, *t_small, *t_island[9], *t_plank;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* an island d across centred at (cx, cy) in a picture: a disc of the plank's wood, a brass ring round it, its number
 * burned in */
static void paint_island(uint8_t *o, int w, int h, float cx, float cy, int d, int number)
{
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    art_board_n(1, 1, d, 0, 0, b);
    float r = d / 2.0f;
    for (int y = 0; y < d; y++)                 /* the wood, round and a little lighter */
        for (int x = 0; x < d; x++) {
            uint8_t *p = b + 4 * ((size_t)y * d + x);
            float l = hypotf(x + 0.5f - r, y + 0.5f - r);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)fminf(255, p[i] * 1.08f + 6);
            p[3] = (uint8_t)(255 * clampf(r - l + 0.5f, 0, 1));
        }
    art_fig *f = art_fig_new();
    art_arc(f, ART_PART, 0.5f, 0.5f, 0.37f, 0.495f, 0, 360);
    art_fig_paint(b, d, f, BRASS, BRASS_DARK, 0.018f, 0.7f);
    if (number > 0) puz_burn(b, d, d, number, r, r + 0.02f * d, 0.44f * d);
    art_blend(o, w, h, b, d, d, (int)lroundf(cx - r), (int)lroundf(cy - r));
    free(b);
}

/* a plank across (len x th): a darker wood, its grain along it, its edges bevelled */
static void paint_plank(uint8_t *o, int len, int th)
{
    uint8_t *b = malloc((size_t)len * len * 4);
    if (!b) return;
    art_board_n(1, 1, len, 0, 0, b);
    for (int y = 0; y < th; y++)
        for (int x = 0; x < len; x++) {
            uint8_t *p = o + 4 * ((size_t)y * len + x), *s = b + 4 * ((size_t)(y + len / 3) * len + x);
            p[0] = (uint8_t)(s[0] * 0.7f), p[1] = (uint8_t)(s[1] * 0.5f), p[2] = (uint8_t)(s[2] * 0.36f), p[3] = 255;
        }
    free(b);
    puz_bevel(o, len, th, 0.25f * th, fmaxf(1, 0.22f * th), 0.45f);
}

/* the board: the plank in its frame, a faint burned dot on every point */
static void paint_board(uint8_t *o, int n, int c, int f, int dots)
{
    int w = n * c + 2 * f;
    art_board_n(n, n, c, f, 0, o);
    if (!dots) return;
    float r = fmaxf(1.0f, 0.045f * c);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            float cx = f + (x + 0.5f) * c, cy = f + (y + 0.5f) * c;
            puz_ink_rect(o, w, w, cx - r, cy - r, cx + r, cy + r, PUZ_INK, 0.22f);
        }
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int th = (int)fmaxf(2, 0.16f * size), d = (int)(0.62f * size);
    static const uint8_t WOOD[3] = {150, 96, 52}, EDGE[3] = {90, 56, 30};
    float cy = size * 0.5f, x0 = size * 0.4f;
    for (int k = 0; k < 2; k++) {               /* two planks leaving to the right */
        float y0 = cy + (k ? 0.07f : -0.07f) * size - th / 2.0f;
        puz_ink_rect(rgba, size, size, x0, y0, (float)size, y0 + th, WOOD, 1);
        puz_ink_rect(rgba, size, size, x0, y0, (float)size, y0 + 1, EDGE, 1);
        puz_ink_rect(rgba, size, size, x0, y0 + th - 1, (float)size, y0 + th, EDGE, 1);
        for (int y = (int)y0; y < (int)(y0 + th); y++)
            for (int x = (int)(size * 0.4f); x < size; x++) rgba[4 * (y * size + x) + 3] = 255;
    }
    paint_island(rgba, size, size, 0.36f * size, cy, d, 2);
}

static void layout(box b)
{
    last_box = b;
    int n = G.p.n ? G.p.n : 7;
    float S = fminf(b.w, b.h);
    fr = (int)(S * FRAME);
    q = (int)((S - 2 * fr) / n);
    int w = n * q + 2 * fr;
    bx = floorf(b.x + (b.w - w) / 2);
    by = floorf(b.y + (b.h - w) / 2);
    int key = (w * 32 + n) * 4096 + serial % 4096;
    if (made_key != key) {
        made_key = key;
        ui_free(&t_board);
        uint8_t *o = ui_buf(w, w);
        paint_board(o, n, q, fr, 1);
        t_board = ui_tex(w, w, o);
        isl_d = (int)(0.84f * q);
        for (int k = 1; k <= 8; k++) {
            ui_free(&t_island[k]);
            uint8_t *t = ui_buf(isl_d, isl_d);
            memset(t, 0, (size_t)isl_d * isl_d * 4);
            paint_island(t, isl_d, isl_d, isl_d / 2.0f, isl_d / 2.0f, isl_d, k);
            t_island[k] = ui_tex(isl_d, isl_d, t);
        }
        int th = (int)fmaxf(3, 0.25f * q), len = 8 * th;
        ui_free(&t_plank);
        uint8_t *t = ui_buf(len, th);
        paint_plank(t, len, th);
        t_plank = ui_tex(len, th, t);
    }
    pzb_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel = -1;
    hint.link = pop.link = -1;
    win.on = 0;
    memset(&drag, 0, sizeof drag);
}

static void start(int variant)
{
    hsh_new(&G, variant < 0 || variant >= HSH_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return hsh_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return pzb_wave_busy(&win); }
static int score(void) { return G.hist.moves; }   /* the end card: the moves it took */

static void point_xy(int x, int y, float *px, float *py)
{
    *px = bx + fr + (x + 0.5f) * q;
    *py = by + fr + (y + 0.5f) * q;
}

static void after(int link)
{
    if (hsh_solved(&G) && !win.on) {
        int a = G.p.la[link < 0 ? 0 : link];
        pzb_wave_start(&win, G.p.ix[a], G.p.iy[a], (float)G.p.n);
        sel = -1;
    }
    ui_animate();
}

static int undo(void)
{
    int l = hsh_undo(&G);
    if (l < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = hsh_script(&G, moves);
    if (hsh_solved(&G)) after(0);
    return ok;
}

/* a bridge more on the link (two, then none): refused when it would cross one */
static void build(int l)
{
    if (l < 0) { ui_sound(SND_DENY); return; }
    int before = G.br[l];
    if (!hsh_cycle(&G, l)) { ui_sound(SND_DENY); hint.link = -1; return; }
    pop.link = l;
    pop.t0 = ui_ms();
    ui_sound(G.br[l] > before ? SND_PLACE : SND_TAP);
    after(l);
}

static void lamp(void)
{
    int l = hsh_hint(&G);
    if (l < 0) { ui_sound(SND_DENY); return; }
    hint.link = pop.link = l;
    hint.t0 = pop.t0 = ui_ms();
    sel = -1;
    ui_sound(SND_PLACE);
    after(l);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static int island_at(float x, float y)
{
    int best = -1;
    float bd = 0.62f * q;
    for (int i = 0; i < G.p.nisl; i++) {
        float px, py;
        point_xy(G.p.ix[i], G.p.iy[i], &px, &py);
        float d = hypotf(x - px, y - py);
        if (d < bd) bd = d, best = i;
    }
    return best;
}

/* the link whose water (between its two islands, a little either side of the line) is under the finger */
static int link_at(float x, float y)
{
    int best = -1;
    float bd = 0.42f * q;
    for (int l = 0; l < G.p.nlink; l++) {
        float ax, ay, cx, cy;
        point_xy(G.p.ix[G.p.la[l]], G.p.iy[G.p.la[l]], &ax, &ay);
        point_xy(G.p.ix[G.p.lb[l]], G.p.iy[G.p.lb[l]], &cx, &cy);
        float off, along, len;
        if (ay == cy) off = fabsf(y - ay), along = x - ax, len = cx - ax;
        else off = fabsf(x - ax), along = y - ay, len = cy - ay;
        if (along < 0.4f * q || along > len - 0.4f * q) continue;
        if (off < bd) bd = off, best = l;
    }
    return best;
}

/* the island's neighbour the finger points at from it (along the main direction moved), if it went far enough */
static int aimed(int from, float x, float y)
{
    float px, py;
    point_xy(G.p.ix[from], G.p.iy[from], &px, &py);
    float dx = x - px, dy = y - py;
    int across = fabsf(dx) >= fabsf(dy);
    for (int l = 0; l < G.p.nlink; l++) {
        int other = G.p.la[l] == from ? G.p.lb[l] : G.p.lb[l] == from ? G.p.la[l] : -1;
        if (other < 0) continue;
        float ox, oy;
        point_xy(G.p.ix[other], G.p.iy[other], &ox, &oy);
        float tx = ox - px, ty = oy - py;
        if (across ? (ty != 0 || tx * dx <= 0) : (tx != 0 || ty * dy <= 0)) continue;
        float reach = across ? dx / tx : dy / ty;
        if (reach >= 0.4f || island_at(x, y) == other) return l;
    }
    return -1;
}

/* the planks of a link: count of them, its ends; ghost: faded */
static void planks(int l, int count, float alpha, int gold)
{
    float ax, ay, cx, cy;
    point_xy(G.p.ix[G.p.la[l]], G.p.iy[G.p.la[l]], &ax, &ay);
    point_xy(G.p.ix[G.p.lb[l]], G.p.iy[G.p.lb[l]], &cx, &cy);
    int across = ay == cy;
    float th = count == 2 ? 0.19f * q : 0.25f * q, gap = 0.14f * q, len = (across ? cx - ax : cy - ay) - 0.5f * q;
    float mx = (ax + cx) / 2, my = (ay + cy) / 2;
    if (gold) SDL_SetTextureColorMod(t_plank, 255, 226, 150);
    SDL_SetTextureAlphaMod(t_plank, (Uint8)(255 * alpha));
    for (int k = 0; k < count; k++) {
        float o = count == 2 ? (k ? gap : -gap) : 0;
        float px = across ? mx : mx + o, py = across ? my + o : my;
        float sw = across ? len : th, sh = across ? th : len;
        ui_fill(px - sw / 2 + 0.05f * q, py - sh / 2 + 0.06f * q, sw, sh, 20, 12, 6, (Uint8)(70 * alpha));
        if (across) ui_put_c(t_plank, px, py, len, th);
        else ui_put_rot(t_plank, px, py, len, th, 90);
    }
    SDL_SetTextureAlphaMod(t_plank, 255);
    SDL_SetTextureColorMod(t_plank, 255, 255, 255);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.p.n, solved = hsh_solved(&G), w = n * q + 2 * fr;
    ui_put(t_board, bx, by, (float)w, (float)w);
    float px, py;
    if (hint.link >= 0) {                       /* the hint's link: a gold light along it */
        float ax, ay, cx, cy;
        point_xy(G.p.ix[G.p.la[hint.link]], G.p.iy[G.p.la[hint.link]], &ax, &ay);
        point_xy(G.p.ix[G.p.lb[hint.link]], G.p.iy[G.p.lb[hint.link]], &cx, &cy);
        if (!pzb_hint_glow(hint.t0, (ax + cx) / 2, (ay + cy) / 2, 0.9f * hypotf(cx - ax, cy - ay) + 1.5f * q))
            hint.link = -1;
    }
    for (int l = 0; l < G.p.nlink; l++)
        if (G.br[l]) planks(l, G.br[l], 1, solved);
    int aim = drag.on && drag.moved ? aimed(drag.from, drag.x, drag.y) : -1;
    if (aim >= 0) {                             /* where the finger points: the bridge it would make, faded */
        int next = (G.br[aim] + 1) % 3;
        if (next && hsh_blocked(&G, aim) < 0) planks(aim, next, 0.5f, 0);
    }
    int target = aim < 0 ? -1 : G.p.la[aim] == drag.from ? G.p.lb[aim] : G.p.la[aim];
    for (int i = 0; i < G.p.nisl; i++) {
        point_xy(G.p.ix[i], G.p.iy[i], &px, &py);
        pzb_wave_at(&win, G.p.ix[i], G.p.iy[i], px, py, 2.4f * q);
        int has = hsh_has(&G, i), need = G.p.need[i];
        if (!solved && has == need) ui_mark_gold(px, py, 1.5f * q);
        if (!solved && has > need) ui_mark_glow(px, py, 1.3f * q);
        if (i == sel || i == target || (drag.on && i == drag.from)) ui_mark_select_round(px, py, 1.02f * q);
        ui_shadow_flat(px, py, (float)isl_d, (float)isl_d, 0.7f);
        ui_put_c(t_island[need < 1 ? 1 : need > 8 ? 8 : need], px, py, (float)isl_d, (float)isl_d);
    }
    if (sel >= 0)                               /* the islands it can reach: a dot on each */
        for (int l = 0; l < G.p.nlink; l++) {
            int other = G.p.la[l] == sel ? G.p.lb[l] : G.p.lb[l] == sel ? G.p.la[l] : -1;
            if (other < 0 || (!G.br[l] && hsh_blocked(&G, l) >= 0)) continue;
            point_xy(G.p.ix[other], G.p.iy[other], &px, &py);
            ui_mark_dot(px + 0.36f * q, py - 0.36f * q, 0.3f * q);
        }
    pzb_wave_tick(&win);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&drag, 0, sizeof drag);
    if (hsh_solved(&G)) return;
    int i = island_at(x, y);
    if (i >= 0) {
        drag.on = 1;
        drag.from = i;
        drag.x = drag.x0 = x;
        drag.y = drag.y0 = y;
        ui_animate();
        return;
    }
    int l = link_at(x, y);
    if (l >= 0) {
        sel = -1;
        build(l);
        return;
    }
    sel = -1;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (hypotf(x - drag.x0, y - drag.y0) > 0.3f * q) drag.moved = 1;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    ui_animate();
    int from = drag.from;
    if (drag.moved) {                           /* dragged: a bridge towards the island pointed at */
        int l = aimed(from, x, y);
        sel = -1;
        if (l >= 0) build(l);
        return;
    }
    if (sel < 0 || sel == from) {               /* a tap: the island picked (or let go) */
        sel = sel == from ? -1 : from;
        ui_sound(SND_TAP);
        return;
    }
    int l = hsh_link(&G.p, sel, from);          /* a second island: the bridge between, if they see each other */
    if (l < 0) {
        sel = from;
        ui_sound(SND_TAP);
        return;
    }
    sel = -1;
    build(l);
}

static void bar(int player, box b)
{
    (void)player;
    pzb_bar_draw(&barl, b, t_small, G.hist.moves, hsh_solved(&G) ? UI_DIM : 0);
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (!pzb_bar_lamp(&barl, x, y)) return 0;
    if (!hsh_solved(&G)) lamp();
    return 1;
}

/* ---- the menu's and the picker's cards: a puzzle of the size and level with its bridges, the level's ebony stones
 * under it ------------------------------------------------------------------------------------------------------ */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int n = variant < 0 ? 7 : hsh_size_of(variant), level = variant < 0 ? HSH_MEDIUM : hsh_level_of(variant);
    static hsh_puzzle p;
    hsh_make(n, level, 2026u + (uint32_t)(variant + 1), &p);
    int side = variant < 0 ? size : (int)(0.84f * size);
    int f = (int)(side * FRAME), c = (side - 2 * f) / n, w = n * c + 2 * f, o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    paint_board(b, n, c, f, c >= 14);   /* the dots only where they show */
    static const uint8_t WOOD[3] = {150, 96, 52}, EDGE[3] = {84, 52, 28};
    for (int l = 0; l < p.nlink; l++) {      /* its bridges: a card that reads at a glance */
        if (!p.sol[l]) continue;
        float ax = f + (p.ix[p.la[l]] + 0.5f) * c, ay = f + (p.iy[p.la[l]] + 0.5f) * c;
        float cx = f + (p.ix[p.lb[l]] + 0.5f) * c, cy = f + (p.iy[p.lb[l]] + 0.5f) * c;
        for (int k = 0; k < p.sol[l]; k++) {
            float off = p.sol[l] == 2 ? (k ? 0.13f : -0.13f) * c : 0, th = (p.sol[l] == 2 ? 0.17f : 0.22f) * c;
            float x0 = ay == cy ? ax : ax + off - th / 2, x1 = ay == cy ? cx : ax + off + th / 2;
            float y0 = ay == cy ? ay + off - th / 2 : ay, y1 = ay == cy ? ay + off + th / 2 : cy;
            puz_ink_rect(b, w, w, x0 - 1, y0 - 1, x1 + 1, y1 + 1, EDGE, 1);
            puz_ink_rect(b, w, w, x0, y0, x1, y1, WOOD, 1);
        }
    }
    for (int i = 0; i < p.nisl; i++)
        paint_island(b, w, w, f + (p.ix[i] + 0.5f) * c, f + (p.iy[i] + 0.5f) * c, (int)(0.86f * c), p.need[i]);
    art_blend(rgba, size, size, b, w, w, o, variant < 0 ? o : 0);
    free(b);
    if (variant >= 0) pzb_stones(rgba, size, w + (size - w - (int)(0.1f * size)) / 2, level);
}

static const game hashi_game = {
    .id = "hashi", .order = 447, .variants = HSH_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1, .score = score,
};
REGISTER_GAME(hashi_game)
