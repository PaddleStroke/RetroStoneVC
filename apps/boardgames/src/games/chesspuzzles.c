/*
 * Chess Puzzles in RetroStone Board, for one player: a position from a real game, a winning line to find (the rules:
 * chesspuzzles_rules.c, with chess.c's; the collection: chesspuzzles_data.h). The level screen: 150 levels in six
 * chapters on a rising rating (chesspuzzles_levels.h), and a die tab for a random puzzle from one of six packs:
 * beginner, easy, medium, hard, expert (by rating) and the mates (in one, two and three moves).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board turns so the player's side is at the bottom. The opponent's first move comes by itself, then the player
 * finds each of their moves (a tap on a piece then a square, or a drag); the opponent's replies come by themselves.
 * A move off the line is marked red and taken back after a moment (a mistake; any mate is accepted). The lamp in the
 * bar: the piece to move lights up, a second press its square too. Undo takes back the player's last move and its
 * reply, all the way to the start. The bar: the puzzle's rating, the streak (puzzles solved in a row without a mistake
 * or a hint) and the puzzles solved this session. Solved: a gold light spreads from the last move, then the stars: 3
 * with no mistake and no hint, 2 with one slip, else 1.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/boardpuz.h"
#include "games/chesspuzzles.h"
#include "shell.h"

#define ANIM_MS 220                             /* a move sliding */
#define INTRO_MS 650                            /* the position shown before the opponent's first move */
#define REPLY_MS 260                            /* the pause before a reply */
#define WRONG_MS 650                            /* a wrong move held, in red, before it goes back */
#define WAVE_STEP 70
#define WAVE_LEN 620
#define WAVE_END (7 * WAVE_STEP + WAVE_LEN)

static cpz_game G;
static bpz_board B;
static bpz_pics P;
static box last_box, barbox;
static int made_small = -1;
static SDL_Texture *t_small;                    /* the bar's token for the end card */

static int sel = -1;
static cmove legal[MAX_MOVES];
static int nlegal;
static struct { int on[CPZ_MAX_MOVES]; Uint32 at[CPZ_MAX_MOVES]; } anim;   /* move k: when it slides (pos k -> k+1) */
static struct { int on, from, to, slid; Uint32 t0; } wrong;
static struct { int on; Uint32 t0; } win;
static struct { int on, from, moved; float x, y, x0, y0; } drag;

/* the session: the streak, the puzzles solved */
static int streak, solved, counted;

static int flip(void) { return G.solver == BLACK; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = cpz_to_find(&G) ? chess_legal(cpz_pos(&G), legal) : 0;
    sel = -1;
    memset(&drag, 0, sizeof drag);
}

static void calm(void)
{
    memset(&anim, 0, sizeof anim);
    memset(&wrong, 0, sizeof wrong);
    win.on = 0;
    refresh();
}

/* the puzzle just set up in G: shown, then the opponent's first move */
static void begun(void)
{
    counted = 0;
    calm();
    anim.on[0] = 1;                             /* the opponent's first move, after a moment */
    anim.at[0] = ui_ms() + INTRO_MS;
    ui_sound_later(G.played[0].flags & MV_CAPTURE ? SND_CAPTURE : SND_MOVE, INTRO_MS + ANIM_MS);
    if (last_box.w > 0) ui_animate();
}

static void start(int level)                    /* a level of the level screen */
{
    if (!cpz_level(&G, level)) cpz_start_random(&G, CPZ_BEGINNER, 0);   /* never: every level is tested */
    begun();
}

static void start_random(int variant)           /* the die tab: a pack's random puzzle */
{
    int pack = variant < 0 || variant >= CPZ_VARIANTS ? CPZ_EASY : variant;
    cpz_start_random(&G, pack, ui_random(cpz_count(pack) > 0 ? cpz_count(pack) : 1));
    begun();
}

static int level_stars(void) { return cpz_stars(&G); }

static int status(void) { return cpz_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }

/* the move being shown at `now`: k, and f (-1: pos k still waiting; 0..1: move k sliding); k = G.ply: all shown */
static int shown(Uint32 now, float *f)
{
    for (int k = 0; k < G.ply; k++) {
        if (!anim.on[k]) continue;
        if (now < anim.at[k]) { *f = -1; return k; }
        if (now < anim.at[k] + ANIM_MS) { *f = (now - anim.at[k]) / (float)ANIM_MS; return k; }
    }
    *f = 2;
    return G.ply;
}

