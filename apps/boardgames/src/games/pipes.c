/*
 * Pipes in RetroStone Board, for one player: square tiles cut in the house plank, a brass pipe on each (open on top,
 * a channel down its middle), a brass tank in the middle of the board that holds the water (the rules: pipes_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The shell's level screen chooses a level (150 in six chapters, pipes_levels.h), or its die tab a puzzle made at
 * random (the variants). Stars: the moves against par (the fewest quarter turns), a hint caps them (pzkit.h).
 * Tap a tile: its pipe turns a quarter turn clockwise. The water runs from the tank along every pipe joined to it, tile
 * by tile (a cool blue filling the channels, the round basins at the pipes' ends); a pipe turned away cuts the flow and
 * what lay beyond drains. A pipe that ends open where the water reaches it spills (drops at its mouth). The wrapped
 * board has brass portholes all round its frame: a pipe leaving one edge comes in at the other. The board takes the
 * room the shell gives (game.fill); the shell's tool: the hint (a wrong tile turned right, next to the water when it
 * can be, in gold light). Undo takes back one turn or hint, all the way to the start. The bar: the moves.
 * Solved: the water fills every tile, then a gold light runs out from the tank along the pipes.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/pipes.h"
#include "games/pipes_levels.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "shell.h"

#define FRAME 0.035f
#define RO 0.19f                                /* the pipe's radius, a part of a tile */
#define RI 0.085f                               /* its channel's */
#define RB 0.27f                                /* an end's basin */
#define RBI 0.18f
#define TANK 0.38f                              /* the source's tank */
#define TANK_IN 0.29f
#define SPIN_MS 150
#define STEP_MS 110                             /* the water through a tile */
#define DRAIN_MS 260
#define HINT_MS 1300
#define WAVE_STEP 60
#define WAVE_LEN 520

enum { K_BASIN = 1, K_WET = 2, K_TANK = 4 };

static pp_game G;
static box last_box;
static float bx, by, side;
static int fr, cs, ox, oy, made_key = -1, serial;
static SDL_Texture *t_board, *t_pipe[4], *t_strip, *t_hub, *t_basin, *t_tank;

/* the water and the turns, on the screen */
static uint8_t wet[PP_TILES], wet_mask[PP_TILES];
static int wet_from[PP_TILES];
static Uint32 wet_t0[PP_TILES], dry_t0[PP_TILES], spin_t0[PP_TILES];
static float spin_off[PP_TILES];
static int dist_now[PP_TILES], from_now[PP_TILES], max_dist;
static struct { int on; Uint32 t0; } win;
static struct { int tile; Uint32 t0; } hint = {-1, 0};
static struct { int on, tile; } ptr;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void mix_over(uint8_t *p, float r, float g, float b, float a)
{
    if (a <= 0) return;
    float da = p[3] / 255.0f, oa = a + da * (1 - a);
    float c[3] = {r, g, b};
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf((c[i] * a + p[i] * da * (1 - a)) / oa, 0, 255);
    p[3] = (uint8_t)clampf(oa * 255, 0, 255);
}

/* the water's colour across a channel: t 0 its middle .. 1 its side */
static void water_rgb(float t, float *c)
{
    static const float deep[3] = {28, 88, 160}, mid[3] = {66, 146, 212}, hi[3] = {176, 222, 246};
    float k = clampf(t, 0, 1), h = expf(-k * k * 9);
    for (int i = 0; i < 3; i++) c[i] = mid[i] + (deep[i] - mid[i]) * k * k + (hi[i] - mid[i]) * 0.55f * h;
}

/* brass lit from the upper left: n the surface's slope (x, y, a part of its radius) */
static void brass_rgb(float nx, float ny, float *c)
{
    static const float base[3] = {196, 150, 66};
    float nz = sqrtf(fmaxf(0, 1 - nx * nx - ny * ny));
    float l = clampf(-0.42f * nx - 0.5f * ny + 0.76f * nz, 0, 1), spec = powf(l, 14);
    for (int i = 0; i < 3; i++) c[i] = base[i] * (0.42f + 0.72f * l) + 255 * 0.4f * spec;
}

/* one tile's pipe (sides: its pipes), painted over a picture (w x h) in the q x q square at (x0, y0): brass with an
 * open channel (dry: dark brass; K_WET: water), an end's basin (K_BASIN), the source's tank (K_TANK) */
