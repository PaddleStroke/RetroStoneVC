/*
 * Shogi Mate in RetroStone Board, for one player: mate problems on the shogi board (the rules and the solver:
 * shogimate_rules.c with shogi_rules.c; the levels: shogimate_levels.h, mates in 1, 3, 5 and 7). No random tab.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The player is sente, at the bottom; the pieces in hand are in the bar (a tap chooses one for a drop). A tap on a
 * piece shows where it gives check (only checks are allowed), a tap there plays it (or drag the piece); when the move
 * may promote or not and both give check, a small picker asks. The defender answers by itself. A check off the line
 * is a mistake: the defender's escape comes, both glow red a moment and go back. The lamp (the shell's tool): the
 * piece to move lights up, a second press its square too. Undo takes back the last move and its answer. The bar: the
 * pieces in hand, and the moves left to mate beside gote's king. Solved: a gold light from the mating piece, then the
 * stars (ui_logic_stars: hints and mistakes).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/shogimate.h"
#include "games/shogimate_art.h"
#include "shell.h"

#define FRAME 0.30f
#define ANIM_MS 170
#define REPLY_MS 280
#define WRONG_MS 900
#define WAVE_STEP 60
#define WAVE_LEN 600
#define WAVE_END (9 * WAVE_STEP + WAVE_LEN)

static smt_game G;
static box room;
static int sq, fr, made = -1, made_small = -1;
static float bx, by;                            /* the top left square's corner */
static SDL_Texture *t_pc[2][32], *t_small[2][32], *t_board, *t_pick;
static smove ch[SG_MAX_MOVES];                  /* the checks now */
static int nch, sel = -1, hand_sel;
static box hand_at[8];
static Uint32 shown_at[SMT_MAX + 4];
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, from, to; } promo;
static struct { int on; Uint32 t0; } wrong, win;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nch = smt_to_find(&G) ? smt_checks(smt_pos(&G), ch) : 0;
    sel = -1;
    hand_sel = 0;
    promo.on = 0;
    drag.on = 0;
}

static void calm(void)
{
    memset(shown_at, 0, sizeof shown_at);
    wrong.on = win.on = 0;
    refresh();
}

static void start(int level)
{
    if (!smt_level(&G, level)) smt_level(&G, 0);   /* never: every level is tested */
    calm();
}

static int status(void) { return smt_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return ui_logic_stars(G.hints, G.mistakes); }
static int score(void) { return (G.ply + 1) / 2; }

static int shown(Uint32 now)
{
    int k = G.ply;
    while (k > 0 && now < shown_at[k]) k--;
    return k;
}

static Uint32 wrong_end(void) { return ANIM_MS + REPLY_MS + ANIM_MS + WRONG_MS; }

static int busy(void)
{
    Uint32 now = ui_ms();
    if (shown(now) < G.ply || (G.ply && now < shown_at[G.ply] + ANIM_MS)) return 1;
    if (wrong.on) return 1;
    return win.on && now - win.t0 < WAVE_END;
}

static int undo(void)
{
    if (busy() || !smt_undo(&G)) return 0;
    calm();
    return 1;
}

static int sound_of(int k)                      /* the sound of the move that made position k */
{
    smove m = G.played[k - 1];
    return m.drop ? SND_PLACE : G.pos[k - 1].sq[m.to] ? SND_CAPTURE : SND_MOVE;
}

/* the player's move; slide: it slides from its square (a tap), else it is there already (a drag) */
static void attempt(smove m, int slide)
{
    int before = G.ply, r = smt_try(&G, m);
    Uint32 now = ui_ms();
    if (r == SMT_ILLEGAL) { ui_sound(SND_DENY); refresh(); return; }
    shown_at[before + 1] = slide ? now : now - ANIM_MS;
    Uint32 landed = shown_at[before + 1] + ANIM_MS;
    ui_sound_later(sound_of(before + 1), (int)(landed - now));
    ui_sound_later(SND_CHECK, (int)(landed - now) + 60);
    if (G.ply > before + 1) {                   /* the defender's answer */
        shown_at[before + 2] = landed + REPLY_MS;
        ui_sound_later(sound_of(before + 2), (int)(landed + REPLY_MS + ANIM_MS - now));
    }
    if (r == SMT_WRONG) {
        wrong.on = 1;
        wrong.t0 = shown_at[before + 1];
        ui_sound_later(SND_DENY, (int)(landed + REPLY_MS + ANIM_MS - now) + 150);
    }
    if (r == SMT_SOLVED) { win.on = 1; win.t0 = landed; }
    refresh();
    ui_animate();
}

