/*
 * Mahjong in RetroStone Board: the table game for four, Hong Kong Old Style, passing the device round (the rules:
 * mahjong4_rules.c, mahjong4.h). Every hand is secret (VIEW_HIDDEN).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The table, seen by the player holding the device: the four walls in a square in the middle (two tiles high, broken
 * where the dice said, the dead wall at the end of the break), the dice, the prevailing wind and the tiles left in
 * the live wall in the square's middle; each player's discards in front of them inside the walls (rows of six); the
 * three others' tiles standing (their backs), their melds and flowers beside them, along the table's edges, each
 * turned to face its player; the viewer's melds and flowers in a row, their hand at the bottom (the tile just drawn a
 * little apart). The tiles are Mahjong Solitaire's (games/mjtiles.h).
 * A hand starts with the dice and the deal: the tiles fly from the wall, four by four, the flowers to their place and
 * their replacements from the dead wall. A turn: tap a tile to lift it, tap it again (or the table) to discard it, or
 * drag it onto the table. The choices (a win, a kong, a pung, the chows) show above the hand as the sets they would
 * make, glowing: tap one. Pass (the bar's pass button, or a tap on the walls when it is one's turn to draw) lets a
 * discard go. A player asked about a discard they cannot use (see mahjong4.h: the device goes round the same way
 * whoever could claim) sees it a moment, then the device passes on by itself; the next player with nothing to claim
 * draws at once. The hand over: a panel shows the winning hand set by set, its faan, what each player paid or won
 * and the totals; tap it for the next hand.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/mahjong4.h"
#include "games/mjtiles.h"
#include "shell.h"

_Static_assert((int)M4_BAMBOO == MJ_BAMBOO && (int)M4_CHARS == MJ_CHARS && (int)M4_WINDS == MJ_WINDS &&
                   (int)M4_DRAGONS == MJ_DRAGONS && (int)M4_FLOWERS == MJ_FLOWERS && (int)M4_SEASONS == MJ_SEASONS &&
                   (int)M4_ALLKINDS == MJ_FACES,
               "the tiles are Mahjong Solitaire's faces");

#define SLIDE_MS 300
#define REST_MS 320                             /* after the last tile lands: a moment before the device passes */
#define DICE_MS 760                             /* the deal: the dice first */
#define GROUP_MS 120                            /* then four tiles at a time */
#define REPL_MS 180                             /* the flowers' replacements */
#define AUTO_MS 1100                            /* nothing to claim: the discard shown that long, then on */
#define PI_F 3.14159265f

static m4game G;
static int variant, turn_n;                     /* turn_n: the actions at the last hand-off (undo stops there) */
static int sel = -1;                            /* the tile lifted in the hand */
static int deal_pending;                        /* a new hand: the deal plays once its dealer sees the table */
static Uint32 seen_t0;                          /* when the viewer got the device */

/* ---- the tiles' pictures: faces, bodies (face up, face down; standing upright or lying sideways), shadows -------- */
enum { TX_FRONT, TX_BACK, TX_FACE, TX_SHADOW };
#define CACHE 384
typedef struct { int key, w; unsigned used; SDL_Texture *t; } cached;
static cached cache[CACHE];
static unsigned tick;

static int face_h(int fw) { return 2 * (int)lroundf(MJT_ASPECT * fw / 2); }
static int thick(int fw) { return (int)fmaxf(2, lroundf(MJT_THICK * fw)); }

static SDL_Texture *tex(int type, int arg, int fw)
{
    if (fw < 4) return NULL;
    int key = type << 8 | arg, slot = 0;
    for (int i = 0; i < CACHE; i++) {
        if (cache[i].t && cache[i].key == key && cache[i].w == fw) { cache[i].used = ++tick; return cache[i].t; }
        if (!cache[i].t || (cache[slot].t && cache[i].used < cache[slot].used)) slot = i;
    }
    ui_free(&cache[slot].t);
    int fh = face_h(fw), d = thick(fw), w, h;
    uint8_t *buf;
    switch (type) {
    case TX_FACE:
        w = fw, h = fh;
        buf = ui_buf(w, h);
        mjt_face_pic(arg, w, h, buf);
        break;
    case TX_SHADOW:
        w = (arg ? fh : fw) + 2 * d, h = (arg ? fw : fh) + 2 * d;
        buf = ui_buf(w, h);
        mjt_shadow_pic(w, h, 1.1f * d, buf);
        break;
    default:                                    /* a body: arg 1 lying sideways */
        w = (arg ? fh : fw) + d, h = (arg ? fw : fh) + d;
        buf = ui_buf(w, h);
        if (type == TX_BACK) mjt_back_pic(w - d, h - d, d, buf);
        else mjt_tile_pic(w - d, h - d, d, buf);
        break;
    }
    cache[slot] = (cached){key, fw, ++tick, ui_tex(w, h, buf)};
    return cache[slot].t;
}

/* how a tile is drawn: face down; dimmed; lifted (chosen); a gold light under it; no shadow; a warm tint (the last
 * discard); standing (a hidden hand: its back seen from above, shorter) */
enum { TD_BACK = 1, TD_DIM = 2, TD_RAISED = 4, TD_GOLD = 8, TD_FLAT = 16, TD_WARM = 32, TD_STAND = 64 };

