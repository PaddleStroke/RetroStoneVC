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
 * The layout takes all the room the shell gives (game.fill): the tiles as big as it lets them be (a wide screen gives
 * the Turtle much bigger tiles; never turned, as the free tiles' rule goes by left and right).
 *
 * The input: tap a free tile (the select mark), then its match: both lift away. A tap on the tile picked lets it
 * go; on another free tile, picks that one. Only free tiles can be played, so a tap goes to the free tile under the
 * finger, or the free tile nearest to it if the finger is just beside one (the Turtle's tiles are small on a phone).
 * The tiles that are not free are dimmed. The shell's tools: the hint (two matching free tiles blink), the shuffle
 * (lit when no pair is left, and then big in the middle of the board too: the tiles left are dealt again so that they
 * can be cleared) and, when the whole layout leaves the tiles small (the Turtle on a phone held upright), the
 * magnifying glass: the tiles near, bigger, the layout dragged sideways with a finger (a tap still picks a tile).
 * Undo takes back a pair, or a shuffle.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/mahjong.h"
#include "games/mjtiles.h"
#include "games/pztools.h"
#include "shell.h"

#define TAKE_MS 420                             /* a pair lifting away */
#define HINT_MS 2400
#define PI_F 3.14159265f
#define SMALL 0.075f                            /* tiles narrower than this part of the screen's short side: the
                                                 * magnifier is offered */
#define NEAR 1.7f                               /* near: the tiles this much bigger (at most as the room's height
                                                 * lets them) */

enum { T_HINT, T_SHUFFLE, T_ZOOM, TOOLS };     /* the shell's tools */

/* ---- the state --------------------------------------------------------------------------------------------------- */
static mjgame G;
static int sel = -1;
static int order[MJ_MAX];                       /* the drawing order: layer by layer, back to front */
static uint8_t freeish[MJ_MAX];                 /* the free tiles (worked out once a frame) */
static struct { int on, a, b; uint8_t fa, fb; Uint32 t0; } anim;
static struct { int on, a, b, shuffle; Uint32 t0; } hint;
static box last_box;                            /* the shell's room (game.fill) */
static int fw, fh, th, made_key = -1, made_bar = -1;
static float ox, oy;                            /* where the layout's (0, 0) on layer 0 is drawn */
static int zoom, zoomable;                      /* the magnifier on (kept from game to game); offered */
static float lx0, ly0, lw, lh, pan_x, pan_y;    /* the layout's box (from its origin) at the size shown; the view's
                                                 * offset into it (near) */
static struct { int on, panning; float x0, y0, px, py; } ptr;   /* a finger down on the layout, near */
static SDL_Texture *t_tile, *t_shadow, *t_face[MJ_FACES], *t_small;

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
    ptr.on = 0;
}

