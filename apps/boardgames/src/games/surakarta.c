/*
 * Surakarta in RetroStone Board: the 6 x 6 board of lines on one plank (lines.h), its two circuits in colour (the
 * inner one madder red, the outer one bottle green) with their eight loops round the corners, the discs, tap or drag;
 * a capture runs round the loops to the piece it takes, a trail of light behind it (the rules and the notation:
 * surakarta_rules.c, surakarta.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>

#include "games/lines.h"
#include "games/surakarta.h"
#include "shell.h"

#define CELLS 10                                /* the plank: the 6 x 6 points, two cells round them for the loops */
#define AT 2                                    /* the cell of the point a1 */
#define DISC 0.68f                              /* a disc's size, in cells */
#define STEP_MS 160
#define RUN_MS0 300                             /* a capture's run: so long, and so long a cell of its way */
#define RUN_MS_CELL 55
#define RUN_MS_MAX 2200
#define FADE_MS 260                             /* the piece taken, lifted away */
#define TRAIL 2.4f                              /* the trail of light behind a run, in cells */
#define PI_F 3.14159265f

static skgame G;
static skmove legal[SK_MAX_MOVES];
static int nlegal;
static lines_geo L = {.cells = CELLS};
static int made_small = -1;
static SDL_Texture *t_disc[2], *t_small[2], *t_board, *t_glow;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, who, run_ms; skmove m; Uint32 t0; } anim;

static const uint8_t CIRCUIT[2][3] = {{164, 48, 36}, {38, 98, 72}};   /* inner: madder red; outer: bottle green */

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int status(void)
{
    switch (sk_winner(&G)) {
    case -1: return GS_PLAY;
    case 0: return GS_WIN0;
    case 1: return GS_WIN1;
    default: return GS_DRAW;
    }
}

static void refresh(void)
{
    nlegal = sk_legal(sk_pos(&G), legal);
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    sk_new(&G);
    anim.on = 0;
    refresh();
}

static int turn(void) { return sk_pos(&G)->side; }

static int find(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static int can_move(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from) return 1;
    return 0;
}

/* a step slides; a capture runs its way round the loops (always, even dropped there: the run is the game's joy) */
static void play(skmove m, int animate)
{
    int run = m.circuit >= 0;
    anim.who = sk_pos(&G)->side;
    if (!sk_play(&G, m)) return;
    anim.on = animate || run;
    anim.m = m;
    anim.t0 = ui_ms();
    anim.run_ms = run ? (int)fminf(RUN_MS0 + RUN_MS_CELL * sk_length(m), RUN_MS_MAX) : STEP_MS;
    refresh();
    if (run && anim.on) {
        ui_sound(SND_MOVE);
        ui_sound_later(SND_CAPTURE, anim.run_ms);
    } else {
        ui_sound(run ? SND_CAPTURE : SND_MOVE);
    }
}