/* a tile: its face's middle at (cx, cy), w wide (face units), turned deg (clockwise: 90 lying, its top to the right) */
static void draw_tile(int kind, float cx, float cy, float w, float deg, int how)
{
    int fw = (int)(w + 0.5f);
    if (fw < 4) return;
    int fh = face_h(fw), d = thick(fw), back = (how & TD_BACK) || kind < 0;
    float a = fmodf(fmodf(deg, 360) + 360, 360);
    int q = (int)lroundf(a / 90) & 3, square = fabsf(a - 90.0f * (int)lroundf(a / 90)) < 0.5f, side = q & 1;
    Uint8 k = how & TD_DIM ? 150 : 255;
    if (how & TD_RAISED) cy -= 0.2f * fh;
    float sw = (float)(side ? fh : fw), sh = (float)(side ? fw : fh);
    if (how & TD_STAND) {
        if (side) sw *= 0.68f;
        else sh *= 0.68f;
    }
    if (how & TD_GOLD) ui_mark_gold(cx, cy, 1.9f * fmaxf(sw, sh));
    if (how & TD_WARM) ui_mark_last(cx - sw / 2 - 0.08f * fw, cy - sh / 2 - 0.08f * fw, sw + 0.16f * fw + d,
                                    sh + 0.16f * fw + d);
    SDL_Texture *body = tex(back ? TX_BACK : TX_FRONT, square ? side : 0, fw);
    SDL_Texture *face = back ? NULL : tex(TX_FACE, kind, fw);
    if (!body) return;
    SDL_SetTextureColorMod(body, k, k, k);
    if (face) SDL_SetTextureColorMod(face, k, k, k);
    if (square) {
        float x = cx - sw / 2, y = cy - sh / 2;  /* the face's top left; the body goes on to the lower right */
        if (!(how & TD_FLAT)) {
            SDL_Texture *s = tex(TX_SHADOW, side, fw);
            if (s) ui_put(s, x + 0.25f * d, y + 0.55f * d, sw + 2 * d, sh + 2 * d);
        }
        ui_put(body, x, y, sw + d, sh + d);
        if (face) ui_put_rot(face, cx, cy, (float)fw, (float)fh, q * 90.0);
    } else {                                    /* on its way round: all of it turned */
        float r = a * PI_F / 180, ox = d / 2.0f * (cosf(r) - sinf(r)), oy = d / 2.0f * (sinf(r) + cosf(r));
        ui_put_rot(body, cx + ox, cy + oy, (float)(fw + d), (float)(fh + d), a);
        if (face) ui_put_rot(face, cx, cy, (float)fw, (float)fh, a);
    }
}

/* ---- the table's geometry ---------------------------------------------------------------------------------------- */
static struct {
    box b;
    int me, viewer;                             /* the player at the bottom; the view (-1: the cover) */
    float u, S, Wh, Wm;                         /* the tiles' widths: the table's, the hand's, the melds' */
    float cx, cy, A, D;                         /* the walls' square: its middle, half its side, the walls' depth */
    float rowout[M4_P], rowmax[M4_P];           /* the others' rows: from the middle; their longest */
    float hy, my;                               /* the hand's row, the melds' row (the faces' middles) */
    float oy;                                   /* the choices' row */
} T;

static int rel(int q) { return (q - T.me + M4_P) % M4_P; }

/* a point of a seat's own frame: along its edge (its left to its right), out from the middle */
static void seat_xy(int r, float along, float out, float *x, float *y)
{
    switch (r) {
    case 0: *x = T.cx + along, *y = T.cy + out; break;
    case 1: *x = T.cx + out, *y = T.cy - along; break;
    case 2: *x = T.cx - along, *y = T.cy - out; break;
    default: *x = T.cx - out, *y = T.cy + along; break;
    }
}

static void geometry(box b)
{
    float u = ui.unit;
    T.b = b;
    T.u = u;
    T.Wh = floorf(fminf(b.w / 15.2f, fminf(u, b.h * 0.072f)));
    T.Wm = floorf(T.Wh * 0.68f);
    float hh = 1.43f * T.Wh, mh = 1.43f * T.Wm;
    T.hy = b.y + b.h - 0.06f * u - hh / 2;
    T.my = T.hy - hh / 2 - 0.16f * u - mh / 2;
    float mine = b.y + b.h - (T.my - mh / 2) + 0.1f * u;
    T.S = floorf(fminf((b.w - 0.1f * u) / 23.4f, (b.h - mine) / 21.5f));
    float S = T.S, rowd = 1.43f * S;
    T.D = 1.3f * S + 2.4f * thick((int)S);
    T.A = (18 * S + T.D) / 2;
    T.cx = b.x + b.w / 2;
    T.cy = b.y + rowd + 0.3f * S + T.A;
    T.rowout[2] = T.cy - (b.y + rowd / 2);
    T.rowout[1] = T.rowout[3] = fminf(T.A + 0.3f * S + rowd / 2, b.w / 2 - rowd / 2);
    float below = T.my - mh / 2 - 0.1f * u;
    T.rowmax[1] = T.rowmax[3] = 2 * fminf(T.cy - (b.y + rowd + 0.2f * S), below - T.cy);
    T.rowmax[2] = 2 * (T.rowout[1] - rowd / 2 - 0.2f * S);
    float gap = below - (T.cy + T.A);           /* room between the square and the melds' row: the choices go there */
    T.oy = gap > 1.43f * T.Wm + 0.5f * u ? T.cy + T.A + gap / 2 : T.my;
}

/* ---- where each tile goes ---------------------------------------------------------------------------------------- */
typedef struct { float x, y, deg, w; int face, how, layer; } place;
static place at[M4_TILES];                     /* this frame's */

static int stack_of(const m4pos *p, int i) { return (p->start + i / 2) % M4_STACKS; }

/* a tile of the wall (its index in the wall's order), standing in its stack: on top (if the one under it is still
 * there) or at the bottom */
