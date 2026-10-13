/*
 * Backgammon's bot (src/games/backgammon_bot.c), its four games (backgammon, plakoto, fevga, long nardy): the ways it
 * plays a throw against the rules' own moves on random games (every first move it finds is one of bg_legal()'s and
 * none is missing), its registry entry, the throw it makes ("roll"), a plakoto mother pinned for the game, a race
 * borne off, the cancel, then self-play at every level for each game (tests/ai_selfplay.h). Headless: this file plays
 * the screen's part (backgammon_live(), backgammon_show(): "roll" throws dice of its own, from the session's seed the
 * harness sets before each think, so every game repeats). --full for the 200 games and the tables; --run v for one
 * game only (0 backgammon, 1 plakoto, 2 fevga, 3 long nardy).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/backgammon.h"

int bgb_ends(const bpos *p, bpos *out, int cap);
int bgb_choose(const bpos *p, int level, uint64_t seed, bmove *first, ai_stats *stats);
void bgb_notation(bmove m, char *out);

static bgame T;                                 /* the "screen's" game: the bot reads it */
static int V;
const bgame *backgammon_live(void) { return &T; }

static int one(const char *m)
{
    if (!strcmp(m, "roll")) {                   /* dice from the session's seed and the events so far */
        const bpos *p = bg_pos(&T);
        if (p->phase != BG_OPENING && p->phase != BG_THROW) return 0;
        ai_rng r;
        ai_seed(&r, ai_mix(ai_session_seed(), (uint64_t)T.n));
        int a = 1 + ai_rand_n(&r, 6);
        return bg_throw(&T, a, 1 + ai_rand_n(&r, 6));
    }
    return bg_moves(&T, m);
}
int backgammon_show(const char *move) { return one(move); }

static void reset(void) { bg_new(&T, V); }
static int actor(void) { return bg_pos(&T)->side; }
static int result(void)
{
    int s = bg_status(&T);
    return s == BG_PLAY ? -1 : s == BG_WIN0 ? 0 : s == BG_WIN1 ? 1 : 2;
}

