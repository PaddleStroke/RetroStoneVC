/*
 * Chess in RetroStone Board: the board, the pieces, tap or drag, the promotion picker (the rules: chess.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <string.h>

#include "games/chess.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define ANIM_MS 150

static cgame G;
static cmove legal[MAX_MOVES];
static int nlegal;
static int sq, fr, made = -1, made_small = -1;
static float bx, by;                            /* the a8 corner (h1 when flipped) */
static SDL_Texture *t_piece[2][7], *t_small[2][7], *t_board, *t_pick;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; cmove m; Uint32 t0; } anim;
static struct { int on, from, to; } promo;
static const int PICK[4] = {QUEEN, KNIGHT, ROOK, BISHOP};

/* ---- the position ---------------------------------------------------------------------------------------------------- */
static void refresh(void)
{
    nlegal = chess_legal(game_pos(&G), legal);
    sel = -1;
    promo.on = 0;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    game_new(&G);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    switch (game_status(&G)) {
    case ST_PLAY: return GS_PLAY;
    case ST_CHECKMATE: return game_pos(&G)->side == WHITE ? GS_WIN1 : GS_WIN0;
    default: return GS_DRAW;
    }
}

static int turn(void) { return game_pos(&G)->side; }

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
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.flags & MV_CAPTURE ? SND_CAPTURE : SND_MOVE);
    if (status() == GS_PLAY && chess_in_check(game_pos(&G), game_pos(&G)->side)) ui_sound(SND_CHECK);
}

/* a move between two squares: a promotion first asks what the pawn becomes */
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

static int undo(void)
{
    if (!game_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "e2e4 e7e5 e7e8q": the screenshots' and the tests' moves */
static int script(const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (strlen(s) < 4) break;
        int from = (s[0] - 'a') + (s[1] - '1') * 8, to = (s[2] - 'a') + (s[3] - '1') * 8, k = target(from, to);
        if (k < 0) return 0;
        int want = s[4] == 'n' ? KNIGHT : s[4] == 'r' ? ROOK : s[4] == 'b' ? BISHOP : QUEEN;
        if (legal[k].promo) for (; k < nlegal && !(legal[k].to == to && legal[k].promo == want); k++) {}
        if (k >= nlegal) return 0;
        play(legal[k], 0);
        s += 4;
        while (*s && *s != ' ') s++;
    }
    return 1;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------------ */
static void layout(box b)
{
    sq = (int)floorf(b.w / (8 + 2 * FRAME));
    fr = (int)(sq * FRAME);
    float bw = 8.0f * sq + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2) + fr;
    by = floorf(b.y + (b.h - bw) / 2) + fr;
    if (made != sq) {
        made = sq;
        for (int c = 0; c < 2; c++)
            for (int t = PAWN; t <= KING; t++) {
                ui_free(&t_piece[c][t]);
                art_piece(t, c, sq, ui_buf(sq, sq));
                t_piece[c][t] = ui_tex(sq, sq, ui_buf(sq, sq));
            }
        int n = 8 * sq + 2 * fr;
        ui_free(&t_board);
        art_board(sq, fr, ui_buf(n, n));
        t_board = ui_tex(n, n, ui_buf(n, n));
        const uint8_t pick_c[4] = {246, 240, 226, 255};
        ui_free(&t_pick);
        art_roundrect(sq, 4 * sq, 0.22f * sq, pick_c, ui_buf(sq, 4 * sq));
        t_pick = ui_tex(sq, 4 * sq, ui_buf(sq, 4 * sq));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++)
            for (int t = PAWN; t <= KING; t++) {
                ui_free(&t_small[c][t]);
                art_piece(t, c, small, ui_buf(small, small));
                t_small[c][t] = ui_tex(small, small, ui_buf(small, small));
            }
    }
}

static void sq_xy(int s, float *x, float *y)
{
    int f = FILE_OF(s), r = RANK_OF(s);
    if (ui.flipped) { f = 7 - f; r = 7 - r; }
    *x = bx + f * sq;
    *y = by + (7 - r) * sq;
}

static int xy_sq(float x, float y)
{
    int f = (int)floorf((x - bx) / sq), r = 7 - (int)floorf((y - by) / sq);
    if (f < 0 || f > 7 || r < 0 || r > 7) return -1;
    return ui.flipped ? SQ(7 - f, 7 - r) : SQ(f, r);
}

/* the picker: four squares from the promotion square towards the middle of the board */
static int pick_at(int i, float *x, float *y)
{
    sq_xy(promo.to, x, y);
    int down = *y < by + 4 * sq;
    *y += (down ? 1 : -1) * i * sq;
    return down;
}

static void piece_at(int pc, float x, float y, float size)
{
    int c = COLOR(pc), up = ui_upside(c);
    ui_shadow(x, up ? y - 0.79f * size : y, size);
    if (up) ui_put_rot(t_piece[c][TYPE(pc)], x + size / 2, y + size / 2, size, size, 180);
    else ui_put(t_piece[c][TYPE(pc)], x, y, size, size);
}