static void wall_place(const m4pos *p, int i, int lo, int hi, place *pl)
{
    int s = stack_of(p, i), block = s / 18, j = s % 18, owner = (M4_P - block) % M4_P, r = rel(owner);
    int top = !(i & 1) && i + 1 >= lo && i + 1 < hi;
    float along = T.A - T.D - (j + 0.5f) * T.S, out = T.A - T.D / 2 + 0.12f * T.S, x, y;
    seat_xy(r, along, out, &x, &y);
    float z = top ? 0.9f * thick((int)T.S) + 1 : 0;
    *pl = (place){x - z, y - z, -90.0f * r, T.S, 0, TD_FLAT * !top, 100 + (int)(y / 4) * 2 + top};
}

static void pond_place(int r, int k, place *pl)
{
    float W = T.S, step = face_h((int)W) + thick((int)W) * 0.6f, x, y;
    int row = k < 18 ? k / 6 : 2, col = k < 18 ? k % 6 : k - 12;
    seat_xy(r, (col - 2.5f) * (W + 1), 3.05f * W + (row + 0.5f) * step, &x, &y);
    *pl = (place){x, y, -90.0f * r, W, 1, 0, 3000 + r * 100 + k};
}

static int by_kind(const void *a, const void *b)
{
    return *(const uint8_t *)a - *(const uint8_t *)b;
}

/* the viewer's hand in order (by kind: the ids follow the kinds); the tile just drawn last, apart */
static int hand_order(const m4pos *p, int q, uint8_t *o, int *apart)
{
    int n = p->nhand[q];
    memcpy(o, p->hand[q], (size_t)n);
    qsort(o, (size_t)n, 1, by_kind);
    *apart = 0;
    if (p->phase == MP_DISCARD && p->turn == q && p->drawn != M4_NONE)
        for (int i = 0; i < n; i++)
            if (o[i] == p->drawn) {
                memmove(&o[i], &o[i + 1], (size_t)(n - i - 1));
                o[n - 1] = p->drawn;
                *apart = 1;
                break;
            }
    return n;
}

static void hand_x(int i, int n, int apart, float *x)
{
    float step = T.Wh + 1, total = n * step + (apart ? 0.4f * T.Wh : 0);
    *x = T.cx - total / 2 + step / 2 + i * step + (apart && i == n - 1 ? 0.4f * T.Wh : 0);
}

static int reveal_all(void) { return G.pos.phase == MP_SCORE || G.pos.phase == MP_OVER; }

/* the tiles of a meld, face up (a concealed kong: its ends face down) */
static int meld_face(const m4meld *m, int i) { return m->from != M4_NONE || (i == 1 || i == 2); }

/* every tile's place now */
static void places(void)
{
    const m4pos *p = &G.pos;
    for (int i = 0; i < M4_TILES; i++) at[i].layer = -1;
    if (deal_pending) {                         /* before the deal: the walls whole */
        for (int i = 0; i < M4_TILES; i++) wall_place(p, i, 0, M4_TILES, &at[p->wall[i]]);
        return;
    }
    for (int i = p->head; i < p->tail; i++) wall_place(p, i, p->head, p->tail, &at[p->wall[i]]);
    for (int q = 0; q < M4_P; q++) {
        int r = rel(q);
        for (int k = 0; k < p->npond[q]; k++) {
            int id = p->pond[q][k];
            pond_place(r, k, &at[id]);
            if (id == p->tile && k == p->npond[q] - 1 && q == p->from)
                at[id].how |= p->phase == MP_CLAIM ? TD_GOLD : TD_WARM;
        }
        if (r == 0) {                           /* the viewer: the hand, the melds and flowers' row */
            uint8_t o[M4_HAND];
            int apart, n = hand_order(p, q, o, &apart), up = T.viewer >= 0 || reveal_all();
            for (int i = 0; i < n; i++) {
                float x;
                hand_x(i, n, apart, &x);
                at[o[i]] = (place){x, T.hy, 0, T.Wh, up, o[i] == sel ? TD_RAISED : 0, 6000 + i};
            }
            float x = T.b.x + 0.15f * T.u + T.Wm / 2;
            for (int m = 0; m < p->nmeld[q]; m++) {
                const m4meld *md = &p->meld[q][m];
                for (int i = 0; i < md->n; i++, x += T.Wm + 1)
                    at[md->tile[i]] = (place){x, T.my, 0, T.Wm, meld_face(md, i), 0, 5000 + m * 4 + i};
                x += 0.3f * T.Wm;
            }
            float fw = floorf(0.8f * T.Wm), fx = T.b.x + T.b.w - 0.15f * T.u - fw / 2;
            for (int i = p->nflower[q] - 1; i >= 0; i--, fx -= fw + 1)
                at[p->flower[q][i]] = (place){fx, T.my + 0.1f * T.Wm, 0, fw, 1, 0, 5100 + i};
            continue;
        }
        /* another player: their tiles standing, their melds, their flowers; one row along their edge */
        float S = T.S, fw = floorf(0.72f * S), len = p->nhand[q] * (S + 1) + 0.4f * S;
        for (int m = 0; m < p->nmeld[q]; m++) len += p->meld[q][m].n * (S + 1) + 0.3f * S;
        len += p->nflower[q] * (fw + 1);
        float k = len > T.rowmax[r] ? T.rowmax[r] / len : 1, a = -len * k / 2, out = T.rowout[r], x, y;
        int up = reveal_all();
        uint8_t o[M4_HAND];
        int apart, n = hand_order(p, q, o, &apart);
        for (int i = 0; i < n; i++, a += (S + 1) * k) {
            seat_xy(r, a + S / 2, out, &x, &y);
            at[o[i]] = (place){x, y, -90.0f * r, S, up, up ? 0 : TD_STAND, 2000 + r * 100 + i};
        }
        a += 0.4f * S * k;
        for (int m = 0; m < p->nmeld[q]; m++) {
            const m4meld *md = &p->meld[q][m];
            for (int i = 0; i < md->n; i++, a += (S + 1) * k) {
                seat_xy(r, a + S / 2, out, &x, &y);
                at[md->tile[i]] = (place){x, y, -90.0f * r, S, meld_face(md, i), 0, 2000 + r * 100 + 20 + m * 4 + i};
            }
            a += 0.3f * S * k;
        }
        for (int i = 0; i < p->nflower[q]; i++, a += (fw + 1) * k) {
            seat_xy(r, a + fw / 2, out + 0.14f * S, &x, &y);
            at[p->flower[q][i]] = (place){x, y, -90.0f * r, fw, 1, 0, 2000 + r * 100 + 50 + i};
        }
    }
}

