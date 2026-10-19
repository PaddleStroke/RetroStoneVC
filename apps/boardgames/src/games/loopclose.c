/*
 * Close the Loops in RetroStone Board, for one player: a plank of the house wood ruled in faint squares, each square a
 * tile carrying a piece of ebony line inlaid in the wood: an end (a round knob), a straight, a corner (a quarter
 * circle, so closed lines run round smoothly), a tee, a cross (the rules: loopclose_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a tile: it turns a quarter clockwise, swinging a hair past and settling back. A cross or an empty square cannot
 * turn: it shivers. The shell's tool: the hint (a tile not yet as the nearest closing way has it, turned into place in
 * gold light). Undo takes back one tap, all the way to the start. The bar: the taps, and at its right end the par by
 * its flag. Closed: the lines light up in gold, the shapes they close fill with soft lacquer colours (red, jade, amber,
 * lapis: neighbours never alike) that breathe in and out, a gold wave running over them.
 * Everything moves on the shell's clock (ui_ms): the state changes at the tap, the pictures follow.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/loopclose.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define FRAME 0.3f                              /* the plank's walnut frame, of a square */
#define ROT_MS 190
#define SHIVER_MS 240
#define WIN_MS 1700
#define HINT_TOOL 0
#define HW 0.105f                               /* half the line's width, of a square */
#define KNOB 0.17f                              /* an end's knob */

static const uint8_t EBONY[3] = {46, 34, 28}, SHEEN[3] = {150, 128, 110};
static const uint8_t FILL[4][3] = {{214, 104, 86}, {92, 170, 132}, {232, 184, 92}, {104, 140, 204}};

static lcl_game G;
static int serial, tool_down = -1, hint_cell = -1;
static struct { uint8_t on, from; Uint32 t0; } rot[LCL_CELLS];
static struct { int on, cell; Uint32 t0; } shiver;
static struct { int on; Uint32 t0; } win;
static Uint32 hint_t0;

static box last_box;
static float bx0, by0, q;
static int made_q = -1, made_serial = -1, made_small = -1, fill_key = -1;
static SDL_Texture *t_board, *t_line[16], *t_fill, *t_par;

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
static int degree(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) + (m >> 3 & 1); }
static int is_corner(int m) { return m == 3 || m == 6 || m == 12 || m == 9; }

/* how far a point of a tile (lx, ly: 0..1 from its top left) is inside its line (> 0 inside), its line's middle
 * at most HW in */
static float line_in(int m, float lx, float ly)
{
    if (!m) return -1;
    if (is_corner(m)) {                         /* a quarter circle round the corner between its arms */
        float cx = m & LCL_RIGHT ? 1.0f : 0.0f, cy = m & LCL_DOWN ? 1.0f : 0.0f;
        return HW - fabsf(hypotf(lx - cx, ly - cy) - 0.5f);
    }
    static const float ex[4] = {0.5f, 1.2f, 0.5f, -0.2f}, ey[4] = {-0.2f, 0.5f, 1.2f, 0.5f};
    float best = -1;
    for (int k = 0; k < 4; k++)
        if (m >> k & 1) best = fmaxf(best, HW - pzk_seg_dist(lx, ly, 0.5f, 0.5f, ex[k], ey[k]));
    if (degree(m) == 1) best = fmaxf(best, KNOB - hypotf(lx - 0.5f, ly - 0.5f));
    return best;
}

/* tile m's line painted over the picture o (W x H) in the square x0, y0, s pixels: a rounded ebony rod inlaid, its
 * shadow on the wood */
