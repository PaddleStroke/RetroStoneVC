/*
 * Dominosa in RetroStone Board, for one player: the house plank with a cell inlaid for each number, the numbers
 * burned in, and the player's dominoes laid on them as the house's bones: ivory faces with ebony pips (up to eight)
 * on ebony backs, a brass pin in the middle (the rules: dominosa_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag from a cell to its neighbour (or tap one, then the other): a bone is laid on the two, and the bones on them go.
 * Tap a bone to take it away. A domino laid twice turns red. The lamp at the right of the bar: a hint (a wrong bone
 * taken away, else the easiest right one laid, in gold light). Undo takes back one action, all the way to the start.
 * The bar: the moves. Solved: a wave of gold light from the last bone.
 *
 * The bones are drawn here, not with the dominoes game's code (that is private to dominoes.c, and draws pips up to
 * six only): the same look, eight pips at most.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/dominosa.h"
#include "games/logicgrid2_ui.h"
#include "games/puzzles.h"
#include "shell.h"

#define FRAME 0.035f
#define CACHE 64
#define POP_MS 220

static const art_style IVORY = {{244, 236, 218}, {60, 48, 40}, {120, 104, 90}, {30, 26, 24}, 0.22f, 0};
static const art_style EBONY = {{58, 52, 50}, {14, 12, 12}, {120, 110, 104}, {30, 26, 24}, 0.35f, 0};
static const art_style BRASS = {{206, 162, 70}, {92, 62, 22}, {120, 90, 40}, {255, 236, 170}, 0.65f, 0};
static const float CELL_TONE[3] = {1.06f, 1.04f, 1.02f};

static dsa_game G;
static int pending = -1, serial;
static struct { int on, a, b; float x, y; } ptr;
static struct { int cell; Uint32 t0; } hint = {-1, 0}, pop = {-1, 0};
static lg2_wave win;
static lg2_bar barl;
static lg2_buttons buttons;

static box last_box;
static float bx, by, bw, bh, gx, gy, cs;
static int fr, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_small;

/* ---- the bones --------------------------------------------------------------------------------------------------- */
/* a half's pips, 0..8, in its square (x, y 0..1) */
static int pips(int v, float xy[9][2])
{
    static const float at[3] = {0.24f, 0.5f, 0.76f};
    int k = 0;
    switch (v) {
    case 1: xy[k][0] = at[1], xy[k++][1] = at[1]; break;
    case 2: case 3:
        xy[k][0] = at[0], xy[k++][1] = at[0];
        xy[k][0] = at[2], xy[k++][1] = at[2];
        if (v == 3) xy[k][0] = at[1], xy[k++][1] = at[1];
        break;
    case 4: case 5:
        for (int i = 0; i < 4; i++) xy[k][0] = at[i % 2 * 2], xy[k++][1] = at[i / 2 * 2];
        if (v == 5) xy[k][0] = at[1], xy[k++][1] = at[1];
        break;
    case 6: case 7:
        for (int i = 0; i < 6; i++) xy[k][0] = at[i % 3], xy[k++][1] = at[i / 3 * 2];
        if (v == 7) xy[k][0] = at[1], xy[k++][1] = at[1];
        break;
    case 8:
        for (int i = 0; i < 9; i++)
            if (i != 4) xy[k][0] = at[i % 3], xy[k++][1] = at[i / 3];
        break;
    default: break;
    }
    return k;
}

static void render_over(art_fig *f, art_style st, int w, int h, float scale, uint8_t *out)
{
    uint8_t *tmp = malloc((size_t)w * h * 4);
    if (tmp) {
        st.line = fmaxf(0.02f, 1.2f / scale);
        art_render(f, &st, w, h, scale, 0, 0, tmp);
        art_blend(out, w, h, tmp, w, h, 0, 0);
        free(tmp);
    }
    art_fig_free(f);
}