/* the random player: its own dice, then any legal move of one die */
static int random_move(ai_rng *r, char *m, int cap)
{
    const bpos *p = bg_pos(&T);
    if (cap < 12) return 0;
    if (p->phase == BG_OPENING || p->phase == BG_THROW) {
        snprintf(m, (size_t)cap, "%d%d:", 1 + ai_rand_n(r, 6), 1 + ai_rand_n(r, 6));
        return 1;
    }
    bmove l[BG_MAXMOVES];
    int n = bg_legal(p, l);
    if (!n) return 0;
    bgb_notation(l[ai_rand_n(r, n)], m);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

static void clear(bpos *p)
{
    memset(p->n, 0, sizeof p->n);
    memset(p->pin, 0, sizeof p->pin);
}

static void dice(bpos *p, int a, int b)
{
    p->phase = BG_MOVE;
    p->used = 0;
    p->die[0] = (signed char)a;
    p->die[1] = (signed char)b;
    p->nd = 2;
    if (a == b) p->die[2] = p->die[3] = (signed char)a, p->nd = 4;
}

/* two positions alike: the checkers and the pins */
static int alike(const bpos *a, const bpos *b)
{
    return !memcmp(a->n, b->n, sizeof a->n) && !memcmp(a->pin, b->pin, sizeof a->pin);
}

/* every end of the turn by the rules' moves (bg_legal() until it has none), each once */
static void ends(const bpos *p, bpos *out, int *n, int cap)
{
    bmove l[BG_MAXMOVES];
    int k = bg_legal(p, l);
    if (!k) {
        for (int i = 0; i < *n; i++)
            if (alike(&out[i], p)) return;
        if (*n < cap) out[(*n)++] = *p;
        return;
    }
    for (int i = 0; i < k; i++) {
        bpos q = *p;
        bg_apply(&q, l[i]);
        ends(&q, out, n, cap);
    }
}

static const char *NAMES[BG_VARIANTS] = {"backgammon", "plakoto", "fevga", "long nardy"};

int main(int argc, char **argv)
{
    char name[200];
    int full = 0, only = -1;
    for (int i = 1; i < argc; i++) {
        full |= !strcmp(argv[i], "--full");
        if (!strcmp(argv[i], "--run") && i + 1 < argc) only = atoi(argv[i + 1]);
    }
    /* the ways lead exactly where the rules' moves do: random games of the four, every position to move; and the
     * first move each level picks is a legal one */
    ai_rng r;
    ai_seed(&r, 7);
    int same = 1, positions = 0, games = full ? 160 : 60, picks = 1;
    bpos *mine = malloc(sizeof(bpos) * 4096), *rules = malloc(sizeof(bpos) * 4096);
    for (int gn = 0; gn < games && same; gn++) {
        V = gn % BG_VARIANTS;
        reset();
        for (int ply = 0; ply < 3000 && bg_status(&T) == BG_PLAY && same; ply++) {
            const bpos *p = bg_pos(&T);
            if (p->phase == BG_MOVE) {
                int k = bgb_ends(p, mine, 4096), n = 0;
                ends(p, rules, &n, 4096);
                same = k == n;
                for (int i = 0; i < k && same; i++) {
                    int got = 0;
                    for (int j = 0; j < n && !got; j++) got = alike(&mine[i], &rules[j]);
                    same = got;
                }
                if (!same)
                    printf("     %s, ply %d: dice %d %d (used %x), %d ends by the rules, %d by the bot\n", NAMES[V],
                           ply, p->die[0], p->die[1], p->used, n, k);
                for (int lv = AI_EASY; lv <= AI_HARD && gn % 8 == 0; lv++) {
                    bmove f, m;
                    picks &= bgb_choose(p, lv, (uint64_t)ply, &f, NULL) && bg_find(p, f.from, f.to, &m);
                }
                positions++;
            }
            char m[16];
            random_move(&r, m, sizeof m);
            bg_moves(&T, m);
        }
    }
    free(mine);
    free(rules);
    snprintf(name, sizeof name, "the ways' ends = the rules' moves played out (%d positions, the four games)",
             positions);
    check(name, same);
    check("every level's first move is legal", picks);

    const bot *b = bot_find("backgammon");
    check("registered: bot_find(\"backgammon\")", b && b->think && b->play == backgammon_show);
    {
        char m[BOT_MOVE_CAP] = "";
        V = BG_BACKGAMMON;
        reset();
        check("the opening roll: \"roll\"", b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "roll"));
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    {   /* plakoto: the other's mother alone on its start (our 1), ours gone from our 24: 4/1 pins it, the game won */
        int all = 1;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            V = BG_PLAKOTO;
            reset();
            bpos *p = &T.pos[T.n];
            clear(p);
            p->n[0][4] = 1;
            p->n[0][10] = 14;
            p->n[1][24] = 1;
            p->n[1][13] = 14;
            p->side = 0;
            dice(p, 3, 5);
            char m[BOT_MOVE_CAP] = "";
            all &= b->think(0, lv, m, sizeof m) && (!strcmp(m, "4/1*") || !strcmp(m, "4/1"));
        }
        check("plakoto: every level pins the mother when it wins", all);
    }
    {   /* backgammon, a race: two checkers on 2 and 1, a 2-1 bears both off; the bot's moves (two thinks) win */
        V = BG_BACKGAMMON;
        reset();
        bpos *p = &T.pos[T.n];
        clear(p);
        p->n[0][2] = p->n[0][1] = 1;
        p->n[0][BG_OFF] = 13;
        p->n[1][6] = 15;
        p->side = 0;
        dice(p, 2, 1);
        char m[BOT_MOVE_CAP] = "";
        int ok = b->think(0, AI_HARD, m, sizeof m) && one(m);
        ok = ok && b->think(0, AI_HARD, m, sizeof m) && one(m);
        check("a race: the last two borne off", ok && bg_status(&T) == BG_WIN0);
    }
    {   /* cancelled: hard gives up at once, and says so */
        V = BG_BACKGAMMON;
        reset();
        one("31:");
        one("64:");                             /* not legal yet: ivory has the 3-1 to play */
        char m[BOT_MOVE_CAP] = "";
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work on positions of random games, a checker's choice each (not checked: the calibration) */
        int npos = full ? 60 : 12;
        for (int v = 0; v < BG_VARIANTS; v++) {
            if (only >= 0 && v != only) continue;
            ai_seed(&r, 1234 + (uint64_t)v);
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                double ms = 0, worst = 0, work = 0;
                int k = 0;
                while (k < npos) {
                    V = v;
                    reset();
                    int plies = 4 + ai_rand_n(&r, 150);
                    for (int i = 0; i < plies && bg_status(&T) == BG_PLAY; i++) {
                        char m[16];
                        random_move(&r, m, sizeof m);
                        bg_moves(&T, m);
                    }
                    const bpos *p = bg_pos(&T);
                    if (p->phase != BG_MOVE || p->used) continue;
                    ai_stats s;
                    bmove f;
                    bgb_choose(p, lv, 5, &f, &s);
                    ms += s.ms;
                    work += s.nodes;
                    if (s.ms > worst) worst = s.ms;
                    k++;
                }
                printf("     %-10s %-6s %8.0f positions %7.2f ms a choice (worst %7.2f)\n", NAMES[v], sp_name(lv),
                       work / npos, ms / npos, worst);
            }
        }
    }
    if (fails) return 1;

    int bad = 0;
    for (int v = 0; v < BG_VARIANTS; v++) {
        if (only >= 0 && v != only) continue;
        V = v;
        printf("== %s\n", NAMES[v]);
        const sp_game G = {
            .id = "backgammon", .max_plies = 4000, .opening = 0, .reset = reset, .actor = actor, .result = result,
            .random_move = random_move, .script = one,
        };
        const sp_pair P[] = {
            {AI_EASY, SP_RANDOM, 200, 75},
            {AI_MEDIUM, AI_EASY, 200, 55},
            {AI_HARD, AI_EASY, 200, 60},
            {AI_HARD, AI_MEDIUM, 200, 50},
        };
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
