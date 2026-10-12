/*
 * The Knight's Tour in RetroStone Board, for one player: the house board (5 x 5, 6 x 6 or 8 x 8) and a knight that
 * must visit every square once (the rules: knightstour_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The knight starts on a square drawn for the game. Tap a square it can reach (the dots), or drag it there: it jumps,
 * and the square it left keeps its number, a faint line joining the path. Stuck with squares left: a red glow (undo).
 * The lamp in the bar (Warnsdorff's rule): each square the knight can reach shows how many moves it would have from
 * there, and the best one lights up in gold (a search checks it still leads to a whole tour; a red glow on the knight
 * when none can be finished from here). Undo takes back one jump, all the way to the start. The bar: the squares
 * visited and the board's count. Done: a gold light runs along the path in its order; a closed tour (the last square
 * a knight's move from the first) also joins the ends with a gold line, and a gold ring shows in the bar.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/boardpuz.h"
#include "games/knightstour.h"
#include "shell.h"

#define ANIM_MS 200
#define WAVE_TOTAL 1500                         /* the light's run along the whole path */
#define WAVE_LEN 520
#define CLOSE_MS 500                            /* the closed tour's gold line, after the run */

static kt_game G;
static bpz_board B;
static bpz_pics P;
static bpz_discs D;
static puz_font F;
static box last_box, barbox;
static int made_small = -1, made_line = -1;
static SDL_Texture *t_small, *t_line;

static struct { int on, from; Uint32 t0; } anim;
static struct { int on; Uint32 t0; } win;
static struct { int sq, none; } hint = {-1, 0};
static struct { int on, moved; float x, y, x0, y0; } drag;

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    anim.on = win.on = 0;
    hint.sq = -1;
    hint.none = 0;
    memset(&drag, 0, sizeof drag);
}

