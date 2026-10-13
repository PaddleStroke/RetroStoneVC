/*
 * Shut the Box's bot (src/games/shutthebox_bot.c), every row and one to four players: its registry entry, the throws
 * it makes ("d??", "d?" once allowed), a few choices, the cancel; alone, how often each level shuts the box; then
 * self-play at every level for each row and count of players (tests/ai_selfplay.h; three or four players: the even
 * players one level, the odd ones the other). Headless: this file plays the screen's part (shutthebox_live(),
 * shutthebox_show(): the faces of a bot's throw from the session's seed). --full for the 200 games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_dice.h"

#include "games/shutthebox.h"

static sgame T;                                 /* the "screen's" game: the bot reads it */
static int V, NP;
const sgame *shutthebox_live(void) { return &T; }

static int one(const char *m)
{
    char f[BOT_MOVE_CAP];
    dice_fill(m, f, sizeof f, T.pos.throws);
    return stb_script(&T, f);
}
int shutthebox_show(const char *move) { return one(move); }

static void reset(void) { stb_new(&T, V, NP); }
static int actor(void) { return T.pos.turn; }
static int result(void) { return T.pos.phase != STB_OVER ? -1 : dice_side(stb_winners(&T.pos)); }

static int random_move(ai_rng *r, char *m, int cap)
{
    const spos *p = &T.pos;
    if (p->phase == STB_THROW) {
        if (stb_one_die(p) && ai_rand_n(r, 2)) snprintf(m, (size_t)cap, "d%d", 1 + ai_rand_n(r, 6));
        else snprintf(m, (size_t)cap, "d%d%d", 1 + ai_rand_n(r, 6), 1 + ai_rand_n(r, 6));
        return 1;
    }
    unsigned open = p->open[p->turn], sets[64];
    int n = 0;
    for (unsigned s = open; s && n < 64; s = (s - 1) & open)
        if (stb_sum(s) == stb_total(p)) sets[n++] = s;
    if (!n) return 0;
    unsigned s = sets[ai_rand_n(r, n)];
    int len = 0;
    m[0] = 0;
    for (int t = 1; t <= STB_MAX_TILES; t++)
        if (s >> t & 1) len += snprintf(m + len, (size_t)(cap - len), len ? "+%d" : "%d", t);
    return 1;
}

/* alone: a game played out by one level; 1: the box shut */
static int solo(const bot *b, int level, uint64_t seed, double *worst)
{
    V = (int)(seed % 3);
    NP = 1;
    reset();
    for (int ply = 0; T.pos.phase != STB_OVER && ply < 200; ply++) {
        char m[BOT_MOVE_CAP];
        ai_set_seed(ai_mix(seed, (uint64_t)ply));
        double t0 = ai_now_ms();
        if (!b->think(0, level, m, sizeof m) || !one(m)) return -1;
        double dt = ai_now_ms() - t0;
        if (dt > *worst) *worst = dt;
    }
    return T.pos.boxed;
}

