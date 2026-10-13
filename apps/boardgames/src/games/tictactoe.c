/*
 * Tic-tac-toe in RetroStone Board: classic, and Ultimate (nine small boards in a big one). The grid carved in the
 * plank, ivory crosses and ebony rings, tap a square (the rules and the notation: tictactoe_rules.c, tictactoe.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board is measured in ninths: a classic square is three of them, an Ultimate square one. Ultimate: the board(s)
 * where the player must play are lit, a decided small board is veiled and its owner's big mark laid over it.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/tictactoe.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in ninths */
#define POP_MS 190                              /* a mark set down */
#define CLAIM_MS 300                            /* then a small board's big mark */
#define STRIKE_W 0.09f                          /* the strike through a winning line: its width, in squares (3 u) */
#define STRIKE_OVER 0.62f                       /* and how far it runs past the marks at its ends */

static tt_game G;
static int u, fr, made = -1, made_v = -1;      /* a ninth of the board, the frame, pixels */
static float bx, by;                            /* the top left square's corner */
static box last_box;
static SDL_Texture *t_board, *t_mark[2], *t_big[2], *t_strike[2];   /* the strikes: straight, diagonal */
static int strike_px[2][2];                     /* their pictures' sizes */
static struct { int on, cell, claim; Uint32 t0; } anim;
static const float INK[3] = {104, 68, 42}, LIT[3] = {248, 226, 186}, WALNUT[3] = {92, 60, 40};
static const uint8_t GOLD[3] = {255, 204, 72}, LIGHT[4] = {255, 224, 126, 104}, VEIL[4] = {226, 190, 136, 170};
static const art_style STRIKE = {{250, 200, 74}, {84, 52, 18}, {84, 52, 18}, {0, 0, 0}, 0, 0};   /* flat */

static int side(void) { return G.ultimate ? 9 : 3; }            /* squares along a side */
static int cs(void) { return G.ultimate ? u : 3 * u; }          /* a square, pixels */

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void layout(box b);

static void start(int variant)
{
    tt_new(&G, variant == 1);
    anim.on = 0;
    if (made >= 0) layout(last_box);            /* the other variant's board */
}

static int status(void)
{
    switch (tt_status(&G)) {
    case TT_WIN0: return GS_WIN0;
    case TT_WIN1: return GS_WIN1;
    case TT_DRAW: return GS_DRAW;
    default: return GS_PLAY;
    }
}

static int turn(void) { return tt_turn(&G); }

static int undo(void)
{
    if (!tt_undo(&G)) return 0;
    anim.on = 0;
    return 1;
}

/* "5 1 9" (classic, squares 1-9 from the top left, row by row) or "5.5 5.1 1.9" (Ultimate, board.square, both
 * numbered so): the screenshots' and the tests' moves */