static Uint32 wrong_end(void) { return (wrong.slid ? 0 : ANIM_MS) + WRONG_MS + ANIM_MS; }

static int busy(void)
{
    Uint32 now = ui_ms();
    float f;
    if (shown(now, &f) < G.ply) return 1;
    if (wrong.on && now - wrong.t0 < wrong_end()) return 1;
    return win.on && (int)(now - win.t0) < WAVE_END;
}

static void tally(void)                         /* a puzzle solved: once */
{
    if (counted) return;
    counted = 1;
    solved++;
    streak = G.mistakes || G.hints ? 0 : streak + 1;
}

static int undo(void)
{
    if (!cpz_undo(&G)) return 0;
    calm();
    return 1;
}

/* the player's moves ("e2e4 g1f3"; the replies by themselves); "#n" first: the pack's puzzle n, or level n + 1 in a
 * game of levels (the pictures) */
static int script(const char *moves)
{
    if (moves[0] == '#') {
        int n = atoi(moves + 1);
        if (G.pack < 0 ? !cpz_level(&G, n) : n < 0 || n >= cpz_count(G.pack) || !cpz_start(&G, G.pack, n)) return 0;
        counted = 0;
        while (*moves && *moves != ' ') moves++;
    }
    int ok = cpz_script(&G, moves);
    calm();
    if (cpz_solved(&G)) {
        tally();
        win.on = 1;
        win.t0 = ui_ms();
    }
    return ok;
}

static int capture_sound(cmove m) { return m.flags & MV_CAPTURE ? SND_CAPTURE : SND_MOVE; }

/* the player's move from -> to; slide: it slides there (a tap), else it is already there (a drag) */
static void attempt(int from, int to, int slide)
{
    int r = cpz_try(&G, from, to);
    Uint32 now = ui_ms();
    if (r == CPZ_ILLEGAL) {
        ui_sound(SND_DENY);
        refresh();
        return;
    }
    if (r == CPZ_WRONG) {
        wrong.on = 1;
        wrong.from = from;
        wrong.to = to;
        wrong.slid = !slide;
        wrong.t0 = now;
        streak = 0;
        ui_sound(SND_MOVE);
        ui_sound_later(SND_DENY, slide ? ANIM_MS : 0);
        refresh();
        ui_animate();
        return;
    }
    int k = G.ply - 1;
    anim.on[k] = slide;
    anim.at[k] = now;
    ui_sound_later(capture_sound(G.played[k]), slide ? ANIM_MS : 0);
    Uint32 lands = now + (slide ? ANIM_MS : 0);
    if (r == CPZ_SOLVED) {
        tally();
        win.on = 1;
        win.t0 = lands;
    } else if (cpz_reply_due(&G)) {
        cmove m = cpz_reply(&G);
        anim.on[k + 1] = 1;
        anim.at[k + 1] = lands + REPLY_MS;
        int delay = (int)(lands + REPLY_MS + ANIM_MS - now);
        ui_sound_later(capture_sound(m), delay);
        if (chess_in_check(cpz_pos(&G), G.solver)) ui_sound_later(SND_CHECK, delay + 60);
    }
    refresh();
    ui_animate();
}

