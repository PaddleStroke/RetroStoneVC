/*
 * Magic Squares in RetroStone Board, for one player: a figure inlaid in the house plank, a square of cells or a
 * triangle of round places along grooved sides; the numbers given burned into maple, the missing ones as hollows, the
 * target on a brass plate (above the square, in the triangle's middle); the missing numbers wait as wooden tiles (round
 * for the triangles) in a tray beside the board (the rules: magicsquare_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag a tile from the tray to a hollow (a tile already there goes back), from a hollow to another (they swap) or off
 * the board (back to the tray); or tap a tile, then a hollow (or the other way); a tap on a placed tile sends it back.
 * A line filled and right lights gold (a triangle's grooves, a pointer out of a square's row, column or diagonal);
 * filled and wrong, red (a mistake, for the stars). The
 * shell's tool: the hint (a wrong tile put right, else a hollow on the line nearest done). The keys: the arrows move a
 * frame over the hollows and the tray, Return taps what it frames. Undo takes back one action, all the way to the
 * start. Solved: gold light runs round the figure; the stars count the hints and the mistakes (ui_logic_stars).
 * The shell's level screen chooses a level (150 in six chapters: magicsquare_levels.h), or its die tab a puzzle made
 * at random (four figures).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/magicsquare.h"
#include "games/magicsquare_levels.h"
#include "games/mathsigns.h"
#include "shell.h"

enum { T_HINT, TOOLS };
#define FLY_MS 200
#define WIN_MS 900
#define PITCH 1.1f

static const float TONE_GIVEN[3] = {1.12f, 1.10f, 1.04f}, TONE_TRAY[3] = {0.62f, 0.48f, 0.38f};
static const uint8_t BRASS[3] = {206, 160, 74}, BRASS_DARK[3] = {104, 72, 26};

static msq_game G;
static int serial;

static struct {
    box room;
    float B, bx, by;                            /* the board: its side, its corner */
    float px[MSQ_PLACES], py[MSQ_PLACES], q;    /* each place's centre on the screen; a place's size */
    float tx, ty, tw, th, ts;                   /* the tray; a tile's size */
    float hx[MSQ_PLACES], hy[MSQ_PLACES];       /* each tile's home in the tray */
    float plate_x, plate_y, plate_w;            /* the target's brass plate (its centre, its width) */
    int made;
} L;
static SDL_Texture *t_board, *t_tray, *t_tile, *t_small;
static int made_small = -1;

static int sel_tile = -1, sel_gap = -1, cur = -1;
static struct { int on, tile, gap, moved; float x0, y0, x, y; } drag;
typedef struct { float x, y; Uint32 t0; int on; } flight;
static flight fly[MSQ_PLACES];
static bench_flash hint;
static struct { int on; Uint32 t0; } win;

static int square(void) { return G.p.kind == MSQ_SQ3 || G.p.kind == MSQ_SQ4; }
static float digits_h(int v, float q) { return v >= 100 ? 0.36f * q : v >= 10 ? 0.46f * q : 0.54f * q; }

/* ---- the figure's places, in parts of the board's side ------------------------------------------------------------ */
typedef struct { float x[MSQ_PLACES], y[MSQ_PLACES], q, plate_x, plate_y, plate_w; } figure;

