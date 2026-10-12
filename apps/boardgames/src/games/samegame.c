/*
 * Same Game in RetroStone Board, for one player: lacquered wooden tiles in a walnut tray, each colour with its mark
 * (hues.h) (the rules: samegame_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Tap a group of two or more tiles of one colour: it is chosen (a gold rim round its tiles, the others dimmed) and what
 * it would score shows over it; tap it again to take it away (a tap elsewhere chooses another group, or nothing). The
 * tiles go, those above fall, and the columns close up to the left. Undo takes back one removal, all the way to the
 * start. The bar: the score; the lamp: a hint (the group to take next, blinking gold, chosen as by a tap; on the way
 * that clears the board while there is one). The end, when no group is left: a board cleared lights up in gold, column
 * by column, and earns its bonus; else the tiles left fade. Either way the final score stands on a brass plaque at the
 * tray's top and the shell's end card follows: the game is over, never lost.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/hues.h"
#include "games/puzzles.h"
#include "games/samegame.h"
#include "shell.h"

#define FRAME 0.035f
#define POP_MS 150
#define FALL_MS 190
#define SHIFT_MS 190
#define HINT_MS 2400
#define END_MS 1500

static const int COLOURS[5] = {1, 4, 3, 2, 5}; /* red lacquer, lapis, amber, jade, ivory (hues.h) */

static sm_game G;
static struct { int on, n; uint8_t mask[SM_W][SM_H]; } sel;
static struct {
    int on, falls, shifts;
    sm_board old;
    uint8_t gone[SM_W][SM_H];
    int8_t fx[SM_W][SM_H], fy[SM_W][SM_H];      /* each tile's place before */
    Uint32 t0;
} anim;
static struct { int on, cleared; Uint32 t0; } end;
static struct { int on; uint8_t mask[SM_W][SM_H]; Uint32 t0; } hint;
static hue_lamp lamp;
static struct { int on, x, y; } tapd;

static box last_box;
static float bx, by, bw, bh, gx, gy0, q;        /* the tray; the grid's left and bottom, a cell's side */
static int fr, made_key = -1, made_small = -1;
static SDL_Texture *t_tray, *t_tile[6], *t_small, *t_plaque;
static puz_font font;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a tile of colour c (1..5: COLOURS), d x d: lacquered wood, bevelled, its mark inlaid */
static void paint_tile(int c, int d, uint8_t *o)
{
    int P = 3 * d;
    uint8_t *plank = malloc((size_t)P * P * 4);
    if (!plank) return;
    art_board_n(1, 1, P, 0, 0, plank);
    int ox = (c % 3) * d, oy = (c / 3) * d;     /* each colour cut from its own part of the plank */
    for (int y = 0; y < d; y++) memcpy(o + 4 * (size_t)y * d, plank + 4 * ((size_t)(oy + y) * P + ox), (size_t)d * 4);
    free(plank);
    hue_lacquer(o, d, d, COLOURS[c - 1]);
    puz_bevel(o, d, d, 0.12f * d, fmaxf(1.5f, 0.1f * d), 0.38f);
    hue_mark_on(o, d, d, COLOURS[c - 1], d * 0.5f, d * 0.5f, (int)(0.5f * d));
}

/* the tray: the walnut frame, its floor sunk in shade (w x h) */
static void paint_tray(int w, int h, int f, uint8_t *o)
{
    uint8_t *sq = malloc((size_t)w * w * 4);
    if (!sq) return;
    art_board_n(1, 1, w - 2 * f, f, 0, sq);
    for (int y = 0; y < h; y++) {               /* the square board's top rows, then its frame's bottom rows */
        int sy = y < h - f ? y : y + (w - h);
        memcpy(o + 4 * (size_t)y * w, sq + 4 * (size_t)sy * w, (size_t)w * 4);
    }
    free(sq);
    float sh = 0.04f * (h < w ? h : w);
    for (int y = f; y < h - f; y++)
        for (int x = f; x < w - f; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float e = fminf(x - f, y - f), k = 0.46f - 0.14f * clampf(1 - e / sh, 0, 1);
            p[0] = (uint8_t)(p[0] * k), p[1] = (uint8_t)(p[1] * k * 1.02f), p[2] = (uint8_t)(p[2] * k * 1.05f);
        }
}