static void paint_tile(uint8_t *o, int w, int h, int x0, int y0, int q, unsigned sides, int kind)
{
    static const float ax[4] = {0, 1, 0, -1}, ay[4] = {-1, 0, 1, 0};
    for (int iy = 0; iy < q; iy++)
        for (int ix = 0; ix < q; ix++) {
            int X = x0 + ix, Y = y0 + iy;
            if (X < 0 || Y < 0 || X >= w || Y >= h) continue;
            float u = (ix + 0.5f) / q - 0.5f, v = (iy + 0.5f) / q - 0.5f;
            float dd = 1e9f, px = 0, py = 0;    /* the nearest pipe's axis: how far, which way */
            for (int d = 0; d < 4; d++) {
                if (!(sides >> d & 1)) continue;
                float t = clampf(u * ax[d] + v * ay[d], 0, 0.8f), qx = u - ax[d] * t, qy = v - ay[d] * t;
                float e = hypotf(qx, qy);
                if (e < dd) dd = e, px = qx, py = qy;
            }
            /* a wall from r0 to r1 round the axis (the pipe's, or the basin's round the middle): its rounded lip */
            float r0 = RI, r1 = RO, dc = hypotf(u, v), e = dd;
            if (kind & K_BASIN && dc / RB < dd / RO) r0 = RBI, r1 = RB, e = dc, px = u, py = v;
            float cover = clampf((r1 - e) * q + 0.5f, 0, 1);
            if (cover <= 0) continue;
            float s = clampf((e - (r0 + r1) / 2) / ((r1 - r0) / 2), -1, 1), c[3];
            brass_rgb(px / (e + 1e-6f) * s * 0.95f, py / (e + 1e-6f) * s * 0.95f, c);
            if ((r1 - e) * q < 1.2f)           /* the rim's dark line */
                for (int i = 0; i < 3; i++) c[i] *= 0.6f + 0.3f * clampf((r1 - e) * q / 1.2f, 0, 1);
            float ch = dd, cht = dd / RI;       /* the channel (into the basin) */
            if (kind & K_BASIN && dc < RBI && dc / RBI * RI < ch) ch = dc / RBI * RI, cht = dc / RBI;
            float inner = clampf((RI - ch) * q + 0.5f, 0, 1);
            if (inner > 0) {
                float wc[3];
                if (kind & K_WET) water_rgb(cht, wc);
                else
                    for (int i = 0; i < 3; i++) wc[i] = (i == 0 ? 74 : i == 1 ? 52 : 26) * (0.7f + 0.3f * cht);
                for (int i = 0; i < 3; i++) c[i] += (wc[i] - c[i]) * inner;
            }
            mix_over(o + 4 * ((size_t)Y * w + X), c[0], c[1], c[2], cover);
        }
    if (!(kind & K_TANK)) return;
    for (int iy = 0; iy < q; iy++)              /* the tank: a brass ring, rivets, the water */
        for (int ix = 0; ix < q; ix++) {
            int X = x0 + ix, Y = y0 + iy;
            if (X < 0 || Y < 0 || X >= w || Y >= h) continue;
            float u = (ix + 0.5f) / q - 0.5f, v = (iy + 0.5f) / q - 0.5f, dc = hypotf(u, v);
            float cover = clampf((TANK - dc) * q + 0.5f, 0, 1);
            if (cover <= 0) continue;
            float c[3], mid = (TANK + TANK_IN) / 2, half = (TANK - TANK_IN) / 2;
            float s = clampf((dc - mid) / half, -1, 1);   /* across the ring: a rounded rim */
            brass_rgb(u / (dc + 1e-6f) * s, v / (dc + 1e-6f) * s, c);
            if ((TANK - dc) * q < 1.3f)
                for (int i = 0; i < 3; i++) c[i] *= 0.6f;
            for (int k = 0; k < 8; k++) {      /* the rivets */
                float a = k * PUZ_PI / 4 + PUZ_PI / 8, rx = cosf(a) * mid, ry = sinf(a) * mid;
                float rd = hypotf(u - rx, v - ry), rr = 0.022f;
                if (rd < rr + 0.5f / q) {
                    float rc[3], cov = clampf((rr - rd) * q + 0.5f, 0, 1);
                    brass_rgb((u - rx) / rr * 0.9f, (v - ry) / rr * 0.9f, rc);
                    for (int i = 0; i < 3; i++) c[i] += (rc[i] * 1.08f - c[i]) * cov;
                }
            }
            float in = clampf((TANK_IN - dc) * q + 0.5f, 0, 1);
            if (in > 0) {
                float wc[3];
                water_rgb(dc / TANK_IN * 0.85f, wc);
                for (int i = 0; i < 3; i++) wc[i] *= 0.8f + 0.2f * (dc / TANK_IN);
                float sh = expf(-powf(hypotf(u + 0.09f, v + 0.1f) / 0.07f, 2));   /* a gleam */
                for (int i = 0; i < 3; i++) c[i] += (fminf(255, wc[i] + 120 * sh) - c[i]) * in;
            }
            mix_over(o + 4 * ((size_t)Y * w + X), c[0], c[1], c[2], cover);
        }
}

