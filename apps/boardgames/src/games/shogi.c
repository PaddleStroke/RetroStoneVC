/*
 * Shogi in RetroStone Board: the board (9 x 9, or mini shogi's 5 x 5), the wooden wedges and their characters, tap or
 * drag, the drops from the pieces in hand (in the players' bars), the promotion picker (the rules: shogi_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A piece points at the opponent: the top player's are turned round, face to face or not (a shogi piece already faces
 * its owner), drawn again rather than rotated so the light stays on their upper left. The characters: 歩 香 桂 銀 金
 * 角 飛, the kings 玉 (sente) and 王 (gote); the promoted pieces in red ink: と 杏 圭 全 馬 龍. A tap on a piece in
 * hand chooses it for a drop (or drag it onto the board). The script's moves: USI, "7g7f 3c3d 8h2b+ P*5e" (shogi.h).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/grid.h"
#include "games/shogi.h"
#include "shell.h"
#include "text.h"

#define FRAME 0.30f                             /* the board's frame, in squares */
#define ANIM_MS 150
#define DROP_MS 170
#define KING1 (SG_KING | SG_PROMOTED)           /* the textures' slot for gote's king (王): no king promotes */

static sgame G;
static smove legal[SG_MAX_MOVES];
static int nlegal;
static sstatus st;
static int N = 9;                               /* the board's side: 9, or 5 (mini shogi) */
static int sq, fr, made = -1, made_small = -1;
static float bx, by;                            /* the top left square's corner on the screen */
static SDL_Texture *t_pc[2][32], *t_small[2][32], *t_board, *t_pick;   /* [pointing down][face] */
static int sel = -1, hand_sel;                  /* the selected square, or the kind chosen in hand */
static box hand_at[2][8];                       /* the pieces in hand, in the frame each bar was drawn in */
static struct { int on, from, kind, moved; float x, y; } drag;   /* from a square, or (kind) from the hand */
static struct { int on; smove m; Uint32 t0; } anim;
static struct { int on, from, to; } promo;

/* ---- the pieces -------------------------------------------------------------------------------------------------- */
static int tex_of(int pc) { return SG_KIND(pc) == SG_KING && SG_OWNER(pc) ? KING1 : SG_FACE(pc); }

static int glyph_of(int ix)
{
    switch (ix) {
    case SG_PAWN: return 0x6B69;                /* 歩 */
    case SG_LANCE: return 0x9999;               /* 香 */
    case SG_KNIGHT: return 0x6842;              /* 桂 */
    case SG_SILVER: return 0x9280;              /* 銀 */
    case SG_GOLD: return 0x91D1;                /* 金 */
    case SG_BISHOP: return 0x89D2;              /* 角 */
    case SG_ROOK: return 0x98DB;                /* 飛 */
    case SG_KING: return 0x7389;                /* 玉 */
    case KING1: return 0x738B;                  /* 王 */
    case SG_PAWN | SG_PROMOTED: return 0x3068;  /* と */
    case SG_LANCE | SG_PROMOTED: return 0x674F; /* 杏 */
    case SG_KNIGHT | SG_PROMOTED: return 0x572D;    /* 圭 */
    case SG_SILVER | SG_PROMOTED: return 0x5168;    /* 全 */
    case SG_BISHOP | SG_PROMOTED: return 0x99AC;    /* 馬 */
    case SG_ROOK | SG_PROMOTED: return 0x9F8D;  /* 龍 */
    default: return 0;
    }
}

/* the wedge's width (a part of the square) by rank: the king the largest, the pawn the smallest */
static float width_of(int kind)
{
    static const float w[9] = {0, 0.66f, 0.70f, 0.72f, 0.75f, 0.75f, 0.78f, 0.78f, 0.81f};
    return w[kind];
}

