/*
 * Corridor in RetroStone Board: the 9 x 9 board of inlaid squares with grooves between them, the pawns racing to the
 * far side (each goal edge striped in its player's colour on the frame), the walnut walls (the rules and the
 * notation: corridor_rules.c, corridor.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A turn: tap a dotted square (or drag the pawn there), or place a wall. A wall: drag it from one's bar, or from any
 * groove, onto the board: its ghost follows the finger in the grooves (lying along the groove the finger is in), cream
 * where it may go, red where it may not, and is placed where the finger lifts. With taps alone: a tap on a groove
 * shows the ghost there, a second tap on it places it. The pawn slides (hops over a pawn); the wall drops in.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"                        /* PAWN, for the pawns' figure */
#include "games/corridor.h"
#include "shell.h"

#define FRAME 0.42f                             /* the frame, in pitches (the goal stripes in it) */
#define GROOVE 0.2f                             /* a groove, in pitches (a square: the rest) */
#define SLIDE_MS 190
#define DROP_MS 170
#define PAWN_K 1.22f                            /* a pawn's picture, in pitches */
#define NEAR_LINE 0.15f                         /* a press this close to a line between squares (pitches): a groove */

static crgame G;
static int pitch, fr, made = -1, made_small = -1, made_np = -1;
static float bx, by;                            /* the squares' top left on the screen */
static SDL_Texture *t_board, *t_pawn[CR_PLAYERS], *t_wall[2], *t_small;   /* t_wall: CR_H, CR_V */
static struct { int on, from, to, move; Uint32 t0; } anim;
/* the finger: a pawn dragged, a wall dragged (from the bar or a groove), a press on the waiting ghost */
enum { F_NONE, F_PAWN, F_WALL, F_GHOST };
static struct { int mode, moved, slot, orient; float x0, y0, x, y; } fing;
static struct { int on, slot, orient; } ghost;  /* a wall shown by a tap, waiting for a second tap */

static const crpos *now(void) { return cr_pos(&G); }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    anim.on = 0;
    fing.mode = F_NONE;
    ghost.on = 0;
}

static void start(int variant)
{
    (void)variant;
    cr_new(&G, ui.players == 4 ? 4 : 2);
    calm();
}

static int status(void)
{
    int w = now()->winner;
    return w < 0 ? GS_PLAY : w == CR_DRAW ? GS_DRAW : gs_win(w);
}

static int turn(void) { return now()->turn; }

static void play(int m, int animate)
{
    int from = now()->pawn[now()->turn];
    if (!cr_play(&G, m)) { ui_sound(SND_DENY); return; }
    ghost.on = 0;
    anim.on = animate;
    anim.from = from;
    anim.to = m;
    anim.move = m;
    anim.t0 = ui_ms();
    ui_sound(CR_IS_WALL(m) ? SND_PLACE : SND_MOVE);
}

static int undo(void)
{
    calm();
    return cr_undo(&G);
}

static int script(const char *s)
{
    calm();
    return cr_script(&G, s);
}

static int busy(void) { return anim.on; }

static int can_pass(void) { return now()->winner < 0 && cr_legal(now(), CR_PASS); }
static void pass(void) { if (can_pass()) play(CR_PASS, 0); }

/* the bot's side (corridor_bot.c): the game, read-only; a move in the notation, played as a tap plays it */
const crgame *corridor_live(void) { return &G; }

int corridor_show(const char *move)
{
    int len, m = cr_parse(now(), move, &len);
    if (m < 0 || move[len] || !cr_legal(now(), m)) return 0;
    calm();
    play(m, m != CR_PASS);
    return 1;
}

/* ---- the geometry: board units (u along the files, v up the ranks, 0..9) and the screen ------------------------- */
static void to_screen(float u, float v, float *x, float *y)
{
    if (ui.flipped) { u = CR_N - u; v = CR_N - v; }
    *x = bx + u * pitch;
    *y = by + (CR_N - v) * pitch;
}

static void to_board(float x, float y, float *u, float *v)
{
    *u = (x - bx) / pitch;
    *v = CR_N - (y - by) / pitch;
    if (ui.flipped) { *u = CR_N - *u; *v = CR_N - *v; }
}

static void square_c(int s, float *x, float *y) { to_screen(s % CR_N + 0.5f, s / CR_N + 0.5f, x, y); }

