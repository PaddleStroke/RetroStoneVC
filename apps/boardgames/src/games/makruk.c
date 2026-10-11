/*
 * Makruk (Thai chess) in RetroStone Board: the plain wooden board with its lines (no checkered squares, as in
 * Thailand), the pieces in the house's ivory and ebony, tap or drag, the Bia turned into a Met on the sixth rank, the
 * honour count in the counting player's bar (the rules and the notation: makruk_rules.c, makruk.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The pieces: the Khun, the Rua and the Ma move as chess's king, rook and knight and wear their pictures (art_piece);
 * the others have their own, in the same style: the Met (one step diagonally) a cut gem, a diamond, on a slim body;
 * the Khon (one step diagonally or straight forwards) a temple's bell with its spire; the Bia a low cowrie dome. A
 * promoted Bia is a Met and looks like one.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"
#include "games/grid.h"
#include "games/makruk.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define ANIM_MS 150
#define FADE_MS 200
#define PI_F 3.14159265f

static mkgame G;
static mkmove legal[MK_MAX_MOVES];
static int nlegal;
static int sq, fr, made = -1, made_small = -1;
static float bx, by;                            /* the a8 corner (h1 when flipped) */
static SDL_Texture *t_piece[2][7], *t_small[2][7], *t_board;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; mkmove m; uint8_t pc; Uint32 t0; } anim;   /* pc: the piece as it left (a Bia promoting) */

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = mk_legal(mk_pos(&G), legal);
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    mk_new(&G);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    switch (mk_status(&G)) {
    case MS_PLAY: return GS_PLAY;
    case MS_CHECKMATE: return mk_pos(&G)->side == MK_WHITE ? GS_WIN1 : GS_WIN0;
    default: return GS_DRAW;
    }
}

static int turn(void) { return mk_pos(&G)->side; }

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static void play(mkmove m, int animate)
{
    anim.pc = mk_pos(&G)->sq[m.from];
    if (!mk_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.taken ? SND_CAPTURE : SND_MOVE);
    if (status() == GS_PLAY && mk_in_check(mk_pos(&G), mk_pos(&G)->side)) ui_sound(SND_CHECK);
}

