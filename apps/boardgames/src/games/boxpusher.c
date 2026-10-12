/*
 * Box Pusher in RetroStone Board, for one player: a storehouse of walnut blocks on the felt, its floor the house
 * plank, brass rings set in it for the places, wooden crates, and the house's ivory pawn as the keeper; beside it, the
 * hint's button (the rules: boxpusher_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The shell's level screen chooses the level (three chapters of 20; its stars: 3 in the fewest pushes). Then:
 * - swipe: the keeper steps that way (as many cells as the swipe is long), pushing a crate in front of it;
 * - tap a cell: the keeper walks there round the crates;
 * - tap a crate beside the keeper: it is pushed one cell away from it; tap another crate: dots show where it can be
 *   pushed in a straight line, and a tap on a dot sends the keeper round to push it there.
 * The hint walks to the next push of a shortest way and makes it (from a stuck position it first goes back to where
 * a way remains). Undo takes back one action (a walk, a push, a swipe), all the way to the start. The bar: the keeper's
 * steps and, at its right end, the pushes by a crate. Solved: the crates glow gold one after another.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/boxpusher.h"
#include "games/chess.h"
#include "games/puzzles.h"
#include "games/pztools.h"
#include "shell.h"

#define STEP_MS 75
#define WAVE_STEP 140
#define WAVE_LEN 600
#define HINT_MS 1200

enum { B_HINT, BTNS };

static const uint8_t WALNUT[3] = {120, 80, 52}, CRATE[3] = {214, 150, 84};

static bp_game G;
static int sel_box = -1;
static struct {                                 /* the last action, shown step by step */
    int on, n;
    uint8_t before[BP_CELLS];
    int keeper0, cells[BP_CELLS + 1], from[BP_CELLS], to[BP_CELLS];   /* the keeper's cells; each step's crate */
    Uint32 t0;
} anim;
static struct { int on; Uint32 t0; } win;
static struct { int cell; Uint32 t0; } hint = {-1, 0};
static struct { int on, moved; float x0, y0; } ptr;

static box last_box;
static pzt_column col;
static float mx, my;                            /* the map's top left */
static int cs, made_level = -1, made_cs = -1, made_small = -1;
static SDL_Texture *t_map, *t_crate, *t_crate_on, *t_keeper, *t_small, *t_crate_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void paint_crate(int q, int on, uint8_t *o)
{
    pzt_plank(o, q, q, 7);
    for (int i = 0; i < q * q; i++) pzt_tint(o + 4 * i, on ? PZT_BRASS : CRATE, on ? 1.05f : 0.95f);
    float b = fmaxf(1.5f, 0.16f * q), th = fmaxf(1, 0.07f * q);
    static const uint8_t dark[3] = {70, 44, 26};
    puz_ink_rect(o, q, q, b, b, q - b, b + th, dark, 0.5f);   /* the frame's inner edges */
    puz_ink_rect(o, q, q, b, q - b - th, q - b, q - b, dark, 0.5f);
    puz_ink_rect(o, q, q, b, b, b + th, q - b, dark, 0.5f);
    puz_ink_rect(o, q, q, q - b - th, b, q - b, q - b, dark, 0.5f);
    for (int y = 0; y < q; y++)                 /* the brace across */
        for (int x = 0; x < q; x++) {
            float d = fabsf((x + 0.5f) - (q - y - 0.5f)) / 1.4142f;
            if (d < th * 0.9f && x > b && x < q - b && y > b && y < q - b) {
                uint8_t *p = o + 4 * (y * q + x);
                for (int k = 0; k < 3; k++) p[k] = (uint8_t)(p[k] * 0.78f);
            }
        }
    puz_bevel(o, q, q, 0.08f * q, fmaxf(1.5f, 0.08f * q), 0.4f);
}

