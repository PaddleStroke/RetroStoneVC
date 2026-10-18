/*
 * Corner Blocks in RetroStone Board: the board of sunken squares (14 x 14 for two, 20 x 20 for three or four), the
 * players' blocks in lacquered wood of their colours, the tray of the blocks still in hand of the player to move
 * (the rules and the notation: cornerblocks_rules.c, cornerblocks.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board and the tray share the room (game.fill): the tray below the board, or beside it on a wide screen. A turn:
 * take a block from the tray (a tap chooses it, a tap on it again turns it; the tools turn it a quarter, turn it over,
 * or lay it), and set it on the board: drag it there, or tap a square (its ghost goes there) and tap the ghost. The
 * ghost shows in the block's colour where it may go, in red where it may not; a block let go where it may go is laid.
 * Dots show the squares where the player's blocks may start (their free corners). The keys: the arrows move the ghost,
 * Q / E (or Page Up / Down, Tab) choose another block, R turns it, X turns it over, Return or Space lays it.
 * A laid block drops in; a bot's block flies from its tray to its place.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/cornerblocks.h"
#include "games/hues.h"
#include "shell.h"

#define GAP 0.025f                              /* between the board and the tray, a part of the board */
#define LIFT 1.4f                               /* a dragged block above the finger, in squares */
#define DROP_MS 170
#define FLY_MS 420
enum { T_TURN, T_OVER, T_LAY, TOOLS };

static cblgame G;
static int n = 14, seats = 2, tall, fr, made_q = -1, made_n = -1, made_s = -1, made_tw = -1, made_th = -1;
static int made_small = -1, made_tq = -1, tcols = 7, trows = 3;
static float q, gx, gy, slot;                   /* a square; the squares' top left; a tray slot */
static box room, tray, tin;                     /* the room, the tray's plank, its inside */
static SDL_Texture *t_board, *t_tray, *t_block[CBL_SEATS], *t_tblock[CBL_SEATS], *t_small[CBL_SEATS];
static int sel = -1;                            /* the block chosen (of the player to move) */
static uint8_t orient[CBL_SEATS][CBL_BLOCKS];   /* each block's turn in each tray */
static struct { int on, ax, ay; } ghost;        /* the chosen block's ghost: its anchor (the box's top left) */
enum { F_NONE, F_TRAY, F_GHOST, F_BOARD };
static struct { int mode, moved, block; float x0, y0, x, y, ox, oy; } fing;   /* ox, oy: finger to the block's corner */
static struct { int on, move, seat, fly; float fx, fy, fc; Uint32 t0; } anim;
static int dirty = 1;
static uint32_t playable;                       /* the blocks of the player to move that fit somewhere */
static uint8_t corner_mark[CBL_SQ];

static const cblpos *now(void) { return cbl_pos(&G); }

static int hue_of(int seat)
{
    for (int c = 1; c <= HUE_N; c++)
        if (HUE_PLAYER[c] == seat) return c;
    return 1;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    anim.on = 0;
    fing.mode = F_NONE;
    ghost.on = 0;
    sel = -1;
    dirty = 1;
}

static void start(int variant)
{
    (void)variant;
    cbl_newgame(&G, ui.players >= 2 && ui.players <= 4 ? ui.players : 2);
    memset(orient, 0, sizeof orient);
    calm();
}

static int status(void)
{
    const cblpos *p = now();
    if (!p->over) return GS_PLAY;
    unsigned w = cbl_winners(p);
    if (w & (w - 1)) return GS_DRAW;
    for (int s = 0; s < p->seats; s++)
        if (w >> s & 1) return gs_win(s);
    return GS_DRAW;
}

static unsigned winners(void) { return now()->over ? cbl_winners(now()) : 0; }

static int turn(void) { return now()->turn; }

static int busy(void) { return anim.on; }

static void refresh(void)                       /* what the player to move can lay (the tray's light, the dots) */
{
    if (!dirty) return;
    dirty = 0;
    const cblpos *p = now();
    playable = 0;
    memset(corner_mark, 0, sizeof corner_mark);
    if (p->over) return;
    static int mv[CBL_MOVES];
    int k = cbl_moves(p, mv, CBL_MOVES);
    for (int i = 0; i < k; i++) playable |= 1u << cbl_block(mv[i]);
    cbl_corners(p, p->turn, corner_mark);
}

/* tray slot i's centre */
static void slot_c(int i, float *x, float *y)
{
    *x = tin.x + (i % tcols + 0.5f) * slot;
    *y = tin.y + (i / tcols + 0.5f) * slot;
}

