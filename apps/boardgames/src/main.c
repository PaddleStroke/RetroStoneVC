/*
 * RetroStone Board (prototype): chess for two players on one device. SDL2, drawn at the screen's own resolution
 * (art.c), no text (nothing to translate): the turn shows on the players' tokens, the end of the game with the kings.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   chess                       the game (a window; full screen on Android)
 *   chess --shot out.bmp [--size 720x1280] [--moves "e2e4 e7e5"] [--select e2] [--flip] [--promote e7e8] [--keep]
 *                               one picture of a position, no window (tests and screenshots)
 *
 * Mouse or touch: tap a piece, then a square (or drag it there). Keys: U undo, F flip, N new game, Esc quit.
 */
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "chess.h"

#define FRAME   0.32f                           /* the board's frame, in squares */
#define ANIM_MS 150

static cgame G;
static cstatus status;
static SDL_Renderer *R;
static SDL_Window *win;
static int W, H, flipped, quit;

typedef struct { float x, y, w, h; } box;
static struct {
    int sq, fr;                                 /* a square, the frame (pixels) */
    float bx, by;                               /* the board's top left (the a8 corner, or h1 when flipped) */
    box cap[2];                                 /* each player's bar: the token and the pieces taken */
    float btn_x[ICONS], btn_y, btn_r;
} L;

static SDL_Texture *t_piece[2][7], *t_small[2][7], *t_big[2], *t_board, *t_dot, *t_ring, *t_glow, *t_gold, *t_shadow,
    *t_icon[ICONS], *t_btn, *t_btn_armed, *t_card, *t_pick, *t_token[2], *t_rim, *t_bg;
static int tex_sq = -1;

static int sel = -1;                            /* the selected square */
static cmove legal[MAX_MOVES];
static int nlegal;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; cmove m; Uint32 t0; } anim;
static struct { int on, from, to; } promo;      /* choosing what a pawn becomes */
static int over_hidden;                         /* the end-of-game card was tapped away */
static Uint32 armed;                            /* "new game" tapped once (a second tap within 2.5 s confirms) */

/* ---- layout ------------------------------------------------------------------------------------------------------- */
static void layout(void)
{
    float m = fminf(W, H) * 0.025f, block = 8 + 2 * FRAME;
    if (W <= H) {                               /* portrait: bar, board, bar, buttons */
        float sq = fminf((W - 2 * m) / block, (H - 2 * m) / (block + 4.0f));
        L.sq = (int)sq;
        L.fr = (int)(L.sq * FRAME);
        float bw = 8.0f * L.sq + 2 * L.fr, total = bw + 4.0f * L.sq;
        float y = floorf((H - total) / 2);
        L.bx = floorf((W - bw) / 2) + L.fr;
        L.by = y + 1.15f * L.sq + L.fr;
        L.by = floorf(L.by);
        box top = {L.bx - L.fr, y, bw, 0.95f * L.sq};
        box bottom = {L.bx - L.fr, L.by + 8 * L.sq + L.fr + 0.2f * L.sq, bw, 0.95f * L.sq};
        L.cap[flipped ? WHITE : BLACK] = top;
        L.cap[flipped ? BLACK : WHITE] = bottom;
        L.btn_r = 0.55f * L.sq;
        L.btn_y = bottom.y + bottom.h + 0.3f * L.sq + L.btn_r;
        for (int i = 0; i < ICONS; i++) L.btn_x[i] = W / 2.0f + (i - 1) * 1.7f * L.sq;
    } else {                                    /* landscape: the board, then a column: bar, buttons, bar */
        float sq = fminf((H - 2 * m) / block, (W - 2 * m) / (block + 4.2f));
        L.sq = (int)sq;
        L.fr = (int)(L.sq * FRAME);
        float bw = 8.0f * L.sq + 2 * L.fr, total = bw + 4.2f * L.sq;
        float x = floorf((W - total) / 2), y = floorf((H - bw) / 2);
        L.bx = x + L.fr;
        L.by = y + L.fr;
        float cx = x + bw + 0.4f * L.sq, cw = 3.8f * L.sq;
        box top = {cx, y, cw, 0.95f * L.sq}, bottom = {cx, y + bw - 0.95f * L.sq, cw, 0.95f * L.sq};
        L.cap[flipped ? WHITE : BLACK] = top;
        L.cap[flipped ? BLACK : WHITE] = bottom;
        L.btn_r = 0.5f * L.sq;
        L.btn_y = y + bw / 2;
        for (int i = 0; i < ICONS; i++) L.btn_x[i] = cx + cw / 2 + (i - 1) * 1.3f * L.sq;
    }
}

