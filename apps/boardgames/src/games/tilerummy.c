/*
 * Tile Rummy in RetroStone Board: 2 to 4 players passing the device round (the rules: tilerummy_rules.c,
 * tilerummy.h). Every rack is secret (VIEW_HIDDEN): the device passes after each turn.
 * The view: the table's sets on the green table, laid out in rows (the tiles shrink as the table fills); under it a
 * strip with the pool, the first meld's worth so far, and three buttons (sort the rack, back to the turn's start, end
 * the turn); the player's rack at the bottom, a wooden rack of one or two rows.
 * Moving tiles, by taps: tap tiles to pick them up (from the rack, or from one set), then tap a tile of another set
 * to put them there, the table's empty room (or the place at its end) for a new set, or the rack to take your own
 * tiles back; a second quick tap on a table tile picks up its whole set. Or drag a tile (or the tiles picked up) and
 * drop it there. Sets that are not valid glow red; end turn lights up once the table is right. Undo takes back the
 * last move; the start button takes the whole turn back; the pool draws a tile when the table is as it was.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/tilerummy.h"
#include "shell.h"

#define SLIDE_MS 260
#define REST_MS 300                             /* after the last tile lands: a moment before the device passes */
#define DOUBLE_MS 380                           /* two taps on a table tile this quick: its whole set */

static tgame G;
static int variant, by_number;                  /* the rack sorted by colour (0) or by number */

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static struct {
    box b, table, rack;                         /* the whole table; where the sets go; the wooden rack (its frame) */
    int me;
    float tw;                                   /* a tile's width on the table */
    float rw;                                   /* in the rack */
    float x[TR_TILES], y[TR_TILES];             /* each visible tile's centre (the table, the viewer's rack) */
    int vis[TR_TILES];                          /* 1: on the table, 2: in the viewer's rack */
    box set[TR_MAXSETS];                        /* each set's rectangle */
    float nx, ny;                               /* the place for a new set */
    float px, py, pw;                           /* the pool */
    float mx, my, mr;                           /* the first meld's worth */
    float bx[3], by[3], br;                     /* the buttons: sort, back to the start, end the turn */
} T;
enum { B_SORT, B_RESET, B_END };

static void place_table(float tw, int commit, int *fits)
{
    const tpos *p = &G.pos;
    float th = tw * 1.3f, gap = 0.36f * tw, rg = 0.2f * th, W = T.table.w, x = 0, y = 0;
    *fits = 1;
    for (int k = 0; k < p->nsets; k++) {
        uint8_t t[TR_TILES];
        int n = tr_set(p, k, t, NULL, NULL);
        float w = n * tw;
        if (w > W) *fits = 0;
        if (x > 0 && x + w > W) x = 0, y += th + rg;
        if (commit) {
            T.set[k] = (box){T.table.x + x, T.table.y + y, w, th};
            for (int i = 0; i < n; i++) {
                T.x[t[i]] = T.table.x + x + (i + 0.5f) * tw;
                T.y[t[i]] = T.table.y + y + th / 2;
                T.vis[t[i]] = 1;
            }
        }
        x += w + gap;
    }
    if (x > 0 && x + tw > W) x = 0, y += th + rg;
    if (commit) T.nx = T.table.x + x + tw / 2, T.ny = T.table.y + y + th / 2;
    if (y + th > T.table.h) *fits = 0;
}

static void place_rack(void)
{
    uint8_t t[TR_TILES];
    int n = tr_rack(&G.pos, T.me, by_number, t);
    box r = T.rack;
    float fr = 0.16f * ui.unit, iw = r.w - 2 * fr - 0.2f * ui.unit, ih = r.h - 2 * fr - 0.1f * ui.unit;
    int k = n > 0 ? n : 1;
    float w1 = fminf(iw / (k * 1.02f), ih / 1.3f), w2 = fminf(iw / (((k + 1) / 2) * 1.02f), ih / (2 * 1.3f + 0.1f));
    int rows = n > 1 ? 2 : 1;                   /* two rows, as on a real rack */
    (void)w1;
    T.rw = floorf(fminf(rows == 2 ? w2 : w1, 0.98f * ui.unit));
    int per = rows == 2 ? (n + 1) / 2 : n;
    float rh = T.rw * 1.3f, top = r.y + r.h / 2 - (rows * rh + (rows - 1) * 0.1f * T.rw) / 2;
    for (int i = 0; i < n; i++) {
        int row = i / (per ? per : 1), c = i - row * per, in_row = row ? n - per : per;
        T.x[t[i]] = r.x + r.w / 2 + (c - (in_row - 1) / 2.0f) * T.rw * 1.02f;
        T.y[t[i]] = top + rh / 2 + row * (rh + 0.1f * T.rw);
        T.vis[t[i]] = 2;
    }
}

