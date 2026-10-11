/*
 * Go in RetroStone Board: the board (9 x 9, 13 x 13, 19 x 19), the stones, the counting (the rules: go_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A tap shows a ghost stone on the nearest point (19 x 19 points are small on a phone), a second tap on it plays it;
 * a drag moves the ghost, its release does not play. Two passes: the counting. Each point shows its owner (a small
 * square), a tap on a group marks it dead (faded, its points the other player's) or alive again; the pass button then
 * means done. The script's moves: "d4 q16 pass" (go.h), a pass while counting is done.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/go.h"
#include "shell.h"

#define FRAME 0.035f                            /* the board's frame, a part of its side */
#define FADE_MS 260                             /* captured stones fade out */
#define GHOST_A 150

static gogame G;
static int n = 19, sq, fr, made_sq = -1, made_n = -1, made_u = -1;
static float ox, oy;                            /* the board's picture, its top left corner */
static SDL_Texture *t_board, *t_stone[2], *t_ring, *t_small[2], *t_half;
static struct { int on, pt, down, armed, moved; } ghost;
static struct { int on, k, pt[GO_PTS]; uint8_t who[GO_PTS]; Uint32 t0; } fade;

static const uint8_t INK[3] = {34, 24, 16}, WHITE[3] = {255, 255, 255};

static int art_of(int player) { return player ? 0 : 1; }   /* player 0 plays ebony: the house's second colour */

/* ---- the position ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    ghost.on = ghost.down = 0;
    fade.on = 0;
}

static void start(int variant)
{
    n = variant == 0 ? 9 : variant == 1 ? 13 : 19;
    go_new(&G, n);
    calm();
}

static int status(void)
{
    if (G.phase != GO_OVER) return GS_PLAY;
    return go_winner(&G) == 0 ? GS_WIN0 : GS_WIN1;
}

static int turn(void) { return go_now(&G)->side; }

static int legal(int pt)
{
    gopos next;
    return go_try(&G, pt, &next) >= 0;
}

/* a stone on pt: the stones it takes fade out */
static void play(int pt)
{
    gopos before = *go_now(&G);
    int took = go_play(&G, pt);
    if (took < 0) { ui_sound(SND_DENY); return; }
    ghost.on = 0;
    fade.on = took > 0;
    fade.k = 0;
    fade.t0 = ui_ms();
    for (int p = 0; p < n * n; p++)
        if (before.pt[p] != GO_EMPTY && go_now(&G)->pt[p] == GO_EMPTY) {
            fade.pt[fade.k] = p;
            fade.who[fade.k++] = (uint8_t)(before.pt[p] - 1);
        }
    ui_sound(took ? SND_CAPTURE : SND_PLACE);
}

static int can_pass(void) { return G.phase != GO_OVER; }

static void pass(void)
{
    go_pass(&G);
    calm();
}

static int undo(void)
{
    calm();
    return go_undo(&G);
}

static int script(const char *s)
{
    calm();
    return go_script(&G, s);
}

/* ---- pictures --------------------------------------------------------------------------------------------------------- */
/* coverage of the lines and star points (0..1 a pixel, the most of what covers it), then inked in once */
static void cover_rect(float *m, int w, float x0, float y0, float x1, float y1)
{
    for (int y = (int)floorf(y0); y < (int)ceilf(y1); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x1); x++) {
            if (x < 0 || y < 0 || x >= w || y >= w) continue;
            float cx = fminf(x + 1, x1) - fmaxf(x, x0), cy = fminf(y + 1, y1) - fmaxf(y, y0), c = cx * cy;
            if (c > m[y * w + x]) m[y * w + x] = c;
        }
}

static void cover_disc(float *m, int w, float cx, float cy, float r)
{
    for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
        for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
            if (x < 0 || y < 0 || x >= w || y >= w) continue;
            float d = hypotf(x + 0.5f - cx, y + 0.5f - cy), c = fminf(fmaxf(r - d + 0.5f, 0), 1);
            if (c > m[y * w + x]) m[y * w + x] = c;
        }
}

/* the star points: the corners' and the middle's (9 x 9, 13 x 13), nine on 19 x 19 */
static int hoshi(int size, int xy[9][2])
{
    int a = size == 9 ? 2 : 3, b = size - 1 - a, c = size / 2, k = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            int x = i == 0 ? a : i == 1 ? c : b, y = j == 0 ? a : j == 1 ? c : b;
            if (size == 19 || (i != 1 && j != 1) || (i == 1 && j == 1)) { xy[k][0] = x; xy[k][1] = y; k++; }
        }
    return k;
}