static void sq_xy(int s, float *x, float *y)
{
    int f = FILE_OF(s), r = RANK_OF(s);
    if (flipped) { f = 7 - f; r = 7 - r; }
    *x = L.bx + f * L.sq;
    *y = L.by + (7 - r) * L.sq;
}

static int xy_sq(float x, float y)
{
    int f = (int)floorf((x - L.bx) / L.sq), r = 7 - (int)floorf((y - L.by) / L.sq);
    if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
    return flipped ? SQ(7 - f, 7 - r) : SQ(f, r);
}

/* ---- pictures ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *tex(int w, int h, const uint8_t *rgba)
{
    SDL_Texture *t = SDL_CreateTexture(R, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    SDL_UpdateTexture(t, NULL, rgba, w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

static void destroy(SDL_Texture **t)
{
    if (*t) SDL_DestroyTexture(*t);
    *t = NULL;
}

static uint8_t *buf(int w, int h)
{
    static uint8_t *b;
    static size_t cap;
    size_t need = (size_t)w * h * 4;
    if (need > cap) { free(b); b = malloc(need); cap = need; }
    return b;
}

static void textures(void)
{
    if (tex_sq == L.sq) return;
    tex_sq = L.sq;
    int s = L.sq, small = (int)(0.56f * s), big = (int)(2.3f * s), btn = (int)(2 * L.btn_r);
    for (int c = 0; c < 2; c++) {
        for (int t = PAWN; t <= KING; t++) {
            destroy(&t_piece[c][t]);
            art_piece(t, c, s, buf(s, s));
            t_piece[c][t] = tex(s, s, buf(s, s));
            destroy(&t_small[c][t]);
            art_piece(t, c, small, buf(small, small));
            t_small[c][t] = tex(small, small, buf(small, small));
        }
        destroy(&t_big[c]);
        art_piece(KING, c, big, buf(big, big));
        t_big[c] = tex(big, big, buf(big, big));
    }
    int bn = 8 * s + 2 * L.fr;
    destroy(&t_board);
    art_board(s, L.fr, buf(bn, bn));
    t_board = tex(bn, bn, buf(bn, bn));

    static const uint8_t shade[3] = {24, 34, 26}, red[3] = {235, 40, 30}, black[3] = {0, 0, 0},
                         gold[3] = {255, 196, 64}, btn_c[3] = {54, 72, 64}, armed_c[3] = {176, 64, 52};
    int d = (int)(0.32f * s);
    destroy(&t_dot);
    art_disc(d, shade, 0.30f, 0, buf(d, d));
    t_dot = tex(d, d, buf(d, d));
    destroy(&t_ring);
    art_ring(s, 0.82f, shade, 0.30f, buf(s, s));
    t_ring = tex(s, s, buf(s, s));
    destroy(&t_glow);
    art_disc(s, red, 0.9f, 1.5f, buf(s, s));
    t_glow = tex(s, s, buf(s, s));
    destroy(&t_gold);
    art_disc(big, gold, 0.75f, 1.3f, buf(big, big));
    t_gold = tex(big, big, buf(big, big));
    destroy(&t_shadow);
    art_disc(64, black, 0.42f, 1.2f, buf(64, 64));
    t_shadow = tex(64, 64, buf(64, 64));
    destroy(&t_btn);
    art_disc(btn, btn_c, 1, 0, buf(btn, btn));
    t_btn = tex(btn, btn, buf(btn, btn));
    destroy(&t_btn_armed);
    art_disc(btn, armed_c, 1, 0, buf(btn, btn));
    t_btn_armed = tex(btn, btn, buf(btn, btn));
    int ic = (int)(0.62f * btn);
    for (int i = 0; i < ICONS; i++) {
        destroy(&t_icon[i]);
        art_icon(i, ic, buf(ic, ic));
        t_icon[i] = tex(ic, ic, buf(ic, ic));
    }
    int tk = (int)(0.62f * s);
    static const uint8_t token_c[2][3] = {{240, 232, 214}, {46, 42, 40}};
    for (int c = 0; c < 2; c++) {
        destroy(&t_token[c]);
        art_disc(tk, token_c[c], 1, 0, buf(tk, tk));
        t_token[c] = tex(tk, tk, buf(tk, tk));
    }
    static const uint8_t rim_c[3] = {226, 214, 190};  /* the black token's rim: it shows on the dark table */
    destroy(&t_rim);
    art_ring(tk, 0.84f, rim_c, 0.95f, buf(tk, tk));
    t_rim = tex(tk, tk, buf(tk, tk));
    const uint8_t card_c[4] = {28, 34, 32, 236}, pick_c[4] = {246, 240, 226, 255};
    int cw = 6 * s, ch = (int)(3.4f * s);
    destroy(&t_card);
    art_roundrect(cw, ch, 0.35f * s, card_c, buf(cw, ch));
    t_card = tex(cw, ch, buf(cw, ch));
    destroy(&t_pick);
    art_roundrect(s, 4 * s, 0.22f * s, pick_c, buf(s, 4 * s));
    t_pick = tex(s, 4 * s, buf(s, 4 * s));
    if (!t_bg) {                                /* the table: a dark green, lighter in the middle */
        uint8_t *b = buf(2, 256);
        for (int y = 0; y < 256; y++)
            for (int x = 0; x < 2; x++) {
                float k = 1 - fabsf((float)(y - 110)) / 256.0f * 0.55f;
                uint8_t *p = b + 4 * (y * 2 + x);
                p[0] = (uint8_t)(34 * k);
                p[1] = (uint8_t)(52 * k);
                p[2] = (uint8_t)(46 * k);
                p[3] = 255;
            }
        t_bg = tex(2, 256, b);
        SDL_SetTextureBlendMode(t_bg, SDL_BLENDMODE_NONE);
    }
}

