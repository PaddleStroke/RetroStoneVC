/*
 * Konane's bot (src/games/konane_bot.c): its registry entry, its notation, a position it must get right, the cancel,
 * then self-play at every level on each board (6 x 6, 8 x 8, 10 x 10; tests/ai_selfplay.h). Headless: this file plays
 * the screen's part (konane_live(), konane_show()). --full for all the games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/konane.h"

static kngame T;                                /* the "screen's" game: the bot reads it */
static int size = 8;                            /* the board of the games played */
const kngame *konane_live(void) { return &T; }

static int one(const char *s)                   /* one move, as the screen's script() takes it */
{
    return kn_winner(&T) < 0 && !strchr(s, ' ') && kn_script(&T, s);
}
int konane_show(const char *move) { return one(move); }

static void reset(void) { kn_new(&T, size); }
static int actor(void) { return kn_pos(&T)->side; }
static int result(void) { return kn_winner(&T); }

static int random_move(ai_rng *r, char *m, int cap)
{
    knmove l[KN_MAX_MOVES];
    const knpos *p = kn_pos(&T);
    int n = kn_legal(p, l), w = p->n;
    if (!n || cap < 8) return 0;
    knmove x = l[ai_rand_n(r, n)];
    if (x.from == x.to) snprintf(m, (size_t)cap, "%c%d", 'a' + x.from % w, x.from / w + 1);
    else snprintf(m, (size_t)cap, "%c%d-%c%d", 'a' + x.from % w, x.from / w + 1, 'a' + x.to % w, x.to / w + 1);
    return 1;
}

static int choose(int level, ai_stats *s) { return knb_choose(kn_pos(&T), level, 5, s) >= 0; }

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "konane", .max_plies = KN_MAX_PLY, .opening = 4, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    cp_basics("konane", &G, konane_show);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        /* 6 x 6, the jumps: ebony a1 d4, ivory b1 d5. a1-c1 leaves d5 to take d4 and ebony stuck; d4-d6 leaves ivory
         * without a jump: it wins at once */
        kn_new(&T, 6);
        knpos *p = &T.pos[0];
        memset(p->at, -1, sizeof p->at);
        p->opening = 2;
        p->at[0] = p->at[3 + 6 * 3] = 0;
        p->at[1] = p->at[3 + 6 * 4] = 1;
        all &= !strcmp(cp_move("konane", 0, l), "d4-d6");
    }
    cp_check("the jump that leaves the other none: every level plays it", all);
    for (size = 6; size <= 10; size += 2) {
        printf("     %d x %d:\n", size, size);
        cp_levels(argc, argv, &G, 6 + size, choose);
    }
    if (cp_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 75},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 75},
        {AI_HARD, AI_MEDIUM, 40, 55},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    int fails = 0;
    for (size = 6; size <= 10; size += 2) {
        printf("== %d x %d\n", size, size);
        fails += cp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return fails != 0;
}