static int undo(void)
{
    if (!sk_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "b2-b3 e5-e4": the screenshots' and the tests' moves (surakarta.h) */
static int script(const char *s)
{
    int ok = sk_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the bot (surakarta_bot.c) ----------------------------------------------------------------------------------- */
const skgame *surakarta_live(void) { return &G; }

/* "b2-c3", "b2xe5" (or "b2c3"): one move, played as a tap plays it (a capture runs round its loops); 0: not legal */
int surakarta_show(const char *move)
{
    int a = sk_point(move), sep = a >= 0 && (move[2] == '-' || move[2] == 'x');
    int b = a >= 0 ? sk_point(move + 2 + sep) : -1;
    int k = b >= 0 && !move[4 + sep] ? find(a, b) : -1;
    if (k < 0 || status() != GS_PLAY) return 0;
    play(legal[k], 1);
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank: the board's edge lines in ink, the circuits in colour with their loops, the 36 points; cells of s pixels
 * in a frame of f; (10 s + 2 f) squared */
static void paint_board(int s, int f, uint8_t *o)
{
    const int8_t edges[4][4] = {{AT, AT, AT + 5, AT}, {AT, AT + 5, AT + 5, AT + 5}, {AT, AT, AT, AT + 5},
                                {AT + 5, AT, AT + 5, AT + 5}};
    float S = (float)s, F = (float)f, lw = fmaxf(2.0f, 0.05f * s), cw = fmaxf(2.4f, 0.075f * s);
    int n = CELLS * s + 2 * f;
    lines_paint(CELLS, s, f, edges, 4, NULL, 0, lw, 0, o);
    for (int c = 1; c >= 0; c--) {              /* the outer circuit first: the inner one crosses over it */
        for (int k = c + 1; k <= 4 - c; k += 3 - 2 * c) {
            float a0 = lines_px(AT, S, F), a1 = lines_px(AT + 5, S, F), x = lines_px(AT + k, S, F);
            float b0 = lines_py(CELLS, AT, S, F), b1 = lines_py(CELLS, AT + 5, S, F), y = lines_py(CELLS, AT + k, S, F);
            lines_band(o, n, x, b0, x, b1, cw, CIRCUIT[c], 1);
            lines_band(o, n, a0, y, a1, y, cw, CIRCUIT[c], 1);
        }
        for (int i = 5; i < SK_TRACK; i += 6) {
            int k = sk_corner(c, i), qx = sk_x(k) ? -1 : 1, qy = sk_y(k) ? 1 : -1;   /* towards the board, y down */
            lines_arc(o, n, lines_px(AT + sk_x(k), S, F), lines_py(CELLS, AT + sk_y(k), S, F), (c + 1) * S, cw, qx,
                      qy, CIRCUIT[c]);
        }
    }
    for (int pt = 0; pt < SK_POINTS; pt++)
        lines_dot(o, n, lines_px(AT + sk_x(pt), S, F), lines_py(CELLS, AT + sk_y(pt), S, F), fmaxf(1.8f, 0.085f * s),
                  LINES_INK);
}

static void layout(box b)
{
    int changed = lines_fit(&L, b), n = (int)lines_side(&L);
    if (changed) {
        ui_free(&t_board);
        paint_board(L.sq, L.fr, ui_buf(n, n));
        t_board = ui_tex(n, n, ui_buf(n, n));
        int d = (int)(DISC * L.sq);
        for (int c = 0; c < 2; c++) {
            ui_free(&t_disc[c]);
            art_counter(COUNTER_DISC, c, d, ui_buf(d, d));
            t_disc[c] = ui_tex(d, d, ui_buf(d, d));
        }
        static const uint8_t GOLD[3] = {255, 222, 150};
        int g = (int)fmaxf(8, 0.42f * L.sq);
        ui_free(&t_glow);
        art_disc(g, GOLD, 1, 0.9f, ui_buf(g, g));
        t_glow = ui_tex(g, g, ui_buf(g, g));
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
/* a place on the board (in points: a1 at 0, 0; between the points and out on the loops too) on the screen */
static void place_xy(float px, float py, float *x, float *y)
{
    float c = AT + px, r = CELLS - 1 - (AT + py);
    if (ui.flipped) { c = CELLS - 1 - c; r = CELLS - 1 - r; }
    *x = L.x + (c + 0.5f) * L.sq;
    *y = L.y + (r + 0.5f) * L.sq;
}

static void pt_xy(int pt, float *x, float *y) { place_xy((float)sk_x(pt), (float)sk_y(pt), x, y); }

static int xy_pt(float x, float y)              /* the point whose cell holds (x, y); -1 */
{
    int gx, gy;
    if (!lines_at(&L, x, y, &gx, &gy)) return -1;
    gx -= AT, gy -= AT;
    return gx >= 0 && gy >= 0 && gx < SK_SIDE && gy < SK_SIDE ? gx + SK_SIDE * gy : -1;
}

/* where a move's piece is, dist cells along its way (in points; a capture's run: straight on the lines, three quarters
 * of a circle round each loop) */
static void way_at(skmove m, float dist, float *px, float *py)
{
    if (m.circuit < 0) {
        float t = fminf(dist / sk_length(m), 1);
        *px = sk_x(m.from) + (sk_x(m.to) - sk_x(m.from)) * t;
        *py = sk_y(m.from) + (sk_y(m.to) - sk_y(m.from)) * t;
        return;
    }
    int j = m.start;
    for (int k = 0; k < m.steps; k++) {
        int next = (j + m.dir + SK_TRACK) % SK_TRACK, a = sk_track[m.circuit][j], b = sk_track[m.circuit][next];
        int loop = sk_loop_after(m.dir > 0 ? j : next);
        float r = (float)(m.circuit + 1), len = loop ? 1.5f * PI_F * r : 1;
        if (dist <= len || k == m.steps - 1) {
            float t = fminf(fmaxf(dist / len, 0), 1);
            if (!loop) {
                *px = sk_x(a) + (sk_x(b) - sk_x(a)) * t;
                *py = sk_y(a) + (sk_y(b) - sk_y(a)) * t;
                return;
            }
            int cn = sk_corner(m.circuit, m.dir > 0 ? j : next);   /* round the corner, away from the board */
            float cx = (float)sk_x(cn), cy = (float)sk_y(cn);
            float a0 = atan2f(sk_y(a) - cy, sk_x(a) - cx), a1 = atan2f(sk_y(b) - cy, sk_x(b) - cx), d = a1 - a0;
            while (d > PI_F) d -= 2 * PI_F;
            while (d <= -PI_F) d += 2 * PI_F;
            float sweep = d > 0 ? d - 2 * PI_F : d + 2 * PI_F, ang = a0 + sweep * t;
            *px = cx + r * cosf(ang);
            *py = cy + r * sinf(ang);
            return;
        }
        dist -= len;
        j = next;
    }
    *px = (float)sk_x(m.to), *py = (float)sk_y(m.to);
}

static void disc_at(int who, float cx, float cy, float d, float alpha) { ui_flat_at(t_disc[who], cx, cy, d, alpha, 0); }

/* a capture's run so far: a glow along the way behind the piece and fading echoes of it (fading out after it has
 * landed) */
static void trail(skmove m, int who, float dist, float fade)
{
    float gs = 0.5f * L.sq, d = DISC * L.sq, x, y, px, py;
    for (float s = fmaxf(0, dist - TRAIL); s <= dist; s += 0.08f) {
        float k = 1 - (dist - s) / TRAIL;       /* 0 at the tail, 1 at the piece */
        way_at(m, s, &px, &py);
        place_xy(px, py, &x, &y);
        SDL_SetTextureAlphaMod(t_glow, (Uint8)(230 * k * fade));
        ui_put_c(t_glow, x, y, gs * (0.6f + 0.4f * k), gs * (0.6f + 0.4f * k));
    }
    SDL_SetTextureAlphaMod(t_glow, 255);
    for (float s = dist - 0.25f; s > dist - TRAIL; s -= 0.25f) {
        float k = 1 - (dist - s) / TRAIL;
        if (s < 0) break;
        way_at(m, s, &px, &py);
        place_xy(px, py, &x, &y);
        SDL_SetTextureAlphaMod(t_disc[who], (Uint8)(110 * k * k * fade));
        ui_put_c(t_disc[who], x, y, d * (0.8f + 0.2f * k), d * (0.8f + 0.2f * k));
    }
    SDL_SetTextureAlphaMod(t_disc[who], 255);
}

static void draw(void)
{
    const skpos *p = sk_pos(&G);
    float n = lines_side(&L), d = DISC * L.sq, x, y;
    ui_put(t_board, L.x - L.fr, L.y - L.fr, n, n);
    if (G.ply) {
        const skmove *m = &G.move[G.ply - 1];
        lines_mark_last_round(&L, AT + sk_x(m->from), AT + sk_y(m->from));
        lines_mark_last_round(&L, AT + sk_x(m->to), AT + sk_y(m->to));
    }
    if (sel >= 0) lines_mark_select_round(&L, AT + sk_x(sel), AT + sk_y(sel));
    int ms = anim.on ? (int)(ui_ms() - anim.t0) : 0, run = anim.on && anim.m.circuit >= 0;
    if (anim.on) {
        if (ms >= anim.run_ms + (run ? FADE_MS * 2 : 0)) anim.on = 0, run = 0;
        else ui_animate();
    }
    int travelling = anim.on && ms < anim.run_ms;
    for (int i = 0; i < SK_POINTS; i++) {
        if (p->at[i] < 0 || (drag.on && drag.moved && i == drag.from) || (anim.on && i == anim.m.to)) continue;
        pt_xy(i, &x, &y);
        disc_at(p->at[i], x, y, d, 1);
    }
    if (status() == GS_PLAY && sel >= 0)        /* where the selected piece can step, the pieces it can take */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            pt_xy(legal[i].to, &x, &y);
            if (legal[i].circuit >= 0) ui_mark_ring(x, y, 0.92f * L.sq);
            else ui_mark_dot(x, y, 0.36f * L.sq);
        }
    if (anim.on) {
        float t = fminf(ms / (float)anim.run_ms, 1), len = sk_length(anim.m), px, py;
        float e = run ? t - sinf(2 * PI_F * t) / (2 * PI_F) : 1 - (1 - t) * (1 - t) * (1 - t);   /* ease in, out */
        if (run) {
            pt_xy(anim.m.to, &x, &y);
            if (travelling) disc_at(!anim.who, x, y, d, 1);   /* the piece taken: there until the run reaches it */
            else if (ms < anim.run_ms + FADE_MS) {
                float k = (ms - anim.run_ms) / (float)FADE_MS;
                disc_at(!anim.who, x, y, d * (1 + 0.25f * k), 1 - k);
            }
            trail(anim.m, anim.who, e * len, travelling ? 1 : 1 - fminf((ms - anim.run_ms) / (2.0f * FADE_MS), 1));
        }
        way_at(anim.m, e * len, &px, &py);
        place_xy(px, py, &x, &y);
        disc_at(anim.who, x, y, d * (run && travelling ? 1 + 0.1f * sinf(PI_F * t) : 1), 1);
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
    const skpos *p = sk_pos(&G);
    int pt = xy_pt(x, y), i;
    if (pt < 0) { sel = -1; return; }
    if (sel >= 0 && (i = find(sel, pt)) >= 0) {
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
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int pt = xy_pt(x, y), i = pt >= 0 ? find(drag.from, pt) : -1;
    if (i >= 0) play(legal[i], 0);
    else if (pt != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b) { ui_bar_count(b, t_small[!player], SK_PIECES - sk_pos(&G)->left[!player]); }

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card: the board with its loops, the pieces at the start */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (CELLS + 2 * LINES_FRAME)), f = (int)(s * LINES_FRAME), n = CELLS * s + 2 * f;
    int o = (size - n) / 2, d = (int)(DISC * s);
    uint8_t *board = ui_buf(n, n);
    paint_board(s, f, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    skpos p;
    sk_start(&p);
    for (int i = 0; i < SK_POINTS; i++) {
        if (p.at[i] < 0) continue;
        float cx = o + lines_px(AT + sk_x(i), (float)s, (float)f);
        float cy = o + lines_py(CELLS, AT + sk_y(i), (float)s, (float)f);
        uint8_t *one = ui_buf(d, d);
        art_counter(COUNTER_DISC, p.at[i], d, one);
        art_flat_put(rgba, size, size, one, d, cx, cy);
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last move to finish */

static const game surakarta_game = {
    .id = "surakarta", .order = 235, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(surakarta_game)