/* a piece, size x size, pointing up (or down): a pentagon of light wood, a bevel, its character in ink */
static void make_piece(int ix, int down, int size, uint8_t *rgba)
{
    static const art_style wood = {{238, 210, 152}, {70, 46, 24}, {188, 150, 96}, {0, 0, 0}, 0.30f, 0};
    static const uint8_t black[3] = {26, 18, 12}, red[3] = {184, 30, 24};
    art_style s = wood;
    s.line = fmaxf(0.018f, 1.1f / size);
    float w = width_of(SG_KIND(ix == KING1 ? SG_KING : ix)), h = w * 1.1f, y0 = 0.5f - h / 2, e = 0.05f * w;
    const float outer[] = {0.5f, y0, 0.5f + 0.40f * w, y0 + 0.24f * h, 0.5f + 0.5f * w, y0 + h,
                           0.5f - 0.5f * w, y0 + h, 0.5f - 0.40f * w, y0 + 0.24f * h};
    const float inner[] = {0.5f, y0 + 1.6f * e, 0.5f + 0.40f * w - e, y0 + 0.24f * h + 0.6f * e,
                           0.5f + 0.5f * w - e, y0 + h - e, 0.5f - 0.5f * w + e, y0 + h - e,
                           0.5f - 0.40f * w + e, y0 + 0.24f * h + 0.6f * e};
    float pts[2][10];
    for (int i = 0; i < 10; i++) {              /* turned round: the light stays on the upper left */
        pts[0][i] = down ? 1 - outer[i] : outer[i];
        pts[1][i] = down ? 1 - inner[i] : inner[i];
    }
    art_fig *f = art_fig_new();
    art_poly(f, ART_PART, pts[0], 5);
    art_poly(f, ART_PART, pts[1], 5);
    art_render(f, &s, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
    uint8_t *g = calloc((size_t)size * size, 4);
    if (!g) return;
    int promoted = ix != KING1 && (ix & SG_PROMOTED);
    if (text_glyph(glyph_of(ix), 0.62f * w * size, 1, promoted ? red : black, g, size, size)) {
        if (down)                               /* the character turned round with the piece */
            for (int i = 0, j = size * size - 1; i < j; i++, j--) {
                uint32_t t;
                memcpy(&t, g + 4 * i, 4);
                memcpy(g + 4 * i, g + 4 * j, 4);
                memcpy(g + 4 * j, &t, 4);
            }
        int dy = (int)lroundf((y0 + 0.58f * h - 0.5f) * size);
        art_blend(rgba, size, size, g, size, size, 0, down ? -dy : dy);
    }
    free(g);
}

static void make_set(SDL_Texture *t[2][32], int size)
{
    for (int d = 0; d < 2; d++)
        for (int ix = 1; ix < 32; ix++) {
            ui_free(&t[d][ix]);
            if (!glyph_of(ix)) continue;
            make_piece(ix, d, size, ui_buf(size, size));
            t[d][ix] = ui_tex(size, size, ui_buf(size, size));
        }
}

/* the board: the plank in its frame, n x n squares of s pixels inside a margin, the lines, the four star points */
static int paint_board(uint8_t *o, int n, int s, int f, int star)
{
    static const uint8_t INK[3] = {40, 28, 18};
    /* the margin round the lines: the plank's n squares (art_board_n) are a little larger than the board's */
    int k = s < 75 ? 1 : (int)(0.02f * s + 0.5f), m = n * k, w = n * (s + 2 * k) + 2 * f;
    if (!o) return w;
    art_board_n(n, n, s + 2 * k, f, 0, o);
    float *cov = calloc((size_t)w * w, sizeof *cov);
    if (!cov) return w;
    float lw = 0.8f + 0.018f * s, c0 = (float)(f + m);
    grid_lines(cov, w, c0, c0, n, n, s, lw, 1.6f * lw, 0.8f * lw, 0);
    for (int i = 0; i < 4 && star; i++)
        grid_cover_disc(cov, w, c0 + (i & 1 ? 6 : 3) * s, c0 + (i & 2 ? 6 : 3) * s, fmaxf(1.5f * lw, 0.055f * s));
    grid_ink(o, w, w, cov, w, INK, 0.88f);
    free(cov);
    return w;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = sg_legal(sg_pos(&G), legal);
    st = sg_status(&G);
    sel = -1;
    hand_sel = 0;
    promo.on = 0;
    drag.on = 0;
}

static void start(int variant)
{
    sg_new(&G, variant);
    N = sg_pos(&G)->n;
    anim.on = 0;
    refresh();
}

static int status(void)
{
    return st == SG_WON0 ? GS_WIN0 : st == SG_WON1 ? GS_WIN1 : st == SG_DRAW ? GS_DRAW : GS_PLAY;
}

static int turn(void) { return sg_pos(&G)->side; }

/* the legal move from a square (or a drop of a kind) to a square, promoting or not; -1: none */
static int find(int from, int kind, int to, int promote)
{
    for (int i = 0; i < nlegal; i++) {
        smove m = legal[i];
        if (m.to == to && m.drop == kind && (kind || (m.from == from && m.promo == promote))) return i;
    }
    return -1;
}

static int can_go(int from, int to) { return find(from, 0, to, 0) >= 0 || find(from, 0, to, 1) >= 0; }

static void play(smove m, int animate)
{
    int capture = !m.drop && sg_pos(&G)->sq[m.to];
    sg_play(&G, m);
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(capture ? SND_CAPTURE : m.drop ? SND_PLACE : SND_MOVE);
    if (st == SG_PLAY && sg_in_check(sg_pos(&G), sg_pos(&G)->side)) ui_sound(SND_CHECK);
}

/* a move between two squares: when the piece may promote or not, the picker asks first */
static void move_to(int from, int to, int animate)
{
    int yes = find(from, 0, to, 1), no = find(from, 0, to, 0);
    if (yes >= 0 && no >= 0) {
        promo.on = 1;
        promo.from = from;
        promo.to = to;
        return;
    }
    if (yes >= 0 || no >= 0) play(legal[yes >= 0 ? yes : no], animate);
}

static int undo(void)
{
    if (!sg_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "7g7f 3c3d 8h2b+ P*5e": the screenshots' and the tests' moves */
static int script(const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int used, k = sg_parse(N, legal, nlegal, s, &used);
        if (k < 0) return 0;
        play(legal[k], 0);
        s += used;
        while (*s && *s != ' ') s++;
    }
    return 1;
}

/* ---- the bot (shogi_bot.c) --------------------------------------------------------------------------------------- */
const sgame *shogi_live(void) { return &G; }

/* one move in USI ("7g7f", "8h2b+", "P*5e"), played as if tapped: it slides (no promotion picker) */
int shogi_show(const char *move)
{
    int used = 0, k = move && st == SG_PLAY ? sg_parse(N, legal, nlegal, move, &used) : -1;
    if (k < 0 || move[used]) return 0;
    play(legal[k], 1);
    return 1;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    int s = (int)floorf(b.w / (N + 2 * FRAME + 0.4f));
    while (s > 4 && paint_board(NULL, N, s, (int)(s * FRAME), N == 9) > b.w) s--;
    sq = s;
    fr = (int)(sq * FRAME);
    int n = paint_board(NULL, N, sq, fr, N == 9), m = (n - N * sq) / 2;
    bx = floorf(b.x + (b.w - n) / 2) + m;
    by = floorf(b.y + (b.h - n) / 2) + m;
    if (made != sq * 10 + N) {           /* the pieces for the squares, the board for its side too */
        made = sq * 10 + N;
        make_set(t_pc, sq);
        ui_free(&t_board);
        paint_board(ui_buf(n, n), N, sq, fr, N == 9);
        t_board = ui_tex(n, n, ui_buf(n, n));
        const uint8_t pick_c[4] = {246, 240, 226, 255};
        ui_free(&t_pick);
        art_roundrect(sq, 2 * sq, 0.22f * sq, pick_c, ui_buf(sq, 2 * sq));
        t_pick = ui_tex(sq, 2 * sq, ui_buf(sq, 2 * sq));
    }
    int small = (int)(0.74f * ui.unit);
    if (made_small != small) {
        made_small = small;
        make_set(t_small, small);
    }
}

static void sq_xy(int s, float *x, float *y)
{
    int c = SG_COL(s), r = SG_ROW(s);
    if (ui.flipped) { c = N - 1 - c; r = N - 1 - r; }
    *x = bx + c * sq;
    *y = by + r * sq;
}

static int xy_sq(float x, float y)
{
    int c = (int)floorf((x - bx) / sq), r = (int)floorf((y - by) / sq);
    if (c < 0 || c >= N || r < 0 || r >= N) return -1;
    return ui.flipped ? SG_SQ(N - 1 - c, N - 1 - r) : SG_SQ(c, r);
}

/* the picker: the promoted piece on the square, the piece as it is next, towards the middle of the board */
static int pick_at(int i, float *x, float *y)
{
    sq_xy(promo.to, x, y);
    int down = *y < by + (N - 1) / 2.0f * sq;
    *y += (down ? 1 : -1) * i * sq;
    return down;
}

/* a piece in the square at (x, y), lifted up pixels off the board (its shadow left lower, fainter, the piece a little
 * bigger) */
static void piece_up(int pc, float x, float y, float size, float up)
{
    int down = SG_OWNER(pc) != ui_bottom();
    float k = 1 + 0.6f * up / size, s = size * k;
    ui_shadow_flat(x + size / 2 + 0.4f * up, y + size / 2 + 0.6f * up, 0.86f * s * width_of(SG_KIND(pc)), 0.9f * s,
                   0.75f - 1.5f * up / size);
    ui_put(t_pc[down][tex_of(pc)], x + (size - s) / 2, y + (size - s) / 2 - up, s, s);
}

static void piece_at(int pc, float x, float y, float size) { piece_up(pc, x, y, size, 0); }

static void draw(void)
{
    const spos *p = sg_pos(&G);
    float n = (float)N * sq, m = (paint_board(NULL, N, sq, fr, N == 9) - n) / 2, x, y;
    ui_put(t_board, bx - m, by - m, n + 2 * m, n + 2 * m);
    if (G.ply) {
        smove lm = G.move[G.ply - 1];
        if (!lm.drop) {
            sq_xy(lm.from, &x, &y);
            ui_mark_last(x, y, sq, sq);
        }
        sq_xy(lm.to, &x, &y);
        ui_mark_last(x, y, sq, sq);
    }
    if (sel >= 0) {
        sq_xy(sel, &x, &y);
        ui_mark_select(x, y, sq, sq);
    }
    if (st != SG_DRAW && sg_in_check(p, p->side)) {
        sq_xy(sg_king(p, p->side), &x, &y);
        ui_mark_glow(x + sq / 2.0f, y + sq / 2.0f, 1.6f * sq);   /* round the wedge, which fills its square */
    }
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(anim.m.drop ? DROP_MS : ANIM_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    for (int s = 0; s < 81; s++) {
        if (!p->sq[s] || (drag.on && drag.moved && s == drag.from) || (anim.on && s == anim.m.to)) continue;
        sq_xy(s, &x, &y);
        piece_up(p->sq[s], x, y, (float)sq, s == sel && !(drag.on && drag.moved) ? 0.1f * sq : 0);   /* the selected: lifted */
    }
    if ((sel >= 0 || hand_sel) && !promo.on) {  /* where the selected piece can go */
        uint8_t done[81] = {0};
        for (int i = 0; i < nlegal; i++) {
            smove lm = legal[i];
            if (done[lm.to] || (hand_sel ? lm.drop != hand_sel : lm.drop || lm.from != sel)) continue;
            done[lm.to] = 1;
            sq_xy(lm.to, &x, &y);
            if (p->sq[lm.to]) ui_mark_ring(x + sq / 2.0f, y + sq / 2.0f, sq);
            else ui_mark_dot(x + sq / 2.0f, y + sq / 2.0f, 0.32f * sq);
        }
    }
    if (anim.on) {
        float x1, y1;
        sq_xy(anim.m.to, &x1, &y1);
        if (anim.m.drop) {                      /* a drop: set down from a little above the board */
            float k = 1 + 0.22f * (1 - t);
            piece_at(p->sq[anim.m.to], x1 + sq * (1 - k) / 2, y1 + sq * (1 - k) / 2, sq * k);
        } else {
            float x0, y0;
            sq_xy(anim.m.from, &x0, &y0);
            piece_up(p->sq[anim.m.to], x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, (float)sq,
                     0.16f * sq * sinf(3.14159265f * t));   /* a little hop: up, then down */
        }
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? -0.25f : 0.75f;
        piece_at(drag.kind ? SG_PC(p->side, drag.kind) : p->sq[drag.from], drag.x - s / 2, drag.y - s * off, s);
    }
    if (promo.on) {
        ui_fill(bx - m, by - m, n + 2 * m, n + 2 * m, 0, 0, 0, 110);
        float px, py;
        int down = pick_at(0, &px, &py), pc = p->sq[promo.from];
        ui_put(t_pick, px, down ? py : py - sq, (float)sq, 2.0f * sq);
        for (int i = 0; i < 2; i++) {
            pick_at(i, &px, &py);
            piece_at(i ? pc : pc | SG_PROMOTED, px, py, (float)sq);
        }
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (promo.on) {
        for (int i = 0; i < 2; i++) {
            float px, py;
            pick_at(i, &px, &py);
            if (x >= px && x < px + sq && y >= py && y < py + sq) {
                int k = find(promo.from, 0, promo.to, !i);
                if (k >= 0) play(legal[k], 0);
                return;
            }
        }
        promo.on = 0;
        return;
    }
    if (st != SG_PLAY) return;
    int s = xy_sq(x, y);
    if (s < 0) { sel = -1; hand_sel = 0; return; }
    if (hand_sel) {
        int k = find(0, hand_sel, s, 0);
        if (k >= 0) { play(legal[k], 1); return; }
    }
    if (sel >= 0 && can_go(sel, s)) {
        move_to(sel, s, 1);
        return;
    }
    const spos *p = sg_pos(&G);
    if (p->sq[s] && SG_OWNER(p->sq[s]) == p->side) {
        sel = s;
        hand_sel = 0;
        drag.on = 1;
        drag.from = s;
        drag.kind = drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    if (sel >= 0 || hand_sel) ui_sound(SND_DENY);
    sel = -1;
    hand_sel = 0;
}

/* a piece in hand: chosen for a drop (a second tap puts it back), or dragged onto the board */
static int bar_press(int player, float x, float y)
{
    if (st != SG_PLAY || promo.on) return 0;
    for (int k = SG_PAWN; k <= SG_ROOK; k++) {
        box b = hand_at[player][k];
        if (!b.w || x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
        if (player != turn()) { ui_sound(SND_DENY); return 1; }
        sel = -1;
        hand_sel = hand_sel == k ? 0 : k;
        if (hand_sel) { drag.on = 1; drag.from = -1; drag.kind = k; drag.moved = 0; }
        return 1;
    }
    return 0;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (drag.kind ? xy_sq(x, y) >= 0 : hypotf(x - drag.x, y - drag.y) > sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int s = xy_sq(x, y);
    if (s < 0) return;
    if (drag.kind) {
        int k = find(0, drag.kind, s, 0);
        if (k >= 0) play(legal[k], 0);
    } else if (s != drag.from && can_go(drag.from, s)) {
        move_to(drag.from, s, 0);
    }
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces in hand, by kind, a count beside each kind held more than once */
static void bar(int player, box b)
{
    static const int order[7] = {SG_ROOK, SG_BISHOP, SG_GOLD, SG_SILVER, SG_KNIGHT, SG_LANCE, SG_PAWN};
    const spos *p = sg_pos(&G);
    memset(hand_at[player], 0, sizeof hand_at[player]);
    int kinds = 0;
    for (int i = 0; i < 7; i++) kinds += p->hand[player][order[i]] > 0;
    if (!kinds) return;
    float cw = fminf(1.05f * ui.unit, b.w / kinds), ps = fminf(0.74f * ui.unit, cw / 1.4f), x = b.x;
    float cy = b.y + b.h / 2;
    int down = player != ui_bottom() && !ui_upside(player);   /* the top player's, as on the board */
    for (int i = 0; i < 7; i++) {
        int k = order[i], n = p->hand[player][k];
        if (!n) continue;
        hand_at[player][k] = (box){x, b.y, cw, b.h};
        if (hand_sel == k && player == p->side) ui_mark_select(x, cy - ps / 2, ps, ps);
        ui_shadow_flat(x + ps / 2, cy, 0.86f * ps * width_of(k), 0.9f * ps, 0.75f);
        ui_put(t_small[down][k], x, cy - ps / 2, ps, ps);
        if (n > 1) ui_number(x + 1.2f * ps, cy + 0.05f * ps, 0.5f * ps, n, 1);
        x += cw;
    }
}

static void token(int player, int size, uint8_t *rgba) { make_piece(player ? KING1 : SG_KING, 0, size, rgba); }

/* the menu's card: sente's left corner, a few pieces, a dragon of gote's come down; the picker's: each board at the
 * start (9 x 9 full of pieces, mini shogi's 5 x 5 with its five pieces and a pawn each) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int place[][4] = {{3, 0, SG_ROOK | SG_PROMOTED, 1}, {1, 1, SG_PAWN, 0}, {0, 2, SG_PAWN, 0},
                                   {2, 2, SG_SILVER, 0}, {0, 3, SG_LANCE, 0}, {1, 3, SG_KNIGHT, 0},
                                   {3, 3, SG_KING, 0}};
    spos p;
    int k = 4;                                  /* the squares a side */
    if (variant >= 0) {
        sg_start(&p, variant);
        k = p.n;
    }
    int s = (int)(size / (k + 2 * FRAME + 0.3f)), f = (int)(s * FRAME), n = paint_board(NULL, k, s, f, k == 9);
    while (n > size && s > 4) n = paint_board(NULL, k, --s, f, k == 9);
    int o = (size - n) / 2, m = (n - k * s) / 2;
    uint8_t *board = ui_buf(n, n);
    paint_board(board, k, s, f, k == 9);
    art_blend(rgba, size, size, board, n, n, o, o);
    for (int i = 0; i < k * k; i++) {
        int c = i % k, r = i / k, ix = 0, down = 0;
        if (variant < 0) {
            if (i >= (int)(sizeof place / sizeof place[0])) break;
            c = place[i][0], r = place[i][1], ix = place[i][2], down = place[i][3];
        } else if (p.sq[SG_SQ(c, r)]) {
            ix = tex_of(p.sq[SG_SQ(c, r)]);
            down = SG_OWNER(p.sq[SG_SQ(c, r)]);
        }
        if (!ix) continue;
        uint8_t *one = ui_buf(s, s);
        make_piece(ix, down, s, one);
        art_blend(rgba, size, size, one, s, s, o + m + c * s, o + m + r * s);
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the mating piece to arrive */

static const game shogi_game = {
    .id = "shogi", .order = 170, .variants = SG_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press,
};
REGISTER_GAME(shogi_game)
