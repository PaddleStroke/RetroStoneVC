/*
 * Mahjong Solitaire in RetroStone Board, for one player: the tiles stacked on the green table, ivory faces on a
 * bamboo-green back, each layer drawn up and to the left of the one under it, its shadow on the tiles below (the
 * rules: mahjong_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The tiles and their faces are drawn by code in games/mjtiles.h (shared with the table game for four, mahjong4.c):
 * the dots, the bamboo (the one a bird), the characters (the numeral over 萬), the winds 東南西北, the dragons 中, 發
 * and the white one (a blue frame), the flowers and the seasons (a picture, and their number in a corner).
 *
 * The input: tap a free tile (the select mark), then its match: both lift away. A tap on the tile picked lets it
 * go; on another free tile, picks that one. Only free tiles can be played, so a tap goes to the free tile under the
 * finger, or the free tile nearest to it if the finger is just beside one (the Turtle's tiles are small on a phone).
 * The tiles that are not free are dimmed. The bar: the tiles left, a hint button (two matching free tiles blink),
 * and, when no pair is left, the shuffle button (big in the middle of the board, and in the bar when it has room):
 * the tiles left are dealt again so that they can be cleared. Undo takes back a pair, or a shuffle.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/mahjong.h"
#include "games/mjtiles.h"
#include "shell.h"

#define TAKE_MS 420                             /* a pair lifting away */
#define HINT_MS 2400
#define PI_F 3.14159265f

/* ---- the state --------------------------------------------------------------------------------------------------- */
static mjgame G;
static int sel = -1;
static int order[MJ_MAX];                       /* the drawing order: layer by layer, back to front */
static uint8_t freeish[MJ_MAX];                 /* the free tiles (worked out once a frame) */
static struct { int on, a, b; uint8_t fa, fb; Uint32 t0; } anim;
static struct { int on, a, b, shuffle; Uint32 t0; } hint;
static box last_box, barbox;
static int fw, fh, th, made_key = -1, made_bar = -1;
static float ox, oy;                            /* where the layout's (0, 0) on layer 0 is drawn */
static SDL_Texture *t_tile, *t_shadow, *t_face[MJ_FACES], *t_small, *t_disc, *t_disc_on, *t_icon[2];

static void layout(box b);

static void sort_places(const mjpos *pos, int n, int *ord)
{
    for (int i = 0; i < n; i++) ord[i] = i;
    for (int i = 1; i < n; i++) {               /* insertion sort: layer, then towards the lower right */
        int k = ord[i], j = i - 1;
        mjpos p = pos[k];
        while (j >= 0) {
            mjpos q = pos[ord[j]];
            int before = q.z < p.z || (q.z == p.z && (q.x + q.y < p.x + p.y || (q.x + q.y == p.x + p.y && q.x <= p.x)));
            if (before) break;
            ord[j + 1] = ord[j];
            j--;
        }
        ord[j + 1] = k;
    }
}

static void calm(void)
{
    sel = -1;
    anim.on = 0;
    hint.on = 0;
}

static void start(int variant)
{
    int v = variant < 0 || variant >= MJ_VARIANTS ? 0 : variant;
    for (int t = 0; t < 8 && !mj_deal(&G, v, ui_random); t++) {}
    sort_places(G.pos, G.n, order);
    calm();
    if (made_key >= 0) layout(last_box);        /* another layout: another size of tiles */
}

static int status(void)
{
    int st = mj_status(&G);
    return st == MJ_WON ? GS_WIN0 : st == MJ_LOST ? GS_WIN1 : GS_PLAY;
}

static int turn(void) { return 0; }
static int busy(void) { return anim.on; }

static int undo(void)
{
    calm();
    return mj_undo(&G);
}

static int script(const char *moves)
{
    calm();
    return mj_script(&G, moves, ui_random);
}

static void take(int a, int b)
{
    anim.a = a;
    anim.b = b;
    anim.fa = G.face[a];
    anim.fb = G.face[b];
    if (!mj_take(&G, a, b)) { ui_sound(SND_DENY); return; }
    anim.on = 1;
    anim.t0 = ui_ms();
    sel = -1;
    hint.on = 0;
    ui_sound(SND_CAPTURE);
    ui_animate();
}

