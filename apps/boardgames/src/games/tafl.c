/*
 * Hnefatafl (Viking chess) in RetroStone Board: Copenhagen Hnefatafl on 11 x 11 and Tablut on 9 x 9. The board with
 * its throne and corners inlaid, the attackers (ebony discs), the defenders (ivory discs) and their king (the ivory
 * chess king), tap or drag (the rules: tafl_rules.c, tafl.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The script's notation (the screenshots, the tests): "a4-d4 f4-e4 ...", a move from one square to another, the files
 * a.. from the left and the ranks 1.. from the bottom as the attackers (player 0) see the board.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/chess.h"                        /* KING, WHITE: the king is the chess king */
#include "games/tafl.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define SLIDE_MS 150
#define FADE_MS 260                             /* the pieces taken fade after the slide */

static tgame G;
static tmove legal[TF_MAX_MOVES];
static int nlegal, state = GS_PLAY;
static int n = 11, sq, fr, made = -1, made_n = -1, made_small = -1;
static float bx, by;                            /* the top left square's corner */
static SDL_Texture *t_piece[4], *t_small[2], *t_board;   /* [TF_ATT..TF_KING]; the bars' discs [colour] */
static int sel = -1;
static int threat = -1;                         /* the king's square, when the attackers could take it next move */
static uint8_t escape[TF_MAX_SQ];               /* where the king could escape next move (the attackers' warning) */
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; tmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
/* the other player's moves, as if it moved now */
static int their_moves(tmove *out)
{
    tpos p = *tf_pos(&G);
    p.side = (uint8_t)!p.side;
    return tf_legal(&p, G.variant, out);
}

/* the warnings for the player to move: the defenders, a king the attackers could take; the attackers, the squares
 * the king could escape to */
static void warnings(void)
{
    static tmove m[TF_MAX_MOVES];
    const tpos *p = tf_pos(&G);
    int k = tf_king(p, G.variant), count = state == GS_PLAY && k >= 0 ? their_moves(m) : 0;
    threat = -1;
    memset(escape, 0, sizeof escape);
    for (int i = 0; i < count; i++) {
        if (p->side == 0) {
            if (m[i].from == k && tf_escape(G.variant, m[i].to)) escape[m[i].to] = 1;
            continue;
        }
        tpos q = *p;
        q.side = 0;
        tf_make(&q, G.variant, &m[i]);
        if (tf_king(&q, G.variant) < 0) threat = k;
    }
}

static void refresh(void)
{
    nlegal = tf_legal(tf_pos(&G), G.variant, legal);
    switch (tf_status(&G)) {
    case TF_PLAY: state = GS_PLAY; break;
    case TF_WON0: state = GS_WIN0; break;
    case TF_WON1: state = GS_WIN1; break;
    default: state = GS_DRAW;
    }
    warnings();
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    tf_new(&G, variant == 1 ? TF_TABLUT : TF_HNEFATAFL);
    n = tf_size(G.variant);
    anim.on = 0;
    refresh();
}

static int status(void) { return state; }
static int turn(void) { return tf_pos(&G)->side; }

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

/* plays a legal move; slide: the piece slides there (a tap), else it is already there (dropped) */
static void play(int i, int slide)
{
    tmove m = legal[i];
    if (!tf_play(&G, &m)) return;
    anim.on = 1;
    anim.m = m;
    anim.t0 = ui_ms() - (slide ? 0 : SLIDE_MS);
    refresh();
    ui_sound(m.ncap ? SND_CAPTURE : SND_MOVE);
    if (state == GS_PLAY && (threat >= 0 || memchr(escape, 1, sizeof escape))) ui_sound(SND_CHECK);
}