static int script(const char *s)
{
    int ok = tt_script(&G, s);
    anim.on = 0;
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a rectangle (pixels, smooth edges) painted over a picture w x h */
static void paint_rect(uint8_t *o, int w, int h, float x0, float y0, float x1, float y1, const float c[3], float a)
{
    int px = (int)floorf(x0), py = (int)floorf(y0), pw = (int)ceilf(x1) - px, ph = (int)ceilf(y1) - py;
    uint8_t *one = malloc((size_t)pw * ph * 4);
    art_fig *f = art_fig_new();
    if (one && f) {
        art_style st = {{c[0], c[1], c[2]}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 0, 0};
        art_rect(f, ART_PART, x0 - px, y0 - py, x1 - px, y1 - py);
        art_render(f, &st, pw, ph, 1, 0, 0, one);
        for (int i = 0; i < pw * ph; i++) one[4 * i + 3] = (uint8_t)(one[4 * i + 3] * a);
        art_blend(o, w, h, one, pw, ph, px, py);
    }
    free(one);
    art_fig_free(f);
}

/* a line carved in the wood (dark, its lower edge catching the light) or inlaid (walnut), pixels */
static void groove(uint8_t *o, int n, float x0, float y0, float x1, float y1, int inlaid)
{
    float d = fmaxf(1, 0.3f * fminf(x1 - x0, y1 - y0));
    paint_rect(o, n, n, x0 + d, y0 + d, x1 + d, y1 + d, LIT, 0.75f);
    paint_rect(o, n, n, x0, y0, x1, y1, inlaid ? WALNUT : INK, inlaid ? 1 : 0.8f);
}

/* the plank and its grid: ninths of s pixels in a frame of f; (9 s + 2 f) squared */
static void paint_board(int ultimate, int s, int f, uint8_t *o)
{
    int n = 9 * s + 2 * f;
    art_board_n(9, 9, s, f, 0, o);
    if (!ultimate) {
        float w = fmaxf(2, 0.1f * s) / 2, a = f + 0.35f * s, b = f + 8.65f * s;
        for (int k = 3; k <= 6; k += 3) {
            groove(o, n, f + k * s - w, a, f + k * s + w, b, 0);
            groove(o, n, a, f + k * s - w, b, f + k * s + w, 0);
        }
        return;
    }
    float w = fmaxf(1.4f, 0.045f * s) / 2;      /* the small boards' lines: thin, short of the bold ones */
    for (int B = 0; B < 9; B++) {
        float ox = f + 3 * (B % 3) * s, oy = f + 3 * (B / 3) * s, a = 0.2f * s, b = 2.8f * s;
        for (int k = 1; k <= 2; k++) {
            groove(o, n, ox + k * s - w, oy + a, ox + k * s + w, oy + b, 0);
            groove(o, n, ox + a, oy + k * s - w, ox + b, oy + k * s + w, 0);
        }
    }
    float W = fmaxf(3, 0.13f * s) / 2, a = f + 0.12f * s, b = f + 8.88f * s;   /* between them: walnut, bold */
    for (int k = 3; k <= 6; k += 3) {
        groove(o, n, f + k * s - W, a, f + k * s + W, b, 1);
        groove(o, n, a, f + k * s - W, b, f + k * s + W, 1);
    }
}

/* a player's mark, size x size: player 0 a cross, 1 a ring, in the house's ivory and ebony (outline, light); shadow:
 * its soft shadow on the wood under it, towards the lower right */
static void mark_pic(int player, int size, int shadow, uint8_t *rgba)
{
    art_style st = art_player[player ? 1 : 0];
    st.line = fmaxf(0.022f, 1.3f / size);
    art_fig *f = art_fig_new();
    uint8_t *hole = malloc((size_t)size * size * 4);
    if (!f || !hole) { art_fig_free(f); free(hole); memset(rgba, 0, (size_t)size * size * 4); return; }
    if (!player) {                              /* a plus turned a quarter round, its arms square-ended */
        const float a = 0.07f, l = 0.335f, c = 0.70710678f;
        const float plus[24] = {a, a, l, a, l, -a, a, -a, a, -l, -a, -l, -a, -a, -l, -a, -l, a, -a, a, -a, l, a, l};
        float p[24];
        for (int i = 0; i < 12; i++) {
            p[2 * i] = 0.5f + c * (plus[2 * i] - plus[2 * i + 1]);
            p[2 * i + 1] = 0.5f + c * (plus[2 * i] + plus[2 * i + 1]);
        }
        art_poly(f, ART_PART, p, 12);
        art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    } else {                                    /* a disc, then its middle cut out: an inner outline, a hole */
        const float r0 = 0.19f, r1 = 0.31f;
        art_ellipse(f, ART_PART, 0.5f, 0.5f, r1, r1);
        art_render(f, &st, size, size, (float)size, 0, 0, rgba);
        art_style flat = st;
        flat.line = 0;
        art_fig_free(f);
        f = art_fig_new();
        if (f) {
            art_ellipse(f, ART_PART, 0.5f, 0.5f, r0 + st.line, r0 + st.line);
            art_render(f, &flat, size, size, (float)size, 0, 0, hole);
            for (int i = 0; i < size * size; i++)
                for (int k = 0; k < 3; k++) {
                    float t = hole[4 * i + 3] / 255.0f;
                    rgba[4 * i + k] = (uint8_t)(rgba[4 * i + k] + (st.outer[k] - rgba[4 * i + k]) * t + 0.5f);
                }
            art_fig_free(f);
            f = art_fig_new();
        }
        if (f) {
            art_ellipse(f, ART_PART, 0.5f, 0.5f, r0, r0);
            art_render(f, &flat, size, size, (float)size, 0, 0, hole);
            for (int i = 0; i < size * size; i++)
                rgba[4 * i + 3] = (uint8_t)(rgba[4 * i + 3] * (1 - hole[4 * i + 3] / 255.0f) + 0.5f);
        }
    }
    art_fig_free(f);
    if (shadow) {                               /* the mark's outline, moved and blurred (a box blur, twice) */
        int dx = (int)lroundf(0.035f * size), dy = (int)lroundf(0.05f * size), r = (int)fmaxf(1, 0.025f * size);
        float *a = calloc((size_t)size * size, sizeof *a), *b = calloc((size_t)size * size, sizeof *b);
        if (a && b) {
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++)
                    if (x - dx >= 0 && y - dy >= 0) a[y * size + x] = rgba[4 * ((y - dy) * size + x - dx) + 3];
            for (int pass = 0; pass < 4; pass++) {     /* across, down, across, down */
                for (int y = 0; y < size; y++)
                    for (int x = 0; x < size; x++) {
                        float sum = 0;
                        for (int k = -r; k <= r; k++) {
                            int xx = pass & 1 ? x : x + k, yy = pass & 1 ? y + k : y;
                            if (xx >= 0 && yy >= 0 && xx < size && yy < size) sum += a[yy * size + xx];
                        }
                        b[y * size + x] = sum / (2 * r + 1);
                    }
                float *t = a;
                a = b;
                b = t;
            }
            for (int i = 0; i < size * size; i++) {
                hole[4 * i] = hole[4 * i + 1] = hole[4 * i + 2] = 0;
                hole[4 * i + 3] = (uint8_t)(0.42f * a[i]);
            }
            art_blend(hole, size, size, rgba, size, size, 0, 0);
            memcpy(rgba, hole, (size_t)size * size * 4);
        }
        free(a);
        free(b);
    }
    free(hole);
}

