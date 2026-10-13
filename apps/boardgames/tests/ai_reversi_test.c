/*
 * Reversi's bot (src/games/reversi_bot.c): its bitboard moves against the rules' own on random games, its registry
 * entry, the cancel, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (reversi_live(), reversi_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/reversi.h"

static rgame T;                                 /* the "screen's" game: the bot reads it */
const rgame *reversi_live(void) { return &T; }

/* one move as reversi.c's script() and reversi_show() take it: "d3", a legal square */
static int one(const char *s)
{
    if (strlen(s) != 2) return 0;
    int f = (s[0] | 32) - 'a', r = s[1] - '1';
    const rpos *p = rv_pos(&T);
    if (f < 0 || f > 7 || r < 0 || r > 7 || p->over || !(rv_legal(p, p->side) >> RV_SQ(f, r) & 1)) return 0;
    return rv_play(&T, RV_SQ(f, r));
}
int reversi_show(const char *move) { return one(move); }

static void reset(void) { rv_new(&T); }
static int actor(void) { return rv_pos(&T)->side; }
static int result(void)
{
    int s = rv_status(&T);
    return s == RV_PLAY ? -1 : s == RV_WIN0 ? 0 : s == RV_WIN1 ? 1 : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    const rpos *p = rv_pos(&T);
    uint64_t l = rv_legal(p, p->side);
    int n = __builtin_popcountll(l), k = n ? ai_rand_n(r, n) : -1;
    if (k < 0 || cap < 3) return 0;
    while (k--) l &= l - 1;
    int s = __builtin_ctzll(l);
    m[0] = (char)('a' + s % 8);
    m[1] = (char)('1' + s / 8);
    m[2] = 0;
    return 1;
}

/* the end by brute force: the final discs (mine - theirs) with the best play on both sides */
static int exact(const rpos *p)
{
    if (p->over) return __builtin_popcountll(p->disc[p->side]) - __builtin_popcountll(p->disc[!p->side]);
    int best = -100;
    for (uint64_t l = rvb_moves(p->disc[p->side], p->disc[!p->side]); l; l &= l - 1) {
        rpos q = *p;
        rvb_make(&q, __builtin_ctzll(l));
        int v = q.side == p->side ? exact(&q) : -exact(&q);
        if (v > best) best = v;
    }
    return best;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

int main(int argc, char **argv)
{
    char name[128];
    /* the bot's bitboards are the rules' moves exactly: 3000 random games, every position */
    ai_rng r;
    ai_seed(&r, 7);
    int same = 1, positions = 0;
    for (int gn = 0; gn < 3000 && same; gn++) {
        rpos p, q;
        rv_new(&T);
        p = q = *rv_pos(&T);
        while (!p.over && same) {
            uint64_t l = rv_legal(&p, p.side);
            same = l == rvb_moves(p.disc[p.side], p.disc[!p.side]);
            for (int s = 0; s < 64 && same; s++)
                same = rv_flips(&p, p.side, s) == ((l >> s & 1) ? rvb_flips(p.disc[p.side], p.disc[!p.side], s) : 0);
            int n = __builtin_popcountll(l), k = ai_rand_n(&r, n);
            while (k--) l &= l - 1;
            int s = __builtin_ctzll(l);
            rv_make(&p, s);
            rvb_make(&q, s);
            same = same && !memcmp(&p, &q, sizeof p);
            positions++;
        }
    }
    printf("     (%d positions)\n", positions);
    check("bitboard moves, flips and make = the rules' (passes and the end too)", same);

    const bot *b = bot_find("reversi");
    check("registered: bot_find(\"reversi\")", b && b->think && b->play == reversi_show);
    check("no bot for an unknown id", !bot_find("no-such-game"));

    /* every level answers the opening with one of its four moves; the move is notation */
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP] = "";
        rv_new(&T);
        int ok = b->think(0, l, m, sizeof m);
        snprintf(name, sizeof name, "%s opens with d3 c4 f5 or e6 (%s)", sp_name(l), m);
        check(name, ok && (!strcmp(m, "d3") || !strcmp(m, "c4") || !strcmp(m, "f5") || !strcmp(m, "e6")));
    }
    {   /* not its turn, or over: no move */
        char m[BOT_MOVE_CAP] = "";
        rv_new(&T);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    {   /* ebony c3 e4 d5, ivory b2 d4 e5: f6 turns the diagonal, ivory is left with b2 alone and no move, then a1
         * takes it: medium and hard see the wipe-out */
        rv_new(&T);
        T.pos[0].disc[0] |= 1ull << RV_SQ(2, 2);
        T.pos[0].disc[1] |= 1ull << RV_SQ(1, 1);
        int all = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            char m[BOT_MOVE_CAP] = "";
            all &= b->think(0, lv, m, sizeof m) && !strcmp(m, "f6");
        }
        check("a wipe-out in three: medium and hard see it", all);
    }
    {   /* the end searched exactly: 40 positions 8 empties from the end, hard keeps the true result (win, draw,
         * loss: a brute-force search says) */
        int good = 0, tried = 0;
        ai_seed(&r, 99);
        for (int k = 0; k < 400 && tried < 40; k++) {
            rv_new(&T);
            while (!rv_pos(&T)->over && __builtin_popcountll(rv_pos(&T)->disc[0] | rv_pos(&T)->disc[1]) < 56) {
                char m[3];
                random_move(&r, m, 3);
                one(m);
            }
            const rpos *p = rv_pos(&T);
            if (p->over) continue;
            tried++;
            int best = exact(p), me = p->side;
            char m[BOT_MOVE_CAP] = "";
            b->think(me, AI_HARD, m, sizeof m);
            rpos q = *p;
            rvb_make(&q, RV_SQ(m[0] - 'a', m[1] - '1'));
            int got = q.side == me ? exact(&q) : -exact(&q);
            good += (got > 0) == (best > 0) && (got < 0) == (best < 0);
        }
        snprintf(name, sizeof name, "hard plays the end perfectly (win, draw or loss kept): %d of %d", good, tried);
        check(name, tried == 40 && good == tried);
    }
    {   /* cancelled: hard gives up at once, and says so */
        char m[BOT_MOVE_CAP] = "";
        rv_new(&T);
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
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            for (int k = 0; k < npos; k++) {
                rv_new(&T);
                for (int i = 0, n = 8 + k; i < n && !rv_pos(&T)->over; i++) {
                    char m[3];
                    random_move(&r, m, 3);
                    one(m);
                }
                ai_stats s;
                rvb_choose(rv_pos(&T), lv, 5, &s);
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
        .id = "reversi", .max_plies = 64, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 75},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 85},
        {AI_HARD, AI_MEDIUM, 40, 60},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