static const uint8_t SHAPE_SIDES[4] = {PP_UP, PP_UP | PP_DOWN, PP_UP | PP_RIGHT, PP_UP | PP_RIGHT | PP_DOWN};

/* the water's pictures: a channel's strip (centre to edge, upwards), the joint's disc, a basin's disc */
static SDL_Texture *water_strip(int q)
{
    int w = (int)ceilf(2 * RI * q) + 2, h = (int)ceilf(0.5f * q) + 1;
    uint8_t *b = ui_buf(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float across = fabsf(x + 0.5f - w / 2.0f) / (RI * q), c[3];
            water_rgb(across, c);
            uint8_t *p = b + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)c[i];
            p[3] = (uint8_t)(255 * clampf((1 - across) * RI * q + 0.5f, 0, 1));
        }
    return ui_tex(w, h, b);
}

static SDL_Texture *water_disc(float r)
{
    int w = (int)ceilf(2 * r) + 2;
    uint8_t *b = ui_buf(w, w);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            float d = hypotf(x + 0.5f - w / 2.0f, y + 0.5f - w / 2.0f) / r, c[3];
            water_rgb(d, c);
            uint8_t *p = b + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)c[i];
            p[3] = (uint8_t)(255 * clampf((1 - d) * r + 0.5f, 0, 1));
        }
    return ui_tex(w, w, b);
}

/* the wrapped board's portholes: a brass ring round a dark hole in the frame, at each edge tile */
static void portholes(uint8_t *o, int S, int f, int n, int q, int x0, int y0)
{
    float r = fminf(0.42f * f, 1.1f * RO * q), ri = 0.6f * r;
    for (int i = 0; i < n; i++)
        for (int e = 0; e < 4; e++) {
            float cx = e < 2 ? x0 + (i + 0.5f) * q : e == 2 ? f / 2.0f : S - f / 2.0f;
            float cy = e >= 2 ? y0 + (i + 0.5f) * q : e == 0 ? f / 2.0f : S - f / 2.0f;
            for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
                for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
                    if (x < 0 || y < 0 || x >= S || y >= S) continue;
                    float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = hypotf(dx, dy);
                    float cover = clampf(r - d + 0.5f, 0, 1);
                    if (cover <= 0) continue;
                    float c[3], s = clampf((d - (r + ri) / 2) / ((r - ri) / 2), -1, 1);
                    brass_rgb(dx / (d + 1e-6f) * s, dy / (d + 1e-6f) * s, c);
                    float hole = clampf(ri - d + 0.5f, 0, 1);
                    for (int k = 0; k < 3; k++) c[k] += ((k == 0 ? 40 : k == 1 ? 28 : 18) - c[k]) * hole;
                    mix_over(o + 4 * ((size_t)y * S + x), c[0], c[1], c[2], cover);
                }
        }
}

/* the board: the plank in its frame, the tiles cut in it; the wrapped board's portholes */
static void paint_board(uint8_t *o, int S, int f, int n, int q, int x0, int y0, int wrap)
{
    pzk_tiles(o, S, f, n, n, q, x0, y0);
    if (wrap) portholes(o, S, f, n, q, x0, y0);
}

