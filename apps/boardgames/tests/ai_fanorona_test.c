/*
 * Fanorona's bot (src/games/fanorona_bot.c): its registry entry, its notation (a turn is one word: "a1-b1a-b2a"; the
 * word's end stops a chain), positions it must get right, the cancel, then self-play at every level
 * (tests/ai_selfplay.h). Headless: this file plays the screen's part (fanorona_live(), fanorona_show()). --full for the
 * 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/fanorona.h"

static fngame T;                                /* the "screen's" game: the bot reads it */
const fngame *fanorona_live(void) { return &T; }

static int result(void)
{
    switch (fn_status(&T)) {
    case FS_PLAY: return -1;
    case FS_LOST: return !fn_pos(&T)->side;
    default: return 2;
    }
}

/* one whole turn, one word, as the screen's script() takes it */
static int one(const char *s)
{
    int side = fn_pos(&T)->side;
    if (result() >= 0 || strchr(s, ' ') || !fn_script(&T, s)) return 0;
    return fn_pos(&T)->side != side;
}
int fanorona_show(const char *move) { return one(move); }

static void reset(void) { fn_new(&T); }
static int actor(void) { return fn_pos(&T)->side; }

static int random_move(ai_rng *r, char *m, int cap)
{
    fnpos p = *fn_pos(&T);
    int side = p.side, k = 0;
    do {                                        /* a whole turn: captures on while it can, or a stop at random */
        fnmove l[FN_MAX_MOVES];
        int n = fn_legal(&p, l), i = ai_rand_n(r, n + (p.chain >= 0));
        if (!n || k + 8 > cap) return 0;
        if (i == n) break;                      /* the stop: the word ends */
        fnmove x = l[i];
        if (!k) k += snprintf(m + k, (size_t)(cap - k), "%c%c", 'a' + fn_x(x.from), '1' + fn_y(x.from));
        k += snprintf(m + k, (size_t)(cap - k), "-%c%c", 'a' + fn_x(x.to), '1' + fn_y(x.to));
        if (x.kind != FN_PAIKA) m[k++] = x.kind == FN_APPROACH ? 'a' : 'w', m[k] = 0;
        fn_make(&p, x);
    } while (p.side == side);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    int mv[40];
    return fnb_turn(&T, level, 5, mv, 40, s) > 0;
}

/* a position from two lists of points ("a1 c3"), side to move */
static void setup(const char *ivory, const char *ebony, int side)
{
    fn_new(&T);
    fnpos *p = &T.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->left[0] = p->left[1] = 0;
    const char *list[2] = {ivory, ebony};
    for (int pl = 0; pl < 2; pl++)
        for (const char *s = list[pl]; *s; s++)
            if (*s >= 'a' && *s <= 'i') {
                p->at[fn_point(s)] = (int8_t)pl;
                p->left[pl]++;
                s++;
            }
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "fanorona", .max_plies = 2000, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    cp_basics("fanorona", &G, fanorona_show);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* a1-b1 approaches a line of three; a1-a2 or a1-b2 takes one */
        setup("a1 h1 i1 h2 i2", "c1 d1 e1 a3 c3 i5", 0);
        all &= !strcmp(cp_move("fanorona", 0, l), "a1-b1a");
    }
    cp_check("three or one: every level takes the line of three (a1-b1a)", all);
    all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* a1-b1 takes c1, then b1-b2 approaches b3: going on takes two */
        setup("a1", "c1 b3 i5", 0);
        all &= !strcmp(cp_move("fanorona", 0, l), "a1-b1a-b2a");
    }
    cp_check("a chain worth going on: every level goes on (a1-b1a-b2a)", all);
    cp_levels(argc, argv, &G, 4, choose);
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