static int undo(void)
{
    if (!tf_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "a4-d4 f4-e4": the screenshots' and the tests' moves */
static int script(const char *s)
{
    int ok = tf_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the inlay of a marked square, s x s: a walnut square, a light wood star (the throne) or diamond (a corner) */
static void paint_inlay(int throne, int s, uint8_t *rgba)
{
    art_style st = {{120, 80, 52}, {58, 36, 22}, {58, 36, 22}, {236, 212, 172}, 0.25f, fmaxf(0.02f, 1.0f / s)};
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, 0.08f, 0.08f, 0.92f, 0.92f);
    if (throne) {                               /* a star of eight points, the four long ones along the lines */
        float star[32];
        for (int i = 0; i < 16; i++) {
            float a = i * 3.14159265f / 8, r = i % 4 == 0 ? 0.34f : i % 2 ? 0.1f : 0.2f;
            star[2 * i] = 0.5f + r * cosf(a);
            star[2 * i + 1] = 0.5f + r * sinf(a);
        }
        art_poly(f, ART_MARK, star, 16);
    } else {
        const float diamond[] = {0.5f, 0.2f, 0.8f, 0.5f, 0.5f, 0.8f, 0.2f, 0.5f};
        art_poly(f, ART_MARK, diamond, 4);
    }
    art_render(f, &st, s, s, (float)s, 0, 0, rgba);
    art_fig_free(f);
}

/* the board of k x k squares of s pixels in a frame of f, the throne (the middle square) and the corners (if
 * `corners`) inlaid; (k s + 2 f) squared */
static void paint_board(int k, int corners, int s, int f, uint8_t *rgba)
{
    int w = k * s + 2 * f;
    art_board_n(k, k, s, f, 1, rgba);
    uint8_t *tile = malloc((size_t)s * s * 4);
    if (!tile) return;
    paint_inlay(1, s, tile);
    art_blend(rgba, w, w, tile, s, s, f + k / 2 * s, f + k / 2 * s);
    if (corners) {
        paint_inlay(0, s, tile);
        for (int c = 0; c < 4; c++)
            art_blend(rgba, w, w, tile, s, s, f + (c & 1) * (k - 1) * s, f + c / 2 * (k - 1) * s);
    }
    free(tile);
}

/* a piece's picture, size x size */
static void paint_piece(int pc, int size, uint8_t *rgba)
{
    if (pc == TF_KING) art_piece(KING, WHITE, size, rgba);
    else art_counter(COUNTER_DISC, pc == TF_ATT, size, rgba);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    sq = (int)floorf(b.w / (n + 2 * FRAME));
    fr = (int)(sq * FRAME);
    float bw = (float)n * sq + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2) + fr;
    by = floorf(b.y + (b.h - bw) / 2) + fr;
    if (made != sq || made_n != n) {
        made = sq;
        made_n = n;
        for (int pc = TF_ATT; pc <= TF_KING; pc++) {
            ui_free(&t_piece[pc]);
            paint_piece(pc, sq, ui_buf(sq, sq));
            t_piece[pc] = ui_tex(sq, sq, ui_buf(sq, sq));
        }
        int w = n * sq + 2 * fr;
        ui_free(&t_board);
        paint_board(n, G.variant == TF_HNEFATAFL, sq, fr, ui_buf(w, w));
        t_board = ui_tex(w, w, ui_buf(w, w));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_small[c]);
            art_counter(COUNTER_DISC, c, small, ui_buf(small, small));
            t_small[c] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

static void sq_xy(int s, float *x, float *y)
{
    int f = s % n, r = s / n;
    if (ui.flipped) { f = n - 1 - f; r = n - 1 - r; }
    *x = bx + (float)f * sq;
    *y = by + (float)(n - 1 - r) * sq;
}

static int xy_sq(float x, float y)
{
    int f = (int)floorf((x - bx) / sq), r = n - 1 - (int)floorf((y - by) / sq);
    if (f < 0 || f >= n || r < 0 || r >= n) return -1;
    return ui.flipped ? (n - 1 - r) * n + n - 1 - f : r * n + f;
}

/* a piece at (x, y), size x size (alpha: below 1, fading): the discs lie flat, the king stands, turned round for the
 * top player face to face */
static void piece_at(int pc, float x, float y, float size, float alpha)
{
    SDL_Texture *t = t_piece[pc];
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * alpha));
    if (pc == TF_KING) {
        int up = ui_upside(1);
        ui_shadow(x, up ? y - 0.79f * size : y, size);
        if (up) ui_put_rot(t, x + size / 2, y + size / 2, size, size, 180);
        else ui_put(t, x, y, size, size);
    } else {
        ui_shadow_flat(x + size / 2, y + size / 2, size, size, alpha);
        ui_put(t, x, y, size, size);
    }
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw(void)
{
    const tpos *p = tf_pos(&G);
    float w = (float)n * sq + 2 * fr, x, y, h = sq / 2.0f;
    ui_put(t_board, bx - fr, by - fr, w, w);
    if (G.ply) {
        const tmove *m = &G.move[G.ply - 1];
        sq_xy(m->from, &x, &y);
        ui_mark_last(x, y, sq, sq);
        sq_xy(m->to, &x, &y);
        ui_mark_last(x, y, sq, sq);
    }
    if (sel >= 0) {
        sq_xy(sel, &x, &y);
        ui_mark_select(x, y, sq, sq);
    }
    if (threat >= 0) {                          /* the king the attackers could take next move */
        sq_xy(threat, &x, &y);
        ui_mark_glow(x + h, y + h, sq);
    }
    float slide = 1, fade = 1;
    if (anim.on) {
        float ms = (float)(ui_ms() - anim.t0);
        slide = fminf(ms / SLIDE_MS, 1);
        fade = anim.m.ncap ? fminf(fmaxf((ms - SLIDE_MS) / FADE_MS, 0), 1) : 1;
        if (slide >= 1 && fade >= 1) anim.on = 0;
        else ui_animate();
        slide = 1 - (1 - slide) * (1 - slide) * (1 - slide);
    }
    for (int s = 0; s < n * n; s++) {
        if (!p->sq[s] || (drag.on && drag.moved && s == drag.from) || (anim.on && s == anim.m.to)) continue;
        sq_xy(s, &x, &y);
        piece_at(p->sq[s], x, y, (float)sq, 1);
    }
    for (int s = 0; s < n * n; s++)             /* where the king could escape: the attackers' warning */
        if (escape[s]) {
            sq_xy(s, &x, &y);
            ui_mark_gold(x + h, y + h, sq);
        }
    for (int i = 0; sel >= 0 && i < nlegal; i++)
        if (legal[i].from == sel) {
            sq_xy(legal[i].to, &x, &y);
            ui_mark_dot(x + h, y + h, 0.32f * sq);
        }
    if (anim.on) {
        const tpos *before = &G.pos[G.ply - 1];
        for (int i = 0; i < anim.m.ncap; i++) { /* the pieces taken, lifted and fading */
            sq_xy(anim.m.cap[i], &x, &y);
            float grow = 1 + 0.18f * fade, s = sq * grow;
            piece_at(before->sq[anim.m.cap[i]], x + h - s / 2, y + h - s / 2, s, 1 - fade);
        }
        float x0, y0, x1, y1;
        sq_xy(anim.m.from, &x0, &y0);
        sq_xy(anim.m.to, &x1, &y1);
        piece_at(p->sq[anim.m.to], x0 + (x1 - x0) * slide, y0 + (y1 - y0) * slide, (float)sq, 1);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? -0.25f : 0.75f;
        piece_at(p->sq[drag.from], drag.x - s / 2, drag.y - s * off, s, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (state != GS_PLAY) return;
    int s = xy_sq(x, y);
    if (s < 0) { sel = -1; return; }
    if (sel >= 0 && target(sel, s) >= 0) {
        play(target(sel, s), 1);
        return;
    }
    const tpos *p = tf_pos(&G);
    if (p->sq[s] && TF_OWNER(p->sq[s]) == p->side) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    if (sel >= 0 || p->sq[s]) ui_sound(SND_DENY);
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
    if (i >= 0) play(i, 0);
    else if (s >= 0 && s != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces a player took: a disc of the other side, and how many (the king's capture ends the game) */
static void bar(int player, box b)
{
    const tpos *p = tf_pos(&G);
    int victim = player ? TF_ATT : TF_DEF, left = 0;
    for (int s = 0; s < n * n; s++) left += p->sq[s] == victim;
    ui_bar_count(b, t_small[victim == TF_ATT], tf_count(G.variant, victim) - left);
}

/* the attackers: an ebony disc; the defenders: their king */
static void token(int player, int size, uint8_t *rgba) { paint_piece(player ? TF_KING : TF_ATT, size, rgba); }

/* the menu's card (-1): the throne and the king among his defenders, the attackers closing in; the picker's: the whole
 * board at the start (11 x 11, or Tablut's 9 x 9) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const char *const CARD = ".A..."
                                    "..D.A"
                                    "ADK.."
                                    "...D."
                                    "..A..";
    int v = variant == 1 ? TF_TABLUT : TF_HNEFATAFL, k = variant < 0 ? 5 : tf_size(v);
    int s = (int)(size / (k + 2 * FRAME)), f = (int)(s * FRAME), w = k * s + 2 * f, o = (size - w) / 2;
    uint8_t *board = ui_buf(w, w);
    paint_board(k, variant == 0, s, f, board);
    art_blend(rgba, size, size, board, w, w, o, o);
    tpos p;
    memset(&p, 0, sizeof p);
    if (variant >= 0) tf_start(&p, v);
    for (int pc = TF_ATT; pc <= TF_KING; pc++) {
        uint8_t *one = ui_buf(s, s);
        paint_piece(pc, s, one);
        for (int y = 0; y < k; y++)
            for (int x = 0; x < k; x++) {
                char c = variant < 0 ? CARD[(k - 1 - y) * k + x] : "-ADK"[p.sq[y * k + x]];
                if (c == "-ADK"[pc]) art_blend(rgba, size, size, one, s, s, o + f + x * s, o + f + (k - 1 - y) * s);
            }
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last move: the slide, the pieces fading */

static const game tafl_game = {
    .id = "tafl", .order = 220, .variants = 2,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(tafl_game)