static void hint(void)
{
    if (busy() || !cpz_hint(&G)) { ui_sound(SND_DENY); return; }
    streak = 0;
    sel = -1;
    ui_sound(SND_TAP);
    ui_animate();
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    bpz_layout(&B, 8, b, 1);
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        art_piece(KING, WHITE, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

static void sq_xy(int s, float *x, float *y) { bpz_xy(&B, FILE_OF(s), RANK_OF(s), flip(), x, y); }

static void centre(int s, float *x, float *y)
{
    sq_xy(s, x, y);
    *x += B.sq / 2.0f;
    *y += B.sq / 2.0f;
}

static int sq_at(float x, float y)
{
    int f, r;
    return bpz_at(&B, x, y, flip(), &f, &r) ? SQ(f, r) : -1;
}

static int can_go(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && cpz_lands(legal[i]) == to) return 1;
    return 0;
}

static void draw(void)
{
    if (!B.board) layout(last_box);
    Uint32 now = ui_ms();
    float f;
    int k = shown(now, &f);
    const cpos *p = f < 0 ? &G.pos[k] : &G.pos[k < G.ply ? k + 1 : k];
    int moving = f >= 0 && f <= 1 ? cpz_lands(G.played[k]) : -1;   /* the piece sliding (drawn last) */
    bpz_draw_board(&B);
    /* the marks: the last move shown, the king in check, the selection and its moves, the hint */
    int last = f >= 0 && f <= 1 ? k : f < 0 ? k - 1 : G.ply - 1;
    if (last >= 0) {
        float x, y;
        sq_xy(G.played[last].from, &x, &y);
        ui_mark_last(x, y, (float)B.sq, (float)B.sq);
        sq_xy(cpz_lands(G.played[last]), &x, &y);
        ui_mark_last(x, y, (float)B.sq, (float)B.sq);
    }
    int calm_now = k == G.ply && !(wrong.on && now - wrong.t0 < wrong_end());
    if (calm_now && G.hint_level && cpz_to_find(&G)) {
        cmove m = cpz_expected(&G);
        float x, y;
        centre(m.from, &x, &y);
        ui_mark_gold(x, y, 1.5f * B.sq);       /* twice: a stronger light */
        ui_mark_gold(x, y, 1.1f * B.sq);
        if (G.hint_level >= 2) {
            centre(cpz_lands(m), &x, &y);
            ui_mark_gold(x, y, 1.5f * B.sq);
            ui_mark_gold(x, y, 1.1f * B.sq);
        }
    }
    if (calm_now && sel >= 0) {
        float x, y;
        sq_xy(sel, &x, &y);
        ui_mark_select(x, y, (float)B.sq, (float)B.sq);
    }
    int side = p->side;
    if (chess_in_check(p, side)) {
        float x, y;
        centre(chess_king(p, side), &x, &y);
        ui_mark_glow(x, y, 1.15f * B.sq);
    }
    /* a wrong move: where its piece is now (0..1 from its square to the wrong one) */
    float wt = -1;
    if (wrong.on) {
        float e = (float)(now - wrong.t0), in = wrong.slid ? 0 : ANIM_MS;
        if (e < in) wt = e / in;
        else if (e < in + WRONG_MS) wt = 1;
        else if (e < in + WRONG_MS + ANIM_MS) wt = 1 - (e - in - WRONG_MS) / ANIM_MS;
        else wrong.on = 0;
        if (wt >= 0.999f) {
            float x, y;
            centre(wrong.to, &x, &y);
            ui_mark_glow(x, y, 1.3f * B.sq);
        }
    }
    /* the pieces */
    for (int s = 0; s < 64; s++) {
        int pc = p->sq[s];
        if (!pc || s == moving || (drag.on && drag.moved && s == drag.from)) continue;
        if (wrong.on && wt >= 0 && s == wrong.from) continue;
        if (wrong.on && wt > 0.5f && s == wrong.to) continue;   /* the piece it would take, gone meanwhile */
        float x, y;
        sq_xy(s, &x, &y);
        bpz_piece_at(&B, TYPE(pc), COLOR(pc), x, y, 1, 255);
    }
    if (calm_now && sel >= 0)                   /* where the selected piece can go */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            int to = cpz_lands(legal[i]);
            float x, y;
            centre(to, &x, &y);
            if (p->sq[to] && COLOR(p->sq[to]) != COLOR(p->sq[sel])) ui_mark_ring(x, y, 0.9f * B.sq);
            else ui_mark_dot(x, y, 0.3f * B.sq);
        }
    if (win.on) {                               /* the gold light, from the last move's square */
        float e = (float)now - (float)win.t0;
        int from = cpz_lands(G.played[G.ply - 1]);
        for (int s = 0; s < 64 && e >= 0; s++) {
            int d = abs(FILE_OF(s) - FILE_OF(from)), r = abs(RANK_OF(s) - RANK_OF(from));
            bpz_glow(&B, FILE_OF(s), RANK_OF(s), flip(), 0.8f * puz_pulse(e, (d > r ? d : r) * WAVE_STEP, WAVE_LEN));
        }
        if (e < WAVE_END) ui_animate();
        else win.on = 0;
    }
    if (moving >= 0) {                          /* the sliding piece over the others */
        float x0, y0, x1, y1, t = puz_ease(f);
        sq_xy(G.played[k].from, &x0, &y0);
        sq_xy(moving, &x1, &y1);
        int pc = p->sq[moving];
        if (pc) bpz_piece_at(&B, TYPE(pc), COLOR(pc), x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 1, 255);
        ui_animate();
    } else if (f < 0) ui_animate();
    if (wrong.on && wt >= 0) {
        float x0, y0, x1, y1, t = puz_ease(wt);
        sq_xy(wrong.from, &x0, &y0);
        sq_xy(wrong.to, &x1, &y1);
        int pc = p->sq[wrong.from];
        if (pc) bpz_piece_at(&B, TYPE(pc), COLOR(pc), x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 1, 255);
        ui_animate();
    }
    if (drag.on && drag.moved) {
        int pc = p->sq[drag.from];
        if (pc) bpz_piece_at(&B, TYPE(pc), COLOR(pc), drag.x - B.sq / 2.0f, drag.y - 0.62f * B.sq, 1.12f, 255);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int mine(int s) { return s >= 0 && cpz_pos(&G)->sq[s] && COLOR(cpz_pos(&G)->sq[s]) == G.solver; }

static void press(float x, float y)
{
    memset(&drag, 0, sizeof drag);
    if (busy() || !cpz_to_find(&G)) return;
    int s = sq_at(x, y);
    if (s < 0) return;
    if (sel >= 0 && s != sel && can_go(sel, s)) {   /* a tap on where the selected piece goes */
        attempt(sel, s, 1);
        return;
    }
    if (mine(s)) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.x = drag.x0 = x;
        drag.y = drag.y0 = y;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (sel >= 0) ui_sound(SND_DENY);
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    drag.x = x;
    drag.y = y;
    if (hypotf(x - drag.x0, y - drag.y0) > 0.25f * B.sq) drag.moved = 1;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    int moved = drag.moved, from = drag.from;
    drag.on = 0;
    ui_animate();
    if (!moved) return;                         /* a tap: the piece stays selected */
    int s = sq_at(x, y);
    if (s < 0 || s == from) return;
    if (!can_go(from, s)) { ui_sound(SND_DENY); return; }
    attempt(from, s, 0);
}

/* ---- the bar: the rating, the streak, the puzzles solved; the lamp ----------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    barbox = b;
    float u = ui.unit, cy = b.y + b.h / 2, x = b.x, gap = 0.34f * u, pic = 0.7f * u;
    float room = b.w - (cpz_solved(&G) ? 0 : 1.1f * u);   /* left of the lamp */
    float w_streak = pic + bpz_number_w(streak), w_solved = pic + bpz_number_w(solved);
    float w = bpz_number_w(G.pz.rating) + w_streak + w_solved;
    if (w + 2 * gap > room) gap = 0.12f * u;    /* a narrow bar (landscape): closer, and what does not fit left out */
    int with_streak = bpz_number_w(G.pz.rating) + gap + w_streak <= room, with_solved = w + 2 * gap <= room;
    x = bpz_bar_number(x, cy, G.pz.rating) + gap;
    if (with_streak) {
        x = bpz_bar_pic(&P, BPZ_FLAME, x, cy, 1);
        x = bpz_bar_number(x, cy, streak) + gap;
    }
    if (with_solved) {
        x = bpz_bar_pic(&P, BPZ_TICK, x, cy, 1);
        bpz_bar_number(x, cy, solved);
    }
    if (!cpz_solved(&G)) bpz_button(-1, b, 0, G.hint_level > 0, G.hint_level >= 2);
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    if (cpz_solved(&G) || !bpz_button_hit(barbox, 0, x, y)) return 0;
    hint();
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    art_piece(KING, WHITE, size, rgba);
}

/* the menu's card: a mate to find, its square in gold light; the picker's: the pack's piece (a pawn for beginners ..
 * a queen for experts) with as many gold pips; the mates: the black king in a red glow */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const uint8_t gold[3] = {255, 196, 64}, red[3] = {235, 40, 30};
    int ox = 0, sq;
    if (variant < 0) {
        sq = bpz_thumb_board(5, size, rgba, &ox);
        int d = (int)(1.6f * sq);
        uint8_t *g = malloc((size_t)d * d * 4);
        if (g) {
            art_disc(d, gold, 0.85f, 1.3f, g);
            art_blend(rgba, size, size, g, d, d, ox + sq + sq / 2 - d / 2, ox + sq / 2 - d / 2);   /* the mate */
            free(g);
        }
        static const int at[4][4] = {{KING, BLACK, 3, 0}, {PAWN, BLACK, 2, 1}, {QUEEN, WHITE, 1, 3},
                                     {ROOK, WHITE, 4, 2}};
        for (int i = 0; i < 4; i++)
            bpz_thumb_piece(rgba, size, at[i][0], at[i][1], ox + at[i][2] * sq, ox + at[i][3] * sq, sq);
        return;
    }
    sq = bpz_thumb_board(2, size, rgba, &ox);
    int d = (int)(1.45f * sq);
    if (variant == CPZ_MATES) {
        uint8_t *g = malloc((size_t)d * d * 4);
        if (g) {
            art_disc(d, red, 0.9f, 1.4f, g);
            art_blend(rgba, size, size, g, d, d, size / 2 - d / 2, size / 2 - d / 2);
            free(g);
        }
        bpz_thumb_piece(rgba, size, KING, BLACK, size / 2 - d / 2, size / 2 - d / 2 - sq / 10, d);
        return;
    }
    static const int piece[5] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN};
    bpz_thumb_piece(rgba, size, piece[variant], WHITE, size / 2 - d / 2, size / 2 - d / 2 - sq / 5, d);
    int pd = (int)fmaxf(4, 0.17f * sq), n = variant + 1, gap = pd / 3;
    int w = n * pd + (n - 1) * gap, y = ox + 2 * sq - pd - sq / 10;
    uint8_t *g = malloc((size_t)pd * pd * 4);
    if (!g) return;
    art_disc(pd, gold, 1, 0, g);
    for (int i = 0; i < n; i++) art_blend(rgba, size, size, g, pd, pd, size / 2 - w / 2 + i * (pd + gap), y);
    free(g);
}

