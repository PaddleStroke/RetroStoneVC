/*
 * Rithmomachia's bot (src/games/rithmomachia_bot.c), every victory: its moves and captures against the rules' on
 * random games, its registry entry, the start at every level, a capture and a harmony it must find, the cancel, then
 * self-play at every level for each victory (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (rithmomachia_live(), rithmomachia_show()). --full for all the games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"

#include "games/rithmomachia.h"

int rhb_gen(const rhmpos *p, int *out);
int rhb_agrees(const rhmpos *r, int variant, rhmmove m);
int rhb_choose(const rhmpos *r, int variant, int level, uint64_t seed, ai_stats *stats);

static rhmgame T;                               /* the "screen's" game: the bot reads it */
static int variant;
const rhmgame *rithmomachia_live(void) { return &T; }

static int one(const char *s) { return !strchr(s, ' ') && rhm_script(&T, s); }
int rithmomachia_show(const char *move) { return one(move); }

static void reset(void) { rhm_new(&T, variant); }
static int actor(void) { return rhm_pos(&T)->side; }
static int result(void)
{
    int s = rhm_status(&T);
    return s == RHM_PLAY ? -1 : s == RHM_WIN0 ? 0 : s == RHM_WIN1 ? 1 : 2;
}

static void name(rhmmove x, char *m)
{
    sprintf(m, "%c%d%c%d", 'a' + x.from % RHM_W, x.from / RHM_W + 1, 'a' + x.to % RHM_W, x.to / RHM_W + 1);
}

static int random_move(ai_rng *r, char *m, int cap)
{
    rhmmove l[RHM_MAX_MOVES];
    int n = rhm_legal(rhm_pos(&T), l);
    if (!n || cap < 8) return 0;
    name(l[ai_rand_n(r, n)], m);
    return 1;
}

static int choose(int level, ai_stats *s) { return rhb_choose(rhm_pos(&T), variant, level, 5, s) >= 0; }

/* a position: pieces by id (0..23 the evens', 24..47 the odds') and square, "1:d4 27:d8"; the side to move */
static void setup(int v, const char *pieces, int side)
{
    variant = v;
    rhm_new(&T, v);
    rhmpos *p = &T.pos[0];
    memset(p->sq, 0, sizeof p->sq);
    for (const char *s = pieces; *s;) {
        char *end;
        long id = strtol(s, &end, 10);
        int len, sq = rhm_square(end + 1, &len);
        p->sq[sq] = (uint8_t)(id + 1);
        s = end + 1 + len;
        while (*s == ' ') s++;
    }
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    char name_[200];
    static const sp_game G = {
        .id = "rithmomachia", .max_plies = RHM_MAX_PLY + 2, .opening = 2, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const char *const NAME[RHM_VARIANTS] = {"take 8", "take 12", "a harmony"};
    {   /* the bot's moves and captures are the rules': every move of every position of random games */
        ai_rng r;
        ai_seed(&r, 5);
        int same = 1, positions = 0, captures = 0;
        for (int gn = 0; gn < 300 && same; gn++) {
            variant = gn % RHM_VARIANTS;
            reset();
            while (result() < 0 && same) {
                const rhmpos *p = rhm_pos(&T);
                rhmmove l[RHM_MAX_MOVES];
                int mv[AI_MAX_MOVES], n = rhm_legal(p, l), k = rhb_gen(p, mv);
                same = n == k;
                for (int i = 0; i < n && same; i++) {
                    int found = 0;
                    for (int j = 0; j < k && !found; j++) found = mv[j] == l[i].from * RHM_N + l[i].to;
                    same = found && rhb_agrees(p, variant, l[i]);
                }
                char m[8];
                int before = p->taken[0] + p->taken[1];
                random_move(&r, m, sizeof m);
                one(m);
                captures += rhm_pos(&T)->taken[0] + rhm_pos(&T)->taken[1] > before;
                positions++;
            }
        }
        snprintf(name_, sizeof name_, "moves and captures = the rules' (%d positions, %d captures)", positions,
                 captures);
        st_check(name_, same);
    }
    for (variant = 0; variant < RHM_VARIANTS; variant++) st_basics("rithmomachia", &G, rithmomachia_show, 2);
    int all = 1;
    for (int l = AI_MEDIUM; l <= AI_HARD; l++) {  /* the triangle 9 to d6 meets the odds' round 9 on d8 */
        setup(RHM_COMMON8, "9:d4 0:a1 27:d8 24:h16", 0);
        const char *m = st_move("rithmomachia", 0, l);
        all &= m[0] && one(m) && rhm_pos(&T)->taken[0] == 1;
    }
    st_check("medium and hard take by meeting", all);
    all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* the round 6 to d10 between 4 and 8: a harmony, the game */
        setup(RHM_PROPER, "1:b10 3:f10 2:c9 0:a1 24:h16 25:g15", 0);
        const char *m = st_move("rithmomachia", 0, l);
        all &= m[0] && one(m) && result() == 0;
    }
    st_check("every level makes the harmony that wins", all);
    for (variant = 0; variant < RHM_VARIANTS; variant++) {
        printf("     %s:\n", NAME[variant]);
        st_levels(argc, argv, &G, 10, choose);
    }
    if (st_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 60},           /* take 12 ends in many draws (100 quiet moves); no losses */
        {AI_MEDIUM, AI_EASY, 40, 52},
        {AI_HARD, AI_EASY, 40, 55},
        {AI_HARD, AI_MEDIUM, 40, 50},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    int fails = 0;
    for (variant = 0; variant < RHM_VARIANTS; variant++) {
        printf("== %s\n", NAME[variant]);
        fails += st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return fails != 0;
}
