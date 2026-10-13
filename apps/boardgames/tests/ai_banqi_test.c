/*
 * Banqi's bot (src/games/banqi_bot.c): its registry entry, that it never peeks (the face-down pieces shuffled among
 * their squares: the same move), its pool of face-down pieces (as many as the squares face down), a free capture, the
 * cancel, the levels' work, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the
 * screen's part (banqi_live(), banqi_show()). --full for the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/banqi.h"

int bqb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats);
void bqb_root(const bqgame *g, void *root);

static bqgame T;
static unsigned deals;
const bqgame *banqi_live(void) { return &T; }

static int one(const char *s)
{
    size_t n = strlen(s);
    if (bq_status(&T) != BQ_PLAY || (n != 2 && n != 4)) return 0;
    return bq_script(&T, s);
}
int banqi_show(const char *move) { return one(move); }

static void reset(void) { bq_new(&T, 1000 + deals++ * 7919); }
static int actor(void) { return bq_pos(&T)->side; }
static int result(void)
{
    int s = bq_status(&T);
    return s == BQ_PLAY ? -1 : s == BQ_WIN0 ? 0 : s == BQ_WIN1 ? 1 : 2;
}

static void notation(bqmove m, char *out)
{
    if (m.kind == BQ_FLIP) sprintf(out, "%c%c", 'a' + BQ_X(m.from), '1' + BQ_Y(m.from));
    else sprintf(out, "%c%c%c%c", 'a' + BQ_X(m.from), '1' + BQ_Y(m.from), 'a' + BQ_X(m.to), '1' + BQ_Y(m.to));
}

static int random_move(ai_rng *r, char *m, int cap)
{
    bqmove l[BQ_MAX_MOVES];
    int n = bq_legal(bq_pos(&T), l);
    if (!n || cap < 5) return 0;
    notation(l[ai_rand_n(r, n)], m);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* a game played at random for n plies from deal d */
static void random_game(unsigned d, int n, ai_rng *r)
{
    bq_new(&T, d);
    for (int i = 0; i < n && bq_status(&T) == BQ_PLAY; i++) {
        char m[8];
        random_move(r, m, 8);
        one(m);
    }
}

int main(int argc, char **argv)
{
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    const bot *b = bot_find("banqi");
    check("registered: bot_find(\"banqi\")", b && b->think && b->play == banqi_show);
    {
        char m[BOT_MOVE_CAP] = "";
        bq_new(&T, 5);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        int ok = b->think(0, AI_EASY, m, sizeof m);
        check("the first turn: a piece turned over", ok && strlen(m) == 2 && one(m) && bq_pos(&T)->decided);
    }
    {   /* the pool: as many pieces as squares face down, at every ply of random games */
        ai_rng r;
        ai_seed(&r, 3);
        int good = 1;
        for (int gn = 0; gn < 200 && good; gn++) {
            bq_new(&T, (unsigned)gn);
            while (bq_status(&T) == BQ_PLAY && good) {
                static uint64_t root[512];
                bqb_root(&T, root);
                const uint8_t *pool = (const uint8_t *)root + sizeof(bqpos);
                int down = 0, in_pool = 0;
                for (int q = 0; q < BQ_N; q++) down += (bq_pos(&T)->sq[q] & BQ_HIDDEN) != 0;
                for (int k = 0; k < 16; k++) in_pool += pool[k];
                good = down == in_pool;
                char m[8];
                random_move(&r, m, 8);
                one(m);
            }
        }
        check("the pool: as many pieces as face down", good);
    }
    {   /* never peeks: the face-down pieces shuffled among their squares, the same move at every level */
        ai_rng r;
        ai_seed(&r, 11);
        int same = 1, tried = 0;
        for (int k = 0; k < 24 && same; k++) {
            random_game(100 + (unsigned)k, 10 + k, &r);
            if (bq_status(&T) != BQ_PLAY || !bq_pos(&T)->decided) continue;
            int seat = bq_pos(&T)->side;
            for (int lv = AI_EASY; lv <= AI_HARD && same; lv++) {
                char a[BOT_MOVE_CAP] = "", c[BOT_MOVE_CAP] = "";
                ai_set_seed(77);
                b->think(seat, lv, a, sizeof a);
                bqpos *p = &T.pos[T.ply], keep = *p;
                int hid[BQ_N], n = 0;
                for (int q = 0; q < BQ_N; q++)
                    if (p->sq[q] & BQ_HIDDEN) hid[n++] = q;
                for (int i = n - 1; i > 0; i--) {
                    int j = ai_rand_n(&r, i + 1);
                    uint8_t t = p->sq[hid[i]];
                    p->sq[hid[i]] = p->sq[hid[j]];
                    p->sq[hid[j]] = t;
                }
                ai_set_seed(77);
                b->think(seat, lv, c, sizeof c);
                *p = keep;
                same = !strcmp(a, c);
                tried++;
            }
        }
        char name[120];
        snprintf(name, sizeof name, "never peeks: the face-down pieces shuffled, the same move (%d positions)", tried);
        check(name, same && tried > 20);
    }
    {   /* a free capture: red's chariot next to black's horse, nothing to guard it */
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            char m[BOT_MOVE_CAP] = "", name[120];
            T.ply = 0;
            bqpos *p = &T.pos[0];
            memset(p, 0, sizeof *p);
            for (int q = 0; q < BQ_N; q++) p->sq[q] = BQ_HIDDEN | BQ_PC(q & 1, BQ_SOLDIER);
            p->sq[BQ_SQ(0, 0)] = BQ_PC(0, BQ_CHARIOT);
            p->sq[BQ_SQ(1, 0)] = BQ_PC(1, BQ_HORSE);
            p->sq[BQ_SQ(2, 0)] = 0;
            p->sq[BQ_SQ(0, 1)] = 0;
            p->sq[BQ_SQ(1, 1)] = 0;
            p->decided = 1;
            int ok = b->think(0, lv, m, sizeof m);
            snprintf(name, sizeof name, "the free horse taken (%s: %s)", sp_name(lv), m);
            check(name, ok && !strcmp(m, "a1b1") && one(m));
        }
    }
    {
        char m[BOT_MOVE_CAP] = "";
        ai_rng r;
        ai_seed(&r, 5);
        random_game(9, 12, &r);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(bq_pos(&T)->side, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {
        int npos = full ? 24 : 8;
        ai_rng r;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            int done = 0;
            for (int k = 0; k < npos; k++) {
                random_game(500 + (unsigned)k, 6 + 4 * k, &r);
                if (bq_status(&T) != BQ_PLAY || !bq_pos(&T)->decided) continue;
                static uint64_t root[512];
                bqb_root(&T, root);
                ai_stats s;
                char m[8];
                bqb_choose(root, lv, 5, m, &s);
                ms += s.ms;
                nodes += s.nodes;
                depth += s.depth;
                if (s.ms > worst) worst = s.ms;
                done++;
            }
            if (done)
                printf("     %-6s %8.0f nodes %6.1f ms (worst %6.1f) depth %4.1f\n", sp_name(lv), nodes / done,
                       ms / done, worst, depth / done);
        }
    }
    if (fails) return 1;
    static const sp_game G = {
        .id = "banqi", .max_plies = 4000, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 65},
        {AI_MEDIUM, AI_EASY, 50, 50},
        {AI_HARD, AI_EASY, 50, 65},
        {AI_HARD, AI_MEDIUM, 50, 50},
    };
    if (!full) P[1].min_pct = P[3].min_pct = 0;   /* three games: the close pairings' scores are noise (--full) */
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
