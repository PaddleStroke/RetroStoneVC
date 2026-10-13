/*
 * The Game of the Amazons' bot (src/games/amazons_bot.c): its registry entry, whole turns at every level, the arrow
 * alone after the amazon moved, positions it must get right, the cancel, then self-play at every level
 * (tests/ai_selfplay.h; a ply here is a whole turn). Headless: this file plays the screen's part (amazons_live(),
 * amazons_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"

#include "games/amazons.h"

int amb_choose(const amgame *g, int level, uint64_t seed, int *from, int *to, int *arrow, ai_stats *stats);

static amgame T;                                /* the "screen's" game: the bot reads it */
const amgame *amazons_live(void) { return &T; }

static int one(const char *s) { return am_winner(&T) < 0 && !strchr(s, ' ') && am_script(&T, s); }
int amazons_show(const char *move) { return one(move); }

static void reset(void) { am_new(&T); }
static int actor(void) { return am_pos(&T)->side; }
static int result(void) { return am_winner(&T); }

static void name(int s, char *out)              /* "d1", "j10" */
{
    int r = s / AM_N + 1;
    out[0] = (char)('a' + s % AM_N);
    out[1] = (char)(r >= 10 ? '1' : '0' + r);
    out[2] = (char)(r >= 10 ? '0' + r - 10 : 0);
    out[3] = 0;
}

/* a whole turn at random: an amazon that can move, a square it reaches, an arrow from there */
static int random_move(ai_rng *r, char *m, int cap)
{
    const ampos *p = am_pos(&T);
    int own[4], n = 0;
    uint8_t to[AM_MAX_LINE], ar[AM_MAX_LINE];
    for (int s = 0; s < AM_SQUARES; s++)
        if (p->sq[s] == AM_AMAZON + p->side && am_line(p, s, to)) own[n++] = s;
    if (!n || cap < 12) return 0;
    int from = own[ai_rand_n(r, n)], k = am_line(p, from, to), dest = to[ai_rand_n(r, k)];
    ampos q = *p;
    q.sq[dest] = q.sq[from];
    q.sq[from] = AM_EMPTY;
    int na = am_line(&q, dest, ar), arrow = ar[ai_rand_n(r, na)];
    char a[4], b[4], c[4];
    name(from, a);
    name(dest, b);
    name(arrow, c);
    snprintf(m, (size_t)cap, "%s-%s/%s", a, b, c);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    int f, t, a;
    return amb_choose(&T, level, 5, &f, &t, &a, s);
}

/* a position: the amazons ("a1 c3" each), the arrows, the side to move */
static void setup(const char *ivory, const char *ebony, const char *arrows, int side)
{
    am_new(&T);
    ampos *p = &T.pos[0];
    memset(p->sq, AM_EMPTY, sizeof p->sq);
    const char *list[3] = {ivory, ebony, arrows};
    for (int k = 0; k < 3; k++)
        for (const char *s = list[k]; *s;) {
            int sq = am_square(s);
            if (sq < 0) { s++; continue; }
            p->sq[sq] = (uint8_t)(k == 2 ? AM_ARROW : AM_AMAZON + k);
            s += sq >= 9 * AM_N ? 3 : 2;
        }
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "amazons", .max_plies = AM_MAX_PLY / 2 + 2, .opening = 2, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    st_basics("amazons", &G, amazons_show, 2);
    {   /* the amazon already moved (a step played before the bot took the seat): the arrow alone */
        reset();
        am_script(&T, "d1-d7");
        const char *m = st_move("amazons", 0, AI_HARD);
        st_check("after the amazon's step: the arrow alone (\"/..\")", m[0] == '/' && one(m) && am_pos(&T)->side);
    }
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        /* ebony's amazons walled in but j10, whose one way out is i10: ivory shuts it (an amazon or an arrow) */
        setup("a1 e1 h1 j1", "a10 b10 c10 j10", "a9 b9 c9 d9 d10 i9 j9", 0);
        const char *m = st_move("amazons", 0, l);
        all &= m[0] && one(m) && am_winner(&T) == 0;
    }
    st_check("every level shuts the last amazon in and wins", all);
    st_levels(argc, argv, &G, 4, choose);
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
    return st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