static void shuffle(void)
{
    if (!mj_shuffle(&G, ui_random)) { ui_sound(SND_DENY); return; }
    calm();
    ui_sound(SND_DICE);
    ui_animate();
}

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static void place_xy(int i, float *x, float *y)  /* a tile's face, its top left corner */
{
    mjpos p = G.pos[i];
    *x = ox + p.x * (fw / 2.0f) - p.z * th;
    *y = oy + p.y * (fh / 2.0f) - p.z * th;
}

/* the box the layout takes for tiles fw x fh, d thick: from (x0, y0) (relative to the layout's origin) */
static void extent(const mjpos *pos, int n, int w, int h, int d, float *x0, float *y0, float *x1, float *y1)
{
    *x0 = *y0 = 1e9f;
    *x1 = *y1 = -1e9f;
    for (int i = 0; i < n; i++) {
        float x = pos[i].x * (w / 2.0f) - pos[i].z * d, y = pos[i].y * (h / 2.0f) - pos[i].z * d;
        *x0 = fminf(*x0, x);
        *y0 = fminf(*y0, y);
        *x1 = fmaxf(*x1, x + w + d);
        *y1 = fmaxf(*y1, y + h + d);
    }
}

/* the largest tiles (even sizes) for which the layout fits in W x H */
static void fit(const mjpos *pos, int n, float W, float H, int *w, int *h, int *d)
{
    for (*w = 400; *w > 6; *w -= 2) {
        *h = 2 * (int)lroundf(MJT_ASPECT * *w / 2);
        *d = (int)fmaxf(2, lroundf(MJT_THICK * *w));
        float x0, y0, x1, y1;
        extent(pos, n, *w, *h, *d, &x0, &y0, &x1, &y1);
        if (x1 - x0 <= W && y1 - y0 <= H) return;
    }
}

static void layout(box b)
{
    last_box = b;
    fit(G.pos, G.n, 0.99f * b.w, 0.99f * b.h, &fw, &fh, &th);
    float x0, y0, x1, y1;
    extent(G.pos, G.n, fw, fh, th, &x0, &y0, &x1, &y1);
    ox = floorf(b.x + (b.w - (x1 - x0)) / 2 - x0);
    oy = floorf(b.y + (b.h - (y1 - y0)) / 2 - y0);
    int key = fw * 1000 + fh;
    if (made_key == key) return;
    made_key = key;
    int w = fw + th, h = fh + th;
    ui_free(&t_tile);
    mjt_tile_pic(fw, fh, th, ui_buf(w, h));
    t_tile = ui_tex(w, h, ui_buf(w, h));
    int sw = fw + 2 * th, sh = fh + 2 * th;
    ui_free(&t_shadow);
    mjt_shadow_pic(sw, sh, 1.1f * th, ui_buf(sw, sh));
    t_shadow = ui_tex(sw, sh, ui_buf(sw, sh));
    for (int f = 0; f < MJ_FACES; f++) {
        ui_free(&t_face[f]);
        mjt_face_pic(f, fw, fh, ui_buf(fw, fh));
        t_face[f] = ui_tex(fw, fh, ui_buf(fw, fh));
    }
}

