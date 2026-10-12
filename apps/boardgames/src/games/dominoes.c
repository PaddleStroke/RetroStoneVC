/*
 * Dominoes in RetroStone Board: the double-six set for 2 to 4 players passing the device round (the rules:
 * dominoes_rules.c, dominoes.h). Every hand is secret (VIEW_HIDDEN): the device passes after each turn. The view: the
 * line of play on the green table, snaking round at the table's edges (the spinner's four arms going round like a
 * pinwheel), the player's hand standing at the bottom, the boneyard on its left, the open ends' count on its right
 * (All Fives). Tap a bone, then one of the glowing places at the ends it fits (or drag it there); tap the boneyard to
 * draw. The bones are drawn here: ivory faces with ebony pips riveted on ebony backs, a brass pin in the middle.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "cards_table.h"
#include "games/dominoes.h"
#include "shell.h"

#define SLIDE_MS 320
#define REST_MS 350                             /* after the bone lands: a moment before the device passes */
#define POP_MS 1300                             /* All Fives: the points scored, rising */
#define PI_F 3.14159265f

static dgame G;
static int variant, turn_n;                     /* turn_n: the actions at the last hand-off (undo stops there) */

/* ---- the bones' pictures ----------------------------------------------------------------------------------------- */
static const art_style IVORY = {{244, 236, 218}, {60, 48, 40}, {120, 104, 90}, {30, 26, 24}, 0.22f, 0};
static const art_style EBONY = {{58, 52, 50}, {14, 12, 12}, {120, 110, 104}, {30, 26, 24}, 0.35f, 0};
static const art_style BRASS = {{206, 162, 70}, {92, 62, 22}, {120, 90, 40}, {255, 236, 170}, 0.65f, 0};

/* a shape in the bone's own frame (horizontal: u 0..2, v 0..1) mapped to the picture (vertical: turned a quarter) */
typedef struct { int vertical; } frame_t;
static void map(const frame_t *fr, float u, float v, float *x, float *y)
{
    if (fr->vertical) *x = 1 - v, *y = u;
    else *x = u, *y = v;
}

static void rrect(art_fig *f, const frame_t *fr, int role, float u0, float v0, float u1, float v1, float r)
{
    float p[2 * 44];
    int n = 0;
    for (int c = 0; c < 4; c++) {
        float cu = c == 0 || c == 3 ? u1 - r : u0 + r, cv = c < 2 ? v1 - r : v0 + r;
        for (int i = 0; i <= 10; i++) {
            float a = (c * 90 + i * 9) * PI_F / 180;
            map(fr, cu + r * cosf(a), cv + r * sinf(a), &p[n], &p[n + 1]);
            n += 2;
        }
    }
    art_poly(f, role, p, n / 2);
}

static void quad(art_fig *f, const frame_t *fr, int role, float u0, float v0, float u1, float v1)
{
    float p[8];
    map(fr, u0, v0, &p[0], &p[1]);
    map(fr, u1, v0, &p[2], &p[3]);
    map(fr, u1, v1, &p[4], &p[5]);
    map(fr, u0, v1, &p[6], &p[7]);
    art_poly(f, role, p, 4);
}

static void render_blend(art_fig *f, art_style st, int w, int h, float scale, uint8_t *out)
{
    uint8_t *tmp = malloc((size_t)w * h * 4);
    if (!tmp) return;
    st.line = fmaxf(0.02f, 1.2f / scale);
    art_render(f, &st, w, h, scale, 0, 0, tmp);
    art_blend(out, w, h, tmp, w, h, 0, 0);
    free(tmp);
}

/* a bone a | b (a on the left, or on top when vertical), face up or its back, w x h pixels (2:1 or 1:2) */
static void paint_bone(int a, int b, int face, int vertical, int w, int h, uint8_t *rgba)
{
    static const float pip[7][6][2] = {         /* a half's pips (u across the bone's length, v along its width) */
        {{0}}, {{0.5f, 0.5f}}, {{0.27f, 0.27f}, {0.73f, 0.73f}}, {{0.25f, 0.25f}, {0.5f, 0.5f}, {0.75f, 0.75f}},
        {{0.27f, 0.27f}, {0.73f, 0.27f}, {0.27f, 0.73f}, {0.73f, 0.73f}},
        {{0.25f, 0.25f}, {0.75f, 0.25f}, {0.5f, 0.5f}, {0.25f, 0.75f}, {0.75f, 0.75f}},
        {{0.24f, 0.27f}, {0.5f, 0.27f}, {0.76f, 0.27f}, {0.24f, 0.73f}, {0.5f, 0.73f}, {0.76f, 0.73f}}};
    frame_t fr = {vertical};
    float scale = vertical ? (float)w : (float)h, t = 0.075f, fu = 2 - t, fv = 1 - t;
    memset(rgba, 0, (size_t)w * h * 4);
    art_fig *f = art_fig_new();                 /* the ebony back, its edge showing at the lower right */
    rrect(f, &fr, ART_PART, 0, 0, 2, 1, 0.16f);
    render_blend(f, EBONY, w, h, scale, rgba);
    art_fig_free(f);
    f = art_fig_new();                          /* the face: ivory (or ebony: the back), the pips, the line */
    rrect(f, &fr, ART_PART, 0, 0, fu, fv, 0.15f);
    if (face) {
        quad(f, &fr, ART_MARK, fu / 2 - 0.016f, 0.13f, fu / 2 + 0.016f, fv - 0.13f);
        for (int half = 0; half < 2; half++) {
            int v = half ? b : a;
            float u0 = half * fu / 2, hw = fu / 2;
            for (int i = 0; i < v && v <= 6; i++) {
                float x, y;
                map(&fr, u0 + (0.04f + 0.92f * pip[v][i][0]) * hw, (0.04f + 0.92f * pip[v][i][1]) * fv, &x, &y);
                art_ellipse(f, ART_MARK, x, y, 0.082f, 0.082f);
            }
        }
    }
    render_blend(f, face ? IVORY : EBONY, w, h, scale, rgba);
    art_fig_free(f);
    f = art_fig_new();                          /* the brass pin */
    float x, y;
    map(&fr, fu / 2, fv / 2, &x, &y);
    art_ellipse(f, ART_PART, x, y, face ? 0.075f : 0.1f, face ? 0.075f : 0.1f);
    art_ellipse(f, ART_MARK, x - 0.022f, y - 0.022f, 0.022f, 0.022f);
    render_blend(f, BRASS, w, h, scale, rgba);
    art_fig_free(f);
}

