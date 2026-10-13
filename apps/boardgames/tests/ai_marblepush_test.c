/*
 * Marble Push's bot (src/games/marblepush_bot.c), every starting layout: its moves against the rules' on random
 * games, its registry entry, the start at every level, a push off it must make and one it must not allow, the cancel,
 * then self-play at every level on each layout (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (marblepush_live(), marblepush_show()). --full for all the games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"

#include "games/marblepush.h"

int mpb_agrees(const mppos *q);
int mpb_choose(const mppos *q, int level, uint64_t seed, int *a, int *b, int *dir, ai_stats *stats);

static mpgame T;                                /* the "screen's" game: the bot reads it */
static int layout;
const mpgame *marblepush_live(void) { return &T; }

static int one(const char *s) { return !strchr(s, ' ') && mp_script(&T, s); }
int marblepush_show(const char *move) { return one(move); }

static void reset(void) { mp_new(&T, layout); }
static int actor(void) { return mp_pos(&T)->side; }
static int result(void)
{
    int s = mp_status(&T);
    return s == MP_PLAY ? -1 : s == MP_WIN0 ? 0 : s == MP_WIN1 ? 1 : 2;
}

static void notation(const mpmove *x, char *m)
{
    static const char *const DIR[6] = {"e", "ne", "nw", "w", "sw", "se"};
    char a[3], b[3];
    mp_name(x->cell[0], a);
    mp_name(x->cell[x->n - 1], b);
    sprintf(m, "%s%s:%s", a, x->n > 1 ? b : "", DIR[x->dir]);
}

static int random_move(ai_rng *r, char *m, int cap)
{
    mpmove l[MP_MAX_MOVES];
    int n = mp_legal(mp_pos(&T), l);
    if (!n || cap < 9) return 0;
    notation(&l[ai_rand_n(r, n)], m);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    int a, b, d;
    return mpb_choose(mp_pos(&T), level, 5, &a, &b, &d, s);
}

/* a position on the standard board: the marbles of player 0, of player 1 ("a1 b2"), the side to move */
static void setup(const char *p0, const char *p1, int side)
{
    mp_new(&T, MP_STANDARD);
    mppos *p = &T.pos[0];
    memset(p->cell, 0, sizeof p->cell);
    const char *list[2] = {p0, p1};
    for (int k = 0; k < 2; k++)
        for (const char *s = list[k]; *s; s++) {
            int len, h = mp_hole(s, &len);
            if (h >= 0) { p->cell[h] = (uint8_t)(k + 1); s++; }
        }
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    char name[160];
    static const sp_game G = {
        .id = "marblepush", .max_plies = MP_MAX_PLY + 2, .opening = 2, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const char *const NAME[MP_VARIANTS] = {"standard", "Belgian daisy", "German daisy"};
    {   /* the bot's moves are the rules': every position of random games on every layout */
        ai_rng r;
        ai_seed(&r, 9);
        int same = 1, positions = 0, offs = 0;
        for (int gn = 0; gn < 300 && same; gn++) {
            layout = gn % MP_VARIANTS;
            reset();
            while (result() < 0 && same) {
                same = mpb_agrees(mp_pos(&T));
                char m[12];
                int before = mp_pos(&T)->lost[0] + mp_pos(&T)->lost[1];
                random_move(&r, m, sizeof m);
                one(m);
                offs += mp_pos(&T)->lost[0] + mp_pos(&T)->lost[1] > before;
                positions++;
            }
        }
        snprintf(name, sizeof name, "moves and pushes = the rules' (%d positions, %d marbles off)", positions, offs);
        st_check(name, same);
    }
    for (layout = 0; layout < MP_VARIANTS; layout++) st_basics("marblepush", &G, marblepush_show, 2);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* g7 g8 push g9 off the row's east end: the sixth, the game */
        setup("a1 a2 g7 g8", "g9 e5 c3", 0);
        mppos *p = &T.pos[0];
        p->lost[1] = 5;                         /* one more off wins */
        const char *m = st_move("marblepush", 0, l);
        all &= m[0] && one(m) && result() == 0;
    }
    st_check("every level pushes the winning marble off", all);
    for (layout = 0; layout < MP_VARIANTS; layout++) {
        printf("     %s:\n", NAME[layout]);
        st_levels(argc, argv, &G, 8, choose);
    }
    if (st_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 80},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 70},
        {AI_HARD, AI_MEDIUM, 40, 55},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    int fails = 0;
    for (layout = 0; layout < MP_VARIANTS; layout++) {
        printf("== %s\n", NAME[layout]);
        fails += st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return fails != 0;
}
