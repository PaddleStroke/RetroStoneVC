/*
 * Dominosa in RetroStone Board, for one player: the house plank with a cell inlaid for each number, the numbers
 * burned in, and the player's dominoes laid on them as the house's bones: ivory faces with ebony pips (up to eight)
 * on ebony backs, a brass pin in the middle (the rules: dominosa_rules.c; what the logic grids share: bench.h,
 * bench_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Drag from a cell to its neighbour (or tap one, then the other): a bone is laid on the two, and the bones on them go.
 * Tap a bone to take it away. A domino laid twice turns red. The shell's tool, the lamp: a hint (a wrong bone
 * taken away, else the easiest right one laid, in gold light). Undo takes back one action, all the way to the start.
 * The bar: the moves. Solved: a wave of gold light from the last bone; the stars count the hints and the mistakes
 * (bench.h).
 *
 * The bones are drawn here, not with the dominoes game's code (that is private to dominoes.c, and draws pips up to
 * six only): the same look, eight pips at most.
 *
 * The shell's level screen chooses a level (150 in six chapters, dominosa_levels.h), or its die tab a puzzle made at
 * random (the variants). The grid stands turned a quarter when that makes its cells bigger (a phone held upright).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/dominosa.h"
#include "games/dominosa_levels.h"
#include "shell.h"

#define CACHE 64

static const art_style IVORY = {{244, 236, 218}, {60, 48, 40}, {120, 104, 90}, {30, 26, 24}, 0.22f, 0};
static const art_style EBONY = {{58, 52, 50}, {14, 12, 12}, {120, 110, 104}, {30, 26, 24}, 0.35f, 0};
static const art_style BRASS = {{206, 162, 70}, {92, 62, 22}, {120, 90, 40}, {255, 236, 170}, 0.65f, 0};
static const float CELL_TONE[3] = {1.06f, 1.04f, 1.02f};

static dsa_game G;
static int pending = -1, serial;
static struct { int on, a, b; float x, y; } ptr;
static bench_flash hint = {-1, 0}, pop = {-1, 0};
static bench_wave win;

static box last_box;
static float bx, by, bw, bh, gx, gy, cs;
static int fr, turned, made_key = -1, made_small = -1;
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
/* a cell's inlay: the wood under a rounded square tinted (the grain stays), a darker seam round it (seam pixels) */
static void inlay(uint8_t *o, int pw, int ph, float x0, float y0, float s, float rad, float seam)
{
    for (int y = (int)floorf(y0); y < (int)ceilf(y0 + s); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x0 + s); x++) {
            if (x < 0 || y < 0 || x >= pw || y >= ph) continue;
            float px = x + 0.5f - x0, py = y + 0.5f - y0;
            float dx = fmaxf(rad - px, px - (s - rad)), dy = fmaxf(rad - py, py - (s - rad)), d;
            if (dx > 0 && dy > 0) d = rad - hypotf(dx, dy);                 /* in a rounded corner */
            else d = fminf(fminf(px, s - px), fminf(py, s - py));           /* inside: to the nearest side */
            float in = clampf(d + 0.5f, 0, 1);
            if (in <= 0) continue;
            float k = d < seam ? 0.72f + 0.28f * clampf(d / seam - 0.4f, 0, 1) : 1;   /* the seam */
            uint8_t *p = o + 4 * ((size_t)y * pw + x);
            for (int c = 0; c < 3; c++) {
                float v = p[c] * CELL_TONE[c] * k;
                p[c] = (uint8_t)clampf(p[c] + (v - p[c]) * in, 0, 255);
            }
        }
}

/* the plank, each cell a lighter inlay with its number burned in; turned: the grid transposed (the game's columns
 * running down) */