static void rrect(art_fig *f, int role, float x0, float y0, float x1, float y1, float r)
{
    float p[2 * 44];
    int n = 0;
    for (int c = 0; c < 4; c++) {
        float cx = c == 0 || c == 3 ? x1 - r : x0 + r, cy = c < 2 ? y1 - r : y0 + r;
        for (int i = 0; i <= 10; i++) {
            float a = (c * 90 + i * 9) * PUZ_PI / 180;
            p[n++] = cx + r * cosf(a);
            p[n++] = cy + r * sinf(a);
        }
    }
    art_poly(f, role, p, n / 2);
}

/* a bone a | b lying (a on the left), len x len / 2 pixels */
static void paint_bone(int a, int b, int len, uint8_t *o)
{
    int h = len / 2;
    float scale = (float)h, t = 0.075f, fu = 2 - t, fv = 1 - t;
    memset(o, 0, (size_t)len * h * 4);
    art_fig *f = art_fig_new();                 /* the ebony back, its edge showing at the lower right */
    rrect(f, ART_PART, 0, 0, 2, 1, 0.16f);
    render_over(f, EBONY, len, h, scale, o);
    f = art_fig_new();                          /* the ivory face, its pips, the line across */
    rrect(f, ART_PART, 0, 0, fu, fv, 0.15f);
    art_rect(f, ART_MARK, fu / 2 - 0.016f, 0.13f, fu / 2 + 0.016f, fv - 0.13f);
    for (int half = 0; half < 2; half++) {
        float xy[9][2];
        int k = pips(half ? b : a, xy), many = (half ? b : a) >= 7;
        for (int i = 0; i < k; i++)
            art_ellipse(f, ART_MARK, half * fu / 2 + (0.05f + 0.9f * xy[i][0]) * fu / 2, (0.05f + 0.9f * xy[i][1]) * fv,
                        many ? 0.07f : 0.08f, many ? 0.07f : 0.08f);
    }
    render_over(f, IVORY, len, h, scale, o);
    f = art_fig_new();                          /* the brass pin */
    art_ellipse(f, ART_PART, fu / 2, fv / 2, 0.06f, 0.06f);
    art_ellipse(f, ART_MARK, fu / 2 - 0.018f, fv / 2 - 0.018f, 0.018f, 0.018f);
    render_over(f, BRASS, len, h, scale, o);
}

typedef struct { int key, len; unsigned used; SDL_Texture *t; } cached;
static cached cache[CACHE];
static unsigned tick;

static SDL_Texture *bone_tex(int a, int b, int len)
{
    if (len < 8) return NULL;
    int key = a * 16 + b, slot = 0;
    for (int i = 0; i < CACHE; i++) {
        if (cache[i].t && cache[i].key == key && cache[i].len == len) { cache[i].used = ++tick; return cache[i].t; }
        if (!cache[i].t || (cache[slot].t && cache[i].used < cache[slot].used)) slot = i;
    }
    ui_free(&cache[slot].t);
    uint8_t *buf = malloc((size_t)len * (len / 2) * 4);
    if (!buf) return NULL;
    paint_bone(a, b, len, buf);
    cache[slot] = (cached){key, len, ++tick, ui_tex(len, len / 2, buf)};
    free(buf);
    return cache[slot].t;
}

/* a bone a | b centred on (cx, cy), len long, turned deg (90: standing, a on top); red: laid twice */
static void draw_bone(int a, int b, float cx, float cy, float len, float deg, int red, float k)
{
    SDL_Texture *t = bone_tex(a, b, (int)(len + 0.5f));
    if (!t) return;
    float off = 0.04f * len;
    SDL_SetTextureColorMod(t, 0, 0, 0);         /* its shadow */
    SDL_SetTextureAlphaMod(t, 70);
    ui_put_rot(t, cx + 0.6f * off, cy + off, len * k, len / 2 * k, deg);
    SDL_SetTextureAlphaMod(t, 255);
    if (red) SDL_SetTextureColorMod(t, 255, 150, 130);
    else SDL_SetTextureColorMod(t, 255, 255, 255);
    ui_put_rot(t, cx, cy, len * k, len / 2 * k, deg);
    SDL_SetTextureColorMod(t, 255, 255, 255);
}

