/*
 * Lasca's bot (src/games/lasca_bot.c): its registry entry, its notation (a chain of captures is one turn:
 * "c3xe5xc7"), positions it must get right, the cancel, then self-play at every level (tests/ai_selfplay.h). Headless:
 * this file plays the screen's part (lasca_live(), lasca_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/lasca.h"

static lsgame T;                                /* the "screen's" game: the bot reads it */
const lsgame *lasca_live(void) { return &T; }

static int result(void)
{
    switch (ls_status(&T)) {
    case LS_PLAY: return -1;
    case LS_STUCK: return !ls_pos(&T)->side;
    default: return 2;
    }
}

/* one whole turn, as the screen's script() takes it (a chain that stops before its end is refused) */
static int one(const char *s)
{
    int side = ls_pos(&T)->side;
    if (result() >= 0 || strchr(s, ' ') || !ls_script(&T, s)) return 0;
    return ls_pos(&T)->side != side;
}
int lasca_show(const char *move) { return one(move); }

static void reset(void) { ls_new(&T); }
static int actor(void) { return ls_pos(&T)->side; }

static int random_move(ai_rng *r, char *m, int cap)
{
    lspos p = *ls_pos(&T);
    int side = p.side, k = 0;
    do {                                        /* a whole turn: the jumps on to the chain's end */
        lsmove l[LS_MAX_MOVES];
        int n = ls_legal(&p, l);
        if (!n || k + 8 > cap) return 0;
        lsmove x = l[ai_rand_n(r, n)];
        if (!k) k += snprintf(m + k, (size_t)(cap - k), "%c%c", 'a' + ls_x(x.from), '1' + ls_y(x.from));
        k += snprintf(m + k, (size_t)(cap - k), "%c%c%c", x.over >= 0 ? 'x' : '-', 'a' + ls_x(x.to), '1' + ls_y(x.to));
        ls_make(&p, x);
    } while (p.side == side);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    int mv[40];
    return lsb_turn(&T, level, 5, mv, 40, s) > 0;
}

static void setup(int side, const char *columns)
{
    ls_new(&T);
    ls_setup(&T.pos[0], side, columns);
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "lasca", .max_plies = LS_MAX_PLY, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    cp_basics("lasca", &G, lasca_show);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* c3 over d4 to e5, over d6 to c7 (crowned there: it stops) */
        setup(0, "c3:w a1:w d4:b d6:b g7:b");
        all &= !strcmp(cp_move("lasca", 0, l), "c3xe5xc7");
    }
    cp_check("a chain of two jumps: every level writes it whole (c3xe5xc7)", all);
    all = 1;
    for (int l = AI_MEDIUM; l <= AI_HARD; l++) {   /* e3-d4 or e3-f4 walks into a capture; a1-b2 is safe */
        setup(0, "e3:w a1:w e5:b c5:b g5:b g7:b");
        const char *m = cp_move("lasca", 0, l);
        all &= !strcmp(m, "a1-b2");
    }
    cp_check("a step into a capture or a safe one: medium and hard take the safe one", all);
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