static void paint_line(uint8_t *o, int W, int H, float x0, float y0, float s, int m)
{
    static const uint8_t dark[3] = {30, 18, 10};
    float px1 = 1.0f / s;
    for (int y = (int)fmaxf(0, floorf(y0)); y < H && y < y0 + s; y++)
        for (int x = (int)fmaxf(0, floorf(x0)); x < W && x < x0 + s; x++) {
            float lx = (x + 0.5f - x0) / s, ly = (y + 0.5f - y0) / s;
            uint8_t *p = o + 4 * ((size_t)y * W + x);
            float sh = line_in(m, lx - 0.035f, ly - 0.05f);   /* the shadow, down and to the right */
            if (sh > -0.05f) pzk_over(p, dark, 0.3f * clampf((sh + 0.05f) / 0.07f, 0, 1));
            float d = line_in(m, lx, ly);
            float cov = clampf(d / px1 + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float t = clampf(d / HW, 0, 1);    /* 0 at its edge, 1 along its middle */
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(EBONY[i] * (0.75f + 0.5f * sqrtf(t)), 0, 255);
            pzk_over(p, c, cov);
            float sheen = line_in(m, lx + 0.03f, ly + 0.03f);   /* a soft light along it, towards the top left */
            if (sheen > 0.6f * HW) pzk_over(p, SHEEN, 0.35f * cov * clampf((sheen - 0.6f * HW) / (0.3f * HW), 0, 1));
        }
}

/* ---- the shapes closed: the squares between the tiles' middles ("faces", (w + 1) x (h + 1) with the outside's round
 * the edge) joined where no line parts them; each piece of the inside a region, coloured unlike its neighbours ---- */
static int face_of(int w, int i, int j) { return (i + 1) + (j + 1) * (w + 1); }

/* 1: a line parts face (i, j) from the next one, right (d 0) or down (d 1) */
static int parted(int w, int h, const uint8_t *m, int i, int j, int d)
{
    if (d == 0) {                               /* the line between tiles (i + 1, j) and (i + 1, j + 1) */
        int x = i + 1;
        return x >= 0 && x < w && j >= 0 && j + 1 < h && (m[x + j * w] & LCL_DOWN);
    }
    int y = j + 1;                              /* the line between tiles (i, j + 1) and (i + 1, j + 1) */
    return y >= 0 && y < h && i >= 0 && i + 1 < w && (m[i + y * w] & LCL_RIGHT);
}

/* the region of each face (-1 the outside), its colour (0..3) in col; how many regions */
static int regions(int w, int h, const uint8_t *m, int *reg, int *col)
{
    int F = (w + 1) * (h + 1), n = 0, stack[(LCL_MAXW + 1) * (LCL_MAXH + 1)];
    for (int f = 0; f < F; f++) reg[f] = -2;
    for (int f0 = 0; f0 < F; f0++) {
        if (reg[f0] != -2) continue;
        int id = f0 == 0 ? -1 : n++, top = 0;
        stack[top++] = f0;
        reg[f0] = id;
        while (top) {
            int f = stack[--top], i = f % (w + 1) - 1, j = f / (w + 1) - 1;
            static const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                int ni = i + di[k], nj = j + dj[k];
                if (ni < -1 || nj < -1 || ni > w - 1 || nj > h - 1) continue;
                int part = k == 0 ? parted(w, h, m, i, j, 0) : k == 1 ? parted(w, h, m, ni, nj, 0)
                         : k == 2 ? parted(w, h, m, i, j, 1) : parted(w, h, m, ni, nj, 1);
                int g = face_of(w, ni, nj);
                if (part || reg[g] != -2) continue;
                reg[g] = id;
                stack[top++] = g;
            }
        }
    }
    for (int r = 0; r < n; r++) {               /* colours: unlike any neighbour region coloured before */
        int used = 0;
        for (int f = 0; f < F; f++) {
            if (reg[f] != r) continue;
            int i = f % (w + 1) - 1, j = f / (w + 1) - 1;
            if (i < w - 1 && reg[f + 1] >= 0 && reg[f + 1] < r) used |= 1 << col[reg[f + 1]];
            if (i > -1 && reg[f - 1] >= 0 && reg[f - 1] < r) used |= 1 << col[reg[f - 1]];
            if (j < h - 1 && reg[f + w + 1] >= 0 && reg[f + w + 1] < r) used |= 1 << col[reg[f + w + 1]];
            if (j > -1 && reg[f - w - 1] >= 0 && reg[f - w - 1] < r) used |= 1 << col[reg[f - w - 1]];
        }
        int c = (r * 3) % 4;
        for (int k = 0; k < 4 && (used >> c & 1); k++) c = (c + 1) % 4;
        col[r] = c;
    }
    return n;
}

