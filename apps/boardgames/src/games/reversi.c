/*
 * Reversi in RetroStone Board: the board, a tap on a dot sets a disc down, the outflanked discs turn one after the
 * other, a player who cannot move passes by itself (the pass sign in their bar). The rules: reversi_rules.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The moves' notation (script: the screenshots, the tests): "d3 c5 f6 ...", a square a move, the column a..h from
 * player 0's left, the row 1..8 from the top (player 1's side), as in the usual reversi notation. Passes are not
 * written: they play themselves.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/reversi.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define PI 3.14159265f
#define DROP_MS 90                              /* the new disc set down, before the first one turns */
#define STEP_MS 65                              /* from one turning disc to the next */
#define FLIP_MS 220                             /* one disc turning over */
#define PASS_MS 1800                            /* the pass sign in the bar */

static rgame G;
static uint64_t legal;
static int sq, fr, made = -1, made_unit = -1;
static float bx, by;                            /* the a1 corner (h8 when flipped) */
static SDL_Texture *t_disc[2], *t_shade, *t_board, *t_pass, *t_pass_bg;
static struct { int on, s, n, order[64]; Uint32 t0; } anim;   /* the turning discs, nearest first */
static struct { int on, who, rang; Uint32 t0; } pass;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    const rpos *p = rv_pos(&G);
    legal = p->over ? 0 : rv_legal(p, p->side);
}

static void start(int variant)
{
    (void)variant;
    rv_new(&G);
    anim.on = pass.on = 0;
    refresh();
}

static int status(void)
{
    static const int map[] = {[RV_PLAY] = GS_PLAY, [RV_WIN0] = GS_WIN0, [RV_WIN1] = GS_WIN1, [RV_DRAW] = GS_DRAW};
    return map[rv_status(&G)];
}

static int turn(void) { return rv_pos(&G)->side; }

static int dist(int a, int b)
{
    int df = abs(a % 8 - b % 8), dr = abs(a / 8 - b / 8);
    return df > dr ? df : dr;
}

static int anim_ms(void) { return DROP_MS + (anim.n ? (anim.n - 1) * STEP_MS + FLIP_MS : 0); }

static void play(int s, int animate)
{
    const rpos *p = rv_pos(&G);
    uint64_t flips = rv_flips(p, p->side, s);
    if (!rv_play(&G, s)) return;
    anim.on = animate;
    anim.s = s;
    anim.n = 0;
    anim.t0 = ui_ms();
    for (int d = 1; d < 8; d++)                 /* the turn runs out from the new disc */
        for (int k = 0; k < 64; k++)
            if ((flips >> k & 1) && dist(k, s) == d) anim.order[anim.n++] = k;
    p = rv_pos(&G);
    pass.on = p->passed;                        /* the other player had no move: their pass sign, after the turns */
    pass.who = !p->side;
    pass.rang = !animate;
    pass.t0 = animate ? anim.t0 + anim_ms() : anim.t0 - 300;
    refresh();
    ui_sound(SND_PLACE);
}

static int undo(void)
{
    if (!rv_undo(&G)) return 0;
    anim.on = pass.on = 0;
    refresh();
    return 1;
}

/* "d3 c5 f6": the screenshots' and the tests' moves */
static int script(const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int f = (s[0] | 32) - 'a', r = s[1] - '1';
        if (f < 0 || f > 7 || r < 0 || r > 7 || !(legal >> RV_SQ(f, r) & 1)) return 0;
        play(RV_SQ(f, r), 0);
        s += 2;
    }
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the lines between the squares and the four dots at the corners of the middle, inked on the plank (w pixels wide,
 * n x n squares of s pixels from (f, f)) */
static void inlay(uint8_t *o, int w, int n, int s, int f)
{
    static const float ink[3] = {74, 46, 28};
    int lw = s >= 48 ? s / 24 : s >= 16 ? 2 : 1, len = n * s;
    char *on = calloc((size_t)len, 1);
    if (!on) return;
    for (int i = 0; i <= n; i++) {
        int c = i * s - lw / 2;
        c = c < 0 ? 0 : c > len - lw ? len - lw : c;
        memset(on + c, 1, (size_t)lw);
    }
    for (int y = 0; y < len; y++)
        for (int x = 0; x < len; x++) {
            if (!on[x] && !on[y]) continue;
            uint8_t *p = o + 4 * ((y + f) * w + x + f);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * 0.3f + ink[i] * 0.7f);
        }
    free(on);
    float r = fmaxf(1.6f, 0.075f * s);
    for (int gy = 2; gy < n; gy += 4)
        for (int gx = 2; gx < n; gx += 4) {
            float cx = f + gx * s, cy = f + gy * s;
            for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
                for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
                    float a = r - hypotf(x + 0.5f - cx, y + 0.5f - cy) + 0.5f;
                    a = a < 0 ? 0 : a > 1 ? 1 : a;
                    uint8_t *p = o + 4 * (y * w + x);
                    for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * (1 - a) + ink[i] * 0.8f * a);
                }
        }
}