/* the textures: a bone lying horizontally (the lower half on the left), len pixels long; its back (a -1) */
#define CACHE 96
typedef struct { int key, len; unsigned used; SDL_Texture *t; } cached;
static cached cache[CACHE];
static unsigned tick;

static SDL_Texture *bone_tex(int a, int b, int len)
{
    if (len < 6) return NULL;
    int key = a < 0 ? 99 : a * 10 + b, slot = 0;
    for (int i = 0; i < CACHE; i++) {
        if (cache[i].t && cache[i].key == key && cache[i].len == len) { cache[i].used = ++tick; return cache[i].t; }
        if (!cache[i].t || (cache[slot].t && cache[i].used < cache[slot].used)) slot = i;
    }
    ui_free(&cache[slot].t);
    int h = len / 2;
    uint8_t *buf = malloc((size_t)len * h * 4);
    if (!buf) return NULL;
    paint_bone(a < 0 ? 0 : a, b, a >= 0, 0, len, h, buf);
    cache[slot] = (cached){key, len, ++tick, ui_tex(len, h, buf)};
    free(buf);
    return cache[slot].t;
}

enum { BD_DIM = 1, BD_FLAT = 2, BD_RAISED = 4 };
/* a bone centred on (cx, cy), len long, turned deg (0: lying, a on the left), face up (else its back) */
static void draw_bone(int a, int b, float cx, float cy, float len, float deg, int face, int how)
{
    if (face && a > b) {
        int t = a;
        a = b, b = t;
        deg += 180;
    }
    SDL_Texture *t = bone_tex(face ? a : -1, face ? b : 0, (int)(len + 0.5f));
    if (!t) return;
    if (how & BD_RAISED) cy -= 0.12f * len;
    if (!(how & BD_FLAT)) {                     /* its shadow: the bone in black, faint, to the lower right */
        float off = (how & BD_RAISED ? 0.09f : 0.035f) * len;
        SDL_SetTextureColorMod(t, 0, 0, 0);
        SDL_SetTextureAlphaMod(t, 70);
        ui_put_rot(t, cx + 0.6f * off, cy + off, len, len / 2, deg);
        SDL_SetTextureAlphaMod(t, 255);
    }
    if (how & BD_DIM) SDL_SetTextureColorMod(t, 150, 150, 150);
    else SDL_SetTextureColorMod(t, 255, 255, 255);
    ui_put_rot(t, cx, cy, len, len / 2, deg);
    SDL_SetTextureColorMod(t, 255, 255, 255);
}

/* ---- the table's geometry ---------------------------------------------------------------------------------------- */
typedef struct { float x, y, hw, hh; } rect;    /* a centre and half sizes */
typedef struct { int bone, a, b; float x, y, deg; } spot;   /* a bone on the line: its pips left, right; turned */
static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};   /* east, south, west, north */
static const int ARM_DIR[ENDS] = {2, 0, 3, 1};  /* END_W goes west, END_E east, END_N north, END_S south */

static struct {
    box b;
    int me;
    rect line;                                  /* where the line goes */
    float L;                                    /* a bone's length on the line */
    spot sp[DOM_BONES];
    int nsp;
    float ex[ENDS], ey[ENDS];                   /* each arm's end: where its next bone joins, its direction */
    int ed[ENDS];
    float hs, hy0, hstep;                       /* the hand: a bone's width (standing: twice as tall), rows */
    int per;                                    /* bones a row */
    float hx0;
    float yx, yy, yl;                           /* the boneyard: its middle, a bone's length there */
    float cx, cy, cr;                           /* All Fives: the count of the open ends */
    int made;                                   /* the line laid out for this many actions (-1: again) */
} T;

static float room(float x, float y, int d, const rect *R)
{
    switch (d) {
    case 0: return R->x + R->hw - x;
    case 1: return R->y + R->hh - y;
    case 2: return x - (R->x - R->hw);
    default: return y - (R->y - R->hh);
    }
}