/* the token: a wooden tile, a bend full of water */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = (int)(0.86f * size), o = (size - w) / 2;
    uint8_t *b = calloc((size_t)w * w, 4);
    if (!b) return;
    art_board_n(1, 1, w, 0, 0, b);
    puz_bevel(b, w, w, 0.12f * w, fmaxf(1, 0.06f * w), 0.3f);
    paint_tile(b, w, w, 0, 0, w, PP_RIGHT | PP_DOWN, K_WET);
    art_blend(rgba, size, size, b, w, w, o, o);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h));
    side = S;
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    fr = (int)(S * FRAME);
    cs = (int)((S - 2 * fr) / G.n);
    ox = oy = (int)((S - G.n * cs) / 2);
    int key = ((int)S * 64 + serial % 64) * 2 + G.wrap;
    if (made_key != key) {
        made_key = key;
        int w = (int)S;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), w, fr, G.n, cs, ox, oy, G.wrap);
        t_board = ui_tex(w, w, ui_buf(w, w));
        for (int s = 0; s < 4; s++) {
            ui_free(&t_pipe[s]);
            memset(ui_buf(cs, cs), 0, (size_t)cs * cs * 4);
            paint_tile(ui_buf(cs, cs), cs, cs, 0, 0, cs, SHAPE_SIDES[s], s == PP_END ? K_BASIN : 0);
            t_pipe[s] = ui_tex(cs, cs, ui_buf(cs, cs));
        }
        ui_free(&t_tank);
        memset(ui_buf(cs, cs), 0, (size_t)cs * cs * 4);
        paint_tile(ui_buf(cs, cs), cs, cs, 0, 0, cs, 0, K_TANK);
        t_tank = ui_tex(cs, cs, ui_buf(cs, cs));
        ui_free(&t_strip);
        t_strip = water_strip(cs);
        ui_free(&t_hub);
        t_hub = water_disc(RI * cs);
        ui_free(&t_basin);
        t_basin = water_disc(RBI * cs);
    }
    ui_board_used(bx, by, S, S);
}

/* ---- the water --------------------------------------------------------------------------------------------------- */
static float spin_now(int t, Uint32 now)      /* the turn still to come (degrees: below 0, behind) */
{
    float p = (now - spin_t0[t]) / (float)SPIN_MS;
    return p >= 1 ? 0 : spin_off[t] * (1 - puz_ease(p));
}

static int spinning(int t, Uint32 now) { return spin_off[t] != 0 && now - spin_t0[t] < SPIN_MS; }

/* the tile's pipe turned on the screen from its sides before to now: dir 1 clockwise, -1 back */
static void spin(int t, uint8_t was, uint8_t now_sides, int dir)
{
    Uint32 now = ui_ms();
    int k0, k1, shape = pp_shape(was, &k0), period = shape == PP_STRAIGHT ? 2 : 4;
    pp_shape(now_sides, &k1);
    int q = dir > 0 ? ((k1 - k0) % period + period) % period : -(((k0 - k1) % period + period) % period);
    if (!q) q = dir > 0 ? period : -period;
    if (shape == PP_STRAIGHT && q == 2) q = 0;
    spin_off[t] = spin_now(t, now) - 90.0f * q;
    spin_t0[t] = now;
}

/* the water after a change: the tiles newly joined fill after the tile they come from, the others keep their time;
 * the tiles cut off drain */
static void flow(void)
{
    Uint32 now = ui_ms();
    int nt = G.n * G.n, order[PP_TILES], no = 0;
    uint8_t retimed[PP_TILES] = {0};
    pp_flow(&G, dist_now, from_now);
    max_dist = 0;
    for (int t = 0; t < nt; t++)
        if (dist_now[t] > max_dist) max_dist = dist_now[t];
    for (int d = 0; d <= max_dist; d++)
        for (int t = 0; t < nt; t++)
            if (dist_now[t] == d) order[no++] = t;
    for (int t = 0; t < nt; t++)
        if (dist_now[t] < 0 && wet[t]) wet[t] = 0, dry_t0[t] = now;
    for (int i = 0; i < no; i++) {
        int t = order[i], f = from_now[t];
        int changed = !wet[t] || wet_mask[t] != G.now[t] || wet_from[t] != f || (f >= 0 && retimed[f]);
        if (changed) {
            Uint32 st = now;
            if (spinning(t, now)) st = spin_t0[t] + SPIN_MS;
            if (f >= 0 && wet_t0[f] + STEP_MS > st) st = wet_t0[f] + STEP_MS;
            wet_t0[t] = st;
            retimed[t] = 1;
        }
        wet[t] = 1;
        wet_from[t] = f;
        wet_mask[t] = G.now[t];
    }
    if (pp_solved(&G) && !win.on) {
        win.on = 1;
        win.t0 = now;
        for (int t = 0; t < nt; t++)
            if (wet_t0[t] + STEP_MS > win.t0) win.t0 = wet_t0[t] + STEP_MS;
    }
    ui_animate();
}