/* the brass plaque for the final score, w x h */
static void paint_plaque(int w, int h, uint8_t *o)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            float t = (y + 0.5f) / h, u = (x + 0.5f) / w, k = 1.12f - 0.36f * t + 0.1f * sinf(u * 9 + t * 2);
            p[0] = (uint8_t)clampf(212 * k, 0, 255), p[1] = (uint8_t)clampf(168 * k, 0, 255);
            p[2] = (uint8_t)clampf(82 * k, 0, 255), p[3] = 255;
        }
    puz_bevel(o, w, h, 0.25f * h, fmaxf(2, 0.1f * h), 0.35f);
}

static void token(int player, int size, uint8_t *rgba)   /* three tiles: two side by side, one on top */
{
    (void)player;
    int d = (int)(0.44f * size);
    uint8_t *t = malloc((size_t)d * d * 4);
    if (!t) return;
    static const int at[3][3] = {{1, 0, 1}, {2, 1, 1}, {3, 0, 0}};   /* colour, column, row from the bottom */
    for (int i = 0; i < 3; i++) {
        paint_tile(at[i][0], d, t);
        art_blend(rgba, size, size, t, d, d, (int)(size / 2.0f - d + at[i][1] * d),
                  (int)(size * 0.94f - (at[i][2] + 1) * d));
    }
    free(t);
}

static void fit(int w, int h, float S, float *o_bw, float *o_bh, float *o_q, int *o_fr)
{
    int f = (int)(S * FRAME);
    float pad = 0.012f * S, cq = floorf(fminf((S - 2 * f - 2 * pad) / w, (S - 2 * f - 2 * pad) / h));
    *o_q = cq;
    *o_fr = f;
    *o_bw = floorf(w * cq + 2 * f + 2 * pad);
    *o_bh = floorf(h * cq + 2 * f + 2 * pad);
}

