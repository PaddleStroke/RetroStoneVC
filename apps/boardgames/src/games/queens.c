/*
 * Eight Queens in RetroStone Board, for one player: N queens on the house board (4 x 4 to 10 x 10), none attacking
 * another (the rules: queens_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The level screen: 150 levels in six chapters (boards of 4 x 4 to 10 x 10 with holes and fixed queens, one solution
 * each), and a die tab for a random game (5 x 5, 6 x 6, 8 x 8 or 10 x 10, one fixed queen, many solutions). The fixed
 * queens are ebony, the holes dark recesses where no queen may stand: tap a square to put an ivory queen on it, tap a
 * queen to take it off. The squares the queens attack are shaded (the queen's lines, a tool, turn the shade off and
 * on); queens that attack each other glow red. The lamp: a square to fill lights up in gold, or, when the queens on the
 * board can no longer all be part of a solution, the one to take off in red. Undo takes back one tap, all the way to
 * the start. The bar: the taps, then (a random game) the solutions found this session out of the board's count. Solved:
 * a gold light spreads from the last queen; a solution found for the first time this session glows in the bar too. The
 * stars: 3 with no hint and no mistake (a queen put where another attacks it), 2 when hints and mistakes together are
 * two at most, else 1. The lamp and the lines are the shell's tools; the board fills its room.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/boardpuz.h"
#include "games/queens.h"
#include "shell.h"

#define POP_MS 160
#define WAVE_STEP 60
#define WAVE_LEN 560

static qn_game G;
static int variant_now = -1;                   /* the random game's variant; -1: a level */
static bpz_board B;
static bpz_pics P;
static box last_box;
static int made_small = -1, shade = 1;
static SDL_Texture *t_small;

/* the session: the solutions found on each board */
static uint64_t found[QN_VARIANTS][724];
static int nfound[QN_VARIANTS];

static struct { int sq; Uint32 t0; } pop = {-1, 0};
static struct { int on, from, fresh; Uint32 t0; } win;
static struct { int sq, remove; } hint = {-1, 0};

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void calm(void)
{
    pop.sq = -1;
    win.on = 0;
    hint.sq = -1;
}

static void start(int level)                    /* a level of the level screen */
{
    variant_now = -1;
    if (!qn_level(&G, level)) qn_empty(&G, 8, -1);   /* never: every level is tested */
    calm();
    win.fresh = 0;
}

static void start_random(int variant)           /* the die tab: a random game */
{
    variant_now = variant < 0 || variant >= QN_VARIANTS ? QN_8 : variant;
    qn_new(&G, variant_now, (uint32_t)ui_random(0x7fffffff) + 1u, found[variant_now], nfound[variant_now]);
    calm();
    win.fresh = 0;
}

static int level_stars(void) { return qn_stars(&G); }

