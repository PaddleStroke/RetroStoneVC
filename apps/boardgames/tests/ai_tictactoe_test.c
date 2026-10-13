/*
 * Tic-tac-toe's bot (src/games/tictactoe_bot.c): its registry entry, a few positions (the win taken, the block made,
 * hard never losing the classic game against anyone), the cancel, then self-play at every level in both variants
 * (tests/ai_selfplay.h). Headless: this file plays the screen's part (tictactoe_live(), tictactoe_show()).
 * --full for the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/tictactoe.h"

static tt_game T;                               /* the "screen's" game: the bot reads it */
static int ultimate;
const tt_game *tictactoe_live(void) { return &T; }
int tictactoe_show(const char *move)
{
    tt_game t = T;
    if (!tt_script(&t, move) || t.n != T.n + 1) return 0;
    T = t;
    return 1;
}

static void reset(void) { tt_new(&T, ultimate); }
static int actor(void) { return tt_turn(&T); }
static int result(void)
{
    int s = tt_status(&T);
    return s == TT_PLAY ? -1 : s == TT_WIN0 ? 0 : s == TT_WIN1 ? 1 : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    int legal[81], n = 0;
    for (int c = 0; c < tt_cells(&T); c++)
        if (tt_legal(&T, c)) legal[n++] = c;
    if (!n) return 0;
    int c = legal[ai_rand_n(r, n)];
    if (T.ultimate) snprintf(m, (size_t)cap, "%d.%d", c / 9 + 1, c % 9 + 1);
    else snprintf(m, (size_t)cap, "%d", c + 1);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* classic, exhaustively: the best result the side to move can force (1 win, 0 draw, -1 loss) */
static int solve(tt_game *g)
{
    int s = tt_status(g);
    if (s != TT_PLAY) return s == TT_DRAW ? 0 : -1;   /* the last mover won */
    int best = -2;
    for (int c = 0; c < 9; c++)
        if (tt_play(g, c)) {
            int v = -solve(g);
            tt_undo(g);
            if (v > best) best = v;
        }
    return best;
}

/* hard against every reply in every line: never a loss (seat: hard's) */
static int never_loses(int seat, long *lines)
{
    int s = tt_status(&T);
    if (s != TT_PLAY) { ++*lines; return !(s == (seat ? TT_WIN0 : TT_WIN1)); }
    if (tt_turn(&T) == seat) {
        char m[BOT_MOVE_CAP];
        ai_set_seed(ai_mix(77, (uint64_t)T.n));
        if (!bot_find("tictactoe")->think(seat, AI_HARD, m, sizeof m)) return 0;
        tt_game keep = T;
        if (!tictactoe_show(m)) return 0;
        int ok = never_loses(seat, lines);
        T = keep;
        return ok;
    }
    for (int c = 0; c < 9; c++) {
        tt_game keep = T;
        if (tt_play(&T, c)) {
            int ok = never_loses(seat, lines);
            T = keep;
            if (!ok) return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("tictactoe");
    check("registered: bot_find(\"tictactoe\")", b && b->think && b->play == tictactoe_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], name[160];
    {   /* every level takes a win in one; medium and hard block one */
        int all = 1;
        for (int l = AI_EASY; l <= AI_HARD; l++) {
            tt_new(&T, 0);
            tt_script(&T, "1 4 2 5");           /* ivory: 1 2, ebony 4 5: ivory wins on 3 */
            all &= b->think(0, l, m, sizeof m) && !strcmp(m, "3");
        }
        check("every level takes the win in one (classic)", all);
        all = 1;
        for (int l = AI_MEDIUM; l <= AI_HARD; l++) {
            tt_new(&T, 0);
            tt_script(&T, "1 5 2");             /* ebony must block on 3 */
            all &= b->think(1, l, m, sizeof m) && !strcmp(m, "3");
        }
        check("medium and hard block (classic)", all);
    }
    {   /* hard is perfect: its move keeps the position's value, from 200 random positions */
        ai_rng r;
        ai_seed(&r, 5);
        int good = 0, tried = 0;
        for (int k = 0; k < 200; k++) {
            tt_new(&T, 0);
            for (int i = 0, n = ai_rand_n(&r, 6); i < n && tt_status(&T) == TT_PLAY; i++) {
                random_move(&r, m, sizeof m);
                tictactoe_show(m);
            }
            if (tt_status(&T) != TT_PLAY) continue;
            tt_game g = T;
            int want = solve(&g);
            b->think(tt_turn(&T), AI_HARD, m, sizeof m);
            g = T;
            tictactoe_show(m);
            int got = -solve(&T);
            T = g;
            tried++;
            good += got == want;
        }
        snprintf(name, sizeof name, "hard keeps the value of every position (classic): %d of %d", good, tried);
        check(name, good == tried);
        long lines = 0;
        int ok = 1;
        for (int seat = 0; seat < 2; seat++) {
            tt_new(&T, 0);
            ok &= never_loses(seat, &lines);
        }
        snprintf(name, sizeof name, "hard never loses, either seat, against every line (%ld games)", lines);
        check(name, ok);
    }
    {   /* Ultimate: the board's win taken (board 5: ivory has 5.1 5.2, sent there) */
        tt_new(&T, 1);
        int ok = tt_script(&T, "5.1 1.5 5.2 2.5");
        int all = ok;
        for (int l = AI_MEDIUM; l <= AI_HARD; l++) all &= b->think(0, l, m, sizeof m) && !strcmp(m, "5.3");
        check("Ultimate: medium and hard take a small board", all);
    }
    {   /* not its turn: no move; cancelled: no move, at once */
        tt_new(&T, 1);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    if (fails) return 1;

    static const sp_game C = {
        .id = "tictactoe", .max_plies = 9, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = tictactoe_show,
    };
    static const sp_pair PC[] = {
        {AI_EASY, SP_RANDOM, 200, 75},
        {AI_MEDIUM, AI_EASY, 200, 55},
        {AI_HARD, AI_EASY, 200, 60},
        {AI_HARD, AI_MEDIUM, 200, 50},
        {AI_HARD, SP_RANDOM, 200, 85},
        {AI_HARD, AI_HARD, 20, 0},
    };
    printf("-- classic\n");
    ultimate = 0;
    int bad = sp_run(&C, PC, (int)(sizeof PC / sizeof PC[0]), argc, argv);
    static const sp_game U = {
        .id = "tictactoe", .max_plies = 81, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = tictactoe_show,
    };
    static const sp_pair PU[] = {
        {AI_EASY, SP_RANDOM, 100, 75},
        {AI_MEDIUM, AI_EASY, 100, 65},
        {AI_HARD, AI_EASY, 60, 80},
        {AI_HARD, AI_MEDIUM, 60, 60},
        {AI_HARD, AI_HARD, 20, 0},
    };
    printf("-- Ultimate\n");
    ultimate = 1;
    bad |= sp_run(&U, PU, (int)(sizeof PU / sizeof PU[0]), argc, argv);
    return bad;
}
