/*
 * Chess in RetroStone Board: the board, the pieces, tap or drag, the promotion picker (the rules: chess.c). Nine
 * variants: standard chess, Chess960 (the back rank drawn at random), crazyhouse (the pieces taken in the players'
 * bars, dropped back with a tap on one then a square, or dragged), three-check (the checks given in the bars: three
 * slots, a red light for each), king of the hill (the four squares of the hill framed in gold), losing chess (the
 * picker offers a king too), atomic (a capture bursts in a red and gold light, the pieces it blows up fading in
 * it), racing kings (the eighth rank framed in gold, the winning king lit), horde (the pawns taken counted in the
 * bar).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <string.h>

#include "games/chess.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define ANIM_MS 150
#define BOOM_MS 420                             /* atomic: a capture's explosion, after the slide */

enum { V_STANDARD, V_960, V_CRAZYHOUSE, V_THREECHECK, V_HILL, V_LOSING, V_ATOMIC, V_RACING, V_HORDE, VARIANTS };

static cgame G;
static cmove legal[MAX_MOVES];
static int nlegal;
static int sq, fr, made = -1, made_small = -1, made_hand = -1;
static float bx, by;                            /* the a8 corner (h1 when flipped) */
static SDL_Texture *t_piece[2][7], *t_small[2][7], *t_board, *t_pick, *t_pick5;
static SDL_Texture *t_hand[2][7], *t_slot;      /* crazyhouse's pieces in hand; three-check's slot */
static int sel = -1, hand_sel;                  /* the selected square, or the type chosen in hand (crazyhouse) */
static box hand_at[2][7];                       /* the pieces in hand, in the frame each bar was drawn in */
static struct { int on, from, moved, kind; float x, y; } drag;   /* from a square, or (kind) from the hand */
static struct { int on, from, to, pc; Uint32 t0; } anim;   /* pc: the piece moving (atomic: gone once it lands) */
/* atomic: what a capture blew up (the pieces, the square, the piece that took) */
static struct { int on, at, mover; uint64_t gone; uint8_t pc[64]; Uint32 t0; } boom;
static struct { int on, from, to; } promo;
static const int PICK[5] = {QUEEN, KNIGHT, ROOK, BISHOP, KING};   /* the king: losing chess only */

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = chess_legal(game_pos(&G), legal);
    sel = -1;
    hand_sel = 0;
    promo.on = 0;
    drag.on = 0;
}

static void start(int variant)
{
    if (variant == V_960) game_new960(&G, ui_random(960));
    else if (variant >= V_CRAZYHOUSE && variant < VARIANTS) game_new_rules(&G, CR_CRAZYHOUSE + variant - V_CRAZYHOUSE);
    else game_new(&G);
    anim.on = boom.on = 0;
    refresh();
}

static int status(void)
{
    switch (game_status(&G)) {
    case ST_PLAY: return GS_PLAY;
    case ST_CHECKMATE: case ST_CHECKS: case ST_HILL: case ST_EXPLODED: case ST_HORDE:
        return game_pos(&G)->side == WHITE ? GS_WIN1 : GS_WIN0;
    case ST_ANTI: return game_pos(&G)->side == WHITE ? GS_WIN0 : GS_WIN1;   /* nothing left, or stalemated: won */
    case ST_RACE: {                             /* the king on the eighth rank */
        int b = chess_king(game_pos(&G), BLACK);
        return b >= 0 && RANK_OF(b) == 7 ? GS_WIN1 : GS_WIN0;
    }
    default: return GS_DRAW;
    }
}

static int rules(void) { return game_pos(&G)->rules; }
static int npick(void) { return rules() == CR_ANTI ? 5 : 4; }

/* the drop of a piece of that type from the hand onto s; -1 */
static int drop_at(int kind, int s)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].drop == kind && legal[i].to == s) return i;
    return -1;
}

static int turn(void) { return game_pos(&G)->side; }

/* where the moving piece lands: a castling move goes onto the rook, the king lands on g or c */
static int lands(cmove m) { return m.flags & MV_CASTLE ? chess_castle_king(m) : m.to; }

/* the move from one square to another; a castling king also goes onto its destination, when no other move of the
 * king goes there (and it does move) */
static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to && !legal[i].drop) return i;
    if (to == from) return -1;
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && (legal[i].flags & MV_CASTLE) && chess_castle_king(legal[i]) == to) return i;
    return -1;
}