static void start(int variant)
{
    kt_new(&G, variant < 0 || variant >= KT_VARIANTS ? KT_5 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    calm();
    if (last_box.w > 0) ui_animate();
}

static int status(void) { return kt_done(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int step_ms(void) { return WAVE_TOTAL / (G.n * G.n); }
static int win_end(void) { return G.n * G.n * step_ms() + WAVE_LEN + (kt_closed(&G) ? CLOSE_MS : 0); }

static int busy(void)
{
    Uint32 now = ui_ms();
    return (anim.on && now - anim.t0 < ANIM_MS) || (win.on && (int)(now - win.t0) < win_end());
}

static void won(Uint32 at)
{
    if (!kt_done(&G)) return;
    win.on = 1;
    win.t0 = at;
}

static void jump(int sq, int slide)
{
    int from = G.path[G.len - 1];
    if (!kt_move(&G, sq)) { ui_sound(SND_DENY); return; }
    calm();
    anim.on = slide;
    anim.from = from;
    anim.t0 = ui_ms();
    ui_sound_later(SND_PLACE, slide ? ANIM_MS : 0);
    if (kt_stuck(&G)) ui_sound_later(SND_DENY, (slide ? ANIM_MS : 0) + 120);
    won(anim.t0 + (slide ? ANIM_MS : 0));
    ui_animate();
}

static int undo(void)
{
    if (!kt_undo(&G)) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = kt_script(&G, moves);
    won(ui_ms());
    return ok;
}

static void ask_hint(void)
{
    if (busy() || kt_done(&G) || kt_stuck(&G)) { ui_sound(SND_DENY); return; }
    int s = kt_hint(&G);
    hint.sq = s;
    hint.none = s == -1;
    ui_sound(s >= 0 ? SND_TAP : SND_DENY);
    ui_animate();
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    bpz_layout(&B, G.n ? G.n : 5, b, 1);
    puz_font_make(&F, (int)(0.44f * B.sq), 1);
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        art_piece(KNIGHT, WHITE, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
    if (made_line < 0) {                        /* a stroke: white, tinted when drawn */
        made_line = 1;
        static const uint8_t white[4] = {255, 255, 255, 255};
        art_roundrect(64, 12, 6, white, ui_buf(64, 12));
        t_line = ui_tex(64, 12, ui_buf(64, 12));
    }
}

static void centre(int s, float *x, float *y) { bpz_centre(&B, s % G.n, s / G.n, 0, x, y); }

static int sq_at(float x, float y)
{
    int f, r;
    return bpz_at(&B, x, y, 0, &f, &r) ? r * G.n + f : -1;
}

/* a stroke from one square's centre to another's: tint, alpha, thickness (a part of a square) */
static void stroke(int a, int b, Uint8 r, Uint8 g, Uint8 bl, Uint8 al, float th)
{
    float x0, y0, x1, y1;
    centre(a, &x0, &y0);
    centre(b, &x1, &y1);
    float len = hypotf(x1 - x0, y1 - y0) + th * B.sq;
    SDL_SetTextureColorMod(t_line, r, g, bl);
    SDL_SetTextureAlphaMod(t_line, al);
    ui_put_rot(t_line, (x0 + x1) / 2, (y0 + y1) / 2, len, th * B.sq, atan2f(y1 - y0, x1 - x0) * 180 / PUZ_PI);
    SDL_SetTextureAlphaMod(t_line, 255);
}

static void draw(void)
{
    if (B.n != G.n || !B.board) layout(last_box);
    Uint32 now = ui_ms();
    int n = G.n, at = G.path[G.len - 1];
    bpz_draw_board(&B);
    float st = anim.on ? (now - anim.t0) / (float)ANIM_MS : 2;
    if (anim.on && st >= 1) anim.on = 0;
    float wt = win.on ? (float)now - (float)win.t0 : -1;
    /* the path: faint lines, then the numbers */
    for (int i = 1; i < G.len; i++) {
        if (anim.on && i == G.len - 1) continue;   /* the jump still on its way */
        stroke(G.path[i - 1], G.path[i], 52, 34, 22, 80, 0.06f);
    }
    if (win.on && kt_closed(&G)) {              /* the closed tour's ends joined in gold */
        float q = clampf((wt - n * n * step_ms() - WAVE_LEN * 0.4f) / CLOSE_MS, 0, 1);
        if (q > 0) stroke(G.path[G.len - 1], G.path[0], 232, 190, 92, (Uint8)(230 * q), 0.09f);
    } else if (kt_closed(&G))
        stroke(G.path[G.len - 1], G.path[0], 232, 190, 92, 230, 0.09f);
    if (wt >= 0) {
        for (int i = 0; i < G.len; i++) {
            int s = G.path[i];
            bpz_glow(&B, s % n, s / n, 0, 0.75f * puz_pulse(wt, (float)i * step_ms(), WAVE_LEN));
        }
        if (wt < win_end()) ui_animate();
        else win.on = 0;
    }
    for (int s = 0; s < n * n; s++) {
        if (!G.order[s] || s == at) continue;     /* under the knight: none */
        float x, y;
        centre(s, &x, &y);
        int light = bpz_light(n, s % n, s / n);
        puz_number(&F, x, y, 0.44f * B.sq, G.order[s], light ? PUZ_INK : PUZ_CREAM, light ? 230 : 245);
    }
    /* where the knight can go; the hint's counts and its square */
    int8_t to[8];
    int k = anim.on || kt_done(&G) ? 0 : kt_moves(&G, to);
    for (int i = 0; i < k; i++) {
        float x, y;
        centre(to[i], &x, &y);
        if (hint.sq >= 0) {
            if (to[i] == hint.sq) {
                ui_mark_gold(x, y, 1.5f * B.sq);
                ui_mark_gold(x, y, 1.1f * B.sq);
            }
            ui_mark_dot(x, y, 0.5f * B.sq);      /* the count of onward moves, in cream on a dark dot */
            puz_number(&F, x, y, 0.3f * B.sq, kt_onward(&G, to[i]), PUZ_CREAM, 255);
        } else
            ui_mark_dot(x, y, 0.3f * B.sq);
    }
    /* the knight */
    float x, y;
    bpz_xy(&B, at % n, at / n, 0, &x, &y);
    if (anim.on) {
        float x0, y0, t = puz_ease(st);
        bpz_xy(&B, anim.from % n, anim.from / n, 0, &x0, &y0);
        x = x0 + (x - x0) * t;
        y = y0 + (y - y0) * t - sinf(PUZ_PI * t) * 0.35f * B.sq;   /* a knight leaps */
        stroke(anim.from, G.path[G.len - 1], 52, 34, 22, (Uint8)(80 * t), 0.06f);
        ui_animate();
    } else if (kt_stuck(&G) || hint.none) ui_mark_glow(x + B.sq / 2.0f, y + B.sq / 2.0f, 1.2f * B.sq);
    if (drag.on && drag.moved) {
        x = drag.x - B.sq / 2.0f;
        y = drag.y - 0.62f * B.sq;
    }
    bpz_piece_at(&B, KNIGHT, WHITE, x, y, drag.on && drag.moved ? 1.12f : 1, 255);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&drag, 0, sizeof drag);
    if (busy() || kt_done(&G)) return;
    int s = sq_at(x, y);
    if (s < 0) return;
    if (s == G.path[G.len - 1]) {               /* the knight: dragged */
        drag.on = 1;
        drag.x = drag.x0 = x;
        drag.y = drag.y0 = y;
        return;
    }
    if (kt_can(&G, s)) jump(s, 1);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (hypotf(x - drag.x0, y - drag.y0) > 0.25f * B.sq) drag.moved = 1;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    int moved = drag.moved;
    drag.on = 0;
    ui_animate();
    if (!moved) return;
    int s = sq_at(x, y);
    if (s < 0 || s == G.path[G.len - 1]) return;
    if (kt_can(&G, s)) jump(s, 0);
    else ui_sound(SND_DENY);
}

/* ---- the bar: the squares visited, of all; the closed tour's ring; the lamp -------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    barbox = b;
    float u = ui.unit, cy = b.y + b.h / 2, room = b.w - (kt_done(&G) ? 0 : 1.1f * u);
    float w_moves = 0.72f * u + bpz_number_w(G.len), w_total = 0.6f * u + bpz_number_w(G.n * G.n);
    float x = puz_bar_moves(b, t_small, G.len) + 0.08f * u;
    if (w_moves + w_total <= room) {            /* a narrow bar (landscape): the board's count left out */
        x = bpz_bar_pic(&P, BPZ_SLASH, x, cy, 0.8f);
        x = bpz_bar_number(x, cy, G.n * G.n);
    }
    x += 0.3f * u;
    if (kt_closed(&G) && !(win.on && busy()) && x + 0.6f * u <= b.x + b.w) bpz_bar_pic(&P, BPZ_LOOP, x, cy, 1);
    if (!kt_done(&G)) bpz_button(&D, &P, BPZ_LAMP, b, 0, hint.sq >= 0, kt_stuck(&G));
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (kt_done(&G) || !bpz_button_hit(barbox, 0, x, y)) return 0;
    ask_hint();
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    art_piece(KNIGHT, WHITE, size, rgba);
}

/* the menu's card and the picker's: the board, the start of a tour numbered, the knight on its last square */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? KT_5 : variant, ox = 0;
    kt_game T;
    kt_new_at(&T, kt_size(v), 0);
    int8_t out[KT_MAX];
    if (kt_solve(&T, out, 200000) == 1)
        for (int i = 0; i < (variant < 0 ? 9 : T.n + 2); i++) kt_move(&T, out[i]);
    int n = T.n, sq = bpz_thumb_board(n, size, rgba, &ox);
    if (!sq) return;
    for (int i = 0; i + 1 < T.len; i++) {
        int s = T.path[i], f = s % n, r = n - 1 - s / n, light = bpz_light(n, s % n, s / n);
        puz_digits(rgba, size, size, i + 1, ox + (f + 0.5f) * sq, ox + (r + 0.53f) * sq, 0.5f * sq,
                   light ? PUZ_INK : PUZ_CREAM, 1, 1);
    }
    int s = T.path[T.len - 1], d = (int)(1.1f * sq);
    bpz_thumb_piece(rgba, size, KNIGHT, WHITE, ox + s % n * sq + (sq - d) / 2, ox + (n - 1 - s / n) * sq + (sq - d) / 2,
                    d);
}

static const game knightstour_game = {
    .id = "knightstour", .order = 431, .variants = KT_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
};
REGISTER_GAME(knightstour_game)
