/*
 * Corridor's bot (src/games/corridor_bot.c): its registry entry, moves at every level, positions it must get right
 * (the step home, the wall that stops the other's), the cancel, then self-play at every level with two players and
 * with four (seats 0 and 2 at one level, 1 and 3 at the other; tests/ai_selfplay.h). Headless: this file plays the
 * screen's part (corridor_live(), corridor_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"

#include "games/corridor.h"

int crb_choose(const crpos *now, int level, uint64_t seed, ai_stats *stats);

static crgame T;                                /* the "screen's" game: the bot reads it */
static int players = 2;
const crgame *corridor_live(void) { return &T; }

static int one(const char *s) { return cr_pos(&T)->winner < 0 && !strchr(s, ' ') && cr_script(&T, s); }
int corridor_show(const char *move) { return one(move); }

static void reset(void) { cr_new(&T, players); }
static int actor(void) { return cr_pos(&T)->turn; }

static int result(void)
{
    int w = cr_pos(&T)->winner;
    return w < 0 ? -1 : w == CR_DRAW ? 2 : w & 1;   /* four players: seats 0 and 2 are a, 1 and 3 b */
}

/* a random player: a step half the time (else it walls itself in its own corner of the board for ever), a wall */
static int random_move(ai_rng *r, char *m, int cap)
{
    const crpos *p = cr_pos(&T);
    int mv[CR_MAX_MOVES], n = cr_moves(p, mv);
    if (!n || cap < 4) return 0;
    uint8_t to[8];
    int k = cr_pawn_moves(p, to), pick = k && ai_rand_n(r, 2) ? to[ai_rand_n(r, k)] : mv[ai_rand_n(r, n)];
    cr_name(pick, m);
    return 1;
}

static int choose(int level, ai_stats *s) { return crb_choose(cr_pos(&T), level, 5, s) >= 0; }

/* a position: the pawns ("e5 e6"), the walls ("e6h d5v"), who moves */
static void setup(const char *pawns, const char *walls, int turn)
{
    cr_new(&T, players);
    crpos *p = &T.pos[0];
    int i = 0;
    for (const char *s = pawns; *s && i < players;) {
        p->pawn[i++] = (uint8_t)cr_square(s);
        while (*s && *s != ' ') s++;
        while (*s == ' ') s++;
    }
    for (const char *s = walls; *s;) {
        int len, m = cr_parse(p, s, &len);
        if (CR_IS_WALL(m)) p->wall[CR_SLOT(m)] = (uint8_t)CR_ORIENT(m);
        while (*s && *s != ' ') s++;
        while (*s == ' ') s++;
    }
    p->turn = (uint8_t)turn;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "corridor", .max_plies = CR_MAX_PLY + 4, .opening = 2, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    st_basics("corridor", &G, corridor_show, 2);
    int all = 1, walls = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        setup("e8 c3", "", 0);                  /* one step home: every level takes it */
        const char *m = st_move("corridor", 0, l);
        all &= m[0] && one(m) && cr_pos(&T)->winner == 0;
        if (l == AI_EASY) continue;
        setup("e5 e2", "", 0);                  /* ebony one step from home, ivory far: a wall must stop it */
        m = st_move("corridor", 0, l);
        walls &= m[0] && one(m) && cr_dist(cr_pos(&T), 1) > 1;
    }
    st_check("every level steps home when it can", all);
    st_check("medium and hard wall the other's last step", walls);
    st_levels(argc, argv, &G, 6, choose);
    if (st_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 80},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 70},
        {AI_HARD, AI_MEDIUM, 40, 55},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    int fail = st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    char dump4[300];                            /* --dump FILE: the games of four in FILE.4 */
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--dump")) {
            snprintf(dump4, sizeof dump4, "%s.4", argv[i + 1]);
            argv[i + 1] = dump4;
        }
    players = 4;                                /* four: the same pairings, seats 0 and 2 against 1 and 3 */
    st_levels(argc, argv, &G, 6, choose);
    static const sp_pair P4[] = {
        {AI_EASY, SP_RANDOM, 40, 80},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 75},
        {AI_HARD, AI_MEDIUM, 40, 55},
    };
    fail |= st_run(&G, P4, (int)(sizeof P4 / sizeof P4[0]), argc, argv);
    return fail;
}