static void hint(void)
{
    if (busy() || !smt_hint(&G)) { ui_sound(SND_DENY); return; }
    sel = -1;
    hand_sel = 0;
    ui_sound(SND_TAP);
    ui_animate();
}

/* "G*2b 1c2b" (USI; the answers by themselves, a mistake taken back); "#n" first: level n + 1 */
static int script(const char *moves)
{
    int ok = smt_script(&G, moves);
    calm();
    if (smt_solved(&G)) { win.on = 1; win.t0 = ui_ms(); }
    return ok;
}

/* the checks from a square (or a drop of a kind) to a square: promoting, not promoting (-1: none) */
static void find(int from, int kind, int to, int *yes, int *no)
{
    *yes = *no = -1;
    for (int i = 0; i < nch; i++) {
        smove m = ch[i];
        if (m.to != to || m.drop != kind || (!kind && m.from != from)) continue;
        if (m.promo) *yes = i; else *no = i;
    }
}

static int can_go(int from, int kind, int to)
{
    int y, n;
    find(from, kind, to, &y, &n);
    return y >= 0 || n >= 0;
}

/* a move between squares (or a drop): the picker when promoting or not both give check */
static void move_to(int from, int kind, int to, int slide)
{
    int yes, no;
    find(from, kind, to, &yes, &no);
    if (yes >= 0 && no >= 0) {
        promo.on = 1;
        promo.from = from;
        promo.to = to;
        return;
    }
    if (yes >= 0 || no >= 0) attempt(ch[yes >= 0 ? yes : no], slide);
    else ui_sound(SND_DENY);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void make_set(SDL_Texture *t[2][32], int size)
{
    for (int d = 0; d < 2; d++)
        for (int ix = 1; ix < 32; ix++) {
            ui_free(&t[d][ix]);
            if (!sma_glyph(ix)) continue;
            sma_piece(ix, d, size, ui_buf(size, size));
            t[d][ix] = ui_tex(size, size, ui_buf(size, size));
        }
}

static void layout(box b)
{
    room = b;
    float side = fminf(b.w, b.h);
    int s = (int)floorf(side / (9 + 2 * FRAME + 0.3f));
    while (s > 4 && sma_board(NULL, 9, s, (int)(s * FRAME)) > side) s--;
    sq = s;
    fr = (int)(sq * FRAME);
    int n = sma_board(NULL, 9, sq, fr), m = (n - 9 * sq) / 2;
    bx = floorf(b.x + (b.w - n) / 2) + m;
    by = floorf(b.y + (b.h - n) / 2) + m;
    ui_board_used(bx - m, by - m, (float)n, (float)n);
    if (made != sq) {
        made = sq;
        make_set(t_pc, sq);
        ui_free(&t_board);
        sma_board(ui_buf(n, n), 9, sq, fr);
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
    *x = bx + SG_COL(s) * sq;
    *y = by + SG_ROW(s) * sq;
}

static int xy_sq(float x, float y)
{
    int c = (int)floorf((x - bx) / sq), r = (int)floorf((y - by) / sq);
    return c < 0 || c > 8 || r < 0 || r > 8 ? -1 : SG_SQ(c, r);
}

/* a piece; lift 0..1: how high it is held (its shadow falls further off and fainter: picked up, sliding) */
static void piece_lift(int pc, float x, float y, float size, int alpha, float lift)
{
    SDL_Texture *t = t_pc[SG_OWNER(pc)][sma_tex_of(pc)];
    float off = (0.03f + 0.09f * lift) * size;
    ui_shadow_flat(x + size / 2 + off, y + size / 2 + 1.6f * off, 0.86f * size * sma_width(SG_KIND(pc)), 0.9f * size,
                   (0.75f - 0.3f * lift) * alpha / 255);
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    ui_put(t, x, y, size, size);
    SDL_SetTextureAlphaMod(t, 255);
}
static void piece_at(int pc, float x, float y, float size, int alpha) { piece_lift(pc, x, y, size, alpha, 0); }

static void pick_at(int i, float *x, float *y)  /* the picker: the promoted piece on the square, the other below */
{
    sq_xy(promo.to, x, y);
    *y += i * sq;
    if (SG_ROW(promo.to) == 8) *y -= sq;
}

static void draw(void)
{
    if (!t_board) layout(room);
    Uint32 now = ui_ms();
    if (wrong.on && now - wrong.t0 >= wrong_end()) {   /* the mistake and its answer go back */
        wrong.on = 0;
        smt_undo(&G);
        memset(shown_at, 0, sizeof shown_at);
        refresh();
    }
    int k = shown(now);
    float f = k && now < shown_at[k] + ANIM_MS ? (now - shown_at[k]) / (float)ANIM_MS : 1, x, y;
    const spos *p = &G.pos[k];
    smove mv = k ? G.played[k - 1] : (smove){0, 0, 0, 0};
    int n = sma_board(NULL, 9, sq, fr), m = (n - 9 * sq) / 2;
    ui_put(t_board, bx - m, by - m, (float)n, (float)n);
    if (k) {                                    /* the last move */
        if (!mv.drop) { sq_xy(mv.from, &x, &y); ui_mark_last(x, y, sq, sq); }
        sq_xy(mv.to, &x, &y);
        ui_mark_last(x, y, sq, sq);
    }
    if (k % 2 == 1 && f >= 1 && sg_king(p, 1) >= 0) {   /* the king in check */
        sq_xy(sg_king(p, 1), &x, &y);
        ui_mark_glow(x + sq / 2.0f, y + sq / 2.0f, 1.6f * sq);
    }
    if (wrong.on)                               /* the mistake and its answer, in red */
        for (int i = G.ply - 2; i < G.ply && i < k; i++) {
            sq_xy(G.played[i].to, &x, &y);
            ui_mark_glow(x + sq / 2.0f, y + sq / 2.0f, 1.5f * sq);
        }
    int calm_now = k == G.ply && f >= 1 && !wrong.on;
    if (calm_now && G.hint_level && smt_to_find(&G)) {
        smove h = smt_expected(&G);
        if (!h.drop) {
            sq_xy(h.from, &x, &y);
            ui_mark_gold(x + sq / 2.0f, y + sq / 2.0f, 1.5f * sq);
        } else if (hand_at[h.drop].w) {
            box b = hand_at[h.drop];
            ui_mark_gold(b.x + b.h / 2, b.y + b.h / 2, 1.2f * b.h);
        }
        if (G.hint_level >= 2) {
            sq_xy(h.to, &x, &y);
            ui_mark_gold(x + sq / 2.0f, y + sq / 2.0f, 1.5f * sq);
        }
    }
    if (calm_now && sel >= 0) { sq_xy(sel, &x, &y); ui_mark_select(x, y, sq, sq); }
    for (int s = 0; s < 81; s++) {
        if (!p->sq[s] || (f < 1 && s == mv.to) || (drag.on && drag.moved && s == drag.from)) continue;
        sq_xy(s, &x, &y);
        piece_at(p->sq[s], x, y, (float)sq, 255);
    }
    if (f < 1) {                                /* the move sliding (a drop set down from a little above) */
        float t = puz_ease(f), x1, y1;
        sq_xy(mv.to, &x1, &y1);
        if (mv.drop) {
            float z = 1 + 0.22f * (1 - t);
            piece_lift(p->sq[mv.to], x1 + sq * (1 - z) / 2, y1 + sq * (1 - z) / 2, sq * z, (int)(255 * t), 1 - t);
        } else {
            float x0, y0;
            sq_xy(mv.from, &x0, &y0);
            piece_lift(p->sq[mv.to], x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, (float)sq, 255, sinf(3.14159265f * t));
        }
        ui_animate();
    } else if (k < G.ply) ui_animate();
    if (calm_now && (sel >= 0 || hand_sel) && !promo.on) {   /* where the chosen piece gives check */
        uint8_t done[81] = {0};
        for (int i = 0; i < nch; i++) {
            smove c = ch[i];
            if (done[c.to] || (hand_sel ? c.drop != hand_sel : c.drop || c.from != sel)) continue;
            done[c.to] = 1;
            sq_xy(c.to, &x, &y);
            if (p->sq[c.to]) ui_mark_ring(x + sq / 2.0f, y + sq / 2.0f, sq);
            else ui_mark_dot(x + sq / 2.0f, y + sq / 2.0f, 0.32f * sq);
        }
    }
    if (drag.on && drag.moved && drag.from >= 0) {
        float s = 1.18f * sq;
        piece_lift(p->sq[drag.from], drag.x - s / 2, drag.y - 0.75f * s, s, 255, 1);
    }
    if (promo.on) {
        ui_fill(bx - m, by - m, (float)n, (float)n, 0, 0, 0, 110);
        float px, py;
        pick_at(0, &px, &py);
        ui_put(t_pick, px, py, (float)sq, 2.0f * sq);
        int pc = p->sq[promo.from];
        for (int i = 0; i < 2; i++) {
            pick_at(i, &px, &py);
            piece_at(i ? pc : pc | SG_PROMOTED, px, py, (float)sq, 255);
        }
    }
    if (win.on) {                               /* the gold light from the mating piece */
        float e = (float)now - (float)win.t0;
        int from = G.played[G.ply - 1].to;
        for (int s = 0; s < 81 && e >= 0; s++) {
            int d = abs(SG_COL(s) - SG_COL(from)), r = abs(SG_ROW(s) - SG_ROW(from));
            float q = puz_pulse(e, (d > r ? d : r) * WAVE_STEP, WAVE_LEN);
            if (q <= 0) continue;
            sq_xy(s, &x, &y);
            ui_mark_gold_a(x + sq / 2.0f, y + sq / 2.0f, 1.2f * sq, 0.7f * q);
        }
        if (e < WAVE_END) ui_animate();
        else win.on = 0;
    }
    if (wrong.on) ui_animate();
    ui_tool(0, ICON_HINT, (G.hint_level ? UI_ON : 0) | (G.hint_level >= 2 || !smt_to_find(&G) ? UI_DIM : 0));
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    drag.on = 0;
    if (ui_tool_hit(x, y) == 0) { hint(); return; }
    if (busy() || !smt_to_find(&G)) return;
    if (promo.on) {
        for (int i = 0; i < 2; i++) {
            float px, py;
            pick_at(i, &px, &py);
            if (x >= px && x < px + sq && y >= py && y < py + sq) {
                int yes, no;
                find(promo.from, 0, promo.to, &yes, &no);
                promo.on = 0;
                attempt(ch[i ? no : yes], 1);
                return;
            }
        }
        promo.on = 0;
        return;
    }
    int s = xy_sq(x, y);
    if (s < 0) { sel = -1; hand_sel = 0; return; }
    if (hand_sel || (sel >= 0 && s != sel && can_go(sel, 0, s))) {
        if (hand_sel && !can_go(0, hand_sel, s)) { ui_sound(SND_DENY); hand_sel = 0; return; }
        move_to(sel, hand_sel, s, 1);
        return;
    }
    const spos *p = smt_pos(&G);
    if (p->sq[s] && !SG_OWNER(p->sq[s])) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (sel >= 0) ui_sound(SND_DENY);
    sel = -1;
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (busy() || !smt_to_find(&G) || promo.on) return 0;
    for (int k = SG_PAWN; k <= SG_ROOK; k++) {
        box b = hand_at[k];
        if (!b.w || x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
        sel = -1;
        hand_sel = hand_sel == k ? 0 : k;
        ui_sound(SND_TAP);
        ui_animate();
        return 1;
    }
    return 0;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > sq * 0.15f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays chosen */
    int s = xy_sq(x, y);
    if (s < 0 || s == drag.from) return;
    if (!can_go(drag.from, 0, s)) { ui_sound(SND_DENY); return; }
    move_to(drag.from, 0, s, 0);
}

/* ---- the bar: the pieces in hand, the moves left to mate --------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    static const int order[7] = {SG_ROOK, SG_BISHOP, SG_GOLD, SG_SILVER, SG_KNIGHT, SG_LANCE, SG_PAWN};
    const spos *p = smt_pos(&G);
    memset(hand_at, 0, sizeof hand_at);
    float u = ui.unit, cy = b.y + b.h / 2, ks = 0.7f * u;
    int base = G.failed ? G.ply - 2 : G.ply, left = smt_solved(&G) ? 0 : G.pz.n - (base & ~1);   /* both sides' */
    float right = b.x + b.w - 0.1f * u;          /* gote's king, the moves left after it */
    ui_number(right - 0.25f * u, cy, 0.55f * u, left, 1);
    ui_put(t_small[1][SMA_KING1], right - 0.55f * u - ks, cy - ks / 2, ks, ks);
    float room_w = b.w - 1.4f * u;
    int kinds = 0;
    for (int i = 0; i < 7; i++) kinds += p->hand[0][order[i]] > 0;
    if (!kinds) return;
    float cw = fminf(1.05f * u, room_w / kinds), ps = fminf(0.74f * u, cw / 1.4f), x = b.x;
    for (int i = 0; i < 7; i++) {
        int k = order[i], n = p->hand[0][k];
        if (!n) continue;
        hand_at[k] = (box){x, b.y, cw, b.h};
        if (hand_sel == k) ui_mark_select(x, cy - ps / 2, ps, ps);
        ui_shadow_flat(x + ps / 2, cy, 0.86f * ps * sma_width(k), 0.9f * ps, 0.75f);
        ui_put(t_small[0][k], x, cy - ps / 2, ps, ps);
        if (n > 1) ui_number(x + 1.2f * ps, cy + 0.05f * ps, 0.5f * ps, n, 1);
        x += cw;
    }
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    sma_piece(SG_KING, 0, size, rgba);
}

/* the menu's card: gote's king in its corner, a gold set down before it in gold light, a silver behind */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    static const uint8_t GOLD[3] = {255, 196, 64}, RED[3] = {235, 40, 30};
    int k = 4, s = (int)(size / (k + 2 * FRAME + 0.3f)), f = (int)(s * FRAME), n = sma_board(NULL, k, s, f);
    while (n > size && s > 4) n = sma_board(NULL, k, --s, f);
    int o = (size - n) / 2, m = (n - k * s) / 2;
    uint8_t *board = ui_buf(n, n);
    sma_board(board, k, s, f);
    art_blend(rgba, size, size, board, n, n, o, o);
    int d = (int)(1.7f * s);
    uint8_t *g = malloc((size_t)d * d * 4);
    if (g) {
        art_disc(d, RED, 0.7f, 1.3f, g);
        art_blend(rgba, size, size, g, d, d, o + m + 2 * s + s / 2 - d / 2, o + m + s / 2 - d / 2);
        art_disc(d, GOLD, 0.9f, 1.2f, g);
        art_blend(rgba, size, size, g, d, d, o + m + 2 * s + s / 2 - d / 2, o + m + s + s / 2 - d / 2);
        free(g);
    }
    static const int at[4][4] = {{2, 0, SMA_KING1, 1}, {2, 1, SG_GOLD, 0}, {1, 2, SG_SILVER, 0}, {3, 0, SG_LANCE, 1}};
    for (int i = 0; i < 4; i++) {
        uint8_t *one = ui_buf(s, s);
        sma_piece(at[i][2], at[i][3], s, one);
        art_blend(rgba, size, size, one, s, s, o + m + at[i][0] * s, o + m + at[i][1] * s);
    }
}

/* a chapter's tab: gote's king, as many gold pips below as the chapter's number */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const uint8_t GOLD[3] = {255, 196, 64};
    int d = (int)(0.74f * size);
    uint8_t *one = malloc((size_t)d * d * 4);
    if (!one) return;
    memset(one, 0, (size_t)d * d * 4);
    sma_piece(SMA_KING1, 0, d, one);
    art_blend(rgba, size, size, one, d, d, (size - d) / 2, 0);
    int n = chapter + 1, pd = (int)fmaxf(3, 0.13f * size), gap = pd / 3, w = n * pd + (n - 1) * gap;
    art_disc(pd, GOLD, 1, 0, one);
    for (int i = 0; i < n; i++) art_blend(rgba, size, size, one, pd, pd, size / 2 - w / 2 + i * (pd + gap), size - pd);
    free(one);
}

static const game shogimate_game = {
    .id = "shogimate", .order = 432, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .bar_press = bar_press, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 1,
    .levels = SMT_LEVELS, .chapters = SMT_CHAPTERS, .chapter_icon = chapter_icon, .level_stars = level_stars,
    .score = score,
};
REGISTER_GAME(shogimate_game)
