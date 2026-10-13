/*
 * Spread's bot (src/games/spread_bot.c), every board: its bitboard moves, positions and ends against the rules' on
 * random games, its registry entry, the start at every level, positions it must get right, the cancel, then
 * self-play at every level on each board (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (spread_live(), spread_show()). --full for all the games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"

#include "games/spread.h"

int sdb_agrees(const spdpos *r, int over, int diff);
int sdb_choose(const spdpos *r, int level, uint64_t seed, int *from, int *to, ai_stats *stats);

static spdgame T;                               /* the "screen's" game: the bot reads it */
static int board;
const spdgame *spread_live(void) { return &T; }

static int one(const char *s) { return strlen(s) == 4 && spd_script(&T, s); }
int spread_show(const char *move) { return one(move); }

static void reset(void) { spd_new(&T, board); }
static int actor(void) { return spd_pos(&T)->side; }
static int result(void)
{
    int s = spd_status(&T);
    return s == SPD_PLAY ? -1 : s == SPD_WIN0 ? 0 : s == SPD_WIN1 ? 1 : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    spdmove l[SPD_MAX_MOVES];
    int n = spd_legal(spd_pos(&T), l);
    if (!n || cap < 5) return 0;
    spdmove x = l[ai_rand_n(r, n)];
    m[0] = (char)('a' + x.from % SPD_N);
    m[1] = (char)('1' + x.from / SPD_N);
    m[2] = (char)('a' + x.to % SPD_N);
    m[3] = (char)('1' + x.to / SPD_N);
    m[4] = 0;
    return 1;
}

static int choose(int level, ai_stats *s)
{
    int f, t;
    return sdb_choose(spd_pos(&T), level, 5, &f, &t, s);
}

/* a position on the plain board: player 0's pieces, player 1's ("a1 b2"), the side to move */
static void setup(const char *p0, const char *p1, int side)
{
    spd_new(&T, 0);
    spdpos *p = &T.pos[0];
    memset(p->c, SPD_EMPTY, sizeof p->c);
    const char *list[2] = {p0, p1};
    for (int k = 0; k < 2; k++)
        for (const char *s = list[k]; *s; s++)
            if (*s >= 'a' && *s <= 'g') {
                p->c[spd_cell(s)] = (uint8_t)(SPD_P0 + k);
                s++;
            }
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    char name[160];
    static const sp_game G = {
        .id = "spread", .max_plies = SPD_MAX_PLY + 2, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {   /* the bitboards are the rules: every position of random games on every board, the ends included */
        ai_rng r;
        ai_seed(&r, 3);
        int same = 1, positions = 0, ends = 0;
        for (int gn = 0; gn < 2000 && same; gn++) {
            board = gn % SPD_BOARDS;
            reset();
            for (;;) {
                const spdpos *p = spd_pos(&T);
                int over = result() >= 0, me = p->side;
                same = sdb_agrees(p, over, spd_score(&T, me) - spd_score(&T, !me));
                positions++;
                if (over || !same) { ends += over; break; }
                char m[8];
                random_move(&r, m, sizeof m);
                one(m);
            }
        }
        snprintf(name, sizeof name, "bitboard moves, positions and ends = the rules' (%d positions, %d ends)",
                 positions, ends);
        st_check(name, same);
    }
    for (board = 0; board < SPD_BOARDS; board++) st_basics("spread", &G, spread_show, 2);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* a jump to c3 turns all five: b2 b3 b4 c2 d2 */
        setup("a1 a2 g7", "b2 b3 b4 c2 d2 g1", 0);
        int f, t;
        all &= sdb_choose(spd_pos(&T), l, 5, &f, &t, NULL) && t == spd_cell("c3");
    }
    st_check("every level jumps where it turns five", all);
    all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* ebony has one piece left, which a1's clone to b1 turns: the end */
        setup("a1 a2 a3 g7", "c1", 0);
        const char *m = st_move("spread", 0, l);
        all &= m[0] && one(m) && result() == 0;
    }
    st_check("every level takes the last piece", all);
    for (board = 0; board < SPD_BOARDS; board++) {
        printf("     board %d:\n", board);
        st_levels(argc, argv, &G, 6, choose);
    }
    if (st_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 80},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 75},
        {AI_HARD, AI_MEDIUM, 40, 55},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    int fails = 0;
    for (board = 0; board < SPD_BOARDS; board++) {
        printf("== board %d\n", board);
        fails += st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return fails != 0;
}
