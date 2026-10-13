/*
 * Gomoku's bot (src/games/gomoku_bot.c): its registry entry, threats (a row completed, a four blocked, an open three
 * stopped, a win by continuous fours found), renju's forbidden points never played, the cancel, the levels' work,
 * then self-play at every level in the four variants (tests/ai_selfplay.h). Headless: this file plays the screen's
 * part (gomoku_live(), gomoku_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/gomoku.h"

static gkgame T;                                /* the "screen's" game: the bot reads it */
static int variant;
const gkgame *gomoku_live(void) { return &T; }
int gomoku_show(const char *move)
{
    int len, p = gk_point(T.n, move, &len);
    return p >= 0 && !move[len] && gk_play(&T, p);
}

static void reset(void) { gk_new(&T, variant); }
static int actor(void) { return gk_side(&T); }
static int result(void)
{
    int s = gk_status(&T);
    return s == GK_PLAY ? -1 : s == GK_WIN0 ? 0 : s == GK_WIN1 ? 1 : 2;
}

static void name(int p, char *m, int cap) { snprintf(m, (size_t)cap, "%c%d", 'a' + p % T.n, p / T.n + 1); }

static int random_move(ai_rng *r, char *m, int cap)
{
    int legal[GK_MAXPTS], n = 0;
    for (int p = 0; p < T.n * T.n; p++)
        if (gk_allowed(&T, p)) legal[n++] = p;
    if (!n) return 0;
    name(legal[ai_rand_n(r, n)], m, cap);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* every level from `lo` answers the position (variant v, moves s) with one of the moves in `want` (space separated) */
static int answers(int v, const char *s, int lo, const char *want)
{
    const bot *b = bot_find("gomoku");
    int all = 1;
    for (int l = lo; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP];
        gk_new(&T, v);
        if (!gk_script(&T, s)) { printf("     bad script %s\n", s); return 0; }
        int ok = b->think(gk_side(&T), l, m, sizeof m);
        char pad[80];
        snprintf(pad, sizeof pad, " %s ", m);
        char w[160];
        snprintf(w, sizeof w, " %s ", want);
        if (!ok || !strstr(w, pad)) { printf("     %s: %s (want %s)\n", sp_name(l), ok ? m : "none", want); all = 0; }
    }
    return all;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("gomoku");
    check("registered: bot_find(\"gomoku\")", b && b->think && b->play == gomoku_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], nm[160];
    /* ebony h8 i8 j8 k8 (a four, both ends open... l8 or g8 wins) */
    check("every level completes a row", answers(GK_FREESTYLE, "h8 h9 i8 i9 j8 j9 k8 a1", AI_EASY, "g8 l8"));
    check("every level blocks a four", answers(GK_FREESTYLE, "h8 h9 i8 i9 j8 a1 k8", AI_EASY, "g8 l8"));
    /* ivory to move: ebony's open three h8 i8 j8 (both ends free): ivory stops it at an end (or one further) */
    check("medium and hard stop an open three", answers(GK_FREESTYLE, "h8 h10 i8 c3 j8", AI_MEDIUM, "g8 k8 f8 l8"));
    /* a win by fours: ebony's broken lines; hard finds the sequence */
    {
        gk_new(&T, GK_FREESTYLE);
        gk_script(&T, "h8 a1 i9 a3 j10 a5 h10 a7 h11 a9");   /* ebony: a diagonal of three, a column h8 h10 h11 */
        int ok = b->think(0, AI_HARD, m, sizeof m) && gomoku_show(m);
        /* play it out: ivory blocks the four each time (hard as ivory), ebony must win soon */
        for (int k = 0; k < 16 && ok && result() < 0; k++) ok = b->think(gk_side(&T), AI_HARD, m, sizeof m) &&
                                                               gomoku_show(m);
        check("hard wins a position with a winning attack", ok && result() == 0);
    }
    /* standard: ebony's g8 h8 i8 j8 _ l8: k8 makes six (no win), f8 makes exactly five */
    check("standard: every level wins with exactly five, not six",
          answers(GK_STANDARD, "g8 a1 h8 a3 i8 a5 j8 a7 l8 a9", AI_EASY, "f8"));
    {   /* renju: a hundred games of hard against random, every ebony stone allowed (gk_play checks) */
        ai_rng r;
        ai_seed(&r, 11);
        int ok = 1, games = 0;
        for (int k = 0; k < 6 && ok; k++, games++) {
            gk_new(&T, GK_RENJU);
            while (result() < 0 && ok) {
                if (gk_side(&T) == 0) ok = b->think(0, k & 1 ? AI_MEDIUM : AI_EASY, m, sizeof m) && gomoku_show(m);
                else ok = random_move(&r, m, sizeof m) && gomoku_show(m);
            }
        }
        snprintf(nm, sizeof nm, "renju: ebony's bot never plays a forbidden point (%d games)", games);
        check(nm, ok);
    }
    {   /* not its turn: no move; cancelled: no move */
        gk_new(&T, GK_FREESTYLE);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        gk_script(&T, "h8 h9 i8");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(1, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work on positions of games between easy bots (not checked: the calibration, one core) */
        for (int v = 0; v < GK_VARIANTS; v++)
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                double ms = 0, nodes = 0, depth = 0, worst = 0;
                int npos = 8;
                for (int k = 0; k < npos; k++) {
                    gk_new(&T, v);
                    for (int i = 0, n = 4 + 3 * k; i < n && result() < 0; i++) {
                        ai_set_seed((uint64_t)(k * 100 + i));
                        b->think(gk_side(&T), AI_EASY, m, sizeof m);
                        gomoku_show(m);
                    }
                    if (result() >= 0) gk_new(&T, v);
                    ai_stats s;
                    gkb_choose(&T, lv, 5, &s);
                    ms += s.ms;
                    nodes += s.nodes;
                    depth += s.depth;
                    if (s.ms > worst) worst = s.ms;
                }
                printf("     variant %d %-6s %8.0f nodes %6.1f ms (worst %6.1f) depth %4.1f\n", v, sp_name(lv),
                       nodes / npos, ms / npos, worst, depth / npos);
            }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "gomoku", .max_plies = GK_MAXPTS, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = gomoku_show,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 90},
        {AI_MEDIUM, AI_EASY, 100, 55},
        {AI_HARD, AI_EASY, 60, 80},
        {AI_HARD, AI_MEDIUM, 60, 60},
        {AI_HARD, AI_HARD, 20, 0},
    };
    static const char *NAME[GK_VARIANTS] = {"freestyle", "standard", "six in a row", "renju"};
    int bad = 0;
    for (variant = 0; variant < GK_VARIANTS; variant++) {
        printf("-- %s\n", NAME[variant]);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