static void play(cmove m, int animate)
{
    const cpos *was = game_pos(&G);
    int moving = was->sq[m.from];
    boom.on = 0;
    if (was->rules == CR_ATOMIC && (m.flags & MV_CAPTURE)) {   /* the pieces it will blow up */
        boom.gone = 0;
        memcpy(boom.pc, was->sq, 64);
        boom.at = m.to;
    }
    game_play(&G, m);
    const cpos *now = game_pos(&G);
    if (now->rules == CR_ATOMIC && (m.flags & MV_CAPTURE)) {
        for (int s = 0; s < 64; s++)
            if (boom.pc[s] && !now->sq[s] && s != m.from) boom.gone |= 1ull << s;
        boom.on = 1;
        boom.mover = moving;
        boom.t0 = ui_ms() + (animate ? ANIM_MS : 0);   /* once it has slid there */
    }
    anim.on = animate;
    anim.from = m.from;
    anim.to = lands(m);
    anim.pc = moving;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.flags & MV_CAPTURE ? SND_CAPTURE : m.drop ? SND_PLACE : SND_MOVE);
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
    boom.on = 0;
    refresh();
    return 1;
}

/* "e2e4 e7e5 e7e8q": the screenshots' and the tests' moves (castling: e1g1, or the king onto its rook; crazyhouse's
 * drops: "N@f3"; losing chess's promotion to a king: e7e8k); animate: each slides as if tapped (the bot's move) */
static int run(const char *s, int animate)
{
    static const char types[] = "PNBRQ";
    while (*s) {
        while (*s == ' ') s++;
        if (s[0] && s[1] == '@') {
            const char *t = strchr(types, s[0]);
            int k = t && s[2] >= 'a' && s[2] <= 'h' && s[3] >= '1' && s[3] <= '8'
                    ? drop_at((int)(t - types) + PAWN, (s[2] - 'a') + (s[3] - '1') * 8) : -1;
            if (k < 0) return 0;
            play(legal[k], animate);
            s += 4;
            continue;
        }
        if (strlen(s) < 4) break;
        int from = (s[0] - 'a') + (s[1] - '1') * 8, to = (s[2] - 'a') + (s[3] - '1') * 8, k = target(from, to);
        if (k < 0) return 0;
        int want = s[4] == 'n' ? KNIGHT : s[4] == 'r' ? ROOK : s[4] == 'b' ? BISHOP : s[4] == 'k' ? KING : QUEEN;
        if (legal[k].promo) for (; k < nlegal && !(legal[k].to == to && legal[k].promo == want); k++) {}
        if (k >= nlegal) return 0;
        play(legal[k], animate);
        s += 4;
        while (*s && *s != ' ') s++;
    }
    return 1;
}

static int script(const char *s) { return run(s, 0); }

/* the moves since the start, in that notation (the shell's game.history) */
static int history(int i, char *move, int cap)
{
    if (move && i >= 0 && i < G.ply) chess_move_text(G.move[i], move, cap);
    return G.ply;
}

/* ---- the bot (chess_bot.c) --------------------------------------------------------------------------------------- */
const cgame *chess_live(void) { return &G; }

