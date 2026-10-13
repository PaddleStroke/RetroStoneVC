/*
 * The Marienbad game's bot (src/games/nim_bot.c): its registry entry, hard's perfect play (from every winning
 * position it leaves one the opponent cannot win; checked by brute force on small layouts), the cancel, then
 * self-play at every level in the four variants (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (nim_live(), nim_show()). --full for the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/nim.h"

static nim_game T;                              /* the "screen's" game: the bot reads it */
static int variant, fresh;
const nim_game *nim_live(void) { return &T; }
int nim_show(const char *move)
{
    nim_game t = T;
    if (!nim_script(&t, move) || t.n != T.n + 1) return 0;
    T = t;
    return 1;
}

static void reset(void)
{
    nim_new(&T, variant, 1);
    fresh = 1;                                  /* the random layouts: dealt by the game's own seed, below */
}
static int actor(void) { return T.turn; }
static int result(void)
{
    int s = nim_status(&T);
    return s == NIM_PLAY ? -1 : s - NIM_WIN0;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (fresh) {                                /* the first move of a game: its layout from the game's seed */
        nim_new(&T, variant, ai_rand(r));
        fresh = 0;
    }
    int rows[NIM_ROWS], n = 0;
    for (int i = 0; i < T.rows; i++)
        if (T.mask[i]) rows[n++] = i;
    if (!n) return 0;
    int row = rows[ai_rand_n(r, n)];
    snprintf(m, (size_t)cap, "%c%d", 'a' + row, 1 + ai_rand_n(r, nim_count(T.mask[row])));
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* brute force: can the player to move win? (rows as counts) */
static int wins(int *c, int rows, int misere)
{
    int left = 0;
    for (int i = 0; i < rows; i++) left += c[i];
    if (!left) return misere;                   /* the other took the last: misere, they lost */
    for (int i = 0; i < rows; i++)
        for (int k = 1; k <= c[i]; k++) {
            c[i] -= k;
            int w = !wins(c, rows, misere);
            c[i] += k;
            if (w) return 1;
        }
    return 0;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("nim");
    check("registered: bot_find(\"nim\")", b && b->think && b->play == nim_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], name[160];
    {   /* hard against brute force: every position of small layouts (3 rows of up to 4), both ways of play */
        int good = 0, tried = 0;
        for (int misere = 0; misere < 2; misere++)
            for (int a = 0; a <= 4; a++)
                for (int bb = 0; bb <= 4; bb++)
                    for (int c = 0; c <= 4; c++) {
                        if (!(a + bb + c)) continue;
                        nim_new(&T, misere ? NIM_RANDOM : NIM_RANDOM_NORMAL, 1);
                        T.rows = 3;
                        int cnt[3] = {a, bb, c};
                        for (int i = 0; i < 3; i++) T.start[i] = 4, T.mask[i] = (uint8_t)((1u << cnt[i]) - 1);
                        T.n = 1;                /* a game under way (nim_status: not over before a move) */
                        int can = wins(cnt, 3, misere);
                        if (!b->think(T.turn, AI_HARD, m, sizeof m) || !nim_show(m)) { tried++; continue; }
                        int after[3];
                        for (int i = 0; i < 3; i++) after[i] = nim_count(T.mask[i]);
                        tried++;
                        good += !can || !wins(after, 3, misere);
                    }
        snprintf(name, sizeof name, "hard always leaves a lost position when it can: %d of %d", good, tried);
        check(name, good == tried);
    }
    {   /* Marienbad, first move: the nim-sum is zero (a loss for the first player): hard takes one match */
        nim_new(&T, NIM_MARIENBAD, 1);
        check("a lost position: hard takes one match from the longest row",
              b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "d1"));
    }
    {   /* not its turn: no move; cancelled: no move */
        nim_new(&T, NIM_MARIENBAD, 1);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        ai_cancel();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        ai_cancel_clear();
        check("cancelled: no move", !ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "nim", .max_plies = NIM_MAX_MOVES, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = nim_show,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 60},
        {AI_MEDIUM, AI_EASY, 200, 55},
        {AI_HARD, AI_EASY, 200, 60},
        {AI_HARD, AI_MEDIUM, 200, 52},
        {AI_HARD, AI_HARD, 20, 0},
    };
    static const char *NAME[NIM_VARIANTS] = {"Marienbad (misere)", "Marienbad (normal)", "random (misere)",
                                             "random (normal)"};
    int bad = 0;
    for (variant = 0; variant < NIM_VARIANTS; variant++) {
        printf("-- %s\n", NAME[variant]);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
