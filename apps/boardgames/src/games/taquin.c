/*
 * The 15 Puzzle in RetroStone Board, for one player: a walnut tray, its floor sunk in shadow, and the tiles, all cut
 * from one plank of the house wood (in order, the grain runs on from tile to tile), their numbers burned in; or cut
 * from a picture: the house's chess pieces on a board in the middle of a game, each tile's number small in its
 * corner (the rules: taquin_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a tile in the gap's row or column: it slides into the gap, and the tiles between with it. Or push it with a
 * finger towards the gap (past halfway it goes on; short of that it falls back). Undo takes back one slide, all the
 * way to the start. The bar: the tiles moved (a slide of three: three).
 * Solved: a gold light runs over the tiles in their order; a picture's missing piece then comes into the gap.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"
#include "games/puzzles.h"
#include "games/taquin.h"
#include "shell.h"

#define FRAME 0.035f
#define GAP 0.014f                              /* between the tiles, a part of the tray's inside */
#define SLIDE_MS 130
#define WAVE_STEP 60
#define WAVE_LEN 480
#define LAST_MS 420                             /* the picture's last piece coming in */

static tq_game G;
static struct { int on, n, place[TQ_MAX], from[TQ_MAX]; float f0; Uint32 t0; } slide;   /* tiles sliding */
static struct { int on; Uint32 t0; } win;
static struct { int on, place, k, dx, dy, moved; float x0, y0, off; } push;   /* a finger pushing tiles */

static box last_box;
static float bx, by, side;
static int fr, tile, gap, made_key = -1, made_small = -1;
static SDL_Texture *t_tray, *t_tile[TQ_PLACES], *t_small;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the chess position the picture shows (rank 8 first, files a..h): a piece in every 2 x 2 block of squares, none
 * in a block's top left square (the 4 x 4 tiles' numbers sit there) */
static const char *const POSITION[8] = {".r.q.k..", "p.p..ppp", ".n.b....", "...pPpb.", ".B.P.P..", "..N...P.",
                                        ".P...Q.P", "R.B.K..R"};

static void paint_picture(int P, uint8_t *o)    /* P x P: the board and its pieces */
{
    int sq = (P + 7) / 8, w = 8 * sq;
    uint8_t *b = malloc((size_t)w * w * 4);
    if (!b) return;
    art_board_n(8, 8, sq, 0, 1, b);
    int d = (int)(0.92f * sq);
    uint8_t *pc = calloc((size_t)d * d, 4);
    for (int r = 0; r < 8 && pc; r++)
        for (int f = 0; f < 8; f++) {
            char c = POSITION[r][f];
            if (c == '.') continue;
            const char *types = "pnbrqk", *t = strchr(types, c | 32);
            if (!t) continue;
            memset(pc, 0, (size_t)d * d * 4);
            art_piece(PAWN + (int)(t - types), c >= 'a' ? BLACK : WHITE, d, pc);
            art_blend(b, w, w, pc, d, d, f * sq + (sq - d) / 2, r * sq + (sq - d) / 2);
        }
    free(pc);
    for (int y = 0; y < P; y++) memcpy(o + 4 * (size_t)y * P, b + 4 * (size_t)y * w, (size_t)P * 4);
    free(b);
}

/* tile `home` (its place when solved) of an n x n tray, T pixels, cut from the source (n T x n T) */
static void paint_tile(int n, int picture, int home, int T, const uint8_t *src, uint8_t *o)
{
    int P = n * T, hx = home % n, hy = home / n;
    for (int y = 0; y < T; y++) memcpy(o + 4 * (size_t)y * T, src + 4 * ((size_t)(hy * T + y) * P + hx * T), (size_t)T * 4);
    puz_bevel(o, T, T, 0.07f * T, fmaxf(2, 0.065f * T), 0.34f);
    if (home == n * n - 1) return;              /* the missing piece: no number */
    if (!picture) {
        puz_burn(o, T, T, home + 1, T / 2.0f, T * 0.52f, 0.46f * T);
        return;
    }
    int d = (int)(0.3f * T);                    /* the number small, on a cream disc in the corner */
    uint8_t *disc = calloc((size_t)d * d, 4);
    if (!disc) return;
    static const uint8_t cream[3] = {246, 238, 220};
    art_disc(d, cream, 0.9f, 0, disc);
    int at = (int)(0.07f * T);
    art_blend(o, T, T, disc, d, d, at, at);
    free(disc);
    puz_digits(o, T, T, home + 1, at + d / 2.0f, at + d * 0.53f, (home + 1 >= 10 ? 0.5f : 0.56f) * d, PUZ_INK, 1, 1);
}

