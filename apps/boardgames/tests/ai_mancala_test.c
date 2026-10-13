/*
 * Mancala's bot (src/games/mancala_bot.c), Oware and Kalah: its sowing against the rules' on random games, its
 * registry entry, the start at every level, positions it must get right, the cancel, then self-play at every level
 * in each game (tests/ai_selfplay.h). Headless: this file plays the screen's part (mancala_live(), mancala_show()).
 * --full for all the games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"

#include "games/mancala.h"

int mcb_agrees(const mc_game *before, const mc_game *after, int pit);
int mcb_choose(const mc_game *g, int level, uint64_t seed, ai_stats *stats);

static mc_game T, U;                            /* the "screen's" game: the bot reads it */
static int variant = MC_OWARE;
const mc_game *mancala_live(void) { return &T; }

static int one(const char *s)                   /* one pit, "1".."12", as the screen's script() takes it */
{
    char *end;
    long p = strtol(s, &end, 10);
    return end != s && !*end && mc_play(&T, (int)p - 1, NULL);
}
int mancala_show(const char *move) { return one(move); }

static void reset(void) { mc_new(&T, variant); }
static int actor(void) { return mc_turn(&T); }
static int result(void)
{
    int s = mc_status(&T);
    return s == MC_PLAY ? -1 : s == MC_WIN0 ? 0 : s == MC_WIN1 ? 1 : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    int legal[6], n = 0;
    for (int i = 0; i < MC_PITS; i++)
        if (mc_legal(&T, i)) legal[n++] = i;
    if (!n) return 0;
    snprintf(m, (size_t)cap, "%d", legal[ai_rand_n(r, n)] + 1);
    return 1;
}

static int choose(int level, ai_stats *s) { return mcb_choose(&T, level, 5, s) >= 0; }

static void setup(const int *seeds, int turn)   /* the 12 pits, the 2 stores */
{
    mc_new(&T, variant);
    for (int i = 0; i < MC_SLOTS; i++) T.pos[0].seeds[i] = (uint8_t)seeds[i];
    T.pos[0].turn = (uint8_t)turn;
}

int main(int argc, char **argv)
{
    char name[160];
    static const sp_game G = {
        .id = "mancala", .max_plies = MC_MAX + 2, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const char *const NAME[2] = {"Oware", "Kalah"};
    /* the bot's sowing is the rules' (the rules' own end by repetition or by the move limit aside) */
    for (variant = 0; variant < 2; variant++) {
        ai_rng r;
        ai_seed(&r, 11 + (uint64_t)variant);
        int same = 1, positions = 0, ends = 0;
        for (int gn = 0; gn < 3000 && same; gn++) {
            reset();
            while (result() < 0 && same) {
                char m[8];
                random_move(&r, m, sizeof m);
                int pit = atoi(m) - 1;
                U = T;
                one(m);
                int k = mcb_agrees(&U, &T, pit);
                same = k == 1 || (k == 2 && variant == MC_OWARE) || (k == 2 && T.n >= MC_MAX);
                ends += k == 2;
                positions++;
            }
        }
        snprintf(name, sizeof name, "%s: the bot's sowing = the rules' (%d positions; %d ends by repetition)",
                 NAME[variant], positions, ends);
        st_check(name, same);
    }
    variant = MC_OWARE;
    st_basics("mancala", &G, mancala_show, 2);
    variant = MC_KALAH;
    st_basics("mancala", &G, mancala_show, 2);

    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* Kalah: pit 3 ends in the store (an extra turn), then 6 does too */
        variant = MC_KALAH;
        setup((const int[]){0, 0, 4, 0, 0, 1, 3, 3, 3, 3, 3, 3, 0, 0}, 0);
        const char *m = st_move("mancala", 0, l);
        all &= !strcmp(m, "6") || !strcmp(m, "3");
    }
    st_check("Kalah: every level takes the extra turn", all);
    all = 1;
    for (int l = AI_MEDIUM; l <= AI_HARD; l++) {   /* Oware: pit 6 (2 seeds) ends on 8 with 2: takes 3 + 3 */
        variant = MC_OWARE;
        setup((const int[]){4, 4, 4, 3, 4, 2, 2, 2, 5, 4, 4, 4, 0, 0}, 0);
        const char *m = st_move("mancala", 0, l);
        all &= !strcmp(m, "6");
    }
    st_check("Oware: medium and hard take two pits at once", all);
    all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* Oware: ebony has nothing; only pit 5 (2 seeds) reaches it */
        variant = MC_OWARE;
        setup((const int[]){3, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 20, 22}, 0);
        const char *m = st_move("mancala", 0, l);
        all &= !strcmp(m, "5");
    }
    st_check("Oware: every level feeds the other player", all);
    for (variant = 0; variant < 2; variant++) {
        printf("     %s:\n", NAME[variant]);
        st_levels(argc, argv, &G, 6, choose);
    }
    if (st_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 75},
        {AI_MEDIUM, AI_EASY, 100, 60},
        {AI_HARD, AI_EASY, 100, 75},
        {AI_HARD, AI_MEDIUM, 100, 55},
        {AI_EASY, AI_EASY, 20, 0},
        {AI_MEDIUM, AI_MEDIUM, 20, 0},
        {AI_HARD, AI_HARD, 20, 0},
    };
    int fails = 0;
    for (variant = 0; variant < 2; variant++) {
        printf("== %s\n", NAME[variant]);
        fails += st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return fails != 0;
}