/* ---- moving tiles: each glides to where it belongs now ----------------------------------------------------------- */
typedef struct {
    int on, face, face0;
    float x, y, deg, w, x0, y0, deg0, w0;
    Uint32 t0;                                  /* when it starts (may be ahead: the deal) */
} sprite;
static sprite spr[M4_TILES];
static Uint32 quiet;

static float progress(const sprite *s)
{
    Uint32 now = ui_ms();
    if ((int32_t)(now - s->t0) <= 0) return 0;
    Uint32 el = now - s->t0;
    if (el >= SLIDE_MS) return 1;
    float t = (float)el / SLIDE_MS;
    return t * t * (3 - 2 * t);
}

static void spr_now(int id, float *x, float *y, float *deg, float *w, int *face)
{
    const sprite *s = &spr[id];
    float e = progress(s), dd = fmodf(s->deg - s->deg0 + 540.0f, 360.0f) - 180;
    *x = s->x0 + (s->x - s->x0) * e;
    *y = s->y0 + (s->y - s->y0) * e;
    *deg = s->deg0 + dd * e;
    *w = s->w0 + (s->w - s->w0) * e;
    *face = e < 0.5f ? s->face0 : s->face;
}

static int moving(int id) { return spr[id].on && progress(&spr[id]) < 1; }

static void target(int id, const place *pl, int delay)
{
    sprite *s = &spr[id];
    if (!s->on) {
        *s = (sprite){1, pl->face, pl->face, pl->x, pl->y, pl->deg, pl->w, pl->x, pl->y, pl->deg, pl->w,
                      ui_ms() - SLIDE_MS};
        return;
    }
    if (fabsf(s->x - pl->x) < 0.5f && fabsf(s->y - pl->y) < 0.5f && fabsf(s->w - pl->w) < 0.5f &&
        fabsf(fmodf(s->deg - pl->deg + 720.0f, 360.0f)) < 0.5f && s->face == pl->face)
        return;
    float x, y, d, w;
    int f;
    spr_now(id, &x, &y, &d, &w, &f);
    *s = (sprite){1, pl->face, f, pl->x, pl->y, pl->deg, pl->w, x, y, d, w, ui_ms() + (Uint32)delay};
    Uint32 end = ui_ms() + (Uint32)(delay + SLIDE_MS + REST_MS);
    if (end > quiet) quiet = end;
}

static void sync(void)
{
    places();
    for (int id = 0; id < M4_TILES; id++)
        if (at[id].layer >= 0) target(id, &at[id], 0);
        else spr[id].on = 0;
}

/* everything in its place at once (another view) */
static void snap(void)
{
    memset(spr, 0, sizeof spr);
    quiet = 0;
    sync();
}

/* the deal: the dice, then the tiles from the wall to the hands, four at a time, then the flowers' replacements */
static void start_deal(void)
{
    const m4pos *p = &G.pos;
    deal_pending = 1;
    snap();                                     /* the walls whole */
    deal_pending = 0;
    int d[3] = {p->dice[0], p->dice[1], p->dice[2]};
    ui_dice_throw(3, d, p->dealer);
    places();
    for (int i = 0; i < M4_TILES; i++) {
        int id = p->wall[i], delay;
        if (i < 53) delay = DICE_MS + (i < 48 ? i / 4 : 12 + i - 48) * GROUP_MS;
        else if (i >= p->tail) delay = DICE_MS + 17 * GROUP_MS + (M4_TILES - 1 - i) * REPL_MS;
        else continue;
        target(id, &at[id], delay);
    }
    for (int id = 0; id < M4_TILES; id++)
        if (at[id].layer >= 0) target(id, &at[id], 0);
    ui_sound_later(SND_DICE, 0);
    for (int g = 0; g < 17; g++) ui_sound_later(SND_MOVE, DICE_MS + g * GROUP_MS + SLIDE_MS - 40);
}

static int busy(void)
{
    for (int id = 0; id < M4_TILES; id++)
        if (moving(id)) return 1;
    return (quiet && ui_ms() < quiet) || ui_dice_busy();
}

static void hand_start(void)
{
    const m4pos *p = &G.pos;
    int d[3] = {p->dice[0], p->dice[1], p->dice[2]};
    ui_dice_show(3, d, p->dealer);
}