/* everything's place now: the sets (the biggest tiles that fit), the rack */
static void place(void)
{
    memset(T.vis, 0, sizeof T.vis);
    float tw = floorf(fminf(0.86f * ui.unit, T.table.w / 7.5f));
    int fits = 0;
    for (int i = 0; i < 60; i++, tw = floorf(tw * 0.95f)) {
        place_table(tw, 0, &fits);
        if (fits || tw < 12) break;
    }
    T.tw = tw;
    place_table(tw, 1, &fits);
    place_rack();
}

/* ---- moving tiles: each tile glides to where it is drawn now ----------------------------------------------------- */
typedef struct {
    int on, face, face0;
    float x, y, w, x0, y0, w0;
    Uint32 t0;
} sprite;
static sprite spr[TR_TILES];
static int from_pool[TR_TILES];
static Uint32 quiet;

static float ease(Uint32 t0)
{
    Uint32 el = ui_ms() - t0;
    if (el >= SLIDE_MS) return 1;
    float t = (float)el / SLIDE_MS;
    return t * t * (3 - 2 * t);
}

static void spr_now(int t, float *x, float *y, float *w, int *face)
{
    float e = ease(spr[t].t0);
    *x = spr[t].x0 + (spr[t].x - spr[t].x0) * e;
    *y = spr[t].y0 + (spr[t].y - spr[t].y0) * e - sinf(e * 3.14159f) * fminf(0.2f * spr[t].w,
                                                       0.08f * hypotf(spr[t].x - spr[t].x0, spr[t].y - spr[t].y0));
    *w = spr[t].w0 + (spr[t].w - spr[t].w0) * e;
    *face = e < 0.5f ? spr[t].face0 : spr[t].face;
}

static void target(int t, float x, float y, float w)
{
    if (!spr[t].on) {
        if (from_pool[t]) {                     /* drawn: from the pool, turning over */
            from_pool[t] = 0;
            spr[t] = (sprite){1, 1, 0, x, y, w, T.px, T.py, T.pw, ui_ms()};
            return;
        }
        spr[t] = (sprite){1, 1, 1, x, y, w, x, y, w, ui_ms() - SLIDE_MS};
        return;
    }
    if (fabsf(spr[t].x - x) < 0.5f && fabsf(spr[t].y - y) < 0.5f && fabsf(spr[t].w - w) < 0.5f) return;
    float cx, cy, cw;
    int f;
    spr_now(t, &cx, &cy, &cw, &f);
    spr[t] = (sprite){1, 1, f, x, y, w, cx, cy, cw, ui_ms()};
}

static void sync(void)
{
    place();
    for (int t = 0; t < TR_TILES; t++) {
        if (T.vis[t]) target(t, T.x[t], T.y[t], T.vis[t] == 1 ? T.tw : T.rw);
        else spr[t].on = 0;
    }
}

static int busy(void)
{
    for (int t = 0; t < TR_TILES; t++)
        if (spr[t].on && ease(spr[t].t0) < 1) return 1;
    return quiet && ui_ms() < quiet;
}

static void snap(void)
{
    memset(spr, 0, sizeof spr);
    memset(from_pool, 0, sizeof from_pool);
    quiet = 0;
}

