/*
 * Bagh-Chal's bot (src/games/baghchal_bot.c), both sides: its registry entry, its notation, positions each side must
 * get right, the cancel, then self-play at every level (tests/ai_selfplay.h: each pairing plays both sides, the
 * goats in even games for a). Headless: this file plays the screen's part (baghchal_live(), baghchal_show()). --full
 * for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/baghchal.h"

static bcgame T;                                /* the "screen's" game: the bot reads it */
const bcgame *baghchal_live(void) { return &T; }

static int result(void)
{
    switch (bc_status(&T)) {
    case BS_PLAY: return -1;
    case BS_TRAPPED: return 0;
    case BS_FIVE: case BS_STUCK: return 1;
    default: return 2;
    }
}

static int one(const char *s)                   /* one move, as the screen's script() takes it */
{
    return result() < 0 && strlen(s) <= 5 && !strchr(s, ' ') && bc_script(&T, s);
}
int baghchal_show(const char *move) { return one(move); }

static void reset(void) { bc_new(&T); }
static int actor(void) { return bc_pos(&T)->side; }

static int random_move(ai_rng *r, char *m, int cap)
{
    bcmove l[BC_MAX_MOVES];
    int n = bc_legal(bc_pos(&T), l);
    if (!n || cap < 6) return 0;
    bcmove x = l[ai_rand_n(r, n)];
    bcb_write((x.from + 1) | x.to << 5 | (x.over >= 0) << 10, m, cap);
    return 1;
}

static int choose(int level, ai_stats *s) { return bcb_choose(&T, level, 5, s) >= 0; }

/* a position: goats and tigers on points ("b2 c3"), goats set down so far, side to move */
static void setup(const char *goats, const char *tigers, int placed, int side)
{
    bc_new(&T);
    bcpos *p = &T.pos[0];
    memset(p->at, -1, sizeof p->at);
    const char *list[2] = {goats, tigers};
    for (int k = 0; k < 2; k++)
        for (const char *s = list[k]; *s; s++)
            if (*s >= 'a' && *s <= 'e') {
                p->at[bc_point(s)] = (int8_t)k;
                s++;
            }
    p->placed = (uint8_t)placed;
    p->side = (uint8_t)side;
}

static int tiger_jumps(void)
{
    bcpos q = *bc_pos(&T);
    q.side = BC_TIGER;
    bcmove l[BC_MAX_MOVES];
    int n = bc_legal(&q, l), k = 0;
    for (int i = 0; i < n; i++) k += l[i].over >= 0;
    return k;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "baghchal", .max_plies = BC_MAX_PLY, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    cp_basics("baghchal", &G, baghchal_show);
    int all = 1;
    /* b2 hangs (a1 over it to c3; easy and medium may play for more threats first: the goats cannot hold them all) */
    setup("b2", "a1 e1 a5 e5", 1, BC_TIGER);
    for (int k = 0; k < 2 && result() < 0; k++)
        if (!one(cp_move("baghchal", BC_TIGER, AI_HARD)) || (result() < 0 && !one(cp_move("baghchal", BC_GOAT, AI_HARD))))
            all = 0;
    cp_check("tigers: a goat hanging, hard takes a goat within two moves", all && bc_pos(&T)->taken >= 1);
    all = 1;
    for (int l = AI_MEDIUM; l <= AI_HARD; l++) {   /* b2 and d4 hang (over them to c3): a goat there covers both */
        setup("b2 e3 d4", "a1 e1 a5 e5", 3, BC_GOAT);
        const char *m = cp_move("baghchal", BC_GOAT, l);
        all &= m[0] && one(m) && !tiger_jumps();
    }
    cp_check("goats: a goat hanging, medium and hard leave no jump", all);
    all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* a lone tiger hemmed in on a1: a goat on b1 closes the cage */
        setup("a2 a3 b2 c3 c1", "a1", 19, BC_GOAT);
        const char *m = cp_move("baghchal", BC_GOAT, l);
        all &= m[0] && one(m) && result() == 0;
    }
    cp_check("goats: the tiger's last way out closed, every level closes it", all);
    cp_levels(argc, argv, &G, 8, choose);
    if (cp_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 75},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 70},
        {AI_HARD, AI_MEDIUM, 40, 55},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    return cp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
