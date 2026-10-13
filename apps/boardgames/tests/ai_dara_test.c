/*
 * Dara's bot (src/games/dara_bot.c): its registry entry, a few positions (a row of three made, the capture), the
 * cancel, the levels' work, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the
 * screen's part (dara_live(), dara_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/dara.h"

static dargame T;                               /* the "screen's" game: the bot reads it */
const dargame *dara_live(void) { return &T; }
int dara_show(const char *move)
{
    int ply = T.ply;
    if (!dar_script(&T, move)) { T.ply = ply; return 0; }
    return T.ply == ply + 1;
}

static void reset(void) { dar_new(&T); }
static int actor(void) { return dar_pos(&T)->side; }
static int result(void)
{
    int s = dar_status(&T);
    return s == DAR_PLAY ? -1 : s == DAR_WIN0 ? 0 : s == DAR_WIN1 ? 1 : 2;
}

static void name(darmove m, char *s, int cap)
{
    if (m.kind == DAR_MOVE)
        snprintf(s, (size_t)cap, "%c%d%c%d", 'a' + m.from % DAR_W, 1 + m.from / DAR_W, 'a' + m.to % DAR_W,
                 1 + m.to / DAR_W);
    else snprintf(s, (size_t)cap, "%s%c%d", m.kind == DAR_TAKE ? "x" : "", 'a' + m.to % DAR_W, 1 + m.to / DAR_W);
}

static int random_move(ai_rng *r, char *m, int cap)
{
    darmove l[DAR_MAX_MOVES];
    int n = dar_legal(dar_pos(&T), l);
    if (!n) return 0;
    name(l[ai_rand_n(r, n)], m, cap);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* a moving position from a picture: rows from the top (y 5) down, 'o' ivory, 'x' ebony, '.' empty */
static void setup(const char *rows[DAR_H], int side)
{
    dar_new(&T);
    darpos *p = &T.pos[0];
    memset(p->c, 0, sizeof p->c);
    for (int y = 0; y < DAR_H; y++)
        for (int x = 0; x < DAR_W; x++) {
            char c = rows[DAR_H - 1 - y][x];
            p->c[DAR_AT(x, y)] = c == 'o' ? 1 : c == 'x' ? 2 : 0;
        }
    p->hand[0] = p->hand[1] = 0;
    p->phase = DAR_MOVE;
    p->side = (uint8_t)side;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("dara");
    check("registered: bot_find(\"dara\")", b && b->think && b->play == dara_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], nm[120];
    {   /* ivory steps c3 to c2 to make a row a2 b2 c2; then takes */
        static const char *pic[DAR_H] = {"x....x", "..x...", "..o.x.", "oo..x.", "x....."};
        int all = 1;
        for (int l = AI_MEDIUM; l <= AI_HARD; l++) {
            setup(pic, 0);
            all &= b->think(0, l, m, sizeof m) && !strcmp(m, "c3c2") && dara_show(m);
            all &= b->think(0, l, m, sizeof m) && m[0] == 'x' && dara_show(m);
        }
        check("medium and hard make a row of three and take", all);
    }
    {   /* not its turn: no move; cancelled: no move */
        dar_new(&T);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work on positions of random games (not checked: the calibration, alone on one core) */
        ai_rng r;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            int npos = 12;
            for (int k = 0; k < npos; k++) {
                dar_new(&T);
                for (int i = 0, n = 6 + 3 * k; i < n && result() < 0; i++) {
                    random_move(&r, m, sizeof m);
                    dara_show(m);
                }
                if (result() >= 0) dar_new(&T);
                ai_stats s;
                darmove x;
                drb_choose(dar_pos(&T), lv, 5, &x, &s);
                ms += s.ms;
                nodes += s.nodes;
                depth += s.depth;
                if (s.ms > worst) worst = s.ms;
            }
            snprintf(nm, sizeof nm, "     %-6s %8.0f nodes %6.1f ms (worst %6.1f) depth %4.1f", sp_name(lv),
                     nodes / npos, ms / npos, worst, depth / npos);
            printf("%s\n", nm);
        }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "dara", .max_plies = DAR_MAX_PLY - 8, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = dara_show,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 80},
        {AI_MEDIUM, AI_EASY, 100, 60},
        {AI_HARD, AI_EASY, 60, 75},
        {AI_HARD, AI_MEDIUM, 60, 55},
        {AI_HARD, AI_HARD, 20, 0},
    };
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