/* one move in the notation, played as if tapped (it slides; a promotion needs no picker) */
int chess_show(const char *move)
{
    if (!move || !*move || strchr(move, ' ') || status() != GS_PLAY) return 0;
    return run(move, 1);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
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
        ui_free(&t_pick5);                      /* losing chess: a king too */
        art_roundrect(sq, 5 * sq, 0.22f * sq, pick_c, ui_buf(sq, 5 * sq));
        t_pick5 = ui_tex(sq, 5 * sq, ui_buf(sq, 5 * sq));
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
    int hs = (int)(0.74f * ui.unit);
    if (made_hand != hs) {                      /* the variants' bars: the pieces in hand, the checks' slots */
        made_hand = hs;
        for (int c = 0; c < 2; c++)
            for (int t = PAWN; t <= QUEEN; t++) {
                ui_free(&t_hand[c][t]);
                art_piece(t, c, hs, ui_buf(hs, hs));
                t_hand[c][t] = ui_tex(hs, hs, ui_buf(hs, hs));
            }
        static const uint8_t cream[3] = {246, 238, 220};
        int ss = (int)(0.34f * ui.unit);
        ui_free(&t_slot);
        art_ring(ss, 0.72f, cream, 0.45f, ui_buf(ss, ss));
        t_slot = ui_tex(ss, ss, ui_buf(ss, ss));
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

/* the picker: four squares (losing chess: five) from the promotion square towards the middle of the board */
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

/* king of the hill: the four middle squares framed by a thin line of gold, inside their edge (the same squares turned
 * round: the frame stays put when flipped) */
static void hill(void)
{
    float t = fmaxf(2, roundf(0.045f * sq)), i = roundf(0.06f * sq), x = bx + 3 * sq + i, y = by + 3 * sq + i;
    float w = 2.0f * sq - 2 * i;
    ui_fill(x, y, w, t, 214, 168, 64, 210);
    ui_fill(x, y + w - t, w, t, 214, 168, 64, 210);
    ui_fill(x, y + t, t, w - 2 * t, 214, 168, 64, 210);
    ui_fill(x + w - t, y + t, t, w - 2 * t, 214, 168, 64, 210);
}

/* racing kings: the eighth rank framed by a thin line of gold, inside its edge (the hill's line) */
static void finish(void)
{
    float t = fmaxf(2, roundf(0.045f * sq)), i = roundf(0.06f * sq), x, y;
    sq_xy(ui.flipped ? SQ(7, 7) : SQ(0, 7), &x, &y);
    x += i, y += i;
    float w = 8.0f * sq - 2 * i, h = (float)sq - 2 * i;
    ui_fill(x, y, w, t, 214, 168, 64, 210);
    ui_fill(x, y + h - t, w, t, 214, 168, 64, 210);
    ui_fill(x, y + t, t, h - 2 * t, 214, 168, 64, 210);
    ui_fill(x + w - t, y + t, t, h - 2 * t, 214, 168, 64, 210);
}

/* a piece faded (alpha 0..1) and grown (k: of a square), centred on its square */
static void piece_faded(int pc, int s, float alpha, float k)
{
    float x, y, d = k * sq;
    SDL_Texture *t = t_piece[COLOR(pc)][TYPE(pc)];
    sq_xy(s, &x, &y);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(alpha, 0, 1)));
    piece_at(pc, x + (sq - d) / 2, y + (sq - d) / 2, d);
    SDL_SetTextureAlphaMod(t, 255);
}

/* atomic: the capture's explosion: until the piece taking lands, what it will blow up stands; then a red and gold
 * light bursts from the square and they all fade in it, swelling */
static void draw_boom(void)
{
    int t = (int)(ui_ms() - boom.t0);
    float f = t < 0 ? 0 : clampf(t / (float)BOOM_MS, 0, 1), x, y;
    if (f >= 1) { boom.on = 0; return; }
    ui_animate();
    for (int s = 0; s < 64; s++)
        if (boom.gone >> s & 1) piece_faded(boom.pc[s], s, 1 - f, 1 + 0.25f * f);
    if (t < 0) return;
    piece_faded(boom.mover, boom.at, 1 - f, 1 + 0.25f * f);
    sq_xy(boom.at, &x, &y);
    float e = sinf(3.14159265f * fminf(1, 1.6f * f));
    ui_mark_glow(x + sq / 2.0f, y + sq / 2.0f, (1.2f + 2.4f * f) * sq * e);
    ui_mark_gold_a(x + sq / 2.0f, y + sq / 2.0f, (0.8f + 2.0f * f) * sq, e);
}