/* the gold stroke struck through a winning line, from (x0, y0) to (x1, y1), w wide, its ends round */
static void strike(art_fig *f, float x0, float y0, float x1, float y1, float w)
{
    float l = hypotf(x1 - x0, y1 - y0), dx = (x1 - x0) / l, dy = (y1 - y0) / l, p[2 * 32];
    int n = 0;
    for (int end = 0; end < 2; end++)
        for (int i = 0; i <= 15; i++) {         /* half a circle round each end */
            float a = 3.14159265f * (i / 15.0f - 0.5f), c = cosf(a) * w / 2, s = sinf(a) * w / 2;
            float sx = end ? -1.0f : 1.0f, cx = end ? x0 : x1, cy = end ? y0 : y1;
            p[n++] = cx + sx * (c * dx - s * dy);
            p[n++] = cy + sx * (c * dy + s * dx);
        }
    art_poly(f, ART_PART, p, n / 2);
}

/* a strike lying across, l x w pixels (its round ends included), in a picture a little larger */
static SDL_Texture *strike_tex(float l, float w, int *pw, int *ph)
{
    *pw = (int)ceilf(l + 4);
    *ph = (int)ceilf(w + 4);
    uint8_t *buf = malloc((size_t)*pw * *ph * 4);
    art_fig *f = art_fig_new();
    SDL_Texture *t = NULL;
    if (buf && f) {
        art_style st = STRIKE;
        st.line = fmaxf(1.2f, 0.07f * w);
        float in = w - 2 * st.line;             /* the outline goes round the shape */
        strike(f, *pw / 2.0f - (l - w) / 2, *ph / 2.0f, *pw / 2.0f + (l - w) / 2, *ph / 2.0f, in);
        art_render(f, &st, *pw, *ph, 1, 0, 0, buf);
        t = ui_tex(*pw, *ph, buf);
    }
    free(buf);
    art_fig_free(f);
    return t;
}

static void make_tex(SDL_Texture **t, int player, int size)
{
    ui_free(t);
    uint8_t *buf = malloc((size_t)size * size * 4);
    if (!buf) return;
    mark_pic(player, size, 1, buf);
    *t = ui_tex(size, size, buf);
    free(buf);
}

