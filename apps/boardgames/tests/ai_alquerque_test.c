/*
 * Alquerque's bot (src/games/alquerque_bot.c): its registry entry, its notation (a chain of captures is one turn:
 * "b2xd4xb4"), positions it must get right, the cancel, then self-play at every level (tests/ai_selfplay.h).
 * Headless: this file plays the screen's part (alquerque_live(), alquerque_show()). --full for the 200 games and the
 * table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/alquerque.h"

static aqgame T;                                /* the "screen's" game: the bot reads it */
const aqgame *alquerque_live(void) { return &T; }

static int result(void)
{
    switch (aq_status(&T)) {
    case AS_PLAY: return -1;
    case AS_STUCK: return !aq_pos(&T)->side;
    default: return 2;
    }
}

/* one whole turn, as the screen's script() takes it (a chain that stops before its end is refused) */
static int one(const char *s)
{
    int side = aq_pos(&T)->side;
    if (result() >= 0 || strchr(s, ' ') || !aq_script(&T, s)) return 0;
    return aq_pos(&T)->side != side;
}
int alquerque_show(const char *move) { return one(move); }

static void reset(void) { aq_new(&T); }
static int actor(void) { return aq_pos(&T)->side; }

static int random_move(ai_rng *r, char *m, int cap)
{
    aqpos p = *aq_pos(&T);
    int side = p.side, k = 0;
    do {                                        /* a whole turn: the captures on to the chain's end */
        aqmove l[AQ_MAX_MOVES];
        int n = aq_legal(&p, l);
        if (!n || k + 8 > cap) return 0;
        aqmove x = l[ai_rand_n(r, n)];
        if (!k) k += snprintf(m + k, (size_t)(cap - k), "%c%c", 'a' + aq_x(x.from), '1' + aq_y(x.from));
        k += snprintf(m + k, (size_t)(cap - k), "%c%c%c", x.over >= 0 ? 'x' : '-', 'a' + aq_x(x.to), '1' + aq_y(x.to));
        aq_make(&p, x);
    } while (p.side == side);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    int mv[40];
    return aqb_turn(&T, level, 5, mv, 40, s) > 0;
}

/* a position from two lists of points ("a2 b3"), side to move */
static void setup(const char *ivory, const char *ebony, int side)
{
    aq_new(&T);
    aqpos *p = &T.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->left[0] = p->left[1] = 0;
    const char *list[2] = {ivory, ebony};
    for (int pl = 0; pl < 2; pl++)
        for (const char *s = list[pl]; *s; s++)
            if (*s >= 'a' && *s <= 'e') {
                p->at[aq_point(s)] = (int8_t)pl;
                p->left[pl]++;
                s++;
            }
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "alquerque", .max_plies = AQ_MAX_PLY, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    cp_basics("alquerque", &G, alquerque_show);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* b2 over c3 to d4, then over c4 to b4: one turn, written whole */
        setup("b2 a1", "c3 c4 e5 d5", 0);
        all &= !strcmp(cp_move("alquerque", 0, l), "b2xd4xb4");
    }
    cp_check("a chain of two captures: every level writes it whole (b2xd4xb4)", all);
    all = 1;
    for (int l = AI_MEDIUM; l <= AI_HARD; l++) {   /* the same, or e1 over e2: one piece instead of two */
        setup("b2 a1 e1", "c3 c4 e2 a5", 0);
        all &= !strcmp(cp_move("alquerque", 0, l), "b2xd4xb4");
    }
    cp_check("two captures or one: medium and hard take two", all);
    cp_levels(argc, argv, &G, 6, choose);
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