/* the next bone of an arm from (x, y) going d: straight on (a double across the line), or round a corner when the
 * edge is near (the bone's first half straight on, the second turned clockwise). Its spot and rectangle; the arm's
 * end moves on. */
static void step(float *x, float *y, int *d, int dbl, float L, const rect *R, spot *s, rect *r)
{
    float h = L / 2, len = dbl ? h : L;
    if (room(*x, *y, *d, R) >= len + h) {
        int k = *d;
        s->x = *x + DX[k] * len / 2;
        s->y = *y + DY[k] * len / 2;
        float along = len / 2, across = dbl ? L / 2 : h / 2;
        *r = (rect){s->x, s->y, DX[k] ? along : across, DX[k] ? across : along};
        s->deg = dbl ? k * 90.0f + 90 : k * 90.0f;
        *x += DX[k] * len;
        *y += DY[k] * len;
        return;
    }
    int k = *d, n = (k + 1) & 3;
    float ax = *x + DX[k] * h / 2, ay = *y + DY[k] * h / 2;
    s->x = ax + DX[n] * h / 2;
    s->y = ay + DY[n] * h / 2;
    *r = (rect){s->x, s->y, DX[n] ? h : h / 2, DX[n] ? h / 2 : h};
    s->deg = n * 90.0f;
    *x = ax + DX[n] * 1.5f * h;
    *y = ay + DY[n] * 1.5f * h;
    *d = n;
}

static int inside(const rect *a, const rect *R)
{
    return a->x - a->hw >= R->x - R->hw - 0.5f && a->x + a->hw <= R->x + R->hw + 0.5f &&
           a->y - a->hh >= R->y - R->hh - 0.5f && a->y + a->hh <= R->y + R->hh + 0.5f;
}

static int overlap(const rect *a, const rect *b, float gap)
{
    return fabsf(a->x - b->x) < a->hw + b->hw + gap && fabsf(a->y - b->y) < a->hh + b->hh + gap;
}

/* the line at bone length L; 1: it fits (inside, nothing over anything, room for a bone at each open end) */
static int try_line(float L)
{
    const dpos *p = &G.pos;
    rect rr[DOM_BONES + ENDS];
    int n = 0, chain[DOM_BONES + ENDS], at[DOM_BONES + ENDS];    /* each bone's arm (-1 the middle), its place */
    T.nsp = 0;
    if (p->center == DOM_NONE) return 1;
    rect *R = &T.line;
    float cx = R->x, cy = R->y, h = L / 2;
    int dbl = p->cw == p->ce;
    spot *c = &T.sp[T.nsp++];
    *c = (spot){p->center, p->cw, p->ce, cx, cy, dbl ? 90.0f : 0.0f};
    chain[n] = -1, at[n] = 0;
    rr[n++] = dbl ? (rect){cx, cy, h / 2, h} : (rect){cx, cy, h, h / 2};
    for (int e = 0; e < ENDS; e++) {
        int d = ARM_DIR[e];
        float x = cx + DX[d] * (dbl || e >= END_N ? (DX[d] ? h / 2 : h) : h), y = cy + DY[d] * (dbl ? h : h / 2);
        if (e >= END_N && !p->spinner) continue;
        int inner = e == END_E ? p->ce : p->cw, corners = 0, since = 0;
        for (int i = 0; i < p->narm[e]; i++) {
            int b = p->arm[e][i], o = p->out[e][i], d0 = d;
            spot *s = &T.sp[T.nsp++];
            chain[n] = e, at[n] = i + 1;
            step(&x, &y, &d, dom_double(b), L, R, s, &rr[n++]);
            if (d != d0) {                      /* a corner: not one right after another (a tight spiral) */
                if (corners++ && since < 2 && L > 0.5f * ui.unit) return 0;
                since = 0;
            } else since++;
            s->bone = b;
            s->a = inner;
            s->b = o;
            inner = o;
        }
        T.ex[e] = x, T.ey[e] = y, T.ed[e] = d;
        if (dom_open(p, e)) {                   /* room for the next bone */
            spot g;
            float gx = x, gy = y;
            int gd = d;
            chain[n] = e, at[n] = p->narm[e] + 1;
            step(&gx, &gy, &gd, 0, L, R, &g, &rr[n++]);
        }
    }
    for (int i = 0; i < n; i++) {
        if (!inside(&rr[i], R)) return 0;
        for (int j = 0; j < i; j++) {          /* neighbours touch; the others keep apart */
            int next = (chain[i] == chain[j] && (at[i] - at[j] == 1 || at[j] - at[i] == 1)) ||
                       (chain[j] < 0 && at[i] == 1);
            if (overlap(&rr[i], &rr[j], next ? -1 : 0.14f * L)) return 0;
        }
    }
    return 1;
}

static void line_layout(void)
{
    float L = floorf(fminf(fminf(1.9f * ui.unit, 2 * T.line.hw / 5.2f), 2 * T.line.hh / 3.2f));
    for (int i = 0; i < 80 && !try_line(L); i++) L = fmaxf(8, floorf(L * 0.95f));
    T.L = L;
    T.made = G.n;
}

