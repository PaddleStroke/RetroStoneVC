/*
 * Lines of Action's bot (src/games/loa_bot.c): its registry entry, the start at every level, a joining move it must
 * find, a join it must stop, the cancel, then self-play at every level (tests/ai_selfplay.h). Headless: this file
 * plays the screen's part (loa_live(), loa_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"

#include "games/loa.h"

int lob_choose(const lpos *now, int level, uint64_t seed, ai_stats *stats);

static lgame T;                                 /* the "screen's" game: the bot reads it */
const lgame *loa_live(void) { return &T; }

static int one(const char *s)                   /* one move, as the screen's script() takes it */
{
    lmove l[LOA_MAX_MOVES];
    const lpos *p = loa_pos(&T);
    int n = p->result == LOA_PLAY ? loa_legal(p, p->side, l) : 0, used, k = loa_find(l, n, s, &used);
    return k >= 0 && !s[used] && loa_play(&T, l[k]);
}
int loa_show(const char *move) { return one(move); }

static void reset(void) { loa_new(&T); }
static int actor(void) { return loa_pos(&T)->side; }
static int result(void)
{
    int r = loa_pos(&T)->result;
    return r == LOA_PLAY ? -1 : r == LOA_WIN0 ? 0 : r == LOA_WIN1 ? 1 : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    lmove l[LOA_MAX_MOVES];
    const lpos *p = loa_pos(&T);
    int n = loa_legal(p, p->side, l);
    if (!n || cap < 6) return 0;
    lmove x = l[ai_rand_n(r, n)];
    snprintf(m, (size_t)cap, "%c%c-%c%c", 'a' + x.from % 8, '1' + x.from / 8, 'a' + x.to % 8, '1' + x.to / 8);
    return 1;
}

static int choose(int level, ai_stats *s) { return lob_choose(loa_pos(&T), level, 5, s) >= 0; }

static void setup(const char *p0, const char *p1, int side)   /* the pieces ("a1 b2"), the side to move */
{
    loa_new(&T);
    lpos *p = &T.pos[0];
    memset(p->at, 0, sizeof p->at);
    const char *list[2] = {p0, p1};
    for (int k = 0; k < 2; k++)
        for (const char *s = list[k]; *s; s++)
            if (*s >= 'a' && *s <= 'h') {
                p->at[loa_square(s)] = (uint8_t)(k + 1);
                s++;
            }
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "loa", .max_plies = LOA_MAX_PLY + 2, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    st_basics("loa", &G, loa_show, 2);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {  /* d2 to c1 or c3 joins a1 b2 d2 */
        setup("a1 b2 d2", "h8 h6 f8 e6", 0);
        const char *m = st_move("loa", 0, l);
        all &= m[0] && one(m) && result() == 0;
    }
    st_check("every level joins its pieces when one move does", all);
    {   /* positions of random games where some moves let the other player join at once and some don't: medium and
         * hard never pick one that does */
        ai_rng r;
        ai_seed(&r, 77);
        int tried = 0, good = 0;
        for (int gn = 0; gn < 4000 && tried < 30; gn++) {
            reset();
            for (int i = 0, n = 10 + ai_rand_n(&r, 50); i < n && result() < 0; i++) {
                char m[8];
                random_move(&r, m, sizeof m);
                one(m);
            }
            if (result() >= 0) continue;
            const lpos *p = loa_pos(&T);
            lmove l[LOA_MAX_MOVES], q[LOA_MAX_MOVES];
            int n = loa_legal(p, p->side, l), safe = 0, me = p->side;
            uint64_t ok[2] = {0, 0};            /* ok[i / 64] bit i: move i leaves no join in one */
            for (int i = 0; i < n; i++) {
                lpos a = *p;
                loa_make(&a, l[i]);
                int bad = 0;
                if (a.result == LOA_PLAY && a.side != me)
                    for (int j = 0, k = loa_legal(&a, a.side, q); j < k && !bad; j++) {
                        lpos b = a;
                        loa_make(&b, q[j]);
                        bad = b.result == LOA_WIN0 + !me;
                    }
                else bad = a.result == LOA_WIN0 + !me;
                if (!bad) { ok[i / 64] |= 1ull << (i % 64); safe++; }
            }
            if (!safe || safe == n) continue;
            tried++;
            int fine = 1;
            for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
                const char *m = st_move("loa", me, lv);
                int used, k = loa_find(l, n, m, &used);
                fine &= k >= 0 && (ok[k / 64] >> (k % 64) & 1);
            }
            good += fine;
        }
        char name[120];
        snprintf(name, sizeof name, "medium and hard never let the other join in one: %d of %d positions", good, tried);
        st_check(name, tried >= 10 && good == tried);
    }
    st_levels(argc, argv, &G, 6, choose);
    if (st_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 80},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 75},
        {AI_HARD, AI_MEDIUM, 40, 55},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    return st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