static void layout(box b)
{
    int me = ui_bottom();
    if (me < 0 || me >= M4_P) me = 0;
    geometry(b);
    if (me != T.me || ui.viewer != T.viewer) {  /* another view: nothing glides from the last one */
        T.me = me;
        T.viewer = ui.viewer;
        seen_t0 = ui_ms();
        if (ui.viewer >= 0 && deal_pending) start_deal();
        else snap();
    }
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void start(int v)
{
    variant = v >= 0 && v < M4_VARIANTS ? v : M4_EAST;
    m4_new(&G, variant, (uint32_t)ui_random(1 << 30));
    turn_n = 0;
    sel = -1;
    deal_pending = 1;
    hand_start();
    T.viewer = -2;
}

static int status(void)
{
    if (G.pos.phase != MP_OVER) return GS_PLAY;
    unsigned w = m4_winners(&G.pos);
    if (w & (w - 1)) return GS_DRAW;
    for (int q = 0; q < M4_P; q++)
        if (w >> q & 1) return gs_win(q);
    return GS_DRAW;
}

static unsigned winners(void) { return m4_winners(&G.pos); }

static int turn(void)
{
    int a = m4_actor(&G.pos);
    if (a >= 0) return a;
    unsigned w = m4_winners(&G.pos);
    for (int q = 0; q < M4_P; q++)
        if (w >> q & 1) return q;
    return 0;
}

static void act(int a)
{
    m4pos *p = &G.pos;
    int head = p->head, tail = p->tail, kind = MA_KIND(a);
    if (!m4_play(&G, a)) { ui_sound(SND_DENY); return; }
    sel = -1;
    if (kind == MA_NEXT) {                      /* the next hand: its dealer gets the device, then the deal */
        deal_pending = 1;
        hand_start();
        turn_n = 0;
        sync();                                 /* the tiles back to the walls; then the device to the dealer */
        ui_handoff(m4_actor(p));
        return;
    }
    sync();
    if (kind == MA_DISCARD) ui_sound_later(SND_PLACE, SLIDE_MS - 40);
    else if (kind == MA_PUNG || kind == MA_CHOW || kind == MA_KONG) ui_sound(SND_CAPTURE);
    else if (kind == MA_PASS) ui_sound(SND_TAP);
    if (p->head != head || p->tail != tail) ui_sound_later(SND_MOVE, SLIDE_MS - 40);
    if (p->phase == MP_SCORE) {
        ui_sound_later(p->result.winner == M4_NONE ? SND_TAP : SND_CHECK, SLIDE_MS + 100);
        quiet = ui_ms() + SLIDE_MS + 900;       /* the table a moment, then the hand's summary */
        turn_n = G.n;
        return;
    }
    int who = m4_actor(p);
    if (who != T.viewer) {
        ui_handoff(who);
        turn_n = G.n;
    } else if (p->head != head || p->tail != tail) turn_n = G.n;   /* a tile drawn: no taking it back */
}

static int undo(void)
{
    if (G.n <= turn_n || !m4_undo(&G)) return 0;
    sel = -1;
    snap();
    return 1;
}

static int mine(void) { return !ui_handing() && T.viewer >= 0 && m4_actor(&G.pos) == T.viewer; }

static int can_pass(void)
{
    return mine() && (G.pos.phase == MP_CLAIM || G.pos.phase == MP_ROB);
}

static void pass(void) { if (can_pass()) act(MA_PASS); }

static int script(const char *s)
{
    int ok = m4_script(&G, s);
    sel = -1;
    deal_pending = G.n == 0 && G.pos.phase == MP_DISCARD;
    hand_start();
    turn_n = G.n;
    T.viewer = -2;
    ui_handoff(turn());
    return ok;
}

/* ---- the choices: a win, the kongs, a pung, the chows; shown as the sets they make ------------------------------- */
typedef struct { int act, n, hi; uint8_t kind[4]; float x, w; } offer;
static offer offers[8];
static int noffers;

static void make_offers(void)
{
    const m4pos *p = &G.pos;
    noffers = 0;
    if (!mine()) return;
    if (p->phase == MP_DISCARD) {
        if (m4_legal(p, MA_WIN)) offers[noffers++] = (offer){MA_WIN, 1, 0, {(uint8_t)m4_kind(p->drawn)}, 0, 0};
        int ks[8], n = m4_kongs(p, ks);
        for (int i = 0; i < n && noffers < 8; i++)
            offers[noffers++] = (offer){MA_KONG | ks[i], 4, -1, {(uint8_t)ks[i], (uint8_t)ks[i], (uint8_t)ks[i],
                                                                  (uint8_t)ks[i]}, 0, 0};
    } else if (p->phase == MP_CLAIM || p->phase == MP_ROB) {
        int ch[3], nch, o = m4_options(p, ch, &nch), k = m4_kind(p->tile);
        if (o & MO_WIN) offers[noffers++] = (offer){MA_WIN, 1, 0, {(uint8_t)k}, 0, 0};
        if (o & MO_KONG) offers[noffers++] = (offer){MA_KONG | k, 4, 3, {(uint8_t)k, (uint8_t)k, (uint8_t)k,
                                                                          (uint8_t)k}, 0, 0};
        if (o & MO_PUNG) offers[noffers++] = (offer){MA_PUNG, 3, 2, {(uint8_t)k, (uint8_t)k, (uint8_t)k}, 0, 0};
        for (int i = 0; i < nch; i++)
            offers[noffers++] = (offer){MA_CHOW | ch[i], 3, k - ch[i], {(uint8_t)ch[i], (uint8_t)(ch[i] + 1),
                                                                         (uint8_t)(ch[i] + 2)}, 0, 0};
    }
    float W = T.Wm, gap = 0.55f * T.u, total = 0;
    for (int i = 0; i < noffers; i++) {
        offers[i].w = offers[i].act == MA_WIN ? 2.3f * W : offers[i].n * (W + 1);
        total += offers[i].w + (i ? gap : 0);
    }
    float x = T.cx - total / 2;
    for (int i = 0; i < noffers; i++) {
        offers[i].x = x;
        x += offers[i].w + gap;
    }
}

static SDL_Texture *t_panel, *t_disc;
static int panel_w, panel_h, disc_d, tok_d;

static void panel(float x, float y, float w, float h, Uint8 alpha)
{
    if (panel_w != (int)w || panel_h != (int)h) {
        panel_w = (int)w;
        panel_h = (int)h;
        const uint8_t c[4] = {28, 34, 32, 255};
        ui_free(&t_panel);
        art_roundrect(panel_w, panel_h, fminf(0.35f * T.u, h / 3), c, ui_buf(panel_w, panel_h));
        t_panel = ui_tex(panel_w, panel_h, ui_buf(panel_w, panel_h));
    }
    SDL_SetTextureAlphaMod(t_panel, alpha);
    ui_put(t_panel, x, y, w, h);
}

static void gold_disc(float cx, float cy, float d)
{
    if (disc_d != (int)d) {
        static const uint8_t gold[3] = {196, 156, 64};
        disc_d = (int)d;
        ui_free(&t_disc);
        art_disc(disc_d, gold, 1, 0, ui_buf(disc_d, disc_d));
        t_disc = ui_tex(disc_d, disc_d, ui_buf(disc_d, disc_d));
    }
    ui_put_c(t_disc, cx, cy, d, d);
}

static void draw_offers(void)
{
    if (!noffers) return;
    float W = T.Wm, h = face_h((int)W) + thick((int)W), last = offers[noffers - 1].x + offers[noffers - 1].w;
    float pad = 0.25f * T.u;
    panel(offers[0].x - pad, T.oy - h / 2 - pad, last - offers[0].x + 2 * pad, h + 2 * pad, 235);
    for (int i = 0; i < noffers; i++) {
        offer *o = &offers[i];
        float cx = o->x + o->w / 2;
        if (o->act == MA_WIN) {                 /* a win: the tile on a gold disc, in gold light */
            ui_mark_gold(cx, T.oy, 3.6f * W);
            gold_disc(cx, T.oy, 2.2f * W);
            draw_tile(o->kind[0], cx - 0.05f * W, T.oy - 0.06f * W, 1.05f * W, 0, TD_FLAT);
            continue;
        }
        ui_mark_gold(cx, T.oy, o->w + 0.9f * W);
        for (int k = 0; k < o->n; k++)
            draw_tile(o->kind[k], o->x + (k + 0.5f) * (W + 1), T.oy, W, 0, k == o->hi ? TD_RAISED : 0);
    }
}

static int offer_hit(float x, float y)
{
    float h = face_h((int)T.Wm) + thick((int)T.Wm);
    for (int i = 0; i < noffers; i++)
        if (x >= offers[i].x - 0.15f * T.u && x <= offers[i].x + offers[i].w + 0.15f * T.u &&
            fabsf(y - T.oy) <= h / 2 + 0.25f * T.u)
            return i;
    return -1;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
/* the middle of the square: the prevailing wind, the dice, the tiles left in the live wall */
static void draw_middle(void)
{
    const m4pos *p = &G.pos;
    float W = T.S, die = 0.62f * W;
    if (ui_dice_busy()) {                       /* thrown: big in the middle */
        ui_dice_draw(T.cx, T.cy, 1.1f * W, 0);
        return;
    }
    int over = p->phase == MP_OVER;            /* over: the last round's */
    draw_tile(M4_WINDS + ((p->wind - over) & 3), T.cx - 0.75f * W, T.cy - 0.95f * W, 0.95f * W, 0, 0);
    ui_number(T.cx + 0.55f * W, T.cy - 0.95f * W, 0.62f * W, over ? M4_P : p->dealer + 1, 1);   /* the hand */
    for (int i = 0; i < 3; i++) ui_die_at(T.cx + (i - 1) * 0.85f * W, T.cy + 0.25f * W, die, p->dealer, p->dice[i], 1);
    if (!deal_pending) ui_number(T.cx, T.cy + 1.15f * W, 0.6f * W, m4_live(p), 1);
}

/* the hand over: the winning hand set by set, its faan; each player's points and total */
static void draw_summary(void)
{
    const m4pos *p = &G.pos;
    const m4result *r = &p->result;
    box b = T.b;
    float u = T.u, pw = fminf(b.w * 0.96f, 8.8f * u);
    uint8_t kind[40];
    int n = 0, gaps = 0, gold = -1, flat[40];
    if (r->winner != M4_NONE) {
        const m4score *s = &r->score;
        int w = r->winner, wk = m4_kind(r->tile);
        if (s->nset) {
            for (int i = 0; i < s->nset; i++) {
                int t = s->set_type[i], k = s->set_kind[i], c = t == MS_PAIR ? 2 : 3;
                for (int j = 0; j < c; j++) {
                    int kk = t == MS_CHOW ? k + j : k;
                    if (gold < 0 && kk == wk) gold = n;
                    flat[n] = gaps;
                    kind[n++] = (uint8_t)kk;
                }
                gaps++;
            }
        } else {                                /* thirteen orphans: as they are */
            uint8_t o[M4_HAND];
            int apart, m = hand_order(p, w, o, &apart);
            for (int i = 0; i < m; i++) {
                if (gold < 0 && m4_kind(o[i]) == wk) gold = n;
                flat[n] = gaps;
                kind[n++] = (uint8_t)m4_kind(o[i]);
            }
            gaps++;
        }
        for (int m = 0; m < p->nmeld[w]; m++, gaps++)
            for (int i = 0; i < p->meld[w][m].n; i++) flat[n] = gaps, kind[n++] = p->meld[w][m].kind +
                                                                  (p->meld[w][m].type == MS_CHOW ? i : 0);
        for (int i = 0; i < p->nflower[w]; i++) flat[n] = gaps, kind[n++] = (uint8_t)m4_kind(p->flower[w][i]);
    }
    float tw = n ? fminf(0.6f * u, (pw - 0.5f * u) / (n + gaps * 0.3f + 0.2f)) : 0.5f * u;
    float th = face_h((int)tw) + thick((int)tw), rh = fminf(0.78f * u, b.h * 0.08f);
    float ph = 0.35f * u + th + 0.3f * u + 1.0f * u + M4_P * rh + 0.3f * u;
    float x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2, y = y0 + 0.35f * u + th / 2;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 70);
    panel(x0, y0, pw, ph, 255);
    if (r->winner != M4_NONE) {
        float x = x0 + (pw - (n * (tw + 1) + gaps * 0.3f * tw)) / 2 + tw / 2;
        for (int i = 0; i < n; i++) {
            float gx = x + i * (tw + 1) + flat[i] * 0.3f * tw;
            int fl = i >= n - p->nflower[r->winner] && p->nflower[r->winner];
            draw_tile(kind[i], gx, y, fl ? 0.8f * tw : tw, 0, i == gold ? TD_GOLD | TD_RAISED : 0);
        }
        y += th / 2 + 0.3f * u + 0.5f * u;
        gold_disc(T.cx, y, 0.95f * u);          /* the faan */
        ui_number(T.cx, y, 0.5f * u, r->score.faan, 0);
        tok_d = (int)(0.62f * u);
        ui_mark_gold(T.cx - 1.6f * u, y, 1.5f * u);
        ui_put_c(ct_token(r->winner, tok_d), T.cx - 1.6f * u, y, 0.62f * u, 0.62f * u);
        if (r->payer != M4_NONE) {              /* who paid most: their token, dimmed */
            SDL_Texture *pt = ct_token(r->payer, tok_d);
            SDL_SetTextureAlphaMod(pt, 150);
            ui_put_c(pt, T.cx + 1.6f * u, y, 0.5f * u, 0.5f * u);
        } else draw_tile(-1, T.cx + 1.6f * u, y, 0.36f * u, 0, TD_BACK);   /* self-drawn: from the wall */
    } else {                                    /* a drawn hand: the wall used up */
        for (int i = 0; i < 4; i++) draw_tile(-1, T.cx + (i - 1.5f) * 0.52f * u, y, 0.5f * u, 0, TD_BACK);
        y += th / 2 + 0.3f * u + 0.5f * u;
    }
    y += 0.5f * u + 0.2f * u;
    tok_d = (int)(0.62f * u);
    for (int q = 0; q < M4_P; q++, y += rh) {
        float cy = y + rh / 2, d = fminf(0.62f * u, 0.8f * rh);
        if (q == r->winner) ui_mark_gold(x0 + 0.7f * u, cy, 2.0f * d);
        ui_put_c(ct_token(q, tok_d), x0 + 0.7f * u, cy, d, d);
        draw_tile(M4_WINDS + m4_seat(p, q), x0 + 1.6f * u, cy, 0.42f * rh, 0, TD_FLAT);
        if (r->pay[q]) ui_number_signed(x0 + pw * 0.5f, cy, 0.42f * rh, r->pay[q], 1, 1);
        ui_number_signed(x0 + pw * 0.8f, cy, 0.5f * rh, p->total[q], 1, 0);
        if (q < M4_P - 1) ct_rule(x0 + 0.3f * u, y + rh, pw - 0.6f * u);
    }
}

typedef struct { int on, moved, id; float x0, y0, x, y; } dragging;
static dragging drag;

static int cmp_layer(const void *a, const void *b)
{
    int ia = *(const int *)a, ib = *(const int *)b;
    int la = moving(ia) ? 100000 + at[ia].layer : at[ia].layer, lb = moving(ib) ? 100000 + at[ib].layer : at[ib].layer;
    return la - lb;
}

static void draw(void)
{
    const m4pos *p = &G.pos;
    if (deal_pending && T.viewer >= 0 && !ui_handing() && m4_actor(p) == T.viewer && !busy())
        start_deal();                           /* the same dealer again: no hand-off, the deal at once */
    sync();
    int ids[M4_TILES], n = 0;
    for (int id = 0; id < M4_TILES; id++)
        if (spr[id].on && at[id].layer >= 0) ids[n++] = id;
    qsort(ids, (size_t)n, sizeof ids[0], cmp_layer);
    draw_middle();
    for (int i = 0; i < n; i++) {
        int id = ids[i];
        if (drag.on && drag.moved && drag.id == id) continue;
        float x, y, d, w;
        int f;
        spr_now(id, &x, &y, &d, &w, &f);
        int how = at[id].how;
        if (moving(id)) how &= ~(TD_GOLD | TD_WARM);
        draw_tile(m4_kind(id), x, y, w, d, how | (f ? 0 : TD_BACK));
    }
    if (drag.on && drag.moved) draw_tile(m4_kind(drag.id), drag.x, drag.y, T.Wh, 0, TD_RAISED);
    int b = busy();
    make_offers();
    if (!b) draw_offers();
    if (p->phase == MP_SCORE && !b) draw_summary();
    /* the device's holder with nothing to claim: on at once (the next player: their draw), or after a moment */
    if (mine() && !b && (p->phase == MP_CLAIM || p->phase == MP_ROB) && !m4_options(p, NULL, NULL)) {
        int draws = p->phase == MP_CLAIM && p->k == p->nask - 1;
        if (draws || (int32_t)(ui_ms() - seen_t0) >= AUTO_MS) act(MA_PASS);
        else ui_animate();
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int hand_hit(float x, float y)
{
    const m4pos *p = &G.pos;
    uint8_t o[M4_HAND];
    int apart, n = hand_order(p, T.me, o, &apart);
    float h = face_h((int)T.Wh);
    for (int i = n - 1; i >= 0; i--) {
        float cx;
        hand_x(i, n, apart, &cx);
        float cy = T.hy - (o[i] == sel ? 0.2f * h : 0);
        if (fabsf(x - cx) <= (T.Wh + 1) / 2 && y >= cy - h / 2 - 0.1f * T.u && y <= cy + h / 2 + 0.2f * T.u)
            return o[i];
    }
    return -1;
}

static int on_table(float x, float y) { return fabsf(x - T.cx) <= T.A && fabsf(y - T.cy) <= T.A; }

static void press(float x, float y)
{
    const m4pos *p = &G.pos;
    drag.on = 0;
    if (p->phase == MP_SCORE) { if (!busy()) act(MA_NEXT); return; }
    if (p->phase == MP_OVER || busy()) return;
    if (!mine()) { ui_sound(SND_DENY); return; }
    make_offers();
    int o = offer_hit(x, y);
    if (o >= 0) { act(offers[o].act); return; }
    drag = (dragging){1, 0, p->phase == MP_DISCARD ? hand_hit(x, y) : -1, x, y, x, y};
}

static void motion(float x, float y)
{
    if (!drag.on || drag.id < 0) return;
    if (!drag.moved && hypotf(x - drag.x0, y - drag.y0) > 0.3f * T.u) {
        drag.moved = 1;
        sel = drag.id;
    }
    drag.x = x;
    drag.y = y;
    if (drag.moved) ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    const m4pos *p = &G.pos;
    if (drag.moved) {                           /* dropped on the table: discarded; else back in the hand */
        if (y < T.my) act(MA_DISCARD | drag.id);
        else sel = -1;
        return;
    }
    if (drag.id >= 0) {                         /* a tile of the hand: lifted; lifted already: discarded */
        if (drag.id == sel) act(MA_DISCARD | drag.id);
        else { sel = drag.id; ui_sound(SND_TAP); }
        return;
    }
    if (on_table(x, y)) {
        if (p->phase == MP_DISCARD && sel >= 0) { act(MA_DISCARD | sel); return; }
        if (p->phase == MP_CLAIM && p->k == p->nask - 1 && p->pend == M4_NONE) { act(MA_PASS); return; }   /* draw */
    }
    if (sel >= 0) { sel = -1; return; }
    ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the player's seat wind and total */
static void bar(int player, box b)
{
    const m4pos *p = &G.pos;
    float u = T.u ? T.u : ui.unit, tw = fminf(0.42f * u, b.h * 0.5f), h = fminf(0.44f * u, b.h * 0.48f);
    draw_tile(M4_WINDS + m4_seat(p, player), b.x + tw / 2 + 0.05f * u, b.y + b.h / 2 - 0.03f * u, tw, 0, 0);
    int v = p->total[player], digits = (v < 0) + (abs(v) >= 1000 ? 4 : abs(v) >= 100 ? 3 : abs(v) >= 10 ? 2 : 1);
    float room = b.w - tw - 0.25f * u, need = (0.74f * (digits - 1) + 0.62f) * h;
    if (need > room) h *= room / need, need = room;
    ui_number_signed(b.x + tw + 0.2f * u + need / 2, b.y + b.h / 2, h, v, 1, 0);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* a tile painted into a picture (the menu's and the picker's cards): its face's top left at (x, y) */
static void blend_tile(uint8_t *rgba, int size, int kind, int x, int y, int fw)
{
    int fh = face_h(fw), d = thick(fw), w = fw + d, h = fh + d, sw = fw + 2 * d, sh = fh + 2 * d;
    uint8_t *body = malloc((size_t)w * h * 4), *face = malloc((size_t)fw * fh * 4), *s = malloc((size_t)sw * sh * 4);
    if (body && face && s) {
        mjt_shadow_pic(sw, sh, 1.1f * d, s);
        art_blend(rgba, size, size, s, sw, sh, x + d / 4, y + d / 2);
        if (kind < 0) mjt_back_pic(fw, fh, d, body);
        else {
            mjt_tile_pic(fw, fh, d, body);
            mjt_face_pic(kind, fw, fh, face);
            art_blend(body, w, h, face, fw, fh, 0, 0);
        }
        art_blend(rgba, size, size, body, w, h, x, y);
    }
    free(body);
    free(face);
    free(s);
}

/* the menu's card: a pung of the east wind over a chow and the red dragon; the picker's: the east round (東), the
 * east and south rounds (東 南) */
static void thumb(int v, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    if (v >= 0) {
        int n = v == M4_FULL ? 2 : 1, fw = (int)(size * (n == 1 ? 0.42f : 0.36f)), fh = face_h(fw);
        int gap = (int)(size * 0.06f), total = n * fw + (n - 1) * gap;
        for (int i = 0; i < n; i++)
            blend_tile(rgba, size, M4_WINDS + i, (size - total) / 2 + i * (fw + gap), (size - fh) / 2, fw);
        return;
    }
    int fw = (int)(size * 0.22f), fh = face_h(fw), step = fw + (int)(size * 0.01f) + 1;
    int x0 = (size - 3 * step) / 2, y0 = (int)(size * 0.12f);
    for (int i = 0; i < 3; i++) blend_tile(rgba, size, M4_WINDS, x0 + i * step, y0, fw);
    int y1 = y0 + fh + (int)(size * 0.1f), x1 = (size - 4 * step - (int)(size * 0.05f)) / 2;
    for (int i = 0; i < 3; i++) blend_tile(rgba, size, M4_BAMBOO + 2 + i, x1 + i * step, y1, fw);
    blend_tile(rgba, size, M4_DRAGONS, x1 + 3 * step + (int)(size * 0.05f), y1, fw);
}

static const game mahjong4_game = {
    .id = "mahjong4", .order = 307, .variants = M4_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 4, .view = VIEW_HIDDEN, .players_usual = 4,
};
REGISTER_GAME(mahjong4_game)