/* the board: the plank in its frame (size x size points of s pixels, frame f), the lines, the star points; corner:
 * only the top left corner of a board, its lines running on to the frame (the menu's card) */
static void paint_board(uint8_t *o, int size, int s, int f, int corner)
{
    int w = size * s + 2 * f;
    art_board_n(size, size, s, f, 0, o);
    float *m = calloc((size_t)w * w, sizeof *m);
    if (!m) return;
    float lw = 0.9f + 0.02f * s, edge = 1.8f * lw, c0 = f + s / 2.0f, c1 = c0 + (size - 1) * s;
    float end = corner ? (float)(f + size * s) : c1 + edge / 2;
    for (int i = 0; i < size; i++) {
        float c = c0 + i * s, t = (i == 0 || (i == size - 1 && !corner)) ? edge : lw;
        cover_rect(m, w, c - t / 2, c0 - edge / 2, c + t / 2, end);
        cover_rect(m, w, c0 - edge / 2, c - t / 2, end, c + t / 2);
    }
    int xy[9][2], k = corner ? 1 : hoshi(size, xy);
    if (corner) xy[0][0] = xy[0][1] = 3;
    for (int i = 0; i < k; i++) cover_disc(m, w, c0 + xy[i][0] * s, c0 + xy[i][1] * s, fmaxf(1.6f * lw, 0.11f * s));
    for (int i = 0; i < w * w; i++) {
        float a = 0.9f * m[i];
        for (int j = 0; j < 3; j++) o[4 * i + j] = (uint8_t)(o[4 * i + j] * (1 - a) + INK[j] * a + 0.5f);
    }
    free(m);
}

static SDL_Texture *stone_tex(int player, int size)
{
    art_counter(COUNTER_STONE, art_of(player), size, ui_buf(size, size));
    return ui_tex(size, size, ui_buf(size, size));
}

/* the komi's half point: a ring, its left half filled */
static void paint_half(int size, uint8_t *o)
{
    float r = size * 0.5f, lw = fmaxf(1.2f, size * 0.14f);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = x + 0.5f - r, dy = y + 0.5f - r, d = hypotf(dx, dy);
            float disc = fminf(fmaxf(r - d, 0), 1), ring = disc * fminf(fmaxf(d - (r - lw), 0), 1);
            float half = disc * fminf(fmaxf(-dx + 0.5f, 0), 1);
            uint8_t *p = o + 4 * (y * size + x);
            memcpy(p, WHITE, 3);
            p[3] = (uint8_t)(255 * fmaxf(ring, half));
        }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------------ */
static void layout(box b)
{
    sq = (int)(b.w * (1 - 2 * FRAME) / n);
    sq -= !(sq & 1);                            /* odd: the points on pixel centres, the lines crisp */
    fr = (int)((b.w - n * sq) / 2);             /* the frame takes what is left: the board fills its square */
    float bw = (float)(n * sq + 2 * fr);
    ox = floorf(b.x + (b.w - bw) / 2);
    oy = floorf(b.y + (b.h - bw) / 2);
    if (made_sq != sq || made_n != n) {
        made_sq = sq;
        made_n = n;
        int w = n * sq + 2 * fr;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), n, sq, fr, 0);
        t_board = ui_tex(w, w, ui_buf(w, w));
        for (int p = 0; p < 2; p++) {
            ui_free(&t_stone[p]);
            t_stone[p] = stone_tex(p, sq);
        }
        int rs = (int)fmaxf(8, 0.46f * sq);
        ui_free(&t_ring);
        art_ring(rs, 0.70f, WHITE, 1, ui_buf(rs, rs));
        t_ring = ui_tex(rs, rs, ui_buf(rs, rs));
    }
    int u = (int)ui.unit;
    if (made_u != u) {
        made_u = u;
        int s = (int)(0.5f * u), h = (int)(0.3f * u);
        for (int p = 0; p < 2; p++) {
            ui_free(&t_small[p]);
            t_small[p] = stone_tex(p, s);
        }
        ui_free(&t_half);
        paint_half(h, ui_buf(h, h));
        t_half = ui_tex(h, h, ui_buf(h, h));
        SDL_SetTextureColorMod(t_half, 246, 238, 220);
    }
}

