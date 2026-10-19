/*
 * Paint Roller in RetroStone Board, for one player: a floor of light wooden tiles in a walnut frame, walls of raised
 * walnut blocks, a ball of red lacquer that leaves its paint on every tile it rolls over (the rules:
 * paintroller_rules.c). The paint goes on as the ball passes, each tile with a little splash; a double tile shows a
 * double groove, half painted after its first coat; portals are brass rings round a jewel of their pair's colour (the
 * ball sinks into one and wells up from the other); arrows are brass, painted round.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Swipe on the board, tap one of the four arrows (the shell's tools), tap beside the ball (it rolls that way) or use
 * the arrow keys. A push that moves nothing: the ball shivers, its arrow dimmed. The lamp: the hint (the next push of
 * a shortest way: its arrow and the tile where the ball will stop in gold light; from a dead end it first takes back
 * the pushes that led there). Undo takes back one push, all the way to the start. The bar: the pushes, and at its
 * right end the par by a small ball. A floor wider than tall stands upright on a phone (the shell's fit): the ways
 * follow the screen. Solved: a wave of gold light runs over the paint from the ball.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/paintroller.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/swipepad.h"
#include "shell.h"

#define STEP_MS 42                              /* a tile rolled over */
#define PORT_MS 160                             /* through a portal */
#define SPLASH_MS 140
#define SHIVER_MS 220
#define WIN_MS 1300
#define FRAME 0.3f                              /* the walnut frame, of a tile */
#define HINT_CAP 400000

static const uint8_t PAINT[3] = {206, 66, 50}, BALL[3] = {170, 34, 26}, WALNUT[3] = {104, 68, 44},
                     BRASS[3] = {214, 170, 84}, DARK[3] = {50, 33, 22};
static const uint8_t JEWEL[3][3] = {{52, 156, 110}, {66, 108, 196}, {232, 170, 52}};

static pnr_game G;
static int serial, hint_way = -1, down = -1;
static swp_swipe sw;
static struct {
    int on, n;
    pnr_state from;
    int path[PNR_PATH];
    Uint32 at[PNR_PATH];                        /* when the ball reaches path[i] (ms after t0) */
    Uint32 t0;
} roll;
static struct { int on, dir; Uint32 t0; } shiver;
static struct { int on; Uint32 t0; } win;

static box last_box;
static ui_grid grid;
static float ex[2], ey[2], c0[2];               /* the map's +x and +y on the screen (a tile), cell (0, 0)'s centre */
static int made_cell = -1, made_serial = -1, made_small = -1, pw, ph;
static SDL_Texture *t_board, *t_paint, *t_arrow, *t_ball, *t_small;

/* ---- the map on the screen ---------------------------------------------------------------------------------------- */
static void at_map(float fx, float fy, float *sx, float *sy)   /* a point of the map (cells; a cell's centre +0.5) */
{
    *sx = c0[0] + (fx - 0.5f) * ex[0] + (fy - 0.5f) * ey[0];
    *sy = c0[1] + (fx - 0.5f) * ex[1] + (fy - 0.5f) * ey[1];
}

static void at_cell(int c, float *sx, float *sy) { at_map(c % G.m.w + 0.5f, c / G.m.w + 0.5f, sx, sy); }

static void way_vec(int way, float *vx, float *vy)   /* a map way on the screen */
{
    float k = way == PNR_UP || way == PNR_LEFT ? -1 : 1;
    const float *e = way & 1 ? ex : ey;
    *vx = k * e[0];
    *vy = k * e[1];
}

static int map_way(float dx, float dy)          /* the map way closest to a screen direction */
{
    int best = 0;
    float bd = -1e9f;
    for (int w = 0; w < 4; w++) {
        float vx, vy;
        way_vec(w, &vx, &vy);
        float d = dx * vx + dy * vy;
        if (d > bd) { bd = d; best = w; }
    }
    return best;
}

