/*
 * Corner Blocks' bot (src/games/cornerblocks_bot.c): its registry entry, a move at every level, the seat not to move,
 * the cancel, a position it must get right (the big block that fits rather than a small one), the levels' work, then
 * self-play with two players, and with four (seats 0 and 2 at one level, 1 and 3 at the other; a tie for the best
 * score between the two sides: a draw) (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (cornerblocks_live(), cornerblocks_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"
#include "games/cornerblocks.h"

static cblgame T;
static int players = 2;
const cblgame *cornerblocks_live(void) { return &T; }
static int one(const char *s) { return !cbl_pos(&T)->over && !strchr(s, ' ') && cbl_script(&T, s); }
int cornerblocks_show(const char *move) { return one(move); }

static void reset(void) { cbl_newgame(&T, players); }
static int actor(void) { return cbl_pos(&T)->turn; }

/* the side (seats 0 and 2, or 1 and 3) of the best score; both: a draw */
static int result(void)
{
    const cblpos *p = cbl_pos(&T);
    if (!p->over) return -1;
    unsigned w = cbl_winners(p), even = w & 5u, odd = w & 10u;
    return even && odd ? 2 : odd ? 1 : 0;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    static int mv[CBL_MOVES];
    const cblpos *p = cbl_pos(&T);
    int k = cbl_moves(p, mv, CBL_MOVES);
    if (!k || cap < 8) return 0;
    cbl_name(p, mv[ai_rand_n(r, k)], m);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    memset(s, 0, sizeof *s);
    double t0 = ai_now_ms();
    int m = cblb_choose(cbl_pos(&T), level, 5);
    s->ms = ai_now_ms() - t0;
    s->nodes = 1;
    return m >= 0;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "cornerblocks", .max_plies = CBL_MAX_PLY + 2, .opening = 0, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    st_basics("cornerblocks", &G, cornerblocks_show, 2);
    {   /* medium and hard open with a five, towards the middle */
        int all = 1;
        for (int l = AI_MEDIUM; l <= AI_HARD; l++) {
            reset();
            const char *m = st_move("cornerblocks", 0, l);
            int len, mv = m[0] ? cbl_parse(cbl_pos(&T), m, &len) : -1;
            all &= mv >= 0 && cbl_size(cbl_block(mv)) == 5 && one(m);
        }
        st_check("medium and hard open with a five", all);
    }
    st_levels(argc, argv, &G, 10, choose);
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
    st_levels(argc, argv, &G, 16, choose);
    static const sp_pair P4[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 70},
        {AI_HARD, AI_EASY, 40, 70},
        {AI_HARD, AI_MEDIUM, 40, 55},
    };
    fail |= st_run(&G, P4, (int)(sizeof P4 / sizeof P4[0]), argc, argv);
    return fail;
}