static void source(int n, int picture, int T, uint8_t *src)
{
    int P = n * T;
    if (picture) paint_picture(P, src);
    else art_board_n(1, 1, P, 0, 0, src);
}

/* the tray: the walnut frame, its floor sunk (darker, in the frame's shadow at the top and the left) */
static void paint_tray(int w, int f, uint8_t *o)
{
    art_board_n(1, 1, w - 2 * f, f, 0, o);
    float sh = 0.03f * w;
    for (int y = f; y < w - f; y++)
        for (int x = f; x < w - f; x++) {
            float e = fminf(x - f, y - f), k = 0.5f - 0.16f * clampf(1 - e / sh, 0, 1);
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
}

static void sizes(int n, float S, int *f, int *g, int *T)
{
    *f = (int)(S * FRAME);
    float inner = S - 2 * *f;
    *g = (int)fmaxf(2, inner * GAP);
    *T = (int)((inner - (n + 1) * *g) / n);
}

static void token(int player, int size, uint8_t *rgba)   /* a tile: 15 */
{
    (void)player;
    int T = (int)(0.86f * size);
    uint8_t *src = malloc((size_t)T * T * 4), *t = calloc((size_t)T * T, 4);
    if (src && t) {
        art_board_n(1, 1, T, 0, 0, src);
        memcpy(t, src, (size_t)T * T * 4);
        puz_bevel(t, T, T, 0.08f * T, fmaxf(1.5f, 0.07f * T), 0.34f);
        puz_burn(t, T, T, 15, T / 2.0f, T * 0.52f, 0.46f * T);
        art_blend(rgba, size, size, t, T, T, (size - T) / 2, (size - T) / 2);
    }
    free(src);
    free(t);
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h));
    sizes(G.n, S, &fr, &gap, &tile);
    side = S;
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    int key = ((int)S * 8 + G.n) * 2 + G.picture;
    if (made_key != key) {
        made_key = key;
        int w = (int)S;
        ui_free(&t_tray);
        paint_tray(w, fr, ui_buf(w, w));
        t_tray = ui_tex(w, w, ui_buf(w, w));
        int P = G.n * tile;
        uint8_t *src = malloc((size_t)P * P * 4);
        for (int i = 0; i < TQ_PLACES; i++) {
            ui_free(&t_tile[i]);
            if (!src || i >= G.n * G.n) continue;
            if (i == 0) source(G.n, G.picture, tile, src);
            paint_tile(G.n, G.picture, i, tile, src, ui_buf(tile, tile));
            t_tile[i] = ui_tex(tile, tile, ui_buf(tile, tile));
        }
        free(src);
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        token(0, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    slide.on = win.on = 0;
    memset(&push, 0, sizeof push);
}

static void begin(void)
{
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a designed level */
{
    if (!tq_level(&G, level)) tq_level(&G, 0);
    begin();
}

static void start_random(int variant)           /* a tray shuffled at random; its par found now */
{
    G.picture = 0;
    tq_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    G.par = tq_par(&G);
    begin();
}

static int status(void) { return tq_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }     /* the tiles moved on the way kept */
static int par(void) { return G.par; }
static int level_stars(void) { return ui_par_stars(G.moves, G.par); }

static Uint32 win_end(void)
{
    return (Uint32)((G.n * G.n - 1) * WAVE_STEP + WAVE_LEN + (G.picture ? LAST_MS : 0));
}

static int busy(void)
{
    Uint32 now = ui_ms();
    return (slide.on && now - slide.t0 < SLIDE_MS) || (win.on && now - win.t0 < win_end());
}

static void won(Uint32 at)
{
    if (!tq_solved(&G)) return;
    win.on = 1;
    win.t0 = at;
}

/* a slide from the place p: the tiles on the way move one step towards the gap; f0: how far they were pushed */
static void slide_from(int p, float f0)
{
    int n = G.n, gap0 = G.gap, k = tq_line(&G, p);
    if (!k) { ui_sound(SND_DENY); return; }
    int step = p % n == gap0 % n ? (p > gap0 ? n : -n) : (p > gap0 ? 1 : -1);
    slide.n = 0;
    for (int at = gap0 + step, i = 0; i < k; i++, at += step) {   /* each tile: from at to at - step */
        slide.place[slide.n] = at - step;
        slide.from[slide.n++] = at;
    }
    tq_tap(&G, p);
    slide.on = 1;
    slide.f0 = f0;
    slide.t0 = ui_ms();
    ui_sound(k > 1 ? SND_MOVE : SND_PLACE);
    won(slide.t0 + SLIDE_MS);
    ui_animate();
}

static int undo(void)
{
    calm();
    return tq_undo(&G) >= 0;
}

static int script(const char *moves)
{
    calm();
    int ok = tq_script(&G, moves);
    won(ui_ms());
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void place_xy(int p, float *x, float *y)   /* a place's centre */
{
    *x = bx + fr + gap + (p % G.n) * (tile + gap) + tile / 2.0f;
    *y = by + fr + gap + (p / G.n) * (tile + gap) + tile / 2.0f;
}

static int place_at(float x, float y)
{
    float ix = x - bx - fr - gap / 2.0f, iy = y - by - fr - gap / 2.0f, pitch = (float)(tile + gap);
    if (ix < 0 || iy < 0 || ix >= G.n * pitch || iy >= G.n * pitch) return -1;
    return (int)(iy / pitch) * G.n + (int)(ix / pitch);
}

static void tile_at(int t, float x, float y)
{
    ui_shadow_flat(x + 0.03f * tile, y + 0.05f * tile, 0.96f * tile, 0.96f * tile, 0.55f);
    ui_put_c(t_tile[t - 1], x, y, (float)tile, (float)tile);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_tray, bx, by, side, side);
    float st = slide.on ? (now - slide.t0) / (float)SLIDE_MS : 2;
    if (slide.on && st >= 1) slide.on = 0;
    float wt = win.on ? (float)(now - win.t0) : -1;
    float pitch = (float)(tile + gap);
    for (int p = 0; p < G.n * G.n; p++) {
        int t = G.at[p];
        if (!t) continue;
        float x, y;
        place_xy(p, &x, &y);
        for (int i = 0; slide.on && i < slide.n; i++)
            if (slide.place[i] == p) {          /* sliding in from its old place */
                float fx, fy, f = slide.f0 + (1 - slide.f0) * puz_ease(st);
                place_xy(slide.from[i], &fx, &fy);
                x = fx + (x - fx) * f;
                y = fy + (y - fy) * f;
            }
        if (push.on && push.moved)
            for (int i = 0, at = G.gap; i < push.k; i++) {   /* pushed by the finger */
                at += push.dx + push.dy * G.n;
                if (at == p) { x -= push.dx * push.off * pitch; y -= push.dy * push.off * pitch; }
            }
        if (wt >= 0) {
            float q = puz_pulse(wt, (t - 1) * WAVE_STEP, WAVE_LEN);
            if (q > 0) ui_mark_gold(x, y, 1.9f * tile * q);
        }
        tile_at(t, x, y);
        if (wt >= 0) {                          /* the tile itself lit as the light passes */
            float q = puz_pulse(wt, (t - 1) * WAVE_STEP, WAVE_LEN);
            if (q > 0) ui_mark_gold(x, y, 1.2f * tile * q);
        }
    }
    if (wt >= 0) {
        if (G.picture) {                        /* the missing piece comes in */
            float q = clampf((wt - (G.n * G.n - 1) * WAVE_STEP - WAVE_LEN * 0.5f) / LAST_MS, 0, 1);
            if (q > 0) {
                float x, y;
                place_xy(G.n * G.n - 1, &x, &y);
                SDL_SetTextureAlphaMod(t_tile[G.n * G.n - 1], (Uint8)(255 * q));
                float s = 1.25f - 0.25f * puz_ease(q);
                ui_put_c(t_tile[G.n * G.n - 1], x, y, tile * s, tile * s);
                SDL_SetTextureAlphaMod(t_tile[G.n * G.n - 1], 255);
            }
        }
        if (wt < win_end()) ui_animate();
    }
    if (slide.on) ui_animate();
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&push, 0, sizeof push);
    if (tq_solved(&G)) return;
    slide.on = 0;
    int p = place_at(x, y);
    if (p < 0) return;
    int k = tq_line(&G, p);
    if (!k) { ui_sound(SND_DENY); return; }
    push.on = 1;
    push.place = p;
    push.k = k;
    int n = G.n;                                /* the way from the gap to the tile */
    push.dx = p % n > G.gap % n ? 1 : p % n < G.gap % n ? -1 : 0;
    push.dy = p / n > G.gap / n ? 1 : p / n < G.gap / n ? -1 : 0;
    push.x0 = x;
    push.y0 = y;
}

static void motion(float x, float y)
{
    if (!push.on) return;
    float pitch = (float)(tile + gap), along = -((x - push.x0) * push.dx + (y - push.y0) * push.dy);
    push.off = clampf(along / pitch, 0, 1);
    if (fabsf(along) > 0.12f * pitch) push.moved = 1;
    ui_animate();
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!push.on) return;
    push.on = 0;
    ui_animate();
    if (!push.moved) { slide_from(push.place, 0); return; }
    if (push.off > 0.45f) slide_from(push.place, push.off);
}

