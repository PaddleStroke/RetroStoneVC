/*
 * Janggi's bot (src/games/janggi_bot.c): its own moves against the rules' on random games, its registry entry, a free
 * horse, a mate in one, the cancel, the levels' work, then self-play at every level (tests/ai_selfplay.h). Headless:
 * this file plays the screen's part (janggi_live(), janggi_show()). --full for the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/janggi.h"

int jnb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats);
void jnb_root(const jgame *g, void *root);
int jnb_legal(const jpos *p, jmove *out);

static jgame T;
const jgame *janggi_live(void) { return &T; }

static int one(const char *s)
{
    jmove l[JG_MAX_MOVES];
    if (jn_status(&T) != JG_PLAY) return 0;
    if (!strcmp(s, "pass")) return jn_can_pass(jn_pos(&T)) && jn_play(&T, (jmove){JG_PASS, JG_PASS, 0});
    if (strlen(s) != 4) return 0;
    int k = jn_parse(l, jn_legal(jn_pos(&T), l), s);
    return k >= 0 && jn_play(&T, l[k]);
}
int janggi_show(const char *move) { return one(move); }

static void reset(void) { jn_new(&T); }
static int actor(void) { return jn_pos(&T)->side; }
static int result(void)
{
    jstatus s = jn_status(&T);
    return s == JG_PLAY ? -1 : s == JG_MATE ? !jn_pos(&T)->side : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    jmove l[JG_MAX_MOVES];
    const jpos *p = jn_pos(&T);
    int n = jn_legal(p, l), pass = jn_can_pass(p);
    if ((!n && !pass) || cap < 5) return 0;
    int k = ai_rand_n(r, n + pass);             /* the pass as likely as any one move */
    if (k == n) { strcpy(m, "pass"); return 1; }
    sprintf(m, "%c%c%c%c", 'a' + JG_X(l[k].from), '0' + JG_Y(l[k].from), 'a' + JG_X(l[k].to), '0' + JG_Y(l[k].to));
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* want NULL: the move must mate */
static void solve(const char *what, const char *cho, const char *han, const char *want, int from)
{
    const bot *b = bot_find("janggi");
    for (int l = from; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP] = "", name[160];
        T.ply = 0;
        jn_setup(&T.pos[0], 0, cho, han);
        int ok = b->think(0, l, m, sizeof m);
        snprintf(name, sizeof name, "%s (%s: %s)", what, sp_name(l), m);
        check(name, ok && (want ? strstr(want, m) != NULL : 1) && one(m) && (want || jn_status(&T) == JG_MATE));
    }
}

int main(int argc, char **argv)
{
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    {   /* the bot's moves are the rules' exactly: random games (the pass among the moves), every position */
        ai_rng r;
        ai_seed(&r, 7);
        int same = 1, positions = 0;
        for (int gn = 0; gn < (full ? 2000 : 400) && same; gn++) {
            jn_new(&T);
            for (int ply = 0; ply < 400 && same && jn_status(&T) == JG_PLAY; ply++) {
                jmove a[JG_MAX_MOVES], c[JG_MAX_MOVES];
                int na = jn_legal(jn_pos(&T), a), nc = jnb_legal(jn_pos(&T), c);
                same = na == nc;
                for (int i = 0; i < na && same; i++) {
                    int found = 0;
                    for (int j = 0; j < nc && !found; j++) found = !memcmp(&a[i], &c[j], sizeof a[0]);
                    same = found;
                }
                if (!same) printf("     differs at ply %d of game %d (rules %d, bot %d)\n", ply, gn, na, nc);
                positions++;
                char m[8];
                random_move(&r, m, 8);
                one(m);
            }
        }
        printf("     (%d positions)\n", positions);
        check("the bot's moves = the rules' (the palace lines, the cannon, the facing generals)", same);
    }
    const bot *b = bot_find("janggi");
    check("registered: bot_find(\"janggi\")", b && b->think && b->play == janggi_show);
    {
        char m[BOT_MOVE_CAP] = "";
        jn_new(&T);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    solve("the free horse taken", "Ke1 Ra4", "Kd8 He4", "a4e4", AI_EASY);
    solve("mate in one", "Ke1 Ra7 Rb8", "Kd9", NULL, AI_EASY);
    {
        char m[BOT_MOVE_CAP] = "";
        jn_new(&T);
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
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            int done = 0;
            for (int k = 0; k < npos; k++) {
                jn_new(&T);
                for (int i = 0, n = 6 + 3 * k; i < n && jn_status(&T) == JG_PLAY; i++) {
                    char m[8];
                    random_move(&r, m, 8);
                    one(m);
                }
                if (jn_status(&T) != JG_PLAY) continue;
                static uint64_t root[1024];
                jnb_root(&T, root);
                ai_stats s;
                char m[8];
                jnb_choose(root, lv, 5, m, &s);
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
        .id = "janggi", .max_plies = 2000, .opening = 2, .reset = reset, .actor = actor, .result = result,
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
