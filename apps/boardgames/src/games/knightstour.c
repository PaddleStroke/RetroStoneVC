/*
 * The Knight's Tour in RetroStone Board, for one player: the house board and a knight that must visit every square once
 * (the rules: knightstour_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The level screen: 150 levels in six chapters (boards of every shape, holes cut in them, the start given), and a die
 * tab for a random tour on 5 x 5, 6 x 6 or 8 x 8 (the start drawn). Tap a square the knight can reach (the dots), or
 * drag it there: it jumps, and the square it left keeps its number, a faint line joining the path. Stuck with squares
 * left: a red glow (a dead end; undo). The lamp, the shell's tool (Warnsdorff's rule): each square the knight can reach
 * shows how many moves it would have from there, and the best one lights up in gold (a search checks it still leads to
 * a whole tour; a red glow on the knight when none can be finished from here). Undo takes back one jump, all the way to
 * the start. The bar: the squares visited and the board's count. Done: a gold light runs along the path in its order; a
 * closed tour (the last square a knight's move from the first) also joins the ends with a gold line, and a gold ring
 * shows in the bar. The stars: 3 with no hint and no dead end, 2 with up to two hints and dead ends together, else 1.
 * The board fills the room the shell gives, turned a quarter when that makes its squares bigger.
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
static puz_font F;
static box last_box;
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

static void begun(void)
{
    calm();
    if (last_box.w > 0) ui_animate();
}

static void start(int level)                    /* a level of the level screen */
{
    if (!kt_level(&G, level)) kt_new(&G, KT_5, 1);   /* never: every level is tested */
    begun();
}