static void figure_of(const msq_puzzle *p, figure *f)
{
    if (p->kind == MSQ_SQ3 || p->kind == MSQ_SQ4) {
        int n = p->kind == MSQ_SQ3 ? 3 : 4;
        float band = 0.16f, side = 0.66f, x0 = (1 - side) / 2, y0 = band + 0.025f;  /* room below for the pointers */
        f->q = side / n;
        for (int i = 0; i < p->np; i++) f->x[i] = x0 + (i % n + 0.5f) * f->q, f->y[i] = y0 + (i / n + 0.5f) * f->q;
        f->plate_x = 0.5f, f->plate_y = 0.095f, f->plate_w = 0.26f;
        return;
    }
    int k = p->kind == MSQ_TRI3 ? 3 : p->kind == MSQ_TRI4 ? 4 : 5, in = k - 2;
    const float cx[3] = {0.5f, 0.13f, 0.87f}, cy[3] = {0.14f, 0.8f, 0.8f};
    for (int c = 0; c < 3; c++) f->x[c] = cx[c], f->y[c] = cy[c];
    for (int s = 0; s < 3; s++)
        for (int j = 0; j < in; j++) {
            float t = (j + 1.0f) / (k - 1);
            f->x[3 + s * in + j] = cx[s] + (cx[(s + 1) % 3] - cx[s]) * t;
            f->y[3 + s * in + j] = cy[s] + (cy[(s + 1) % 3] - cy[s]) * t;
        }
    f->q = fminf(0.17f, 0.74f * 0.78f / (k - 1));
    f->plate_x = 0.5f, f->plate_y = 0.6f, f->plate_w = 0.24f;
}

/* ---- painting ---------------------------------------------------------------------------------------------------- */
static void paint_plate(uint8_t *o, int w, int h, float cx, float cy, float pw, int target)
{
    float ph = 0.55f * pw;
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, 0, 0, 1, 0.55f);
    art_style st = art_flat(BRASS, BRASS_DARK, 0.03f, 0.45f);
    art_fig_blend(o, w, h, f, &st, (int)(cx - pw / 2), (int)(cy - ph / 2), (int)pw, 1);
    art_fig_free(f);
    puz_digits(o, w, h, target, cx, cy + 0.02f * ph, (target >= 100 ? 0.5f : 0.62f) * ph, BRASS_DARK, 1, 1);
}

/* the board (S x S), a share of the gaps shown filled (the menu's picture) */
static void paint_board(uint8_t *o, int S, const msq_puzzle *p, float filled)
{
    int fr = (int)fmaxf(2, 0.03f * S);
    art_plank(S - 2 * fr, S - 2 * fr, fr, o);
    figure f;
    figure_of(p, &f);
    float q = f.q * S, in = fmaxf(1, 0.05f * q), seam = fmaxf(1, 0.05f * q);
    if (!(p->kind == MSQ_SQ3 || p->kind == MSQ_SQ4))   /* the triangle's grooves */
        for (int s = 0; s < 3; s++) {
            for (int j = 0; j + 1 < p->len[s]; j++) {   /* a groove between each two places */
                int a = p->line[s][j], b = p->line[s][j + 1];
                float dx = (f.x[b] - f.x[a]) * S, dy = (f.y[b] - f.y[a]) * S, l = hypotf(dx, dy), r = 0.52f * q / l;
                if (r < 0.5f)
                    pzk_stroke(o, S, S, f.x[a] * S + dx * r, f.y[a] * S + dy * r, f.x[b] * S - dx * r,
                               f.y[b] * S - dy * r, fmaxf(1.5f, 0.018f * S), PUZ_INK, 0.35f);
            }
        }
    int gaps = 0, k = 0;
    for (int i = 0; i < p->np; i++) gaps += !p->given[i];
    for (int i = 0; i < p->np; i++) {
        float x = f.x[i] * S - q / 2 + in, y = f.y[i] * S - q / 2 + in, w = q - 2 * in, cx = f.x[i] * S, cy = f.y[i] * S;
        float rad = p->kind == MSQ_SQ3 || p->kind == MSQ_SQ4 ? 0.12f * q : w / 2;
        if (!p->given[i] && k++ >= (int)(filled * gaps + 0.5f)) {   /* a gap: a hollow, round on a triangle */
            static const float dark[3] = {0.7f, 0.66f, 0.62f};
            if (rad >= w / 2 - 0.5f) art_inlay(o, S, S, x, y, w, w, rad, dark, seam * 2);
            else msg_hollow(o, S, S, x, y, w, w);
            continue;
        }
        art_inlay(o, S, S, x, y, w, w, rad, p->given[i] ? TONE_GIVEN : BENCH_TONE[1], seam);
        if (p->given[i]) puz_burn(o, S, S, p->val[i], cx, cy + 0.02f * q, digits_h(p->val[i], q));
        else puz_digits(o, S, S, p->val[i], cx, cy + 0.02f * q, digits_h(p->val[i], q), PUZ_INK, 1, 1);
    }
    paint_plate(o, S, S, f.plate_x * S, f.plate_y * S, f.plate_w * S, p->target);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    msq_puzzle p;
    msq_decode("s:15:2,7,6,9,5,1,4,3,8", &p);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    int fr = (int)fmaxf(1, 0.05f * size);
    art_plank(size - 2 * fr, size - 2 * fr, fr, b);
    float q = (size - 2 * fr) / 3.0f;
    for (int i = 0; i < 9; i++)
        if (q >= 6) puz_digits(b, size, size, p.val[i], fr + (i % 3 + 0.5f) * q, fr + (i / 3 + 0.54f) * q, 0.7f * q,
                               i == 4 ? BENCH_BLUE : PUZ_INK, 1, 1);
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
}

