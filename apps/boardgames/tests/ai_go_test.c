/*
 * Go's bot (src/games/go_bot.c): its registry entry, a few positions (a capture, an escape, the pass at the end, the
 * dead stones marked while counting, capture go's win), the cancel, the levels' work, then self-play at every level
 * on 9 x 9, a few games on 13 x 13 and 19 x 19, and capture go (tests/ai_selfplay.h). Headless: this file plays the
 * screen's part (go_live(), go_show()). --full for all the games.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/go.h"

static gogame T;                                /* the "screen's" game: the bot reads it */
static int size = 9, capture;
const gogame *go_live(void) { return &T; }
int go_show(const char *move)
{
    int n = T.nmoves, phase = T.phase;
    uint8_t dead[GO_PTS];
    memcpy(dead, T.dead, sizeof dead);
    if (!go_script(&T, move)) return 0;
    return T.nmoves != n || T.phase != phase || memcmp(dead, T.dead, sizeof dead);
}

static void reset(void)
{
    if (capture) go_new_capture(&T, size);
    else go_new(&T, size);
}
static int actor(void) { return go_now(&T)->side; }
static int result(void) { return T.phase == GO_OVER ? go_winner(&T) : -1; }

static const char COLS[] = "abcdefghjklmnopqrst";
static void name(int p, char *m, int cap) { snprintf(m, (size_t)cap, "%c%d", COLS[p % T.n], p / T.n + 1); }

/* a random player: any legal point that is not its own eye (all four neighbours its own), else a pass; while
 * counting, done */
