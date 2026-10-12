/*
 * Ice Slide in RetroStone Board, for one player: a sheet of ice in the walnut frame (the house plank gone pale
 * blue-white, laid in planks, a seam under every row of squares), the rocks as the house's ebony stones, the hole a
 * dark well in a brass ring, the puck a red lacquer disc (the rules: iceslide_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Swipe on the ice, or tap a square in the puck's row or column (it goes that way), or tap one of the four round
 * buttons under the board, or use the arrow keys: the puck slides until a rock or the edge stops it, or drops into the
 * hole on its way. A way it cannot go: its button dimmed. The lamp: the hint (the next push of a shortest way, its
 * button in gold light and the way it would slide dotted in gold; from a dead end, the hint first takes back the pushes
 * that led there). Undo takes back one push, all the way to the start.
 * The bar: the pushes made, and at its right end the par (the fewest possible) by a small hole in gold light.
 * Solved: the puck drops in, the hole flares gold; at par a gold light runs out over the ice from the hole.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/iceslide.h"
#include "games/puzzles.h"
#include "games/swipepad.h"
#include "shell.h"

#define FRAME 0.035f
#define STRIP 0.16f                             /* the buttons' row, a part of the board's square */
#define BASE_MS 70                              /* a slide: this, and so much a square */
#define CELL_MS 42
#define DROP_MS 320
#define FLARE_MS 900
#define WAVE_STEP 55
#define WAVE_LEN 520

static const uint8_t ICE[3] = {212, 230, 242};

static ics_game G;
static struct { int on, from, len; Uint32 t0; } slide;
static struct { int on; Uint32 t0; } win;      /* t0: the puck at the hole (the end of its slide) */
static int hint_way = -1;
static swp_swipe sw;
static swp_pad pad = {.down = -1};

static box last_box;
static float bx, by, side, ox, oy;              /* the board's square; the ice's top left, inside the frame */
static int fr, cell, bw, bh, serial, made_cell = -1, made_serial = -1, made_small = -1;
static SDL_Texture *t_board, *t_puck, *t_small, *t_par;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the ice: the house plank in its frame (w x h squares of sq pixels, the frame f), turned pale blue-white with its
 * grain kept faintly, a seam under every row (the planks), the columns ruled faintly, a sheen across, the frame's
 * shadow along the top and the left */
static void paint_ice(int w, int h, int sq, int f, uint8_t *o)
{
    int W = w * sq + 2 * f, H = h * sq + 2 * f;
    art_board_n(w, h, sq, f, 0, o);
    double sum = 0;
    for (int y = f; y < H - f; y++)
        for (int x = f; x < W - f; x++) {
            const uint8_t *p = o + 4 * (y * W + x);
            sum += (p[0] + p[1] + p[2]) / 3.0;
        }
    float mean = (float)(sum / ((double)w * sq * h * sq)), edge = fmaxf(2, 0.14f * sq);
    for (int y = f; y < H - f; y++)
        for (int x = f; x < W - f; x++) {
            uint8_t *p = o + 4 * (y * W + x);
            int lx = x - f, ly = y - f, rx = lx % sq, ry = ly % sq;
            float grain = (p[0] + p[1] + p[2]) / 3.0f - mean;
            float sheen = 7 * sinf((lx - 0.55f * ly) * 3.14159f / (2.3f * sq)) + 5 * sinf((lx + ly) * 0.9f / sq);
            float k = (1 - 0.07f * ly / (h * sq)) * (1 + 0.025f * sinf(ly / sq * 2.3f + 0.7f));   /* a tone a plank */
            if (ly >= sq && ry == 0) k *= 0.8f;                       /* a seam between two planks */
            else if (ly >= sq && ry == 1) k *= 1.05f;                  /* the next plank's lit edge */
            if (lx >= sq && rx == 0) k *= 0.95f;                       /* the squares' sides, faint */
            float e = fminf((float)lx, (float)ly);
            if (e < edge) k *= 1 - 0.2f * (1 - e / edge);              /* in the frame's shadow */
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf((ICE[i] + 0.28f * grain + sheen) * k, 0, 255);
        }
}