static void layout(box b)
{
    last_box = b;
    float S = floorf(fminf(b.w, b.h));
    fit(G.w, G.h, S, &bw, &bh, &q, &fr);
    bx = floorf(b.x + (b.w - bw) / 2);
    by = floorf(b.y + (b.h - bh) / 2);
    gx = bx + (bw - G.w * q) / 2;
    gy0 = by + bh - (bh - G.h * q) / 2;
    int key = ((int)S * 32 + G.w) * 16 + G.h;
    if (made_key != key) {
        made_key = key;
        int w = (int)bw, h = (int)bh;
        ui_free(&t_tray);
        paint_tray(w, h, fr, ui_buf(w, w));
        t_tray = ui_tex(w, h, ui_buf(w, w));
        int d = (int)q - (int)fmaxf(1, roundf(0.05f * q));
        for (int c = 1; c <= 5; c++) {
            ui_free(&t_tile[c]);
            paint_tile(c, d, ui_buf(d, d));
            t_tile[c] = ui_tex(d, d, ui_buf(d, d));
        }
        int pw = (int)(0.42f * S), ph = (int)(0.15f * S);
        ui_free(&t_plaque);
        paint_plaque(pw, ph, ui_buf(pw, ph));
        t_plaque = ui_tex(pw, ph, ui_buf(pw, ph));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        memset(ui_buf(small, small), 0, (size_t)small * small * 4);
        token(0, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
    puz_font_make(&font, (int)fmaxf(12, 0.07f * S), 1);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    sel.on = anim.on = hint.on = tapd.on = 0;
    end.on = 0;
}

static void start(int variant)
{
    sm_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    calm();
    made_key = -1;
    if (last_box.w > 0) layout(last_box);
}

static int status(void) { return sm_over(&G) ? GS_WIN0 : GS_PLAY; }   /* over: cleared or not, done (never lost) */
static int turn(void) { return 0; }

static Uint32 anim_len(void) { return POP_MS + (anim.falls ? FALL_MS : 0) + (anim.shifts ? SHIFT_MS : 0); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && now - anim.t0 < anim_len()) || (end.on && now - end.t0 < END_MS);
}

static void ended(Uint32 at)
{
    if (!sm_over(&G)) return;
    end.on = 1;
    end.cleared = sm_cleared(&G);
    end.t0 = at;
}

static int undo(void)
{
    calm();
    return sm_undo(&G);
}

static int script(const char *moves)
{
    calm();
    int ok = sm_script(&G, moves);
    ended(ui_ms());
    return ok;
}

static void take(int x, int y)
{
    anim.old = G.b;
    memset(anim.gone, 0, sizeof anim.gone);
    int n = sm_group(&G.b, G.w, G.h, x, y, anim.gone);
    if (!sm_remove(&G, x, y)) return;
    /* where each tile was: the columns left in order, the tiles left in each from the bottom */
    anim.falls = anim.shifts = 0;
    int nx = 0;
    for (int ox = 0; ox < G.w; ox++) {
        int ny = 0;
        for (int oy = 0; oy < G.h; oy++) {
            if (!anim.old.c[ox][oy] || anim.gone[ox][oy]) continue;
            anim.fx[nx][ny] = (int8_t)ox;
            anim.fy[nx][ny] = (int8_t)oy;
            anim.falls |= oy != ny;
            anim.shifts |= ox != nx;
            ny++;
        }
        if (ny) nx++;
    }
    anim.on = 1;
    anim.t0 = ui_ms();
    sel.on = hint.on = 0;
    ui_sound(n >= 6 ? SND_CAPTURE : SND_MOVE);
    if (anim.falls || anim.shifts) ui_sound_later(SND_PLACE, (int)anim_len() - 30);
    ended(anim.t0 + anim_len());
    ui_animate();
}

static void choose(int x, int y)
{
    memset(sel.mask, 0, sizeof sel.mask);
    sel.n = sm_group(&G.b, G.w, G.h, x, y, sel.mask);
    sel.on = sel.n >= 2;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_xy(float x, float y, float *cx, float *cy)
{
    *cx = gx + (x + 0.5f) * q;
    *cy = gy0 - (y + 0.5f) * q;
}

static void tile_at(int c, float cx, float cy, float scale, Uint8 light, Uint8 alpha)
{
    SDL_Texture *t = t_tile[c];
    if (!t) return;
    float d = (q - fmaxf(1, roundf(0.05f * q))) * scale;
    SDL_SetTextureColorMod(t, light, light, light);
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, cx, cy, d, d);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw(void)
{
    if (made_key < 0) layout(last_box);
    Uint32 now = ui_ms();
    ui_put(t_tray, bx, by, bw, bh);
    Uint32 at = anim.on ? now - anim.t0 : 0;
    if (anim.on && at >= anim_len()) anim.on = 0;
    float et = end.on && now >= end.t0 ? (float)(now - end.t0) : -1;
    if (end.on && now < end.t0) ui_animate();
    int blink = hint.on && (now - hint.t0) / 300 % 2 == 0;
    if (hint.on && now - hint.t0 > HINT_MS) hint.on = 0;
    if (hint.on) ui_animate();
    if (anim.on) {                              /* the group going, the tiles falling, the columns closing up */
        float pop = clampf(at / (float)POP_MS, 0, 1);
        float fall = anim.falls ? clampf((at - POP_MS) / (float)FALL_MS, 0, 1) : 1;
        float shift = anim.shifts ? clampf((at - POP_MS - (anim.falls ? FALL_MS : 0)) / (float)SHIFT_MS, 0, 1) : 1;
        if (pop < 1)
            for (int x = 0; x < G.w; x++)
                for (int y = 0; y < G.h; y++) {
                    if (!anim.gone[x][y]) continue;
                    float cx, cy;
                    cell_xy((float)x, (float)y, &cx, &cy);
                    ui_mark_gold(cx, cy, 1.6f * q * (1 - pop));
                    tile_at(anim.old.c[x][y], cx, cy, 1 - 0.7f * pop, 255, (Uint8)(255 * (1 - pop)));
                }
        float ef = puz_ease(fall), es = puz_ease(shift);
        for (int x = 0; x < G.w; x++)
            for (int y = 0; y < G.h && G.b.c[x][y]; y++) {
                float ox = anim.fx[x][y], oy = anim.fy[x][y], cx, cy;
                cell_xy(ox + (x - ox) * es, oy + (y - oy) * ef, &cx, &cy);
                tile_at(G.b.c[x][y], cx, cy, 1, 255, 255);
            }
        ui_animate();
    } else {
        float fade = et >= 0 && !end.cleared ? clampf(et / 600.0f, 0, 1) : 0;
        for (int x = 0; x < G.w; x++)
            for (int y = 0; y < G.h && G.b.c[x][y]; y++) {
                float cx, cy;
                cell_xy((float)x, (float)y, &cx, &cy);
                int chosen = sel.on && sel.mask[x][y], hinted = blink && hint.mask[x][y];
                if (chosen || hinted) ui_mark_select(cx - q / 2, cy - q / 2, q, q);
                Uint8 light = sel.on && !chosen ? 150 : (Uint8)(255 - 110 * fade);
                tile_at(G.b.c[x][y], cx, cy, chosen ? 0.92f : 1, light, 255);
            }
    }
    if (sel.on && !anim.on) {                   /* what the group would score, over its top tile */
        int tx = 0, ty = -1;
        for (int x = 0; x < G.w; x++)
            for (int y = 0; y < G.h; y++)
                if (sel.mask[x][y] && y > ty) tx = x, ty = y;
        float cx, cy, h = fmaxf(0.42f * q, 0.3f * ui.unit);
        cell_xy((float)tx, (float)ty, &cx, &cy);
        int pts = sm_points(sel.n);
        float w = puz_number_w(pts, h) + (pts > 0 ? 0.75f * h : 0) + 0.6f * h;
        cy = fmaxf(cy - 0.5f * q, by + 0.4f * h);   /* a tag on the top tile's upper edge */
        cx = clampf(cx, bx + w / 2, bx + bw - w / 2);
        ui_fill(cx - w / 2, cy - 0.66f * h, w, 1.32f * h, 40, 28, 20, 225);
        ui_fill(cx - w / 2, cy - 0.66f * h, w, fmaxf(1, 0.08f * h), 230, 190, 90, 255);
        ui_number_signed(cx, cy, h, pts, 1, 1);
    }
    if (et >= 0) {
        if (end.cleared) {                      /* cleared: gold light through the tray, column by column */
            for (int x = 0; x < G.w; x++) {
                float p = puz_pulse(et, x * 45.0f, 520);
                for (int y = 0; y < G.h && p > 0; y += 2) {
                    float cx, cy;
                    cell_xy((float)x, y + 0.5f, &cx, &cy);
                    ui_mark_gold(cx, cy, 2.8f * q * p);
                }
            }
        }
        float a = clampf((et - 300) / 400.0f, 0, 1);   /* the final score on its plaque */
        if (a > 0) {
            float S = fminf(last_box.w, last_box.h), pw = 0.42f * S, ph = 0.15f * S, cx = bx + bw / 2;
            float cy = last_box.y + (last_box.h - S) / 2 + 0.035f * S + ph / 2;   /* the top, clear of the card */
            if (end.cleared) ui_mark_gold(cx, cy, 0.6f * S * a);
            SDL_SetTextureAlphaMod(t_plaque, (Uint8)(255 * a));
            ui_put_c(t_plaque, cx, cy, pw, ph);
            SDL_SetTextureAlphaMod(t_plaque, 255);
            puz_number(&font, cx, cy + 0.02f * ph, 0.58f * ph, G.score, PUZ_INK, (Uint8)(255 * a));
        }
        if (et < END_MS) ui_animate();
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int cell_at(float x, float y, int *cx, int *cy)
{
    float fx = (x - gx) / q, fy = (gy0 - y) / q;
    if (fx < 0 || fy < 0 || fx >= G.w || fy >= G.h) return 0;
    *cx = (int)fx;
    *cy = (int)fy;
    return 1;
}

static void press(float x, float y)
{
    tapd.on = 0;
    if (anim.on) anim.on = 0;                   /* the last removal lands at once */
    if (sm_over(&G)) return;
    int cx, cy;
    if (!cell_at(x, y, &cx, &cy)) {
        if (sel.on) { sel.on = 0; ui_animate(); }
        return;
    }
    tapd.on = 1;
    tapd.x = cx;
    tapd.y = cy;
}

static void motion(float x, float y)
{
    (void)x;
    (void)y;
}

static void release(float x, float y)
{
    if (!tapd.on) return;
    tapd.on = 0;
    int cx, cy;
    if (!cell_at(x, y, &cx, &cy)) return;
    hint.on = 0;
    if (sel.on && sel.mask[cx][cy]) { take(cx, cy); return; }
    if (sm_group(&G.b, G.w, G.h, cx, cy, NULL) < 2) {
        sel.on = 0;
        ui_sound(SND_DENY);
        ui_animate();
        return;
    }
    choose(cx, cy);
    ui_sound(SND_TAP);
    ui_animate();
}

/* ---- the bar: the score, the lamp -------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    box left = b;
    left.w = b.w - hue_lamp_room();
    puz_bar_moves(left, left.w > 2.4f * ui.unit ? t_small : NULL, G.score);
    if (sm_over(&G)) { lamp.bar.w = 0; return; }
    hue_lamp_draw(&lamp, b, hint.on && (ui_ms() - hint.t0) / 320 % 2 == 0, 0);
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (sm_over(&G) || !hue_lamp_hit(&lamp, x, y)) return 0;
    anim.on = 0;
    int hx, hy;
    if (!sm_hint(&G, &hx, &hy)) { ui_sound(SND_DENY); return 1; }
    memset(hint.mask, 0, sizeof hint.mask);
    sm_group(&G.b, G.w, G.h, hx, hy, hint.mask);
    hint.on = 1;
    hint.t0 = ui_ms();
    choose(hx, hy);                             /* chosen as by a tap: a second tap takes it */
    ui_sound(SND_TAP);
    ui_animate();
    return 1;
}

/* ---- the menu's and the picker's cards --------------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    sm_game *T = malloc(sizeof *T);
    if (!T) return;
    sm_new(T, variant < 0 ? 0 : variant, 5);
    if (variant < 0) sm_script(T, "=8");        /* the menu's: a few groups taken, the tiles fallen */
    float tw, th, cq;
    int f;
    fit(T->w, T->h, (float)size, &tw, &th, &cq, &f);
    int w = (int)tw, h = (int)th, ox = (size - w) / 2, oy = (size - h) / 2, d = (int)cq - (cq >= 12 ? 1 : 0);
    uint8_t *b = malloc((size_t)w * w * 4), *t = malloc((size_t)(d > 0 ? d : 1) * (d > 0 ? d : 1) * 4);
    if (b && t && d > 2) {
        paint_tray(w, h, f, b);
        art_blend(rgba, size, size, b, w, h, ox, oy);
        float x0 = ox + (w - T->w * cq) / 2, y0 = oy + h - (h - T->h * cq) / 2;
        for (int c = 1; c <= T->colours; c++) {
            paint_tile(c, d, t);
            for (int x = 0; x < T->w; x++)
                for (int y = 0; y < T->h; y++)
                    if (T->b.c[x][y] == c)
                        art_blend(rgba, size, size, t, d, d, (int)(x0 + x * cq + (cq - d) / 2),
                                  (int)(y0 - (y + 1) * cq + (cq - d) / 2));
        }
    }
    free(b);
    free(t);
    free(T);
}

static const game samegame_game = {
    .id = "samegame", .order = 422, .variants = SM_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(samegame_game)