static void put(SDL_Texture *t, float x, float y, float w, float h)
{
    SDL_FRect d = {x, y, w, h};
    SDL_RenderCopyF(R, t, NULL, &d);
}

static void put_c(SDL_Texture *t, float cx, float cy, float w, float h) { put(t, cx - w / 2, cy - h / 2, w, h); }

static void fill(float x, float y, float w, float h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_FRect q = {x, y, w, h};
    SDL_SetRenderDrawColor(R, r, g, b, a);
    SDL_RenderFillRectF(R, &q);
}

/* ---- the game ----------------------------------------------------------------------------------------------------- */
static void refresh(void)
{
    status = game_status(&G);
    nlegal = chess_legal(game_pos(&G), legal);
    sel = -1;
    promo.on = 0;
}

static void new_game(void)
{
    game_new(&G);
    over_hidden = 0;
    anim.on = 0;
    refresh();
}

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static void play(cmove m, int animate)
{
    game_play(&G, m);
    anim.on = animate;
    anim.m = m;
    anim.t0 = SDL_GetTicks();
    over_hidden = 0;
    refresh();
}

/* a move from a square to another: a promotion first asks what the pawn becomes */
static void move_to(int from, int to, int animate)
{
    int i = target(from, to);
    if (i < 0) return;
    if (legal[i].promo) {
        promo.on = 1;
        promo.from = from;
        promo.to = to;
        return;
    }
    play(legal[i], animate);
}

static void undo(void)
{
    if (game_undo(&G)) { anim.on = 0; over_hidden = 0; refresh(); }
}

static void flip(void)
{
    flipped = !flipped;
    layout();
}

/* the promotion picker: four squares from the promotion square towards the middle of the board */
static int pick_at(int i, float *x, float *y)
{
    sq_xy(promo.to, x, y);
    int down = *y < L.by + 4 * L.sq;
    *y += (down ? 1 : -1) * i * L.sq;
    return down;
}

static const int PICK[4] = {QUEEN, KNIGHT, ROOK, BISHOP};

/* ---- input -------------------------------------------------------------------------------------------------------- */
static int button_at(float x, float y)
{
    for (int i = 0; i < ICONS; i++)
        if (hypotf(x - L.btn_x[i], y - L.btn_y) < L.btn_r * 1.1f) return i;
    return -1;
}