static void layout(box b)
{
    last_box = b;
    u = (int)floorf(b.w / (9 + 2 * FRAME));
    fr = (int)(u * FRAME);
    int n = 9 * u + 2 * fr;
    bx = floorf(b.x + (b.w - n) / 2) + fr;
    by = floorf(b.y + (b.h - n) / 2) + fr;
    if (made == u && made_v == G.ultimate) return;
    made = u;
    made_v = G.ultimate;
    uint8_t *board = malloc((size_t)n * n * 4);
    if (board) {
        paint_board(G.ultimate, u, fr, board);
        ui_free(&t_board);
        t_board = ui_tex(n, n, board);
        free(board);
    }
    for (int p = 0; p < 2; p++) {
        make_tex(&t_mark[p], p, cs());
        make_tex(&t_big[p], p, 3 * u);
    }
    for (int d = 0; d < 2; d++) {               /* from a square's middle two squares on (or across), a little over */
        ui_free(&t_strike[d]);
        float l = (d ? 2.828f : 2) + STRIKE_OVER;
        t_strike[d] = strike_tex(l * 3 * u, STRIKE_W * 3 * u, &strike_px[d][0], &strike_px[d][1]);
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void cell_xy(int cell, float *x, float *y)    /* a square's centre */
{
    int c = G.ultimate ? cell / 9 % 3 * 3 + cell % 3 : cell % 3;
    int r = G.ultimate ? cell / 27 * 3 + cell % 9 / 3 : cell / 3;
    if (ui.flipped) { c = side() - 1 - c; r = side() - 1 - r; }
    *x = bx + (c + 0.5f) * cs();
    *y = by + (r + 0.5f) * cs();
}

static void board_xy(int b, float *x, float *y)      /* an Ultimate small board's top left corner */
{
    int c = b % 3, r = b / 3;
    if (ui.flipped) { c = 2 - c; r = 2 - r; }
    *x = bx + 3.0f * c * u;
    *y = by + 3.0f * r * u;
}

static int xy_cell(float x, float y)
{
    int c = (int)floorf((x - bx) / cs()), r = (int)floorf((y - by) / cs()), n = side();
    if (c < 0 || c >= n || r < 0 || r >= n) return -1;
    if (ui.flipped) { c = n - 1 - c; r = n - 1 - r; }
    return G.ultimate ? (r / 3 * 3 + c / 3) * 9 + r % 3 * 3 + c % 3 : r * 3 + c;
}

static float ease(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }

/* a mark set down: it grows in from a little above the board, turning, and fades in (t 0..1) */
static void pop(SDL_Texture *t, float cx, float cy, float size, float k)
{
    float e = ease(k), s = size * (1.3f - 0.3f * e);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(k * 3, 0, 1)));
    ui_put_rot(t, cx, cy, s, s, -30 * (1 - e));
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw(void)
{
    if (made_v != G.ultimate) layout(last_box);
    float n = 9.0f * u + 2 * fr, s = (float)cs(), x, y;
    ui_put(t_board, bx - fr, by - fr, n, n);
    float k1 = 1, k2 = 1;                       /* the mark set down, the board it claimed: how far along */
    if (anim.on) {
        float ms = (float)(ui_ms() - anim.t0);
        k1 = ms / POP_MS;
        k2 = anim.claim ? (ms - POP_MS) / CLAIM_MS : 1;
        if (k1 >= 1 && k2 >= 1) anim.on = 0;
        else ui_animate();
    }
    int st = status(), glow = G.line && !anim.on;
    if (G.ultimate && st == GS_PLAY) {          /* where the player must play: a soft light */
        int t = tt_target(&G);
        for (int b = 0; b < 9; b++) {
            if (G.board[b] != TT_OPEN || (t >= 0 && b != t)) continue;
            board_xy(b, &x, &y);
            float m = 0.1f * u;
            ui_fill(x + m, y + m, 3.0f * u - 2 * m, 3.0f * u - 2 * m, LIGHT[0], LIGHT[1], LIGHT[2], LIGHT[3]);
        }
    }
    if (G.n) {
        cell_xy(G.move[G.n - 1], &x, &y);
        ui_mark_last(x - s / 2, y - s / 2, s, s);
    }
    if (glow && !G.ultimate)                    /* the winning line(s): a gold light under the marks */
        for (int c = 0; c < 9; c++)
            if (G.line >> c & 1) {
                cell_xy(c, &x, &y);
                ui_mark_gold(x, y, 1.25f * s);
            }
    for (int c = 0; c < tt_cells(&G); c++) {
        if (G.cell[c] < 0) continue;
        cell_xy(c, &x, &y);
        if (anim.on && c == anim.cell) pop(t_mark[G.cell[c]], x, y, s, k1);
        else ui_put_c(t_mark[G.cell[c]], x, y, s, s);
    }
    for (int b = 0; G.ultimate && b < 9; b++) { /* the decided boards: veiled, their owner's big mark over them */
        int who = G.board[b];
        if (who == TT_OPEN) continue;
        float k = anim.on && anim.claim && b == anim.cell / 9 ? k2 : 1;
        if (k <= 0) continue;
        board_xy(b, &x, &y);
        float m = 0.08f * u, w = 3.0f * u;
        float veil = (who == TT_FULL ? 0.7f : 1) * VEIL[3] * ease(k);   /* nobody's: a lighter veil */
        ui_fill(x + m, y + m, w - 2 * m, w - 2 * m, VEIL[0], VEIL[1], VEIL[2], (Uint8)veil);
        if (glow && G.line >> b & 1) ui_mark_gold(x + w / 2, y + w / 2, 1.4f * w);
        if (who != TT_FULL) {
            if (k < 1) pop(t_big[who], x + w / 2, y + w / 2, w, k);
            else ui_put_c(t_big[who], x + w / 2, y + w / 2, w, w);
        }
    }
    for (int i = 0; glow && i < 8; i++) {       /* a gold stroke through each winning line */
        const int8_t *l = tt_lines[i];
        if ((G.line >> l[0] & G.line >> l[1] & G.line >> l[2] & 1) == 0) continue;
        float x0, y0, x1, y1;
        if (G.ultimate) {
            board_xy(l[0], &x0, &y0);
            board_xy(l[2], &x1, &y1);
        } else {
            cell_xy(l[0], &x0, &y0);
            cell_xy(l[2], &x1, &y1);
        }
        int d = l[0] % 3 != l[2] % 3 && l[0] / 3 != l[2] / 3;
        double deg = atan2(y1 - y0, x1 - x0) * 180 / 3.14159265358979;
        float mid = G.ultimate ? 1.5f * u : 0;  /* the boards: from their corners to their middles */
        ui_put_rot(t_strike[d], (x0 + x1) / 2 + mid, (y0 + y1) / 2 + mid,
                   (float)strike_px[d][0], (float)strike_px[d][1], deg);
    }
}

/* ---- the pointer: a tap on a square ------------------------------------------------------------------------------ */
/* a mark on cell c (legal), the claimed board's chime */
static void place(int c)
{
    tt_play(&G, c);
    anim.on = 1;
    anim.cell = c;
    anim.claim = G.ultimate && G.board[c / 9] != TT_OPEN;
    anim.t0 = ui_ms();
    ui_animate();
    ui_sound(SND_PLACE);
    if (anim.claim && G.board[c / 9] != TT_FULL && status() == GS_PLAY) ui_sound(SND_CHECK);   /* a board: a chime */
}

static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    int c = xy_cell(x, y);
    if (c < 0) return;
    if (!tt_legal(&G, c)) { ui_sound(SND_DENY); return; }
    place(c);
}