/* a chapter's tab: the mates (1 and 3) the black king in a red glow over one or two gold pips; then the knight
 * (material won), the bishop (combinations), the pawn (endgames), the queen (the hardest) */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    static const uint8_t gold[3] = {255, 196, 64}, red[3] = {235, 40, 30};
    static const int piece[CPZ_CHAPTERS] = {KING, KNIGHT, KING, BISHOP, PAWN, QUEEN};
    int c = chapter < 0 ? 0 : chapter >= CPZ_CHAPTERS ? CPZ_CHAPTERS - 1 : chapter, mate = c == 0 || c == 2;
    int d = (int)(0.84f * size), x = (size - d) / 2, y = (size - d) / 2 - (mate ? size / 10 : 0);
    if (mate) {
        uint8_t *g = malloc((size_t)size * size * 4);
        if (g) {
            art_disc(size, red, 0.8f, 1.4f, g);
            art_blend(rgba, size, size, g, size, size, 0, -size / 10);
            free(g);
        }
    }
    bpz_thumb_piece(rgba, size, piece[c], mate ? BLACK : WHITE, x, y, d);
    if (!mate) return;
    int pd = (int)fmaxf(3, 0.16f * size), n = c == 0 ? 1 : 2, gap = pd / 2, w = n * pd + (n - 1) * gap;
    uint8_t *g = malloc((size_t)pd * pd * 4);
    if (!g) return;
    art_disc(pd, gold, 1, 0, g);
    for (int i = 0; i < n; i++) art_blend(rgba, size, size, g, pd, pd, size / 2 - w / 2 + i * (pd + gap), size - pd);
    free(g);
}

static const game chesspuzzles_game = {
    .id = "chesspuzzles", .order = 430, .variants = CPZ_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1,
    .levels = CPZ_LEVELS, .chapters = CPZ_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_stars = level_stars,
};
REGISTER_GAME(chesspuzzles_game)