int main(int argc, char **argv)
{
    int full = dice_full(argc, argv);
    const bot *b = bot_find("shutthebox");
    dice_check("registered: bot_find(\"shutthebox\")", b && b->think && b->play == shutthebox_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP];
    V = STB_NINE, NP = 2;
    reset();
    int ok = 1;
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "d??");
    dice_check("a turn's first throw: two dice, \"d??\", at every level", ok);
    dice_check("not its turn: no move", !b->think(1, AI_HARD, m, sizeof m));
    {   /* tiles 1, 2, 3 open (7 and up shut): medium and hard throw one die */
        reset();
        stb_script(&T, "d63 9 d62 8 d61 7 d42 6 d41 5 d22 4");
        ok = T.pos.turn == 0 && stb_one_die(&T.pos);
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "d?");
        dice_check("1, 2, 3 left: one die (\"d?\"), medium and hard", ok);
    }
    {   /* 1, 4 and 9 left, a throw of 5: 1+4 the only set */
        reset();
        stb_script(&T, "d44 8 d61 7 d42 6 d41 5 d32 2+3 d32");
        ok = T.pos.phase == STB_SHUT && stb_total(&T.pos) == 5;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "1+4");
        dice_check("one set makes the throw: every level shuts it", ok);
    }
    {   /* the last player, with 1 to beat (the other shut all but the 1): only a shut box wins (or a 1 draws).
         * 1, 2, 3, 4 open and a 4 to shut: "4" leaves 1, 2, 3 (one die), "1+3" leaves 2, 4;
         * hard takes the likelier way to the box */
        V = STB_NINE, NP = 2;
        reset();
        stb_script(&T, "d54 9 d44 8 d61 7 d42 6 d41 5 d3 3 d4 4 d2 2 d11");   /* player 0: 1 left, the turn over */
        ok = T.pos.turn == 1 && T.pos.phase == STB_THROW && stb_score(&T.pos, 0) == 1;
        dice_check("the last player has a score of 1 to beat", ok);
        stb_script(&T, "d63 9 d62 8 d61 7 d42 6 d41 5 d22");
        ok = T.pos.phase == STB_SHUT && stb_total(&T.pos) == 4;
        ok &= b->think(1, AI_HARD, m, sizeof m) && stb_script(&T, m);
        printf("     (hard shuts %s with 1, 2, 3, 4 open and a 1 to beat)\n", m);
        dice_check("its choice is legal", ok);
    }
    {   /* cancelled: no move, at once */
        V = STB_TWELVE, NP = 4;
        reset();
        ai_cancel();
        double t0 = ai_now_ms();
        ok = !b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        dice_check("cancelled: no move, at once", ok && dt < 50);
    }
    {   /* alone: the box shut, each level (the luck the same for all: the same seeds) */
        int n = full ? 3000 : 300, shut[AI_LEVELS] = {0};
        double worst = 0;
        ok = 1;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++)
            for (int i = 0; i < n; i++) {
                int s = solo(b, lv, ai_mix(77, (uint64_t)i), &worst);
                ok &= s >= 0;
                shut[lv] += s > 0;
            }
        printf("     alone, %d games each (the three rows): easy %.1f%%, medium %.1f%%, hard %.1f%% shut "
               "(worst %.1f ms)\n", n, 100.0 * shut[0] / n, 100.0 * shut[1] / n, 100.0 * shut[2] / n, worst);
        /* (worked out exactly: the habit shuts 9.56% of rows of 9, the best play 9.76%: the luck decides) */
        dice_check("alone: every move legal; medium and hard shut it more often than easy",
                   ok && shut[1] > shut[0] && shut[2] > shut[0]);
    }
    if (dice_fails) return 1;

    int bad = 0, only = -1;
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--run")) only = atoi(argv[i + 1]);
    static const char *ROWS[] = {"1..9", "1..10", "1..12"};
    int run = 0;
    for (V = 0; V < STB_VARIANTS; V++)
        for (NP = 2; NP <= STB_PLAYERS; NP++, run++) {
            if (only >= 0 && run != only) continue;
            printf("== tiles %s, %d players\n", ROWS[V], NP);
            const sp_game G = {
                .id = "shutthebox", .max_plies = 400, .opening = 0, .reset = reset, .actor = actor,
                .result = result, .random_move = random_move, .script = one,
            };
            const int f = full;          /* luck counts most: the short run (10 games) only checks the play */
            const sp_pair P[] = {
                {AI_EASY, SP_RANDOM, dice_games(200), f ? 52 : 0},
                {AI_MEDIUM, AI_EASY, dice_games(200), f ? 50 : 0},
                {AI_HARD, AI_EASY, dice_games(200), f ? 50 : 0},
                {AI_HARD, AI_MEDIUM, dice_games(200), f ? 45 : 0},
            };
            bad |= sp_run(&G, P, 4, argc, argv);
        }
    return bad;
}
