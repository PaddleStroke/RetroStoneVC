/*
 * Breakthrough's bot (src/games/breakthrough_bot.c): its registry entry, its notation, a few positions it must get
 * right, the cancel, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (breakthrough_live(), breakthrough_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/breakthrough.h"

static btgame T;                                /* the "screen's" game: the bot reads it */
const btgame *breakthrough_live(void) { return &T; }

static int one(const char *s)                   /* one move, as the screen's script() takes it */
{
    return bt_winner(bt_pos(&T)) < 0 && strlen(s) <= 5 && bt_script(&T, s);
}
int breakthrough_show(const char *move) { return one(move); }

static void reset(void) { bt_new(&T); }
static int actor(void) { return bt_pos(&T)->side; }
static int result(void) { return bt_winner(bt_pos(&T)); }

static int random_move(ai_rng *r, char *m, int cap)
{
    btmove l[BT_MAX_MOVES];
    int n = bt_legal(bt_pos(&T), l);
    if (!n || cap < 6) return 0;
    btmove x = l[ai_rand_n(r, n)];
    snprintf(m, (size_t)cap, "%c%c-%c%c", 'a' + x.from % 8, '1' + x.from / 8, 'a' + x.to % 8, '1' + x.to / 8);
    return 1;
}

static int choose(int level, ai_stats *s) { return btb_choose(bt_pos(&T), level, 5, s) >= 0; }

/* a position from two lists of squares ("a2 b3"), side to move */
static void setup(const char *ivory, const char *ebony, int side)
{
    bt_new(&T);
    btpos *p = &T.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->left[0] = p->left[1] = 0;
    const char *list[2] = {ivory, ebony};
    for (int pl = 0; pl < 2; pl++)
        for (const char *s = list[pl]; *s; s++)
            if (*s >= 'a' && *s <= 'h') {
                p->at[bt_square(s)] = (int8_t)pl;
                p->left[pl]++;
                s++;
            }
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "breakthrough", .max_plies = BT_MAX_PLY, .opening = 2, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    cp_basics("breakthrough", &G, breakthrough_show);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        setup("a1 b1 c7", "f8 g8 h8 d6", 0);
        all &= cp_move("breakthrough", 0, l)[4] == '8';
    }
    cp_check("a pawn one step from the far row: every level steps through", all);
    all = 1;
    for (int l = AI_MEDIUM; l <= AI_HARD; l++) {
        setup("a1 c1 g4 h4", "b2 g8 h8", 0);
        all &= !strcmp(cp_move("breakthrough", 0, l) + 2, "xb2");
    }
    cp_check("ebony one step from home: medium and hard take it", all);
    setup("a1 b1 c1 e4", "d5 a8 b8 c8", 0);
    cp_check("a free pawn (that threatens): hard takes it", !strcmp(cp_move("breakthrough", 0, AI_HARD), "e4xd5"));
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
