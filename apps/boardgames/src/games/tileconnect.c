/*
 * Tile Connect in RetroStone Board, for one player: the house's Mahjong tiles (games/mjtiles.h, the same faces and
 * bodies as Mahjong Solitaire) in a rectangle on the green table, joined by a gold line (the rules:
 * tileconnect_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a tile (the select mark), then its twin: when a line of at most two turns joins them through empty cells or
 * round the outside, the line shows in gold a moment and both tiles lift away; in the levels where tiles fall, the
 * others then slide down, to the left or towards the middle. A twin that no line reaches: the deny sound, and the
 * tile tapped is picked instead. Blocked cells are walnut blocks. The shell's tools: the lamp (a pair on a way to
 * clear the board blinks with its line; or the shuffle in gold light when no way is left) and the shuffle (the faces
 * left dealt again so they clear; lit when no pair is left, and then big in the middle of the board too). Undo takes
 * back a pair or a shuffle. The bar: the tiles left and, in the levels where they fall, which way.
 *
 * The rectangle takes all the room the shell gives (game.fill), turned a quarter when that makes the tiles bigger
 * (the tiles always stand upright: their faces are not turned; the line turns with the rectangle). A board where the
 * tiles fall is never turned: they fall down the screen, to its left or towards its middle, as the rules say.
 * Stars: no hint and no shuffle 3, the shell's rule for hints with a shuffle counted as two (ui_logic_stars).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/mjtiles.h"
#include "games/pzkit.h"
#include "games/pztools.h"
#include "games/puzzles.h"
#include "games/tileconnect.h"
#include "shell.h"

#define PATH_MS 430                             /* the line shown, the pair lifting away */
#define FALL_MS 210                             /* then the tiles falling */
#define HINT_MS 2600
#define WAVE_STEP 45
#define WAVE_LEN 560
#define RING 0.8f                               /* the line round the outside: this many cells from the edge tiles' middle */

enum { T_HINT, T_SHUFFLE, TOOLS };

static const uint8_t GOLD[3] = {244, 200, 92};

/* ---- the state --------------------------------------------------------------------------------------------------- */
static tcn_game G;
static int sel = -1;
static struct {
    int on, a, b, np, falls;
    uint8_t fa, fb, from[TCN_CELLS];
    tcn_pt path[4];
    Uint32 t0;
} anim;
static struct { int on, kind, a, b, np; tcn_pt path[4]; Uint32 t0; } hint;
static struct { int on; float x, y; Uint32 t0; } win;   /* the wave of light from the last pair (screen cells) */

static box last_box;
static int turned, SC, SR;                      /* the rectangle on the screen: turned, its columns and rows */
static int fw, fh, th, gap, pw, ph;             /* a tile's face, its thickness, the gap; the cells' pitch */
static float ox, oy;                            /* the top left cell's tile */
static int made_key = -1, made_small = -1;
static SDL_Texture *t_tile, *t_shadow, *t_face[TCN_FACES], *t_block, *t_board, *t_fall[TCN_FALLS];
static float board_x, board_y;
static int board_w, board_h;

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static void sizes_for(int w, int *h, int *d, int *g)   /* a tile w wide: its face's height, its thickness, the gap */
{
    *h = 2 * (int)lroundf(MJT_ASPECT * w / 2);
    *d = (int)fmaxf(2, lroundf(MJT_THICK * w));
    *g = (int)fmaxf(1, lroundf(0.05f * w));
}

/* the widest tiles for cols x rows cells (and the outside ring) in W x H */
static int fit_w(int cols, int rows, float W, float H)
{
    for (int w = 400; w > 6; w -= 2) {
        int h, d, g;
        sizes_for(w, &h, &d, &g);
        float cw = (float)(w + d + g), ch = (float)(h + d + g);
        if ((cols + 2 * RING - 0.6f) * cw <= W && (rows + 2 * RING - 0.6f) * ch <= H) return w;
    }
    return 6;
}