static int undo(void)
{
    if (!mk_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "e3e4 d6d5 e4d5": the screenshots' and the tests' moves (makruk.h) */
static int script(const char *s)
{
    int ok = mk_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pieces -------------------------------------------------------------------------------------------------- */
/* the foot every piece stands on: a round plinth and a step */
static void plinth(art_fig *f)
{
    art_rect(f, ART_PART, 0.20f, 0.832f, 0.80f, 0.905f);
    const float step[] = {0.26f, 0.832f, 0.74f, 0.832f, 0.695f, 0.772f, 0.305f, 0.772f};
    art_poly(f, ART_PART, step, 4);
}

/* the upper half of an ellipse, from its left end over the top to its right end (a dome) */
static void dome(art_fig *f, float cx, float cy, float rx, float ry)
{
    float p[2 * 25];
    for (int i = 0; i <= 24; i++) {
        float a = PI_F * i / 24;
        p[2 * i] = cx - rx * cosf(a);
        p[2 * i + 1] = cy - ry * sinf(a);
    }
    art_poly(f, ART_PART, p, 25);
}

/* the Met: a slim body, a collar, a gem cut as a diamond (it steps along the diagonals), a bead on top */
static void met(art_fig *f)
{
    const float body[] = {0.365f, 0.772f, 0.635f, 0.772f, 0.565f, 0.585f, 0.435f, 0.585f};
    const float gem[] = {0.50f, 0.215f, 0.685f, 0.415f, 0.50f, 0.615f, 0.315f, 0.415f};
    const float facet[] = {0.50f, 0.315f, 0.59f, 0.415f, 0.50f, 0.515f, 0.41f, 0.415f};
    art_poly(f, ART_PART, body, 4);
    art_poly(f, ART_PART, gem, 4);
    art_poly(f, ART_PART, facet, 4);
    art_ellipse(f, ART_PART, 0.5f, 0.588f, 0.15f, 0.036f);
    art_ellipse(f, ART_PART, 0.5f, 0.20f, 0.04f, 0.04f);
}

/* the Khon: a temple's bell (a chedi) on a broad body, its rings and its spire */
static void khon(art_fig *f)
{
    const float body[] = {0.335f, 0.772f, 0.665f, 0.772f, 0.59f, 0.565f, 0.41f, 0.565f};
    const float spire[] = {0.468f, 0.32f, 0.532f, 0.32f, 0.50f, 0.115f};
    art_poly(f, ART_PART, body, 4);
    dome(f, 0.5f, 0.565f, 0.165f, 0.215f);
    art_ellipse(f, ART_PART, 0.5f, 0.567f, 0.19f, 0.04f);
    art_poly(f, ART_PART, spire, 3);
    art_ellipse(f, ART_PART, 0.5f, 0.355f, 0.078f, 0.03f);
    art_ellipse(f, ART_PART, 0.5f, 0.31f, 0.055f, 0.024f);
}

/* the Bia: low, a cowrie shell's dome with its lip */
static void bia(art_fig *f)
{
    dome(f, 0.5f, 0.775f, 0.215f, 0.185f);
    dome(f, 0.5f, 0.775f, 0.13f, 0.10f);
    art_ellipse(f, ART_PART, 0.5f, 0.585f, 0.045f, 0.03f);
}

/* a piece of a player (ivory or ebony), size x size: the chess pictures for the Khun, the Rua and the Ma */
static void paint_piece(int kind, int color, int size, uint8_t *rgba)
{
    if (kind == MK_KHUN || kind == MK_RUA || kind == MK_MA) {
        art_piece(kind == MK_KHUN ? KING : kind == MK_RUA ? ROOK : KNIGHT, color, size, rgba);
        return;
    }
    art_style st = art_player[color];
    st.line = fmaxf(0.024f, 1.3f / size);
    art_fig *f = art_fig_new();
    plinth(f);
    if (kind == MK_MET) met(f);
    else if (kind == MK_KHON) khon(f);
    else bia(f);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

/* ---- the board --------------------------------------------------------------------------------------------------- */
/* the plain plank in its frame (nx x ny squares of s pixels, frame f) and the lines between the squares, the
 * outer ones thicker; open: the lines of the right and bottom edges left out, running on to the frame (a corner of
 * the board: the menu's card) */
static void paint_board(uint8_t *o, int nx, int ny, int s, int f, int open)
{
    static const uint8_t INK[3] = {40, 28, 18};
    int w = nx * s + 2 * f, h = ny * s + 2 * f, st = w > h ? w : h;
    art_board_n(nx, ny, s, f, 0, o);
    float *cov = calloc((size_t)st * st, sizeof *cov);
    if (!cov) return;
    float lw = 0.8f + 0.018f * s, x1 = (float)(f + nx * s), y1 = (float)(f + ny * s);
    for (int i = 0; i <= nx; i++) {
        float c = (float)(f + i * s), t = i == 0 || (i == nx && !open) ? 1.8f * lw : lw;
        if (i == nx && open) continue;
        grid_cover_rect(cov, st, c - t / 2, f - 0.9f * lw, c + t / 2, y1 + (open ? 0 : 0.9f * lw));
    }
    for (int i = 0; i <= ny; i++) {
        float c = (float)(f + i * s), t = i == 0 || (i == ny && !open) ? 1.8f * lw : lw;
        if (i == ny && open) continue;
        grid_cover_rect(cov, st, f - 0.9f * lw, c - t / 2, x1 + (open ? 0 : 0.9f * lw), c + t / 2);
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float a = 0.85f * cov[y * st + x];
            uint8_t *p = o + 4 * (y * w + x);
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)(p[j] * (1 - a) + INK[j] * a + 0.5f);
        }
    free(cov);
}

static void make_set(SDL_Texture *t[2][7], int size)
{
    for (int c = 0; c < 2; c++)
        for (int k = MK_BIA; k <= MK_KHUN; k++) {
            ui_free(&t[c][k]);
            paint_piece(k, c, size, ui_buf(size, size));
            t[c][k] = ui_tex(size, size, ui_buf(size, size));
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
        make_set(t_piece, sq);
        int n = 8 * sq + 2 * fr;
        ui_free(&t_board);
        paint_board(ui_buf(n, n), 8, 8, sq, fr, 0);
        t_board = ui_tex(n, n, ui_buf(n, n));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        make_set(t_small, small);
    }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void sq_xy(int s, float *x, float *y)    /* a square's top left corner */
{
    int f = MK_FILE(s), r = MK_RANK(s);
    if (ui.flipped) { f = 7 - f; r = 7 - r; }
    *x = bx + f * sq;
    *y = by + (7 - r) * sq;
}

static int xy_sq(float x, float y)
{
    int f = (int)floorf((x - bx) / sq), r = 7 - (int)floorf((y - by) / sq);
    if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
    return ui.flipped ? MK_SQ(7 - f, 7 - r) : MK_SQ(f, r);
}

/* a standing piece at (x, y), size x size, with its shadow; turned round for the top player face to face; alpha */
static void piece_at(int pc, float x, float y, float size, float alpha)
{
    int c = MK_COLOR(pc), up = ui_upside(c);
    SDL_Texture *t = t_piece[c][MK_KIND(pc)];
    if (alpha >= 1) ui_shadow(x, up ? y - 0.79f * size : y, size);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(alpha, 0, 1)));
    if (up) ui_put_rot(t, x + size / 2, y + size / 2, size, size, 180);
    else ui_put(t, x, y, size, size);
    SDL_SetTextureAlphaMod(t, 255);
}

static void mark_sq(int s, void (*mark)(float, float, float, float))
{
    float x, y;
    sq_xy(s, &x, &y);
    mark(x, y, (float)sq, (float)sq);
}

static void draw(void)
{
    const mkpos *p = mk_pos(&G);
    float x, y;
    ui_put(t_board, bx - fr, by - fr, 8.0f * sq + 2 * fr, 8.0f * sq + 2 * fr);
    if (G.ply) {
        mark_sq(G.move[G.ply - 1].from, ui_mark_last);
        mark_sq(G.move[G.ply - 1].to, ui_mark_last);
    }
    if (sel >= 0) mark_sq(sel, ui_mark_select);
    if (status() != GS_DRAW && mk_in_check(p, p->side)) {
        sq_xy(mk_king(p, p->side), &x, &y);
        ui_mark_glow(x + sq / 2.0f, y + sq / 2.0f, sq);
    }
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)ANIM_MS;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    float e = 1 - (1 - t) * (1 - t) * (1 - t);
    for (int s = 0; s < 64; s++) {
        if (!p->sq[s] || (drag.on && drag.moved && s == drag.from) || (anim.on && s == anim.m.to)) continue;
        sq_xy(s, &x, &y);
        piece_at(p->sq[s], x, y, (float)sq, 1);
    }
    if (sel >= 0)                               /* where the selected piece can go */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            sq_xy(legal[i].to, &x, &y);
            if (legal[i].taken) ui_mark_ring(x + sq / 2.0f, y + sq / 2.0f, sq);
            else ui_mark_dot(x + sq / 2.0f, y + sq / 2.0f, 0.32f * sq);
        }
    if (anim.on) {
        float x0, y0;
        sq_xy(anim.m.to, &x, &y);
        if (anim.m.taken && t * ANIM_MS < FADE_MS) piece_at(anim.m.taken, x, y, (float)sq, 1 - e);   /* fading */
        sq_xy(anim.m.from, &x0, &y0);
        float px = x0 + (x - x0) * e, py = y0 + (y - y0) * e;
        piece_at(anim.pc, px, py, (float)sq, 1);   /* a promoting Bia arrives as a Bia */
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? -0.25f : 0.75f;
        piece_at(p->sq[drag.from], drag.x - s / 2, drag.y - s * off, s, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    int s = xy_sq(x, y), i;
    if (s < 0) { sel = -1; return; }
    if (sel >= 0 && (i = target(sel, s)) >= 0) {
        play(legal[i], 1);
        return;
    }
    const mkpos *p = mk_pos(&G);
    if (p->sq[s] && MK_COLOR(p->sq[s]) == p->side) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    if (sel >= 0) ui_sound(SND_DENY);
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int s = xy_sq(x, y), i = s >= 0 && s != drag.from ? target(drag.from, s) : -1;
    if (i >= 0) play(legal[i], 0);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces a player took (a promoted Met counts as the Bia it was), the strongest first; while an honour count
 * runs, the counting player's bar ends with the moves the other player has left to mate */
static void bar(int player, box b)
{
    static const int start_n[7] = {0, 8, 2, 2, 2, 1, 1}, order[5] = {MK_RUA, MK_MET, MK_KHON, MK_MA, MK_BIA};
    const mkpos *p = mk_pos(&G);
    int c = !player, n[7] = {0};
    for (int s = 0; s < 64; s++)
        if (p->sq[s] && MK_COLOR(p->sq[s]) == c) n[p->sq[s] & MK_PROMOTED ? MK_BIA : MK_KIND(p->sq[s])]++;
    float sz = 0.6f * ui.unit, step = 0.36f * ui.unit, x = b.x, cy = b.y + b.h / 2, end = b.x + b.w;
    int left = mk_moves_left(p);
    if (left >= 0 && p->counter == player) {
        float dh = 0.46f * ui.unit, w = (0.74f * (left >= 10 ? 1 : 0) + 0.62f) * dh;
        ui_number(end - w / 2, cy, dh, left, 1);
        end -= w + 0.3f * ui.unit;
    }
    for (int k = 0; k < 5; k++) {
        int kind = order[k], gone = start_n[kind] - n[kind];
        for (int i = 0; i < gone && x + sz <= end; i++, x += step) ui_put(t_small[c][kind], x, cy - sz / 2, sz, sz);
        if (gone > 0) x += 0.14f * ui.unit;
    }
}

static void token(int player, int size, uint8_t *rgba) { art_piece(KING, player, size, rgba); }

/* the menu's card: a corner of the plain board, the pieces that are Makruk's own in front */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = ui_buf(n, n);
    paint_board(board, 4, 4, s, f, 1);
    art_blend(rgba, size, size, board, n, n, o, o);
    static const int place[][3] = {{0, 0, MK_PC(1, MK_KHON)}, {2, 0, MK_PC(1, MK_KHUN)}, {1, 1, MK_PC(1, MK_BIA)},
                                   {3, 1, MK_PC(1, MK_MA)}, {0, 2, MK_PC(0, MK_BIA)}, {2, 2, MK_PC(0, MK_KHON)},
                                   {1, 3, MK_PC(0, MK_MET)}, {3, 3, MK_PC(0, MK_RUA)}};
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        uint8_t *one = ui_buf(s, s);
        paint_piece(MK_KIND(place[i][2]), MK_COLOR(place[i][2]), s, one);
        art_blend(rgba, size, size, one, s, s, o + f + place[i][0] * s, o + f + place[i][1] * s);
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the mating piece to arrive */

static const game makruk_game = {
    .id = "makruk", .order = 165, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(makruk_game)