/* ---- the bar: the tiles moved ------------------------------------------------------------------------------------ */
static void bar(int player, box b)
{
    (void)player;
    ui_bar_number(b, t_small, G.moves);
}

/* ---- the menu's and the picker's cards --------------------------------------------------------------------------- */
static void paint_tray_of(const tq_game *g, int size, uint8_t *rgba);

static void thumb(int variant, int size, uint8_t *rgba)
{
    tq_game T;
    T.picture = 0;
    tq_new(&T, variant < 0 ? 1 : variant, 7);
    if (variant >= 0) {                         /* the picker: nearly in order, so the picture shows */
        int8_t at[TQ_PLACES];
        int n = T.n;
        for (int i = 0; i < n * n; i++) at[i] = (int8_t)((i + 1) % (n * n));
        at[n * n - 1] = (int8_t)(n * n - n);    /* the tile above the gap slid down into it */
        at[n * n - 1 - n] = 0;
        tq_load(&T, n, at);
    }
    paint_tray_of(&T, size, rgba);
}

/* the level screen: a level's tray; a chapter's tab: the tray of its first level in order */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    tq_game T;
    T.picture = 0;
    if (tq_level(&T, level)) paint_tray_of(&T, size, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    tq_game T;
    T.picture = 0;
    if (!tq_level(&T, chapter * (TQ_LEVELS / TQ_CHAPTERS))) return;
    int8_t at[TQ_PLACES];
    for (int i = 0; i < T.n * T.n; i++) at[i] = (int8_t)((i + 1) % (T.n * T.n));
    tq_load(&T, T.n, at);
    paint_tray_of(&T, size, rgba);
}

/* a tray with its tiles where they are, size x size */
static void paint_tray_of(const tq_game *P, int size, uint8_t *rgba)
{
    tq_game T = *P;
    int f, g, t;
    sizes(T.n, (float)size, &f, &g, &t);
    uint8_t *tray = malloc((size_t)size * size * 4), *src = malloc((size_t)T.n * t * T.n * t * 4);
    uint8_t *one = malloc((size_t)t * t * 4);
    if (tray && src && one) {
        paint_tray(size, f, tray);
        art_blend(rgba, size, size, tray, size, size, 0, 0);
        source(T.n, T.picture, t, src);
        for (int p = 0; p < T.n * T.n; p++) {
            if (!T.at[p]) continue;
            paint_tile(T.n, T.picture, T.at[p] - 1, t, src, one);
            art_blend(rgba, size, size, one, t, t, f + g + (p % T.n) * (t + g), f + g + (p / T.n) * (t + g));
        }
    }
    free(tray);
    free(src);
    free(one);
}

static const game taquin_game = {
    .id = "taquin", .order = 360, .variants = TQ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = TQ_LEVELS, .chapters = TQ_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par,
};
REGISTER_GAME(taquin_game)
