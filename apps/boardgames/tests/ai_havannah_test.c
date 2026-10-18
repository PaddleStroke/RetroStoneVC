/*
 * Ring and Fork's bot (src/games/havannah_bot.c): its registry entry, its quick shapes against the rules' on random
 * positions (every empty cell of each), a few positions (its win taken, the other's win stopped, the swap), the
 * cancel, the levels' work, then self-play at every level on both boards (tests/ai_selfplay.h). Headless: this file
 * plays the screen's part (havannah_live(), havannah_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/havannah.h"

static hvgame T;
static int size = 6, swap_on;
const hvgame *havannah_live(void) { return &T; }
int havannah_show(const char *move) { return strlen(move) > 0 && hv_script(&T, move); }

static void reset(void) { hv_new(&T, size, swap_on); }
static int actor(void) { return hv_turn(&T); }
static int result(void) { return T.winner; }

static int random_move(ai_rng *r, char *m, int cap)
{
    int legal[HV_SQ], n = 0;
    for (int c = 0; c < HV_SQ; c++)
        if (hv_legal(&T, c)) legal[n++] = c;
    if (!n) return 0;
    hv_name(legal[ai_rand_n(r, n)], m, cap);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

static int answers(int n, const char *moves, int lo, const char *want)
{
    const bot *b = bot_find("havannah");
    int all = 1;
    for (int l = lo; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP];
        hv_new(&T, n, 0);
        if (!hv_script(&T, moves)) { printf("     bad moves %s\n", moves); return 0; }
        int ok = b->think(hv_turn(&T), l, m, sizeof m) && !strcmp(m, want);
        if (!ok) printf("     %s: %s (want %s)\n", sp_name(l), m, want);
        all &= ok;
    }
    return all;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("havannah");
    check("registered: bot_find(\"havannah\")", b && b->think && b->play == havannah_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], nm[160];
    {   /* the bot's quick test of a winning stone against the rules', on random positions */
        ai_rng r;
        ai_seed(&r, 7);
        int bad = 0, wins = 0, tried = 0;
        for (int game = 0; game < 300; game++) {
            hv_new(&T, game & 1 ? 8 : 6, game % 4 == 0);
            while (T.winner < 0) {
                int p = hv_turn(&T);
                for (int c = 0; c < HV_SQ; c++) {
                    if (!hv_legal(&T, c)) continue;
                    T.cell[c] = (uint8_t)(p + 1);
                    int rules = hv_shapes(T.n, T.cell, p, c) != 0;
                    T.cell[c] = 0;
                    int quick = hvb_wins(&T, c);
                    tried++;
                    wins += rules;
                    if (rules != quick && bad++ < 5) {
                        char cn[8];
                        hv_name(c, cn, sizeof cn);
                        printf("     game %d, %d moves, %s: the rules %d, the bot %d\n", game, T.nmoves, cn, rules,
                               quick);
                    }
                }
                if (hv_can_swap(&T) && ai_rand_n(&r, 2)) { hv_swap(&T); continue; }
                random_move(&r, m, sizeof m);
                hv_script(&T, m);
            }
        }
        snprintf(nm, sizeof nm, "the bot's shapes are the rules' (%d stones tried, %d winning)", tried, wins);
        check(nm, !bad);
    }
    /* 6: ivory's five round f6 (the middle) and ebony's four up the left; e6 closes the ring */
    check("every level takes the win", answers(6, "g6 b6 g5 b7 f5 b8 e7 b9 f7 b10", AI_EASY, "e6"));
    check("medium and hard stop the other's ring", answers(6, "g6 b6 g5 b7 f5 b8 e7 b9 f7", AI_MEDIUM, "e6"));
    {
        hv_new(&T, 8, 1);
        hv_script(&T, "h8");
        check("hard swaps a central opening", b->think(1, AI_HARD, m, sizeof m) && !strcmp(m, "swap"));
        hv_new(&T, 8, 1);
        hv_script(&T, "h1");
        check("hard keeps a corner opening", b->think(1, AI_HARD, m, sizeof m) && strcmp(m, "swap") != 0);
        hv_new(&T, 8, 0);
        hv_script(&T, "h8");
        check("no swap without the variant", b->think(1, AI_HARD, m, sizeof m) && strcmp(m, "swap") != 0);
    }
    {
        hv_new(&T, 8, 0);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        hv_script(&T, "h8 g8");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work (not checked: the calibration) */
        for (int n = 6; n <= 8; n += 2)
            for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
                int npos = 5;
                ai_rng r;
                ai_seed(&r, 99);
                double ms = 0, nodes = 0, worst = 0;
                for (int k = 0; k < npos; k++) {
                    hv_new(&T, n, 0);
                    for (int i = 0, z = 4 + 6 * k; i < z && T.winner < 0; i++) {
                        random_move(&r, m, sizeof m);
                        havannah_show(m);
                    }
                    if (T.winner >= 0) hv_new(&T, n, 0);
                    ai_stats s;
                    hvb_choose(&T, lv, 5, &s);
                    ms += s.ms;
                    nodes += s.nodes;
                    if (s.ms > worst) worst = s.ms;
                }
                snprintf(nm, sizeof nm, "     %d %-6s %8.0f playouts %6.1f ms (worst %6.1f)", n, sp_name(lv),
                         nodes / npos, ms / npos, worst);
                printf("%s\n", nm);
            }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "havannah", .max_plies = HV_SQ + 2, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = havannah_show,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 80},
        {AI_MEDIUM, AI_EASY, 100, 70},
        {AI_HARD, AI_EASY, 60, 80},
        {AI_HARD, AI_MEDIUM, 60, 55},
    };
    int bad = 0;
    for (int v = 0; v < 3; v++) {
        size = v == 1 ? 8 : 6;
        swap_on = v == 2;
        printf("-- %d%s\n", size, swap_on ? " with the swap" : "");
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