static int random_move(ai_rng *r, char *m, int cap)
{
    if (T.phase == GO_COUNT) { snprintf(m, (size_t)cap, "pass"); return 1; }
    const gopos *p = go_now(&T);
    int n = T.n, legal[GO_PTS], k = 0, me = p->side + 1;
    for (int i = 0; i < n * n; i++) {
        if (p->pt[i] != GO_EMPTY) continue;
        int x = i % n, y = i / n, own = 1;
        int nb[4] = {x > 0 ? i - 1 : -1, x < n - 1 ? i + 1 : -1, y > 0 ? i - n : -1, y < n - 1 ? i + n : -1};
        for (int j = 0; j < 4; j++) own &= nb[j] < 0 || p->pt[nb[j]] == me;
        gopos out;
        if (!own && go_try(&T, i, &out) >= 0) legal[k++] = i;
    }
    if (!k) {
        if (T.capture) return 0;
        snprintf(m, (size_t)cap, "pass");
        return 1;
    }
    name(legal[ai_rand_n(r, k)], m, cap);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* from `lo` up, every level answers (the game after `moves`) with one of `want` (spaces around each) */
static int answers(int n, int cap_go, const char *moves, int lo, const char *want)
{
    const bot *b = bot_find("go");
    int all = 1;
    for (int l = lo; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP], pad[BOT_MOVE_CAP + 4];
        if (cap_go) go_new_capture(&T, n);
        else go_new(&T, n);
        if (!go_script(&T, moves)) { printf("     bad moves %s\n", moves); return 0; }
        int ok = b->think(go_now(&T)->side, l, m, sizeof m);
        snprintf(pad, sizeof pad, " %s ", m);
        if (!ok || !strstr(want, pad)) { printf("     %s: %s (want%s)\n", sp_name(l), ok ? m : "none", want); all = 0; }
    }
    return all;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("go");
    check("registered: bot_find(\"go\")", b && b->think && b->play == go_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], nm[160];
    /* ivory's e5 in atari (ebony d5 f5 e6; its last liberty e4): take it */
    check("medium and hard capture a stone in atari",
          answers(9, 0, "d5 e5 f5 a1 e6 a2", AI_MEDIUM, " e4 "));
    /* ebony's e5 in atari after ivory's d5 f5 e6 (ebony played elsewhere): it escapes on e4 */
    check("medium and hard save a stone in atari", answers(9, 0, "e5 d5 a1 f5 a2 e6", AI_MEDIUM, " e4 "));
    /* capture go: the capture wins at once: every level takes it */
    check("capture go: every level takes the winning capture", answers(9, 1, "d5 e5 f5 a1 e6 a2", AI_EASY, " e4 "));
    {   /* the end: a finished 9 x 9 (ebony's left five columns, ivory's right four, a wall between), ivory passed;
         * ebony (7.5 komi behind, 45 to 36: it wins) passes */
        go_new(&T, 9);
        char s[2048] = "";
        for (int y = 1; y <= 9; y++) {
            char mv[32];
            snprintf(mv, sizeof mv, "e%d f%d ", y, y);
            strcat(s, mv);
        }
        strcat(s, "a1 pass");                  /* ebony a1, ivory passes */
        int ok = go_script(&T, s);
        check("the end: ebony ahead, ivory passed: hard passes",
              ok && b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "pass"));
        /* counting: an ivory stone dead in ebony's area (b2), both pass: the bot marks it dead, then is done */
        go_new(&T, 9);
        s[0] = 0;
        for (int y = 1; y <= 9; y++) {
            char mv[32];
            snprintf(mv, sizeof mv, "e%d f%d ", y, y);
            strcat(s, mv);
        }
        strcat(s, "a1 b2 pass pass");          /* ivory b2 in ebony's area, then both pass */
        ok = go_script(&T, s) && T.phase == GO_COUNT;
        int side = go_now(&T)->side, marked = 0, done = 0;
        for (int k = 0; k < 6 && ok && !done; k++) {
            ok = b->think(side, AI_HARD, m, sizeof m) && go_show(m);
            if (!strcmp(m, "pass")) done = 1;
            else marked += !strcmp(m, "b2");
        }
        check("counting: the bot marks the dead stone, then is done", ok && done && marked == 1 && T.dead[1 + 9] &&
              T.phase == GO_OVER && go_winner(&T) == 0);
    }
    {   /* not its turn: no move; cancelled: no move, at once */
        go_new(&T, 19);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 100);
    }
    {   /* the levels' work (not checked: the calibration, alone on one core) */
        for (int n = 9; n <= 19; n += n == 9 ? 4 : 6)
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                ai_rng r;
                ai_seed(&r, 7);
                double ms = 0, nodes = 0, worst = 0;
                int npos = 4;
                for (int k = 0; k < npos; k++) {
                    go_new(&T, n);
                    for (int i = 0, z = 4 + n * k; i < z && T.phase == GO_PLAY; i++) {
                        random_move(&r, m, sizeof m);
                        go_show(m);
                    }
                    if (T.phase != GO_PLAY) go_new(&T, n);
                    ai_stats st;
                    gob_choose(&T, lv, 5, &st);
                    ms += st.ms;
                    nodes += st.nodes;
                    if (st.ms > worst) worst = st.ms;
                }
                snprintf(nm, sizeof nm, "     %2d x %-2d %-6s %8.0f playouts %6.1f ms (worst %6.1f)", n, n, sp_name(lv),
                         nodes / npos, ms / npos, worst);
                printf("%s\n", nm);
            }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "go", .max_plies = GO_HIST, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = go_show,
    };
    static const sp_pair P9[] = {
        {AI_EASY, SP_RANDOM, 100, 90},
        {AI_MEDIUM, AI_EASY, 100, 65},
        {AI_HARD, AI_EASY, 60, 80},
        {AI_HARD, AI_MEDIUM, 60, 60},
        {AI_HARD, AI_HARD, 20, 0},
    };
    static const sp_pair PBIG[] = {
        {AI_EASY, SP_RANDOM, 20, 80},
        {AI_HARD, AI_EASY, 8, 70},
        {AI_HARD, AI_MEDIUM, 8, 50},
    };
    static const sp_pair PCAP[] = {
        {AI_EASY, SP_RANDOM, 100, 85},
        {AI_MEDIUM, AI_EASY, 100, 60},
        {AI_HARD, AI_EASY, 60, 70},
        {AI_HARD, AI_MEDIUM, 60, 55},
        {AI_HARD, AI_HARD, 20, 0},
    };
    int bad = 0;
    capture = 0;
    size = 9;
    printf("-- 9 x 9\n");
    bad |= sp_run(&G, P9, (int)(sizeof P9 / sizeof P9[0]), argc, argv);
    for (size = 13; size <= 19; size += 6) {
        printf("-- %d x %d\n", size, size);
        bad |= sp_run(&G, PBIG, (int)(sizeof PBIG / sizeof PBIG[0]), argc, argv);
    }
    capture = 1;
    for (size = 9; size <= 13; size += 4) {
        printf("-- capture go %d x %d\n", size, size);
        bad |= sp_run(&G, PCAP, (int)(sizeof PCAP / sizeof PCAP[0]), argc, argv);
    }
    return bad;
}