static void slot_c(int slot, float *x, float *y) { to_screen(slot % 8 + 1.0f, slot / 8 + 1.0f, x, y); }

/* a wall's rectangle on the screen (its long side across the screen or down it) */
static void wall_rect(int slot, int orient, float *x, float *y, float *w, float *h)
{
    float cx, cy, len = (2 - 0.12f * GROOVE) * pitch, th = 1.15f * GROOVE * pitch;
    slot_c(slot, &cx, &cy);
    int across = orient == CR_H;                /* a half turn keeps it across */
    *w = across ? len : th;
    *h = across ? th : len;
    *x = cx - *w / 2;
    *y = cy - *h / 2;
}

/* the wall slot a finger at (u, v) points to: the nearest corner inside, lying along the groove it is in (near the
 * corner itself: the orientation it had) */
static void slot_at(float u, float v, int *slot, int *orient)
{
    int i = (int)lroundf(u), j = (int)lroundf(v);
    i = i < 1 ? 1 : i > 8 ? 8 : i;
    j = j < 1 ? 1 : j > 8 ? 8 : j;
    float du = fabsf(u - i), dv = fabsf(v - j);
    if (fabsf(du - dv) > 0.08f) *orient = du > dv ? CR_H : CR_V;
    else if (*orient != CR_H && *orient != CR_V) *orient = CR_H;
    *slot = (i - 1) + 8 * (j - 1);
}

static int on_board(float u, float v) { return u >= -0.2f && v >= -0.2f && u <= CR_N + 0.2f && v <= CR_N + 0.2f; }

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void colour_of(int p, uint8_t rgb[3])
{
    for (int c = 0; c < 3; c++) rgb[c] = (uint8_t)art_player[p].fill[c];
}

/* the board: n x n squares of pitch q in a frame of f pixels ((n q + 2 f) square): the plank, its grooves sunk, the
 * squares inlaid; goal: the stripes on the frame, each side's player (-1 none: top, right, bottom, left) */
static void paint_board(uint8_t *o, int n, int q, int f, const int goal[4])
{
    int w = n * q + 2 * f;
    art_plank(n * q, n * q, f, o);
    for (int y = f; y < w - f; y++)             /* the grooves: the wood sunk in the shade */
        for (int x = f; x < w - f; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] * 0.56f);
        }
    float g = GROOVE * q, c = q - g;
    static const float TONE[3] = {1.62f, 1.6f, 1.56f};
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            art_inlay(o, w, w, f + i * q + g / 2, f + j * q + g / 2, c, c, 0.08f * c, TONE, fmaxf(1.2f, 0.07f * c));
    float sw = fmaxf(2, 0.3f * f), in = f * 0.35f;   /* the goal stripes: inlaid in the frame, its middle */
    for (int k = 0; k < 4; k++) {
        if (goal[k] < 0) continue;
        uint8_t rgb[3];
        colour_of(goal[k], rgb);
        float x0 = f + 0.15f * q, x1 = w - f - 0.15f * q, y0 = in;
        float rx0 = k == 1 ? w - in - sw : k == 3 ? in : x0, ry0 = k == 0 ? y0 : k == 2 ? w - in - sw : x0;
        float rw = k == 1 || k == 3 ? sw : x1 - x0, rh = k == 1 || k == 3 ? x1 - x0 : sw;
        int e = (int)fmaxf(1, 0.25f * sw);      /* a pale rim: ebony shows on the walnut too */
        art_roundrect_put(o, w, w, (int)lroundf(rx0) - e, (int)lroundf(ry0) - e, (int)lroundf(rw) + 2 * e,
                          (int)lroundf(rh) + 2 * e, sw / 2 + e, ART_PALE, 0.55f);
        art_roundrect_put(o, w, w, (int)lroundf(rx0), (int)lroundf(ry0), (int)lroundf(rw), (int)lroundf(rh),
                          sw / 2, rgb, 0.95f);
    }
}

/* the stripes of the game being played: the goal edge of each player, in the board's own sides (top, right...) */
static void goals(int players, int goal[4])
{
    for (int k = 0; k < 4; k++) goal[k] = -1;
    if (players == 2) { goal[0] = 0; goal[2] = 1; return; }
    for (int p = 0; p < 4; p++) goal[p] = p;    /* player p goes to side p (corridor.h) */
}