static void press(float x, float y)
{
    int b = button_at(x, y);
    if (b >= 0) {
        if (b == ICON_UNDO) undo();
        else if (b == ICON_FLIP) flip();
        else if ((status == ST_PLAY && G.ply > 0) && !armed) armed = SDL_GetTicks();
        else { armed = 0; new_game(); }
        return;
    }
    if (promo.on) {
        for (int i = 0; i < 4; i++) {
            float px, py;
            pick_at(i, &px, &py);
            if (x >= px && x < px + L.sq && y >= py && y < py + L.sq) {
                int k = target(promo.from, promo.to);
                for (; k < nlegal; k++)
                    if (legal[k].from == promo.from && legal[k].to == promo.to && legal[k].promo == PICK[i]) break;
                if (k < nlegal) play(legal[k], 0);
                return;
            }
        }
        promo.on = 0;
        return;
    }
    if (status != ST_PLAY) {
        over_hidden = 1;
        return;
    }
    int s = xy_sq(x, y);
    if (s < 0) { sel = -1; return; }
    if (sel >= 0 && target(sel, s) >= 0) {
        move_to(sel, s, 1);
        if (!promo.on) sel = -1;
        return;
    }
    const cpos *p = game_pos(&G);
    if (p->sq[s] && COLOR(p->sq[s]) == p->side) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > L.sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int s = xy_sq(x, y);
    if (s >= 0 && s != drag.from && target(drag.from, s) >= 0) {
        move_to(drag.from, s, 0);
        if (!promo.on) sel = -1;
    }
}

/* ---- drawing ------------------------------------------------------------------------------------------------------ */
static void draw_piece(int pc, float x, float y, float size)
{
    put(t_shadow, x + size * 0.17f, y + size * 0.82f, size * 0.66f, size * 0.15f);
    put(t_piece[COLOR(pc)][TYPE(pc)], x, y, size, size);
}

/* the pieces of a colour no longer on the board (a promoted pawn counts as the pawn) */
static void taken(const cpos *p, int color, int out[7])
{
    static const int start[7] = {0, 8, 2, 2, 2, 1, 1};
    int n[7] = {0}, extra = 0;
    for (int s = 0; s < 64; s++)
        if (p->sq[s] && COLOR(p->sq[s]) == color) n[TYPE(p->sq[s])]++;
    for (int t = KNIGHT; t <= QUEEN; t++) if (n[t] > start[t]) extra += n[t] - start[t];
    for (int t = PAWN; t <= KING; t++) out[t] = start[t] - n[t] > 0 ? start[t] - n[t] : 0;
    out[PAWN] -= extra;
    if (out[PAWN] < 0) out[PAWN] = 0;
}

static void draw_bar(int c)
{
    const cpos *p = game_pos(&G);
    box b = L.cap[c];
    float tk = 0.62f * L.sq, cy = b.y + b.h / 2, tx = b.x + 0.1f * L.sq + tk / 2;
    int turn = status == ST_PLAY && p->side == c, won = status == ST_CHECKMATE && p->side != c;
    if (turn || won) {
        float g = turn ? 1.7f : 2.2f;
        put_c(t_gold, tx, cy, tk * g, tk * g);
    }
    put_c(t_token[c], tx, cy, tk, tk);
    if (c == BLACK) put_c(t_rim, tx, cy, tk, tk);
    int n[7];
    taken(p, !c, n);
    float x = tx + tk / 2 + 0.25f * L.sq, step = 0.36f * L.sq, sz = 0.56f * L.sq;
    static const int order[5] = {QUEEN, ROOK, BISHOP, KNIGHT, PAWN};
    for (int k = 0; k < 5; k++) {
        int t = order[k];
        for (int i = 0; i < n[t] && x + sz < b.x + b.w + 0.2f * L.sq; i++, x += step)
            put(t_small[!c][t], x, cy - sz / 2, sz, sz);
        if (n[t]) x += 0.14f * L.sq;
    }
}