static void pt_xy(int pt, float *x, float *y)
{
    int c = pt % n, r = n - 1 - pt / n;         /* r: rows from the top */
    if (ui.flipped) { c = n - 1 - c; r = n - 1 - r; }
    *x = ox + fr + c * sq + sq / 2.0f;
    *y = oy + fr + r * sq + sq / 2.0f;
}

/* the nearest point, or -1 off the board */
static int xy_pt(float x, float y)
{
    int c = (int)floorf((x - ox - fr) / sq), r = (int)floorf((y - oy - fr) / sq);
    if (c < 0 || r < 0 || c >= n || r >= n) return -1;
    if (ui.flipped) { c = n - 1 - c; r = n - 1 - r; }
    return c + (n - 1 - r) * n;
}

static void stone_at(int player, float cx, float cy, int alpha)
{
    SDL_Texture *t = t_stone[player];
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    ui_put_c(t, cx, cy, (float)sq, (float)sq);
    SDL_SetTextureAlphaMod(t, 255);
}

/* a point's owner while counting: a small square of the player's colour (ivory outlined on the wood, ebony on the bar) */
static void owner_square(int player, float cx, float cy, float s, int on_wood)
{
    float e = fmaxf(1, roundf(0.12f * s));
    if (player == 1 && on_wood) ui_fill(cx - s / 2 - e, cy - s / 2 - e, s + 2 * e, s + 2 * e, 52, 40, 32, 255);
    if (player == 0 && !on_wood) ui_fill(cx - s / 2 - e, cy - s / 2 - e, s + 2 * e, s + 2 * e, 214, 204, 188, 255);
    if (player) ui_fill(cx - s / 2, cy - s / 2, s, s, 242, 234, 216, 255);
    else ui_fill(cx - s / 2, cy - s / 2, s, s, 40, 36, 34, 255);
}