static void start(int variant)
{
    int v = variant < 0 || variant >= MJ_VARIANTS ? 0 : variant;
    for (int t = 0; t < 8 && !mj_deal(&G, v, ui_random); t++) {}
    sort_places(G.pos, G.n, order);
    calm();
    pan_x = pan_y = 1e9f;                       /* near: the view in the middle (clamped below) */
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

static void tile_size(int w, int *h, int *d)    /* a tile w wide: its face's height, its thickness */
{
    *h = 2 * (int)lroundf(MJT_ASPECT * w / 2);
    *d = (int)fmaxf(2, lroundf(MJT_THICK * w));
}

/* the largest tiles (even sizes) for which the layout fits in W x H */
static void fit(const mjpos *pos, int n, float W, float H, int *w, int *h, int *d)
{
    for (*w = 400; *w > 6; *w -= 2) {
        tile_size(*w, h, d);
        float x0, y0, x1, y1;
        extent(pos, n, *w, *h, *d, &x0, &y0, &x1, &y1);
        if (x1 - x0 <= W && y1 - y0 <= H) return;
    }
}

/* the layout on the screen: centred in the room, or (near, larger than the room) the view pan_x, pan_y into it */
static void place_view(void)
{
    box b = last_box;
    if (pan_x > 1e8f) pan_x = (lw - b.w) / 2;  /* a new game: the middle */
    if (pan_y > 1e8f) pan_y = (lh - b.h) / 2;
    pan_x = clampf(pan_x, 0, fmaxf(0, lw - b.w));
    pan_y = clampf(pan_y, 0, fmaxf(0, lh - b.h));
    ox = floorf(lw <= b.w ? b.x + (b.w - lw) / 2 - lx0 : b.x - pan_x - lx0);
    oy = floorf(lh <= b.h ? b.y + (b.h - lh) / 2 - ly0 : b.y - pan_y - ly0);
}

static int near_view(void) { return zoom && zoomable; }

/* near: the view's middle on the point (x, y) of the layout (from its origin) */
static void centre_view(float x, float y)
{
    pan_x = x - lx0 - last_box.w / 2;
    pan_y = y - ly0 - last_box.h / 2;
    place_view();
}

/* the whole layout in all the room (as big as it goes; never turned); near, the tiles bigger and the view dragged */
static void layout(box b)
{
    last_box = b;
    fit(G.pos, G.n, 0.99f * b.w, 0.99f * b.h, &fw, &fh, &th);
    zoomable = fw < SMALL * fminf(ui.w, ui.h);
    ui_tools_shown(zoomable ? TOOLS : T_ZOOM);
    if (near_view()) {                          /* bigger, at most as tall as the room */
        int w, h, d;
        fit(G.pos, G.n, 1e9f, 0.99f * b.h, &w, &h, &d);
        fw = (int)fminf((float)w, 2 * floorf(NEAR * fw / 2));
        tile_size(fw, &fh, &th);
    }
    float x0, y0, x1, y1;
    extent(G.pos, G.n, fw, fh, th, &x0, &y0, &x1, &y1);
    lx0 = x0;
    ly0 = y0;
    lw = x1 - x0;
    lh = y1 - y0;
    place_view();
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

/* ---- the tools: the hint (the shell's lamp), the shuffle (two arrows crossing, white: tinted), the magnifier ----- */
static void paint_shuffle(int kind, int size, uint8_t *o)
{
    (void)kind;
    art_style st = {{255, 255, 255}, {0, 0, 0}, {0, 0, 0}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    art_band(f, ART_PART, 0.18f, 0.34f, 0.40f, 0.34f, 0.075f);
    art_band(f, ART_PART, 0.38f, 0.34f, 0.62f, 0.66f, 0.075f);
    art_band(f, ART_PART, 0.60f, 0.66f, 0.70f, 0.66f, 0.075f);
    art_band(f, ART_PART, 0.18f, 0.66f, 0.40f, 0.66f, 0.075f);
    art_band(f, ART_PART, 0.38f, 0.66f, 0.62f, 0.34f, 0.075f);
    art_band(f, ART_PART, 0.60f, 0.34f, 0.70f, 0.34f, 0.075f);
    const float a1[] = {0.68f, 0.22f, 0.84f, 0.34f, 0.68f, 0.46f}, a2[] = {0.68f, 0.54f, 0.84f, 0.66f, 0.68f, 0.78f};
    art_poly(f, ART_PART, a1, 3);
    art_poly(f, ART_PART, a2, 3);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

static void paint_glass(int plus, int size, uint8_t *o) { pzt_pic_glass(o, size, plus); }

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
    int s = (int)(0.6f * ui.unit);
    ui_free(&t_small);
    token(0, s, ui_buf(s, s));
    t_small = ui_tex(s, s, ui_buf(s, s));
}

/* the big shuffle button in the middle of the room, when no pair is left */
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
    int blink = hint.on && (now - hint.t0) / 300 % 2 == 0, near = near_view();
    box b = last_box;
    SDL_Rect clip = {(int)b.x, (int)b.y, (int)b.w, (int)b.h};
    if (near) SDL_RenderSetClipRect(ui.r, &clip);   /* near: the tiles kept inside the room */
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
    if (near) {
        SDL_RenderSetClipRect(ui.r, NULL);
        float t = fmaxf(2, floorf(0.06f * ui.unit));   /* where the view is: a thin bar under the layout */
        float yb = fminf(oy + ly0 + lh + 0.3f * ui.unit, b.y + b.h - t);
        if (lw > b.w) {
            ui_fill(b.x, yb, b.w, t, 246, 238, 220, 60);
            ui_fill(b.x + b.w * pan_x / lw, yb, b.w * b.w / lw, t, 246, 238, 220, 190);
        }
        if (lh > b.h) ui_fill(b.x + b.w - t, b.y + b.h * pan_y / lh, t, b.h * b.h / lh, 246, 238, 220, 190);
    }
    float r;
    int stuck = big_shuffle(&x, &y, &r), lit = !hint.on || blink;
    if (stuck)
        ui_button_paint(x, y, 2 * r, paint_shuffle, 0, 0.65f, (lit ? UI_ON : 0) | UI_TINT);
    int play = status() == GS_PLAY;             /* the shell's tools: where it put them */
    ui_tool(T_HINT, ICON_HINT, play ? 0 : UI_DIM);
    ui_tool_paint(T_SHUFFLE, paint_shuffle, 0, 0.65f, UI_TINT | (stuck ? (lit ? UI_ON : 0) : UI_DIM));
    if (zoomable) ui_tool_paint(T_ZOOM, paint_glass, !zoom, 1, 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the tile a press at (x, y) means: the top tile there if it is free, else the free tile nearest the finger if it is
 * just beside one; *hit: a tile is under the finger */
static int tile_under(float x, float y, int *hit)
{
    *hit = 0;
    box b = last_box;
    if (near_view() && (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h)) return -1;   /* out of view */
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

static void tap(float x, float y)               /* a tap on the layout */
{
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

/* near: the middle of a tile in view (else the view brought to it) */
static void show_tile(int i)
{
    float x, y;
    place_xy(i, &x, &y);
    box b = last_box;
    if (x < b.x || y < b.y || x + fw + th > b.x + b.w || y + fh + th > b.y + b.h)
        centre_view(x - ox + fw / 2.0f, y - oy + fh / 2.0f);
}

static void use_hint(void)
{
    anim.on = 0;
    hint.on = 1;
    hint.t0 = ui_ms();
    hint.shuffle = !mj_hint(&G, &hint.a, &hint.b);
    sel = -1;
    if (!hint.shuffle && near_view()) {         /* the pair brought into view (both, when they fit) */
        float ax, ay, bx, by;
        place_xy(hint.a, &ax, &ay);
        place_xy(hint.b, &bx, &by);
        if (fabsf(ax - bx) + fw < last_box.w && fabsf(ay - by) + fh < last_box.h)
            centre_view((ax + bx) / 2 - ox + fw / 2.0f, (ay + by) / 2 - oy + fh / 2.0f);
        else show_tile(hint.a);
    }
    ui_sound(hint.shuffle ? SND_DENY : SND_TAP);
    ui_animate();
}

static void use_tool(int t)
{
    if (t == T_ZOOM) {                          /* near or whole: the tile picked (or the view's middle) kept */
        float mx = (last_box.x + last_box.w / 2 - ox) / fw, my = (last_box.y + last_box.h / 2 - oy) / fw;
        zoom = !zoom;
        layout(last_box);
        if (sel >= 0) show_tile(sel);
        else centre_view(mx * fw, my * fw);
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (status() != GS_PLAY) { ui_sound(SND_DENY); return; }
    if (t == T_HINT) use_hint();
    else if (mj_status(&G) == MJ_STUCK && !anim.on) shuffle();
    else ui_sound(SND_DENY);                    /* the shuffle: only when no pair is left */
}

static void press(float x, float y)
{
    ptr.on = 0;
    int t = ui_tool_hit(x, y);
    if (t >= 0) { use_tool(t); return; }
    if (status() != GS_PLAY) return;
    float bx, by, br;
    if (big_shuffle(&bx, &by, &br) && hypotf(x - bx, y - by) < 1.1f * br) { shuffle(); return; }
    if (!near_view()) { tap(x, y); return; }
    ptr.on = 1;                                 /* near: a tap, or the view dragged (decided as the finger moves) */
    ptr.panning = 0;
    ptr.x0 = x;
    ptr.y0 = y;
    ptr.px = pan_x;
    ptr.py = pan_y;
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    if (!ptr.panning && hypotf(x - ptr.x0, y - ptr.y0) > 0.25f * ui.unit) ptr.panning = 1;
    if (!ptr.panning) return;
    pan_x = ptr.px - (x - ptr.x0);
    pan_y = ptr.py - (y - ptr.y0);
    place_view();
    ui_animate();
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!ptr.on) return;
    ptr.on = 0;
    if (!ptr.panning) tap(ptr.x0, ptr.y0);
}

/* ---- the bar: the tiles left ------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    bar_textures();
    ui_bar_count(b, t_small, G.tiles);
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
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(mahjong_game)