/* the place a bone would go at an end (the first bone: the middle) */
static void ghost(int e, int bone, spot *s, rect *r)
{
    float L = T.L;
    if (e == END_C) {
        int dbl = dom_double(bone);
        *s = (spot){bone, dom_lo(bone), dom_hi(bone), T.line.x, T.line.y, dbl ? 90.0f : 0.0f};
        *r = dbl ? (rect){s->x, s->y, L / 4, L / 2} : (rect){s->x, s->y, L / 2, L / 4};
        return;
    }
    float x = T.ex[e], y = T.ey[e];
    int d = T.ed[e];
    step(&x, &y, &d, dom_double(bone), L, &T.line, s, r);
}

static int hand_order(uint8_t *o)              /* the viewer's hand, by its pips */
{
    const dpos *p = &G.pos;
    int n = p->nhand[T.me];
    memcpy(o, p->hand[T.me], (size_t)n);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && o[j] < o[j - 1]; j--) {
            uint8_t t = o[j];
            o[j] = o[j - 1], o[j - 1] = t;
        }
    return n;
}

static void hand_layout(void)
{
    const dpos *p = &G.pos;
    box b = T.b;
    float u = ui.unit, side = 1.7f * u, hh = fminf(2.5f * u, b.h * 0.3f), aw = b.w - 2 * side;
    int n = p->nhand[T.me] > 0 ? p->nhand[T.me] : 1, rows = 1;
    T.hs = fminf(0.95f * u, fminf(aw / (n * 1.12f), hh / 2.35f));
    if (T.hs < 0.62f * u && n > 7) {            /* many bones: two rows */
        rows = 2;
        int per = (n + 1) / 2;
        T.hs = fminf(0.95f * u, fminf(aw / (per * 1.12f), hh / 4.5f));
    }
    T.per = rows == 2 ? (n + 1) / 2 : n;
    T.hstep = T.hs * 1.12f;
    T.hx0 = b.x + side + aw / 2;
    float top = b.y + b.h - hh;
    T.hy0 = top + hh / 2 - (rows - 1) * T.hs * 1.08f;
    T.yl = fminf(1.3f * u, side * 0.85f);
    T.yx = b.x + side / 2;
    T.yy = top + hh / 2 - 0.25f * u;
    T.cr = fminf(0.62f * u, side * 0.4f);
    T.cx = b.x + b.w - side / 2;
    T.cy = top + hh / 2;
    T.line = (rect){b.x + b.w / 2, b.y + (b.h - hh) / 2, b.w / 2 - 0.15f * u, (b.h - hh) / 2 - 0.15f * u};
}

static void hand_spot(int i, int n, float *x, float *y)
{
    int row = i / T.per, in_row = row ? n - T.per : (n < T.per ? n : T.per), c = i % T.per;
    *x = T.hx0 + (c - (in_row - 1) / 2.0f) * T.hstep;
    *y = T.hy0 + row * T.hs * 2.16f;
}

/* ---- moving bones: each bone glides to where it is drawn now ----------------------------------------------------- */
typedef struct {
    int on, face, face0;
    float x, y, deg, len, x0, y0, deg0, len0;
    Uint32 t0;
} sprite;
static sprite spr[DOM_BONES];
static int from_yard[DOM_BONES];
static Uint32 quiet;
typedef struct { int v; float x, y; Uint32 t0; } popup;
static popup pop;

static float ease(Uint32 t0)
{
    Uint32 el = ui_ms() - t0;
    if (el >= SLIDE_MS) return 1;
    float t = (float)el / SLIDE_MS;
    return t * t * (3 - 2 * t);
}

static void spr_now(int b, float *x, float *y, float *deg, float *len, int *face)
{
    float e = ease(spr[b].t0), dd = fmodf(spr[b].deg - spr[b].deg0 + 540.0f, 360.0f) - 180;
    *x = spr[b].x0 + (spr[b].x - spr[b].x0) * e;
    *y = spr[b].y0 + (spr[b].y - spr[b].y0) * e;
    *deg = spr[b].deg0 + dd * e;
    *len = spr[b].len0 + (spr[b].len - spr[b].len0) * e;
    *face = e < 0.5f ? spr[b].face0 : spr[b].face;
}

static void target(int b, float x, float y, float deg, float len, int face)
{
    if (!spr[b].on) {
        spr[b].on = 1;
        if (from_yard[b]) {                     /* drawn: from the boneyard, turning over */
            from_yard[b] = 0;
            spr[b] = (sprite){1, face, 0, x, y, deg, len, T.yx, T.yy, 0, T.yl, ui_ms()};
            return;
        }
        spr[b] = (sprite){1, face, face, x, y, deg, len, x, y, deg, len, ui_ms() - SLIDE_MS};
        return;
    }
    if (fabsf(spr[b].x - x) < 0.5f && fabsf(spr[b].y - y) < 0.5f && fabsf(spr[b].len - len) < 0.5f &&
        fabsf(fmodf(spr[b].deg - deg + 720.0f, 360.0f)) < 0.5f && spr[b].face == face)
        return;
    float cx, cy, cd, cl;
    int cf;
    spr_now(b, &cx, &cy, &cd, &cl, &cf);
    spr[b] = (sprite){1, face, cf, x, y, deg, len, cx, cy, cd, cl, ui_ms()};
}

static int busy(void)
{
    for (int b = 0; b < DOM_BONES; b++)
        if (spr[b].on && ease(spr[b].t0) < 1) return 1;
    return (quiet && ui_ms() < quiet) || (pop.v && ui_ms() - pop.t0 < POP_MS);
}