/* the closed shapes filled (alpha a) over the picture o (W x H), the board's first square at x0, y0, s pixels */
static void paint_fill(uint8_t *o, int W, int H, float x0, float y0, float s, int w, int h, const uint8_t *m, float a)
{
    int reg[(LCL_MAXW + 1) * (LCL_MAXH + 1)], col[(LCL_MAXW + 1) * (LCL_MAXH + 1)];
    if (!regions(w, h, m, reg, col)) return;
    for (int y = (int)fmaxf(0, y0); y < H && y < y0 + h * s; y++)
        for (int x = (int)fmaxf(0, x0); x < W && x < x0 + w * s; x++) {
            float fx = (x + 0.5f - x0) / s, fy = (y + 0.5f - y0) / s;
            int tx = (int)fx, ty = (int)fy;
            if (tx >= w || ty >= h) continue;
            float lx = fx - tx, ly = fy - ty;
            int qx = lx >= 0.5f, qy = ly >= 0.5f, t = m[tx + ty * w];
            if (is_corner(t)) {                 /* outside the quarter circle: the other side of the line */
                int cx = !!(t & LCL_RIGHT), cy = !!(t & LCL_DOWN);
                if (qx == cx && qy == cy && hypotf(lx - cx, ly - cy) > 0.5f) { qx = !qx; qy = !qy; }
            }
            int r = reg[face_of(w, tx - 1 + qx, ty - 1 + qy)];
            if (r >= 0) pzk_over(o + 4 * ((size_t)y * W + x), FILL[col[r]], a);
        }
}

/* the plank, its squares ruled faintly */
static void paint_board(int w, int h, int s, int f, uint8_t *o)
{
    int W = w * s + 2 * f, H = h * s + 2 * f;
    art_plank(w * s, h * s, f, o);
    float t = fmaxf(1, 0.02f * s);
    for (int i = 1; i < w; i++) art_tint_rect(o, W, H, f + i * s - t / 2, (float)f, f + i * s + t / 2, (float)(H - f), PUZ_INK, 0.13f);
    for (int i = 1; i < h; i++) art_tint_rect(o, W, H, (float)f, f + i * s - t / 2, (float)(W - f), f + i * s + t / 2, PUZ_INK, 0.13f);
}

/* a board in a picture size x size: the plank, the tiles m, the shapes filled (fill: alpha, 0 none) */
static void paint_mini(int w, int h, const uint8_t *m, int size, uint8_t *rgba, float fill)
{
    int s = (int)(size / (fmaxf((float)w, (float)h) + 2 * FRAME)), f = (int)(FRAME * s);
    if (s < 2) return;
    int W = w * s + 2 * f, H = h * s + 2 * f;
    uint8_t *o = malloc((size_t)W * H * 4);
    if (!o) return;
    paint_board(w, h, s, f, o);
    if (fill > 0) paint_fill(o, W, H, (float)f, (float)f, (float)s, w, h, m, fill);
    for (int c = 0; c < w * h; c++) paint_line(o, W, H, (float)(f + c % w * s), (float)(f + c / w * s), (float)s, m[c]);
    art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    free(o);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    lcl_map m;
    if (!lcl_parse(&m, "6c|39")) return;
    memset(rgba, 0, (size_t)size * size * 4);
    float s = 0.42f * size, x0 = (size - 2 * s) / 2;
    paint_fill(rgba, size, size, x0, x0, s, 2, 2, m.start, 0.85f);
    for (int c = 0; c < 4; c++) paint_line(rgba, size, size, x0 + c % 2 * s, x0 + c / 2 * s, s, m.start[c]);
}

