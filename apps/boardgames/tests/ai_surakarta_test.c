/*
 * Surakarta's bot (src/games/surakarta_bot.c): its registry entry, its notation, a few positions it must get right,
 * the cancel, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (surakarta_live(), surakarta_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/surakarta.h"

static skgame T;                                /* the "screen's" game: the bot reads it */
const skgame *surakarta_live(void) { return &T; }

static int one(const char *s)                   /* one move, as the screen's script() takes it */
{
    return sk_winner(&T) < 0 && strlen(s) <= 5 && sk_script(&T, s);
}
int surakarta_show(const char *move) { return one(move); }

static void reset(void) { sk_new(&T); }
static int actor(void) { return sk_pos(&T)->side; }
static int result(void) { return sk_winner(&T); }

static int random_move(ai_rng *r, char *m, int cap)
{
    skmove l[SK_MAX_MOVES];
    int n = sk_legal(sk_pos(&T), l);
    if (!n || cap < 6) return 0;
    skmove x = l[ai_rand_n(r, n)];
    snprintf(m, (size_t)cap, "%c%c-%c%c", 'a' + sk_x(x.from), '1' + sk_y(x.from), 'a' + sk_x(x.to), '1' + sk_y(x.to));
    return 1;
}

static int choose(int level, ai_stats *s) { return skb_choose(&T, level, 5, s) >= 0; }

/* a position from two lists of points ("a2 b3"), side to move */
static void setup(const char *ivory, const char *ebony, int side)
{
    sk_new(&T);
    skpos *p = &T.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->left[0] = p->left[1] = 0;
    const char *list[2] = {ivory, ebony};
    for (int pl = 0; pl < 2; pl++)
        for (const char *s = list[pl]; *s; s++)
            if (*s >= 'a' && *s <= 'f') {
                p->at[sk_point(s)] = (int8_t)pl;
                p->left[pl]++;
                s++;
            }
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "surakarta", .max_plies = SK_MAX_PLY, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    cp_basics("surakarta", &G, surakarta_show);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* b1 runs up the b line, round the a5 loop, onto c5 */
        setup("b1 a1", "c5 f6", 0);
        all &= !strcmp(cp_move("surakarta", 0, l), "b1xc5");
    }
    cp_check("a piece to take round a loop: every level takes it", all);
    all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* the same run backwards: ebony to move takes first */
        setup("b1 a1", "c5 f6", 1);
        all &= !strcmp(cp_move("surakarta", 1, l), "c5xb1");
    }
    cp_check("ebony to move: every level takes first", all);
    cp_levels(argc, argv, &G, 6, choose);
    if (cp_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 80},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 80},
        {AI_HARD, AI_MEDIUM, 40, 60},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    return cp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