/* a point of the rules (x -1..w, y -1..h) on the screen: the middle of its cell's tile face */
static void point_xy(int x, int y, float *sx, float *sy)
{
    int c = turned ? y : x, r = turned ? x : y;
    float px = ox + (fw + th) / 2.0f, py = oy + (fh + th) / 2.0f;
    *sx = c < 0 ? px - RING * pw : c >= SC ? px + (SC - 1 + RING) * pw : px + c * pw;
    *sy = r < 0 ? py - RING * ph : r >= SR ? py + (SR - 1 + RING) * ph : py + r * ph;
}

static void cell_xy(int cell, float *x, float *y)  /* a cell's tile: its top left */
{
    int gx = cell % G.b.w, gy = cell / G.b.w, c = turned ? gy : gx, r = turned ? gx : gy;
    *x = ox + c * pw;
    *y = oy + r * ph;
}

static int cell_at(float x, float y)
{
    float fx = (x - ox + gap / 2.0f) / pw, fy = (y - oy + gap / 2.0f) / ph;
    if (fx < 0 || fy < 0) return -1;
    int c = (int)fx, r = (int)fy;
    if (c >= SC || r >= SR) return -1;
    int gx = turned ? r : c, gy = turned ? c : r;
    return gx + gy * G.b.w;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a walnut block (fw + d) x (fh + d): the house plank, darkened, rounded and bevelled */
static void paint_block(int w, int h, uint8_t *o)
{
    pzt_plank(o, w, h, 3);
    for (int i = 0; i < w * h; i++) pzt_tint(o + 4 * i, PZT_WALNUT, 0.95f);
    puz_bevel(o, w, h, 0.12f * w, fmaxf(2, 0.09f * w), 0.5f);
}

/* the rectangle's ground: a darker felt panel round the cells and the outside ring, a faint socket in each cell */
static void paint_board(int W, int H, float cx0, float cy0, uint8_t *o)
{
    memset(o, 0, (size_t)W * H * 4);
    static const uint8_t black[3] = {0, 0, 0};
    art_roundrect_put(o, W, H, 0, 0, W, H, 0.35f * pw, black, 0.16f);
    for (int r = 0; r < SR; r++)
        for (int c = 0; c < SC; c++)
            art_roundrect_put(o, W, H, (int)(cx0 + c * pw), (int)(cy0 + r * ph), fw + th, fh + th, 0.12f * fw, black,
                              0.14f);
}

/* an arrow (cream, white to be tinted) in the unit square, pointing at deg (0 right, 90 down) */
static void arrow_fig(art_fig *f, float cx, float cy, float deg, float l)
{
    float c = cosf(deg * ART_PI / 180), s = sinf(deg * ART_PI / 180);
    art_band(f, ART_PART, cx - c * l / 2, cy - s * l / 2, cx + c * l * 0.18f, cy + s * l * 0.18f, 0.2f * l);
    art_head(f, ART_PART, cx + c * l / 2, cy + s * l / 2, deg, 0.5f * l);
}

/* the fall's picture: an arrow down, to the left, or two meeting in the middle */
static void paint_fall(int fall, int size, uint8_t *o, const uint8_t rgb[3])
{
    memset(o, 0, (size_t)size * size * 4);
    if (fall == TCN_STILL) return;
    art_fig *f = art_fig_new();
    if (fall == TCN_DOWN) arrow_fig(f, 0.5f, 0.5f, 90, 0.8f);
    else if (fall == TCN_LEFT) arrow_fig(f, 0.5f, 0.5f, 180, 0.8f);
    else {
        arrow_fig(f, 0.2f, 0.5f, 0, 0.38f);
        arrow_fig(f, 0.8f, 0.5f, 180, 0.38f);
    }
    art_fig_paint(o, size, f, rgb, PZT_DARK, 0.02f, 0.2f);
}

/* a tile standing with its face, size x size (the token, the bar's count) */
static void tile_pic(int face, int size, uint8_t *rgba)
{
    int w = 2 * (int)(size * 0.30f), h, d, g;
    sizes_for(w, &h, &d, &g);
    int bw = w + d, bh = h + d;
    uint8_t *body = malloc((size_t)bw * bh * 4), *fp = malloc((size_t)w * h * 4);
    if (body && fp) {
        mjt_tile_pic(w, h, d, body);
        mjt_face_pic(face, w, h, fp);
        art_blend(body, bw, bh, fp, w, h, 0, 0);
        memset(rgba, 0, (size_t)size * size * 4);
        art_blend(rgba, size, size, body, bw, bh, (size - bw) / 2, (size - bh) / 2);
    }
    free(body);
    free(fp);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    tile_pic(MJ_DOTS, size, rgba);
}

static void small_textures(void)
{
    int s = (int)(0.6f * ui.unit);
    if (made_small == s) return;
    made_small = s;
    for (int f = 0; f < TCN_FALLS; f++) {
        ui_free(&t_fall[f]);
        paint_fall(f, s, ui_buf(s, s), PZT_CREAM);
        t_fall[f] = ui_tex(s, s, ui_buf(s, s));
    }
}

static void layout(box b)
{
    last_box = b;
    if (G.b.w < 2) return;
    int wn = fit_w(G.b.w, G.b.h, b.w, b.h), wt = fit_w(G.b.h, G.b.w, b.w, b.h);
    turned = G.b.fall == TCN_STILL && wt > 1.06f * wn;   /* falling tiles fall down the screen: never turned */
    SC = turned ? G.b.h : G.b.w;
    SR = turned ? G.b.w : G.b.h;
    fw = turned ? wt : wn;
    sizes_for(fw, &fh, &th, &gap);
    pw = fw + th + gap;
    ph = fh + th + gap;
    float W = SC * pw - gap, H = SR * ph - gap;
    ox = floorf(b.x + (b.w - W) / 2);
    oy = floorf(b.y + (b.h - H) / 2);
    float m = (RING - 0.5f) * pw + 0.3f * pw, mv = (RING - 0.5f) * ph + 0.3f * pw;
    board_x = floorf(ox - m);
    board_y = floorf(oy - mv);
    board_w = (int)(W + 2 * m);
    board_h = (int)(H + 2 * mv);
    int key = ((fw * 64 + SC) * 64 + SR) * 2 + turned;
    if (made_key != key) {
        made_key = key;
        int w = fw + th, h = fh + th;
        ui_free(&t_tile);
        mjt_tile_pic(fw, fh, th, ui_buf(w, h));
        t_tile = ui_tex(w, h, ui_buf(w, h));
        int sw = fw + 2 * th, sh = fh + 2 * th;
        ui_free(&t_shadow);
        mjt_shadow_pic(sw, sh, 1.1f * th, ui_buf(sw, sh));
        t_shadow = ui_tex(sw, sh, ui_buf(sw, sh));
        for (int f = 0; f < TCN_FACES; f++) {
            ui_free(&t_face[f]);
            mjt_face_pic(f, fw, fh, ui_buf(fw, fh));
            t_face[f] = ui_tex(fw, fh, ui_buf(fw, fh));
        }
        ui_free(&t_block);
        paint_block(w, h, ui_buf(w, h));
        t_block = ui_tex(w, h, ui_buf(w, h));
        ui_free(&t_board);
        paint_board(board_w, board_h, ox - board_x, oy - board_y, ui_buf(board_w, board_h));
        t_board = ui_tex(board_w, board_h, ui_buf(board_w, board_h));
    }
    small_textures();
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    sel = -1;
    anim.on = hint.on = win.on = 0;
}

static void begin(void)
{
    calm();
    if (last_box.w > 0) layout(last_box);
}

static void start(int level)                    /* a designed level */
{
    if (!tcn_level(&G, level)) tcn_level(&G, 0);
    begin();
}

static void start_random(int variant)           /* a board of the variant's size, made at random */
{
    tcn_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return tcn_won(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return ui_logic_stars(G.hints, 2 * G.shuffles); }

static Uint32 anim_len(void) { return PATH_MS + (anim.falls ? FALL_MS : 0); }
static Uint32 win_len(void) { return (Uint32)((SC + SR) * WAVE_STEP + WAVE_LEN); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && now - anim.t0 < anim_len()) || (win.on && (now < win.t0 || now - win.t0 < win_len()));
}

static void win_from(int cell)                  /* the wave of light, from where the last pair was */
{
    int gx = cell % G.b.w, gy = cell / G.b.w;
    win.on = 1;
    win.x = (float)(turned ? gy : gx);
    win.y = (float)(turned ? gx : gy);
    win.t0 = ui_ms() + (anim.on ? anim_len() : 0);
}

static void take(int a, int b)
{
    tcn_pt path[4];
    int np = 0;
    tcn_connect(&G.b, a, b, path, &np);
    uint8_t fa = G.b.face[a], fb = G.b.face[b];
    if (!tcn_take(&G, a, b, anim.from)) { ui_sound(SND_DENY); return; }
    anim.on = 1;
    anim.t0 = ui_ms();
    anim.a = a;
    anim.b = b;
    anim.fa = fa;
    anim.fb = fb;
    anim.np = np;
    memcpy(anim.path, path, sizeof path);
    anim.falls = 0;
    for (int c = 0; c < G.b.w * G.b.h; c++)
        if (anim.from[c] != TCN_EMPTY && anim.from[c] != c) anim.falls = 1;
    sel = -1;
    hint.on = 0;
    ui_sound(SND_CAPTURE);
    if (anim.falls) ui_sound_later(SND_MOVE, PATH_MS + FALL_MS - 40);
    if (tcn_won(&G)) win_from(b);
    ui_animate();
}

static void shuffle(void)
{
    if (!tcn_shuffle(&G)) { ui_sound(SND_DENY); return; }
    calm();
    ui_sound(SND_DICE);
    ui_animate();
}

static int undo(void)
{
    calm();
    return tcn_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = tcn_script(&G, moves);
    if (tcn_won(&G)) {
        win_from(0);
        win.t0 = ui_ms();
    }
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
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

/* the line through the points (rules' coordinates), gold, alpha 0..1 */
static void draw_path(const tcn_pt *p, int n, float alpha, float wide)
{
    if (n < 2 || alpha <= 0) return;
    float wd = fmaxf(3, wide * 0.11f * fw), sx[4], sy[4];
    for (int i = 0; i < n; i++) point_xy(p[i].x, p[i].y, &sx[i], &sy[i]);
    pzk_rgba glow = {255, 190, 80, (Uint8)(110 * alpha)}, core = {GOLD[0], GOLD[1], GOLD[2], (Uint8)(255 * alpha)};
    for (int i = 0; i + 1 < n; i++) pzk_line(sx[i], sy[i], sx[i + 1], sy[i + 1], 2.6f * wd, 1.4f * wd, glow, 1);
    for (int i = 0; i < n; i++) pzk_dot(sx[i], sy[i], 2.6f * wd, 1.4f * wd, glow, 1);
    for (int i = 0; i + 1 < n; i++) pzk_line(sx[i], sy[i], sx[i + 1], sy[i + 1], wd, 1, core, 0);
    for (int i = 0; i < n; i++) pzk_dot(sx[i], sy[i], wd, 1, core, 0);
}

/* the big shuffle button in the middle of the room, when no pair is left */
static int big_shuffle(float *x, float *y, float *r)
{
    *x = last_box.x + last_box.w / 2;
    *y = last_box.y + last_box.h / 2;
    *r = 0.85f * ui.unit;
    return tcn_stuck(&G) && !busy();
}

/* the shuffle's picture: two arrows going round each other, white (tinted) */
static void paint_shuffle(int kind, int size, uint8_t *o)
{
    (void)kind;
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_arc(f, ART_PART, 0.5f, 0.5f, 0.2f, 0.29f, 200, 340);
    art_arc(f, ART_PART, 0.5f, 0.5f, 0.2f, 0.29f, 20, 160);
    art_head(f, ART_PART, 0.27f, 0.62f, 115, 0.2f);
    art_head(f, ART_PART, 0.73f, 0.38f, 295, 0.2f);
    static const uint8_t white[3] = {255, 255, 255};
    art_fig_paint(o, size, f, white, white, 0, 0);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    if (anim.on && now - anim.t0 >= anim_len()) anim.on = 0;
    if (hint.on && now - hint.t0 >= HINT_MS) hint.on = 0;
    if (anim.on || hint.on) ui_animate();
    ui_put(t_board, board_x, board_y, (float)board_w, (float)board_h);
    int n = G.b.w * G.b.h;
    float fall_t = anim.on && anim.falls ? puz_ease((now - anim.t0 - (float)PATH_MS) / FALL_MS) : 1;
    if (anim.on && now - anim.t0 < PATH_MS) fall_t = 0;
    /* where each tile is drawn: falling from its cell before, else in its cell */
    float tx[TCN_CELLS], ty[TCN_CELLS];
    for (int c = 0; c < n; c++) {
        cell_xy(c, &tx[c], &ty[c]);
        if (anim.on && anim.from[c] != TCN_EMPTY && anim.from[c] != c) {
            float x0, y0;
            cell_xy(anim.from[c], &x0, &y0);
            tx[c] = x0 + (tx[c] - x0) * fall_t;
            ty[c] = y0 + (ty[c] - y0) * fall_t;
        }
    }
    for (int c = 0; c < n; c++)                 /* the shadows, then the blocks and the tiles */
        if (G.b.face[c] < TCN_FACES)
            ui_put(t_shadow, tx[c] + 0.2f * th, ty[c] + 0.5f * th, (float)(fw + 2 * th), (float)(fh + 2 * th));
    int blink = hint.on && (now - hint.t0) / 320 % 2 == 0;
    for (int c = 0; c < n; c++) {
        if (G.b.face[c] == TCN_BLOCK) { ui_put(t_block, tx[c], ty[c], (float)(fw + th), (float)(fh + th)); continue; }
        if (G.b.face[c] >= TCN_FACES) continue;
        int lit = hint.on && hint.kind == TCN_HINT_PAIR && (c == hint.a || c == hint.b);
        if (lit) ui_mark_gold_a(tx[c] + fw / 2.0f, ty[c] + fh / 2.0f, 1.9f * fw, blink ? 0.9f : 0.45f);
        tile_at(tx[c], ty[c], G.b.face[c], 255, 255);
        if (c == sel || (lit && blink)) ui_mark_select(tx[c], ty[c], (float)fw, (float)fh);
    }
    if (hint.on && hint.kind == TCN_HINT_PAIR) draw_path(hint.path, hint.np, blink ? 0.75f : 0.4f, 0.8f);
    if (anim.on && now - anim.t0 < PATH_MS) {   /* the pair taken: its line, the tiles rising and fading away */
        float t = (now - anim.t0) / (float)PATH_MS, e = puz_ease(t);
        draw_path(anim.path, anim.np, t < 0.6f ? 1 : 1 - (t - 0.6f) / 0.4f, 1);
        for (int k = 0; k < 2; k++) {
            float x, y;
            cell_xy(k ? anim.b : anim.a, &x, &y);
            ui_mark_gold_a(x + fw / 2.0f, y + fh / 2.0f, 1.8f * fw, 0.8f * (1 - e));
            tile_at(x - e * 0.15f * fw, y - e * 0.35f * fh, k ? anim.fb : anim.fa, 255, (Uint8)(255 * (1 - e)));
        }
    }
    if (win.on) {                               /* solved: a wave of gold light over the cells */
        if (now < win.t0) ui_animate();
        else {
            float wt = (float)(now - win.t0);
            for (int r = 0; r < SR; r++)
                for (int c = 0; c < SC; c++) {
                    float p = puz_pulse(wt, hypotf(c - win.x, r - win.y) * WAVE_STEP, WAVE_LEN);
                    if (p > 0) ui_mark_gold(ox + c * pw + fw / 2.0f, oy + r * ph + fh / 2.0f, 2.0f * fw * p);
                }
            if (wt < win_len()) ui_animate();
            else win.on = 0;
        }
    }
    float x, y, r;
    int stuck = big_shuffle(&x, &y, &r), play = status() == GS_PLAY;
    int shuffle_hint = hint.on && hint.kind == TCN_HINT_SHUFFLE, glow = stuck || shuffle_hint, on = !hint.on || blink;
    if (stuck) ui_button_paint(x, y, 2 * r, paint_shuffle, 0, 0.7f, (on ? UI_ON : 0) | UI_TINT);
    ui_tool(T_HINT, ICON_HINT, play ? 0 : UI_DIM);
    ui_tool_paint(T_SHUFFLE, paint_shuffle, 0, 0.7f,
                  UI_TINT | (!play ? UI_DIM : glow ? (on ? UI_ON : UI_LIT) : 0));
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void use_hint(void)
{
    int a = -1, b = -1, k = tcn_hint(&G, &a, &b);
    if (k == TCN_HINT_NONE) { ui_sound(SND_DENY); return; }
    hint.on = 1;
    hint.kind = k;
    hint.t0 = ui_ms();
    hint.a = a;
    hint.b = b;
    hint.np = 0;
    if (k == TCN_HINT_PAIR) {
        tcn_connect(&G.b, a, b, hint.path, &hint.np);
        G.hints++;
    }
    sel = -1;
    ui_sound(k == TCN_HINT_PAIR ? SND_TAP : SND_DENY);
    ui_animate();
}

static void press(float x, float y)
{
    int t = ui_tool_hit(x, y);
    if (t >= 0) {
        if (status() != GS_PLAY) { ui_sound(SND_DENY); return; }
        anim.on = 0;
        if (t == T_HINT) use_hint();
        else shuffle();
        return;
    }
    if (status() != GS_PLAY) return;
    float bx, by, br;
    if (big_shuffle(&bx, &by, &br) && hypotf(x - bx, y - by) < 1.1f * br) { shuffle(); return; }
    anim.on = 0;
    int c = cell_at(x, y);
    if (c < 0 || G.b.face[c] >= TCN_FACES) {    /* off the tiles: the tile picked let go */
        sel = -1;
        ui_animate();
        return;
    }
    if (sel < 0 || c == sel) sel = c == sel ? -1 : c;
    else if (G.b.face[c] == G.b.face[sel] && tcn_can_take(&G.b, sel, c)) take(sel, c);
    else {
        if (G.b.face[c] == G.b.face[sel]) ui_sound(SND_DENY);   /* its twin, but no line reaches it */
        sel = c;
    }
    ui_animate();
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the bar: the tiles left; which way they fall ---------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    small_textures();
    int falls = G.b.fall != TCN_STILL;
    float d = ui_bar_fit(b, (ui_count[]){{NULL, G.tiles}}, 1, falls);   /* a bare number: the shell draws the token */
    if (falls) {
        float cx, cy;
        ui_bar_spot(b, 0, d, &cx, &cy);
        ui_put_c(t_fall[G.b.fall], cx, cy, 0.9f * d, 0.9f * d);
    }
}

/* ---- the pictures of the shell's screens ------------------------------------------------------------------------ */
/* a board w x h (faces, TCN_EMPTY, TCN_BLOCK) painted into size x size, a gold line through the points p (n) */
static void paint_tiles(int w, int h, const uint8_t *face, const tcn_pt *p, int n, float m, int size, uint8_t *rgba)
{
    int tw, tch, d, g;
    for (tw = size; tw > 4; tw--) {
        sizes_for(tw, &tch, &d, &g);
        if ((w + m) * (tw + d + g) <= 0.96f * size && (h + m) * (tch + d + g) <= 0.96f * size) break;
    }
    sizes_for(tw, &tch, &d, &g);
    int cw = tw + d + g, ch = tch + d + g, bw = tw + d, bh = tch + d;
    float x0 = (size - (w * cw - g)) / 2.0f, y0 = (size - (h * ch - g)) / 2.0f;
    uint8_t *body = malloc((size_t)bw * bh * 4), *blk = malloc((size_t)bw * bh * 4);
    uint8_t *faces = calloc((size_t)TCN_FACES * tw * tch, 4);
    int used[TCN_FACES] = {0};
    if (!body || !blk || !faces) { free(body); free(blk); free(faces); return; }
    mjt_tile_pic(tw, tch, d, body);
    pzt_plank(blk, bw, bh, 3);
    for (int i = 0; i < bw * bh; i++) pzt_tint(blk + 4 * i, PZT_WALNUT, 0.95f);
    puz_bevel(blk, bw, bh, 0.12f * bw, fmaxf(1, 0.09f * bw), 0.5f);
    for (int c = 0; c < w * h; c++) {
        int f = face[c];
        if (f >= TCN_FACES) continue;
        if (!used[f]) mjt_face_pic(f, tw, tch, faces + (size_t)f * tw * tch * 4);
        used[f] = 1;
    }
    for (int c = 0; c < w * h; c++) {
        int x = (int)(x0 + (c % w) * cw), y = (int)(y0 + (c / w) * ch);
        if (face[c] == TCN_BLOCK) art_blend(rgba, size, size, blk, bw, bh, x, y);
        if (face[c] >= TCN_FACES) continue;
        art_blend(rgba, size, size, body, bw, bh, x, y);
        art_blend(rgba, size, size, faces + (size_t)face[c] * tw * tch * 4, tw, tch, x, y);
    }
    float r = fmaxf(1.2f, 0.07f * tw);
    for (int i = 0; i + 1 < n; i++) {
        float px[2], py[2];
        for (int k = 0; k < 2; k++) {
            int q = p[i + k].x, s = p[i + k].y;
            px[k] = x0 + bw / 2.0f + (q < 0 ? -RING : q >= w ? w - 1 + RING : q) * cw;
            py[k] = y0 + bh / 2.0f + (s < 0 ? -RING : s >= h ? h - 1 + RING : s) * ch;
        }
        pzk_stroke(rgba, size, size, px[0], py[0], px[1], py[1], 2.4f * r, GOLD, 0.35f);
        pzk_stroke(rgba, size, size, px[0], py[0], px[1], py[1], r, GOLD, 1);
    }
    free(body);
    free(blk);
    free(faces);
}

/* the menu's card and the picker's: a board of the size made at random (the same each time); the menu's a few pairs
 * gone, one joined by its line */
static void thumb(int variant, int size, uint8_t *rgba)
{
    tcn_game *T = malloc(sizeof *T);
    if (!T) return;
    int v = variant < 0 ? 0 : variant;
    tcn_new(T, v, 2026u + (uint32_t)v);
    tcn_pt p[4];
    int n = 0;
    if (variant < 0) {
        tcn_script(T, "=4");
        for (int a = 0; a < T->b.w * T->b.h; a++)   /* the pair whose line turns the most */
            for (int b = a + 1; b < T->b.w * T->b.h; b++) {
                tcn_pt q[4];
                int k = 0;
                if (tcn_can_take(&T->b, a, b) && tcn_connect(&T->b, a, b, q, &k) && k > n) {
                    memcpy(p, q, sizeof q);
                    n = k;
                }
            }
    }
    paint_tiles(T->b.w, T->b.h, T->b.face, p, n, 0.9f, size, rgba);
    free(T);
}

/* a chapter's tab: a few tiles, a block, the fall's arrow in gold */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const uint8_t F[6][4] = {{0, 0}, {0, 9, TCN_BLOCK, 0}, {27, 31, 27, 31},
                                    {4, 4, 13, 13}, {22, 33, 22, 33}, {1, TCN_BLOCK, 30, 1}};
    static const int H[6] = {1, 2, 2, 2, 2, 2}, FALL[6] = {TCN_STILL, TCN_STILL, TCN_DOWN, TCN_LEFT, TCN_CENTRE, TCN_DOWN};
    int c = chapter < 0 ? 0 : chapter > 5 ? 5 : chapter;
    tcn_pt p[4] = {{0, 0}, {0, -1}, {1, -1}, {1, 0}};   /* the first: two tiles joined over the top */
    paint_tiles(2, H[c], F[c], p, c == 0 ? 4 : 0, c == 0 ? 1.0f : 0.1f, size, rgba);
    if (FALL[c] != TCN_STILL) {
        int s = (int)(0.8f * size);
        uint8_t *a = malloc((size_t)s * s * 4);
        if (a) {
            paint_fall(FALL[c], s, a, PZK_GOLD);
            art_blend(rgba, size, size, a, s, s, (size - s) / 2, (size - s) / 2);
            free(a);
        }
    }
}

static const game tileconnect_game = {
    .id = "tileconnect", .order = 312, .variants = TCN_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1,
    .levels = TCN_LEVELS, .chapters = TCN_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_stars = level_stars, .fill = 1, .tools = TOOLS,
};
REGISTER_GAME(tileconnect_game)