/* ---- the buttons' pictures: a hint (a lamp) and a shuffle (two arrows crossing) ---------------------------------- */
static void icon_pic(int which, int size, uint8_t *o)
{
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    if (!which) {                               /* the hint: a lamp, lit */
        art_ellipse(f, ART_PART, 0.5f, 0.42f, 0.19f, 0.19f);
        const float neck[] = {0.38f, 0.54f, 0.62f, 0.54f, 0.585f, 0.68f, 0.415f, 0.68f};
        art_poly(f, ART_PART, neck, 4);
        art_rect(f, ART_PART, 0.415f, 0.70f, 0.585f, 0.74f);
        art_rect(f, ART_PART, 0.44f, 0.76f, 0.56f, 0.80f);
        for (int i = 0; i < 5; i++) {           /* its rays */
            float a = (90 + (i - 2) * 40) * PI_F / 180, c = cosf(a), s = -sinf(a);
            mjt_band(f, ART_PART, 0.5f + c * 0.26f, 0.42f + s * 0.26f, 0.5f + c * 0.34f, 0.42f + s * 0.34f, 0.045f);
        }
    } else {                                    /* the shuffle: two arrows crossing */
        mjt_band(f, ART_PART, 0.18f, 0.34f, 0.40f, 0.34f, 0.075f);
        mjt_band(f, ART_PART, 0.38f, 0.34f, 0.62f, 0.66f, 0.075f);
        mjt_band(f, ART_PART, 0.60f, 0.66f, 0.70f, 0.66f, 0.075f);
        mjt_band(f, ART_PART, 0.18f, 0.66f, 0.40f, 0.66f, 0.075f);
        mjt_band(f, ART_PART, 0.38f, 0.66f, 0.62f, 0.34f, 0.075f);
        mjt_band(f, ART_PART, 0.60f, 0.34f, 0.70f, 0.34f, 0.075f);
        const float a1[] = {0.68f, 0.22f, 0.84f, 0.34f, 0.68f, 0.46f}, a2[] = {0.68f, 0.54f, 0.84f, 0.66f, 0.68f, 0.78f};
        art_poly(f, ART_PART, a1, 3);
        art_poly(f, ART_PART, a2, 3);
    }
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

/* a tile standing, seen from the front, size x size: the token (the red dragon), the bar's count */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int w = 2 * (int)(size * 0.30f), h = 2 * (int)lroundf(MJT_ASPECT * w / 2);
    int d = (int)fmaxf(1, lroundf(MJT_THICK * w));
    int bw = w + d, bh = h + d;
    uint8_t *body = malloc((size_t)bw * bh * 4), *face = malloc((size_t)w * h * 4);
    if (body && face) {
        mjt_tile_pic(w, h, d, body);
        mjt_face_pic(MJ_DRAGONS, w, h, face);
        art_blend(body, bw, bh, face, w, h, 0, 0);
        memset(rgba, 0, (size_t)size * size * 4);
        art_blend(rgba, size, size, body, bw, bh, (size - bw) / 2, (size - bh) / 2);
    }
    free(body);
    free(face);
}

static void bar_textures(void)
{
    int key = (int)ui.unit;
    if (made_bar == key) return;
    made_bar = key;
    static const uint8_t house[3] = {54, 72, 64}, gold[3] = {150, 118, 46};
    int d = (int)(1.7f * ui.unit), ic = (int)(1.05f * ui.unit), s = (int)(0.6f * ui.unit);
    ui_free(&t_disc);
    art_disc(d, house, 1, 0, ui_buf(d, d));
    t_disc = ui_tex(d, d, ui_buf(d, d));
    ui_free(&t_disc_on);
    art_disc(d, gold, 1, 0, ui_buf(d, d));
    t_disc_on = ui_tex(d, d, ui_buf(d, d));
    for (int i = 0; i < 2; i++) {
        ui_free(&t_icon[i]);
        icon_pic(i, ic, ui_buf(ic, ic));
        t_icon[i] = ui_tex(ic, ic, ui_buf(ic, ic));
        SDL_SetTextureColorMod(t_icon[i], 252, 244, 226);
    }
    ui_free(&t_small);
    token(0, s, ui_buf(s, s));
    t_small = ui_tex(s, s, ui_buf(s, s));
}

static void button(float x, float y, float r, int icon, int on)
{
    bar_textures();
    ui_put_c(on ? t_disc_on : t_disc, x, y, 2 * r, 2 * r);
    ui_put_c(t_icon[icon], x, y, 1.3f * r, 1.3f * r);
}