static void draw(void)
{
    const cpos *p = game_pos(&G);
    ui_put(t_board, bx - fr, by - fr, 8.0f * sq + 2 * fr, 8.0f * sq + 2 * fr);
    float x, y;
    if (p->rules == CR_HILL) hill();
    if (p->rules == CR_RACING) finish();
    if (G.ply && G.move[G.ply - 1].drop) {     /* a drop: the square it landed on */
        sq_xy(G.move[G.ply - 1].to, &x, &y);
        ui_mark_last(x, y, sq, sq);
    } else if (G.ply) {                         /* a castling king that stays: its rook's square instead */
        cmove m = G.move[G.ply - 1];
        sq_xy(m.from, &x, &y);
        ui_mark_last(x, y, sq, sq);
        sq_xy(lands(m) == m.from ? m.to : lands(m), &x, &y);
        ui_mark_last(x, y, sq, sq);
    }
    if (p->rules == CR_HILL && game_status(&G) == ST_HILL)   /* the king that reached the hill, in gold light */
        for (int s = 0; s < 64; s++)
            if (CHESS_HILL(s) && TYPE(p->sq[s]) == KING) {
                sq_xy(s, &x, &y);
                ui_mark_gold(x + sq / 2.0f, y + sq / 2.0f, 2.2f * sq);
            }
    if (p->rules == CR_RACING && game_status(&G) == ST_RACE)   /* the king home first, in gold light */
        for (int f = 0; f < 8; f++)
            if (TYPE(p->sq[SQ(f, 7)]) == KING) {
                sq_xy(SQ(f, 7), &x, &y);
                ui_mark_gold(x + sq / 2.0f, y + sq / 2.0f, 2.2f * sq);
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
        if (!p->sq[s] || (drag.on && drag.moved && s == drag.from) || (anim.on && s == anim.to)) continue;
        sq_xy(s, &x, &y);
        piece_at(p->sq[s], x, y, (float)sq);
    }
    if (boom.on) draw_boom();
    if (hand_sel && !promo.on)                  /* where the piece chosen in hand can be dropped */
        for (int i = 0; i < nlegal; i++)
            if (legal[i].drop == hand_sel) {
                sq_xy(legal[i].to, &x, &y);
                ui_mark_dot(x + sq / 2.0f, y + sq / 2.0f, 0.32f * sq);
            }
    if (sel >= 0 && !promo.on) {                /* where the selected piece can go */
        uint64_t done = 0;
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            int castle = legal[i].flags & MV_CASTLE, to = legal[i].to;
            if (castle && target(sel, lands(legal[i])) == i) to = lands(legal[i]);   /* the king's square, if it can */
            if (done >> to & 1) continue;
            done |= 1ull << to;
            sq_xy(to, &x, &y);
            if (p->sq[to] && !castle) ui_mark_ring(x + sq / 2.0f, y + sq / 2.0f, sq);
            else ui_mark_dot(x + sq / 2.0f, y + sq / 2.0f, 0.32f * sq);
        }
    }
    if (anim.on) {
        float x0, y0, x1, y1;
        sq_xy(anim.from, &x0, &y0);
        sq_xy(anim.to, &x1, &y1);
        piece_at(p->sq[anim.to] ? p->sq[anim.to] : anim.pc, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, (float)sq);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? -0.25f : 0.75f;
        piece_at(drag.kind ? PC(p->side, drag.kind) : p->sq[drag.from], drag.x - s / 2, drag.y - s * off, s);
    }
    if (promo.on) {
        ui_fill(bx, by, 8.0f * sq, 8.0f * sq, 0, 0, 0, 110);
        float px, py;
        int down = pick_at(0, &px, &py), k = npick();
        ui_put(k == 5 ? t_pick5 : t_pick, px, down ? py : py - (k - 1.0f) * sq, (float)sq, (float)k * sq);
        for (int i = 0; i < k; i++) {
            pick_at(i, &px, &py);
            piece_at(PC(p->side, PICK[i]), px, py, (float)sq);
        }
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (promo.on) {
        for (int i = 0; i < npick(); i++) {
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
    if (s < 0) { sel = -1; hand_sel = 0; return; }
    if (hand_sel) {                             /* the piece chosen in hand, dropped there */
        int k = drop_at(hand_sel, s);
        if (k >= 0) { play(legal[k], 1); return; }
    }
    if (sel >= 0 && target(sel, s) >= 0) {
        move_to(sel, s, 1);
        return;
    }
    const cpos *p = game_pos(&G);
    if (p->sq[s] && COLOR(p->sq[s]) == p->side) {
        sel = s;
        hand_sel = 0;
        drag.on = 1;
        drag.from = s;
        drag.moved = drag.kind = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    if (sel >= 0 || hand_sel) ui_sound(SND_DENY);
    sel = -1;
    hand_sel = 0;
}

/* crazyhouse: a piece in hand chosen for a drop (a second tap puts it back), or dragged onto the board */
static int bar_press(int player, float x, float y)
{
    if (rules() != CR_CRAZYHOUSE || status() != GS_PLAY || promo.on) return 0;
    for (int k = PAWN; k <= QUEEN; k++) {
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
    if (drag.kind) {
        int k = s >= 0 ? drop_at(drag.kind, s) : -1;
        if (k >= 0) play(legal[k], 0);
        else if (s >= 0) ui_sound(SND_DENY);
        return;
    }
    if (s >= 0 && s != drag.from && target(drag.from, s) >= 0) move_to(drag.from, s, 0);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces a player took from the other (a promoted pawn counts as the pawn) */
static void taken(int player, box b);

/* crazyhouse: the pieces in the player's hand, by type, a count beside each type held more than once (as shogi's) */
static void hand_bar(int player, box b)
{
    static const int order[5] = {QUEEN, ROOK, BISHOP, KNIGHT, PAWN};
    const cpos *p = game_pos(&G);
    memset(hand_at[player], 0, sizeof hand_at[player]);
    int kinds = 0;
    for (int i = 0; i < 5; i++) kinds += p->hand[player][order[i]] > 0;
    if (!kinds) return;
    float cw = fminf(1.05f * ui.unit, b.w / kinds), ps = fminf(0.74f * ui.unit, cw / 1.4f), x = b.x;
    float cy = b.y + b.h / 2;
    for (int i = 0; i < 5; i++) {
        int k = order[i], n = p->hand[player][k];
        if (!n) continue;
        hand_at[player][k] = (box){x, b.y, cw, b.h};
        if (hand_sel == k && player == p->side) ui_mark_select(x, cy - ps / 2, ps, ps);
        ui_put(t_hand[player][k], x, cy - ps / 2, ps, ps);
        if (n > 1) ui_number(x + 1.2f * ps, cy + 0.05f * ps, 0.5f * ps, n, 1);
        x += cw;
    }
}

/* three-check: the checks the player gave, three slots at the bar's right end, a red light in each one filled */
static void checks_bar(int player, box b)
{
    float d = 0.34f * ui.unit, step = 0.5f * ui.unit, cy = b.y + b.h / 2, x0 = b.x + b.w - 3 * step + step / 2;
    int n = game_pos(&G)->checks[player];
    for (int i = 0; i < 3; i++) {
        float cx = x0 + i * step;
        if (i < n) ui_mark_glow(cx, cy, 1.5f * d);
        ui_put_c(t_slot, cx, cy, d, d);
    }
    b.w -= 3 * step + 0.1f * ui.unit;           /* the pieces taken: before them */
    taken(player, b);
}

static void bar(int player, box b)
{
    if (rules() == CR_CRAZYHOUSE) hand_bar(player, b);
    else if (rules() == CR_THREECHECK) checks_bar(player, b);
    else taken(player, b);
}

/* counted against the game's start (the horde's 36 pawns, racing kings' army); losing chess: a king too, and a pawn
 * promoted to a king; the horde: a long row of pawns as one pawn and how many */
static void taken(int player, box b)
{
    static const int order[6] = {KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN};
    const cpos *p = game_pos(&G), *p0 = &G.pos[0];
    int c = !player, n[7] = {0}, start_n[7] = {0}, extra = 0, gone[7];
    for (int s = 0; s < 64; s++) {
        if (p->sq[s] && COLOR(p->sq[s]) == c) n[TYPE(p->sq[s])]++;
        if (p0->sq[s] && COLOR(p0->sq[s]) == c) start_n[TYPE(p0->sq[s])]++;
    }
    for (int t = KNIGHT; t <= KING; t++) if (n[t] > start_n[t]) extra += n[t] - start_n[t];
    for (int t = PAWN; t <= KING; t++) gone[t] = start_n[t] > n[t] ? start_n[t] - n[t] : 0;
    gone[PAWN] = gone[PAWN] > extra ? gone[PAWN] - extra : 0;
    float sz = 0.6f * ui.unit, step = 0.36f * ui.unit, x = b.x, cy = b.y + b.h / 2;
    for (int k = 0; k < 6; k++) {
        int t = order[k];
        if (rules() == CR_HORDE && gone[t] > 4) {
            x = ui_bar_number((box){x, b.y, b.x + b.w - x, b.h}, t_small[c][t], gone[t]) + 0.14f * ui.unit;
            continue;
        }
        for (int i = 0; i < gone[t] && x + sz <= b.x + b.w; i++, x += step)
            ui_put(t_small[c][t], x, cy - sz / 2, sz, sz);
        if (gone[t]) x += 0.14f * ui.unit;
    }
}

/* a player's king; the horde's white has none: a pawn (the shell makes the tokens again when a game opens) */
static void token(int player, int size, uint8_t *rgba)
{
    art_piece(rules() == CR_HORDE && player == WHITE ? PAWN : KING, player, size, rgba);
}

/* Chess960's card: a whole board at the start, the back ranks shuffled (the bishops side by side in the corner) */
static void thumb960(int size, uint8_t *rgba)
{
    static const int RANK[8] = {BISHOP, BISHOP, QUEEN, KNIGHT, KNIGHT, ROOK, KING, ROOK};
    static const int ROWS[4] = {0, 1, 6, 7};
    int s = (int)(size / (8 + 2 * FRAME)), f = (int)(s * FRAME), n = 8 * s + 2 * f, o = (size - n) / 2;
    uint8_t *board = ui_buf(n, n);
    art_board(s, f, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    for (int x = 0; x < 8; x++)
        for (int i = 0; i < 4; i++) {           /* black's rank, its pawns, white's pawns, white's rank */
            uint8_t *one = ui_buf(s, s);
            art_piece(i == 1 || i == 2 ? PAWN : RANK[x], i < 2 ? BLACK : WHITE, s, one);
            art_blend(rgba, size, size, one, s, s, o + f + x * s, o + f + ROWS[i] * s);
        }
}

/* a light (a disc fading out: gold, red) centred on (cx, cy), d across, on a card */
static void thumb_light(uint8_t *rgba, int size, const uint8_t rgb[3], float a0, float power, int cx, int cy, int d)
{
    uint8_t *one = ui_buf(d, d);
    art_disc(d, rgb, a0, power, one);
    art_blend(rgba, size, size, one, d, d, cx - d / 2, cy - d / 2);
}

/* the variants' cards: four squares by four of a board, pieces (column, row from the top; type + 8: ebony's), and
 * what tells the rules apart: crazyhouse an ivory knight dropping onto an empty square (its shadow there, ebony pieces
 * gone from the board); three-check an ebony king in check in red light, three red lights above it; the hill the
 * middle four squares framed in gold, the ivory king on them in gold light */
static void thumb_rules(int variant, int size, uint8_t *rgba)
{
    static const uint8_t gold[3] = {255, 196, 64}, red[3] = {235, 40, 30};
    static const int zh[][3] = {{0, 0, ROOK + 8}, {1, 0, KING + 8}, {0, 1, PAWN + 8}, {3, 1, PAWN + 8},
                                {1, 3, KING}, {0, 3, ROOK}, {3, 3, BISHOP}, {1, 2, PAWN}},
                     check[][3] = {{2, 1, KING + 8}, {1, 0, PAWN + 8}, {3, 0, PAWN + 8}, {0, 3, QUEEN}, {3, 3, KING},
                                   {2, 3, ROOK}},
                     hillp[][3] = {{1, 2, KING}, {3, 0, KING + 8}, {0, 0, PAWN + 8}, {2, 3, PAWN}, {0, 3, KNIGHT}};
    int s = (int)(size / (4 + 2 * FRAME)), f = (int)(s * FRAME), n = 4 * s + 2 * f, o = (size - n) / 2, b0 = o + f;
    uint8_t *board = ui_buf(n, n);
    art_board_n(4, 4, s, f, 1, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    const int (*place)[3] = variant == V_CRAZYHOUSE ? zh : variant == V_THREECHECK ? check : hillp;
    int count = variant == V_CRAZYHOUSE ? 8 : variant == V_THREECHECK ? 6 : 5;
    if (variant == V_HILL) {                    /* the hill: a gold frame round the middle four squares */
        int t = s / 14 > 2 ? s / 14 : 2, i = s / 12, x0 = b0 + s + i, w = 2 * s - 2 * i;
        uint8_t *line = ui_buf(w, w);
        memset(line, 0, (size_t)w * w * 4);
        for (int y = 0; y < w; y++)
            for (int x = 0; x < w; x++)
                if (x < t || y < t || x >= w - t || y >= w - t) {
                    uint8_t *q = line + 4 * (y * w + x);
                    q[0] = 214, q[1] = 168, q[2] = 64, q[3] = 230;
                }
        art_blend(rgba, size, size, line, w, w, x0, x0);
        thumb_light(rgba, size, gold, 0.75f, 1.3f, b0 + s + s / 2, b0 + 2 * s + s / 2, 2 * s);
    }
    if (variant == V_THREECHECK) {              /* the king in check, and three checks: three red lights */
        thumb_light(rgba, size, red, 0.9f, 1.5f, b0 + 2 * s + s / 2, b0 + s + s / 2, (int)(1.3f * s));
        for (int i = 0; i < 3; i++)
            thumb_light(rgba, size, red, 1, 0.8f, b0 + s / 2 + i * s / 2, b0 + 2 * s + s / 4, s / 3);
    }
    for (int i = 0; i < count; i++) {
        uint8_t *one = ui_buf(s, s);
        art_piece(place[i][2] & 7, place[i][2] >> 3, s, one);
        art_blend(rgba, size, size, one, s, s, b0 + place[i][0] * s, b0 + place[i][1] * s);
    }
    if (variant == V_CRAZYHOUSE) {              /* the knight in the air over c2, its shadow and a dot below it */
        int d = (int)(0.9f * s);
        uint8_t *sh = ui_buf(d, d);
        art_flat_shadow(d, sh);
        art_blend(rgba, size, size, sh, d, d, b0 + 2 * s + (s - d) / 2 + s / 12, b0 + 2 * s + (s - d) / 2 + s / 8);
        int k = (int)(1.15f * s);
        uint8_t *one = ui_buf(k, k);
        art_piece(KNIGHT, WHITE, k, one);
        art_blend(rgba, size, size, one, k, k, b0 + 2 * s + (s - k) / 2, b0 + 2 * s - (int)(0.55f * s));
    }
}

/* a piece (type + 8: ebony's) on a card's board of k squares a side, s pixels each, from (b0, b0), turned by deg */
static void card_piece(uint8_t *rgba, int size, int b0, int s, float col, float row, int pc, float deg)
{
    uint8_t *one = ui_buf(s, s);
    art_piece(pc & 7, pc >> 3, s, one);
    if (deg) art_blend_rot(rgba, size, size, one, s, s, b0 + (col + 0.5f) * s, b0 + (row + 0.5f) * s, deg);
    else art_blend(rgba, size, size, one, s, s, b0 + (int)(col * s), b0 + (int)(row * s));
}

/* the newer variants' cards: losing chess an ivory king alone among ebony pieces, turned upside down in a gold light
 * (losing is winning); atomic a red and gold burst on the board, pieces round it; racing kings the top rank framed in
 * gold, the two kings side by side, gold arrows taking them up; horde a black army facing rows and rows of ivory
 * pawns */
static void thumb_more(int variant, int size, uint8_t *rgba)
{
    static const uint8_t gold[3] = {255, 196, 64}, red[3] = {235, 60, 30}, orange[3] = {250, 150, 40},
                         yellow[3] = {255, 228, 120}, line[3] = {214, 168, 64};
    int k = variant == V_HORDE ? 5 : 4;
    int s = (int)(size / (k + 2 * FRAME)), f = (int)(s * FRAME), n = k * s + 2 * f, o = (size - n) / 2, b0 = o + f;
    uint8_t *board = ui_buf(n, n);
    art_board_n(k, k, s, f, 1, board);
    art_blend(rgba, size, size, board, n, n, o, o);
    if (variant == V_LOSING) {
        static const int place[][3] = {{0, 0, ROOK + 8}, {2, 0, QUEEN + 8}, {3, 1, KNIGHT + 8}, {0, 1, PAWN + 8},
                                       {1, 1, PAWN + 8}, {3, 3, BISHOP + 8}};
        for (int i = 0; i < 6; i++) card_piece(rgba, size, b0, s, place[i][0], place[i][1], place[i][2], 0);
        thumb_light(rgba, size, gold, 0.85f, 1.2f, b0 + (int)(1.5f * s), b0 + (int)(2.5f * s), 2 * s);
        card_piece(rgba, size, b0, s, 1, 2, KING, 180);
    } else if (variant == V_ATOMIC) {
        static const int place[][3] = {{0, 0, PAWN + 8}, {3, 0, ROOK + 8}, {3, 3, KING}, {0, 3, KNIGHT},
                                       {1, 0, PAWN + 8}};
        for (int i = 0; i < 5; i++) card_piece(rgba, size, b0, s, place[i][0], place[i][1], place[i][2], 0);
        int cx = b0 + (int)(1.5f * s), cy = b0 + (int)(1.6f * s);
        thumb_light(rgba, size, red, 1, 1.1f, cx, cy, (int)(2.8f * s));
        art_fig *fg = art_fig_new(), *fy = art_fig_new();
        float big[32], small[32];
        for (int i = 0; i < 16; i++) {          /* a star of sixteen points, a smaller one in it */
            float a = i * 3.14159265f / 8 + 0.2f, r = i & 1 ? 0.2f : 0.42f, r2 = i & 1 ? 0.1f : 0.24f;
            big[2 * i] = 0.5f + r * cosf(a), big[2 * i + 1] = 0.5f + r * sinf(a);
            small[2 * i] = 0.5f + r2 * cosf(a + 0.2f), small[2 * i + 1] = 0.5f + r2 * sinf(a + 0.2f);
        }
        art_poly(fg, ART_PART, big, 16);
        art_poly(fy, ART_PART, small, 16);
        int w = (int)(2.2f * s);
        uint8_t *b = ui_buf(w, w);
        memset(b, 0, (size_t)w * w * 4);
        art_fig_paint(b, w, fg, orange, red, 0.02f, 0.2f);
        art_fig_paint(b, w, fy, yellow, NULL, 0, 0.1f);
        art_blend(rgba, size, size, b, w, w, cx - w / 2, cy - w / 2);
    } else if (variant == V_RACING) {
        int t = s / 14 > 2 ? s / 14 : 2, i = s / 12, w = 4 * s - 2 * i, h = s - 2 * i;
        uint8_t *fr = ui_buf(w, h);
        memset(fr, 0, (size_t)w * h * 4);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                if (x < t || y < t || x >= w - t || y >= h - t) {
                    uint8_t *q = fr + 4 * (y * w + x);
                    q[0] = line[0], q[1] = line[1], q[2] = line[2], q[3] = 230;
                }
        art_blend(rgba, size, size, fr, w, h, b0 + i, b0 + i);
        static const int place[][3] = {{0, 3, ROOK}, {1, 3, KNIGHT}, {2, 3, BISHOP + 8}, {3, 3, ROOK + 8},
                                       {1, 2, KING}, {2, 2, KING + 8}};
        for (int j = 0; j < 6; j++) card_piece(rgba, size, b0, s, place[j][0], place[j][1], place[j][2], 0);
        art_fig *fa = art_fig_new();
        for (int j = 0; j < 2; j++) {           /* the arrows up, from over each king to the gold rank */
            float x = (1.5f + j) / 4;
            art_band(fa, ART_PART, x, 0.5f, x, 0.3f, 0.045f);
            art_head(fa, ART_PART, x, 0.22f, -90, 0.11f);
        }
        uint8_t *b = ui_buf(4 * s, 4 * s);
        memset(b, 0, (size_t)16 * s * s * 4);
        art_fig_paint(b, 4 * s, fa, gold, line, 0.01f, 0.2f);
        art_blend(rgba, size, size, b, 4 * s, 4 * s, b0, b0);
    } else {
        static const int army[][3] = {{0, 0, ROOK + 8}, {2, 0, KING + 8}, {3, 0, QUEEN + 8}, {1, 1, PAWN + 8},
                                      {4, 1, PAWN + 8}};
        for (int i = 0; i < 5; i++) card_piece(rgba, size, b0, s, army[i][0], army[i][1], army[i][2], 0);
        for (int r = 2; r < 5; r++)
            for (int c = 0; c < 5; c++) card_piece(rgba, size, b0, s, c, r, PAWN, 0);
    }
}

/* the menu's card: a corner of a board, a few pieces */
static void thumb(int variant, int size, uint8_t *rgba)
{
    if (variant == V_960) { thumb960(size, rgba); return; }
    if (variant >= V_LOSING) { thumb_more(variant, size, rgba); return; }
    if (variant >= V_CRAZYHOUSE) { thumb_rules(variant, size, rgba); return; }
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

static int busy(void) { return anim.on || boom.on; }   /* the end waits for the mating piece to arrive, the blast */

static const game chess_game = {
    .id = "chess", .order = 10, .variants = VARIANTS,    /* standard, Chess960, crazyhouse, three-check, the hill,
                                                         * losing, atomic, racing kings, horde */
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .history = history,
};
REGISTER_GAME(chess_game)