static void draw_over(void)
{
    const cpos *p = game_pos(&G);
    float cx = L.bx + 4 * L.sq, cy = L.by + 4 * L.sq, cw = 6.0f * L.sq, ch = 3.4f * L.sq;
    fill(L.bx, L.by, 8.0f * L.sq, 8.0f * L.sq, 0, 0, 0, 90);
    put_c(t_card, cx, cy, cw, ch);
    float big = 2.3f * L.sq;
    if (status == ST_CHECKMATE) {               /* the winner stands in gold light, the loser's king lies down */
        int winner = !p->side;
        put_c(t_gold, cx - 0.9f * L.sq, cy, big * 1.25f, big * 1.25f);
        put_c(t_big[winner], cx - 0.9f * L.sq, cy - 0.1f * L.sq, big, big);
        SDL_FRect d = {cx + 0.6f * L.sq, cy - 0.35f * L.sq, 1.6f * L.sq, 1.6f * L.sq};
        SDL_RenderCopyExF(R, t_big[!winner], NULL, &d, 90, NULL, SDL_FLIP_NONE);
    } else {                                    /* a draw: both kings standing, an equals sign between them */
        float k = 1.9f * L.sq;
        put_c(t_big[WHITE], cx - 1.55f * L.sq, cy - 0.1f * L.sq, k, k);
        put_c(t_big[BLACK], cx + 1.55f * L.sq, cy - 0.1f * L.sq, k, k);
        float bw = 0.6f * L.sq, bh = 0.12f * L.sq;
        fill(cx - bw / 2, cy - 0.2f * L.sq, bw, bh, 246, 238, 220, 255);
        fill(cx - bw / 2, cy + 0.05f * L.sq, bw, bh, 246, 238, 220, 255);
    }
}

static void draw(void)
{
    const cpos *p = game_pos(&G);
    put(t_bg, 0, 0, (float)W, (float)H);
    put(t_board, L.bx - L.fr, L.by - L.fr, 8.0f * L.sq + 2 * L.fr, 8.0f * L.sq + 2 * L.fr);
    float x, y;
    if (G.ply) {
        cmove m = G.move[G.ply - 1];
        sq_xy(m.from, &x, &y);
        fill(x, y, L.sq, L.sq, 250, 214, 90, 100);
        sq_xy(m.to, &x, &y);
        fill(x, y, L.sq, L.sq, 250, 214, 90, 100);
    }
    if (sel >= 0) {
        sq_xy(sel, &x, &y);
        fill(x, y, L.sq, L.sq, 250, 214, 90, 150);
    }
    if (status != ST_STALEMATE && chess_in_check(p, p->side)) {
        sq_xy(chess_king(p, p->side), &x, &y);
        put(t_glow, x, y, L.sq, L.sq);
    }
    float t = 1;
    if (anim.on) {
        t = (SDL_GetTicks() - anim.t0) / (float)ANIM_MS;
        if (t >= 1) { t = 1; anim.on = 0; }
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    for (int s = 0; s < 64; s++) {
        if (!p->sq[s] || (drag.on && drag.moved && s == drag.from) || (anim.on && s == anim.m.to)) continue;
        sq_xy(s, &x, &y);
        draw_piece(p->sq[s], x, y, (float)L.sq);
    }
    if (sel >= 0 && !promo.on) {                /* where the selected piece can go */
        uint64_t done = 0;
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel || (done >> legal[i].to & 1)) continue;
            done |= 1ull << legal[i].to;
            sq_xy(legal[i].to, &x, &y);
            if (p->sq[legal[i].to]) put(t_ring, x, y, L.sq, L.sq);
            else put_c(t_dot, x + L.sq / 2.0f, y + L.sq / 2.0f, 0.32f * L.sq, 0.32f * L.sq);
        }
    }
    if (anim.on || (t < 1)) {
        float x0, y0, x1, y1;
        sq_xy(anim.m.from, &x0, &y0);
        sq_xy(anim.m.to, &x1, &y1);
        draw_piece(p->sq[anim.m.to], x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, (float)L.sq);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, above the finger */
        float sz = 1.18f * L.sq;
        draw_piece(p->sq[drag.from], drag.x - sz / 2, drag.y - sz * 0.75f, sz);
    }
    draw_bar(WHITE);
    draw_bar(BLACK);
    if (armed && SDL_GetTicks() - armed > 2500) armed = 0;
    for (int i = 0; i < ICONS; i++) {
        float d = 2 * L.btn_r, ic = 0.62f * d;
        put_c(i == ICON_NEW && armed ? t_btn_armed : t_btn, L.btn_x[i], L.btn_y, d, d);
        put_c(t_icon[i], L.btn_x[i], L.btn_y, ic, ic);
    }
    if (promo.on) {
        fill(L.bx, L.by, 8.0f * L.sq, 8.0f * L.sq, 0, 0, 0, 110);
        float px, py;
        int down = pick_at(0, &px, &py);
        put(t_pick, px, down ? py : py - 3 * L.sq, (float)L.sq, 4.0f * L.sq);
        for (int i = 0; i < 4; i++) {
            pick_at(i, &px, &py);
            put(t_piece[p->side][PICK[i]], px, py, (float)L.sq, (float)L.sq);
        }
    }
    if (status != ST_PLAY && !over_hidden && !anim.on) draw_over();
}