/* a wall: a bar of dark walnut, w x h pixels, lit from the upper left */
static void paint_wall(int w, int h, uint8_t *o)
{
    memset(o, 0, (size_t)w * h * 4);
    int s = w > h ? w : h;
    art_style st = {{104, 64, 38}, {34, 20, 12}, {58, 36, 22}, {150, 104, 64}, 0.55f, fmaxf(0.012f, 1.0f / s)};
    art_fig *f = art_fig_new();
    float a = 0.5f * (w > h ? (float)h / s : (float)w / s), r = 0.5f;
    if (w > h) art_rect(f, ART_PART, 0.01f, r - a + 0.01f, 0.99f, r + a - 0.01f);
    else art_rect(f, ART_PART, r - a + 0.01f, 0.01f, r + a - 0.01f, 0.99f);
    uint8_t *sq = malloc((size_t)s * s * 4);
    if (!sq) { art_fig_free(f); return; }
    art_render(f, &st, s, s, (float)s, 0, 0, sq);
    art_fig_free(f);
    int ox = (s - w) / 2, oy = (s - h) / 2;     /* the bar cut out of the square it was drawn in */
    for (int y = 0; y < h; y++) memcpy(o + 4 * (size_t)y * w, sq + 4 * ((size_t)(y + oy) * s + ox), (size_t)w * 4);
    free(sq);
}

/* the bars' small wall (walls in hand): a short bar across a size x size picture */
static void paint_small(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    int h = (int)fmaxf(3, 0.26f * size), y = (size - h) / 2;
    uint8_t *bar = malloc((size_t)size * h * 4);
    if (!bar) return;
    paint_wall(size, h, bar);
    art_blend(o, size, size, bar, size, h, 0, y);
    free(bar);
}