/* ---- the board --------------------------------------------------------------------------------------------------- */
/* the plank, each cell a lighter inlay with its number burned in */
static void paint_board(uint8_t *o, int W, int H, int f, const dsa_puz *p, float q, float ox, float oy, int digits)
{
    lg2_plank(o, W, H, f);
    float in = 0.05f * q;
    for (int c = 0; c < p->w * p->h; c++) {
        float x = ox + c % p->w * q, y = oy + c / p->w * q;
        race_inlay(o, W, H, x + in, y + in, q - 2 * in, q - 2 * in, 0.1f * q, CELL_TONE, fmaxf(1.5f, 0.06f * q));
        if (digits) puz_burn(o, W, H, p->num[c], x + q / 2, y + q * 0.53f, 0.58f * q);
    }
}

/* the token: a bone 6 | 3 */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    int len = (int)(size * 0.94f) & ~1, h = len / 2;
    uint8_t *b = malloc((size_t)len * h * 4);
    if (!b) return;
    paint_bone(6, 3, len, b);
    art_blend(rgba, size, size, b, len, h, (size - len) / 2, (size - h) / 2);
    free(b);
}

static void layout(box b)
{
    last_box = b;
    int w = G.p.w ? G.p.w : 8, h = G.p.h ? G.p.h : 7;
    float S = floorf(fminf(b.w, b.h));
    fr = (int)(S * FRAME);
    cs = floorf((S - 2 * fr - 0.3f * S / w) / w);
    float m = floorf((S - w * cs) / 2);         /* the room round the grid inside the board */
    bw = S;
    bh = floorf(h * cs + 2 * m);
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - bh) / 2);
    gx = bx + m;
    gy = by + m;
    int key = ((int)S * 16 + w) * 64 + serial % 64;
    if (made_key != key) {
        made_key = key;
        ui_free(&t_board);
        paint_board(ui_buf((int)bw, (int)bh), (int)bw, (int)bh, fr, &G.p, cs, m, m, 1);
        t_board = ui_tex((int)bw, (int)bh, ui_buf((int)bw, (int)bh));
        for (int i = 0; i < CACHE; i++) ui_free(&cache[i].t);
    }
    lg2_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    pending = -1;
    hint.cell = pop.cell = -1;
    win.on = 0;
    memset(&ptr, 0, sizeof ptr);
}