static void play(int m, int fly)
{
    int seat = now()->turn, b = cbl_block(m);
    if (!cbl_play(&G, m)) { ui_sound(SND_DENY); return; }
    anim.on = 1;
    anim.move = m;
    anim.seat = seat;
    anim.fly = fly;
    anim.t0 = ui_ms();
    slot_c(b, &anim.fx, &anim.fy);
    anim.fc = slot / 5.3f;
    ghost.on = 0;
    sel = -1;
    fing.mode = F_NONE;
    dirty = 1;
    ui_sound(SND_PLACE);
}

static int undo(void)
{
    int ok = cbl_undo(&G);
    calm();
    return ok;
}

static int script(const char *s)
{
    calm();
    return cbl_script(&G, s);
}

const cblgame *cornerblocks_live(void) { return &G; }

int cornerblocks_show(const char *move)
{
    int len, m = cbl_parse(now(), move, &len);
    if (m < 0 || move[len] || !cbl_legal(now(), m)) return 0;
    calm();
    orient[now()->turn][cbl_block(m)] = (uint8_t)cbl_orient(m);
    play(m, 1);
    return 1;
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a block's square in a seat's lacquer, d x d: the wood's grain under the colour, bevelled */
static void paint_block(int seat, int d, uint8_t *o)
{
    art_plank(d, d, 0, o);
    hue_lacquer(o, d, d, hue_of(seat));
    puz_bevel(o, d, d, 0.14f * d, fmaxf(1.2f, 0.13f * d), 0.45f);
}

/* the board: n x n squares of side c sunk in the plank (frame f), the start squares inlaid with their colour */
static void paint_board(uint8_t *o, int nn, int c, int f, int ns)
{
    int w = nn * c + 2 * f;
    art_plank(nn * c, nn * c, f, o);
    for (int y = f; y < w - f; y++)
        for (int x = f; x < w - f; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            p[0] = (uint8_t)(p[0] * 0.78f), p[1] = (uint8_t)(p[1] * 0.78f), p[2] = (uint8_t)(p[2] * 0.8f);
        }
    static const float tone[3] = {0.66f, 0.64f, 0.62f};
    float in = fmaxf(1, 0.05f * c);
    for (int y = 0; y < nn; y++)
        for (int x = 0; x < nn; x++)
            art_inlay(o, w, w, f + x * c + in, f + y * c + in, c - 2 * in, c - 2 * in, 0.12f * c, tone,
                      fmaxf(1, 0.05f * c));
    if (ns < 2) return;
    cblpos p;
    cbl_new(&p, ns);
    for (int s = 0; s < ns; s++) {              /* the start squares: a disc of the player's colour, a pale rim */
        int st = cbl_start(&p, s), d = (int)(0.56f * c);
        float cx = f + (st % CBL_MAXN + 0.5f) * c, cy = f + (st / CBL_MAXN + 0.5f) * c;
        uint8_t col[3], *dot = malloc((size_t)d * d * 4);
        if (!dot) return;
        hue_rgb(hue_of(s), col);
        art_fig *fig = art_fig_new();
        art_ellipse(fig, ART_PART, 0.5f, 0.5f, 0.42f, 0.42f);
        memset(dot, 0, (size_t)d * d * 4);
        art_style sty = art_flat(col, ART_PALE, 0.09f, 0.3f);
        art_render(fig, &sty, d, d, (float)d, 0, 0, dot);
        art_fig_free(fig);
        art_blend(o, w, w, dot, d, d, (int)lroundf(cx - d / 2.0f), (int)lroundf(cy - d / 2.0f));
        free(dot);
    }
}

/* the tools' pictures, white (the shell tints them): a round arrow; two halves of a block mirrored; a tick is the
 * shell's */
static void paint_tool(int kind, int size, uint8_t *rgba)
{
    static const uint8_t white[3] = {255, 255, 255};
    memset(rgba, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    if (kind == T_TURN) {
        art_arc(f, ART_PART, 0.5f, 0.54f, 0.21f, 0.31f, -50, 205);   /* clockwise round, its head at the top left */
        float a = 205 * ART_PI / 180;
        art_head(f, ART_PART, 0.5f + 0.26f * cosf(a) + 0.04f, 0.54f + 0.26f * sinf(a) - 0.07f, 295, 0.26f);
        art_rect(f, ART_PART, 0.43f, 0.47f, 0.57f, 0.61f);   /* the block it turns */
    } else {                                    /* a triangle, its outline mirrored across a dotted line */
        static const float L[] = {0.42f, 0.2f, 0.42f, 0.8f, 0.1f, 0.8f};
        art_poly(f, ART_PART, L, 3);
        float w = 0.07f;
        art_band(f, ART_PART, 0.58f + w / 2, 0.2f, 0.58f + w / 2, 0.8f, w);
        art_band(f, ART_PART, 0.58f, 0.8f - w / 2, 0.9f, 0.8f - w / 2, w);
        art_band(f, ART_PART, 0.6f, 0.22f, 0.9f, 0.8f, w);
        for (int i = 0; i < 4; i++) art_rect(f, ART_PART, 0.48f, 0.12f + 0.21f * i, 0.52f, 0.22f + 0.21f * i);
    }
    art_fig_paint(rgba, size, f, white, NULL, 0, 0);   /* (frees the figure) */
}

static void layout(box b)
{
    room = b;
    n = now()->n;
    seats = now()->seats;
    tall = b.h >= b.w;
    float side, gap;
    if (tall) side = fminf(b.w, b.h - clampf(b.h - b.w, 0.3f * b.h, 0.45f * b.h));
    else side = fminf(b.h, b.w - clampf(b.w - b.h, 0.26f * b.w, 0.45f * b.w));
    gap = GAP * side;
    q = floorf((side - 0.05f * side) / n);
    fr = (int)floorf((side - n * q) / 2);
    float W = n * q + 2 * fr;
    if (tall) {
        gx = floorf(b.x + (b.w - W) / 2) + fr;
        gy = floorf(b.y) + fr;
        tray = (box){b.x + (b.w - W) / 2, b.y + W + gap, W, b.h - W - gap};
    } else {
        gx = floorf(b.x) + fr;
        gy = floorf(b.y + (b.h - W) / 2) + fr;
        tray = (box){b.x + W + gap, b.y + (b.h - W) / 2, b.w - W - gap, W};
    }
    int tf = (int)fmaxf(3, 0.6f * fr);
    tin = (box){tray.x + tf, tray.y + tf, tray.w - 2 * tf, tray.h - 2 * tf};
    float best = 0;
    for (int c = 1; c <= CBL_BLOCKS; c++) {     /* the slots: as big as the tray's shape allows */
        int r = (CBL_BLOCKS + c - 1) / c;
        float s = fminf(tin.w / c, tin.h / r);
        if (s > best) best = s, tcols = c, trows = r;
    }
    slot = floorf(best);
    tin.x += floorf((tin.w - tcols * slot) / 2);
    tin.y += floorf((tin.h - trows * slot) / 2);
    if (made_q != (int)q || made_n != n || made_s != seats) {
        made_q = (int)q, made_n = n, made_s = seats;
        int w = (int)W;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), n, (int)q, fr, seats);
        t_board = ui_tex(w, w, ui_buf(w, w));
        int d = (int)q;
        for (int s = 0; s < CBL_SEATS; s++) {
            ui_free(&t_block[s]);
            paint_block(s, d, ui_buf(d, d));
            t_block[s] = ui_tex(d, d, ui_buf(d, d));
        }
    }
    int tq = (int)ceilf(slot / 5.3f * 1.25f);
    if (made_tq != tq) {                        /* the tray's blocks (and the chosen one, a little bigger) */
        made_tq = tq;
        for (int s = 0; s < CBL_SEATS; s++) {
            ui_free(&t_tblock[s]);
            paint_block(s, tq, ui_buf(tq, tq));
            t_tblock[s] = ui_tex(tq, tq, ui_buf(tq, tq));
        }
    }
    if (made_tw != (int)tray.w || made_th != (int)tray.h) {
        made_tw = (int)tray.w, made_th = (int)tray.h;
        int w = made_tw, h = made_th;
        ui_free(&t_tray);
        if (w > 2 * tf && h > 2 * tf) {
            uint8_t *o = ui_buf(w, h);
            art_plank(w - 2 * tf, h - 2 * tf, tf, o);
            for (int y = tf; y < h - tf; y++)   /* its floor in the shade, as the board's squares: both ivory and
                                                 * ebony show on it */
                for (int x = tf; x < w - tf; x++) {
                    uint8_t *p = o + 4 * (y * w + x);
                    p[0] = (uint8_t)(p[0] * 0.7f), p[1] = (uint8_t)(p[1] * 0.69f), p[2] = (uint8_t)(p[2] * 0.68f);
                }
            t_tray = ui_tex(w, h, o);
        }
    }
    int sm = (int)(0.6f * ui.unit);
    if (made_small != sm) {
        made_small = sm;
        for (int s = 0; s < CBL_SEATS; s++) {
            ui_free(&t_small[s]);
            paint_block(s, sm, ui_buf(sm, sm));
            t_small[s] = ui_tex(sm, sm, ui_buf(sm, sm));
        }
    }
}

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static void cell_xy(int x, int y, float *sx, float *sy)   /* a board square's top left on the screen */
{
    if (ui.flipped) x = n - 1 - x, y = n - 1 - y;
    *sx = gx + x * q;
    *sy = gy + y * q;
}