/* a picture blended over o at its centre (cx, cy), its alpha scaled by a */
static void blend_c(uint8_t *o, int W, int H, uint8_t *pic, int d, float cx, float cy, float a)
{
    if (a < 1)
        for (int i = 0; i < d * d; i++) pic[4 * i + 3] = (uint8_t)(pic[4 * i + 3] * a);
    art_blend(o, W, H, pic, d, d, (int)lroundf(cx - d / 2.0f), (int)lroundf(cy - d / 2.0f));
}

/* the hole: a dark well in a brass ring, d pixels */
static void paint_hole(uint8_t *o, int W, int H, float cx, float cy, int d)
{
    static const uint8_t dark[3] = {16, 24, 32}, deep[3] = {0, 0, 0}, brass[3] = {200, 160, 82};
    static const uint8_t frost[3] = {255, 255, 255};
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    art_disc(d, frost, 0.5f, 1.2f, b);          /* the ice worn white round it */
    blend_c(o, W, H, b, d, cx, cy, 1);
    int in = (int)(0.8f * d);
    memset(b, 0, (size_t)d * d * 4);
    art_disc(in, dark, 1, 0, b);
    blend_c(o, W, H, b, in, cx, cy, 1);
    memset(b, 0, (size_t)d * d * 4);
    art_disc(in, deep, 0.75f, 1.5f, b);         /* deeper towards the top left (the far wall in shadow) */
    blend_c(o, W, H, b, in, cx - 0.06f * in, cy - 0.06f * in, 1);
    memset(b, 0, (size_t)d * d * 4);
    art_ring(in, 0.82f, brass, 1, b);
    blend_c(o, W, H, b, in, cx, cy, 1);
    free(b);
}

/* the board: the ice, its rocks (ebony stones with their shadows) and the hole, (w sq + 2 f) x (h sq + 2 f) */
static void paint_board(const ics_board *bd, int sq, int f, uint8_t *o)
{
    int W = bd->w * sq + 2 * f, H = bd->h * sq + 2 * f, d = (int)(0.88f * sq);
    paint_ice(bd->w, bd->h, sq, f, o);
    if (bd->goal >= 0)
        paint_hole(o, W, H, f + (bd->goal % bd->w + 0.5f) * sq, f + (bd->goal / bd->w + 0.5f) * sq, (int)(0.92f * sq));
    uint8_t *st = calloc((size_t)d * d, 4), *sh = calloc((size_t)d * d, 4);
    if (st && sh) {
        art_counter(COUNTER_STONE, 1, d, st);
        for (int c = 0; c < bd->w * bd->h; c++) {
            if (!bd->rock[c]) continue;
            float cx = f + (c % bd->w + 0.5f) * sq, cy = f + (c / bd->w + 0.5f) * sq;
            memset(sh, 0, (size_t)d * d * 4);
            art_flat_shadow(d, sh);
            blend_c(o, W, H, sh, d, cx + 0.07f * d, cy + 0.09f * d, 0.6f);
            art_blend(o, W, H, st, d, d, (int)lroundf(cx - d / 2.0f), (int)lroundf(cy - d / 2.0f));
        }
    }
    free(st);
    free(sh);
}

static void puck_pic(int size, uint8_t *rgba) { art_counter(COUNTER_DISC, 2, size, rgba); }

static void token(int player, int size, uint8_t *rgba)   /* the puck */
{
    (void)player;
    int d = (int)(0.86f * size);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    puck_pic(d, b);
    art_blend(rgba, size, size, b, d, d, (size - d) / 2, (size - d) / 2);
    free(b);
}

/* the par's picture in the bar: a small hole on a patch of ice */
static void par_pic(int size, uint8_t *rgba)
{
    uint8_t *b = calloc((size_t)size * size, 4);
    if (!b) return;
    static const uint8_t ice[4] = {212, 230, 242, 255};
    art_roundrect(size, size, 0.22f * size, ice, b);
    paint_hole(b, size, size, size / 2.0f, size / 2.0f, (int)(0.86f * size));
    art_blend(rgba, size, size, b, size, size, 0, 0);
    free(b);
}