/* ---- the layout -------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    int w = G.m.w, h = G.m.h;
    q = floorf(fminf(b.w / (w + 2 * FRAME), b.h / (h + 2 * FRAME)));
    if (q > 1.7f * ui.unit) q = floorf(1.7f * ui.unit);
    if (q < 4) q = 4;
    bx0 = floorf(b.x + (b.w - w * q) / 2);
    by0 = floorf(b.y + (b.h - h * q) / 2);
    int s = (int)q, f = (int)(FRAME * q);
    if (made_q != s || made_serial != serial) {
        int W = w * s + 2 * f, H = h * s + 2 * f;
        uint8_t *o = malloc((size_t)W * H * 4);
        ui_free(&t_board);
        if (o) {
            paint_board(w, h, s, f, o);
            t_board = ui_tex(W, H, o);
        }
        free(o);
        if (made_q != s)
            for (int k = 0; k < 16; k++) {
                uint8_t *p = ui_buf(s, s);
                memset(p, 0, (size_t)s * s * 4);
                paint_line(p, s, s, 0, 0, (float)s, k);
                ui_free(&t_line[k]);
                t_line[k] = ui_tex(s, s, p);
            }
        made_q = s;
        fill_key = -1;
    }
    made_serial = serial;
    ui_board_used(bx0 - f, by0 - f, w * q + 2 * f, h * q + 2 * f);
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_par);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
    }
}

/* the closed shapes' colours over the board (made once the board is closed) */
static void make_fill(void)
{
    int s = (int)q, key = s * 4096 + serial % 4096;
    if (fill_key == key) return;
    fill_key = key;
    int W = G.m.w * s, H = G.m.h * s;
    uint8_t *o = calloc((size_t)W * H, 4);
    ui_free(&t_fill);
    if (o) {
        paint_fill(o, W, H, 0, 0, (float)s, G.m.w, G.m.h, G.cur, 1);
        t_fill = ui_tex(W, H, o);
    }
    free(o);
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    memset(rot, 0, sizeof rot);
    shiver.on = win.on = 0;
    tool_down = hint_cell = -1;
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!lcl_level(&G, level)) lcl_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    lcl_random(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return lcl_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return pzk_stars(ui_par_stars(G.moves, par()), G.hints, 0); }

static int busy(void)
{
    Uint32 now = ui_ms();
    for (int c = 0; c < G.m.w * G.m.h; c++)
        if (rot[c].on && now - rot[c].t0 < ROT_MS) return 1;
    return win.on && (Sint32)(now - win.t0) < WIN_MS;
}

