/*
 * Jungle's bot (src/games/jungle_bot.c): its registry entry, the den entered, a free wolf, the cancel, the levels'
 * work, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (jungle_live(), jungle_show()). --full for the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/jungle.h"

int jgb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats);
void jgb_root(const jggame *g, void *root);

static jggame T;
const jggame *jungle_live(void) { return &T; }

static int one(const char *s)
{
    if (jg_status(&T) != JS_PLAY || strlen(s) != 4) return 0;
    return jg_script(&T, s);
}
int jungle_show(const char *move) { return one(move); }

static void reset(void) { jg_new(&T); }
static int actor(void) { return jg_pos(&T)->side; }
static int result(void)
{
    jgstatus s = jg_status(&T);
    return s == JS_PLAY ? -1 : s == JS_DEN || s == JS_ALL || s == JS_STUCK ? !jg_pos(&T)->side : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    jgmove l[JG_MAX_MOVES];
    int n = jg_legal(jg_pos(&T), l);
    if (!n || cap < 5) return 0;
    jgmove x = l[ai_rand_n(r, n)];
    sprintf(m, "%c%c%c%c", 'a' + JG_FILE(x.from), '1' + JG_ROW(x.from), 'a' + JG_FILE(x.to), '1' + JG_ROW(x.to));
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* a position from lists of animals: R rat, C cat, D dog, W wolf, P leopard, T tiger, L lion, E elephant ("Ld8") */
static void setup(const char *p0, const char *p1)
{
    T.ply = 0;
    jgpos *p = &T.pos[0];
    memset(p, 0, sizeof *p);
    for (int c = 0; c < 2; c++)
        for (const char *s = c ? p1 : p0; *s; s += s[3] ? 4 : 3) {
            p->sq[jg_square(s + 1)] = (uint8_t)JG_PC(c, (int)(strchr(" RCDWPTLE", s[0]) - " RCDWPTLE"));
            p->left[c]++;
        }
}

static void solve(const char *what, const char *p0, const char *p1, const char *want, int from)
{
    const bot *b = bot_find("jungle");
    for (int l = from; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP] = "", name[160];
        setup(p0, p1);
        int ok = b->think(0, l, m, sizeof m);
        snprintf(name, sizeof name, "%s (%s: %s)", what, sp_name(l), m);
        check(name, ok && strstr(want, m) && one(m));
    }
}

int main(int argc, char **argv)
{
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    const bot *b = bot_find("jungle");
    check("registered: bot_find(\"jungle\")", b && b->think && b->play == jungle_show);
    {
        char m[BOT_MOVE_CAP] = "";
        jg_new(&T);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    solve("into the den", "Ld8 Ea1", "Ra7 Tg9", "d8d9", AI_EASY);
    solve("the free wolf taken", "Ea5 Rg1", "Wa6 Lg9", "a5a6", AI_EASY);
    {
        char m[BOT_MOVE_CAP] = "";
        jg_new(&T);
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
                jg_new(&T);
                for (int i = 0, n = 6 + 3 * k; i < n && jg_status(&T) == JS_PLAY; i++) {
                    char m[8];
                    random_move(&r, m, 8);
                    one(m);
                }
                if (jg_status(&T) != JS_PLAY) continue;
                static uint64_t root[1024];
                jgb_root(&T, root);
                ai_stats s;
                char m[8];
                jgb_choose(root, lv, 5, m, &s);
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
        .id = "jungle", .max_plies = 1700, .opening = 2, .reset = reset, .actor = actor, .result = result,
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