static Uint32 win_end(void) { return win.t0 + (Uint32)(max_dist * WAVE_STEP + WAVE_LEN); }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(&ptr, 0, sizeof ptr);
    hint.tile = -1;
    win.on = 0;
}

static void reset_water(void)
{
    memset(wet, 0, sizeof wet);
    memset(spin_off, 0, sizeof spin_off);
    for (int t = 0; t < PP_TILES; t++) wet_from[t] = -1;
}

static void begin(void)
{
    serial++;
    calm();
    reset_water();
    flow();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!pp_decode(&G, pp_levels[level < 0 || level >= PP_LEVELS ? 0 : level])) pp_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random (the die tab) */
{
    pp_new(&G, variant < 0 || variant >= PP_VARIANTS ? 1 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int score(void) { return G.moves; }
static int par(void) { return G.par; }
static int level_stars(void) { return pzk_stars(ui_par_stars(G.moves, G.par), G.hints, 0); }

static int status(void) { return pp_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return win.on && ui_ms() < win_end(); }

static int undo(void)
{
    if (!G.nh) return 0;
    uint8_t was[PP_TILES];
    memcpy(was, G.now, sizeof was);
    int t = pp_undo(&G);
    calm();
    spin(t, was[t], G.now[t], -1);
    flow();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = pp_script(&G, moves);
    flow();
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void tile_centre(int t, float *x, float *y)
{
    *x = bx + ox + (t % G.n + 0.5f) * cs;
    *y = by + oy + (t / G.n + 0.5f) * cs;
}

static int tile_at(float x, float y)
{
    float ix = x - bx - ox, iy = y - by - oy;
    if (ix < 0 || iy < 0 || ix >= G.n * cs || iy >= G.n * cs) return -1;
    return (int)(iy / cs) * G.n + (int)(ix / cs);
}

/* a part of a channel's water along side d of the tile at (cx, cy): from its edge (in) or from its middle, a 0..1 */
static void strip(float cx, float cy, int d, float a, int from_edge, Uint8 alpha)
{
    if (a <= 0) return;
    int tw, th;
    SDL_QueryTexture(t_strip, NULL, NULL, &tw, &th);
    float half = 0.5f * cs, w = (float)tw * cs / cs, len = half * a;
    int rows = (int)ceilf(th * a);
    if (rows < 1) rows = 1;
    SDL_Rect src = {0, from_edge ? 0 : th - rows, tw, rows};
    SDL_FRect dst = {cx - w / 2, from_edge ? cy - half : cy - len, w, from_edge ? len : len};
    SDL_FPoint centre = {w / 2, from_edge ? half : len};
    SDL_SetTextureAlphaMod(t_strip, alpha);
    SDL_RenderCopyExF(ui.r, t_strip, &src, &dst, 90.0 * d, &centre, SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(t_strip, 255);
}

static void disc(SDL_Texture *t, float cx, float cy, float d, Uint8 alpha)
{
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, cx, cy, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

/* the water in tile t: f 0..1 of its way through (in by the side it comes from, then out by the others) */
static void water(int t, uint8_t sides, int in, float f, Uint8 alpha)
{
    float cx, cy;
    tile_centre(t, &cx, &cy);
    int shape = pp_shape(sides, NULL);
    if (in >= 0) strip(cx, cy, in, clampf(f / 0.5f, 0, 1), 1, alpha);
    if (f < 0.5f && in >= 0) return;
    float out = in < 0 ? f : clampf((f - 0.5f) / 0.5f, 0, 1);
    for (int d = 0; d < 4; d++)
        if ((sides >> d & 1) && d != in) strip(cx, cy, d, out, 0, alpha);
    if (shape != PP_STRAIGHT) disc(t_hub, cx, cy, 2 * RI * cs + 1, alpha);   /* the joint's corner */
    if (shape == PP_END && t != G.source) {
        float k = 0.35f + 0.65f * puz_ease(out);
        disc(t_basin, cx, cy, (2 * RBI * cs + 2) * k, alpha);
    }
}

/* the side tile t's water comes in by (-1: the source) */
static int side_from(int t, int f)
{
    if (f < 0) return -1;
    for (int d = 0; d < 4; d++)
        if ((G.now[t] >> d & 1) && pp_next(G.n, G.wrap, t, d) == f) return d;
    return -1;
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    int nt = G.n * G.n;
    float sh = fmaxf(1.5f, 0.045f * cs);
    ui_put(t_board, bx, by, side, side);
    for (int pass = 0; pass < 2; pass++)       /* the pipes' shadows, then the pipes */
        for (int t = 0; t < nt; t++) {
            float cx, cy;
            int k, shape = pp_shape(G.now[t], &k);
            tile_centre(t, &cx, &cy);
            float deg = 90.0f * k + spin_now(t, now);
            SDL_Texture *tx = t_pipe[shape];
            if (pass == 0) {
                SDL_SetTextureColorMod(tx, 0, 0, 0);
                SDL_SetTextureAlphaMod(tx, 70);
                ui_put_rot(tx, cx + sh, cy + sh * 1.2f, (float)cs, (float)cs, deg);
                SDL_SetTextureColorMod(tx, 255, 255, 255);
                SDL_SetTextureAlphaMod(tx, 255);
            } else {
                ui_put_rot(tx, cx, cy, (float)cs, (float)cs, deg);
            }
            if (spinning(t, now)) ui_animate();
        }
    for (int t = 0; t < nt; t++) {             /* the water: flowing in, or draining away */
        if (spinning(t, now)) continue;
        if (wet[t]) {
            if (now < wet_t0[t]) { ui_animate(); continue; }
            float f = (now - wet_t0[t]) / (float)STEP_MS;
            if (f < 1) ui_animate();
            water(t, G.now[t], side_from(t, wet_from[t]), clampf(f, 0, 1), 255);
        } else if (now - dry_t0[t] < DRAIN_MS && dry_t0[t]) {
            float a = 1 - (now - dry_t0[t]) / (float)DRAIN_MS;
            water(t, wet_mask[t], -1, 1, (Uint8)(255 * a));
            ui_animate();
        }
    }
    {   /* the tank */
        float cx, cy;
        tile_centre(G.source, &cx, &cy);
        ui_shadow_flat(cx, cy, 2 * TANK * cs, 2 * TANK * cs, 0.5f);
        ui_put_c(t_tank, cx, cy, (float)cs, (float)cs);
    }
    for (int t = 0; t < nt; t++) {             /* spills: drops at the open mouths the water has reached */
        if (!wet[t] || now < wet_t0[t] + STEP_MS || spinning(t, now)) continue;
        float cx, cy;
        tile_centre(t, &cx, &cy);
        for (int d = 0; d < 4; d++) {
            if (!pp_open(&G, t, d)) continue;
            float ex = cx + (d == 1 ? 0.5f : d == 3 ? -0.5f : 0) * cs;
            float ey = cy + (d == 2 ? 0.5f : d == 0 ? -0.5f : 0) * cs;
            float wob = 0.5f + 0.5f * sinf((now - wet_t0[t]) / 160.0f + t), dx = ex - cx, dy = ey - cy;
            disc(t_basin, ex + 0.12f * dx, ey + 0.12f * dy, (0.3f + 0.04f * wob) * cs, 110);   /* a puddle */
            disc(t_hub, ex + (0.1f + 0.12f * wob) * dx, ey + (0.1f + 0.12f * wob) * dy, 0.13f * cs, 235);
        }
        if (!pp_solved(&G)) ui_animate();
    }
    if (hint.tile >= 0) {                      /* the hint's tile: a gold light, swelling and fading */
        float p = puz_pulse((float)(now - hint.t0), 0, HINT_MS), cx, cy;
        if (p > 0) {
            tile_centre(hint.tile, &cx, &cy);
            ui_mark_gold(cx, cy, 2.4f * cs * p);
            ui_animate();
        } else {
            hint.tile = -1;
        }
    }
    if (win.on && now >= win.t0) {             /* the win: a gold light out from the tank along the pipes */
        float wt = (float)(now - win.t0);
        for (int t = 0; t < nt; t++) {
            float q = puz_pulse(wt, dist_now[t] * WAVE_STEP, WAVE_LEN), cx, cy;
            if (q <= 0) continue;
            tile_centre(t, &cx, &cy);
            ui_mark_gold(cx, cy, 2.2f * cs * q);
        }
        if (now < win_end()) ui_animate();
    } else if (win.on) {
        ui_animate();
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void use_tool(int t);

static void press(float x, float y)
{
    int tool = ui_tool_hit(x, y);
    if (tool >= 0) {
        memset(&ptr, 0, sizeof ptr);
        use_tool(tool);
        return;
    }
    memset(&ptr, 0, sizeof ptr);
    if (pp_solved(&G)) return;
    int t = tile_at(x, y);
    if (t < 0) return;
    ptr.on = 1;
    ptr.tile = t;
}

static void motion(float x, float y)
{
    (void)x;
    (void)y;
}

static void release(float x, float y)
{
    if (!ptr.on) return;
    ptr.on = 0;
    if (tile_at(x, y) != ptr.tile) return;
    int t = ptr.tile;
    uint8_t was = G.now[t];
    if (!pp_tap(&G, t)) { ui_sound(SND_DENY); return; }
    hint.tile = -1;
    spin(t, was, G.now[t], 1);
    ui_sound(SND_MOVE);
    flow();
}

/* ---- the bar: the moves, the hint ------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    ui_count c[1] = {{NULL, G.moves}};   /* a bare number: the shell draws the token */
    ui_bar_fit(b, c, 1, 0);
}

static void use_hint(void)
{
    uint8_t was[PP_TILES];
    memcpy(was, G.now, sizeof was);
    int t = pp_hint(&G);
    if (t < 0) { ui_sound(SND_DENY); return; }
    spin(t, was[t], G.now[t], 1);
    hint.tile = t;
    hint.t0 = ui_ms();
    ui_sound(SND_PLACE);
    flow();
}

static void use_tool(int t)
{
    if (t == 0) use_hint();
}

/* ---- the menu's and the picker's cards: a board of the size, part of it turned right and full of water ---------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 0 : variant, n, wrap, source;
    pp_variant(v, &n, &wrap);
    uint8_t sol[PP_TILES], now[PP_TILES];
    pp_make(n, wrap, 2026u + (uint32_t)v, sol, &source);
    uint32_t r = 77u + (uint32_t)v;
    for (int t = 0; t < n * n; t++) {           /* the tiles far from the tank turned wrong */
        int dx = abs(t % n - n / 2), dy = abs(t / n - n / 2);
        now[t] = dx + dy > n / 2 ? pp_turn(sol[t], 1 + (int)((r = r * 1103515245u + 12345u) >> 16) % 3) : sol[t];
    }
    pp_game T;
    pp_load(&T, n, wrap, source, sol, now);
    int dist[PP_TILES];
    pp_flow(&T, dist, NULL);
    int f = (int)(size * FRAME), q = (size - 2 * f) / n, o = (size - n * q) / 2;
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    paint_board(b, size, f, n, q, o, o, wrap);
    for (int t = 0; t < n * n; t++) {
        int shape = pp_shape(now[t], NULL);
        paint_tile(b, size, size, o + t % n * q, o + t / n * q, q, now[t],
                   (shape == PP_END ? K_BASIN : 0) | (dist[t] >= 0 ? K_WET : 0) | (t == source ? K_TANK : 0));
    }
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
}

/* a chapter's tab: a small board of its biggest size */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const int SIZE[PP_CHAPTERS] = {5, 6, 7, 8, 9, 11};
    pzk_grid_icon(rgba, size, SIZE[chapter < 0 ? 0 : chapter >= PP_CHAPTERS ? PP_CHAPTERS - 1 : chapter]);
}

/* the board, then the shell's tools: the hint */
static void draw_all(void)
{
    draw();
    ui_tool(0, ICON_HINT, pp_solved(&G) ? UI_DIM : 0);
}

static const game pipes_game = {
    .id = "pipes", .order = 400, .variants = PP_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw_all,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 1,
    .levels = PP_LEVELS, .chapters = PP_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .level_stars = level_stars, .score = score, .par = par,
};
REGISTER_GAME(pipes_game)
