/*
 * Draughts' bot (src/games/draughts_bot.c), every rule set: its registry entry, its notation (every landing square:
 * the move it means and no other, checked on positions of random games), a few positions it must get right, the
 * cancel, then self-play at every level in each variant (tests/ai_selfplay.h). Headless: this file plays the screen's
 * part (draughts_live(), draughts_show()). --full for all the games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/draughts.h"

static dgame T;                                 /* the "screen's" game: the bot reads it */
static int variant = DR_INTERNATIONAL;          /* the rules of the games played */
const dgame *draughts_live(void) { return &T; }

static int result(void)
{
    switch (dr_status(&T)) {
    case DR_PLAY: return -1;
    case DR_WON0: return 0;
    case DR_WON1: return 1;
    default: return 2;
    }
}

/* one move, as the screen's script() takes it (and nothing after it) */
static int one(const char *s)
{
    static dmove l[DR_MAX_MOVES];
    int used, n = dr_legal(dr_pos(&T), T.variant, l), k = result() < 0 ? dr_parse(l, n, T.variant, s, &used) : -1;
    return k >= 0 && !s[used] && dr_play(&T, &l[k]);
}
int draughts_show(const char *move) { return one(move); }

static void reset(void) { dr_new(&T, variant); }
static int actor(void) { return dr_pos(&T)->side; }

static int random_move(ai_rng *r, char *m, int cap)
{
    static dmove l[DR_MAX_MOVES];
    int n = dr_legal(dr_pos(&T), T.variant, l);
    if (!n) return 0;
    dgb_write(T.variant, &l[ai_rand_n(r, n)], m, cap);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    dmove m;
    return dgb_choose(&T, level, 5, &m, s);
}

static const char *const NAME[DR_VARIANTS] = {"international", "English", "Russian", "Brazilian", "Turkish",
                                              "Frisian", "Canadian", "bashni"};

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "draughts", .max_plies = DR_MAX_PLY, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    char name[200];
    for (variant = 0; variant < DR_VARIANTS; variant++) {
        printf("     %s:\n", NAME[variant]);
        cp_basics("draughts", &G, draughts_show);
    }
    {   /* the notation: every legal move of positions of random games, written and read back, is that move */
        static dmove l[DR_MAX_MOVES];
        int bad = 0, moves = 0;
        ai_rng r;
        ai_seed(&r, 77);
        for (variant = 0; variant < DR_VARIANTS; variant++)
            for (int gn = 0; gn < 30; gn++) {
                reset();
                while (result() < 0 && T.ply < 300) {
                    int n = dr_legal(dr_pos(&T), T.variant, l);
                    for (int i = 0; i < n; i++) {
                        char m[BOT_MOVE_CAP];
                        int used, k;
                        dgb_write(T.variant, &l[i], m, sizeof m);
                        k = dr_parse(l, n, T.variant, m, &used);
                        bad += k < 0 || m[used] || !dr_same(&l[k], &l[i]) || l[k].crown != l[i].crown;
                        moves++;
                    }
                    dr_play(&T, &l[ai_rand_n(&r, n)]);
                }
            }
        snprintf(name, sizeof name, "the notation names the move itself (%d moves of random games, all variants)",
                 moves);
        cp_check(name, !bad);
    }
    {   /* international: white's man on 33 takes black's 28 (to 22) and 17 (to 11): two, compulsory, written whole */
        int all = 1;
        for (int l = AI_EASY; l <= AI_HARD; l++) {
            dr_new(&T, DR_INTERNATIONAL);
            dr_setup(&T.pos[0], DR_INTERNATIONAL, 0, "33 45", "28 17 5");
            all &= !strcmp(cp_move("draughts", 0, l), "33x22x11");
        }
        cp_check("international: a capture of two, every level writes it whole (33x22x11)", all);
    }
    for (variant = 0; variant < DR_VARIANTS; variant++) {
        printf("     %s:\n", NAME[variant]);
        cp_levels(argc, argv, &G, 8, choose);
    }
    if (cp_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 30, 75},
        {AI_MEDIUM, AI_EASY, 24, 60},
        {AI_HARD, AI_EASY, 20, 70},
        {AI_HARD, AI_MEDIUM, 24, 55},
        {AI_EASY, AI_EASY, 6, 0},
        {AI_MEDIUM, AI_MEDIUM, 6, 0},
        {AI_HARD, AI_HARD, 6, 0},
    };
    int fails = 0;
    for (variant = 0; variant < DR_VARIANTS; variant++) {
        printf("== %s\n", NAME[variant]);
        fails += cp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return fails != 0;
}