static void paint_board(uint8_t *o, int W, int H, int f, const dsa_puz *p, float q, float ox, float oy, int digits,
                        int turned)
{
    bench_plank(o, W, H, f);
    float in = 0.05f * q;
    for (int c = 0; c < p->w * p->h; c++) {
        int col = c % p->w, row = c / p->w;
        float x = ox + (turned ? row : col) * q, y = oy + (turned ? col : row) * q;
        inlay(o, W, H, x + in, y + in, q - 2 * in, 0.1f * q, fmaxf(1.5f, 0.06f * q));
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


/* the grid as big as the room allows, turned a quarter (its columns running down) when that makes the cells bigger:
 * a wide grid on a phone held upright (ui_grid_fit) */
static void layout(box b)
{
    last_box = b;
    int w = G.p.w ? G.p.w : 8, h = G.p.h ? G.p.h : 7;
    float S = floorf(fminf(b.w, b.h)), m = floorf(S * BENCH_FRAME + 0.15f * S / w);   /* the room round the grid */
    ui_grid fit = ui_grid_fit(b, w, h, m / fmaxf(1, S / (w > h ? w : h)));
    turned = fit.turned;
    int sw = turned ? h : w, sh = turned ? w : h;   /* the grid as it stands on the screen */
    fr = (int)(S * BENCH_FRAME);
    cs = floorf(fminf((b.w - 2 * m) / sw, (b.h - 2 * m) / sh));
    bw = sw * cs + 2 * m;
    bh = sh * cs + 2 * m;
    bx = floorf(b.x + (b.w - bw) / 2);
    by = floorf(b.y + (b.h - bh) / 2);
    gx = bx + m;
    gy = by + m;
    int key = (((int)bw * 16 + w) * 64 + serial % 64) * 2 + turned;
    if (made_key != key) {
        made_key = key;
        ui_free(&t_board);
        paint_board(ui_buf((int)bw, (int)bh), (int)bw, (int)bh, fr, &G.p, cs, m, m, 1, turned);
        t_board = ui_tex((int)bw, (int)bh, ui_buf((int)bw, (int)bh));
        for (int i = 0; i < CACHE; i++) ui_free(&cache[i].t);
    }
    bench_small(&t_small, &made_small, token);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    pending = -1;
    hint.at = pop.at = -1;
    win.on = 0;
    memset(&ptr, 0, sizeof ptr);
}


static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

/* a level of the shell's level screen (dominosa_levels.h) */
static void start(int level)
{
    level = level < 0 ? 0 : level >= DSA_STAGES ? DSA_STAGES - 1 : level;
    if (!dsa_open(&G, dsa_levels[level])) dsa_new(&G, 0, 1);
    begin();
}

/* a puzzle made at random (the level screen's die tab): a set and a level */
static void start_random(int variant)
{
    dsa_new(&G, variant < 0 || variant >= DSA_VARIANTS ? 3 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return dsa_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return bench_wave_busy(&win); }
static int score(void) { return G.hist.moves; }
static int stars(void) { return ui_logic_stars(G.hist.hints, G.hist.mistakes); }

static void after(int cell)
{
    if (dsa_solved(&G) && !win.on) {
        bench_wave_start(&win, (cell % G.p.w) + 0.5f, (cell / G.p.w) + 0.5f, G.p.w, G.p.h);
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

/* ---- the board on the screen (turned: the game's columns run down the screen, its rows across) ------------------- */
static void screen_of(int c, int *col, int *row)   /* a cell's place on the screen */
{
    int x = c % G.p.w, y = c / G.p.w;
    *col = turned ? y : x;
    *row = turned ? x : y;
}

static int cell_of(int col, int row)            /* the cell at a place on the screen (-1: none) */
{
    int x = turned ? row : col, y = turned ? col : row;
    return x < 0 || y < 0 || x >= G.p.w || y >= G.p.h ? -1 : y * G.p.w + x;
}

static float mid_x(int c)
{
    int col, row;
    screen_of(c, &col, &row);
    return gx + (col + 0.5f) * cs;
}

static float mid_y(int c)
{
    int col, row;
    screen_of(c, &col, &row);
    return gy + (row + 0.5f) * cs;
}

static int cell_at(float x, float y)
{
    if (x < gx || y < gy) return -1;
    return cell_of((int)((x - gx) / cs), (int)((y - gy) / cs));
}

/* the neighbour of a the finger points at (the way it has gone furthest), -1 while it stays on a */
static int toward(int a, float x, float y)
{
    int col, row;
    screen_of(a, &col, &row);
    float dx = x - mid_x(a), dy = y - mid_y(a);
    if (fabsf(dx) < 0.55f * cs && fabsf(dy) < 0.55f * cs) return -1;
    if (fabsf(dx) >= fabsf(dy)) col += dx > 0 ? 1 : -1;
    else row += dy > 0 ? 1 : -1;
    return cell_of(col, row);
}

static int adjacent(int a, int b)
{
    int w = G.p.w;
    return (a / w == b / w && abs(a - b) == 1) || abs(a - b) == w;
}

/* a cell picked out, or the two of a bone being laid */
static void select_cells(int a, int b)
{
    float x0 = fminf(mid_x(a), mid_x(b)) - cs / 2, y0 = fminf(mid_y(a), mid_y(b)) - cs / 2;
    ui_mark_select(x0, y0, fabsf(mid_x(a) - mid_x(b)) + cs, fabsf(mid_y(a) - mid_y(b)) + cs);
}

/* the bone on c and o: its lower-numbered cell at its left or top end (turning the grid keeps that) */
static void bone_between(int c, int o, float k)
{
    int a = c < o ? c : o, b = c < o ? o : c, standing = fabsf(mid_x(a) - mid_x(b)) < 1;
    draw_bone(G.p.num[a], G.p.num[b], (mid_x(a) + mid_x(b)) / 2, (mid_y(a) + mid_y(b)) / 2, 1.92f * cs,
              standing ? 90.0f : 0.0f, !dsa_solved(&G) && dsa_clash(&G, a), k);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    int w = G.p.w, h = G.p.h;
    ui_put(t_board, bx, by, bw, bh);
    if (hint.at >= 0) {
        int o = G.link[hint.at] ? G.link[hint.at] - 1 : hint.at;
        bench_glow(&hint, (mid_x(hint.at) + mid_x(o)) / 2, (mid_y(hint.at) + mid_y(o)) / 2, 2.6f * 1.4f * cs);
    }
    for (int c = 0; c < w * h; c++) {
        bench_wave_at(&win, c % w + 0.5f, c / w + 0.5f, mid_x(c), mid_y(c), 2.3f * cs);
        if (dsa_clash(&G, c) && !dsa_solved(&G) && G.link[c] - 1 > c) {
            int o = G.link[c] - 1;
            ui_mark_glow((mid_x(c) + mid_x(o)) / 2, (mid_y(c) + mid_y(o)) / 2, 2.2f * cs);
        }
    }
    for (int c = 0; c < w * h; c++) {           /* the bones */
        int o = G.link[c] - 1;
        if (o <= c) continue;
        float k = bench_pop(&pop, c, 0.12f);
        bone_between(c, o, k);
    }
    bench_wave_tick(&win);
    ui_tool(0, ICON_HINT, dsa_solved(&G) ? UI_DIM : 0);
    if (pending >= 0) select_cells(pending, pending);
    if (ptr.on && ptr.b >= 0) select_cells(ptr.a, ptr.b);   /* the bone being laid */
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void lay(int a, int b)
{
    pending = -1;
    if (!dsa_lay(&G, a, b)) { ui_sound(SND_DENY); return; }
    bench_flash_start(&pop, a < b ? a : b);
    ui_sound(SND_PLACE);
    after(a);
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    if (dsa_solved(&G)) return;
    if (ui_tool_hit(x, y) == 0) {               /* the shell's tool: the hint */
        pending = -1;
        int c = dsa_hint(&G);
        if (c < 0) { ui_sound(SND_DENY); return; }
        bench_flash_start(&hint, c);
        if (G.link[c]) bench_flash_start(&pop, c < G.link[c] - 1 ? c : G.link[c] - 1);
        ui_sound(SND_PLACE);
        after(c);
        return;
    }
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
    bench_bar_moves(b, t_small, G.hist.moves);
}

/* ---- the menu's and the picker's cards: a puzzle of the set and level, some of its bones laid, and as many ebony
 * stones as its level; the menu's a double-4 half done ------------------------------------------------------------- */
/* a puzzle on a picture W wide (its height in *H): every `every`-th bone of the solution laid, the numbers when big
 * enough; NULL when out of memory (the caller frees it) */
static uint8_t *paint_card(const dsa_puz *p, const uint8_t *sol, int every, int W, int *H)
{
    int f = (int)(W * BENCH_FRAME);
    float q = floorf((W - 2 * f - 0.3f * W / p->w) / p->w), m = floorf((W - p->w * q) / 2);
    *H = (int)(p->h * q + 2 * m);
    uint8_t *b = calloc((size_t)W * *H, 4);
    if (!b) return NULL;
    paint_board(b, W, *H, f, p, q, m, m, q >= 9, 0);
    int len = (int)(1.92f * q) & ~1, k = 0;
    uint8_t *bone = len >= 8 ? malloc((size_t)len * (len / 2) * 4) : NULL;
    uint8_t *turned = bone ? malloc((size_t)len * (len / 2) * 4) : NULL;
    for (int c = 0; bone && turned && c < p->w * p->h; c++) {
        int o = sol[c];
        if (o < c || k++ % every) continue;
        paint_bone(p->num[c], p->num[o], len, bone);
        int vertical = o - c == p->w, bwid = vertical ? len / 2 : len, bhgt = vertical ? len : len / 2;
        if (vertical)                           /* standing: the picture turned a quarter (its left end on top) */
            for (int y = 0; y < len; y++)
                for (int x = 0; x < len / 2; x++)
                    memcpy(turned + 4 * (y * (len / 2) + x), bone + 4 * ((len / 2 - 1 - x) * len + y), 4);
        float cx = m + (c % p->w + 0.5f) * q + (vertical ? 0 : q / 2);
        float cy = m + (c / p->w + 0.5f) * q + (vertical ? q / 2 : 0);
        art_blend(b, W, *H, vertical ? turned : bone, bwid, bhgt, (int)(cx - bwid / 2.0f), (int)(cy - bhgt / 2.0f));
    }
    free(bone);
    free(turned);
    return b;
}

static void card(int variant, int size, int bare, uint8_t *rgba)
{
    int v = variant < 0 ? 0 : variant, n = dsa_set_of(v), H;
    dsa_puz p;
    uint8_t sol[DSA_CELLS];
    dsa_make(n, dsa_level_of(v), 2026u + (uint32_t)v, &p, sol);
    int W = variant < 0 || bare ? size : (int)(0.84f * size);
    uint8_t *b = paint_card(&p, sol, variant < 0 ? 2 : 4, W, &H);
    if (!b) return;
    int ox = (size - W) / 2, oy = variant < 0 || bare ? (size - H) / 2 : (int)(0.84f * size - H) / 2;
    art_blend(rgba, size, size, b, W, H, ox, oy);
    free(b);
    if (variant >= 0 && !bare) bench_stones(rgba, size, (int)(0.84f * size), dsa_level_of(v));
}

static void thumb(int variant, int size, uint8_t *rgba) { card(variant, size, 0, rgba); }

/* the picker's sizes (game.axes, above the levels): a puzzle of the size at the medium level, on the whole card */
static void size_pic(int i, int size, uint8_t *rgba) { card(i * 3 + 1, size, 1, rgba); }

/* a chapter's tab: its last level (its biggest set), solved */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= DSA_CHAPTERS ? DSA_CHAPTERS - 1 : chapter, H;
    static dsa_game T;
    if (!dsa_open(&T, dsa_levels[(ch + 1) * DSA_STAGES / DSA_CHAPTERS - 1])) return;
    int W = (int)(0.94f * size);
    uint8_t *b = paint_card(&T.p, T.sol, 1, W, &H);
    if (!b) return;
    art_blend(rgba, size, size, b, W, H, (size - W) / 2, (size - H) / 2);
    free(b);
}

static const game dominosa_game = {
    .id = "dominosa", .order = 417, .variants = DSA_VARIANTS,
    .levels = DSA_STAGES, .chapters = DSA_CHAPTERS, .chapter_icon = chapter_icon, .start_random = start_random,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .axes = {DSA_SIZES, 3}, .axis_pic = {size_pic, NULL},
    .score = score, .level_stars = stars, .fill = 1, .tools = 1,
};
REGISTER_GAME(dominosa_game)