static void draw(void)
{
    const gopos *p = go_now(&G);
    float bw = (float)(n * sq + 2 * fr), x, y;
    ui_put(t_board, ox, oy, bw, bw);
    int counting = G.phase != GO_PLAY;
    int8_t owner[GO_PTS];
    float score[2];
    if (counting) go_score(&G, owner, score);
    for (int pt = 0; pt < n * n; pt++) {        /* the shadows first: a stone's shadow falls on the wood only */
        if (p->pt[pt] == GO_EMPTY || (counting && G.dead[pt])) continue;
        pt_xy(pt, &x, &y);
        ui_shadow_flat(x, y, (float)sq, (float)sq, 1);
    }
    for (int pt = 0; pt < n * n; pt++) {
        if (p->pt[pt] == GO_EMPTY) continue;
        pt_xy(pt, &x, &y);
        stone_at(p->pt[pt] - 1, x, y, counting && G.dead[pt] ? 100 : 255);
    }
    if (fade.on) {
        float t = (ui_ms() - fade.t0) / (float)FADE_MS;
        if (t >= 1) fade.on = 0;
        else {
            ui_animate();
            for (int i = 0; i < fade.k; i++) {
                pt_xy(fade.pt[i], &x, &y);
                stone_at(fade.who[i], x, y, (int)(255 * (1 - t) * (1 - t)));
            }
        }
    }
    int last = go_last(&G);
    if (last >= 0 && p->pt[last] != GO_EMPTY && !(counting && G.dead[last])) {   /* the last stone: a small ring */
        pt_xy(last, &x, &y);
        if (p->pt[last] == GO_EBONY) SDL_SetTextureColorMod(t_ring, 236, 228, 212);
        else SDL_SetTextureColorMod(t_ring, 58, 50, 44);
        float rs = fmaxf(8, 0.46f * sq);
        ui_put_c(t_ring, x, y, rs, rs);
    }
    if (counting)
        for (int pt = 0; pt < n * n; pt++) {
            if (owner[pt] < 0 || (p->pt[pt] != GO_EMPTY && !G.dead[pt])) continue;
            pt_xy(pt, &x, &y);
            owner_square(owner[pt], x, y, roundf(0.22f * sq), 1);
        }
    if (G.phase == GO_PLAY && ghost.on) {
        pt_xy(ghost.pt, &x, &y);
        stone_at(p->side, x, y, GHOST_A);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------------- */
/* the ghost to pt if a stone may go there (else none: loud, the refusal's sound) */
static void aim(int pt, int loud)
{
    ghost.on = pt >= 0 && legal(pt);
    ghost.pt = pt;
    if (!ghost.on && loud) ui_sound(SND_DENY);
}

static void press(float x, float y)
{
    int pt = xy_pt(x, y);
    if (G.phase == GO_COUNT) {                  /* a group dead / alive */
        ui_sound(go_toggle(&G, pt) ? SND_TAP : SND_DENY);
        return;
    }
    if (G.phase != GO_PLAY) return;
    if (pt < 0) { ghost.on = 0; return; }
    ghost.down = 1;
    ghost.moved = 0;
    ghost.armed = ghost.on && ghost.pt == pt;   /* the second tap on the ghost plays it */
    if (!ghost.armed) aim(pt, 1);
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ghost.down) return;
    int pt = xy_pt(x, y);
    if (pt < 0 || pt == ghost.pt) return;
    ghost.moved = 1;
    aim(pt, 0);
    ui_animate();
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!ghost.down) return;
    ghost.down = 0;
    if (ghost.armed && !ghost.moved && ghost.on) play(ghost.pt);
    else if (ghost.moved && !ghost.on) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ------------------------------------------------------------------------------------------------ */
/* playing: the stones the player took (a stone of the other colour, the count); counting: the player's score (a square
 * of the player's colour, the points; ivory's half point of the komi) */
static void bar(int player, box b)
{
    float u = ui.unit, cy = b.y + b.h / 2, x = b.x, h = 0.5f * u;
    int value;
    if (G.phase == GO_PLAY) {
        value = go_now(&G)->caps[player];
        if (!value) return;
        ui_put(t_small[!player], x, cy - h / 2, h, h);
        x += h + 0.18f * u;
    } else {
        int8_t owner[GO_PTS];
        float score[2];
        go_score(&G, owner, score);
        value = (int)score[player];
        owner_square(player, x + 0.17f * u, cy, roundf(0.3f * u), 0);
        x += 0.52f * u;
    }
    int digits = value >= 100 ? 3 : value >= 10 ? 2 : 1;
    float w = 0.74f * h * (digits - 1) + 0.62f * h;
    ui_number(x + w / 2, cy, h, value, 1);
    if (G.phase != GO_PLAY && player == 1) {
        float s = 0.3f * u;
        ui_put(t_half, x + w + 0.12f * u, cy - s / 2, s, s);
    }
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_STONE, art_of(player), size, rgba); }

/* the menu's card: a board's corner, a few stones; the picker's: the whole board of each size */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int corner = variant < 0, nn = corner ? 5 : variant == 0 ? 9 : variant == 1 ? 13 : 19;
    int f = (int)(size * (corner ? 0.06f : 0.04f)), s = (size - 2 * f) / nn, w = nn * s + 2 * f, o = (size - w) / 2;
    uint8_t *board = ui_buf(w, w);
    paint_board(board, nn, s, f, corner);
    art_blend(rgba, size, size, board, w, w, o, o);
    static const int corner_st[][3] = {{2, 2, 0}, {3, 1, 0}, {1, 3, 0}, {2, 1, 1}, {1, 2, 1}, {4, 3, 1}};
    static const float board_st[][3] = {{0.17f, 0.17f, 0}, {0.83f, 0.83f, 1}, {0.83f, 0.17f, 0}, {0.17f, 0.75f, 1},
                                        {0.5f, 0.5f, 0}, {0.75f, 0.42f, 1}};
    int k = corner ? (int)(sizeof corner_st / sizeof corner_st[0]) : (int)(sizeof board_st / sizeof board_st[0]);
    for (int i = 0; i < k; i++) {
        int c = corner ? corner_st[i][0] : (int)lroundf(board_st[i][0] * (nn - 1));
        int r = corner ? corner_st[i][1] : (int)lroundf(board_st[i][1] * (nn - 1));
        int who = corner ? corner_st[i][2] : (int)board_st[i][2];
        uint8_t *one = ui_buf(s, s);
        art_counter(COUNTER_STONE, art_of(who), s, one);
        art_blend(rgba, size, size, one, s, s, o + f + c * s, o + f + r * s);
    }
}

static const game go_game = {
    .id = "go", .order = 60, .variants = 3,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script,
};
REGISTER_GAME(go_game)