static void layout(box b)
{
    float side = fminf(b.w, b.h);
    pitch = (int)floorf(side / (CR_N + 2 * FRAME));
    fr = (int)floorf((side - CR_N * pitch) / 2);
    int n = CR_N * pitch + 2 * fr;
    bx = floorf(b.x + (b.w - n) / 2) + fr;
    by = floorf(b.y + (b.h - n) / 2) + fr;
    if (made != pitch || made_np != now()->players) {
        made = pitch;
        made_np = now()->players;
        int goal[4];
        goals(now()->players, goal);
        ui_free(&t_board);
        paint_board(ui_buf(n, n), CR_N, pitch, fr, goal);
        t_board = ui_tex(n, n, ui_buf(n, n));
        int ps = (int)(PAWN_K * pitch);
        for (int p = 0; p < CR_PLAYERS; p++) {
            ui_free(&t_pawn[p]);
            art_piece_style(PAWN, &art_player[p], ps, ui_buf(ps, ps));
            t_pawn[p] = ui_tex(ps, ps, ui_buf(ps, ps));
        }
        int len = (int)((2 - 0.12f * GROOVE) * pitch), th = (int)fmaxf(3, 1.15f * GROOVE * pitch);
        ui_free(&t_wall[0]);
        ui_free(&t_wall[1]);
        paint_wall(len, th, ui_buf(len, th));
        t_wall[0] = ui_tex(len, th, ui_buf(len, th));
        paint_wall(th, len, ui_buf(th, len));
        t_wall[1] = ui_tex(th, len, ui_buf(th, len));
    }
    int s = (int)(0.6f * ui.unit);
    if (made_small != s) {
        made_small = s;
        ui_free(&t_small);
        paint_small(s, ui_buf(s, s));
        t_small = ui_tex(s, s, ui_buf(s, s));
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* tint: 0 the wall, 1 a ghost where it may not go (red), 2 a ghost where it may (pale) */
static void put_wall(int slot, int orient, float k, Uint8 alpha, int tint)
{
    float x, y, w, h;
    wall_rect(slot, orient, &x, &y, &w, &h);
    SDL_Texture *t = t_wall[orient == CR_V];
    float cx = x + w / 2, cy = y + h / 2;
    SDL_SetTextureAlphaMod(t, alpha);
    if (tint == 1) SDL_SetTextureColorMod(t, 255, 110, 90);
    else if (tint == 2) SDL_SetTextureColorMod(t, 255, 236, 196);
    if (alpha == 255) {                         /* its shadow on the wood */
        SDL_SetTextureColorMod(t, 0, 0, 0);
        SDL_SetTextureAlphaMod(t, 90);
        ui_put_c(t, cx + 0.06f * pitch, cy + 0.08f * pitch, k * w, k * h);
        SDL_SetTextureColorMod(t, 255, 255, 255);
        SDL_SetTextureAlphaMod(t, alpha);
    }
    ui_put_c(t, cx, cy, k * w, k * h);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static void pawn_at(int p, float cx, float cy, float size)
{
    ui_stand_at(t_pawn[p], cx - size / 2, cy - size * 0.6f, size, 0);
}

static void draw(void)
{
    const crpos *p = now();
    float n = (float)(CR_N * pitch + 2 * fr), x, y, h = pitch / 2.0f;
    if (ui.flipped) ui_put_rot(t_board, bx - fr + n / 2, by - fr + n / 2, n, n, 180);
    else ui_put(t_board, bx - fr, by - fr, n, n);
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(CR_IS_WALL(anim.move) ? DROP_MS : SLIDE_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    if (G.ply > 0) {                            /* the last move: the pawn's two squares, or the wall's place */
        int m = G.move[G.ply - 1];
        const crpos *b4 = &G.pos[G.ply - 1];
        if (CR_IS_WALL(m)) {
            float wx, wy, ww, wh;
            wall_rect(CR_SLOT(m), CR_ORIENT(m), &wx, &wy, &ww, &wh);
            ui_mark_last(wx - 0.1f * pitch, wy - 0.1f * pitch, ww + 0.2f * pitch, wh + 0.2f * pitch);
        } else if (m != CR_PASS) {
            int g = (int)(GROOVE * pitch / 2);
            square_c(b4->pawn[b4->turn], &x, &y);
            ui_mark_last(x - h + g, y - h + g, pitch - 2 * g, pitch - 2 * g);
            square_c(m, &x, &y);
            ui_mark_last(x - h + g, y - h + g, pitch - 2 * g, pitch - 2 * g);
        }
    }
    int people = p->winner < 0 && !(ui_bot_seats() >> p->turn & 1) && !anim.on;
    if (people && fing.mode != F_WALL) {        /* where the pawn may go */
        uint8_t to[8];
        int k = cr_pawn_moves(p, to);
        for (int i = 0; i < k; i++) {
            square_c(to[i], &x, &y);
            ui_mark_dot(x, y, 0.3f * pitch);
        }
    }
    for (int s = 0; s < CR_SLOTS; s++)          /* the walls; the newest dropping in */
        if (p->wall[s]) {
            int newest = anim.on && CR_IS_WALL(anim.move) && CR_SLOT(anim.move) == s;
            float k = newest ? 1.3f - 0.3f * ui_ease(t) : 1;
            put_wall(s, p->wall[s], k, newest ? (Uint8)(120 + 135 * t) : 255, 0);
        }
    int sliding = anim.on && !CR_IS_WALL(anim.move) && anim.move != CR_PASS;
    for (int i = 0; i < p->players; i++) {      /* the pawns, the sliding one on its way */
        if (fing.mode == F_PAWN && fing.moved && i == p->turn) continue;
        int mover = sliding && i == (p->turn + p->players - 1) % p->players;
        square_c(p->pawn[i], &x, &y);
        if (mover) {
            float x0, y0, e = ui_ease(t);
            square_c(anim.from, &x0, &y0);
            int far = abs(anim.from % CR_N - anim.to % CR_N) + abs(anim.from / CR_N - anim.to / CR_N) > 1;
            float lift = far ? ui_hop(t) * pitch : 0;
            x = x0 + (x - x0) * e;
            y = y0 + (y - y0) * e - lift;
        }
        pawn_at(i, x, y, PAWN_K * pitch);
    }
    if (people && fing.mode == F_PAWN && fing.moved)   /* the dragged pawn, a little bigger, above the finger */
        pawn_at(p->turn, fing.x, fing.y - 0.55f * pitch, 1.15f * PAWN_K * pitch);
    if (people && (fing.mode == F_WALL || ghost.on)) {   /* the ghost wall: cream where it may go, red where not */
        int slot = fing.mode == F_WALL ? fing.slot : ghost.slot, o = fing.mode == F_WALL ? fing.orient : ghost.orient;
        if (slot >= 0) {
            int ok = cr_wall_ok(p, slot, o);
            if (ok && fing.mode != F_WALL) {
                float cx, cy;
                slot_c(slot, &cx, &cy);
                ui_mark_gold_a(cx, cy, 1.6f * pitch, 0.6f);
            }
            put_wall(slot, o, 1.04f, ok ? 215 : 170, ok ? 2 : 1);
        }
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int reach(int s)
{
    uint8_t to[8];
    int k = cr_pawn_moves(now(), to);
    for (int i = 0; i < k; i++)
        if (to[i] == s) return 1;
    return 0;
}

/* (u, v) near a line between squares (not the board's edge): a groove */
static int in_groove(float u, float v)
{
    float du = fabsf(u - lroundf(u)), dv = fabsf(v - lroundf(v));
    int iu = (int)lroundf(u), iv = (int)lroundf(v);
    return (du < NEAR_LINE && iu >= 1 && iu <= 8 && v > 0.3f && v < CR_N - 0.3f) ||
           (dv < NEAR_LINE && iv >= 1 && iv <= 8 && u > 0.3f && u < CR_N - 0.3f);
}

/* the waiting ghost under (u, v): along its wall, a little room round it */
static int on_ghost(float u, float v)
{
    if (!ghost.on) return 0;
    float cu = ghost.slot % 8 + 1.0f, cv = ghost.slot / 8 + 1.0f;
    float along = ghost.orient == CR_H ? fabsf(u - cu) : fabsf(v - cv), across = ghost.orient == CR_H ? fabsf(v - cv)
                                                                                                        : fabsf(u - cu);
    return along < 1.0f && across < 0.35f;
}

static void begin_wall(float x, float y, float u, float v)
{
    fing.mode = F_WALL;
    fing.moved = 0;
    fing.x0 = fing.x = x;
    fing.y0 = fing.y = y;
    fing.orient = ghost.on ? ghost.orient : CR_H;
    slot_at(u, v, &fing.slot, &fing.orient);
    ui_animate();
}

static void press(float x, float y)
{
    const crpos *p = now();
    if (p->winner >= 0 || anim.on) return;
    float u, v;
    to_board(x, y, &u, &v);
    fing.mode = F_NONE;
    if (!on_board(u, v)) { ghost.on = 0; return; }
    if (on_ghost(u, v)) {                       /* the second tap on the ghost places it (or a drag moves it) */
        fing.mode = F_GHOST;
        fing.moved = 0;
        fing.x0 = fing.x = x;
        fing.y0 = fing.y = y;
        return;
    }
    int col = (int)floorf(u), row = (int)floorf(v), s = col >= 0 && col < CR_N && row >= 0 && row < CR_N ?
                                                         col + CR_N * row : -1;
    if (s >= 0 && s == p->pawn[p->turn]) {      /* the pawn: dragged, or the dots already show where */
        ghost.on = 0;
        fing.mode = F_PAWN;
        fing.moved = 0;
        fing.x0 = fing.x = x;
        fing.y0 = fing.y = y;
        return;
    }
    if (in_groove(u, v) && p->left[p->turn]) { begin_wall(x, y, u, v); return; }
    ghost.on = 0;
    if (s >= 0 && reach(s)) { play(s, 1); return; }
    ui_sound(SND_DENY);
}

/* a wall taken from the bar of the player to move: dragged onto the board */
static int bar_press(int player, float x, float y)
{
    (void)x;
    (void)y;
    const crpos *p = now();
    if (p->winner >= 0 || anim.on || !p->left[player]) return 0;
    if (player != p->turn) { ui_sound(SND_DENY); return 1; }
    ghost.on = 0;
    fing.mode = F_WALL;
    fing.moved = 1;
    fing.slot = -1;
    fing.orient = CR_H;
    return 1;
}

static void motion(float x, float y)
{
    if (fing.mode == F_NONE) return;
    if (hypotf(x - fing.x0, y - fing.y0) > 0.15f * pitch) fing.moved = 1;
    fing.x = x;
    fing.y = y;
    if (fing.mode == F_GHOST && fing.moved) {   /* the ghost picked up: dragged as a wall */
        fing.mode = F_WALL;
        fing.orient = ghost.orient;
        ghost.on = 0;
    }
    if (fing.mode == F_WALL && fing.moved) {    /* the ghost above the finger, in the groove nearest there */
        float u, v;
        to_board(x, y - 0.7f * pitch, &u, &v);
        if (on_board(u, v)) slot_at(u, v, &fing.slot, &fing.orient);
        else fing.slot = -1;
    }
    ui_animate();
}

static void release(float x, float y)
{
    int mode = fing.mode;
    fing.mode = F_NONE;
    const crpos *p = now();
    if (mode == F_GHOST) {                      /* the second tap */
        if (cr_wall_ok(p, ghost.slot, ghost.orient)) play(CR_WALL(ghost.slot, ghost.orient), 1);
        else { ui_sound(SND_DENY); ghost.on = 0; }
        return;
    }
    if (mode == F_WALL) {
        if (!fing.moved) {                      /* a tap on a groove: the ghost waits there */
            ghost.on = 1;
            ghost.slot = fing.slot;
            ghost.orient = fing.orient;
            return;
        }
        if (fing.slot < 0) return;              /* lifted off the board: nothing */
        if (cr_wall_ok(p, fing.slot, fing.orient)) play(CR_WALL(fing.slot, fing.orient), 1);
        else ui_sound(SND_DENY);
        return;
    }
    if (mode == F_PAWN && fing.moved) {
        float u, v;
        to_board(x, y, &u, &v);
        int col = (int)floorf(u), row = (int)floorf(v);
        if (col < 0 || col >= CR_N || row < 0 || row >= CR_N) return;
        int s = col + CR_N * row;
        if (s == p->pawn[p->turn]) return;
        if (reach(s)) play(s, 0);
        else ui_sound(SND_DENY);
    }
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    const crpos *p = now();
    if (p->winner < 0 && player == p->turn) {   /* the last twenty turns before a draw */
        int left = CR_TURNS - p->turns / p->players;
        if (left <= 20) b = ui_bar_countdown(b, left);
    }
    ui_bar_count(b, t_small, p->left[player]);
}

static void token(int player, int size, uint8_t *rgba) { art_piece_style(PAWN, &art_player[player], size, rgba); }

static void home(int player, float *fx, float *fy)
{
    static const float AT[CR_PLAYERS][2] = {{0.5f, 1}, {0, 0.55f}, {0.5f, 0}, {1, 0.45f}};
    int k = ui.players == 4 ? player : player ? 2 : 0;
    *fx = AT[k][0];
    *fy = AT[k][1];
}

/* the menu's card: a corner of the board, the two pawns face to face across a few walls; the picker's cards: two or
 * four pawns */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    const int N = 5;
    int q = (int)(size / (N + 2 * FRAME * 0.8f)), f = (size - N * q) / 2, w = N * q + 2 * f, o = (size - w) / 2;
    int goal[4] = {0, -1, -1, -1};
    uint8_t *board = ui_buf(w, w);
    paint_board(board, N, q, f, goal);
    art_blend(rgba, size, size, board, w, w, o, o);
    int len = (int)((2 - 0.12f * GROOVE) * q), th = (int)fmaxf(3, 1.15f * GROOVE * q);
    uint8_t *bar = malloc((size_t)len * len * 4);
    if (!bar) return;
    static const int WALLS[][3] = {{1, 2, CR_H}, {3, 1, CR_V}, {2, 3, CR_V}};   /* corner (i, j) from the top left */
    for (int k = 0; k < 3; k++) {
        int hz = WALLS[k][2] == CR_H, ww = hz ? len : th, hh = hz ? th : len;
        paint_wall(ww, hh, bar);
        art_blend(rgba, size, size, bar, ww, hh, o + f + WALLS[k][0] * q - ww / 2, o + f + WALLS[k][1] * q - hh / 2);
    }
    free(bar);
    static const int PAWNS[][3] = {{2, 3, 0}, {2, 1, 1}};   /* square (i, j) from the top left, player */
    int ps = (int)(PAWN_K * q);
    for (int k = 0; k < 2; k++) {
        uint8_t *one = ui_buf(ps, ps);
        art_piece_style(PAWN, &art_player[PAWNS[k][2]], ps, one);
        art_blend(rgba, size, size, one, ps, ps, o + f + PAWNS[k][0] * q + (q - ps) / 2,
                  o + f + PAWNS[k][1] * q + (q - ps) / 2 - (int)(0.06f * q));
    }
}

static const game corridor_game = {
    .id = "corridor", .order = 212, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .bar_press = bar_press,
    .players = 1u << 2 | 1u << 4, .home = home, .fill = 1,
};
REGISTER_GAME(corridor_game)