/* the big shuffle button in the middle of the board, when no pair is left */
static int big_shuffle(float *x, float *y, float *r)
{
    *x = last_box.x + last_box.w / 2;
    *y = last_box.y + last_box.h / 2;
    *r = 0.85f * ui.unit;
    return mj_status(&G) == MJ_STUCK && !anim.on;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static float ease(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

static void tile_at(float x, float y, int face, Uint8 k, Uint8 alpha)
{
    SDL_SetTextureColorMod(t_tile, k, k, (Uint8)(k * 0.97f));
    SDL_SetTextureColorMod(t_face[face], k, k, k);
    SDL_SetTextureAlphaMod(t_tile, alpha);
    SDL_SetTextureAlphaMod(t_face[face], alpha);
    ui_put(t_tile, x, y, (float)(fw + th), (float)(fh + th));
    ui_put(t_face[face], x, y, (float)fw, (float)fh);
    SDL_SetTextureAlphaMod(t_tile, 255);
    SDL_SetTextureAlphaMod(t_face[face], 255);
}

static void draw(void)
{
    float x, y;
    uint8_t on[MJ_MAX];
    for (int i = 0; i < G.n; i++) on[i] = G.face[i] != MJ_NONE;
    for (int i = 0; i < G.n; i++) freeish[i] = (uint8_t)mj_free(&G, i);
    Uint32 now = ui_ms();
    if (anim.on && now - anim.t0 >= TAKE_MS) anim.on = 0;
    if (hint.on && now - hint.t0 >= HINT_MS) hint.on = 0;
    if (anim.on || hint.on) ui_animate();
    int blink = hint.on && (now - hint.t0) / 300 % 2 == 0;
    for (int k = 0; k < G.n;) {                 /* a layer at a time: its shadows on the layer under it, then it */
        int z = G.pos[order[k]].z, e = k;
        while (e < G.n && G.pos[order[e]].z == z) e++;
        for (int j = k; j < e; j++) {
            int i = order[j];
            if (!on[i]) continue;
            place_xy(i, &x, &y);
            ui_put(t_shadow, x + 0.2f * th, y + 0.5f * th, (float)(fw + 2 * th), (float)(fh + 2 * th));
        }
        for (int j = k; j < e; j++) {
            int i = order[j];
            if (!on[i]) continue;
            place_xy(i, &x, &y);
            tile_at(x, y, G.face[i], freeish[i] ? 255 : 196, 255);
            if (i == sel) ui_mark_select(x, y, (float)fw, (float)fh);
            if (blink && !hint.shuffle && (i == hint.a || i == hint.b)) ui_mark_select(x, y, (float)fw, (float)fh);
        }
        k = e;
    }
    if (anim.on) {                              /* the pair taken: rising, fading away */
        float t = (now - anim.t0) / (float)TAKE_MS, e = ease(t);
        for (int k = 0; k < 2; k++) {
            place_xy(k ? anim.b : anim.a, &x, &y);
            tile_at(x - e * 0.3f * fw, y - e * 0.6f * fh, k ? anim.fb : anim.fa, 255, (Uint8)(255 * (1 - e)));
        }
    }
    float r;
    if (big_shuffle(&x, &y, &r)) button(x, y, r, 1, !hint.on || blink);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the tile a press at (x, y) means: the top tile there if it is free, else the free tile nearest the finger if it is
 * just beside one; *hit: a tile is under the finger */
static int tile_under(float x, float y, int *hit)
{
    *hit = 0;
    for (int j = G.n - 1; j >= 0; j--) {        /* the top tile there */
        int i = order[j];
        if (G.face[i] == MJ_NONE) continue;
        float tx, ty;
        place_xy(i, &tx, &ty);
        if (x < tx || y < ty || x >= tx + fw + th || y >= ty + fh + th) continue;
        *hit = 1;
        if (mj_free(&G, i)) return i;
        break;
    }
    int best = -1;
    float bd = 1e9f, mx = 0.3f * fw, my = 0.22f * fh;
    for (int i = 0; i < G.n; i++) {             /* beside a free tile */
        if (!mj_free(&G, i)) continue;
        float tx, ty;
        place_xy(i, &tx, &ty);
        if (x < tx - mx || y < ty - my || x >= tx + fw + th + mx || y >= ty + fh + th + my) continue;
        float d = hypotf(x - (tx + fw / 2.0f), y - (ty + fh / 2.0f));
        if (d < bd) bd = d, best = i;
    }
    return best;
}

static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    float bx, by, br;
    if (big_shuffle(&bx, &by, &br) && hypotf(x - bx, y - by) < 1.1f * br) { shuffle(); return; }
    anim.on = 0;
    int hit, t = tile_under(x, y, &hit);
    if (t < 0) {
        if (hit) ui_sound(SND_DENY);            /* a tile that is not free */
        sel = -1;
        ui_animate();
        return;
    }
    hint.on = 0;
    if (t == sel) sel = -1;                     /* the tile picked: let it go */
    else if (sel >= 0 && mj_match(G.face[sel], G.face[t])) take(sel, t);
    else sel = t;                               /* the first of a pair, or another one instead */
    ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the bar: the tiles left, the hint, and when stuck the shuffle ---------------------------------------------- */
static void bar_button(int which, float *x, float *y, float *r)   /* 0 the hint (at the right end), 1 the shuffle */
{
    *r = 0.42f * ui.unit;
    *x = barbox.x + barbox.w - *r - 0.05f * ui.unit - which * (2 * *r + 0.25f * ui.unit);
    *y = barbox.y + barbox.h / 2;
}

/* the shuffle in the bar: when no pair is left, and the bar has room for it beside the count (a narrow bar, in
 * landscape: only the big one in the middle of the board) */
static int bar_shuffle(void)
{
    float x, y, r;
    bar_button(1, &x, &y, &r);
    return mj_status(&G) == MJ_STUCK && x - r >= barbox.x + 1.75f * ui.unit;
}

static void bar(int player, box b)
{
    (void)player;
    bar_textures();
    barbox = b;
    ui_bar_count(b, t_small, G.tiles);
    if (status() != GS_PLAY) return;
    float x, y, r;
    int blink = hint.on && (ui_ms() - hint.t0) / 300 % 2 == 0;
    bar_button(0, &x, &y, &r);
    button(x, y, r, 0, 0);
    if (bar_shuffle()) {
        bar_button(1, &x, &y, &r);
        button(x, y, r, 1, !hint.on || blink);
    }
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (!barbox.w || status() != GS_PLAY) return 0;
    float bx, by, r;
    bar_button(0, &bx, &by, &r);
    if (hypotf(x - bx, y - by) < 1.1f * r) {    /* the hint: a pair, or (none left) the shuffle */
        anim.on = 0;
        hint.on = 1;
        hint.t0 = ui_ms();
        hint.shuffle = !mj_hint(&G, &hint.a, &hint.b);
        sel = -1;
        ui_sound(hint.shuffle ? SND_DENY : SND_TAP);
        ui_animate();
        return 1;
    }
    bar_button(1, &bx, &by, &r);
    if (bar_shuffle() && hypotf(x - bx, y - by) < 1.1f * r) {
        shuffle();
        return 1;
    }
    return 0;
}

/* ---- the menu's and the picker's cards --------------------------------------------------------------------------- */
static uint32_t thumb_rng;
static int thumb_rnd(int n)
{
    thumb_rng = thumb_rng * 1664525u + 1013904223u;
    return n > 0 ? (int)((thumb_rng >> 8) % (uint32_t)n) : 0;
}

/* the layout dealt (the same deal each time); the menu's, the Pyramid under way */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static mjgame T;
    int v = variant < 0 ? 1 : variant;        /* the menu's: the Pyramid (larger tiles on a small card) */
    thumb_rng = 2026u + (uint32_t)v;
    if (!mj_deal(&T, v, thumb_rnd)) return;
    if (variant < 0) mj_script(&T, "=7", thumb_rnd);
    int w, h, d;
    fit(T.pos, T.n, 0.96f * size, 0.96f * size, &w, &h, &d);
    float x0, y0, x1, y1;
    extent(T.pos, T.n, w, h, d, &x0, &y0, &x1, &y1);
    float px0 = (size - (x1 - x0)) / 2 - x0, py0 = (size - (y1 - y0)) / 2 - y0;
    int bw = w + d, bh = h + d, sw = w + 2 * d, sh = h + 2 * d;
    uint8_t *body = malloc((size_t)bw * bh * 4), *sh_pic = malloc((size_t)sw * sh * 4);
    uint8_t *faces = malloc((size_t)MJ_FACES * w * h * 4);
    if (!body || !sh_pic || !faces) { free(body); free(sh_pic); free(faces); return; }
    mjt_tile_pic(w, h, d, body);
    mjt_shadow_pic(sw, sh, 1.1f * d, sh_pic);
    for (int f = 0; f < MJ_FACES; f++) mjt_face_pic(f, w, h, faces + (size_t)f * w * h * 4);
    int ord[MJ_MAX], n = T.n;
    sort_places(T.pos, n, ord);
    for (int j = 0; j < n; j++) {
        int i = ord[j];
        if (T.face[i] == MJ_NONE) continue;
        int x = (int)lroundf(px0 + T.pos[i].x * (w / 2.0f) - T.pos[i].z * d);
        int y = (int)lroundf(py0 + T.pos[i].y * (h / 2.0f) - T.pos[i].z * d);
        art_blend(rgba, size, size, sh_pic, sw, sh, x + d / 5, y + d / 2);
        art_blend(rgba, size, size, body, bw, bh, x, y);
        art_blend(rgba, size, size, faces + (size_t)T.face[i] * w * h * 4, w, h, x, y);
    }
    free(body);
    free(sh_pic);
    free(faces);
}

static const game mahjong_game = {
    .id = "mahjong", .order = 310, .variants = MJ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(mahjong_game)
