/*
 * Tafl's bot (src/games/tafl_bot.c), both sides, every board: its registry entry, its notation, positions each side
 * must get right, the cancel, then self-play at every level on each board (Copenhagen 11 x 11, Tablut 9 x 9, Brandubh
 * 7 x 7; tests/ai_selfplay.h: each pairing plays both sides, the attackers in even games for a). Headless: this file
 * plays the screen's part (tafl_live(), tafl_show()). --full for all the games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/tafl.h"

static tgame T;                                 /* the "screen's" game: the bot reads it */
static int variant = TF_HNEFATAFL;              /* the board of the games played */
const tgame *tafl_live(void) { return &T; }

static int one(const char *s)                   /* one move, as the screen's script() takes it */
{
    return tf_status(&T) == TF_PLAY && !strchr(s, ' ') && tf_script(&T, s);
}
int tafl_show(const char *move) { return one(move); }

static void reset(void) { tf_new(&T, variant); }
static int actor(void) { return tf_pos(&T)->side; }
static int result(void)
{
    switch (tf_status(&T)) {
    case TF_PLAY: return -1;
    case TF_WON0: return 0;
    case TF_WON1: return 1;
    default: return 2;
    }
}

static int random_move(ai_rng *r, char *m, int cap)
{
    tmove l[TF_MAX_MOVES];
    int n = tf_legal(tf_pos(&T), T.variant, l), w = tf_size(T.variant);
    if (!n || cap < 8) return 0;
    tmove x = l[ai_rand_n(r, n)];
    snprintf(m, (size_t)cap, "%c%d-%c%d", 'a' + x.from % w, x.from / w + 1, 'a' + x.to % w, x.to / w + 1);
    return 1;
}

static int choose(int level, ai_stats *s) { return tfb_choose(&T, level, 5, s) >= 0; }

/* a Brandubh position: attackers, defenders ("a5 d3"), the king's square, side to move */
static void setup(const char *att, const char *def, const char *king, int side)
{
    tf_new(&T, TF_BRANDUBH);
    tpos *p = &T.pos[0];
    memset(p->sq, 0, sizeof p->sq);
    const char *list[2] = {att, def};
    for (int k = 0; k < 2; k++)
        for (const char *s = list[k]; *s; s++)
            if (*s >= 'a' && *s <= 'g') {
                int used;
                p->sq[tf_square(TF_BRANDUBH, s, &used)] = (uint8_t)(k ? TF_DEF : TF_ATT);
                s += used - 1;
            }
    int used;
    p->sq[tf_square(TF_BRANDUBH, king, &used)] = TF_KING;
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "tafl", .max_plies = TF_MAX_PLY + 2, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    cp_basics("tafl", &G, tafl_show);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* Brandubh: the king on a4, a clear edge to a1 */
        setup("e6 f2", "e5", "a4", 1);
        const char *m = cp_move("tafl", 1, l);
        all &= m[0] && one(m) && result() == 1;
    }
    cp_check("the defenders: the king's road to a corner open, every level runs", all);
    all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* Brandubh: the king on a4 under a5's attacker; d3 to a3 takes it */
        setup("a5 d3 f6", "e5", "a4", 0);
        const char *m = cp_move("tafl", 0, l);
        all &= m[0] && one(m) && result() == 0;
    }
    cp_check("the attackers: the king to take, every level takes it", all);
    all = 1;
    for (int l = AI_MEDIUM; l <= AI_HARD; l++) {   /* Brandubh: the king on c1, its road to a1 open: shut it */
        setup("b5 e6 f3 e4", "e5", "c1", 0);
        T.pos[0].sq[tf_square(TF_BRANDUBH, "e1", &(int){0})] = TF_DEF;   /* the road to g1 shut by its own piece */
        const char *m = cp_move("tafl", 0, l);
        all &= m[0] && one(m) && one(cp_move("tafl", 1, AI_HARD)) && result() != 1;
    }
    cp_check("the attackers: the king's one road shut, medium and hard shut it", all);
    static const char *const NAME[3] = {"Copenhagen 11 x 11", "Tablut 9 x 9", "Brandubh 7 x 7"};
    for (variant = 0; variant < 3; variant++) {
        printf("     %s:\n", NAME[variant]);
        cp_levels(argc, argv, &G, 10, choose);
    }
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
    int fails = 0;
    for (variant = 0; variant < 3; variant++) {
        printf("== %s\n", NAME[variant]);
        fails += cp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return fails != 0;
}