/* the shadow of a disc lying on the wood: as wide as the disc, its edge soft, size x size */
static void shade(int size, uint8_t *o)
{
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float d = hypotf(x + 0.5f - size / 2.0f, y + 0.5f - size / 2.0f) / size, a = (0.5f - d) / 0.12f;
            uint8_t *p = o + 4 * (y * size + x);
            p[0] = p[1] = p[2] = 0;
            p[3] = (uint8_t)(110 * (a < 0 ? 0 : a > 1 ? 1 : a));
        }
}

static void layout(box b)
{
    sq = (int)floorf(b.w / (8 + 2 * FRAME));
    fr = (int)(sq * FRAME);
    float bw = 8.0f * sq + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2) + fr;
    by = floorf(b.y + (b.h - bw) / 2) + fr;
    if (made != sq) {
        made = sq;
        for (int c = 0; c < 2; c++) {           /* player 0 ebony: the house's colour 1 */
            ui_free(&t_disc[c]);
            art_counter(COUNTER_DISC, !c, sq, ui_buf(sq, sq));
            t_disc[c] = ui_tex(sq, sq, ui_buf(sq, sq));
        }
        ui_free(&t_shade);
        shade(sq, ui_buf(sq, sq));
        t_shade = ui_tex(sq, sq, ui_buf(sq, sq));
        int n = 8 * sq + 2 * fr;
        ui_free(&t_board);
        art_board_n(8, 8, sq, fr, 0, ui_buf(n, n));
        inlay(ui_buf(n, n), n, 8, sq, fr);
        t_board = ui_tex(n, n, ui_buf(n, n));
    }
    int d = (int)(0.84f * ui.unit), ic = (int)(0.62f * d);
    if (made_unit != d) {                       /* the pass sign: the shell's pass button, lit */
        made_unit = d;
        static const uint8_t lit[3] = {150, 118, 46};
        ui_free(&t_pass_bg);
        art_disc(d, lit, 1, 0, ui_buf(d, d));
        t_pass_bg = ui_tex(d, d, ui_buf(d, d));
        ui_free(&t_pass);
        art_icon(ICON_PASS, ic, ui_buf(ic, ic));
        t_pass = ui_tex(ic, ic, ui_buf(ic, ic));
        SDL_SetTextureColorMod(t_pass, 252, 244, 226);
    }
}

static void sq_xy(int s, float *x, float *y)
{
    int f = s % 8, r = s / 8;
    if (ui.flipped) { f = 7 - f; r = 7 - r; }
    *x = bx + f * sq;
    *y = by + r * sq;
}

static int xy_sq(float x, float y)
{
    int f = (int)floorf((x - bx) / sq), r = (int)floorf((y - by) / sq);
    if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
    return ui.flipped ? RV_SQ(7 - f, 7 - r) : RV_SQ(f, r);
}

/* a disc of player c centred on (cx, cy): w its width (1 flat, 0 on its edge), k its size (lifted: bigger) */
static void disc_at(int c, float cx, float cy, float w, float k)
{
    float d = sq * k, lift = (k - 1) * sq;
    ui_put_c(t_shade, cx + 0.05f * sq + 0.5f * lift, cy + 0.08f * sq + lift, d * fmaxf(w, 0.12f), d);
    ui_put_c(t_disc[c], cx, cy, d * fmaxf(w, 0.06f), d);
}

