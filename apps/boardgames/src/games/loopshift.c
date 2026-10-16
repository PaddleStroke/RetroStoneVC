/*
 * Row Shift in RetroStone Board, for one player: a walnut frame round a sunk floor, and its tiles: numbered ones cut
 * from one plank of the house wood (in order, the grain runs on from tile to tile), their numbers burned in; or colour
 * bands, the wood stained in six colours told apart by colourblind eyes too, each with its own sign, the colour of each
 * row shown on the frame at both its ends (the rules: loopshift_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag a tile along its row or its column: the whole line follows the finger, wrapping round (what leaves one end comes
 * in at the other), and snaps to the nearest cell when the finger lifts. Or tap a tile (it is picked), then another
 * of its row or column: the line slides so the picked tile goes there. The hint (a tool) shows the next move: the line
 * in gold light and an arrow as long as the slide. Undo takes back one move, all the way to the start. The bar: the
 * moves, and the par at its right end, lit gold when matched. Solved: a gold light runs over the tiles in order.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/loopshift.h"
#include "games/puzzles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "shell.h"

#define GAP 0.035f                              /* between two tiles, a part of a cell */
#define SNAP_MS 140
#define SLIDE_MS 110                            /* a step of a tapped or undone slide (the longest: three) */
#define WAVE_STEP 45
#define WAVE_LEN 420
#define HINT_MS 1400                            /* one pulse of the hint's light */

enum { B_HINT, BTNS };

/* the colour bands: colourblind-safe tones (after Okabe and Ito) and a sign each */
static const uint8_t BAND[LSH_MAX][3] = {
    {213, 94, 0}, {86, 180, 233}, {240, 228, 66}, {0, 158, 115}, {204, 121, 167}, {0, 114, 178},
};
enum { SIGN_DISC, SIGN_TRIANGLE, SIGN_SQUARE, SIGN_DIAMOND, SIGN_CROSS, SIGN_STAR };
static const uint8_t GOLD[3] = {236, 196, 98};

static lsh_game G;
static int sel = -1;
static struct { int on, line; float from; Uint32 t0, ms; } anim;
static struct { int on, cx, cy, axis; float x0, y0, off; } drag;   /* axis 1: the row, 2: the column */
static struct { int move; Uint32 t0; } hint = {-1, 0};
static struct { int on; Uint32 t0; } win;

static box last_box;
static ui_grid grid;
static float fr;
static int made_key = -1, made_small = -1, made_head = -1;
static SDL_Texture *t_board, *t_tile[LSH_CELLS], *t_band[LSH_MAX], *t_par, *t_head;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static float frame_cells(int colour) { return colour ? 0.42f : 0.24f; }

static void sign_fig(art_fig *f, int sign, float c, float r)   /* centred on (c, c), about r from it */
{
    switch (sign) {
    case SIGN_DISC: art_ellipse(f, ART_PART, c, c, r, r); break;
    case SIGN_TRIANGLE: {
        const float q[] = {c, c - 1.05f * r, c + r, c + 0.75f * r, c - r, c + 0.75f * r};
        art_poly(f, ART_PART, q, 3);
    } break;
    case SIGN_SQUARE: art_rect(f, ART_PART, c - 0.82f * r, c - 0.82f * r, c + 0.82f * r, c + 0.82f * r); break;
    case SIGN_DIAMOND: {
        const float q[] = {c, c - 1.1f * r, c + 1.1f * r, c, c, c + 1.1f * r, c - 1.1f * r, c};
        art_poly(f, ART_PART, q, 4);
    } break;
    case SIGN_CROSS:
        art_rect(f, ART_PART, c - r, c - 0.33f * r, c + r, c + 0.33f * r);
        art_rect(f, ART_PART, c - 0.33f * r, c - r, c + 0.33f * r, c + r);
        break;
    default: art_star(f, ART_PART, c, c + 0.08f * r, 1.15f * r); break;
    }
}