static void fit(float S, int w, int h, int *f, int *sq)
{
    *f = (int)(S * FRAME);
    *sq = (int)fminf((S - 2 * *f) / w, (S * (1 - STRIP) - 2 * *f) / h);
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h)), strip = floorf(S * STRIP);
    side = S;
    bx = floorf(b.x + (b.w - S) / 2);
    by = floorf(b.y + (b.h - S) / 2);
    fit(S, G.b.w, G.b.h, &fr, &cell);
    bw = G.b.w * cell + 2 * fr;
    bh = G.b.h * cell + 2 * fr;
    ox = floorf(bx + (S - bw) / 2) + fr;
    oy = floorf(by + (S - strip - bh) / 2) + fr;
    if (made_cell != cell || made_serial != serial) {
        made_cell = cell;
        made_serial = serial;
        ui_free(&t_board);
        uint8_t *o = malloc((size_t)bw * bh * 4);
        if (o) {
            paint_board(&G.b, cell, fr, o);
            t_board = ui_tex(bw, bh, o);
            free(o);
        }
        int d = (int)(0.8f * cell);
        ui_free(&t_puck);
        memset(ui_buf(d, d), 0, (size_t)d * d * 4);
        puck_pic(d, ui_buf(d, d));
        t_puck = ui_tex(d, d, ui_buf(d, d));
    }
    swp_place(&pad, (box){bx, by + S - strip, S, strip});
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        token(0, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
        ui_free(&t_par);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        par_pic(small, ui_buf(small, small));
        t_par = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static Uint32 slide_ms(int len) { return (Uint32)(BASE_MS + CELL_MS * len); }
static int perfect(void) { return ics_solved(&G) && G.moves == G.par; }

static float far_from_hole(void)                 /* the ice's farthest square from the hole, in squares */
{
    float gx = G.b.goal % G.b.w, gy = G.b.goal / G.b.w;
    return hypotf(fmaxf(gx, G.b.w - 1 - gx), fmaxf(gy, G.b.h - 1 - gy));
}

static Uint32 win_end(void)
{
    return (Uint32)(DROP_MS + (perfect() ? far_from_hole() * WAVE_STEP + WAVE_LEN : FLARE_MS) + 150);
}

static void calm(void)
{
    slide.on = win.on = 0;
    hint_way = -1;
    sw.on = 0;
    pad.down = -1;
}

static void push(int way);
static void key(SDL_Keycode k)                  /* the arrow keys */
{
    int w = swp_key_way(k);
    if (w >= 0 && !ics_solved(&G)) push(w);
}

static void begin(void)
{
    serial++;
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a designed level */
{
    if (!ics_level(&G, level)) ics_level(&G, 0);
    begin();
}

static void start_random(int variant)           /* a puzzle made at random, of the variant's size */
{
    ics_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return ics_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.moves; }      /* the pushes of the way kept (undo takes them back) */
static int par(void) { return G.par > 0 ? G.par : 0; }
static int level_stars(void) { return ui_par_stars(G.moves, par()); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (slide.on && now - slide.t0 < slide_ms(slide.len)) || (win.on && (int)(now - win.t0) < (int)win_end());
}

static void push(int way)
{
    if (ics_solved(&G)) return;
    int from = G.puck, len = ics_can(&G, way);
    if (!len) { ui_sound(SND_DENY); return; }
    ics_move(&G, way);
    hint_way = -1;
    slide.on = 1;
    slide.from = from;
    slide.len = len;
    slide.t0 = ui_ms();
    Uint32 ms = slide_ms(len);
    ui_sound(SND_MOVE);
    if (ics_solved(&G)) {
        ui_sound_later(SND_DROP, (int)ms);
        win.on = 1;
        win.t0 = slide.t0 + ms;
    } else {
        ui_sound_later(SND_PLACE, (int)ms - 20);
    }
    ui_animate();
}

/* the hint: the next push of a shortest way; from a dead end, back first to where the hole can be reached */
static void hint(void)
{
    if (ics_solved(&G)) { ui_sound(SND_DENY); return; }
    int d = ics_hint(&G);
    while (d < 0 && ics_undo(&G)) d = ics_hint(&G);
    slide.on = 0;
    if (d < 0) { ui_sound(SND_DENY); return; }
    hint_way = d;
    ui_sound(SND_TAP);
    ui_animate();
}

static int undo(void)
{
    calm();
    return ics_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = ics_script(&G, moves);
    if (ics_solved(&G)) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_xy(int c, float *x, float *y)
{
    *x = ox + (c % G.b.w + 0.5f) * cell;
    *y = oy + (c / G.b.w + 0.5f) * cell;
}

static void draw(void)
{
    if (made_cell < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_board, ox - fr, oy - fr, (float)bw, (float)bh);
    float px, py, gx, gy, d = 0.8f * cell;
    cell_xy(G.puck, &px, &py);
    cell_xy(G.b.goal, &gx, &gy);
    if (slide.on) {
        float t = (now - slide.t0) / (float)slide_ms(slide.len);
        if (t >= 1) slide.on = 0;
        else {                                  /* off at once, slowing a little at the end */
            float fx, fy, f = t * (1.6f - 0.6f * t);
            cell_xy(slide.from, &fx, &fy);
            px = fx + (px - fx) * f;
            py = fy + (py - fy) * f;
            ui_animate();
        }
    }
    if (G.nh && !ics_solved(&G)) {              /* where the last push came from */
        float lx, ly;
        cell_xy(G.hist[G.nh - 1], &lx, &ly);
        ui_mark_last_round(lx, ly, 0.82f * cell);
    }
    if (hint_way >= 0 && !slide.on) {           /* the hint's way, dotted in gold, its end ringed */
        static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
        int len, to = ics_slide(&G.b, G.puck, hint_way, &len), c = G.puck;
        for (int i = 0; i < len; i++) {
            c += DX[hint_way] + DY[hint_way] * G.b.w;
            float x, y;
            cell_xy(c, &x, &y);
            ui_mark_gold(x, y, (c == to ? 1.4f : 0.7f) * cell);
        }
    }
    float scale = 1, alpha = 1, wt = win.on ? (float)(int)(now - win.t0) : -1e9f;
    if (win.on && wt >= 0) {                    /* dropping into the hole */
        float q = clampf(wt / DROP_MS, 0, 1);
        scale = 1 - 0.5f * puz_ease(q);
        alpha = 1 - q;
        px = gx;
        py = gy;
    }
    if (alpha > 0) ui_flat_at(t_puck, px, py, d * scale, alpha, 0);
    if (win.on) {
        float q = puz_pulse(wt, DROP_MS * 0.4f, FLARE_MS);
        if (q > 0) ui_mark_gold(gx, gy, (1.2f + 1.6f * q) * cell * q);
        if (perfect())                          /* at par: a gold light runs out over the ice from the hole */
            for (int c = 0; c < G.b.w * G.b.h; c++) {
                if (G.b.rock[c] || c == G.b.goal) continue;
                float x, y, dist = hypotf((float)(c % G.b.w - G.b.goal % G.b.w), (float)(c / G.b.w - G.b.goal / G.b.w));
                float k = puz_pulse(wt, DROP_MS + dist * WAVE_STEP, WAVE_LEN);
                if (k <= 0) continue;
                cell_xy(c, &x, &y);
                ui_mark_gold(x, y, 1.25f * cell * k);
            }
        if (wt < (float)win_end()) ui_animate();
    }
    unsigned can = 0;
    for (int w = 0; w < 4; w++)
        if (ics_can(&G, w)) can |= 1u << w;
    swp_draw(&pad, can, slide.on ? -1 : hint_way, !ics_solved(&G));
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* a tap on the ice: a square in the puck's row or column sends it that way */
static void tap_at(float x, float y)
{
    int cx = (int)floorf((x - ox) / cell), cy = (int)floorf((y - oy) / cell);
    int px = G.puck % G.b.w, py = G.puck / G.b.w;
    if (cx < 0 || cy < 0 || cx >= G.b.w || cy >= G.b.h || (cx == px) == (cy == py)) { ui_sound(SND_DENY); return; }
    if (cx == px) push(cy < py ? ICS_UP : ICS_DOWN);
    else push(cx < px ? ICS_LEFT : ICS_RIGHT);
}

static void press(float x, float y)
{
    sw.on = 0;
    pad.down = swp_hit(&pad, x, y);
    if (pad.down >= 0) { ui_animate(); return; }
    if (!ics_solved(&G) && x >= bx && y >= by && x < bx + side && y < by + side) swp_begin(&sw, x, y);
}

static void motion(float x, float y)
{
    int w = swp_follow(&sw, x, y, 0.6f * ui.unit);
    if (w >= 0) push(w);
}

static void release(float x, float y)
{
    if (pad.down >= 0) {
        int k = pad.down;
        pad.down = -1;
        ui_animate();
        if (swp_hit(&pad, x, y) != k) return;
        if (k == SWP_HINT) hint();
        else if (ics_solved(&G)) ui_sound(SND_DENY);
        else push(k);
        return;
    }
    int tap, w = swp_end(&sw, x, y, 0.3f * ui.unit, &tap);
    if (w >= 0) push(w);
    else if (tap) tap_at(sw.x0, sw.y0);
}

/* ---- the bar: the pushes; the par at the right ----------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    float u = ui.unit, dh = 0.46f * u, cy = b.y + b.h / 2, sz = 0.6f * u;
    int digits = G.par >= 10 ? 2 : 1;
    float nw = (0.74f * (digits - 1) + 0.62f) * dh, x = b.x + b.w - nw;
    float need = 1.4f * dh + 0.35f * u + sz + 0.12f * u + nw;
    if (b.w < need) {
        puz_bar_moves(b, NULL, G.moves);
        return;
    }
    puz_bar_moves(b, b.w >= need + sz + 0.12f * u ? t_small : NULL, G.moves);
    ui_mark_gold(x - 0.12f * u - sz / 2, cy, (perfect() ? 2.4f : 1.5f) * sz);
    ui_put(t_par, x - 0.12f * u - sz, cy - sz / 2, sz, sz);
    ui_number(x + nw / 2, cy, dh, G.par, 1);
}

/* ---- the menu's and the picker's cards: a puzzle of that size, the puck at its start --------------------------- */
static void paint_puzzle(const ics_game *T, int size, uint8_t *rgba)
{
    int f = (int)(size * FRAME), sq = (size - 2 * f) / (T->b.w > T->b.h ? T->b.w : T->b.h);
    int W = T->b.w * sq + 2 * f, H = T->b.h * sq + 2 * f, d = (int)(0.8f * sq);
    uint8_t *o = malloc((size_t)W * H * 4), *pk = calloc((size_t)d * d, 4), *sh = calloc((size_t)d * d, 4);
    if (o && pk && sh) {
        paint_board(&T->b, sq, f, o);
        float cx = f + (T->start % T->b.w + 0.5f) * sq, cy = f + (T->start / T->b.w + 0.5f) * sq;
        art_flat_shadow(d, sh);
        blend_c(o, W, H, sh, d, cx + 0.06f * d, cy + 0.08f * d, 0.6f);
        puck_pic(d, pk);
        blend_c(o, W, H, pk, d, cx, cy, 1);
        art_blend(rgba, size, size, o, W, H, (size - W) / 2, (size - H) / 2);
    }
    free(o);
    free(pk);
    free(sh);
}

static void thumb(int variant, int size, uint8_t *rgba)
{
    ics_game T;
    int v = variant < 0 ? 1 : variant;
    ics_new(&T, v, 21u + (uint32_t)v);
    paint_puzzle(&T, size, rgba);
}

/* ---- the level screen: a level's board; a chapter's tab: bare ice of its size, the puck in a corner, the hole in
 * the other ------------------------------------------------------------------------------------------------------ */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    ics_game T;
    if (ics_level(&T, level)) paint_puzzle(&T, size, rgba);
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    ics_game T;
    if (!ics_level(&T, chapter * (ICS_LEVELS / ICS_CHAPTERS))) return;
    memset(T.b.rock, 0, sizeof T.b.rock);
    T.start = 0;
    T.b.goal = T.b.w * T.b.h - 1;
    paint_puzzle(&T, size, rgba);
}

static const game iceslide_game = {
    .id = "iceslide", .order = 423, .variants = ICS_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = ICS_LEVELS, .chapters = ICS_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par,
};
REGISTER_GAME(iceslide_game)