/* a tap on tile c (or the hint's taps): turned, its swing set going */
static void turn_tile(int c, int taps)
{
    Uint32 now = ui_ms();
    int from = G.cur[c], done = 0;
    while (taps-- > 0) done += lcl_tap(&G, c);
    if (!done) {
        shiver.on = 1;
        shiver.cell = c;
        shiver.t0 = now;
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    rot[c].on = 1;
    rot[c].from = (uint8_t)from;
    rot[c].t0 = now;
    ui_sound(SND_TAP);
    if (lcl_solved(&G)) {
        win.on = 1;
        win.t0 = now + ROT_MS;
        ui_sound_later(SND_END, ROT_MS);
    }
    ui_animate();
}

static void hint(void)
{
    int c, taps;
    if (!lcl_hint(&G, &c, &taps)) { ui_sound(SND_DENY); return; }
    G.hints++;
    hint_cell = c;
    hint_t0 = ui_ms();
    turn_tile(c, taps);
}

static int undo(void)
{
    calm();
    return lcl_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = lcl_script(&G, moves);
    if (lcl_solved(&G)) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static float ease_back(float u)                 /* a hair past the end, then back */
{
    u = clampf(u, 0, 1) - 1;
    return 1 + 2.2f * u * u * u + 1.2f * u * u;
}

/* how many quarter turns from a tile to the other (as it turned: clockwise) */
static int quarters(int from, int to)
{
    int t = lcl_taps(from, to);
    return t < 0 ? 0 : t;
}

static void draw(void)
{
    if (made_q < 0 || made_serial != serial) layout(last_box);
    Uint32 now = ui_ms();
    int f = (int)(FRAME * q), any = 0, solved = lcl_solved(&G), n = G.m.w * G.m.h;
    ui_put(t_board, bx0 - f, by0 - f, G.m.w * q + 2 * f, G.m.h * q + 2 * f);
    float wt = win.on ? (float)(Sint32)(now - win.t0) : -1;
    if (solved && wt >= 0) {                    /* the shapes filled, fading in, then breathing */
        make_fill();
        float a = clampf(wt / 500, 0, 1) * (0.62f + 0.16f * sinf(wt / 420.0f));
        if (t_fill) {
            SDL_SetTextureAlphaMod(t_fill, (Uint8)(255 * a));
            ui_put(t_fill, bx0, by0, G.m.w * q, G.m.h * q);
        }
        any = 1;
    }
    if (hint_cell >= 0) {                       /* the hint's tile in gold light, fading */
        float a = 1 - clampf((now - hint_t0) / 1400.0f, 0, 1);
        if (a > 0) {
            ui_mark_gold_a(bx0 + (hint_cell % G.m.w + 0.5f) * q, by0 + (hint_cell / G.m.w + 0.5f) * q, 1.7f * q, a);
            any = 1;
        } else hint_cell = -1;
    }
    for (int c = 0; c < n; c++) {
        float cx = bx0 + (c % G.m.w + 0.5f) * q, cy = by0 + (c / G.m.w + 0.5f) * q, deg = 0;
        int m = G.cur[c];
        if (rot[c].on) {
            float u = (now - rot[c].t0) / (float)ROT_MS;
            if (u < 1) {
                m = rot[c].from;
                deg = 90.0f * quarters(rot[c].from, G.cur[c]) * ease_back(u);
                any = 1;
            } else rot[c].on = 0;
        }
        if (shiver.on && shiver.cell == c) {
            float u = (now - shiver.t0) / (float)SHIVER_MS;
            if (u < 1) { deg = 7 * sinf(u * PUZ_PI * 4) * (1 - u); any = 1; }
            else shiver.on = 0;
        }
        if (!m) continue;
        ui_put_rot(t_line[m], cx, cy, q, q, deg);
    }
    if (solved && wt >= 0) {                    /* the lines in gold light, a wave running over them */
        float mx = G.m.w / 2.0f, my = G.m.h / 2.0f, far = hypotf(mx, my) + 0.5f;
        for (int c = 0; c < n; c++) {
            int m = G.cur[c];
            if (!m) continue;
            float x = c % G.m.w + 0.5f, y = c / G.m.w + 0.5f, d = hypotf(x - mx, y - my);
            float glow = clampf(wt / 400, 0, 1) * (0.55f + 0.2f * sinf(wt / 420.0f)) +
                         0.45f * puz_pulse(wt, 700 * d / far, 650);
            SDL_Texture *t = t_line[m];
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
            SDL_SetTextureColorMod(t, 255, 196, 90);
            SDL_SetTextureAlphaMod(t, (Uint8)clampf(255 * glow, 0, 255));
            ui_put(t, bx0 + (x - 0.5f) * q, by0 + (y - 0.5f) * q, q, q);
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(t, 255, 255, 255);
            SDL_SetTextureAlphaMod(t, 255);
        }
    }
    if (any || (solved && win.on)) ui_animate();
    ui_tool(HINT_TOOL, ICON_HINT, (solved ? UI_DIM : 0) | (tool_down == HINT_TOOL ? UI_ON : 0));
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int cell_at(float x, float y)
{
    float fx = (x - bx0) / q, fy = (y - by0) / q;
    if (fx < -0.15f || fy < -0.15f || fx >= G.m.w + 0.15f || fy >= G.m.h + 0.15f) return -1;
    int cx = (int)clampf(floorf(fx), 0, G.m.w - 1), cy = (int)clampf(floorf(fy), 0, G.m.h - 1);
    return cx + cy * G.m.w;
}

static int down_cell = -1;

static void press(float x, float y)
{
    tool_down = ui_tool_hit(x, y);
    down_cell = tool_down >= 0 ? -1 : cell_at(x, y);
    if (tool_down >= 0) ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }

static void release(float x, float y)
{
    if (tool_down >= 0) {
        int k = tool_down;
        tool_down = -1;
        ui_animate();
        if (ui_tool_hit(x, y) == k && k == HINT_TOOL && !lcl_solved(&G)) hint();
        return;
    }
    int c = down_cell;
    down_cell = -1;
    if (c < 0 || lcl_solved(&G)) return;
    turn_tile(c, 1);
}

static void bar(int player, box b)
{
    (void)player;
    pzt_bar_two_cream(b, NULL, G.moves, t_par, par(), lcl_solved(&G) && G.moves <= par());
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* a design closed and coloured, as the maker draws it: the first seed from `seed` giving at least `least` shapes */
static void paint_design(lcl_spec sp, uint32_t seed, int least, int size, uint8_t *rgba)
{
    lcl_map m;
    int reg[(LCL_MAXW + 1) * (LCL_MAXH + 1)], col[(LCL_MAXW + 1) * (LCL_MAXH + 1)];
    sp.mix = -1;
    for (uint32_t k = 0; k < 200; k++)
        if (lcl_make(&m, &sp, seed + k) && regions(m.w, m.h, m.start, reg, col) >= least) {
            paint_mini(m.w, m.h, m.start, size, rgba, 0.75f);
            return;
        }
}

/* the menu's card: a mandala of closed shapes; a picker's card: a design of that size */
static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant >= 0) {
        lcl_spec sp;
        lcl_variant(variant, &sp);
        paint_design(sp, 31u + 100u * (uint32_t)variant, 3, size, rgba);
        return;
    }
    lcl_spec sp = {6, 6, 2, 55, 0, 0, -1};
    paint_design(sp, 1, 4, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    static lcl_game t;
    if (!lcl_level(&t, level)) return;
    uint8_t way[LCL_CELLS];
    if (ui_level_best(level) >= 0 && lcl_solve(t.m.w, t.m.h, t.m.start, way, NULL) >= 0)
        paint_mini(t.m.w, t.m.h, way, size, rgba, 0.7f);   /* solved: its shapes closed and coloured */
    else paint_mini(t.m.w, t.m.h, t.m.start, size, rgba, 0);
}

/* a chapter's tab: a ring; a ring parted (tees); a line ending in knobs; the bigger boards' and the largest boards'
 * showpieces closed; a mandala (a cross in a ring) */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const char *const ICON[LCL_CHAPTERS] = {"6c|39", "6ec|555|3b9", "6a8|500|3a8", NULL, "6ec|7fd|3b9", NULL};
    lcl_map m;
    int ch = chapter < 0 || chapter >= LCL_CHAPTERS ? 0 : chapter;
    if (ICON[ch]) {
        if (lcl_parse(&m, ICON[ch])) paint_mini(m.w, m.h, m.start, size, rgba, 0.8f);
        return;
    }
    static lcl_game t;
    uint8_t way[LCL_CELLS];
    if (lcl_level(&t, (ch + 1) * (LCL_LEVELS / LCL_CHAPTERS) - 1) && lcl_solve(t.m.w, t.m.h, t.m.start, way, NULL) >= 0)
        paint_mini(t.m.w, t.m.h, way, size, rgba, 0.8f);
}

static const game loopclose_game = {
    .id = "loopclose", .order = 447, .variants = LCL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = LCL_LEVELS, .chapters = LCL_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par, .fill = 1, .tools = 1,
};
REGISTER_GAME(loopclose_game)