/* ---- the program -------------------------------------------------------------------------------------------------- */
static void apply_moves(const char *list)
{
    for (const char *s = list; *s;) {
        while (*s == ' ') s++;
        if (strlen(s) < 4) break;
        int from = (s[0] - 'a') + (s[1] - '1') * 8, to = (s[2] - 'a') + (s[3] - '1') * 8, k = target(from, to);
        if (k < 0) { fprintf(stderr, "not a legal move: %.4s\n", s); exit(1); }
        if (legal[k].promo) for (; k < nlegal && !(legal[k].to == to && legal[k].promo == QUEEN); k++) {}
        play(legal[k], 0);
        s += 4;
        if (*s && *s != ' ') s++;
    }
}

static int shot(int argc, char **argv)
{
    const char *out = NULL, *moves = "", *select = NULL, *promote = NULL;
    int keep = 0;
    W = 720;
    H = 1280;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &W, &H);
        else if (!strcmp(argv[i], "--moves") && i + 1 < argc) moves = argv[++i];
        else if (!strcmp(argv[i], "--select") && i + 1 < argc) select = argv[++i];
        else if (!strcmp(argv[i], "--promote") && i + 1 < argc) promote = argv[++i];
        else if (!strcmp(argv[i], "--flip")) flipped = 1;
        else if (!strcmp(argv[i], "--keep")) keep = 1;  /* the end-of-game card tapped away */
    }
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
    R = SDL_CreateSoftwareRenderer(surf);
    if (!R) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    layout();
    textures();
    new_game();
    apply_moves(moves);
    if (select) sel = (select[0] - 'a') + (select[1] - '1') * 8;
    if (promote) { promo.on = 1; promo.from = (promote[0] - 'a') + (promote[1] - '1') * 8; promo.to = (promote[2] - 'a') + (promote[3] - '1') * 8; }
    over_hidden = keep;
    draw();
    SDL_RenderPresent(R);
    if (SDL_SaveBMP(surf, out)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    printf("%s: %dx%d, status %d\n", out, W, H, status);
    return 0;
}

static void to_pixels(int *x, int *y, float *fx, float *fy)
{
    int ww, wh;
    SDL_GetWindowSize(win, &ww, &wh);
    *fx = *x * (float)W / (ww ? ww : 1);
    *fy = *y * (float)H / (wh ? wh : 1);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--shot")) return shot(argc, argv);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait PortraitUpsideDown");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    win = SDL_CreateWindow("RetroStone Chess", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 540, 900,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    R = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!R) R = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!win || !R) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    new_game();
    while (!quit) {
        SDL_Event e;
        int busy = anim.on || drag.on || armed;
        if (busy ? SDL_PollEvent(&e) : SDL_WaitEventTimeout(&e, 250)) {
            do {
                float fx, fy;
                switch (e.type) {
                case SDL_QUIT: quit = 1; break;
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_LEFT) { to_pixels(&e.button.x, &e.button.y, &fx, &fy); press(fx, fy); }
                    break;
                case SDL_MOUSEMOTION:
                    to_pixels(&e.motion.x, &e.motion.y, &fx, &fy);
                    motion(fx, fy);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.button == SDL_BUTTON_LEFT) { to_pixels(&e.button.x, &e.button.y, &fx, &fy); release(fx, fy); }
                    break;
                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                    case SDLK_u: case SDLK_BACKSPACE: undo(); break;
                    case SDLK_f: flip(); break;
                    case SDLK_n: new_game(); break;
                    case SDLK_ESCAPE: case SDLK_AC_BACK:
                        if (sel >= 0 || promo.on) { sel = -1; promo.on = 0; } else quit = 1;
                        break;
                    }
                    break;
                }
            } while (SDL_PollEvent(&e));
        }
        int w, h;
        SDL_GetRendererOutputSize(R, &w, &h);
        if (w != W || h != H) { W = w; H = h; layout(); }
        textures();
        draw();
        SDL_RenderPresent(R);
    }
    SDL_Quit();
    return 0;
}