/* the bot's side (bots.h): the game, read-only; a move in the notation, played as a tap */
const tt_game *tictactoe_live(void) { return &G; }

int tictactoe_show(const char *move)
{
    tt_game t = G;                              /* the notation read by the rules, on a copy: the cell it marks */
    if (status() != GS_PLAY || !tt_script(&t, move) || t.n != G.n + 1) return 0;
    place(t.move[G.n]);
    return 1;
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* Ultimate: the small boards the player has claimed */
static void bar(int player, box b)
{
    if (!G.ultimate) return;
    float h = 0.46f * ui.unit;
    ui_number(b.x + 0.4f * ui.unit, b.y + b.h / 2, h, tt_claimed(&G, player), 1);
}

static void token(int player, int size, uint8_t *rgba) { mark_pic(player, size, 0, rgba); }

/* paints a mark (or a big one) over a picture w pixels wide, centred on (cx, cy) */
static void thumb_mark(uint8_t *o, int w, int player, int size, float cx, float cy)
{
    uint8_t *one = malloc((size_t)size * size * 4);
    if (!one) return;
    mark_pic(player, size, 1, one);
    art_blend(o, w, w, one, size, size, (int)lroundf(cx - size / 2.0f), (int)lroundf(cy - size / 2.0f));
    free(one);
}

static void thumb_fill(uint8_t *o, int w, float x, float y, float s, const uint8_t c[4])
{
    int n = (int)s;
    uint8_t *one = malloc((size_t)n * n * 4);
    if (!one) return;
    art_roundrect(n, n, 0.04f * n, c, one);
    art_blend(o, w, w, one, n, n, (int)x, (int)y);
    free(one);
}

/* the menu's card and classic's: a game won on the diagonal; Ultimate's: a game under way, two boards claimed */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int s = (int)(size / (9 + 2 * FRAME)), f = (int)(s * FRAME), n = 9 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = malloc((size_t)n * n * 4);
    if (!board) return;
    paint_board(variant == 1, s, f, board);
    if (variant != 1) {
        static const int8_t at[] = {0, 4, 8, 2, 3};
        int g = (int)(3.8f * s);
        uint8_t *gold = malloc((size_t)g * g * 4);
        if (gold) {
            art_disc(g, GOLD, 0.75f, 1.3f, gold);
            for (int i = 0; i < 3; i++)
                art_blend(board, n, n, gold, g, g, f + (at[i] % 3 * 3 + 1) * s + s / 2 - g / 2,
                          f + (at[i] / 3 * 3 + 1) * s + s / 2 - g / 2);
            free(gold);
        }
        for (int i = 0; i < 5; i++)
            thumb_mark(board, n, i >= 3, 3 * s, f + (at[i] % 3 * 3 + 1.5f) * s, f + (at[i] / 3 * 3 + 1.5f) * s);
        uint8_t *one = malloc((size_t)n * n * 4);
        art_fig *fig = art_fig_new();
        if (one && fig) {                       /* the strike through the diagonal */
            art_style st = STRIKE;
            float w = STRIKE_W * 3 * s, e = (1.5f - STRIKE_OVER * 3 * 0.7071f / 2) * s + w / 2 * 0.7071f;
            st.line = fmaxf(1, 0.07f * w);
            strike(fig, f + e, f + e, n - f - e, n - f - e, w - 2 * st.line);
            art_render(fig, &st, n, n, 1, 0, 0, one);
            art_blend(board, n, n, one, n, n, 0, 0);
        }
        free(one);
        art_fig_free(fig);
    } else {

        static const int8_t marks[][2] = {{4, 0}, {4, 2}, {2, 4}, {6, 4}, {0, 0}, {8, 8}, {3, 4}, {5, 4}, {4, 7},
                                          {7, 1}, {1, 6}, {8, 2}};   /* board, square: ivory first, alternating */
        thumb_fill(board, n, f + 6.1f * s, f + 0.1f * s, 2.8f * s, LIGHT);   /* sent to board 3 */
        for (unsigned i = 0; i < sizeof marks / sizeof marks[0]; i++) {
            int b = marks[i][0], q = marks[i][1];
            thumb_mark(board, n, i & 1, s, f + (b % 3 * 3 + q % 3 + 0.5f) * s, f + (b / 3 * 3 + q / 3 + 0.5f) * s);
        }
        thumb_fill(board, n, f + 0.08f * s, f + 3.08f * s, 2.84f * s, VEIL);   /* board 4: ivory's */
        thumb_mark(board, n, 0, 3 * s, f + 1.5f * s, f + 4.5f * s);
        thumb_fill(board, n, f + 6.08f * s, f + 6.08f * s, 2.84f * s, VEIL);   /* board 9: ebony's */
        thumb_mark(board, n, 1, 3 * s, f + 7.5f * s, f + 7.5f * s);
    }
    art_blend(rgba, size, size, board, n, n, o, o);
    free(board);
}

static int busy(void) { return anim.on; }     /* the end waits for the last mark (and the board it claims) */

static const game tictactoe_game = {
    .id = "tictactoe", .order = 110, .variants = 2,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(tictactoe_game)
