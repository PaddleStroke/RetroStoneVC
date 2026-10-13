/*
 * Hex's bot (src/games/hex_bot.c): its registry entry, a few positions (the win taken, the other's win stopped, a
 * bridge kept, the swap), the cancel, the levels' work, then self-play at every level on the three boards
 * (tests/ai_selfplay.h). Headless: this file plays the screen's part (hex_live(), hex_show()). --full for the 200
 * games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/hex.h"

static hexgame T;                               /* the "screen's" game: the bot reads it */
static int size = 7;
const hexgame *hex_live(void) { return &T; }
int hex_show(const char *move) { return hex_script(&T, move) && strlen(move) > 0; }

static void reset(void) { hex_new(&T, size); }
static int actor(void) { return hex_turn(&T); }
static int result(void) { return T.winner; }

static int random_move(ai_rng *r, char *m, int cap)
{
    int legal[HEX_CELLS], n = 0;
    for (int c = 0; c < T.n * T.n; c++)
        if (hex_legal(&T, c)) legal[n++] = c;
    if (!n) return 0;
    int c = legal[ai_rand_n(r, n)];
    snprintf(m, (size_t)cap, "%c%d", 'a' + c % T.n, c / T.n + 1);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* from `lo` up, every level answers the position (n, moves) with the move `want` */
static int answers(int n, const char *moves, int lo, const char *want)
{
    const bot *b = bot_find("hex");
    int all = 1;
    for (int l = lo; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP];
        hex_new(&T, n);
        if (!hex_script(&T, moves)) { printf("     bad moves %s\n", moves); return 0; }
        int ok = b->think(hex_turn(&T), l, m, sizeof m) && !strcmp(m, want);
        if (!ok) printf("     %s: %s (want %s)\n", sp_name(l), m, want);
        all &= ok;
    }
    return all;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("hex");
    check("registered: bot_find(\"hex\")", b && b->think && b->play == hex_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], nm[160];
    /* 7 x 7: ivory (top-bottom) has d1..d6, ebony c7: d7 wins, ivory to move takes it */
    check("medium and hard take the win", answers(7, "d1 a1 d2 a2 d3 a3 d4 a4 d5 c7 d6 b5", AI_MEDIUM, "d7"));
    /* ebony (left-right) has a4..f4, ivory g3: g4 wins for ebony; ivory to move stops it there */
    check("medium and hard stop the other's win", answers(7, "g3 a4 a2 b4 a3 c4 b1 d4 b2 e4 b3 f4", AI_MEDIUM, "g4"));
    /* the swap: a stone in the middle is swapped, one on the edge is not */
    check("hard swaps a central opening", answers(9, "e5", AI_HARD, "swap"));
    {
        hex_new(&T, 9);
        hex_script(&T, "a1");
        check("hard keeps an edge opening", b->think(1, AI_HARD, m, sizeof m) && strcmp(m, "swap") != 0);
    }
    {   /* not its turn: no move; cancelled: no move, at once */
        hex_new(&T, 11);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        hex_script(&T, "a1 b2");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work (not checked: the calibration, alone on one core) */
        for (int n = 7; n <= 11; n += 2)
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                ai_rng r;
                ai_seed(&r, 99);
                double ms = 0, nodes = 0, worst = 0;
                int npos = 6;
                for (int k = 0; k < npos; k++) {
                    hex_new(&T, n);
                    for (int i = 0, z = 2 + 3 * k; i < z && T.winner < 0; i++) {
                        random_move(&r, m, sizeof m);
                        hex_show(m);
                    }
                    if (T.winner >= 0) hex_new(&T, n);
                    ai_stats s;
                    hxb_choose(&T, lv, 5, &s);
                    ms += s.ms;
                    nodes += s.nodes;
                    if (s.ms > worst) worst = s.ms;
                }
                snprintf(nm, sizeof nm, "     %2d x %-2d %-6s %8.0f playouts %6.1f ms (worst %6.1f)", n, n, sp_name(lv),
                         nodes / npos, ms / npos, worst);
                printf("%s\n", nm);
            }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "hex", .max_plies = HEX_CELLS + 2, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = hex_show,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 85},
        {AI_MEDIUM, AI_EASY, 100, 55},
        {AI_HARD, AI_EASY, 60, 80},
        {AI_HARD, AI_MEDIUM, 60, 60},
        {AI_HARD, AI_HARD, 20, 0},
    };
    int bad = 0;
    for (size = 7; size <= 11; size += 2) {
        printf("-- %d x %d\n", size, size);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
