/*
 * Twist Five's bot (src/games/twistfive_bot.c): its registry entry, whole turns at every level, the twist alone after
 * the marble, positions it must get right (five on the marble, five by a twist, the other's four stopped), the
 * cancel, then self-play at every level (tests/ai_selfplay.h; a ply here is a whole turn). Headless: this file plays
 * the screen's part (twistfive_live(), twistfive_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"

#include "games/twistfive.h"

int tvb_choose(const tvpos *now, int level, uint64_t seed, int *hole, int *q, int *dir, ai_stats *stats);

static tvgame T;                                /* the "screen's" game: the bot reads it */
const tvgame *twistfive_live(void) { return &T; }

static int one(const char *s) { return tv_pos(&T)->winner < 0 && !strchr(s, ' ') && tv_script(&T, s); }
int twistfive_show(const char *move) { return one(move); }

static void reset(void) { tv_new(&T); }
static int actor(void) { return tv_pos(&T)->side; }
static int result(void) { return tv_pos(&T)->winner; }

/* a whole turn at random: a marble in an empty hole, then any twist (none if the marble wins) */
static int random_move(ai_rng *r, char *m, int cap)
{
    const tvpos *p = tv_pos(&T);
    if (p->winner >= 0 || cap < 8) return 0;
    char cell[3] = {0};
    if (p->stage == TV_PLACE) {
        int h = ai_rand_n(r, TV_CELLS);
        while (p->cell[h]) h = (h + 1) % TV_CELLS;
        cell[0] = (char)('a' + h % TV_N);
        cell[1] = (char)('1' + h / TV_N);
        tvpos q = *p;
        tv_place(&q, h);
        if (q.winner >= 0) { snprintf(m, (size_t)cap, "%s", cell); return 1; }
    }
    snprintf(m, (size_t)cap, "%s/%d%c", cell, 1 + ai_rand_n(r, 4), ai_rand_n(r, 2) ? '+' : '-');
    return 1;
}

static int choose(int level, ai_stats *s)
{
    int h, q, d;
    return tvb_choose(tv_pos(&T), level, 5, &h, &q, &d, s);
}

/* a position: player 0's marbles, player 1's ("a1 b2"), player 0 to place */
static void setup(const char *mine, const char *theirs)
{
    tv_new(&T);
    tvpos *p = &T.pos[0];
    const char *list[2] = {mine, theirs};
    for (int k = 0; k < 2; k++)
        for (const char *t = list[k]; *t;) {
            int c = tv_square(t);
            if (c >= 0) p->cell[c] = (uint8_t)(k + 1), p->count++;
            while (*t && *t != ' ') t++;
            while (*t == ' ') t++;
        }
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "twistfive", .max_plies = TV_CELLS + 2, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    st_basics("twistfive", &G, twistfive_show, 2);
    {   /* the marble already placed (a step played before the bot took the seat): the twist alone */
        reset();
        tv_script(&T, "b5");
        const char *m = st_move("twistfive", 0, AI_HARD);
        st_check("after the marble: the twist alone (\"/..\")", m[0] == '/' && one(m) && tv_pos(&T)->side == 1);
    }
    int place = 1, twist = 1, stop = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        setup("a1 a2 a3 a4", "d4 e5 f6 d1");    /* five on the marble: a5 */
        const char *m = st_move("twistfive", 0, l);
        place &= !strcmp(m, "a5") && one(m) && tv_pos(&T)->winner == 0;
        setup("a1 a2 a3 b4 c4", "d4 e5 f6 d1"); /* no marble alone makes five: quarter 1 turned does */
        m = st_move("twistfive", 0, l);
        twist &= m[0] && one(m) && tv_pos(&T)->winner == 0;
        if (l == AI_EASY) continue;
        setup("f6 e1 b2", "a1 a2 a3 a4");       /* the other's four up the a file: it must be stopped */
        m = st_move("twistfive", 0, l);
        int ok = m[0] && one(m) && tv_pos(&T)->winner < 0;
        if (ok) {                               /* and no marble of the other's wins at once now */
            const tvpos *p = tv_pos(&T);
            for (int h = 0; h < TV_CELLS && ok; h++) {
                if (p->cell[h]) continue;
                tvpos q = *p;
                tv_place(&q, h);
                ok = q.winner != 1;
            }
        }
        stop &= ok;
    }
    st_check("every level makes five with the marble alone", place);
    st_check("every level wins a turn whose five comes from its marble or twist", twist);
    st_check("medium and hard stop the other's open four", stop);
    st_levels(argc, argv, &G, 6, choose);
    if (st_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 85},
        {AI_MEDIUM, AI_EASY, 40, 65},
        {AI_HARD, AI_EASY, 40, 75},
        {AI_HARD, AI_MEDIUM, 40, 60},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    return st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