/* everything where it belongs now: the line laid out again if it changed, each bone gliding to its place */
static void sync(void)
{
    const dpos *p = &G.pos;
    if (T.made != G.n) line_layout();
    int seen[DOM_BONES] = {0};
    for (int i = 0; i < T.nsp; i++) {
        spot *s = &T.sp[i];
        int a = s->a, b = s->b;
        float deg = s->deg;
        if (a > b) { int t = a; a = b, b = t; deg += 180; }
        target(s->bone, s->x, s->y, deg, T.L, 1);
        seen[s->bone] = 1;
    }
    uint8_t h[DOM_BONES];
    int n = hand_order(h);
    for (int i = 0; i < n; i++) {
        float x, y;
        hand_spot(i, n, &x, &y);
        target(h[i], x, y, 90, 2 * T.hs, 1);
        seen[h[i]] = 1;
    }
    for (int b = 0; b < DOM_BONES; b++)
        if (!seen[b]) spr[b].on = 0;
    (void)p;
}

static void snap(void)
{
    memset(spr, 0, sizeof spr);
    memset(from_yard, 0, sizeof from_yard);
    quiet = 0;
    pop.v = 0;
}

static void layout(box b)
{
    int me = ui_bottom();
    if (me < 0 || me >= G.pos.players) me = 0;
    if (me != T.me) snap();                     /* another player's view: nothing glides from the last one */
    T.me = me;
    T.b = b;
    hand_layout();
    T.made = -1;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static int sel = -1;                            /* the bone chosen in the hand */
typedef struct { int on, moved, bone; float x0, y0, x, y; } dragging;
static dragging drag;

static void start(int v)
{
    variant = v >= 0 && v < DOM_VARIANTS ? v : DOM_BLOCK;
    dom_new(&G, variant, ui.players, (uint32_t)ui_random(1 << 30));
    turn_n = 0;
    sel = -1;
    snap();
    T.made = -1;
}

static int status(void)
{
    if (G.pos.phase != DP_OVER) return GS_PLAY;
    unsigned w = dom_winners(&G.pos);
    if (w & (w - 1)) return GS_DRAW;
    for (int q = 0; q < G.pos.players; q++)
        if (w >> q & 1) return gs_win(q);
    return GS_DRAW;
}

static unsigned winners(void) { return dom_winners(&G.pos); }
static int turn(void) { return G.pos.turn; }

static void act(int a)
{
    const dpos *p = &G.pos;
    int who = p->turn;
    if (DA_KIND(a) == DA_DRAW) from_yard[p->yard[p->nyard - 1]] = 1;
    if (!dom_play(&G, a)) { ui_sound(SND_DENY); return; }
    sel = -1;
    sync();
    int k = DA_KIND(a);
    if (k == DA_PLAY) {
        ui_sound_later(SND_PLACE, SLIDE_MS - 40);
        quiet = ui_ms() + SLIDE_MS + REST_MS;
        if (p->scored) {
            int b = DA_BONE(a);
            pop = (popup){p->scored, spr[b].x, spr[b].y, ui_ms() + SLIDE_MS};
            ui_sound_later(SND_CHECK, SLIDE_MS + 60);
        }
    } else if (k == DA_DRAW) ui_sound(SND_MOVE);
    else if (k == DA_PASS) ui_sound(SND_TAP);
    if (p->phase == DP_PLAY && p->turn != who) { ui_handoff(p->turn); turn_n = G.n; }
    if (k == DA_NEXT) { ui_handoff(p->turn); turn_n = G.n; }
}

static int undo(void)
{
    if (G.n <= turn_n || !dom_undo(&G)) return 0;
    sel = -1;
    snap();
    T.made = -1;
    return 1;
}

static int can_pass(void) { return dom_legal(&G.pos, DA_PASS); }
static void pass(void) { act(DA_PASS); }

static int script(const char *s)
{
    int ok = dom_script(&G, s);
    sel = -1;
    snap();
    T.made = -1;
    turn_n = G.n;
    ui_handoff(G.pos.turn);
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_disc, *t_piece;
static int tok_size, disc_d, piece_d;

static int legal_end(int bone, int e)
{
    return dom_legal(&G.pos, DA(DA_PLAY, bone, e));
}

static int my_turn(void) { return G.pos.phase == DP_PLAY && G.pos.turn == T.me && !ui_handing(); }

/* the round's scores: a row a player, their token, the bones left in their hand, the points of the round, the total;
 * the round's winner in gold light */
static void draw_summary(void)
{
    const dpos *p = &G.pos;
    box b = T.b;
    float u = ui.unit, rh = fminf(1.25f * u, b.h * 0.8f / p->players), pw = fminf(b.w * 0.94f, 8.2f * u);
    float ph = rh * p->players + 0.5f * u, x0 = b.x + (b.w - pw) / 2, y0 = b.y + (b.h - ph) / 2;
    tok_size = (int)(0.6f * u);
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 70);
    ct_panel(x0, y0, pw, ph, 250);
    for (int q = 0; q < p->players; q++) {
        float cy = y0 + 0.25f * u + (q + 0.5f) * rh, tx = x0 + 0.6f * u, d = fminf(0.72f * rh, 0.8f * u);
        if (q == p->winner) ui_mark_gold(tx, cy, 2.0f * d);
        ui_put_c(ct_token(q, tok_size), tx, cy, d, d);
        float bl = fminf(0.62f * rh, 0.75f * u), bx = x0 + 1.15f * u, room_w = pw * 0.5f - 1.15f * u;
        int n = p->nhand[q];
        float stepx = n > 1 ? fminf(bl * 0.56f, (room_w - bl / 2) / (n - 1)) : 0;
        for (int i = 0; i < n; i++)
            draw_bone(dom_lo(p->hand[q][i]), dom_hi(p->hand[q][i]), bx + bl / 4 + i * stepx, cy, bl, 90, 1, BD_FLAT);
        float nh = 0.4f * rh;
        ui_number_signed(x0 + pw * 0.62f, cy, nh, p->last[q] + p->made[q], 1, 1);
        ui_number(x0 + pw * 0.86f, cy, 0.52f * rh, p->total[q], 1);
        if (q < p->players - 1)
            ct_rule(x0 + 0.3f * u, cy + rh / 2, pw - 0.6f * u);
    }
}

static void draw_yard(int glow)
{
    const dpos *p = &G.pos;
    int n = p->nyard, show = n > 3 ? 3 : n;
    if (glow) ui_mark_gold(T.yx, T.yy, 2.0f * T.yl);
    if (!n) {
        ui_fill(T.yx - T.yl / 2, T.yy - T.yl / 4, T.yl, T.yl / 2, 246, 238, 220, 30);
        return;
    }
    for (int i = 0; i < show; i++) {
        float off = (show - 1 - i) * 0.09f * T.yl;
        draw_bone(0, 0, T.yx + off * 0.4f, T.yy - off + (show - 1) * 0.045f * T.yl, T.yl, 0, 0,
                  (i ? BD_FLAT : 0) | (variant == DOM_BLOCK ? BD_DIM : 0));
    }
    ui_number(T.yx, T.yy + 0.52f * T.yl, 0.36f * ui.unit, n, 1);
}

static void draw_count(void)
{
    int c = dom_count(&G.pos), d = (int)(2 * T.cr);
    if (disc_d != d) {
        static const uint8_t dark[3] = {28, 34, 32};
        disc_d = d;
        ui_free(&t_disc);
        art_disc(d, dark, 0.85f, 0, ui_buf(d, d));
        t_disc = ui_tex(d, d, ui_buf(d, d));
    }
    if (c > 0 && c % 5 == 0) ui_mark_gold(T.cx, T.cy, 2.6f * T.cr);
    ui_put_c(t_disc, T.cx, T.cy, 2 * T.cr, 2 * T.cr);
    ui_number(T.cx, T.cy, 0.8f * T.cr, c, 1);
}

static void draw(void)
{
    const dpos *p = &G.pos;
    sync();
    int mine = my_turn(), b = busy();
    /* the line; the last bone laid on a warm light */
    int last = G.n ? G.act[G.n - 1] : 0;
    if (p->phase == DP_PLAY && DA_KIND(last) == DA_PLAY && !b && spr[DA_BONE(last)].on) {
        float x, y, d, l;
        int f;
        spr_now(DA_BONE(last), &x, &y, &d, &l, &f);
        float k = fmodf(d + 360.0f, 180.0f);
        int vert = k > 45 && k < 135;
        float w = (vert ? l / 2 : l) + 0.16f * l, h = (vert ? l : l / 2) + 0.16f * l;
        ui_mark_last(x - w / 2, y - h / 2, w, h);
    }
    for (int i = 0; i < T.nsp; i++) {
        int bone = T.sp[i].bone;
        if (drag.on && drag.moved && drag.bone == bone) continue;
        float x, y, d, l;
        int f;
        spr_now(bone, &x, &y, &d, &l, &f);
        draw_bone(T.sp[i].a <= T.sp[i].b ? T.sp[i].a : T.sp[i].b, T.sp[i].a <= T.sp[i].b ? T.sp[i].b : T.sp[i].a,
                  x, y, l, d, f, 0);
    }
    /* the places the chosen bone fits */
    if (mine && sel >= 0 && !b)
        for (int e = 0; e <= END_C; e++) {
            if (e > END_S && e != END_C) continue;
            if (!legal_end(sel, e)) continue;
            spot s;
            rect r;
            ghost(e, sel, &s, &r);
            ui_fill(r.x - r.hw, r.y - r.hh, 2 * r.hw, 2 * r.hh, 250, 214, 90, 70);
            ui_mark_dot(r.x, r.y, 0.42f * T.L);
        }
    /* the boneyard, the count */
    draw_yard(mine && !b && dom_legal(p, DA_DRAW));
    if (variant == DOM_FIVES) draw_count();
    /* the hand (under the hand-off cover: face down) */
    uint8_t h[DOM_BONES];
    int n = hand_order(h), any = mine && dom_can_play(p), hide = ui.viewer < 0;
    for (int i = 0; i < n; i++) {
        int bone = h[i];
        if (drag.on && drag.moved && drag.bone == bone && !hide) continue;
        float x, y, d, l;
        int f;
        spr_now(bone, &x, &y, &d, &l, &f);
        if (hide) { draw_bone(0, 0, x, y, l, d, 0, 0); continue; }
        int fits = 0;
        for (int e = 0; e <= END_C && !fits; e++) fits = (e <= END_S || e == END_C) && legal_end(bone, e);
        int how = bone == sel ? BD_RAISED : 0;
        if (any && !fits) how |= BD_DIM;
        if (bone == sel) ui_mark_select(x - l / 4 - 0.06f * l, y - 0.12f * l - l / 2 - 0.06f * l, l / 2 + 0.12f * l,
                                        l + 0.12f * l);
        draw_bone(dom_lo(bone), dom_hi(bone), x, y, l, d, f, how);
    }
    if (drag.on && drag.moved && !hide)
        draw_bone(dom_lo(drag.bone), dom_hi(drag.bone), drag.x, drag.y, 2 * T.hs, 90, 1, BD_RAISED);
    if (pop.v && ui_ms() >= pop.t0 && ui_ms() - pop.t0 < POP_MS) {   /* All Fives: the points, rising */
        float t = (float)(ui_ms() - pop.t0) / POP_MS, y = pop.y - t * 1.2f * ui.unit;
        ui_mark_gold(pop.x, y, 1.8f * ui.unit);
        ui_number_signed(pop.x, y, 0.55f * ui.unit, pop.v, 0, 1);
        ui_animate();
    } else if (pop.v && ui_ms() < pop.t0) ui_animate();
    if ((p->phase == DP_ROUND || p->phase == DP_OVER) && !b) draw_summary();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int hand_hit(float x, float y)
{
    uint8_t h[DOM_BONES];
    int n = hand_order(h);
    for (int i = n - 1; i >= 0; i--) {
        float cx, cy;
        hand_spot(i, n, &cx, &cy);
        if (fabsf(x - cx) <= T.hs * 0.56f && fabsf(y - cy) <= T.hs * 1.15f) return h[i];
    }
    return -1;
}

static int end_hit(int bone, float x, float y)
{
    int best = -1;
    float bd = 1e9f;
    for (int e = 0; e <= END_C; e++) {
        if ((e > END_S && e != END_C) || !legal_end(bone, e)) continue;
        spot s;
        rect r;
        ghost(e, bone, &s, &r);
        float m = 0.3f * T.L, dx = fabsf(x - r.x), dy = fabsf(y - r.y);
        if (dx > r.hw + m || dy > r.hh + m) continue;
        if (dx + dy < bd) bd = dx + dy, best = e;
    }
    return best;
}

static int playable(int bone)
{
    for (int e = 0; e <= END_C; e++)
        if ((e <= END_S || e == END_C) && legal_end(bone, e)) return 1;
    return 0;
}

static void press(float x, float y)
{
    const dpos *p = &G.pos;
    drag.on = 0;
    if (T.made != G.n) line_layout();           /* where the ends are (no frame may have been drawn since) */
    if (p->phase == DP_ROUND) { if (!busy()) act(DA_NEXT); return; }
    if (p->phase != DP_PLAY || busy()) return;
    if (!my_turn()) { ui_sound(SND_DENY); return; }
    int b = hand_hit(x, y);
    drag = (dragging){1, 0, b, x, y, x, y};
}

static void motion(float x, float y)
{
    if (!drag.on || drag.bone < 0) return;
    if (!drag.moved && hypotf(x - drag.x0, y - drag.y0) > 0.25f * ui.unit) {
        if (!playable(drag.bone)) return;
        drag.moved = 1;
        sel = drag.bone;
    }
    drag.x = x;
    drag.y = y;
    if (drag.moved) ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    const dpos *p = &G.pos;
    if (drag.moved) {                           /* dropped: on a place it fits, or back in the hand */
        int e = end_hit(drag.bone, x, y);
        if (e >= 0) act(DA(DA_PLAY, drag.bone, e));
        else sel = drag.bone;
        return;
    }
    int b = drag.bone;
    if (b >= 0) {                               /* a bone of the hand: chosen (again: put down) */
        if (b == sel) { sel = -1; ui_sound(SND_TAP); }
        else if (playable(b)) { sel = b; ui_sound(SND_TAP); }
        else ui_sound(SND_DENY);
        return;
    }
    if (sel >= 0) {
        int e = end_hit(sel, x, y);
        if (e >= 0) { act(DA(DA_PLAY, sel, e)); return; }
    }
    if (fabsf(x - T.yx) < T.yl * 0.7f && fabsf(y - T.yy) < T.yl * 0.6f) {   /* the boneyard */
        if (dom_legal(p, DA_DRAW)) act(DA_DRAW);
        else ui_sound(SND_DENY);
        return;
    }
    if (sel >= 0) { sel = -1; return; }
    if (!dom_can_play(p)) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void piece(int size)
{
    if (piece_d == size) return;
    piece_d = size;
    ui_free(&t_piece);
    uint8_t *buf = ui_buf(size, size), *bone = malloc((size_t)(size / 2) * size * 4);
    memset(buf, 0, (size_t)size * size * 4);
    if (bone) {
        paint_bone(0, 0, 0, 1, size / 2, size, bone);
        art_blend(buf, size, size, bone, size / 2, size, size / 4, 0);
        free(bone);
    }
    t_piece = ui_tex(size, size, buf);
}

/* the player's total, and the bones in their hand */
static void bar(int player, box b)
{
    const dpos *p = &G.pos;
    float u = ui.unit, h = fminf(0.46f * u, b.h * 0.5f);
    int v = p->total[player], digits = v >= 100 ? 3 : v >= 10 ? 2 : 1;
    float tw = (0.74f * (digits - 1) + 0.62f) * h, cw = 0.72f * u + 0.74f * h * (p->nhand[player] >= 10 ? 2 : 1);
    if (tw + 0.3f * u + cw > b.w) {             /* narrow (four players): the score smaller */
        h *= fmaxf(0.5f, (b.w - cw - 0.2f * u) / (tw + 0.1f * u));
        tw = (0.74f * (digits - 1) + 0.62f) * h;
    }
    ui_number(b.x + tw / 2, b.y + b.h / 2, h, v, 1);
    piece((int)(0.6f * u));
    box c = {b.x + tw + 0.3f * u, b.y, b.w - tw - 0.3f * u, b.h};
    if (c.w >= cw * 0.9f) ui_bar_count(c, t_piece, p->nhand[player]);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* a bone painted into a picture: centred on (cx, cy), len long, lying or standing */
static void blend_bone(uint8_t *rgba, int size, int a, int b, float cx, float cy, float len, int vertical)
{
    int w = vertical ? (int)(len / 2) : (int)len, h = vertical ? (int)len : (int)(len / 2);
    uint8_t *buf = malloc((size_t)w * h * 4), *sh = malloc((size_t)w * h * 4);
    if (buf && sh) {
        paint_bone(a, b, 1, vertical, w, h, buf);
        for (int i = 0; i < w * h; i++)         /* its shadow: the bone in black, faint */
            sh[4 * i] = sh[4 * i + 1] = sh[4 * i + 2] = 0, sh[4 * i + 3] = buf[4 * i + 3] / 4;
        art_blend(rgba, size, size, sh, w, h, (int)(cx - w / 2.0f + 0.03f * len), (int)(cy - h / 2.0f + 0.05f * len));
        art_blend(rgba, size, size, buf, w, h, (int)(cx - w / 2.0f), (int)(cy - h / 2.0f));
    }
    free(buf);
    free(sh);
}

/* the menu's card: a line turning a corner round a double; the picker's: the block game's short line, the draw
 * game's with the boneyard, All Fives' spinner with its four arms */
static void thumb(int v, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    float c = size / 2.0f, L, h;
    if (v == DOM_FIVES) {                       /* the spinner and its four arms */
        L = size * 0.29f, h = L / 2;
        blend_bone(rgba, size, 5, 5, c, c, L, 1);
        blend_bone(rgba, size, 0, 5, c - h / 2 - h, c, L, 0);
        blend_bone(rgba, size, 5, 3, c + h / 2 + h, c, L, 0);
        blend_bone(rgba, size, 6, 5, c, c - 2 * h, L, 1);
        blend_bone(rgba, size, 5, 2, c, c + 2 * h, L, 1);
        return;
    }
    if (v == DOM_DRAW) {                        /* a short line, the boneyard under it */
        L = size * 0.34f, h = L / 2;
        float y = size * 0.38f, x = c - (L + h) / 2;
        blend_bone(rgba, size, 6, 3, x + h, y, L, 0);
        blend_bone(rgba, size, 3, 3, x + L + h / 2, y, L, 1);
        int w = (int)(0.75f * L), hh = w / 2;
        uint8_t *buf = malloc((size_t)w * hh * 4);
        if (!buf) return;
        paint_bone(0, 0, 0, 0, w, hh, buf);
        for (int i = 0; i < 3; i++)
            art_blend(rgba, size, size, buf, w, hh, (int)(c - w / 2 + 0.02f * i * L),
                      (int)(size * 0.7f - i * 0.07f * L));
        free(buf);
        return;
    }
    if (v == DOM_BLOCK) {                       /* three bones in a line, a double across it */
        L = size * 0.3f, h = L / 2;
        blend_bone(rgba, size, 2, 4, c - h / 2 - h, c, L, 0);
        blend_bone(rgba, size, 4, 4, c, c, L, 1);
        blend_bone(rgba, size, 4, 1, c + h / 2 + h, c, L, 0);
        return;
    }
    L = size * 0.28f, h = L / 2;                /* the menu: a line going round a corner */
    float y = size * 0.25f, x0 = size * 0.06f, ex = x0 + 2 * L + h;
    blend_bone(rgba, size, 1, 3, x0 + h, y, L, 0);
    blend_bone(rgba, size, 3, 3, x0 + L + h / 2, y, L, 1);
    blend_bone(rgba, size, 3, 6, x0 + L + h + h, y, L, 0);
    blend_bone(rgba, size, 6, 2, ex + h / 2, y + h / 2, L, 1);
    blend_bone(rgba, size, 2, 5, ex + h / 2, y + 2.5f * h, L, 1);
    blend_bone(rgba, size, 0, 5, ex, y + 4 * h, L, 0);
}

static const game dominoes_game = {
    .id = "dominoes", .order = 296, .variants = DOM_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4, .view = VIEW_HIDDEN,
};
REGISTER_GAME(dominoes_game)