/* ---- the layout: the board and the tray -------------------------------------------------------------------------- */
static void layout(box b)
{
    L.room = b;
    if (!G.p.np) return;
    figure f;
    figure_of(&G.p, &f);
    int n = G.ntray > 0 ? G.ntray : 1;
    float best = 0, cf = f.q * 0.95f, gap = 0.04f;
    int bc = n, side = 0, brows = 99;
    for (int s = 0; s < 2; s++)
        for (int c = 1; c <= n; c++) {
            int r = (n + c - 1) / c;
            float tw = (c * PITCH + 0.4f) * cf, th = (r * PITCH + 0.4f) * cf, B;
            if (s) B = fminf(b.w / (1 + gap + tw), b.h / fmaxf(1, th));
            else B = fminf(b.w / fmaxf(1, tw), b.h / (1 + gap + th));
            if (B > best * 1.02f || (B > best * 0.98f && r < brows)) best = B, bc = c, side = s, brows = r;
        }
    float B = floorf(best), ts = cf * B;
    int r = (n + bc - 1) / bc;
    L.B = B;
    L.ts = ts;
    L.q = f.q * B;
    L.tw = floorf((bc * PITCH + 0.4f) * ts);
    L.th = floorf((r * PITCH + 0.4f) * ts);
    if (side) {
        float all = B + gap * B + L.tw;
        L.bx = floorf(b.x + (b.w - all) / 2), L.by = floorf(b.y + (b.h - B) / 2);
        L.tx = L.bx + B + gap * B, L.ty = floorf(b.y + (b.h - L.th) / 2);
    } else {
        float all = B + gap * B + L.th;
        L.bx = floorf(b.x + (b.w - B) / 2), L.by = floorf(b.y + (b.h - all) / 2);
        L.tx = floorf(b.x + (b.w - L.tw) / 2), L.ty = L.by + B + gap * B;
    }
    for (int i = 0; i < G.p.np; i++) L.px[i] = L.bx + f.x[i] * B, L.py[i] = L.by + f.y[i] * B;
    for (int t = 0; t < G.ntray; t++) {
        int rr = t / bc, in_row = rr == r - 1 ? G.ntray - rr * bc : bc;
        L.hx[t] = L.tx + (0.2f + PITCH * (t % bc + 0.5f + (bc - in_row) / 2.0f)) * ts;
        L.hy[t] = L.ty + (0.2f + PITCH * (rr + 0.5f)) * ts;
    }
    int key = ((int)B * 64 + (int)L.tw % 64) * 4096 + serial % 4096;
    if (L.made != key || !t_board) {
        L.made = key;
        int S = (int)B;
        uint8_t *o = ui_buf(S, S);
        paint_board(o, S, &G.p, 0);
        ui_free(&t_board);
        t_board = ui_tex(S, S, o);
        int tw = (int)L.tw, th = (int)L.th, fr = (int)fmaxf(2, 0.1f * ts);
        o = ui_buf(tw, th);
        art_plank(tw - 2 * fr, th - 2 * fr, fr, o);
        for (int i = 0; i < tw * th; i++)
            for (int k = 0; k < 3; k++) o[4 * i + k] = (uint8_t)(o[4 * i + k] * TONE_TRAY[k] + 0.5f);
        static const float dark[3] = {0.7f, 0.66f, 0.62f};
        for (int t = 0; t < G.ntray; t++) {     /* the tiles' hollows: round for the triangles */
            float hx = L.hx[t] - L.tx - 0.46f * ts, hy = L.hy[t] - L.ty - 0.46f * ts;
            if (square()) msg_hollow(o, tw, th, hx, hy, 0.92f * ts, 0.92f * ts);
            else art_inlay(o, tw, th, hx, hy, 0.92f * ts, 0.92f * ts, 0.46f * ts, dark, fmaxf(2, 0.1f * ts));
        }
        ui_free(&t_tray);
        t_tray = ui_tex(tw, th, o);
        int s = (int)(0.9f * ts);
        o = ui_buf(s, s);
        bench_tile(o, s, s, square() ? 0.16f * s : 0.5f * s - 0.5f, BENCH_TONE[1]);   /* round for the triangles */
        ui_free(&t_tile);
        t_tile = ui_tex(s, s, o);
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- where things are -------------------------------------------------------------------------------------------- */
static void tile_pos(int t, float *x, float *y)
{
    int i = msq_holder(&G, t);
    if (i >= 0) *x = L.px[G.gaps[i]], *y = L.py[G.gaps[i]];
    else *x = L.hx[t], *y = L.hy[t];
}

static int gap_at(float x, float y)
{
    for (int i = 0; i < G.ngap; i++)
        if (fabsf(x - L.px[G.gaps[i]]) <= 0.55f * L.q && fabsf(y - L.py[G.gaps[i]]) <= 0.55f * L.q) return i;
    return -1;
}

static int tray_at(float x, float y)
{
    for (int t = 0; t < G.ntray; t++)
        if (msq_tile_free(&G, t) && fabsf(x - L.hx[t]) <= PITCH * L.ts / 2 && fabsf(y - L.hy[t]) <= PITCH * L.ts / 2)
            return t;
    return -1;
}

static int in_tray(float x, float y) { return x >= L.tx && y >= L.ty && x < L.tx + L.tw && y < L.ty + L.th; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(&drag, 0, sizeof drag);
    memset(fly, 0, sizeof fly);
    sel_tile = sel_gap = -1;
    hint.at = -1;
    win.on = 0;
}

static void begin(void)
{
    serial++;
    calm();
    cur = -1;
    if (L.room.w > 0) layout(L.room);
}

static void start(int level)
{
    msq_puzzle p;
    if (msq_decode(msq_levels[level < 0 || level >= MSQ_LEVELS ? 0 : level], &p)) msq_load(&G, &p);
    else msq_new(&G, 0, 1);
    begin();
}

static void start_random(int variant)
{
    msq_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return msq_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int flying(void)
{
    for (int t = 0; t < G.ntray; t++)
        if (fly[t].on && ui_ms() - fly[t].t0 < FLY_MS) return 1;
    return 0;
}
static int busy(void) { return flying() || (win.on && ui_ms() - win.t0 < WIN_MS); }
static int score(void) { return G.moves; }
static int stars(void) { return ui_logic_stars(G.hints, G.mistakes); }

typedef struct { float x[MSQ_PLACES], y[MSQ_PLACES]; } spots;

static void spots_now(spots *s)
{
    for (int t = 0; t < G.ntray; t++) tile_pos(t, &s->x[t], &s->y[t]);
}

static void flights(const spots *was, int carried)
{
    for (int t = 0; t < G.ntray; t++) {
        float x, y;
        tile_pos(t, &x, &y);
        if (t != carried && (fabsf(x - was->x[t]) > 1 || fabsf(y - was->y[t]) > 1))
            fly[t] = (flight){was->x[t], was->y[t], ui_ms(), 1};
    }
}

static void after(const spots *was, int carried, int mistakes)
{
    flights(was, carried);
    sel_tile = sel_gap = -1;
    ui_sound(G.mistakes > mistakes ? SND_DENY : SND_PLACE);
    if (msq_solved(&G) && !win.on) {
        win.on = 1;
        win.t0 = ui_ms();
        cur = -1;
    }
    ui_animate();
}

enum { A_PUT, A_TAKE, A_MOVE };
static int act(int what, int i, int j, int carried)
{
    spots was;
    spots_now(&was);
    int m = G.mistakes, ok = what == A_PUT ? msq_put(&G, i, j) : what == A_TAKE ? msq_take(&G, i) : msq_move(&G, i, j);
    if (ok) after(&was, carried, m);
    else ui_sound(SND_DENY);
    return ok;
}

static int undo(void)
{
    spots was;
    spots_now(&was);
    if (!msq_undo(&G)) return 0;
    int keep = cur;
    calm();
    cur = keep;
    flights(&was, -1);
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = msq_script(&G, moves);
    if (msq_solved(&G)) win.on = 1, win.t0 = ui_ms();
    return ok;
}

static void do_hint(void)
{
    if (msq_solved(&G)) return;
    spots was;
    spots_now(&was);
    int m = G.mistakes, i = msq_hint(&G);
    if (i < 0) { ui_sound(SND_DENY); return; }
    bench_flash_start(&hint, G.gaps[i]);
    after(&was, -1, m);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void tile_at(int t, float x, float y, float s)
{
    float d = 0.9f * L.ts * s;
    ui_shadow_flat(x, y, d, d, 0.7f);
    ui_put_c(t_tile, x, y, d, d);
    int v = G.tray[t];
    ui_number_ex(x, y + 0.02f * d, digits_h(v, L.ts) * s, v, PUZ_INK, 255, UI_NUM_TIGHT);
}

static void cur_xy(int f, float *x, float *y)
{
    if (f < G.ngap) *x = L.px[G.gaps[f]], *y = L.py[G.gaps[f]];
    else *x = L.hx[f - G.ngap], *y = L.hy[f - G.ngap];
}

static void draw(void)
{
    if (!t_board) layout(L.room);
    if (!t_board) return;
    int solved = msq_solved(&G);
    float q = L.q;
    ui_put(t_board, L.bx, L.by, L.B, L.B);
    ui_put(t_tray, L.tx, L.ty, L.tw, L.th);
    for (int l = 0; l < G.p.nl; l++) {          /* the lines filled: right (gold), wrong (red) */
        int st = msq_line_state(&G, l);
        if (!st || solved) continue;
        pzk_rgba c = st > 0 ? (pzk_rgba){236, 194, 96, 255} : (pzk_rgba){214, 60, 44, 235};
        if (!square()) {                        /* a triangle's side: its grooves between the places lit */
            for (int j = 0; j + 1 < G.p.len[l]; j++) {
                int a = G.p.line[l][j], b = G.p.line[l][j + 1];
                float dx = L.px[b] - L.px[a], dy = L.py[b] - L.py[a], len = hypotf(dx, dy) + 1e-3f, r = 0.5f * q / len;
                if (r < 0.5f)
                    pzk_line(L.px[a] + dx * r, L.py[a] + dy * r, L.px[b] - dx * r, L.py[b] - dy * r, 0.14f * q,
                             0.05f * q, c, 0);
            }
            continue;
        }
        int a = G.p.line[l][0], z = G.p.line[l][G.p.len[l] - 1];   /* a square's line: a pointer out of its end */
        float dx = L.px[z] - L.px[a], dy = L.py[z] - L.py[a], len = hypotf(dx, dy) + 1e-3f;
        dx /= len, dy /= len;
        float edge = 0.5f * q / fmaxf(fabsf(dx), fabsf(dy)), x0 = L.px[z] + dx * (edge + 0.06f * q),
              y0 = L.py[z] + dy * (edge + 0.06f * q), x1 = x0 + dx * 0.34f * q, y1 = y0 + dy * 0.34f * q;
        pzk_line(x0, y0, x1, y1, 0.13f * q, 0.04f * q, c, 0);
        pzk_dot(x1, y1, 0.2f * q, 0.04f * q, c, 0);
    }
    if (sel_gap >= 0) {
        int pl = G.gaps[sel_gap];
        ui_mark_select(L.px[pl] - q / 2, L.py[pl] - q / 2, q, q);
    }
    if (hint.at >= 0) bench_glow(&hint, L.px[hint.at], L.py[hint.at], 2.2f * q);
    Uint32 now = ui_ms();
    for (int t = 0; t < G.ntray; t++) {
        if (drag.on && drag.moved && drag.tile == t) continue;
        float x, y;
        tile_pos(t, &x, &y);
        if (fly[t].on) {
            float f = (now - fly[t].t0) / (float)FLY_MS;
            if (f >= 1) fly[t].on = 0;
            else {
                f = ui_ease(f);
                x = fly[t].x + (x - fly[t].x) * f, y = fly[t].y + (y - fly[t].y) * f;
                ui_animate();
            }
        }
        if (t == sel_tile) ui_mark_gold(x, y, 1.6f * L.ts);
        tile_at(t, x, y, t == sel_tile ? 1.08f : 1);
    }
    if (win.on) {                               /* solved: gold light round the figure, place by place */
        for (int i = 0; i < G.p.np; i++) {
            float p = puz_pulse((float)(now - win.t0), i * 40.0f, 500);
            if (p > 0) ui_mark_gold(L.px[i], L.py[i], 1.8f * q * p);
        }
        if (now - win.t0 < WIN_MS) ui_animate();
    }
    if (cur >= 0 && cur < G.ngap + G.ntray && !solved) {
        float x, y, s = cur < G.ngap ? q : L.ts;
        cur_xy(cur, &x, &y);
        ui_mark_select(x - 0.55f * s, y - 0.55f * s, 1.1f * s, 1.1f * s);
    }
    if (drag.on && drag.moved) {
        float x = drag.x, y = drag.y - 0.55f * L.ts;
        int i = gap_at(x, y);
        if (i >= 0) ui_mark_select(L.px[G.gaps[i]] - q / 2, L.py[G.gaps[i]] - q / 2, q, q);
        tile_at(drag.tile, x, y, 1.12f);
    }
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- the finger -------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&drag, 0, sizeof drag);
    cur = -1;
    if (msq_solved(&G)) return;
    if (ui_tool_hit(x, y) == T_HINT) { do_hint(); return; }
    int i = gap_at(x, y), t = tray_at(x, y);
    if (i >= 0) t = G.at[i] >= 0 ? G.at[i] : -1;
    drag.on = 1;
    drag.tile = t;
    drag.gap = i;
    drag.x = drag.x0 = x;
    drag.y = drag.y0 = y;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (drag.tile >= 0 && hypotf(x - drag.x0, y - drag.y0) > 0.25f * L.ts) drag.moved = 1;
    ui_animate();
}

static void tap(int t, int i)
{
    if (t >= 0 && i < 0) {
        if (sel_gap >= 0) { act(A_PUT, sel_gap, t, -1); return; }
        sel_tile = sel_tile == t ? -1 : t;
        ui_sound(SND_TAP);
    } else if (i >= 0) {
        if (sel_tile >= 0) { act(A_PUT, i, sel_tile, -1); return; }
        if (G.at[i] >= 0) { act(A_TAKE, i, 0, -1); return; }
        sel_gap = sel_gap == i ? -1 : i;
        ui_sound(SND_TAP);
    } else {
        sel_tile = sel_gap = -1;
    }
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    ui_animate();
    if (!drag.moved) {
        tap(drag.gap >= 0 ? -1 : drag.tile, drag.gap);
        return;
    }
    int t = drag.tile, to = gap_at(x, y - 0.55f * L.ts);
    if (to < 0) to = gap_at(x, y);
    if (drag.gap >= 0) {
        if (to == drag.gap) return;
        if (to >= 0) act(A_MOVE, drag.gap, to, t);
        else act(A_TAKE, drag.gap, 0, t);
        return;
    }
    if (to >= 0) act(A_PUT, to, t, t);
    else if (!in_tray(x, y)) ui_sound(SND_DENY);
}

static void key(SDL_Keycode k)
{
    int n = G.ngap + G.ntray;
    if (msq_solved(&G) || !n) return;
    int dx = (k == SDLK_RIGHT) - (k == SDLK_LEFT), dy = (k == SDLK_DOWN) - (k == SDLK_UP);
    if (dx || dy) {
        if (cur < 0) cur = 0;
        else {
            float x0, y0, best = 1e9f;
            int to = -1;
            cur_xy(cur, &x0, &y0);
            for (int f = 0; f < n; f++) {
                if (f == cur || (f >= G.ngap && !msq_tile_free(&G, f - G.ngap))) continue;
                float x, y;
                cur_xy(f, &x, &y);
                float along = (x - x0) * dx + (y - y0) * dy, across = fabsf((x - x0) * dy) + fabsf((y - y0) * dx);
                if (along < 0.3f * L.ts) continue;
                if (along + 2.5f * across < best) best = along + 2.5f * across, to = f;
            }
            if (to < 0) { ui_sound(SND_DENY); return; }
            cur = to;
        }
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (k != SDLK_RETURN && k != SDLK_KP_ENTER && k != SDLK_SPACE) return;
    if (cur < 0) { cur = 0; ui_animate(); return; }
    if (cur < G.ngap) tap(-1, cur);
    else if (msq_tile_free(&G, cur - G.ngap)) {
        tap(cur - G.ngap, -1);
        for (int i = 0; i < G.ngap && sel_tile >= 0; i++)
            if (G.at[i] < 0) { cur = i; break; }
    }
}

static void bar(int player, box b)
{
    (void)player;
    bench_bar_moves(b, t_small, G.moves);
}

/* ---- the pictures for the shell ---------------------------------------------------------------------------------- */
static void paint_card(const msq_puzzle *p, int size, int side, float filled, int top, uint8_t *rgba)
{
    uint8_t *o = malloc((size_t)side * side * 4);
    if (!o) return;
    paint_board(o, side, p, filled);
    art_blend(rgba, size, size, o, side, side, (size - side) / 2, top ? 0 : (size - side) / 2);
    free(o);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    msq_puzzle p;
    if (variant < 0) {                          /* the menu's: a 3 x 3 half done */
        if (msq_decode(msq_levels[30], &p)) paint_card(&p, size, size, 0.5f, 0, rgba);
        return;
    }
    msq_make m = msq_variant_make(variant);
    msq_make_puzzle(&m, 2026u + (uint32_t)variant, &p);
    paint_card(&p, size, (int)(0.84f * size), 0, 1, rgba);
}

/* a chapter's tab: its first level's figure */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    msq_puzzle p;
    int ch = chapter < 0 ? 0 : chapter >= MSQ_CHAPTERS ? MSQ_CHAPTERS - 1 : chapter;
    if (msq_decode(msq_levels[ch * MSQ_LEVELS / MSQ_CHAPTERS], &p)) paint_card(&p, size, (int)(0.94f * size), 1, 0, rgba);
}

static const game magicsquare_game = {
    .id = "magicsquare", .order = 494, .variants = MSQ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = MSQ_LEVELS, .chapters = MSQ_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .score = score, .level_stars = stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(magicsquare_game)