static int band_dark(int b) { return BAND[b][0] * 0.3f + BAND[b][1] * 0.59f + BAND[b][2] * 0.11f < 120; }

/* a colour band's tile, T x T: the wood stained, its sign inlaid (dark on the light tones, cream on the dark ones) */
static void paint_band(int b, int T, uint8_t *o, int bevel)
{
    pzt_plank(o, T, T, 3 + 7 * b);
    for (int i = 0; i < T * T; i++) pzt_tint(o + 4 * i, BAND[b], 0.95f);
    art_fig *f = art_fig_new();
    sign_fig(f, b, 0.5f, 0.2f);
    const uint8_t *ink = band_dark(b) ? PUZ_CREAM : PUZ_INK;
    pzt_paint(o, T, f, ink, ink, 0, 0.15f);
    if (bevel) puz_bevel(o, T, T, 0.1f * T, fmaxf(2, 0.08f * T), 0.38f);
}

/* tile `home` of n x n numbered tiles, T pixels, cut from the plank (n T x n T) */
static void paint_number(int n, int home, int T, const uint8_t *src, uint8_t *o)
{
    int P = n * T, hx = home % n, hy = home / n;
    for (int y = 0; y < T; y++)
        memcpy(o + 4 * (size_t)y * T, src + 4 * ((size_t)(hy * T + y) * P + hx * T), (size_t)T * 4);
    puz_bevel(o, T, T, 0.08f * T, fmaxf(2, 0.07f * T), 0.34f);
    puz_burn(o, T, T, home + 1, T / 2.0f, T * 0.52f, (home + 1 >= 10 ? 0.42f : 0.46f) * T);
}

/* the board: the walnut frame, the floor sunk (darker, in the frame's shadow at the top and the left); colour bands:
 * each row's colour at both ends, on the frame */