/* the storehouse: the plank floor, the brass rings of the places, the walnut blocks; outside: nothing */
static void paint_map(const bp_game *g, int q, uint8_t *o)
{
    int W = g->w * q, H = g->h * q;
    uint8_t *plank = malloc((size_t)W * H * 4), *t = malloc((size_t)q * q * 4);
    if (!plank || !t) { free(plank); free(t); memset(o, 0, (size_t)W * H * 4); return; }
    art_board_n(g->w, g->h, q, 0, 0, plank);
    memset(o, 0, (size_t)W * H * 4);
    for (int c = 0; c < g->w * g->h; c++) {
        int x0 = c % g->w * q, y0 = c / g->w * q;
        if (g->map[c] == BP_OUT) continue;
        for (int y = 0; y < q; y++) memcpy(t + 4 * y * q, plank + 4 * ((size_t)(y0 + y) * W + x0), (size_t)q * 4);
        if (g->map[c] == BP_WALL) {
            for (int i = 0; i < q * q; i++) pzt_tint(t + 4 * i, WALNUT, 1.0f);
            puz_bevel(t, q, q, 0.06f * q, fmaxf(2, 0.12f * q), 0.45f);
            for (int i = 0; i < q * q; i++) t[4 * i + 3] = 255;   /* square: the blocks meet */
        } else {
            int up = c >= g->w && g->map[c - g->w] == BP_WALL, left = c % g->w && g->map[c - 1] == BP_WALL;
            float sh = 0.22f * q;
            for (int y = 0; y < q; y++)
                for (int x = 0; x < q; x++) {
                    uint8_t *p = t + 4 * (y * q + x);
                    float k = 0.92f;
                    if (up) k -= 0.25f * clampf(1 - y / sh, 0, 1);   /* in the shadow of the block above */
                    if (left) k -= 0.2f * clampf(1 - x / sh, 0, 1);
                    if (x == 0 || y == 0) k *= 0.9f;
                    if (g->goal[c]) {           /* a brass ring set in the wood */
                        float d = hypotf(x + 0.5f - q / 2.0f, y + 0.5f - q / 2.0f) / q;
                        float a = clampf(1 - fabsf(d - 0.27f) / 0.06f, 0, 1), lit = 1 + 0.3f * ((q - x - y) / (float)q);
                        for (int i = 0; i < 3; i++)
                            p[i] = (uint8_t)clampf(p[i] * k + (PZT_BRASS[i] * lit - p[i] * k) * a, 0, 255);
                        if (d < 0.21f)
                            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * 0.8f);
                        continue;
                    }
                    for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
                }
        }
        for (int y = 0; y < q; y++) memcpy(o + 4 * ((size_t)(y0 + y) * W + x0), t + 4 * y * q, (size_t)q * 4);
    }
    free(plank);
    free(t);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int q = (int)(0.84f * size);
    uint8_t *b = calloc((size_t)q * q, 4);
    if (!b) return;
    paint_crate(q, 0, b);
    art_blend(rgba, size, size, b, q, q, (size - q) / 2, (size - q) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    pzt_column_fit(&col, b, BTNS);
    box r = col.room;
    cs = (int)fminf(r.w / G.w, r.h / G.h);
    if (cs > (int)(1.3f * ui.unit)) cs = (int)(1.3f * ui.unit);
    mx = floorf(r.x + (r.w - G.w * cs) / 2);
    my = floorf(r.y + (r.h - G.h * cs) / 2);
    if (made_level != G.level || made_cs != cs) {
        made_level = G.level;
        int W = G.w * cs, H = G.h * cs;
        ui_free(&t_map);
        paint_map(&G, cs, ui_buf(W, H));
        t_map = ui_tex(W, H, ui_buf(W, H));
    }
    if (made_cs != cs) {
        made_cs = cs;
        int q = (int)(0.9f * cs), k = (int)(1.18f * cs);
        ui_free(&t_crate);
        ui_free(&t_crate_on);
        ui_free(&t_keeper);
        paint_crate(q, 0, ui_buf(q, q));
        t_crate = ui_tex(q, q, ui_buf(q, q));
        paint_crate(q, 1, ui_buf(q, q));
        t_crate_on = ui_tex(q, q, ui_buf(q, q));
        memset(ui_buf(k, k), 0, (size_t)k * k * 4);
        art_piece(PAWN, WHITE, k, ui_buf(k, k));
        t_keeper = ui_tex(k, k, ui_buf(k, k));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        ui_free(&t_crate_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_piece(PAWN, WHITE, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        token(0, small, ui_buf(small, small));
        t_crate_small = ui_tex(small, small, ui_buf(small, small));
    }
}

static void relayout(void)
{
    if (last_box.w > 0) layout(last_box);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    anim.on = win.on = 0;
    hint.cell = sel_box = -1;
    memset(&ptr, 0, sizeof ptr);
}

static void start(int level)                    /* a level of the shell's level screen, 0..59 */
{
    bp_new(&G, level);
    calm();
    relayout();
}

static int status(void) { return bp_solved(&G) ? GS_WIN0 : GS_PLAY; }

/* the stars: three in the fewest pushes, two within a quarter more (at least two pushes more), else one */
static int level_stars(void)
{
    int par = bp_par[G.level];
    return G.pushes <= par ? 3 : G.pushes <= par + (par / 4 > 2 ? par / 4 : 2) ? 2 : 1;
}

static int score(void) { return G.pushes; }
static int turn(void) { return 0; }

static Uint32 anim_end(void) { return anim.on ? anim.t0 + (Uint32)(anim.n * STEP_MS) : 0; }
static Uint32 win_end(void) { return win.t0 + (Uint32)(G.nbox * WAVE_STEP + WAVE_LEN); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && now < anim_end()) || (win.on && now < win_end());
}

static void solved_now(void)
{
    if (!bp_solved(&G) || win.on) return;
    win.on = 1;
    win.t0 = anim_end() ? anim_end() : ui_ms();
}

/* steps played as one action, shown one by one */
static int steps(const uint8_t *dirs, int n)
{
    if (n <= 0) return 0;
    anim.on = 1;
    anim.n = 0;
    memcpy(anim.before, G.box, sizeof anim.before);
    anim.keeper0 = anim.cells[0] = G.player;
    bp_begin(&G);
    int pushed = 0;
    for (int i = 0; i < n; i++) {
        int from = -1, to = -1, a = G.player + bp_dx(dirs[i]) + bp_dy(dirs[i]) * G.w;
        if (a >= 0 && a < G.w * G.h && G.box[a]) { from = a; to = a + bp_dx(dirs[i]) + bp_dy(dirs[i]) * G.w; }
        int r = bp_step(&G, dirs[i]);
        if (!r) break;
        if (r == 2) pushed = 1;
        anim.from[anim.n] = r == 2 ? from : -1;
        anim.to[anim.n] = r == 2 ? to : -1;
        anim.cells[++anim.n] = G.player;
    }
    if (!anim.n) { anim.on = 0; return 0; }
    anim.t0 = ui_ms();
    ui_sound(pushed ? SND_MOVE : SND_TAP);
    if (pushed) ui_sound_later(SND_PLACE, anim.n * STEP_MS - 30);
    sel_box = -1;
    solved_now();
    ui_animate();
    return 1;
}

static int walk_to(int cell)
{
    uint8_t dirs[BP_CELLS];
    int n = cell == G.player ? 0 : bp_path(&G, cell, dirs, BP_CELLS);
    return n > 0 && steps(dirs, n);
}

/* the keeper round to the crate's back, then k pushes that way */
static int push_line(int box, int d, int k)
{
    int stand = box - bp_dx(d) - bp_dy(d) * G.w;
    uint8_t dirs[2 * BP_CELLS];
    int n = stand == G.player ? 0 : bp_path(&G, stand, dirs, BP_CELLS);
    if (n < 0) return 0;
    for (int i = 0; i < k; i++) dirs[n++] = (uint8_t)d;
    return steps(dirs, n);
}

/* how far a crate can be pushed straight that way (the keeper able to get behind it): cells */
static int push_reach(int box, int d)
{
    int bx = box % G.w, by = box / G.w, sx = bx - bp_dx(d), sy = by - bp_dy(d);
    if (sx < 0 || sy < 0 || sx >= G.w || sy >= G.h) return 0;
    int stand = sx + sy * G.w;
    uint8_t dirs[BP_CELLS];
    if (stand != G.player && (!bp_free(&G, stand) || bp_path(&G, stand, dirs, BP_CELLS) < 0)) return 0;
    int k = 0;
    for (int x = bx + bp_dx(d), y = by + bp_dy(d); x >= 0 && y >= 0 && x < G.w && y < G.h;
         x += bp_dx(d), y += bp_dy(d)) {
        if (!bp_free(&G, x + y * G.w)) break;
        k++;
    }
    return k;
}

static int undo(void)
{
    if (!bp_undo(&G)) return 0;
    calm();
    return 1;
}


static int script(const char *moves)
{
    calm();
    int ok = bp_script(&G, moves);
    solved_now();
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void centre(int c, float *x, float *y)
{
    *x = mx + (c % G.w + 0.5f) * cs;
    *y = my + (c / G.w + 0.5f) * cs;
}

static int cell_at(float x, float y)
{
    int cx = (int)floorf((x - mx) / cs), cy = (int)floorf((y - my) / cs);
    if (cx < 0 || cy < 0 || cx >= G.w || cy >= G.h) return -1;
    return cx + cy * G.w;
}


static void draw(void)
{
    if (made_cs < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_map, mx, my, (float)(G.w * cs), (float)(G.h * cs));
    /* the boxes and the keeper as the last action shows them, step by step */
    uint8_t box[BP_CELLS];
    memcpy(box, G.box, sizeof box);
    float kx, ky, t = 0;
    int k = 0, moving_from = -1, moving_to = -1;
    centre(G.player, &kx, &ky);
    if (anim.on && now < anim_end()) {
        float e = (now - anim.t0) / (float)STEP_MS;
        k = (int)e;
        t = puz_ease(e - k);
        memcpy(box, anim.before, sizeof box);
        for (int i = 0; i < k; i++)
            if (anim.from[i] >= 0) { box[anim.from[i]] = 0; box[anim.to[i]] = 1; }
        float ax, ay, bx2, by2;
        centre(anim.cells[k], &ax, &ay);
        centre(anim.cells[k + 1], &bx2, &by2);
        kx = ax + (bx2 - ax) * t;
        ky = ay + (by2 - ay) * t;
        if (anim.from[k] >= 0) { moving_from = anim.from[k]; moving_to = anim.to[k]; box[moving_from] = 0; }
        ui_animate();
    }
    float wt = win.on && now >= win.t0 ? (float)(now - win.t0) : -1;
    if (win.on && now < win_end()) ui_animate();
    if (sel_box >= 0) {                         /* where the picked crate can be pushed */
        float x, y;
        centre(sel_box, &x, &y);
        ui_mark_select(x - cs / 2.0f, y - cs / 2.0f, (float)cs, (float)cs);
        for (int d = 0; d < 4; d++) {
            int n = push_reach(sel_box, d);
            for (int i = 1; i <= n; i++) {
                centre(sel_box + i * (bp_dx(d) + bp_dy(d) * G.w), &x, &y);
                ui_mark_dot(x, y, 0.3f * cs);
            }
        }
    }
    int order = 0;
    float q = 0.9f * cs;
    for (int c = 0; c < G.w * G.h; c++) {
        if (!box[c] && c != moving_from) continue;
        float x, y;
        centre(c, &x, &y);
        int on = G.goal[c];
        if (c == moving_from) {
            float x2, y2;
            centre(moving_to, &x2, &y2);
            x += (x2 - x) * t;
            y += (y2 - y) * t;
            on = 0;
        }
        if (on && wt >= 0) {
            float p = puz_pulse(wt, order * WAVE_STEP, WAVE_LEN);
            if (p > 0) ui_mark_gold(x, y, 2.4f * cs * p);
        }
        if (on) ui_mark_gold(x, y, 1.15f * cs);
        if (on) order++;
        if (hint.cell == c) {
            float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
            if (p > 0) { ui_mark_gold(x, y, 2.2f * cs * p); ui_animate(); }
        }
        ui_shadow_flat(x + 0.04f * cs, y + 0.06f * cs, q, q, 0.6f);
        ui_put_c(on ? t_crate_on : t_crate, x, y, q, q);
    }
    float kd = 1.18f * cs;
    ui_shadow(kx - kd / 2, ky - kd / 2, kd);
    ui_put_c(t_keeper, kx, ky - 0.12f * cs, kd, kd);
    if (hint.cell >= 0 && now - hint.t0 >= HINT_MS) hint.cell = -1;
}

static void draw_all(void)
{
    draw();
    ui_button(col.x, col.y[B_HINT], col.d, ICON_HINT, bp_solved(&G) ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void use_hint(void)
{
    int box, d, k = bp_solve(&G, &box, &d);
    for (int i = 0; k == -1 && i < 64 && bp_undo(&G); i++) k = bp_solve(&G, &box, &d);   /* stuck: back first */
    if (k <= 0) { ui_sound(SND_DENY); return; }
    calm();
    if (push_line(box, d, 1)) {
        hint.cell = box + bp_dx(d) + bp_dy(d) * G.w;
        hint.t0 = ui_ms();
    }
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (bp_solved(&G)) return;
    if (ui_button_hit(col.x, col.y[B_HINT], col.d, x, y)) { use_hint(); return; }
    if (anim.on && ui_ms() < anim_end()) anim.on = 0;   /* the last action done at once */
    ptr.on = 1;
    ptr.x0 = x;
    ptr.y0 = y;
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    if (hypotf(x - ptr.x0, y - ptr.y0) > 0.35f * cs) ptr.moved = 1;
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    if (ptr.moved) {                            /* a swipe: steps that way */
        float dx = x - ptr.x0, dy = y - ptr.y0;
        int d = fabsf(dx) > fabsf(dy) ? (dx > 0 ? BP_RIGHT : BP_LEFT) : (dy > 0 ? BP_DOWN : BP_UP);
        int n = (int)fmaxf(1, roundf(fmaxf(fabsf(dx), fabsf(dy)) / cs));
        uint8_t dirs[BP_CELLS];
        for (int i = 0; i < n; i++) dirs[i] = (uint8_t)d;
        if (!bp_can(&G, d)) { ui_sound(SND_DENY); return; }
        bp_game probe = G;                      /* only as far as it can go */
        int k = 0;
        while (k < n && bp_can(&probe, d)) { bp_step(&probe, d); k++; }
        steps(dirs, k);
        return;
    }
    int c = cell_at(x, y);
    if (c < 0 || G.map[c] != BP_FLOOR) { sel_box = -1; return; }
    if (sel_box >= 0 && c != sel_box) {         /* a dot of the picked crate */
        for (int d = 0; d < 4; d++) {
            int n = push_reach(sel_box, d);
            for (int i = 1; i <= n; i++)
                if (sel_box + i * (bp_dx(d) + bp_dy(d) * G.w) == c) { push_line(sel_box, d, i); return; }
        }
    }
    if (G.box[c]) {
        for (int d = 0; d < 4; d++)             /* beside the keeper: pushed away from it */
            if (G.player + bp_dx(d) + bp_dy(d) * G.w == c && G.player / G.w + bp_dy(d) == c / G.w) {
                if (bp_can(&G, d)) { uint8_t dd = (uint8_t)d; steps(&dd, 1); }
                else ui_sound(SND_DENY);
                return;
            }
        sel_box = sel_box == c ? -1 : c;
        int any = 0;
        for (int d = 0; d < 4 && sel_box >= 0; d++) any |= push_reach(sel_box, d) > 0;
        if (sel_box >= 0 && !any) { sel_box = -1; ui_sound(SND_DENY); return; }
        ui_sound(SND_TAP);
        return;
    }
    sel_box = -1;
    if (!walk_to(c) && c != G.player) ui_sound(SND_DENY);
}


/* ---- the bar: the steps; the pushes at the right ----------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two(b, t_small, G.moves, t_crate_small, G.pushes, bp_solved(&G));
}

/* ---- the pictures for the shell: a level's map with its crates and keeper (the level screen's tiles, the menu's
 * card), a chapter's tab (one, two or three crates) ------------------------------------------------------------- */
static void level_pic(int level, int size, float part, uint8_t *rgba)
{
    bp_game T;
    T.level = 0;
    bp_new(&T, level);
    int area = (int)(part * size), q = area / (T.w > T.h ? T.w : T.h), W = T.w * q, H = T.h * q;
    int ox = (size - W) / 2, oy = (size - H) / 2, k = (int)(0.9f * q);
    uint8_t *m = malloc((size_t)W * H * 4), *cr = calloc((size_t)q * q, 4), *on = calloc((size_t)q * q, 4);
    uint8_t *pawn = calloc((size_t)(k > 0 ? k : 1) * (k > 0 ? k : 1), 4);
    if (m && cr && on && pawn && q > 1) {
        paint_map(&T, q, m);
        art_blend(rgba, size, size, m, W, H, ox, oy);
        paint_crate(q, 0, cr);
        paint_crate(q, 1, on);
        art_piece(PAWN, WHITE, k, pawn);
        for (int c = 0; c < T.w * T.h; c++) {
            if (T.box[c]) art_blend(rgba, size, size, T.goal[c] ? on : cr, q, q, ox + c % T.w * q, oy + c / T.w * q);
            if (c == T.player) art_blend(rgba, size, size, pawn, k, k, ox + c % T.w * q, oy + c / T.w * q);
        }
    }
    free(m);
    free(pawn);
    free(cr);
    free(on);
}

static void level_thumb(int level, int size, uint8_t *rgba) { level_pic(level, size, 0.9f, rgba); }

static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    level_pic(2, size, 0.92f, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int n = chapter + 1, d = (int)(size / (1.15f * n + 0.3f));
    if (d > (int)(0.6f * size)) d = (int)(0.6f * size);
    uint8_t *c = calloc((size_t)d * d, 4);
    if (!c) return;
    paint_crate(d, 1, c);
    for (int i = 0; i < n; i++)
        art_blend(rgba, size, size, c, d, d, (int)(size / 2.0f + (i - (n - 1) / 2.0f) * 1.15f * d - d / 2.0f),
                  (size - d) / 2);
    free(c);
}

static const game boxpusher_game = {
    .id = "boxpusher", .order = 442, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw_all,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
    .levels = BP_LEVELS, .chapters = BP_CHAPTERS, .chapter_icon = chapter_icon, .level_thumb = level_thumb,
    .level_stars = level_stars,
};
REGISTER_GAME(boxpusher_game)
