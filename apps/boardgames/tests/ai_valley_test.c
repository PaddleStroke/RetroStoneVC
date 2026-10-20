/*
 * Valley Harvest's bot (src/games/valley_bot.c): its registry entry, a move at every level, the seat not to move, the
 * cancel, the levels' work (the calibration), then self-play with two players, and with four (seats 0 and 2 at one
 * level, 1 and 3 at the other; a tie for the best score between the two sides: a draw) (tests/ai_selfplay.h).
 * Headless: this file plays the screen's part (valley_live(), valley_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"
#include "games/valley.h"

static vl_game T;
static int players = 2;
static uint32_t game_no;
const vl_game *valley_live(void) { return &T; }
static int one(const char *s)
{
    int len, m = vl_parse(s, &len);
    return m >= 0 && !s[len] && vl_play(&T, m, NULL);
}
int valley_show(const char *move) { return one(move); }

static void reset(void) { vl_begin(&T, players, 0, 7919u * ++game_no + 11u); }
static int actor(void) { return T.s.cur; }

static int result(void)
{
    if (!T.s.over) return -1;
    unsigned w = vl_winners(&T.s), even = w & 5u, odd = w & 10u;
    return even && odd ? 2 : odd ? 1 : 0;
}

/* a random player (that ends its turn often, so its games stay games) */
static int random_move(ai_rng *r, char *m, int cap)
{
    int mv[VL_MAXMOVES], k = vl_moves(&T.s, mv);
    if (!k || cap < 6) return 0;
    int pick = T.s.phase == VL_ACT && ai_rand_n(r, 3) == 0 ? VL_MV_END : mv[ai_rand_n(r, k)];
    vl_name(pick, m);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    memset(s, 0, sizeof *s);
    double t0 = ai_now_ms();
    int m = vlb_choose(&T.s, level, 5);
    s->ms = ai_now_ms() - t0;
    s->nodes = 1;
    return m >= 0;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "valley", .max_plies = VL_HIST, .opening = 0, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    st_basics("valley", &G, valley_show, 2);
    {   /* every level harvests at the start, on a land in its reach */
        int all = 1;
        for (int l = AI_EASY; l <= AI_HARD; l++) {
            reset();
            const char *m = st_move("valley", 0, l);
            int len, mv = m[0] ? vl_parse(m, &len) : -1;
            all &= mv >= 0 && mv < VL_CELLS && vl_harvestable(&T.s, mv, 0) && one(m);
        }
        st_check("every level opens with a harvest", all);
    }
    st_levels(argc, argv, &G, 40, choose);
    if (st_fails) return 1;
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 80},
        {AI_MEDIUM, AI_EASY, 100, 70},
        {AI_HARD, AI_EASY, 60, 75},
        {AI_HARD, AI_MEDIUM, 60, 55},
    };
    int fail = st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    char dump4[300];                            /* --dump FILE: the games of four in FILE.4 */
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--dump")) {
            snprintf(dump4, sizeof dump4, "%s.4", argv[i + 1]);
            argv[i + 1] = dump4;
        }
    players = 4;
    st_levels(argc, argv, &G, 80, choose);
    static const sp_pair P4[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 70},
        {AI_HARD, AI_EASY, 40, 70},
        {AI_HARD, AI_MEDIUM, 40, 55},
    };
    fail |= st_run(&G, P4, (int)(sizeof P4 / sizeof P4[0]), argc, argv);
    return fail;
}
