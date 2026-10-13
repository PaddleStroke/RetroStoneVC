/*
 * Briscola's bot (src/games/briscola_bot.c, trickbot.h): the core's rules against the game's (the tricks' winners and
 * points, the draw from the stock with the briscola last, card by card over random games, 2 and 4 players), the
 * determinised deals (the briscola at the bottom of the stock or in its drawer's hand), the view keeps to what the
 * seat may know (the others' hidden cards and the stock's order shuffled: the same move), the cancel, then self-play
 * at every level (tests/ai_selfplay.h; 2 players, then 4 by sides). Headless: this file plays the screen's part
 * (briscola_live(), briscola_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/briscola.h"

static brgame T, U, S;
static int NP = 2, dealt;
const brgame *briscola_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        briscola_new(&T, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!briscola_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int briscola_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == BR_OVER ? briscola_winner(&T.pos) : -1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const brpos *p = &T.pos;
    if (p->phase != BR_PLAY || cap < 3) return 0;
    it_name(p->hand[p->turn][ai_rand_n(r, p->n[p->turn])], m);
    return 1;
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase == BR_PLAY; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 15);
    tb_state v, w;
    {   /* the core's rules are the game's: a revealed game (the stock in its true order) played card by card */
        int same = 1, plays = 0, ok = 1;
        for (int gn = 0; gn < 400 && same && ok; gn++) {
            NP = gn & 1 ? 4 : 2;
            briscola_new(&T, NP, 500 + gn);
            dealt = 1;
            const brpos *p = &T.pos;
            brib_view(&T, p->turn, &v, &S);
            w = v;
            tt_reveal(&w, &p->hand[0][0], BRISCOLA_HAND, p->n);
            memcpy(w.stock, p->stock, p->nstock);
            w.nstock = w.nstock_known = p->nstock;
            while (p->phase == BR_PLAY && same && ok) {
                brib_view(&T, p->turn, &v, &S);
                uint8_t L[TB_HAND];
                same = tb_legal(&v, L) == p->n[p->turn];
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                const char *s = m;
                tb_play(&w, it_parse(&s));
                one(m);
                plays++;
                if (p->ntrick == 0) {
                    int side[2] = {0, 0};
                    for (int q = 0; q < NP; q++) side[q & 1] += w.pts[q];
                    ok &= side[0] == p->points[0] && side[1] == p->points[1] && w.turn == p->turn;
                    for (int q = 0; q < NP; q++) {
                        ok &= w.n[q] == p->n[q];
                        for (int i = 0; i < p->n[q]; i++) ok &= trick_find(w.hand[q], w.n[q], p->hand[q][i]) >= 0;
                    }
                }
            }
            ok &= tb_over(&w);
        }
        snprintf(name, sizeof name, "every card legal, as in the game (%d plays)", plays);
        tt_check(name, same);
        tt_check("a revealed game played by the core: the same tricks, points, draws and end", ok);
    }
    {   /* the determinisation: the briscola where it must be */
        int ok = 1, tried = 0, held = 0;
        for (int gn = 0; gn < 200 && ok; gn++) {
            NP = gn & 1 ? 4 : 2;
            briscola_new(&T, NP, 900 + gn);
            dealt = 1;
            walk(&r, gn % 40);
            const brpos *p = &T.pos;
            if (p->phase != BR_PLAY) continue;
            brib_view(&T, p->turn, &v, &S);
            int owner = -1;
            for (int q = 0; q < NP; q++)
                if (trick_find(p->hand[q], p->n[q], p->briscola) >= 0) owner = q;
            for (int d = 0; d < 10 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                if (p->nstock) ok &= w.stock[0] == p->briscola;
                else if (owner >= 0) ok &= trick_find(w.hand[owner], w.n[owner], p->briscola) >= 0, held++;
                tried++;
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts and the briscola's place (%d, %d in a hand)", tried,
                 held);
        tt_check(name, ok && tried > 500 && held > 50);
    }
    const bot *b = bot_find("briscola");
    tt_check("registered: bot_find(\"briscola\")", b && b->think && b->play == briscola_show);
    {   /* honest: another's hidden card swapped with one in the stock: the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = 2 + 2 * (gn & 1);
            briscola_new(&T, NP, 3000 + gn);
            dealt = 1;
            walk(&r, gn % 25);
            brpos *p = &T.pos;
            if (p->phase != BR_PLAY || p->nstock < 3) continue;
            int me = p->turn, o = (me + 1) % NP;
            uint8_t t = p->hand[o][0];
            p->hand[o][0] = p->stock[p->nstock - 1];
            p->stock[p->nstock - 1] = t;
            int m1 = brib_choose(&T, me, AI_MEDIUM, 77, NULL);
            brgame keep = T;
            briscola_new(&T, NP, 3000 + gn);
            for (int i = 0; i < keep.n; i++) briscola_play(&T, keep.act[i]);
            same &= m1 == brib_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
        }
        snprintf(name, sizeof name, "a hidden card swapped with the stock's: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        NP = 2;
        briscola_new(&T, NP, 5);
        dealt = 1;
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            for (int k = 0; k < npos; k++) {
                NP = 2;
                briscola_new(&T, NP, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k);
                walk(&r, k % 20);
                ai_stats s;
                double t0 = ai_now_ms();
                brib_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "briscola", .max_plies = 60, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 75},
        {AI_MEDIUM, AI_EASY, 200, 52},
        {AI_HARD, AI_MEDIUM, 200, 45},
        {AI_HARD, AI_EASY, 200, 55},
    };
    NP = 2;
    int bad = tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    printf("-- 4 players\n");
    NP = 4;
    bad |= tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    return bad;
}