static void paint_board(int n, int colour, int q, int f, uint8_t *o)
{
    int w = n * q + 2 * f;
    art_board_n(1, 1, n * q, f, 0, o);
    float sh = 0.12f * q;
    for (int y = f; y < w - f; y++)
        for (int x = f; x < w - f; x++) {
            float e = fminf((float)(x - f), (float)(y - f)), k = 0.55f - 0.15f * clampf(1 - e / sh, 0, 1);
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
    if (!colour) return;
    int d = (int)(0.72f * f);
    uint8_t *m = d > 3 ? calloc((size_t)d * d, 4) : NULL;
    for (int r = 0; r < n && m; r++) {
        memset(m, 0, (size_t)d * d * 4);
        paint_band(r, d, m, 0);
        for (int i = 0; i < d * d; i++) {         /* a round mark */
            float dx = i % d + 0.5f - d / 2.0f, dy = i / d + 0.5f - d / 2.0f;
            m[4 * i + 3] = (uint8_t)(m[4 * i + 3] * clampf(d / 2.0f - hypotf(dx, dy) + 0.5f, 0, 1));
        }
        int cy = f + r * q + q / 2 - d / 2;
        art_blend(o, w, w, m, d, d, (f - d) / 2, cy);
        art_blend(o, w, w, m, d, d, w - f + (f - d + 1) / 2, cy);
    }
    free(m);
}

/* a whole grid (the menu, the picker, the chapters): the board, its tiles where they are */
static void paint_grid(const int8_t *at, int n, int colour, int size, uint8_t *rgba)
{
    float fc = frame_cells(colour);
    int q = (int)(size / (n + 2 * fc)), f = (int)(fc * q), w = n * q + 2 * f, g = (int)fmaxf(1, GAP * q), T = q - 2 * g;
    uint8_t *b = malloc((size_t)w * w * 4), *src = malloc((size_t)n * T * n * T * 4), *one = malloc((size_t)T * T * 4);
    if (b && src && one && T > 2) {
        paint_board(n, colour, q, f, b);
        if (!colour) art_board_n(1, 1, n * T, 0, 0, src);
        for (int p = 0; p < n * n; p++) {
            memset(one, 0, (size_t)T * T * 4);
            if (colour) paint_band(at[p] / n, T, one, 1);
            else paint_number(n, at[p], T, src, one);
            art_blend(b, w, w, one, T, T, f + (p % n) * q + g, f + (p / n) * q + g);
        }
        art_blend(rgba, size, size, b, w, w, (size - w) / 2, (size - w) / 2);
    }
    free(b);
    free(src);
    free(one);
}

static void token(int player, int size, uint8_t *rgba)   /* four tiles of the bands */
{
    (void)player;
    int T = (int)(0.46f * size), o = (size - 2 * T - 1) / 2;
    uint8_t *t = calloc((size_t)T * T, 4);
    for (int i = 0; i < 4 && t && T > 3; i++) {
        memset(t, 0, (size_t)T * T * 4);
        paint_band(i == 0 || i == 3 ? 0 : 1, T, t, 1);
        art_blend(rgba, size, size, t, T, T, o + (i % 2) * (T + 1), o + (i / 2) * (T + 1));
    }
    free(t);
}

static void layout(box b)
{
    last_box = b;
    float fc = frame_cells(G.colour);
    grid = ui_grid_fit(b, G.n, G.n, fc);
    int q = (int)grid.cell;
    fr = floorf(fc * q);
    int key = (q * 8 + G.n) * 2 + G.colour;
    if (made_key != key) {
        made_key = key;
        int w = G.n * q + 2 * (int)fr, g = (int)fmaxf(1, GAP * q), T = q - 2 * g;
        ui_free(&t_board);
        paint_board(G.n, G.colour, q, (int)fr, ui_buf(w, w));
        t_board = ui_tex(w, w, ui_buf(w, w));
        for (int i = 0; i < LSH_CELLS; i++) ui_free(&t_tile[i]);
        for (int i = 0; i < LSH_MAX; i++) ui_free(&t_band[i]);
        if (G.colour) {
            for (int i = 0; i < G.n; i++) {
                memset(ui_buf(T, T), 0, (size_t)T * T * 4);
                paint_band(i, T, ui_buf(T, T), 1);
                t_band[i] = ui_tex(T, T, ui_buf(T, T));
            }
        } else {
            uint8_t *src = malloc((size_t)G.n * T * G.n * T * 4);
            if (src) art_board_n(1, 1, G.n * T, 0, 0, src);
            for (int i = 0; i < G.n * G.n && src; i++) {
                paint_number(G.n, i, T, src, ui_buf(T, T));
                t_tile[i] = ui_tex(T, T, ui_buf(T, T));
            }
            free(src);
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small && small > 3) {
        made_small = small;                     /* the bar's par: the shell's flag, cream */
        ui_free(&t_par);
        uint8_t *p = ui_buf(small, small);
        memset(p, 0, (size_t)small * small * 4);
        art_icon(ICON_PAR, small, p);
        for (int i = 0; i < small * small; i++)   /* the white icon tinted cream */
            for (int c = 0; c < 3; c++) p[4 * i + c] = (uint8_t)(p[4 * i + c] * PUZ_CREAM[c] / 255);
        t_par = ui_tex(small, small, p);
    }
    int hd = (int)(0.42f * q);
    if (made_head != hd && hd > 3) {            /* the hint's arrow head, pointing right */
        made_head = hd;
        ui_free(&t_head);
        uint8_t *p = ui_buf(hd, hd);
        memset(p, 0, (size_t)hd * hd * 4);
        art_fig *f = art_fig_new();
        const float tri[] = {0.08f, 0.06f, 0.94f, 0.5f, 0.08f, 0.94f};
        art_poly(f, ART_PART, tri, 3);
        art_fig_paint(p, hd, f, GOLD, PUZ_INK, 0.07f, 0.3f);
        t_head = ui_tex(hd, hd, p);
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel = -1;
    anim.on = win.on = 0;
    hint.move = -1;
    memset(&drag, 0, sizeof drag);
}

static void begin(void)
{
    calm();
    made_key = -1;
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)
{
    if (!lsh_level(&G, level)) lsh_level(&G, 0);
    begin();
}

static void start_random(int variant)
{
    lsh_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return lsh_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return lsh_moves(&G); }
static int par(void) { return G.par; }
static int level_stars(void) { return ui_par_stars(lsh_moves(&G), G.par); }

static Uint32 win_len(void) { return (Uint32)((G.n * G.n - 1) * WAVE_STEP + WAVE_LEN); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && now - anim.t0 < anim.ms) || (win.on && now - win.t0 < win_len());
}

static void check_win(Uint32 at)
{
    if (!lsh_solved(&G) || win.on) return;
    win.on = 1;
    win.t0 = at;
    sel = -1;
    hint.move = -1;
}

/* the line slid back to rest from `from` cells away (its tiles already in their new places) */
static void glide(int line, float from, Uint32 ms)
{
    anim.on = 1;
    anim.line = line;
    anim.from = from;
    anim.t0 = ui_ms();
    anim.ms = ms;
    ui_animate();
}

static void play(int move, float from)          /* a move made, the line gliding in from `from` */
{
    lsh_play(&G, move);
    hint.move = -1;
    sel = -1;
    float d = fabsf(from);
    glide(lsh_line(move), from, (Uint32)(d < 0.6f ? SNAP_MS : SLIDE_MS * fminf(3, 0.6f + d)));
    ui_sound(SND_MOVE);
    check_win(anim.t0 + anim.ms);
}

static int undo(void)
{
    calm();
    int m = lsh_undo(&G);
    if (m < 0) return 0;
    float k = (float)lsh_signed(G.n, m);
    glide(lsh_line(m), k, (Uint32)(SLIDE_MS * fminf(3, 0.6f + fabsf(k))));
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = lsh_script(&G, moves);
    check_win(ui_ms());
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static int moving_line(float *off)              /* the line the finger or a glide moves now, how far (cells); -1 */
{
    int n = G.n;
    if (drag.on && drag.axis) {
        *off = drag.off;
        return drag.axis == 1 ? drag.cy : n + drag.cx;
    }
    if (anim.on) {
        Uint32 t = ui_ms() - anim.t0;
        if (t < anim.ms) {
            *off = anim.from * (1 - puz_ease(t / (float)anim.ms));
            return anim.line;
        }
        anim.on = 0;
    }
    *off = 0;
    return -1;
}

static SDL_Texture *tile_tex(int tile) { return G.colour ? t_band[tile / G.n] : t_tile[tile]; }

static void tile_at(int tile, float x, float y, float T)   /* x, y: its top left */
{
    ui_shadow_flat(x + T / 2 + 0.03f * T, y + T / 2 + 0.05f * T, 0.96f * T, 0.96f * T, 0.5f);
    ui_put(tile_tex(tile), x, y, T, T);
}

/* the hint: an arrow along the line, from its middle, as long as the slide */
static void draw_arrow(int move, float a)
{
    int n = G.n, line = lsh_line(move), k = lsh_signed(n, move);
    float q = grid.cell, len = fabsf((float)k) * q, th = 0.16f * q, hd = 0.42f * q;
    float mid = n * q / 2, cx, cy;
    int across = line < n;                      /* a row: the arrow across */
    float c = (line % n + 0.5f) * q;
    cx = grid.x + (across ? mid : c);
    cy = grid.y + (across ? c : mid);
    float s0 = -len / 2, s1 = len / 2 - 0.5f * hd;
    Uint8 al = (Uint8)(255 * a);
    for (int pass = 0; pass < 2; pass++) {      /* its dark edge, then the gold */
        float e = pass ? 0 : fmaxf(1.5f, 0.03f * q), w = th + 2 * e;
        float lo = s0 - e, hi = s1 + e;
        if (k < 0) { float t = lo; lo = -hi; hi = -t; }
        const uint8_t *col = pass ? GOLD : PUZ_INK;
        if (across) ui_fill(cx + lo, cy - w / 2, hi - lo, w, col[0], col[1], col[2], al);
        else ui_fill(cx - w / 2, cy + lo, w, hi - lo, col[0], col[1], col[2], al);
    }
    float tip = (len / 2 - hd / 2) * (k < 0 ? -1 : 1);
    double deg = across ? (k < 0 ? 180 : 0) : (k < 0 ? 270 : 90);
    SDL_SetTextureAlphaMod(t_head, al);
    ui_put_rot(t_head, across ? cx + tip : cx, across ? cy : cy + tip, hd, hd, deg);
    SDL_SetTextureAlphaMod(t_head, 255);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int n = G.n;
    float q = grid.cell, g = fmaxf(1, floorf(GAP * q)), T = q - 2 * g;
    Uint32 now = ui_ms();
    ui_put(t_board, grid.x - fr, grid.y - fr, n * q + 2 * fr, n * q + 2 * fr);
    float off;
    int line = moving_line(&off);
    float wt = win.on ? (float)(now - win.t0) : -1;
    if (hint.move >= 0) {                       /* the hinted line in gold light, pulsing */
        float p = 0.6f + 0.4f * puz_pulse((float)((now - hint.t0) % HINT_MS), 0, HINT_MS);
        int hl = lsh_line(hint.move);
        for (int i = 0; i < n; i++) {
            int x = hl < n ? i : hl - n, y = hl < n ? hl : i;
            ui_mark_gold_a(grid.x + (x + 0.5f) * q, grid.y + (y + 0.5f) * q, 1.35f * q, p);
        }
        ui_animate();
    }
    SDL_Rect clip = {(int)grid.x, (int)grid.y, (int)(n * q), (int)(n * q)};
    for (int pass = 0; pass < 2; pass++)        /* the still tiles, then the moving line (clipped to the floor) */
        for (int p = 0; p < n * n; p++) {
            int x = p % n, y = p / n, in = line >= 0 && (line < n ? y == line : x == line - n);
            if (in != pass) continue;
            float fx = (float)x, fy = (float)y;
            if (in) {
                float *v = line < n ? &fx : &fy;
                *v = fmodf(*v + off, (float)n);
                if (*v < 0) *v += n;
            }
            if (in) SDL_RenderSetClipRect(ui.r, &clip);
            for (int wrap = 0; wrap < (in ? 2 : 1); wrap++) {
                float wx = fx, wy = fy;
                if (wrap) {                     /* its part come round from the far end */
                    float *v = line < n ? &wx : &wy;
                    if (*v <= n - 1) continue;
                    *v -= n;
                }
                float px = grid.x + wx * q + g, py = grid.y + wy * q + g;
                if (wt >= 0) {
                    float k = puz_pulse(wt, G.at[p] * (float)WAVE_STEP, WAVE_LEN);
                    if (k > 0) ui_mark_gold(px + T / 2, py + T / 2, 1.8f * T * k);
                }
                tile_at(G.at[p], px, py, T);
                if (wt >= 0) {
                    float k = puz_pulse(wt, G.at[p] * (float)WAVE_STEP, WAVE_LEN);
                    if (k > 0) ui_mark_gold_a(px + T / 2, py + T / 2, 1.1f * T, k);
                }
                if (p == sel && !in) ui_mark_select(px, py, T, T);
            }
            if (in) SDL_RenderSetClipRect(ui.r, NULL);
        }
    if (hint.move >= 0 && line < 0) draw_arrow(hint.move, 1);
    if (line >= 0 || (wt >= 0 && wt < win_len())) ui_animate();
    ui_tool(B_HINT, ICON_HINT, lsh_solved(&G) ? UI_DIM : hint.move >= 0 ? UI_ON : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void use_hint(void)
{
    if (lsh_solved(&G)) { ui_sound(SND_DENY); return; }
    int m = lsh_hint(&G);
    if (m < 0) { ui_sound(SND_DENY); return; }
    hint.move = m;
    hint.t0 = ui_ms();
    sel = -1;
    ui_sound(SND_TAP);
    ui_animate();
}

static int cell_at(float x, float y, int *cx, int *cy) { return ui_grid_hit(&grid, x, y, cx, cy); }

static void press(float x, float y)
{
    memset(&drag, 0, sizeof drag);
    if (ui_tool_hit(x, y) == B_HINT) { use_hint(); return; }
    if (lsh_solved(&G)) return;
    int cx, cy;
    if (!cell_at(x, y, &cx, &cy)) {
        if (sel >= 0) { sel = -1; ui_animate(); }
        return;
    }
    anim.on = 0;                                /* a glide still going: done at once */
    drag.on = 1;
    drag.cx = cx;
    drag.cy = cy;
    drag.x0 = x;
    drag.y0 = y;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    float q = grid.cell, dx = x - drag.x0, dy = y - drag.y0;
    if (!drag.axis && hypotf(dx, dy) > 0.15f * q) {
        drag.axis = fabsf(dx) >= fabsf(dy) ? 1 : 2;
        sel = -1;
    }
    if (drag.axis) drag.off = (drag.axis == 1 ? dx : dy) / q;
    ui_animate();
}

static void tap(int cx, int cy)
{
    int n = G.n, p = cx + cy * n;
    if (sel < 0 || sel == p) {                  /* picked, or put back */
        sel = sel == p ? -1 : p;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    int sx = sel % n, sy = sel / n;
    if (sx != cx && sy != cy) {                 /* another line: that tile picked instead */
        sel = p;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    int row = sy == cy, d = row ? cx - sx : cy - sy;
    play(lsh_move(row ? cy : n + cx, ((d % n) + n) % n), (float)-d);
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    ui_animate();
    int n = G.n;
    if (!drag.axis) {
        int cx, cy;
        if (cell_at(x, y, &cx, &cy) && cx == drag.cx && cy == drag.cy) tap(cx, cy);
        return;
    }
    int line = drag.axis == 1 ? drag.cy : n + drag.cx, k = (int)lroundf(drag.off), amount = ((k % n) + n) % n;
    float rest = drag.off - (float)k;
    if (amount) play(lsh_move(line, amount), rest);
    else glide(line, rest, SNAP_MS);
}

/* ---- the bar: the moves; the par at the right -------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    int m = lsh_moves(&G);
    pzt_bar_two(b, NULL, m, t_par, G.par, lsh_solved(&G) && m <= G.par);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int n = variant < 0 ? 4 : LSH_MIN + variant % 4, colour = variant < 0 || variant >= 4;
    int8_t at[LSH_CELLS];
    for (int p = 0; p < n * n; p++) at[p] = (int8_t)p;
    lsh_apply(at, n, lsh_move(1, 1));           /* a row and a column slid: the bands broken */
    lsh_apply(at, n, lsh_move(n + n - 2, n - 1));
    paint_grid(at, n, colour, size, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const int SIDE[LSH_CHAPTERS] = {3, 3, 4, 4, 5, 6};
    int n = SIDE[chapter < 0 ? 0 : chapter % LSH_CHAPTERS];
    if (chapter == 0 || chapter == 2) {         /* the colour bands come first there */
        int8_t at[LSH_CELLS];
        for (int p = 0; p < n * n; p++) at[p] = (int8_t)p;
        paint_grid(at, n, 1, size, rgba);
    } else {
        pzk_grid_icon(rgba, size, n);
    }
}

static const game loopshift_game = {
    .id = "loopshift", .order = 442, .variants = LSH_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .score = score,
    .levels = LSH_LEVELS, .chapters = LSH_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_stars = level_stars, .par = par, .fill = 1, .tools = BTNS,
};
REGISTER_GAME(loopshift_game)
