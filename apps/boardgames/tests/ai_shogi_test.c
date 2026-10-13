/*
 * Shogi's bot (src/games/shogi_bot.c): its own move generator against the rules' (shogi_rules.c) on random games of
 * both boards, its registry entry, a free rook, a drop that mates, the cancel, the levels' work, then self-play at
 * every level on both boards (tests/ai_selfplay.h). Headless: this file plays the screen's part (shogi_live(),
 * shogi_show()). --full for the 200 games a pairing and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/shogi.h"

int sgb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats);
void sgb_root(const sgame *g, void *root);
int sgb_legal(const spos *p, smove *out);

static sgame T;
static int variant;
const sgame *shogi_live(void) { return &T; }

static int one(const char *s)
{
    smove l[SG_MAX_MOVES];
    int used = 0;
    if (sg_status(&T) != SG_PLAY) return 0;
    int k = sg_parse(sg_pos(&T)->n, l, sg_legal(sg_pos(&T), l), s, &used);
    return k >= 0 && !s[used] && sg_play(&T, l[k]);
}
int shogi_show(const char *move) { return one(move); }

static void reset(void) { sg_new(&T, variant); }
static int actor(void) { return sg_pos(&T)->side; }
static int result(void)
{
    sstatus s = sg_status(&T);
    return s == SG_PLAY ? -1 : s == SG_WON0 ? 0 : s == SG_WON1 ? 1 : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    smove l[SG_MAX_MOVES];
    int n = sg_legal(sg_pos(&T), l);
    if (!n || cap < 6) return 0;
    sg_usi(sg_pos(&T)->n, l[ai_rand_n(r, n)], m);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

static int cmp(const void *a, const void *b)
{
    const smove *x = a, *y = b;
    int u = x->from << 24 | x->to << 16 | x->drop << 8 | x->promo, v = y->from << 24 | y->to << 16 | y->drop << 8 | y->promo;
    return (u > v) - (u < v);
}

static void solve(const char *what, const char *sfen, const char *want, int from)
{
    const bot *b = bot_find("shogi");
    for (int l = from; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP] = "", name[160];
        T.ply = 0;
        sg_from_sfen(&T.pos[0], sfen);
        int ok = b->think(0, l, m, sizeof m);
        snprintf(name, sizeof name, "%s (%s: %s)", what, sp_name(l), m);
        check(name, ok && (want ? strstr(want, m) != NULL : 1) && one(m) && (want || sg_status(&T) == SG_WON0));
    }
}

int main(int argc, char **argv)
{
    int full = 0, only = -1;
    for (int i = 1; i < argc; i++) {
        full |= !strcmp(argv[i], "--full");
        if (!strcmp(argv[i], "--variant") && i + 1 < argc) only = atoi(argv[++i]);
    }
    {   /* the bot's moves are the rules' exactly: random games on both boards, every position */
        ai_rng r;
        ai_seed(&r, 7);
        int same = 1, positions = 0;
        for (int gn = 0; gn < (full ? 1200 : 300) && same; gn++) {
            sg_new(&T, gn & 1);
            for (int ply = 0; ply < 300 && same; ply++) {
                smove a[SG_MAX_MOVES], b[SG_MAX_MOVES];
                int na = sg_legal(sg_pos(&T), a), nb = sgb_legal(sg_pos(&T), b);
                qsort(a, (size_t)na, sizeof a[0], cmp);
                qsort(b, (size_t)nb, sizeof b[0], cmp);
                same = na == nb && !memcmp(a, b, sizeof a[0] * (size_t)na);
                positions++;
                if (!same) {
                    char usi[8];
                    printf("     differs at ply %d of game %d (rules %d, bot %d moves)\n", ply, gn, na, nb);
                    for (int i = 0; i < na; i++) { sg_usi(sg_pos(&T)->n, a[i], usi); printf(" %s", usi); }
                    printf("\n");
                    for (int i = 0; i < nb; i++) { sg_usi(sg_pos(&T)->n, b[i], usi); printf(" %s", usi); }
                    printf("\n");
                }
                if (!na) break;
                sg_play(&T, a[ai_rand_n(&r, na)]);
            }
        }
        printf("     (%d positions)\n", positions);
        check("the bot's moves = the rules' (promotions, drops, the pawn drop mate)", same);
    }
    const bot *b = bot_find("shogi");
    check("registered: bot_find(\"shogi\")", b && b->think && b->play == shogi_show);
    {
        char m[BOT_MOVE_CAP] = "";
        sg_new(&T, SG_STANDARD);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    solve("the free rook taken", "4k4/9/9/9/4r4/9/9/9/B7K b - 1", "9i5e", AI_EASY);
    solve("a gold dropped mates", "4k4/9/4P4/9/9/9/9/9/8K b G 1", NULL, AI_EASY);
    solve("mini shogi: the free rook taken", "rbsgk/5/R4/5/4K b - 1", "5c5a+ 5c5a", AI_MEDIUM);
    {
        char m[BOT_MOVE_CAP] = "";
        sg_new(&T, SG_STANDARD);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {
        int npos = full ? 24 : 8;
        ai_rng r;
        ai_seed(&r, 1234);
        for (int v = 0; v < SG_VARIANTS; v++) {
            printf("     %s\n", v ? "mini shogi" : "shogi");
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                double ms = 0, nodes = 0, depth = 0, worst = 0;
                int done = 0;
                for (int k = 0; k < npos; k++) {
                    sg_new(&T, v);
                    for (int i = 0, n = 6 + 3 * k; i < n && sg_status(&T) == SG_PLAY; i++) {
                        char m[8];
                        random_move(&r, m, 8);
                        one(m);
                    }
                    if (sg_status(&T) != SG_PLAY) continue;
                    static uint64_t root[1024];
                    sgb_root(&T, root);
                    ai_stats s;
                    char m[8];
                    sgb_choose(root, lv, 5, m, &s);
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
    }
    if (fails) return 1;
    static const sp_game G = {
        .id = "shogi", .max_plies = 1500, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 65},
        {AI_MEDIUM, AI_EASY, 50, 50},
        {AI_HARD, AI_EASY, 50, 65},
        {AI_HARD, AI_MEDIUM, 50, 50},
    };
    int bad = 0;
    for (variant = 0; variant < SG_VARIANTS; variant++) {
        if (only >= 0 && variant != only) continue;
        printf("---- %s\n", variant ? "mini shogi" : "shogi");
        fflush(stdout);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