static int screen_cell(float px, float py, int *x, int *y)   /* the square under a point, as on the screen; 1: on */
{
    *x = (int)floorf((px - gx) / q);
    *y = (int)floorf((py - gy) / q);
    return *x >= 0 && *y >= 0 && *x < n && *y < n;
}

/* a block's squares as drawn (turned round with the board when flipped) */
static void shape_seen(int block, int o, cbl_shape *s)
{
    cbl_shape_of(block, o, s);
    if (ui.flipped)
        for (int i = 0; i < s->n; i++) s->x[i] = (int8_t)(s->w - 1 - s->x[i]), s->y[i] = (int8_t)(s->h - 1 - s->y[i]);
}

/* the anchor of a block whose box's top left is on the screen's square (sx, sy) */
static void anchor_of(const cbl_shape *s, int sx, int sy, int *ax, int *ay)
{
    if (ui.flipped) sx = n - sx - s->w, sy = n - sy - s->h;
    *ax = sx;
    *ay = sy;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the ghost with its box's top left on the screen square nearest (px, py), kept on the board */
static void ghost_at(float px, float py)
{
    if (sel < 0) return;
    cbl_shape s;
    shape_seen(sel, orient[now()->turn][sel], &s);
    int sx = clampi((int)lroundf((px - gx) / q), 0, n - s.w), sy = clampi((int)lroundf((py - gy) / q), 0, n - s.h);
    anchor_of(&s, sx, sy, &ghost.ax, &ghost.ay);
    ghost.on = 1;
}

/* the ghost's box's top left on the screen */
static void ghost_xy(float *x, float *y)
{
    cbl_shape s;
    cbl_shape_of(sel, orient[now()->turn][sel], &s);
    int x0 = ghost.ax, y0 = ghost.ay;
    if (ui.flipped) x0 = ghost.ax + s.w - 1, y0 = ghost.ay + s.h - 1;
    cell_xy(x0, y0, x, y);
}

static int ghost_move(void) { return cbl_move(sel, orient[now()->turn][sel], ghost.ay * CBL_MAXN + ghost.ax); }

static int ghost_ok(void) { return sel >= 0 && ghost.on && cbl_legal(now(), ghost_move()); }

static int on_ghost(float px, float py)
{
    if (sel < 0 || !ghost.on) return 0;
    cbl_shape s;
    cbl_shape_of(sel, orient[now()->turn][sel], &s);
    for (int i = 0; i < s.n; i++) {
        float x, y;
        cell_xy(ghost.ax + s.x[i], ghost.ay + s.y[i], &x, &y);
        if (px >= x - 0.15f * q && px < x + 1.15f * q && py >= y - 0.15f * q && py < y + 1.15f * q) return 1;
    }
    return 0;
}

/* a place where the chosen block fits, nearest the ghost's (or the board's middle): turned as it is, else any turn */
static void ghost_fit(void)
{
    const cblpos *p = now();
    int me = p->turn, best = -1, bo = 0;
    float bd = 1e9f, cx = ghost.on ? ghost.ax + 1.0f : n / 2.0f, cy = ghost.on ? ghost.ay + 1.0f : n / 2.0f;
    for (int pass = 0; pass < 2 && best < 0; pass++)
        for (int o = 0; o < 8; o++) {
            if (pass == 0 ? o != orient[me][sel] : o == orient[me][sel]) continue;
            for (int a = 0; a < CBL_SQ; a++) {
                if (a % CBL_MAXN >= n || a / CBL_MAXN >= n || !cbl_fits(p, sel, o, a)) continue;
                float d = hypotf(a % CBL_MAXN - cx, a / CBL_MAXN - cy);
                if (d < bd) bd = d, best = a, bo = o;
            }
            if (pass == 1 && best >= 0) break;
        }
    if (best < 0) { ghost_at(gx + n * q / 2, gy + n * q / 2); return; }
    orient[me][sel] = (uint8_t)bo;
    ghost.ax = best % CBL_MAXN;
    ghost.ay = best / CBL_MAXN;
    ghost.on = 1;
}

/* the chosen block turned (o: its new turn), about its middle */
static void turn_to(int o)
{
    int me = now()->turn;
    cbl_shape a, b;
    cbl_shape_of(sel, orient[me][sel], &a);
    cbl_shape_of(sel, o, &b);
    orient[me][sel] = (uint8_t)o;
    if (!ghost.on) return;
    int ax = ghost.ax + (a.w - b.w) / 2, ay = ghost.ay + (a.h - b.h) / 2;
    ghost.ax = clampi(ax, 0, n - b.w);
    ghost.ay = clampi(ay, 0, n - b.h);
}

static void turn_quarter(void)
{
    if (sel < 0) return;
    int o = orient[now()->turn][sel];
    turn_to((o & 4) | ((o + 1) & 3));
}

static void turn_over(void)
{
    if (sel < 0) return;
    int o = orient[now()->turn][sel], r = o & 3;
    turn_to(((o ^ 4) & 4) | ((4 - r) & 3));       /* mirrored left to right as seen: the turn the other way */
}

static void choose(int b)
{
    sel = b;
    ghost_fit();
}

static int people(void)
{
    const cblpos *p = now();
    return !p->over && !(ui_bot_seats() >> p->turn & 1) && !anim.on;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* a block (as shown: flipped with the board) of seat's colour, its squares c wide, its box's top left at (x, y);
 * its shadow under it (lifted: a high one); tint: 0 its colour, 1 red (may not go there) */
static void block_at(SDL_Texture *t, int block, int o, float x, float y, float c, Uint8 light, Uint8 alpha, int lifted,
                     int red)
{
    cbl_shape s;
    shape_seen(block, o, &s);
    float d = c - fmaxf(1, roundf(0.06f * c)), in = (c - d) / 2, sx = lifted ? 0.16f * c : 0.05f * c,
          sy = lifted ? 0.24f * c : 0.08f * c;
    SDL_SetTextureColorMod(t, 0, 0, 0);
    SDL_SetTextureAlphaMod(t, (Uint8)(alpha * (lifted ? 0.3f : 0.32f)));
    for (int i = 0; i < s.n; i++) ui_put(t, x + s.x[i] * c + in + sx, y + s.y[i] * c + in + sy, d, d);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    for (int i = 0; i < s.n; i++) {
        if (red) SDL_SetTextureColorMod(t, 255, 120, 100);
        else SDL_SetTextureColorMod(t, light, light, light);
        SDL_SetTextureAlphaMod(t, alpha);
        ui_put(t, x + s.x[i] * c + in, y + s.y[i] * c + in, d, d);
    }
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw_tray(void)
{
    const cblpos *p = anim.on && G.ply ? &G.pos[G.ply - 1] : now();   /* a block flying: its player's tray */
    if (t_tray) ui_put(t_tray, tray.x, tray.y, tray.w, tray.h);
    int me = p->over ? ui_bottom() : p->turn, live = people();   /* the game over: the bottom player's blocks left */
    if (me >= p->seats) me = 0;
    float c = slot / 5.3f;
    for (int b = 0; b < CBL_BLOCKS; b++) {
        if (!(p->left[me] >> b & 1) || (fing.mode == F_TRAY && fing.moved && b == fing.block)) continue;
        if (anim.on && cbl_block(anim.move) == b) continue;
        cbl_shape s;
        shape_seen(b, orient[me][b], &s);
        float cx, cy, k = b == sel && live ? 1.12f : 1;
        slot_c(b, &cx, &cy);
        if (b == sel && live)
            for (int i = 0; i < s.n; i++)
                ui_mark_gold_a(cx + (s.x[i] + 0.5f - s.w / 2.0f) * c * k, cy + (s.y[i] + 0.5f - s.h / 2.0f) * c * k,
                               2 * c, 0.65f);
        int dim = !anim.on && !p->over && !(playable >> b & 1);
        block_at(t_tblock[me], b, orient[me][b], cx - s.w * c * k / 2, cy - s.h * c * k / 2, c * k, dim ? 120 : 255,
                 dim ? 170 : 255, b == sel && live, 0);
    }
}

static void draw(void)
{
    refresh();
    const cblpos *p = now();
    float W = n * q + 2 * fr, x, y, t = 1;
    if (ui.flipped) ui_put_rot(t_board, gx - fr + W / 2, gy - fr + W / 2, W, W, 180);
    else ui_put(t_board, gx - fr, gy - fr, W, W);
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(anim.fly ? FLY_MS : DROP_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    int newest = anim.on ? anim.move : -1;
    cbl_shape ns = {{0}, {0}, 0, 0, 0};
    if (newest >= 0) cbl_shape_of(cbl_block(newest), cbl_orient(newest), &ns);
    float d = q - fmaxf(1, roundf(0.06f * q)), in = (q - d) / 2;
    if (G.ply > 0 && !anim.on) {                /* the last block laid */
        int m = G.move[G.ply - 1], a = cbl_anchor(m);
        cbl_shape s;
        cbl_shape_of(cbl_block(m), cbl_orient(m), &s);
        for (int i = 0; i < s.n; i++) {
            cell_xy(a % CBL_MAXN + s.x[i], a / CBL_MAXN + s.y[i], &x, &y);
            ui_mark_last(x, y, q, q);
        }
    }
    for (int yy = 0; yy < n; yy++)              /* the blocks laid; the newest one comes in */
        for (int xx = 0; xx < n; xx++) {
            int v = p->cell[yy * CBL_MAXN + xx];
            if (!v) continue;
            int fresh = 0;
            if (newest >= 0) {
                int a = cbl_anchor(newest);
                for (int i = 0; i < ns.n; i++) fresh |= a % CBL_MAXN + ns.x[i] == xx && a / CBL_MAXN + ns.y[i] == yy;
            }
            if (fresh) continue;
            cell_xy(xx, yy, &x, &y);
            ui_put(t_block[v - 1], x + in, y + in, d, d);
        }
    int live = people();
    if (live && fing.mode != F_TRAY)            /* where the player's blocks may start */
        for (int c = 0; c < CBL_SQ; c++)
            if (corner_mark[c]) {
                cell_xy(c % CBL_MAXN, c / CBL_MAXN, &x, &y);
                ui_mark_dot(x + q / 2, y + q / 2, 0.28f * q);
            }
    if (newest >= 0) {                          /* the block laid: dropping in, or flying from its tray */
        float e = ui_ease(t), bxs, bys;
        int a = cbl_anchor(newest), o = cbl_orient(newest);
        cbl_shape s;
        shape_seen(cbl_block(newest), o, &s);
        int x0 = a % CBL_MAXN, y0 = a / CBL_MAXN;
        if (ui.flipped) x0 += ns.w - 1, y0 += ns.h - 1;
        cell_xy(x0, y0, &bxs, &bys);
        if (anim.fly) {
            float c = anim.fc + (q - anim.fc) * e, fx = anim.fx - s.w * anim.fc / 2, fy = anim.fy - s.h * anim.fc / 2;
            float px = fx + (bxs - fx) * e, py = fy + (bys - fy) * e - ui_hop(t) * q * 1.5f;
            block_at(t_block[anim.seat], cbl_block(newest), o, px, py, c, 255, 255, t < 1, 0);
        } else {
            float k = 1.18f - 0.18f * e, c = q * k;
            block_at(t_block[anim.seat], cbl_block(newest), o, bxs - (k - 1) * s.w * q / 2, bys - (k - 1) * s.h * q / 2,
                     c, 255, (Uint8)(150 + 105 * e), t < 1, 0);
        }
    }
    if (live && sel >= 0 && ghost.on) {         /* the ghost: the block's colour where it may go, red where not */
        int ok = ghost_ok();
        float sx, sy;
        ghost_xy(&sx, &sy);
        if (ok) {
            cbl_shape s;
            shape_seen(sel, orient[p->turn][sel], &s);
            for (int i = 0; i < s.n; i++)
                ui_mark_gold_a(sx + (s.x[i] + 0.5f) * q, sy + (s.y[i] + 0.5f) * q, 1.5f * q, 0.4f);
        }
        block_at(t_block[p->turn], sel, orient[p->turn][sel], sx, sy, q, 255, ok ? 200 : 160, 0, !ok);
    }
    draw_tray();
    if (live && fing.mode == F_TRAY && fing.moved) {   /* the block in the finger, above it, at the board's size */
        cbl_shape s;
        shape_seen(fing.block, orient[p->turn][fing.block], &s);
        block_at(t_block[p->turn], fing.block, orient[p->turn][fing.block], fing.x - s.w * q / 2,
                 fing.y - LIFT * q - s.h * q / 2, q, 255, 235, 1, 0);
    }
    if (live) {
        ui_tool_paint(T_TURN, paint_tool, T_TURN, 0.62f, sel >= 0 ? UI_TINT : UI_TINT | UI_DIM);
        ui_tool_paint(T_OVER, paint_tool, T_OVER, 0.62f, sel >= 0 ? UI_TINT : UI_TINT | UI_DIM);
        ui_tool(T_LAY, ICON_TICK, ghost_ok() ? UI_LIT : UI_DIM);
    } else {
        ui_tool_paint(T_TURN, paint_tool, T_TURN, 0.62f, UI_TINT | UI_DIM);
        ui_tool_paint(T_OVER, paint_tool, T_OVER, 0.62f, UI_TINT | UI_DIM);
        ui_tool(T_LAY, ICON_TICK, UI_DIM);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int tray_block(float px, float py)       /* the block in hand under the finger, -1 */
{
    if (px < tin.x || py < tin.y) return -1;
    int c = (int)((px - tin.x) / slot), r = (int)((py - tin.y) / slot), b = r * tcols + c;
    if (c >= tcols || r >= trows || b >= CBL_BLOCKS || !(now()->left[now()->turn] >> b & 1)) return -1;
    return b;
}

static void lay(void)
{
    if (ghost_ok()) play(ghost_move(), 0);
    else ui_sound(SND_DENY);
}

static void press(float px, float py)
{
    refresh();
    if (!people()) return;
    fing.mode = F_NONE;
    fing.moved = 0;
    fing.x0 = fing.x = px;
    fing.y0 = fing.y = py;
    int tool = ui_tool_hit(px, py);
    if (tool == T_TURN || tool == T_OVER) {
        if (sel < 0) { ui_sound(SND_DENY); return; }
        if (tool == T_TURN) turn_quarter();
        else turn_over();
        ui_sound(SND_TAP);
        return;
    }
    if (tool == T_LAY) { lay(); return; }
    int b = tray_block(px, py);
    if (b >= 0) {
        fing.mode = F_TRAY;
        fing.block = b;
        return;
    }
    int cx, cy;
    if (!screen_cell(px, py, &cx, &cy) || sel < 0) return;
    float sx, sy;
    if (on_ghost(px, py)) {
        ghost_xy(&sx, &sy);
        fing.mode = F_GHOST;
        fing.ox = sx - px, fing.oy = sy - py;
    } else {
        fing.mode = F_BOARD;
        cbl_shape s;
        shape_seen(sel, orient[now()->turn][sel], &s);
        fing.ox = -s.w * q / 2, fing.oy = -s.h * q / 2;
    }
}

static void motion(float px, float py)
{
    if (fing.mode == F_NONE) return;
    fing.x = px, fing.y = py;
    if (!fing.moved && hypotf(px - fing.x0, py - fing.y0) > 0.3f * fmaxf(q, slot / 4)) {
        fing.moved = 1;
        if (fing.mode == F_TRAY) {              /* a block taken from the tray: chosen */
            if (!(playable >> fing.block & 1)) { ui_sound(SND_DENY); fing.mode = F_NONE; return; }
            sel = fing.block;
            ghost.on = 0;
        }
    }
    if (!fing.moved) return;
    if (fing.mode == F_TRAY) {                  /* its ghost under it on the board, where it would land */
        cbl_shape s;
        shape_seen(sel, orient[now()->turn][sel], &s);
        float bx0 = px - s.w * q / 2, by0 = py - LIFT * q - s.h * q / 2;
        int cx, cy;
        ghost.on = screen_cell(bx0 + q / 2, by0 + q / 2, &cx, &cy) || screen_cell(px, py - LIFT * q, &cx, &cy);
        if (ghost.on) ghost_at(bx0, by0);
    } else ghost_at(px + fing.ox, py + fing.oy);
    ui_animate();
}

static void release(float px, float py)
{
    int mode = fing.mode, moved = fing.moved;
    fing.mode = F_NONE;
    if (mode == F_NONE) return;
    if (mode == F_TRAY && !moved) {             /* a tap on a block: chosen; on the chosen one: turned */
        if (!(playable >> fing.block & 1)) { ui_sound(SND_DENY); return; }
        if (sel == fing.block) turn_quarter();
        else choose(fing.block);
        ui_sound(SND_TAP);
        return;
    }
    if (moved) {                                /* let go: laid where it may go, else its ghost waits there */
        if (ghost_ok()) play(ghost_move(), 0);
        else if (ghost.on) ui_sound(SND_DENY);
        return;
    }
    if (mode == F_GHOST) { lay(); return; }    /* a tap on the ghost */
    ghost_at(px + fing.ox, py + fing.oy);      /* a tap on a square: the ghost centred there */
}

static void key(SDL_Keycode k)
{
    refresh();
    if (!people()) return;
    const cblpos *p = now();
    int me = p->turn;
    if (k == SDLK_q || k == SDLK_e || k == SDLK_PAGEUP || k == SDLK_PAGEDOWN || k == SDLK_TAB) {
        int step = k == SDLK_q || k == SDLK_PAGEUP ? -1 : 1, b = sel < 0 ? (step > 0 ? -1 : CBL_BLOCKS) : sel;
        for (int i = 0; i < CBL_BLOCKS; i++) {
            b = (b + step + CBL_BLOCKS) % CBL_BLOCKS;
            if (playable >> b & 1) { choose(b); ui_sound(SND_TAP); return; }
        }
        ui_sound(SND_DENY);
        return;
    }
    if (sel < 0 && (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN || k == SDLK_r)) {
        for (int b = CBL_BLOCKS - 1; b >= 0; b--)
            if (playable >> b & 1) { choose(b); return; }
        return;
    }
    if (sel < 0) return;
    if (k == SDLK_r) turn_quarter();
    else if (k == SDLK_x) turn_over();
    else if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_KP_ENTER) lay();
    else if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN) {
        int dx = k == SDLK_LEFT ? -1 : k == SDLK_RIGHT ? 1 : 0, dy = k == SDLK_UP ? -1 : k == SDLK_DOWN ? 1 : 0;
        if (ui.flipped) dx = -dx, dy = -dy;
        cbl_shape s;
        cbl_shape_of(sel, orient[me][sel], &s);
        if (!ghost.on) ghost_fit();
        else {
            ghost.ax = clampi(ghost.ax + dx, 0, n - s.w);
            ghost.ay = clampi(ghost.ay + dy, 0, n - s.h);
        }
    }
    ui_animate();
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    if (player < now()->seats) ui_bar_number(b, t_small[player], cbl_score(now(), player));
}

/* blocks laid into a picture: (x, y) squares of side c from (ox, oy), each of seat's colour */
static void lay_squares(uint8_t *rgba, int size, const int (*sq)[3], int k, int c, int ox, int oy)
{
    int d = c - (c >= 8 ? 1 : 0);
    if (d < 2) return;
    uint8_t *one = malloc((size_t)d * d * 4);
    if (!one) return;
    for (int s = 0; s < CBL_SEATS; s++) {
        paint_block(s, d, one);
        for (int i = 0; i < k; i++)
            if (sq[i][2] == s) art_blend(rgba, size, size, one, d, d, ox + sq[i][0] * c, oy + sq[i][1] * c);
    }
    free(one);
}

static void token(int player, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    int c = (int)(size * 0.4f), o = (size - 2 * c) / 2;
    const int sq[3][3] = {{0, 0, player}, {0, 1, player}, {1, 1, player}};
    lay_squares(rgba, size, sq, 3, c, o, o);
}

/* the menu's card: a corner of the board, four players' blocks meeting at their corners */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    const int N = 8;
    int c = (int)(size * 0.9f / (N + 0.6f)), f = (size - N * c) / 2, w = N * c + 2 * f, o = (size - w) / 2;
    uint8_t *board = ui_buf(w, w);
    paint_board(board, N, c, f, 0);
    art_blend(rgba, size, size, board, w, w, o, o);
    static const int SQ[][3] = {
        {0, 7, 0}, {1, 7, 0}, {1, 6, 0}, {2, 5, 0}, {3, 5, 0}, {3, 4, 0}, {3, 3, 0}, {4, 6, 0}, {5, 6, 0}, {5, 7, 0},
        {0, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, 2, 1}, {2, 3, 1}, {2, 4, 1}, {1, 4, 1}, {4, 4, 1},
        {7, 0, 2}, {6, 0, 2}, {5, 0, 2}, {5, 1, 2}, {4, 2, 2}, {4, 3, 2}, {5, 3, 2},
        {7, 7, 3}, {7, 6, 3}, {7, 5, 3}, {6, 4, 3}, {6, 3, 3}, {7, 3, 3},
    };
    lay_squares(rgba, size, SQ, (int)(sizeof SQ / sizeof SQ[0]), c, o + f, o + f);
}

static void home(int player, float *fx, float *fy)
{
    if (ui.players != 3 && ui.players != 4) {
        *fx = 0.5f;
        *fy = player ? 0 : 1;
        return;
    }
    static const float AT[CBL_SEATS][2] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
    *fx = AT[player][0];
    *fy = AT[player][1];
}

static const game cornerblocks_game = {
    .id = "cornerblocks", .order = 213, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners, .key = key,
    .players = 1u << 2 | 1u << 3 | 1u << 4, .home = home, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(cornerblocks_game)