static void start_random(int variant)           /* the die tab: a random tour */
{
    kt_new(&G, variant < 0 || variant >= KT_VARIANTS ? KT_5 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begun();
}

static int status(void) { return kt_done(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return kt_stars(&G); }
static int step_ms(void) { return WAVE_TOTAL / (G.cells > 0 ? G.cells : 1); }
static int win_end(void) { return G.cells * step_ms() + WAVE_LEN + (kt_closed(&G) ? CLOSE_MS : 0); }

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
    if (G.w) bpz_layout_holes(&B, G.w, G.h, b, 1, G.hole);
    else bpz_layout(&B, 5, b, 1);
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

static void centre(int s, float *x, float *y) { bpz_centre(&B, s % G.w, s / G.w, 0, x, y); }

static int sq_at(float x, float y)
{
    int f, r;
    return bpz_at(&B, x, y, 0, &f, &r) && !G.hole[r * G.w + f] ? r * G.w + f : -1;
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
    if (B.nx != G.w || B.ny != G.h || B.made_holes != bpz_holes_hash(G.w * G.h, G.hole) || !B.board) layout(last_box);
    Uint32 now = ui_ms();
    int w = G.w, at = G.path[G.len - 1];
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
        float q = clampf((wt - G.cells * step_ms() - WAVE_LEN * 0.4f) / CLOSE_MS, 0, 1);
        if (q > 0) stroke(G.path[G.len - 1], G.path[0], 232, 190, 92, (Uint8)(230 * q), 0.09f);
    } else if (kt_closed(&G))
        stroke(G.path[G.len - 1], G.path[0], 232, 190, 92, 230, 0.09f);
    if (wt >= 0) {
        for (int i = 0; i < G.len; i++) {
            int s = G.path[i];
            bpz_glow(&B, s % w, s / w, 0, 0.75f * puz_pulse(wt, (float)i * step_ms(), WAVE_LEN));
        }
        if (wt < win_end()) ui_animate();
        else win.on = 0;
    }
    for (int s = 0; s < w * G.h; s++) {
        if (!G.order[s] || s == at) continue;     /* under the knight: none */
        float x, y;
        centre(s, &x, &y);
        int light = bpz_light_on(&B, s % w, s / w);
        float dh = (G.order[s] >= 100 ? 0.36f : 0.44f) * B.sq;
        puz_number(&F, x, y, dh, G.order[s], light ? PUZ_INK : PUZ_CREAM, light ? 230 : 245);
    }
    /* where the knight can go; the hint's counts and its square */
    uint8_t to[8];
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
    bpz_xy(&B, at % w, at / w, 0, &x, &y);
    if (anim.on) {
        float x0, y0, t = puz_ease(st);
        bpz_xy(&B, anim.from % w, anim.from / w, 0, &x0, &y0);
        x = x0 + (x - x0) * t;
        y = y0 + (y - y0) * t;
        stroke(anim.from, G.path[G.len - 1], 52, 34, 22, (Uint8)(80 * t), 0.06f);
        float up = sinf(PUZ_PI * t);            /* a knight leaps: its shadow stays on the board, smaller below it */
        float sk = 1 - 0.18f * up, ss = sk * B.sq;
        ui_shadow(x + (B.sq - ss) / 2 + 0.12f * up * B.sq, y + (B.sq - ss) / 2, ss);
        y -= up * 0.35f * B.sq;
        ui_animate();
    } else if (kt_stuck(&G) || hint.none) ui_mark_glow(x + B.sq / 2.0f, y + B.sq / 2.0f, 1.2f * B.sq);
    if (drag.on && drag.moved) {
        x = drag.x - B.sq / 2.0f;
        y = drag.y - 0.62f * B.sq;
    }
    /* in the air, its shadow drawn above: 254, all but opaque, draws the piece alone (bpz_piece_at) */
    bpz_piece_at(&B, KNIGHT, WHITE, x, y, drag.on && drag.moved ? 1.12f : 1, anim.on && !(drag.on && drag.moved) ? 254 : 255);
    ui_tool(0, ICON_HINT, (hint.sq >= 0 ? UI_ON : 0) | (kt_done(&G) || kt_stuck(&G) ? UI_DIM : 0));   /* the lamp */
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    memset(&drag, 0, sizeof drag);
    if (ui_tool_hit(x, y) == 0) {               /* the lamp */
        ask_hint();
        return;
    }
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

/* ---- the bar: the squares visited, of all; the closed tour's ring ------------------------------------------------ */
static void bar(int player, box b)
{
    (void)player;
    float u = ui.unit, cy = b.y + b.h / 2, room = b.w;
    float w_moves = 0.72f * u + bpz_number_w(G.len), w_total = 0.6f * u + bpz_number_w(G.cells);
    float x = puz_bar_moves(b, t_small, G.len) + 0.08f * u;
    if (w_moves + w_total <= room) {            /* a narrow bar (landscape): the board's count left out */
        x = bpz_bar_pic(&P, BPZ_SLASH, x, cy, 0.8f);
        x = bpz_bar_number(x, cy, G.cells);
    }
    x += 0.3f * u;
    if (kt_closed(&G) && !(win.on && busy()) && x + 0.6f * u <= b.x + b.w) bpz_bar_pic(&P, BPZ_LOOP, x, cy, 1);
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    art_piece(KNIGHT, WHITE, size, rgba);
}

/* a board in a card: its squares and holes, the first `steps` squares of a tour numbered, the knight on its square */
static void board_pic(kt_game *T, int steps, int size, uint8_t *rgba)
{
    uint8_t out[KT_MAX];
    if (steps > 0 && kt_solve(T, out, 200000) == 1)
        for (int i = 0; i < steps && T->len < T->cells; i++) kt_move(T, out[i]);
    int ox = 0, oy = 0, w = T->w, sq = bpz_thumb_board_holes(w, T->h, T->hole, size, rgba, &ox, &oy);
    if (!sq) return;
    for (int i = 0; i + 1 < T->len; i++) {
        int s = T->path[i], f = s % w, r = T->h - 1 - s / w, light = bpz_light(T->h, s % w, s / w);
        puz_digits(rgba, size, size, i + 1, ox + (f + 0.5f) * sq, oy + (r + 0.53f) * sq, 0.5f * sq,
                   light ? PUZ_INK : PUZ_CREAM, 1, 1);
    }
    int s = T->path[T->len - 1], d = (int)(1.1f * sq);
    bpz_thumb_piece(rgba, size, KNIGHT, WHITE, ox + s % w * sq + (sq - d) / 2,
                    oy + (T->h - 1 - s / w) * sq + (sq - d) / 2, d);
}

/* the menu's card and the picker's: the board, the start of a tour numbered, the knight on its last square */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? KT_5 : variant;
    kt_game T;
    kt_new_at(&T, kt_size(v), 0);
    board_pic(&T, variant < 0 ? 9 : T.w + 2, size, rgba);
}

/* a level's tile: its board, its holes, the knight on the start */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    kt_game T;
    if (kt_level(&T, level)) board_pic(&T, 0, size, rgba);
}

/* a chapter's tab: a small board of its size (4 x 4 .. 9 x 9), the knight in its corner */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int n = 4 + (chapter < 0 ? 0 : chapter > 5 ? 5 : chapter), ox = 0, oy = 0;
    int sq = bpz_thumb_board_holes(n, n, NULL, size, rgba, &ox, &oy), d = (int)(2.2f * sq);
    if (sq) bpz_thumb_piece(rgba, size, KNIGHT, WHITE, ox, oy + n * sq - d, d);
}

static const game knightstour_game = {
    .id = "knightstour", .order = 431, .variants = KT_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 1,
    .levels = KT_LEVELS, .chapters = KT_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars,
};
REGISTER_GAME(knightstour_game)