static int screen_to_map(int sway)              /* a screen way (swipepad's) to the map's */
{
    static const float V[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    return map_way(V[sway][0], V[sway][1]);
}

/* ---- painting ---------------------------------------------------------------------------------------------------- */
static int wall_at(const pnr_map *m, int x, int y)
{
    return x < 0 || y < 0 || x >= m->w || y >= m->h || m->t[x + y * m->w] == PNR_WALL;
}

static float rsq(float u, float v, float lo, float hi, float r)   /* a rounded square's distance (cell units) */
{
    float c = (lo + hi) / 2, h = (hi - lo) / 2 - r, dx = fabsf(u - c) - h, dy = fabsf(v - c) - h;
    return hypotf(fmaxf(dx, 0), fmaxf(dy, 0)) + fminf(fmaxf(dx, dy), 0) - r;
}

/* the floor (in the map's own way round): the plank in its walnut frame f pixels, tiles of q pixels, each bevelled
 * with a groove round it, walls of walnut blocks casting a shadow down and right, the double tiles' grooves, the
 * portals; W x H = (w q + 2 f) x (h q + 2 f) */
static void paint_floor(const pnr_map *m, int q, int f, uint8_t *o)
{
    int W = m->w * q + 2 * f, H = m->h * q + 2 * f;
    art_plank(m->w * q, m->h * q, f, o);
    float aa = 1.0f / q;
    for (int y = f; y < H - f; y++)
        for (int x = f; x < W - f; x++) {
            int cx = (x - f) / q, cy = (y - f) / q, c = cx + cy * m->w, t = m->t[c];
            float u = ((x - f) % q + 0.5f) / q, v = ((y - f) % q + 0.5f) / q;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            if (t == PNR_WALL) {                /* a raised block of walnut: lit at the top left */
                float d = -rsq(u, v, 0.03f, 0.97f, 0.12f);
                pzt_tint(p, WALNUT, 1);
                float k = d < 0.1f ? 1 + (u + v < 1 ? 0.22f : -0.3f) * (1 - d / 0.1f) : 1;
                if (d < 0) k = 0.45f;
                for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
                continue;
            }
            float d = fminf(fminf(u, 1 - u), fminf(v, 1 - v)), k = 1;   /* the tile's groove and bevel */
            if (d < 0.035f) k = 0.6f + 0.2f * d / 0.035f;
            else if (d < 0.09f) k = 1 + (fminf(u, v) == d ? 0.12f : -0.12f) * (1 - (d - 0.035f) / 0.055f);
            if (wall_at(m, cx - 1, cy) && u < 0.16f) k *= 0.7f + 0.3f * u / 0.16f;   /* the walls' shadows */
            if (wall_at(m, cx, cy - 1) && v < 0.16f) k *= 0.7f + 0.3f * v / 0.16f;
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
            if (t == PNR_DOUBLE) {              /* a double groove inside */
                for (int g = 0; g < 2; g++) {
                    float e = fabsf(rsq(u, v, 0.15f + 0.1f * g, 0.85f - 0.1f * g, 0.06f)) - 0.018f;
                    pzk_over(p, DARK, 0.55f * clampf(0.5f - e / aa, 0, 1));
                }
            } else if (t == PNR_PORTAL) {       /* a brass ring round a jewel, its depth dark */
                float r = hypotf(u - 0.5f, v - 0.5f);
                pzk_over(p, DARK, 0.6f * clampf((0.42f - r) / aa, 0, 1));
                uint8_t br[3];
                float lit = 1 + 0.25f * clampf((0.5f - u + 0.5f - v) * 3, -1, 1);
                for (int i = 0; i < 3; i++) br[i] = (uint8_t)clampf(BRASS[i] * lit, 0, 255);
                pzk_over(p, br, clampf(0.5f - (fabsf(r - 0.31f) - 0.06f) / aa, 0, 1));
                uint8_t j[3];
                float jl = 0.6f + 0.6f * clampf(1 - hypotf(u - 0.45f, v - 0.45f) / 0.22f, 0, 1);
                for (int i = 0; i < 3; i++) j[i] = (uint8_t)clampf(JEWEL[m->k[c]][i] * jl, 0, 255);
                pzk_over(p, j, clampf(0.5f - (r - 0.19f) / aa, 0, 1));
            }
        }
}

/* the paint on a tile: q x q, a glossy coat with soft corners */
static void paint_coat(int q, uint8_t *o)
{
    memset(o, 0, (size_t)q * q * 4);
    for (int y = 0; y < q; y++)
        for (int x = 0; x < q; x++) {
            float u = (x + 0.5f) / q, v = (y + 0.5f) / q, d = rsq(u, v, 0.02f, 0.98f, 0.14f);
            float k = 1 + 0.05f * art_noise(x, y) + 0.1f * clampf(1 - u - v, -1, 1);
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(PAINT[i] * k, 0, 255);
            pzk_over(o + 4 * ((size_t)y * q + x), c, clampf(0.5f - d * q, 0, 1));
        }
}

/* a brass arrow pointing right, q x q, outlined dark */
static void paint_arrow(int q, uint8_t *o)
{
    memset(o, 0, (size_t)q * q * 4);
    art_fig *f = art_fig_new();
    art_band(f, ART_PART, 0.2f, 0.5f, 0.6f, 0.5f, 0.15f);
    art_head(f, ART_PART, 0.82f, 0.5f, 0, 0.36f);
    art_fig_paint(o, q, f, BRASS, DARK, 0.03f, 0.4f);
}

static void paint_ball(int size, uint8_t *o)    /* the ball: red lacquer, lit at the top left, a gloss */
{
    static const uint8_t white[3] = {255, 250, 240};
    memset(o, 0, (size_t)size * size * 4);
    float r = 0.46f * size, c = size / 2.0f;
    pzk_ball(o, size, size, c, c, r, BALL, 1, 1.0f);
    pzk_ball(o, size, size, c - 0.36f * r, c - 0.38f * r, 0.22f * r, white, 0.55f, 0);
}

/* ---- the layout --------------------------------------------------------------------------------------------------- */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    paint_ball(size, rgba);
}