static void draw(void)
{
    const cpos *p = game_pos(&G);
    ui_put(t_board, bx - fr, by - fr, 8.0f * sq + 2 * fr, 8.0f * sq + 2 * fr);
    float x, y;
    if (G.ply) {
        cmove m = G.move[G.ply - 1];
        sq_xy(m.from, &x, &y);
        ui_mark_last(x, y, sq, sq);
        sq_xy(m.to, &x, &y);
        ui_mark_last(x, y, sq, sq);
    }
    if (sel >= 0) {
        sq_xy(sel, &x, &y);
        ui_mark_select(x, y, sq, sq);
    }
    if (status() != GS_DRAW && chess_in_check(p, p->side)) {
        sq_xy(chess_king(p, p->side), &x, &y);
        ui_mark_glow(x + sq / 2.0f, y + sq / 2.0f, sq);
    }
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)ANIM_MS;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    for (int s = 0; s < 64; s++) {
        if (!p->sq[s] || (drag.on && drag.moved && s == drag.from) || (anim.on && s == anim.m.to)) continue;
        sq_xy(s, &x, &y);
        piece_at(p->sq[s], x, y, (float)sq);
    }
    if (sel >= 0 && !promo.on) {                /* where the selected piece can go */
        uint64_t done = 0;
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel || (done >> legal[i].to & 1)) continue;
            done |= 1ull << legal[i].to;
            sq_xy(legal[i].to, &x, &y);
            if (p->sq[legal[i].to]) ui_mark_ring(x + sq / 2.0f, y + sq / 2.0f, sq);
            else ui_mark_dot(x + sq / 2.0f, y + sq / 2.0f, 0.32f * sq);
        }
    }
    if (anim.on) {
        float x0, y0, x1, y1;
        sq_xy(anim.m.from, &x0, &y0);
        sq_xy(anim.m.to, &x1, &y1);
        piece_at(p->sq[anim.m.to], x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, (float)sq);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? -0.25f : 0.75f;
        piece_at(p->sq[drag.from], drag.x - s / 2, drag.y - s * off, s);
    }
    if (promo.on) {
        ui_fill(bx, by, 8.0f * sq, 8.0f * sq, 0, 0, 0, 110);
        float px, py;
        int down = pick_at(0, &px, &py);
        ui_put(t_pick, px, down ? py : py - 3 * sq, (float)sq, 4.0f * sq);
        for (int i = 0; i < 4; i++) {
            pick_at(i, &px, &py);
            piece_at(PC(p->side, PICK[i]), px, py, (float)sq);
        }
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (promo.on) {
        for (int i = 0; i < 4; i++) {
            float px, py;
            pick_at(i, &px, &py);
            if (x >= px && x < px + sq && y >= py && y < py + sq) {
                for (int k = 0; k < nlegal; k++)
                    if (legal[k].from == promo.from && legal[k].to == promo.to && legal[k].promo == PICK[i]) {
                        play(legal[k], 0);
                        return;
                    }
            }
        }
        promo.on = 0;
        return;
    }
    if (status() != GS_PLAY) return;
    int s = xy_sq(x, y);
    if (s < 0) { sel = -1; return; }
    if (sel >= 0 && target(sel, s) >= 0) {
        move_to(sel, s, 1);
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
    int s = xy_sq(x, y);
    if (s >= 0 && s != drag.from && target(drag.from, s) >= 0) move_to(drag.from, s, 0);
}

/* ---- the shell's pictures ------------------------------------------------------------------------------------------------ */
/* the pieces a player took from the other (a promoted pawn counts as the pawn) */
static void bar(int player, box b)
{
    static const int start_n[7] = {0, 8, 2, 2, 2, 1, 1}, order[5] = {QUEEN, ROOK, BISHOP, KNIGHT, PAWN};
    const cpos *p = game_pos(&G);
    int c = !player, n[7] = {0}, extra = 0, gone[7];
    for (int s = 0; s < 64; s++)
        if (p->sq[s] && COLOR(p->sq[s]) == c) n[TYPE(p->sq[s])]++;
    for (int t = KNIGHT; t <= QUEEN; t++) if (n[t] > start_n[t]) extra += n[t] - start_n[t];
    for (int t = PAWN; t <= KING; t++) gone[t] = start_n[t] > n[t] ? start_n[t] - n[t] : 0;
    gone[PAWN] = gone[PAWN] > extra ? gone[PAWN] - extra : 0;
    float sz = 0.6f * ui.unit, step = 0.36f * ui.unit, x = b.x, cy = b.y + b.h / 2;
    for (int k = 0; k < 5; k++) {
        int t = order[k];
        for (int i = 0; i < gone[t] && x + sz <= b.x + b.w; i++, x += step) ui_put(t_small[c][t], x, cy - sz / 2, sz, sz);
        if (gone[t]) x += 0.14f * ui.unit;
    }
}

static void token(int player, int size, uint8_t *rgba) { art_piece(KING, player, size, rgba); }

/* the menu's card: a corner of a board, a few pieces */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = ui_buf(n, n);
    art_board_n(4, 4, s, f, 1, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    static const int place[][3] = {{0, 0, ROOK + 8}, {2, 0, KING + 8}, {1, 1, PAWN + 8}, {3, 1, PAWN + 8},
                                   {0, 2, PAWN}, {2, 2, KNIGHT}, {1, 3, QUEEN}, {3, 3, KING}};
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        uint8_t *one = ui_buf(s, s);
        art_piece(place[i][2] & 7, place[i][2] >> 3, s, one);
        art_blend(rgba, size, size, one, s, s, o + f + place[i][0] * s, o + f + place[i][1] * s);
    }
}

static const game chess_game = {
    .id = "chess", .order = 10, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script,
};
REGISTER_GAME(chess_game)
