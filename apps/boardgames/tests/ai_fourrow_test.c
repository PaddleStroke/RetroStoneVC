/*
 * Four in a row's bot (src/games/fourrow_bot.c): its registry entry, a four taken and a four stopped at every level,
 * the cancel, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (fourrow_live(), fourrow_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/fourrow.h"

static fr_game T;                               /* the "screen's" game: the bot reads it */
const fr_game *fourrow_live(void) { return &T; }

/* one move as fourrow.c's script() and fourrow_show() take it: "4", a column with room */
static int one(const char *s)
{
    char *end;
    long c = strtol(s, &end, 10);
    if (end == s || *end || c < 1 || c > FR_COLS) return 0;
    return fr_play(&T, (int)c - 1) >= 0;
}
int fourrow_show(const char *move) { return one(move); }

static int line(const char *s)                  /* "4 4 3 5" from the start */
{
    fr_new(&T);
    for (char *end; *s; s = end) {
        long c = strtol(s, &end, 10);
        if (end == s) break;
        if (fr_play(&T, (int)c - 1) < 0) return 0;
    }
    return 1;
}

static void reset(void) { fr_new(&T); }
static int actor(void) { return fr_turn(&T); }
static int result(void)
{
    int s = fr_status(&T);
    return s == FR_PLAY ? -1 : s == FR_WIN0 ? 0 : s == FR_WIN1 ? 1 : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    int open[FR_COLS], n = 0;
    for (int c = 0; c < FR_COLS; c++)
        if (T.height[c] < FR_ROWS) open[n++] = c;
    if (!n) return 0;
    snprintf(m, (size_t)cap, "%d", open[ai_rand_n(r, n)] + 1);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("fourrow");
    check("registered: bot_find(\"fourrow\")", b && b->think && b->play == fourrow_show);
    char name[96];
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP] = "";
        /* ivory has 1 2 3 along the bottom: it plays 4 and wins */
        line("1 1 2 2 3 3");
        int ok = b->think(0, l, m, sizeof m) && !strcmp(m, "4");
        snprintf(name, sizeof name, "%s takes the four (%s)", sp_name(l), m);
        check(name, ok);
        /* ebony to move, ivory threatens 4 along the bottom: ebony stops it */
        line("1 7 2 7 3");
        ok = b->think(1, l, m, sizeof m) && !strcmp(m, "4");
        snprintf(name, sizeof name, "%s stops the four (%s)", sp_name(l), m);
        check(name, ok);
    }
    {   /* hard sees two threats coming: ivory 2 3 4 on the bottom row open both ways after "3 3 4 4" */
        char m[BOT_MOVE_CAP] = "";
        line("4 4 3 3");                        /* ivory to move: 2 or 5 makes an open three: a win */
        int ok = b->think(0, AI_HARD, m, sizeof m) && (!strcmp(m, "2") || !strcmp(m, "5"));
        snprintf(name, sizeof name, "hard makes the open three (%s)", m);
        check(name, ok);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        line("");
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work on positions of random games (not checked: the calibration, alone on one core) */
        int npos = 8;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 40;
        ai_rng r;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            for (int k = 0; k < npos; k++) {
                fr_new(&T);
                for (int i = 0, n = 2 + k / 2; i < n && fr_status(&T) == FR_PLAY; i++) {
                    char m[4];
                    random_move(&r, m, 4);
                    one(m);
                }
                if (fr_status(&T) != FR_PLAY) fr_undo(&T);
                ai_stats s;
                frb_choose(&T, lv, 5, &s);
                ms += s.ms;
                nodes += s.nodes;
                depth += s.depth;
                if (s.ms > worst) worst = s.ms;
            }
            printf("     %-6s %8.0f nodes %6.1f ms (worst %6.1f) depth %4.1f\n", sp_name(lv), nodes / npos,
                   ms / npos, worst, depth / npos);
        }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "fourrow", .max_plies = FR_HOLES, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 85},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 85},
        {AI_HARD, AI_MEDIUM, 40, 60},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