static void layout(box b)
{
    last_box = b;
    grid = ui_grid_fit(b, G.m.w, G.m.h, FRAME);
    float x, y, x1, y1;
    ui_grid_cell(&grid, 0, 0, &x, &y);
    c0[0] = x + grid.cell / 2;
    c0[1] = y + grid.cell / 2;
    if (G.m.w > 1) { ui_grid_cell(&grid, 1, 0, &x1, &y1); ex[0] = x1 - x; ex[1] = y1 - y; }
    if (G.m.h > 1) { ui_grid_cell(&grid, 0, 1, &x1, &y1); ey[0] = x1 - x; ey[1] = y1 - y; }
    if (G.m.w <= 1) { ex[0] = -ey[1]; ex[1] = ey[0]; }
    if (G.m.h <= 1) { ey[0] = -ex[1]; ey[1] = ex[0]; }
    int q = (int)grid.cell;
    if (made_cell != q || made_serial != serial) {
        made_cell = q;
        made_serial = serial;
        int f = (int)(FRAME * q);
        pw = G.m.w * q + 2 * f;
        ph = G.m.h * q + 2 * f;
        uint8_t *o = malloc((size_t)pw * ph * 4);
        ui_free(&t_board);
        if (o) {
            paint_floor(&G.m, q, f, o);
            t_board = ui_tex(pw, ph, o);
        }
        free(o);
        ui_free(&t_paint);
        paint_coat(q, ui_buf(q, q));
        t_paint = ui_tex(q, q, ui_buf(q, q));
        ui_free(&t_arrow);
        paint_arrow(q, ui_buf(q, q));
        t_arrow = ui_tex(q, q, ui_buf(q, q));
        int bs = (int)(0.8f * q);
        ui_free(&t_ball);
        paint_ball(bs, ui_buf(bs, bs));
        t_ball = ui_tex(bs, bs, ui_buf(bs, bs));
    }
    ui_grid_used(&grid, FRAME);
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));   /* the par's flag, as in the other level puzzles */
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    roll.on = shiver.on = win.on = 0;
    hint_way = down = -1;
    sw.on = 0;
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!pnr_level(&G, level)) pnr_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    pnr_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int solved(void) { return pnr_solved(&G.m, &G.s); }
static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(G.moves, par()); }