static void start(int variant)
{
    dsa_new(&G, variant < 0 || variant >= DSA_VARIANTS ? 3 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return dsa_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return lg2_wave_busy(&win, G.p.w, G.p.h); }

static void after(int cell)
{
    if (dsa_solved(&G) && !win.on) {
        lg2_wave_start(&win, (float)(cell % G.p.w), (float)(cell / G.p.w));
        pending = -1;
    }
    ui_animate();
}

static int undo(void)
{
    if (dsa_undo(&G) < 0) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = dsa_script(&G, moves);
    if (dsa_solved(&G)) after(0);
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static int cell_at(float x, float y)
{
    int cx = (int)floorf((x - gx) / cs), cy = (int)floorf((y - gy) / cs);
    if (cx < 0 || cy < 0 || cx >= G.p.w || cy >= G.p.h) return -1;
    return cy * G.p.w + cx;
}

/* the neighbour of a the finger points at (the way it has gone furthest), -1 while it stays on a */
static int toward(int a, float x, float y)
{
    int w = G.p.w;
    float dx = x - (gx + (a % w + 0.5f) * cs), dy = y - (gy + (a / w + 0.5f) * cs);
    if (fabsf(dx) < 0.55f * cs && fabsf(dy) < 0.55f * cs) return -1;
    int b = fabsf(dx) >= fabsf(dy) ? (dx > 0 ? a + 1 : a - 1) : (dy > 0 ? a + w : a - w);
    if (b < 0 || b >= w * G.p.h || (fabsf(dx) >= fabsf(dy) && b / w != a / w)) return -1;
    return b;
}

static int adjacent(int a, int b)
{
    int w = G.p.w;
    return (a / w == b / w && abs(a - b) == 1) || abs(a - b) == w;
}

static void bone_between(int c, int o, float k)
{
    int w = G.p.w, a = c < o ? c : o, b = c < o ? o : c;
    float x = gx + (a % w + 0.5f) * cs, y = gy + (a / w + 0.5f) * cs;
    int vertical = b - a == w;
    float cx = vertical ? x : x + cs / 2, cy = vertical ? y + cs / 2 : y;
    draw_bone(G.p.num[a], G.p.num[b], cx, cy, 1.92f * cs, vertical ? 90.0f : 0.0f, !dsa_solved(&G) && dsa_clash(&G, a), k);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int w = G.p.w, h = G.p.h;
    Uint32 now = ui_ms();
    ui_put(t_board, bx, by, bw, bh);
    if (hint.cell >= 0) {
        int o = G.link[hint.cell] ? G.link[hint.cell] - 1 : hint.cell;
        float x = gx + ((hint.cell % w + o % w) / 2.0f + 0.5f) * cs, y = gy + ((hint.cell / w + o / w) / 2.0f + 0.5f) * cs;
        if (!lg2_hint_glow(hint.t0, x, y, 1.4f * cs)) hint.cell = -1;
    }
    for (int c = 0; c < w * h; c++) {
        lg2_wave_cell(&win, (float)(c % w), (float)(c / w), gx + (c % w + 0.5f) * cs, gy + (c / w + 0.5f) * cs, cs);
        if (dsa_clash(&G, c) && !dsa_solved(&G) && G.link[c] - 1 > c) {
            int o = G.link[c] - 1;
            ui_mark_glow(gx + ((c % w + o % w) / 2.0f + 0.5f) * cs, gy + ((c / w + o / w) / 2.0f + 0.5f) * cs, 2.2f * cs);
        }
    }
    for (int c = 0; c < w * h; c++) {           /* the bones */
        int o = G.link[c] - 1;
        if (o <= c) continue;
        float k = 1;
        if (pop.cell == c || pop.cell == o) {
            float t = (now - pop.t0) / (float)POP_MS;
            if (t < 1) { k = 1 + 0.12f * (1 - puz_ease(t)); ui_animate(); }
        }
        bone_between(c, o, k);
    }
    lg2_wave_tick(&win, w, h);
    if (pending >= 0) ui_mark_select(gx + pending % w * cs, gy + pending / w * cs, cs, cs);
    if (ptr.on && ptr.b >= 0) {                 /* the bone being laid */
        int a = ptr.a < ptr.b ? ptr.a : ptr.b, b = ptr.a < ptr.b ? ptr.b : ptr.a;
        ui_mark_select(gx + a % w * cs, gy + a / w * cs, b - a == 1 ? 2 * cs : cs, b - a == 1 ? cs : 2 * cs);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void lay(int a, int b)
{
    pending = -1;
    if (!dsa_lay(&G, a, b)) { ui_sound(SND_DENY); return; }
    pop.cell = a < b ? a : b;
    pop.t0 = ui_ms();
    ui_sound(SND_PLACE);
    after(a);
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (dsa_solved(&G)) return;
    int c = cell_at(x, y);
    if (c < 0) {
        if (pending >= 0) pending = -1, ui_animate();
        return;
    }
    ptr.on = 1;
    ptr.a = c;
    ptr.b = -1;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ptr.on) return;
    ptr.b = toward(ptr.a, x, y);
    ui_animate();
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!ptr.on) return;
    ptr.on = 0;
    ui_animate();
    int c = ptr.a;
    if (ptr.b >= 0) { lay(c, ptr.b); return; }
    if (pending >= 0 && pending != c && adjacent(pending, c)) { lay(pending, c); return; }
    if (pending < 0 && G.link[c]) {             /* a bone tapped: taken away */
        dsa_lift(&G, c);
        ui_sound(SND_TAP);
        return;
    }
    pending = pending == c ? -1 : c;
    ui_sound(SND_TAP);
}

/* ---- the bar: the moves, the lamp -------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    lg2_bar_draw(&barl, &buttons, b, t_small, G.hist.moves, !dsa_solved(&G));
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (dsa_solved(&G) || !lg2_bar_lamp(&barl, x, y)) return 0;
    pending = -1;
    int c = dsa_hint(&G);
    if (c < 0) { ui_sound(SND_DENY); return 1; }
    hint.cell = c;
    hint.t0 = ui_ms();
    if (G.link[c]) pop.cell = c < G.link[c] - 1 ? c : G.link[c] - 1, pop.t0 = hint.t0;
    ui_sound(SND_PLACE);
    after(c);
    return 1;
}

/* ---- the menu's and the picker's cards: a puzzle of the set and level, some of its bones laid, and as many ebony
 * stones as its level; the menu's a double-4 half done ------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? 0 : variant, n = dsa_set_of(v);
    dsa_puz p;
    uint8_t sol[DSA_CELLS];
    dsa_make(n, dsa_level_of(v), 2026u + (uint32_t)v, &p, sol);
    int W = variant < 0 ? size : (int)(0.84f * size), f = (int)(W * FRAME);
    float q = floorf((W - 2 * f - 0.3f * W / p.w) / p.w), m = floorf((W - p.w * q) / 2);
    int H = (int)(p.h * q + 2 * m);
    uint8_t *b = calloc((size_t)W * H, 4);
    if (!b) return;
    paint_board(b, W, H, f, &p, q, m, m, q >= 9);
    int len = (int)(1.92f * q) & ~1, k = 0;
    uint8_t *bone = len >= 8 ? malloc((size_t)len * (len / 2) * 4) : NULL;
    uint8_t *turned = bone ? malloc((size_t)len * (len / 2) * 4) : NULL;
    for (int c = 0; bone && turned && c < p.w * p.h; c++) {
        int o = sol[c];
        if (o < c || k++ % (variant < 0 ? 2 : 4)) continue;
        paint_bone(p.num[c], p.num[o], len, bone);
        int vertical = o - c == p.w, bwid = vertical ? len / 2 : len, bhgt = vertical ? len : len / 2;
        if (vertical)                           /* standing: the picture turned a quarter (its left end on top) */
            for (int y = 0; y < len; y++)
                for (int x = 0; x < len / 2; x++)
                    memcpy(turned + 4 * (y * (len / 2) + x), bone + 4 * ((len / 2 - 1 - x) * len + y), 4);
        float cx = m + (c % p.w + 0.5f) * q + (vertical ? 0 : q / 2), cy = m + (c / p.w + 0.5f) * q + (vertical ? q / 2 : 0);
        art_blend(b, W, H, vertical ? turned : bone, bwid, bhgt, (int)(cx - bwid / 2.0f), (int)(cy - bhgt / 2.0f));
    }
    free(bone);
    free(turned);
    int ox = (size - W) / 2, oy = variant < 0 ? (size - H) / 2 : (int)(0.84f * size - H) / 2;
    art_blend(rgba, size, size, b, W, H, ox, oy);
    free(b);
    if (variant >= 0) lg2_stones(rgba, size, (int)(0.84f * size) + (size - (int)(0.84f * size) - (int)(0.1f * size)) / 2,
                                 dsa_level_of(v));
}

static const game dominosa_game = {
    .id = "dominosa", .order = 417, .variants = DSA_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(dominosa_game)