static int status(void) { return qn_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int wave_end(void) { return (G.n - 1) * WAVE_STEP + WAVE_LEN; }

static int busy(void) { return win.on && (int)(ui_ms() - win.t0) < wave_end(); }

/* solved: the light from the last square tapped; the solution noted (new or not) */
static void after(int sq)
{
    if (!qn_solved(&G)) return;
    win.on = 1;
    win.from = sq;
    win.t0 = ui_ms();
    hint.sq = -1;
    ui_animate();
    if (variant_now < 0) return;                /* a level: one solution */
    uint64_t k = qn_key(&G);
    int seen = 0;
    for (int i = 0; i < nfound[variant_now]; i++) seen |= found[variant_now][i] == k;
    win.fresh = !seen;
    if (!seen && nfound[variant_now] < 724) found[variant_now][nfound[variant_now]++] = k;
    hint.sq = -1;
    ui_animate();
}

static void tap(int sq)
{
    if (!qn_toggle(&G, sq)) { ui_sound(SND_DENY); return; }
    hint.sq = -1;
    if (G.q[sq]) {
        pop.sq = sq;
        pop.t0 = ui_ms();
        ui_sound(SND_PLACE);
    } else ui_sound(SND_TAP);
    after(sq);
    ui_animate();
}

static int undo(void)
{
    if (!qn_undo(&G)) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    calm();
    int ok = qn_script(&G, moves);
    after(G.nhist ? G.hist[G.nhist - 1] : G.given > 0 ? G.given : 0);
    return ok;
}

static void ask_hint(void)
{
    if (busy() || qn_solved(&G)) { ui_sound(SND_DENY); return; }
    int v = variant_now;
    hint.sq = v < 0 ? qn_hint(&G, NULL, 0, &hint.remove) : qn_hint(&G, found[v], nfound[v], &hint.remove);
    ui_sound(hint.sq >= 0 ? SND_TAP : SND_DENY);
    ui_animate();
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    last_box = b;
    int n = G.n ? G.n : 8;
    bpz_layout_holes(&B, n, n, b, 1, G.n ? G.hole : NULL);
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        art_piece(QUEEN, WHITE, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

static void draw(void)
{
    if (B.nx != G.n || B.made_holes != bpz_holes_hash(G.n * G.n, G.hole) || !B.board) layout(last_box);
    int n = G.n;
    Uint32 now = ui_ms();
    bpz_draw_board(&B);
    if (shade && !qn_solved(&G))                /* the attacked squares */
        for (int s = 0; s < n * n; s++)
            if (!G.q[s] && !G.hole[s] && qn_attacked(&G, s)) bpz_tint(&B, s % n, s / n, 0, 26, 30, 36, 128);
    for (int s = 0; s < n * n; s++) {
        float x, y;
        bpz_centre(&B, s % n, s / n, 0, &x, &y);
        if (qn_clash(&G, s)) ui_mark_glow(x, y, 1.2f * B.sq);
        if (s == hint.sq) {
            if (hint.remove) ui_mark_ring(x, y, 1.02f * B.sq);   /* take it off: ringed, the queen faded */
            else {
                ui_mark_gold(x, y, 1.5f * B.sq);
                ui_mark_gold(x, y, 1.1f * B.sq);
            }
        }
    }
    if (win.on) {
        float e = (float)now - (float)win.t0;
        for (int s = 0; s < n * n; s++) {
            int d = abs(s % n - win.from % n), r = abs(s / n - win.from / n);
            bpz_glow(&B, s % n, s / n, 0, 0.8f * puz_pulse(e, (d > r ? d : r) * WAVE_STEP, WAVE_LEN));
        }
        if (e < wave_end()) ui_animate();
        else win.on = 0;
    }
    for (int s = 0; s < n * n; s++) {
        if (!G.q[s]) continue;
        float x, y, k = 1;
        bpz_xy(&B, s % n, s / n, 0, &x, &y);
        if (s == pop.sq) {                      /* set down: from a little larger */
            float t = (now - pop.t0) / (float)POP_MS;
            if (t >= 1) pop.sq = -1;
            else {
                k = 1.18f - 0.18f * puz_ease(t);
                ui_animate();
            }
        }
        bpz_piece_at(&B, QUEEN, G.fixed[s] ? BLACK : WHITE, x, y, k, s == hint.sq && hint.remove ? 120 : 255, 1);
    }
    int solved = qn_solved(&G);                 /* the tools: the lamp, the queens' lines */
    ui_tool(0, ICON_HINT, (hint.sq >= 0 ? UI_ON : 0) | (solved ? UI_DIM : 0));
    ui_tool_paint(1, bpz_picture, BPZ_LINES, 0.62f, UI_TINT | (shade ? UI_ON : 0) | (solved ? UI_DIM : 0));
}

static void press(float x, float y)
{
    int t = ui_tool_hit(x, y);
    if (t == 0) { ask_hint(); return; }
    if (t == 1) {                               /* the queens' lines: the shade off and on */
        shade = !shade;
        ui_sound(SND_TAP);
        ui_animate();
        return;
    }
    if (busy() || qn_solved(&G)) return;
    int f, r;
    if (!bpz_at(&B, x, y, 0, &f, &r)) return;
    tap(r * G.n + f);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the bar: the taps; the solutions found of all --------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    float u = ui.unit, cy = b.y + b.h / 2, x = b.x;
    int level = variant_now < 0, total = qn_solutions(G.n), have = level ? 0 : nfound[variant_now];
    float room = b.w;
    if (level) {                                /* a level: the taps */
        puz_bar_moves(b, t_small, G.moves);
        room = 0;
    }
    float w_found = 0.7f * u + bpz_number_w(have), w_total = 0.6f * u + bpz_number_w(total);
    float w_moves = 0.72f * u + bpz_number_w(G.moves) + 0.35f * u;
    /* a narrow bar (landscape): the taps left out first, then the board's count, then the solutions found */
    int with_found = !level && w_found <= room, with_total = w_found + w_total <= room;
    int with_moves = w_moves + w_found + w_total <= room;
    if (with_moves) x = puz_bar_moves(b, t_small, G.moves) + 0.35f * u;
    if (with_found) {
        x = bpz_bar_pic(&P, BPZ_TICK, x, cy, 1);
        if (win.fresh && qn_solved(&G)) ui_mark_gold(x + bpz_number_w(have) / 2, cy, 1.3f * u);
        x = bpz_bar_number(x, cy, have);
    }
    if (with_total) {
        x = bpz_bar_pic(&P, BPZ_SLASH, x + 0.08f * u, cy, 0.8f);
        bpz_bar_number(x, cy, total);
    }
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    art_piece(QUEEN, WHITE, size, rgba);
}

/* the menu's card and the picker's: the board with a solution's queens (the first one found, the given one ebony) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int v = variant < 0 ? QN_8 : variant, n = qn_size(v), ox = 0;
    qn_game T;
    qn_empty(&T, n, -1);
    int rm;
    for (int i = 0; i < n && !qn_solved(&T); i++) {
        int s = qn_hint(&T, NULL, 0, &rm);
        if (s < 0 || rm) break;
        qn_toggle(&T, s);
    }
    int sq = bpz_thumb_board(n, size, rgba, &ox);
    if (!sq) return;
    int d = (int)(0.96f * sq), first = 1;
    for (int s = 0; s < n * n; s++) {
        if (!T.q[s]) continue;
        bpz_thumb_piece(rgba, size, QUEEN, first ? BLACK : WHITE, ox + s % n * sq + (sq - d) / 2,
                        ox + (n - 1 - s / n) * sq + (sq - d) / 2, d);
        first = 0;
    }
}

/* a board in a card: its holes, these queens (the fixed ones ebony) */
static void board_pic(const qn_game *T, int size, uint8_t *rgba)
{
    int n = T->n, ox = 0, oy = 0, sq = bpz_thumb_board_holes(n, n, T->hole, size, rgba, &ox, &oy);
    if (!sq) return;
    int d = (int)(0.96f * sq);
    for (int s = 0; s < n * n; s++)
        if (T->q[s])
            bpz_thumb_piece(rgba, size, QUEEN, T->fixed[s] ? BLACK : WHITE, ox + s % n * sq + (sq - d) / 2,
                            oy + (n - 1 - s / n) * sq + (sq - d) / 2, d);
}

/* a level's tile: its board, holes and fixed queens */
static void level_thumb(int level, int size, uint8_t *rgba)
{
    qn_game T;
    if (qn_level(&T, level)) board_pic(&T, size, rgba);
}

/* a chapter's tab: a small board of its size (5 x 5 for the first, then 6 x 6 .. 10 x 10), a queen in its corner */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int n = 5 + (chapter < 0 ? 0 : chapter > 5 ? 5 : chapter), ox = 0, oy = 0;
    int sq = bpz_thumb_board_holes(n, n, NULL, size, rgba, &ox, &oy), d = (int)(2.2f * sq);
    if (sq) bpz_thumb_piece(rgba, size, QUEEN, BLACK, ox, oy + n * sq - d, d);
}

static const game queens_game = {
    .id = "queens", .order = 432, .variants = QN_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = 2,
    .levels = QN_LEVELS, .chapters = QN_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars,
};
REGISTER_GAME(queens_game)