static Uint32 roll_len(void) { return roll.n ? roll.at[roll.n - 1] : 0; }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (roll.on && now - roll.t0 < roll_len() + SPLASH_MS) || (win.on && now - win.t0 < WIN_MS);
}

static void push(int way)
{
    if (solved() || way < 0) return;
    pnr_state before = G.s;
    hint_way = -1;
    int n;
    if (!pnr_try(&G.m, &G.s, way, NULL, roll.path, &n) || !pnr_move(&G, way)) {
        shiver.on = 1;
        shiver.dir = way;
        shiver.t0 = ui_ms();
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    roll.on = 1;
    roll.n = n;
    roll.from = before;
    roll.t0 = ui_ms();
    Uint32 t = 0;
    for (int i = 0; i < n; i++) {               /* the ball speeds up a little along a long roll */
        if (i) {
            int a = roll.path[i - 1], b = roll.path[i];
            int adj = abs(a % G.m.w - b % G.m.w) + abs(a / G.m.w - b / G.m.w) == 1;
            t += adj ? (Uint32)(STEP_MS * (i < 3 ? 1.5f - 0.2f * i : 0.9f)) : PORT_MS;
        }
        roll.at[i] = t;
    }
    ui_sound(SND_MOVE);
    ui_sound_later(SND_PLACE, (int)roll_len());
    if (solved()) {
        win.on = 1;
        win.t0 = roll.t0 + roll_len() + SPLASH_MS;
        ui_sound_later(SND_END, (int)(roll_len() + SPLASH_MS));
    }
    ui_animate();
}

static void key(SDL_Keycode k)
{
    int w = swp_key_way(k);
    if (w >= 0) push(screen_to_map(w));
}

static void hint(void)
{
    if (solved()) { ui_sound(SND_DENY); return; }
    int d, n = pnr_solve(&G.m, &G.s, HINT_CAP, &d, NULL);
    while (n < 0 && pnr_undo(&G)) n = pnr_solve(&G.m, &G.s, HINT_CAP, &d, NULL);
    roll.on = 0;
    if (n <= 0) { ui_sound(SND_DENY); return; }
    hint_way = d;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    calm();
    return pnr_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = pnr_script(&G, moves);
    if (solved()) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- drawing ------------------------------------------------------------------------------------------------------ */
static void draw(void)
{
    if (made_cell < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    float q = grid.cell;
    {                                           /* the floor's picture laid on the map's corners (a turned grid: its
                                                 * columns down the screen, as the shell's fit turns it) */
        float fr = (int)(FRAME * q) / q, u[5] = {0, 1, 1, 0, 0.5f}, v[5] = {0, 0, 1, 1, 0.5f};
        SDL_Vertex vx[5];                       /* a fan round the centre (a plain quad may be taken for an
                                                 * unturned rectangle by the renderer) */
        for (int k = 0; k < 5; k++) {
            float sx, sy;
            at_map(-fr + u[k] * (G.m.w + 2 * fr), -fr + v[k] * (G.m.h + 2 * fr), &sx, &sy);
            vx[k] = (SDL_Vertex){{sx, sy}, {255, 255, 255, 255}, {u[k], v[k]}};
        }
        int idx[12] = {0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4};
        SDL_RenderGeometry(ui.r, t_board, vx, 5, idx, 12);
    }
    if (roll.on && now - roll.t0 >= roll_len() + SPLASH_MS) roll.on = 0;
    float t = roll.on ? (float)(int)(now - roll.t0) : 1e9f;
    /* the paint: each tile's coats as they were, and those the ball has given it by now */
    int wh = G.m.w * G.m.h;
    for (int c = 0; c < wh; c++) {
        int need = pnr_need(&G.m, c);
        if (!need) continue;
        int coats = roll.on ? pnr_coats(&G.m, &roll.from, c) : pnr_coats(&G.m, &G.s, c);
        float pop = 1;
        if (roll.on)
            for (int i = 1; i < roll.n; i++)
                if (roll.path[i] == c && G.m.t[c] != PNR_PORTAL && t >= roll.at[i] && coats < need) {
                    coats++;
                    pop = clampf((t - roll.at[i]) / SPLASH_MS, 0, 1);
                }
        if (!coats) continue;
        float sx, sy, s = q * (0.8f + 0.2f * puz_ease(pop)) * (pop < 1 ? 1 + 0.08f * sinf(PUZ_PI * pop) : 1);
        at_cell(c, &sx, &sy);
        SDL_SetTextureAlphaMod(t_paint, (Uint8)(coats < need ? 120 : 255));
        ui_put_c(t_paint, sx, sy, s, s);
    }
    SDL_SetTextureAlphaMod(t_paint, 255);
    for (int c = 0; c < wh; c++)               /* the arrows over the paint */
        if (G.m.t[c] == PNR_ARROW) {
            float sx, sy, vx, vy;
            at_cell(c, &sx, &sy);
            way_vec(G.m.k[c], &vx, &vy);
            ui_put_rot(t_arrow, sx, sy, q, q, atan2f(vy, vx) * 180 / PUZ_PI);
        }
    if (hint_way >= 0 && !roll.on) {            /* the hint: where the ball will stop */
        pnr_state n;
        if (pnr_try(&G.m, &G.s, hint_way, &n, NULL, NULL)) {
            float hx, hy;
            at_cell(n.pos, &hx, &hy);
            ui_mark_gold(hx, hy, 1.1f * q);
        }
    }
    /* the ball: along its path (into a portal it sinks, out of the other it wells up), or at rest */
    float bx, by, bs = 0.8f * q, scale = 1;
    if (roll.on && t < roll_len()) {
        int i = 0;
        while (i + 1 < roll.n && roll.at[i + 1] <= t) i++;
        int a = roll.path[i], b = roll.path[i + 1 < roll.n ? i + 1 : i];
        float f = roll.at[i + 1] > roll.at[i] ? (t - roll.at[i]) / (float)(roll.at[i + 1] - roll.at[i]) : 1, ax, ay;
        at_cell(a, &ax, &ay);
        at_cell(b, &bx, &by);
        int adj = abs(a % G.m.w - b % G.m.w) + abs(a / G.m.w - b / G.m.w) <= 1;
        if (adj) { bx = ax + (bx - ax) * f; by = ay + (by - ay) * f; }
        else {                                  /* through the portal */
            if (f < 0.5f) { bx = ax; by = ay; scale = 1 - 2 * f; }
            else scale = 2 * f - 1;
        }
        ui_animate();
    } else {
        at_cell(G.s.pos, &bx, &by);
        if (shiver.on) {
            float k = (now - shiver.t0) / (float)SHIVER_MS, vx, vy;
            if (k >= 1) shiver.on = 0;
            else {
                way_vec(shiver.dir, &vx, &vy);
                float a = 0.07f * sinf(PUZ_PI * 3 * k) * (1 - k);
                bx += vx * a;
                by += vy * a;
                ui_animate();
            }
        }
        if (roll.on) ui_animate();
    }
    if (scale > 0.02f) {
        ui_shadow_flat(bx, by, bs * scale, bs * scale, 0.6f);
        ui_put_c(t_ball, bx, by, bs * scale, bs * scale);
    }
    if (win.on) {                               /* the gold wave over the floor, from the ball */
        float wt = (float)(int)(now - win.t0);
        if (wt >= 0 && wt < WIN_MS) {
            for (int c = 0; c < wh; c++) {
                if (!pnr_need(&G.m, c)) continue;
                float sx, sy;
                at_cell(c, &sx, &sy);
                float d = hypotf(sx - bx, sy - by) / q, a = puz_pulse(wt, 60 * d, 420);
                if (a > 0) ui_mark_gold_a(sx, sy, 1.3f * q, a);
            }
            ui_animate();
        }
    }
    /* the tools: in the screen's ways */
    unsigned can = 0;
    int lit = -1, held = down;
    for (int s = 0; s < 4; s++) {
        int w = screen_to_map(s);
        pnr_state n;
        if (!solved() && pnr_try(&G.m, &G.s, w, &n, NULL, NULL)) can |= 1u << s;
        if (!roll.on && w == hint_way) lit = s;
    }
    swp_tools_draw(can, lit, !solved(), held);
}

/* ---- the pointer -------------------------------------------------------------------------------------------------- */
static void tap_at(float x, float y)
{
    float bx, by;
    at_cell(G.s.pos, &bx, &by);
    if (hypotf(x - bx, y - by) < 0.45f * grid.cell) { ui_sound(SND_DENY); return; }
    push(map_way(x - bx, y - by));
}

static void press(float x, float y)
{
    sw.on = 0;
    down = swp_tool_hit(x, y);
    if (down >= 0) { ui_animate(); return; }
    if (!solved()) swp_begin(&sw, x, y);
}

static void motion(float x, float y)
{
    if (swp_follow(&sw, x, y, 0.6f * ui.unit) >= 0) push(map_way(x - sw.x0, y - sw.y0));
}

static void release(float x, float y)
{
    if (down >= 0) {
        int k = down;
        down = -1;
        ui_animate();
        if (swp_tool_hit(x, y) != k) return;
        if (k == SWP_HINT) hint();
        else if (solved()) ui_sound(SND_DENY);
        else push(screen_to_map(k));
        return;
    }
    int tap;
    if (swp_end(&sw, x, y, 0.3f * ui.unit, &tap) >= 0) push(map_way(x - sw.x0, y - sw.y0));
    else if (tap && !solved()) tap_at(sw.x0, sw.y0);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.moves, t_small, par(), solved() && G.moves <= par());
}

/* ---- the shell's pictures: the floor, the start painted, the ball on it ------------------------------------------ */
static void paint_map(const pnr_map *m, int size, uint8_t *rgba)
{
    int q = (int)(size * 0.94f / (fmaxf((float)m->w, (float)m->h) + 2 * FRAME)), f = (int)(FRAME * q);
    if (q < 3) return;
    int W = m->w * q + 2 * f, H = m->h * q + 2 * f;
    uint8_t *o = malloc((size_t)W * H * 4), *c = malloc((size_t)q * q * 4);
    if (o && c) {
        paint_floor(m, q, f, o);
        paint_coat(q, c);
        art_blend(o, W, H, c, q, q, f + m->start % m->w * q, f + m->start / m->w * q);
        art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
        int bs = (int)(0.8f * q);
        uint8_t *b = malloc((size_t)bs * bs * 4);
        if (b) {
            paint_ball(bs, b);
            art_blend(rgba, size, size, b, bs, bs, (size - W) / 2 + f + m->start % m->w * q + (q - bs) / 2,
                      (size - H) / 2 + f + m->start / m->w * q + (q - bs) / 2);
        }
        free(b);
    }
    free(o);
    free(c);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    pnr_game *t = malloc(sizeof *t);
    if (!t) return;
    if (variant < 0) pnr_level(t, 40);
    else pnr_random(t, variant, 3u + (uint32_t)variant);
    paint_map(&t->m, size, rgba);
    free(t);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    pnr_game *t = malloc(sizeof *t);
    if (t && pnr_level(t, level)) paint_map(&t->m, size, rgba);
    free(t);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const ICON[PNR_CHAPTERS] = {
        "S..|.#.|...", "S...|.#..|..#.|....", "S2.|.#.|.2.", "SA.|.#.|.A.", "S>.|.#.|.<.", "S2A|>#.|A..",
    };
    pnr_map m;
    if (chapter >= 0 && chapter < PNR_CHAPTERS && pnr_parse(&m, ICON[chapter])) paint_map(&m, size, rgba);
}

static const game paintroller_game = {
    .id = "paintroller", .order = 448, .variants = PNR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = PNR_LEVELS, .chapters = PNR_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = SWP_KNOBS,
};
REGISTER_GAME(paintroller_game)