static void layout(box b)
{
    int me = ui_bottom();
    if (me < 0 || me >= G.pos.players) me = 0;
    if (me != T.me) snap();
    T.me = me;
    T.b = b;
    float u = ui.unit, m = 0.12f * u;
    int wide = b.w > b.h * 1.05f;
    float rh = fminf(wide ? 2.6f * u : 3.0f * u, b.h * (wide ? 0.34f : 0.27f)), ch = 1.25f * u;
    if (!wide) {                                /* portrait: the table, the strip, the rack */
        T.rack = (box){b.x, b.y + b.h - rh, b.w, rh};
        float sy = T.rack.y - ch / 2 - 0.05f * u;
        T.table = (box){b.x + m, b.y + m, b.w - 2 * m, sy - ch / 2 - b.y - 2 * m};
        T.pw = 0.72f * u;
        T.px = b.x + 0.2f * u + T.pw / 2;
        T.py = sy;
        T.mr = 0.42f * u;
        T.mx = T.px + T.pw / 2 + 1.5f * u;
        T.my = sy;
        T.br = 0.5f * u;
        for (int i = 0; i < 3; i++) T.bx[i] = b.x + b.w - 0.2f * u - T.br - (2 - i) * 1.3f * u, T.by[i] = sy;
    } else {                                    /* landscape: the table; under it the rack, the controls beside it */
        float cw = 4.0f * u;
        T.rack = (box){b.x, b.y + b.h - rh, b.w - cw, rh};
        T.table = (box){b.x + m, b.y + m, b.w - 2 * m, T.rack.y - b.y - 2 * m};
        float cx = b.x + b.w - cw, r1 = T.rack.y + rh * 0.3f, r2 = T.rack.y + rh * 0.74f;
        T.pw = 0.66f * u;
        T.px = cx + 0.9f * u;
        T.py = r1;
        T.mr = 0.42f * u;
        T.mx = cx + 2.9f * u;
        T.my = r1;
        T.br = 0.5f * u;
        for (int i = 0; i < 3; i++) T.bx[i] = cx + 0.75f * u + i * 1.25f * u, T.by[i] = r2;
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static uint8_t sel[TR_TILES];                   /* the tiles picked up */
static struct { int on, moved, tile, what; float x0, y0, x, y; } drag;
static struct { int tile; Uint32 t; } last_tap = {-1, 0};

static void unselect(void) { memset(sel, 0, sizeof sel); }

static int nsel(void)
{
    int n = 0;
    for (int t = 0; t < TR_TILES; t++) n += sel[t];
    return n;
}

static void start(int v)
{
    variant = v == TR_MATCH ? TR_MATCH : TR_ONE;
    tr_new(&G, variant, ui.players, (uint32_t)ui_random(1 << 30));
    unselect();
    snap();
    T.me = -1;
}

static int status(void)
{
    if (G.pos.phase != TP_OVER) return GS_PLAY;
    unsigned w = tr_winners(&G.pos);
    if (w & (w - 1)) return GS_DRAW;
    for (int q = 0; q < G.pos.players; q++)
        if (w >> q & 1) return gs_win(q);
    return GS_DRAW;
}

static unsigned winners(void) { return tr_winners(&G.pos); }
static int turn(void) { return G.pos.turn; }
static int my_turn(void) { return G.pos.phase == TP_PLAY && G.pos.turn == T.me && !ui_handing(); }

/* a move on the table: the tiles glide to their new places */
static void moved(void)
{
    unselect();
    sync();
    ui_sound_later(SND_PLACE, SLIDE_MS - 40);
}

static void put(int dest)
{
    uint8_t t[TR_TILES];
    int n = 0;
    for (int i = 0; i < TR_TILES; i++)
        if (sel[i]) t[n++] = (uint8_t)i;
    if (n && tr_put(&G, t, n, dest)) moved();
    else ui_sound(SND_DENY);
}

static void end_turn(void)
{
    if (!tr_end(&G)) { ui_sound(SND_DENY); return; }
    unselect();
    sync();
    quiet = ui_ms() + REST_MS;
    ui_sound(SND_MOVE);
    if (G.pos.phase == TP_PLAY) ui_handoff(G.pos.turn);
}

static void draw_tile(void)
{
    const tpos *p = &G.pos;
    int me = p->turn;
    if (p->npool) from_pool[p->pool[p->npool - 1]] = 1;
    if (!tr_draw(&G)) { ui_sound(SND_DENY); from_pool[p->pool[p->npool - 1]] = 0; return; }
    unselect();
    for (int t = 0; t < TR_TILES; t++)            /* the drawn tile flies to the rack (still the drawer's view) */
        if (from_pool[t] && G.pos.where[t] == TR_RACK + me) {
            place();
            from_pool[t] = 0;
            spr[t] = (sprite){1, 1, 0, T.x[t], T.y[t], T.rw, T.px, T.py, T.pw, ui_ms()};
        }
    quiet = ui_ms() + SLIDE_MS + REST_MS;
    ui_sound(SND_MOVE);
    if (G.pos.phase == TP_PLAY) ui_handoff(G.pos.turn);
}

static void reset_turn(void)
{
    if (!tr_reset(&G)) { ui_sound(SND_DENY); return; }
    unselect();
    sync();
    ui_sound(SND_TAP);
}

static int undo(void)
{
    if (!tr_undo(&G)) return 0;
    unselect();
    sync();
    return 1;
}

static int script(const char *s)
{
    int ok = tr_script(&G, s);
    unselect();
    snap();
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- the icons (drawn here: the turn's start, the end of the turn, sorting) -------------------------------------- */
static void icon(int which, int size, uint8_t *rgba)
{
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    if (which == B_RESET) {                     /* back to the start: a bar, two arrows pointing at it */
        const float a[] = {0.31f, 0.5f, 0.52f, 0.29f, 0.52f, 0.71f}, b[] = {0.52f, 0.5f, 0.73f, 0.29f, 0.73f, 0.71f};
        art_rect(f, ART_PART, 0.22f, 0.29f, 0.29f, 0.71f);
        art_poly(f, ART_PART, a, 3);
        art_poly(f, ART_PART, b, 3);
    } else if (which == B_END) {                /* the turn done: a tick */
        const float tick[] = {0.19f, 0.52f, 0.29f, 0.42f, 0.43f, 0.56f, 0.72f, 0.26f, 0.82f, 0.36f, 0.43f, 0.76f};
        art_poly(f, ART_PART, tick, 6);
    } else {                                    /* sorting: three bars, short to tall, an arrow under them */
        art_rect(f, ART_PART, 0.22f, 0.50f, 0.34f, 0.66f);
        art_rect(f, ART_PART, 0.44f, 0.38f, 0.56f, 0.66f);
        art_rect(f, ART_PART, 0.66f, 0.24f, 0.78f, 0.66f);
        const float arrow[] = {0.22f, 0.74f, 0.70f, 0.74f, 0.70f, 0.70f, 0.80f, 0.765f, 0.70f, 0.83f, 0.70f, 0.79f,
                               0.22f, 0.79f};
        art_poly(f, ART_PART, arrow, 7);
    }
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_icon[3], *t_disc, *t_disc_on, *t_disc_off, *t_rack, *t_piece;
static int tok_size, icon_d, rack_w, rack_h, piece_d;

static void controls_textures(void)
{
    int d = (int)(2 * T.br);
    if (icon_d == d) return;
    icon_d = d;
    static const uint8_t btn[3] = {54, 72, 64}, on[3] = {150, 118, 46}, dark[3] = {28, 34, 32};
    for (int i = 0; i < 3; i++) {
        ui_free(&t_icon[i]);
        icon(i, d, ui_buf(d, d));
        t_icon[i] = ui_tex(d, d, ui_buf(d, d));
    }
    ui_free(&t_disc);
    art_disc(d, btn, 1, 0, ui_buf(d, d));
    t_disc = ui_tex(d, d, ui_buf(d, d));
    ui_free(&t_disc_on);
    art_disc(d, on, 1, 0, ui_buf(d, d));
    t_disc_on = ui_tex(d, d, ui_buf(d, d));
    ui_free(&t_disc_off);
    art_disc(d, dark, 0.85f, 0, ui_buf(d, d));
    t_disc_off = ui_tex(d, d, ui_buf(d, d));
}

static void button(int i, int state)            /* state: 0 off (dim), 1 on, 2 lit (gold) */
{
    float x = T.bx[i], y = T.by[i], r = T.br;
    if (state == 2) ui_mark_gold(x, y, 3.0f * r);
    ui_put_c(state == 2 ? t_disc_on : t_disc, x, y, 2 * r, 2 * r);
    SDL_SetTextureAlphaMod(t_icon[i], state ? 255 : 90);
    ui_put_c(t_icon[i], x, y, 1.24f * r, 1.24f * r);
}

static void draw_rack(void)
{
    box r = T.rack;
    int w = (int)r.w, h = (int)r.h;
    if (rack_w != w || rack_h != h) {           /* the house wood, a walnut frame, stretched to the rack */
        rack_w = w;
        rack_h = h;
        int fr = (int)(0.16f * ui.unit), sq = h - 2 * fr, nx = (int)((w - 2 * fr) / (float)sq + 0.5f);
        if (nx < 1) nx = 1;
        int tw = nx * sq + 2 * fr;
        uint8_t *b = malloc((size_t)tw * h * 4);
        if (b) {
            art_board_n(nx, 1, sq, fr, 0, b);
            ui_free(&t_rack);
            t_rack = ui_tex(tw, h, b);
            free(b);
        }
    }
    ui_put(t_rack, r.x, r.y, r.w, r.h);
}

/* the round's scores: a row a player: their token, the tiles left in their rack, the points of the round, the total
 * (several rounds); the round's winner in gold light */
static void draw_summary(void)
{
    const tpos *p = &G.pos;
    box b = T.b;
    float u = ui.unit, rh = fminf(1.4f * u, b.h * 0.8f / p->players), pw = fminf(b.w * 0.96f, 8.6f * u);
    float ph = rh * p->players + 0.5f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    tok_size = (int)(0.6f * u);
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 70);
    ct_panel(x0, y0, pw, ph, 255);
    int match = p->variant == TR_MATCH;
    for (int q = 0; q < p->players; q++) {
        float cy = y0 + 0.25f * u + (q + 0.5f) * rh, tx = x0 + 0.6f * u, d = fminf(0.7f * rh, 0.8f * u);
        if (q == p->winner) ui_mark_gold(tx, cy, 2.0f * d);
        ui_put_c(ct_token(q, tok_size), tx, cy, d, d);
        uint8_t t[TR_TILES];
        int n = tr_rack(p, q, 0, t);
        float room = pw * (match ? 0.56f : 0.68f) - 1.2f * u, tw = fminf(0.5f * rh, 0.55f * u);
        float step = n > 1 ? fminf(tw * 1.02f, (room - tw) / (n - 1)) : 0, x = x0 + 1.2f * u + tw / 2;
        for (int i = 0; i < n; i++) cards_draw(DECK_TILES, tr_face(t[i]), x + i * step, cy, tw, 0, CD_FLAT);
        ui_number_signed(x0 + pw * (match ? 0.7f : 0.84f), cy, 0.4f * rh, p->last[q], 1, 1);
        if (match) ui_number_signed(x0 + pw * 0.88f, cy, 0.5f * rh, p->total[q], 1, 0);
        if (q < p->players - 1)
            ct_rule(x0 + 0.3f * u, cy + rh / 2, pw - 0.6f * u);
    }
}

static int dragging(int t) { return drag.on && drag.moved && (t == drag.tile || (sel[drag.tile] && sel[t])); }

/* the set the dragged tiles would go to (-1 none; TR_NEWSET the table's free room; TR_TORACK the rack) */
static int drop_target(float x, float y)
{
    const tpos *p = &G.pos;
    float m = 0.18f * T.tw;
    for (int k = 0; k < p->nsets; k++) {
        box s = T.set[k];
        if (x >= s.x - m && x < s.x + s.w + m && y >= s.y - m && y < s.y + s.h + m) return k;
    }
    box r = T.rack;
    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return TR_TORACK;
    box t = T.table;
    if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h) return TR_NEWSET;
    return -1;
}

static void draw(void)
{
    const tpos *p = &G.pos;
    sync();
    controls_textures();
    int mine = my_turn(), b = busy(), changed = tr_changed(&G);
    /* the sets: the invalid ones in a red glow; the tiles laid this turn (or, untouched, the last turn) warm */
    for (int k = 0; k < p->nsets; k++) {
        uint8_t t[TR_TILES];
        int kind, n = tr_set(p, k, t, &kind, NULL);
        if (kind == TS_BAD && !b) {             /* not a set: a red light round it */
            box s = T.set[k];
            float m = 0.1f * T.tw;
            for (int i = 0; i < n; i++)
                if (!dragging(t[i])) ui_mark_glow(T.x[t[i]], T.y[t[i]], 2.3f * T.tw);
            ui_fill(s.x - m, s.y - m, s.w + 2 * m, s.h + 2 * m, 214, 52, 36, 150);
        }
        for (int i = 0; i < n; i++) {
            if (dragging(t[i])) continue;
            float x, y, w;
            int f;
            spr_now(t[i], &x, &y, &w, &f);
            int warm = changed ? tr_mine(&G, t[i]) : p->lastplay[t[i]];
            if (warm) ui_mark_last(x - 0.53f * w, y - 0.68f * w, 1.06f * w, 1.36f * w);
        }
    }
    if (drag.on && drag.moved) {                /* the set the tiles would join */
        int k = drop_target(drag.x, drag.y);
        if (k >= 0 && k < p->nsets) {
            box s = T.set[k];
            ui_mark_select(s.x - 0.1f * T.tw, s.y - 0.1f * T.tw, s.w + 0.2f * T.tw, s.h + 0.2f * T.tw);
        } else if (k == TR_NEWSET) ui_mark_select(T.nx - 0.6f * T.tw, T.ny - 0.75f * T.tw, 1.2f * T.tw, 1.5f * T.tw);
    }
    if (mine && nsel()) cards_place(DECK_TILES, T.nx, T.ny, T.tw);   /* the place for a new set */
    for (int t = 0; t < TR_TILES; t++) {
        if (T.vis[t] != 1 || dragging(t)) continue;
        float x, y, w;
        int f;
        spr_now(t, &x, &y, &w, &f);
        cards_draw(DECK_TILES, f ? tr_face(t) : CARD_NONE, x, y, w, 0, (f ? 0 : CD_DOWN) | (sel[t] ? CD_RAISED : 0));
    }
    /* the strip: the pool, the first meld's worth, the buttons */
    int can_draw = mine && !b && !changed;
    cards_pile(DECK_TILES, T.px, T.py, T.pw, p->npool, CARD_NONE, can_draw ? CD_GLOW : 0);
    ui_number(T.px + T.pw * 0.5f + 0.42f * ui.unit, T.py + 0.2f * ui.unit, 0.36f * ui.unit, p->npool, 1);
    if (p->phase == TP_PLAY && !p->out[T.me]) {
        int v = tr_meld(&G), ok = v >= TR_FIRST;
        if (ok) ui_mark_gold(T.mx, T.my, 2.6f * T.mr);
        ui_put_c(ok ? t_disc_on : t_disc_off, T.mx, T.my, 2 * T.mr, 2 * T.mr);
        ui_number(T.mx, T.my, 0.8f * T.mr, v, 1);
    }
    int check = tr_check(&G);
    button(B_SORT, 1);
    button(B_RESET, mine && changed);
    button(B_END, mine && check == TC_OK ? 2 : 0);
    /* the rack (under the hand-off cover: face down) */
    draw_rack();
    for (int t = 0; t < TR_TILES; t++) {
        if (T.vis[t] != 2 || dragging(t)) continue;
        float x, y, w;
        int f;
        spr_now(t, &x, &y, &w, &f);
        if (ui.viewer < 0) f = 0;
        cards_draw(DECK_TILES, f ? tr_face(t) : CARD_NONE, x, y, w, 0, (f ? 0 : CD_DOWN) | (sel[t] ? CD_RAISED : 0));
    }
    for (int t = 0; t < TR_TILES; t++)          /* tiles on their way to elsewhere (back to the pool: none) */
        if (spr[t].on && !T.vis[t]) spr[t].on = 0;
    if (drag.on && drag.moved && ui.viewer >= 0) {   /* the tiles in the hand, under the finger */
        int list[TR_TILES], n = 0;
        for (int t = 0; t < TR_TILES; t++)
            if (dragging(t)) list[n++] = t;
        float w = T.tw > 0.6f * ui.unit ? T.tw : 0.6f * ui.unit;
        for (int i = 0; i < n; i++)
            cards_draw(DECK_TILES, tr_face(list[i]), drag.x + (i - (n - 1) / 2.0f) * w, drag.y - 0.5f * w, w, 0,
                       CD_RAISED);
    }
    if ((p->phase == TP_ROUND || p->phase == TP_OVER) && !b) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
enum { W_NONE, W_TILE, W_TABLE, W_RACK, W_POOL, W_SORT, W_RESET, W_END };

static int tile_at(float x, float y)
{
    for (int t = 0; t < TR_TILES; t++) {
        if (!T.vis[t]) continue;
        float w = T.vis[t] == 1 ? T.tw : T.rw;
        if (fabsf(x - T.x[t]) <= w * 0.52f && fabsf(y - T.y[t]) <= w * 0.67f) return t;
    }
    return -1;
}

static int what_at(float x, float y, int *tile)
{
    *tile = tile_at(x, y);
    if (*tile >= 0) return W_TILE;
    for (int i = 0; i < 3; i++)
        if (hypotf(x - T.bx[i], y - T.by[i]) <= T.br * 1.1f) return W_SORT + i;
    if (fabsf(x - T.px) <= T.pw * 0.7f && fabsf(y - T.py) <= T.pw * 0.85f) return W_POOL;
    box r = T.rack, t = T.table;
    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return W_RACK;
    if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h) return W_TABLE;
    return W_NONE;
}

static int movable(int t)
{
    return tr_can_put(&G, t, TR_NEWSET) || tr_can_put(&G, t, TR_TORACK);
}

/* a tap on a tile: picked up or put down; on a set while tiles from elsewhere are held: they go there */
static void tap_tile(int t)
{
    const tpos *p = &G.pos;
    int w = p->where[t], on_table = w < p->nsets;
    if (on_table && nsel()) {
        int same = 0;
        for (int i = 0; i < TR_TILES; i++) same |= sel[i] && p->where[i] == w;
        if (!same && !sel[t]) { put(w); return; }
    }
    Uint32 now = ui_ms();
    if (on_table && last_tap.tile == t && now - last_tap.t < DOUBLE_MS) {   /* a quick second tap: the whole set */
        for (int i = 0; i < TR_TILES; i++)
            if (p->where[i] == w && movable(i)) sel[i] = 1;
        last_tap.tile = -1;
        ui_sound(SND_TAP);
        return;
    }
    last_tap.tile = t;
    last_tap.t = now;
    if (sel[t]) { sel[t] = 0; ui_sound(SND_TAP); return; }
    if (!movable(t)) { ui_sound(SND_DENY); return; }
    if (on_table)                               /* tiles from one set at a time (and the rack's) */
        for (int i = 0; i < TR_TILES; i++)
            if (sel[i] && p->where[i] < p->nsets && p->where[i] != w) sel[i] = 0;
    sel[t] = 1;
    ui_sound(SND_TAP);
}

/* every tile held is there already (a set, or the player's rack) */
static int all_at(int dest)
{
    int w = dest == TR_TORACK ? TR_RACK + G.pos.turn : dest;
    for (int t = 0; t < TR_TILES; t++)
        if (sel[t] && G.pos.where[t] != w) return 0;
    return 1;
}

static void tap(int what, int t)
{
    switch (what) {
    case W_TILE: tap_tile(t); break;
    case W_TABLE:
        if (nsel()) put(TR_NEWSET);
        break;
    case W_RACK:                                /* the player's own tiles back; else nothing held any more */
        if (nsel() && !all_at(TR_TORACK)) put(TR_TORACK);
        else unselect();
        break;
    case W_POOL: draw_tile(); break;
    case W_SORT:
        by_number = !by_number;
        ui_sound(SND_TAP);
        sync();
        break;
    case W_RESET: reset_turn(); break;
    case W_END: end_turn(); break;
    }
}

static void press(float x, float y)
{
    const tpos *p = &G.pos;
    drag.on = 0;
    place();                                    /* where everything is (no frame may have been drawn since) */
    if (p->phase == TP_ROUND) {
        if (!busy() && tr_next(&G)) {
            unselect();
            snap();
            ui_handoff(G.pos.turn);
        }
        return;
    }
    if (p->phase != TP_PLAY || busy()) return;
    int t, what = what_at(x, y, &t);
    if (!my_turn()) {
        if (what == W_SORT) tap(what, t);
        else ui_sound(SND_DENY);
        return;
    }
    drag.on = 1;
    drag.moved = 0;
    drag.tile = t;
    drag.what = what;
    drag.x0 = drag.x = x;
    drag.y0 = drag.y = y;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (!drag.moved) {
        if (drag.what != W_TILE || hypotf(x - drag.x0, y - drag.y0) < 0.22f * ui.unit) return;
        if (!movable(drag.tile)) return;
        if (!sel[drag.tile]) {                  /* a tile not held: it alone */
            unselect();
            sel[drag.tile] = 1;
        }
        drag.moved = 1;
    }
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) { tap(drag.what, drag.tile); return; }
    int k = drop_target(x, y);
    float w = T.tw > 0.6f * ui.unit ? T.tw : 0.6f * ui.unit;
    int i = 0, n = nsel();
    for (int t = 0; t < TR_TILES; t++)          /* the tiles leave the finger from where they are */
        if (sel[t] && spr[t].on) {
            spr[t].x = spr[t].x0 = x + (i++ - (n - 1) / 2.0f) * w;
            spr[t].y = spr[t].y0 = y - 0.5f * w;
            spr[t].w = spr[t].w0 = w;
            spr[t].t0 = ui_ms() - SLIDE_MS;
        }
    if (k < 0 || (k != TR_NEWSET && all_at(k))) {   /* nowhere new: back */
        unselect();
        sync();
        return;
    }
    put(k);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void piece(int size)
{
    if (piece_d == size) return;
    piece_d = size;
    ui_free(&t_piece);
    uint8_t *buf = ui_buf(size, size);
    int w = (int)(size / 1.3f), h = size;
    uint8_t *tile = malloc((size_t)w * h * 4);
    memset(buf, 0, (size_t)size * size * 4);
    if (tile) {
        cards_paint(DECK_TILES, CARD_NONE, w, h, tile);
        art_blend(buf, size, size, tile, w, h, (size - w) / 2, 0);
        free(tile);
    }
    t_piece = ui_tex(size, size, buf);
}

static int rack_size(int q)
{
    int n = 0;
    for (int t = 0; t < TR_TILES; t++) n += G.pos.where[t] == TR_RACK + q;
    return n;
}

/* the tiles in the player's rack; several rounds: their total too */
static void bar(int player, box b)
{
    piece((int)(0.6f * ui.unit));
    float u = ui.unit;
    if (G.pos.variant == TR_MATCH) {
        int v = G.pos.total[player], digits = 1;
        for (int a = abs(v); a >= 10; a /= 10) digits++;
        float h = fminf(0.42f * u, b.h * 0.5f), w = (0.74f * (digits - 1) + 0.62f + (v < 0 ? 0.56f : 0)) * h;
        ui_number_signed(b.x + w / 2, b.y + b.h / 2, h, v, 1, 0);
        b.x += w + 0.3f * u;
        b.w -= w + 0.3f * u;
    }
    if (b.w > 1.0f * u) ui_bar_count(b, t_piece, rack_size(player));
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

static void blend_tile(uint8_t *rgba, int size, int face, float cx, float cy, float w)
{
    int tw = (int)w, th = (int)(w * 1.3f);
    uint8_t *b = malloc((size_t)tw * th * 4);
    if (!b) return;
    cards_paint(DECK_TILES, face, tw, th, b);
    art_blend(rgba, size, size, b, tw, th, (int)(cx - tw / 2.0f), (int)(cy - th / 2.0f));
    free(b);
}

/* the menu's card and the picker's: a run with a joker and a group; several rounds: tokens stacked beside */
static void thumb(int v, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    float w = size * 0.23f, y1 = size * 0.31f, y2 = size * 0.69f;
    static const int run[4] = {CARD(0, 9), CARD(0, 10), CARD(SUIT_JOKER, 0), CARD(0, 12)};
    static const int group[3] = {CARD(1, 7), CARD(2, 7), CARD(3, 7)};
    for (int i = 0; i < 4; i++) blend_tile(rgba, size, run[i], size / 2.0f + (i - 1.5f) * w, y1, w);
    float gx = v == TR_MATCH ? size * 0.36f : size / 2.0f;
    for (int i = 0; i < 3; i++) blend_tile(rgba, size, group[i], gx + (i - 1) * w, y2, w);
    if (v == TR_MATCH) {
        int d = (int)(size * 0.17f);
        uint8_t *b = malloc((size_t)d * d * 4);
        if (!b) return;
        for (int i = 0; i < 3; i++) {
            cards_token(2 - i, d, b);
            int x = (int)(size * 0.86f - d / 2), y = (int)(y2 + size * 0.06f - d / 2 - i * d * 0.32f);
            art_blend(rgba, size, size, b, d, d, x, y);
        }
        free(b);
    }
}

static const game tilerummy_game = {
    .id = "tilerummy", .order = 265, .variants = TR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(tilerummy_game)