static void draw(void)
{
    const rpos *p = rv_pos(&G);
    ui_put(t_board, bx - fr, by - fr, 8.0f * sq + 2 * fr, 8.0f * sq + 2 * fr);
    float x, y;
    if (G.ply) {
        sq_xy(G.move[G.ply - 1], &x, &y);
        ui_mark_last(x, y, sq, sq);
    }
    int el = (int)(ui_ms() - anim.t0);
    if (anim.on && el >= anim_ms()) anim.on = 0;
    if (anim.on) ui_animate();
    for (int s = 0; s < 64; s++) {
        int c = p->disc[0] >> s & 1 ? 0 : p->disc[1] >> s & 1 ? 1 : -1;
        if (c < 0) continue;
        float w = 1, k = 1;
        if (anim.on && s == anim.s && el < DROP_MS) k = 1.14f - 0.14f * el / DROP_MS;   /* set down from above */
        for (int i = 0; anim.on && i < anim.n; i++) {
            if (anim.order[i] != s) continue;   /* squeezed to a line and back, the colour changing on the edge */
            float t = (el - DROP_MS - i * STEP_MS) / (float)FLIP_MS;
            if (t < 0.5f) c = !c;
            if (t > 0 && t < 1) w = fabsf(cosf(t * PI)), k = 1 + 0.12f * sinf(t * PI);
        }
        sq_xy(s, &x, &y);
        disc_at(c, x + sq / 2.0f, y + sq / 2.0f, w, k);
    }
    if (!anim.on)                               /* where the player to move can set a disc */
        for (int s = 0; s < 64; s++)
            if (legal >> s & 1) {
                sq_xy(s, &x, &y);
                ui_mark_dot(x + sq / 2.0f, y + sq / 2.0f, 0.32f * sq);
            }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    int s = xy_sq(x, y);
    if (s >= 0 && (legal >> s & 1)) play(s, 1);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the player's discs on the board; the pass sign a moment when they had to pass */
static void bar(int player, box b)
{
    float u = ui.unit, cy = b.y + b.h / 2;
    ui_number(b.x + 0.45f * u, cy, 0.46f * u, rv_count(rv_pos(&G), player), 1);
    if (!pass.on || pass.who != player) return;
    int el = (int)(ui_ms() - pass.t0);
    ui_animate();
    if (el < 0) return;
    if (el >= PASS_MS) { pass.on = 0; return; }
    if (!pass.rang) { pass.rang = 1; ui_sound(SND_DENY); }
    float a = fminf(1, el / 150.0f) * fminf(1, (PASS_MS - el) / 400.0f);
    float r = 0.42f * u * (1 + 0.18f * (el < 300 ? sinf(el / 300.0f * PI) : 0)), x = b.x + b.w - 0.47f * u;
    SDL_SetTextureAlphaMod(t_pass_bg, (Uint8)(a * 255));
    SDL_SetTextureAlphaMod(t_pass, (Uint8)(a * 255));
    ui_put_c(t_pass_bg, x, cy, 2 * r, 2 * r);
    ui_put_c(t_pass, x, cy, 1.24f * r, 1.24f * r);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, !player, size, rgba); }

/* a disc picture squeezed across to w of its width (a disc turning over), s x s */
static void squeeze(const uint8_t *src, int s, float w, uint8_t *out)
{
    memset(out, 0, (size_t)s * s * 4);
    for (int x = 0; x < s; x++) {
        int sx = (int)floorf(s / 2.0f + (x + 0.5f - s / 2.0f) / w);
        if (sx < 0 || sx >= s) continue;
        for (int y = 0; y < s; y++) memcpy(out + 4 * (y * s + x), src + 4 * (y * s + sx), 4);
    }
}

/* the menu's card: a corner of the board, the discs of a game going on, one of them turning over */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = ui_buf(n, n);
    art_board_n(4, 4, s, f, 0, board);
    inlay(board, n, 4, s, f);
    art_blend(rgba, size, size, board, n, n, o, o);
    static const int place[][3] = {{3, 3, 1}, {2, 3, 0}, {3, 2, 0}, {1, 3, 0}, {2, 2, 0}, {1, 2, 1}, {3, 1, 1},
                                   {2, 1, 0}};
    uint8_t *flat = malloc((size_t)s * s * 4);
    if (!flat) return;
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        int x = o + f + place[i][0] * s, y = o + f + place[i][1] * s, turning = place[i][0] == 2 && place[i][1] == 1;
        float w = turning ? 0.42f : 1;
        uint8_t *one = ui_buf(s, s);
        shade(s, one);
        squeeze(one, s, w, flat);
        art_blend(rgba, size, size, flat, s, s, x + s / 20, y + s * 8 / 100);
        art_counter(COUNTER_DISC, !place[i][2], s, one);
        squeeze(one, s, w, flat);
        art_blend(rgba, size, size, flat, s, s, x, turning ? y - s / 12 : y);
    }
    free(flat);
}

static const game reversi_game = {
    .id = "reversi", .order = 30, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script,
};
REGISTER_GAME(reversi_game)
