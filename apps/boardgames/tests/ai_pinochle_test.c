/*
 * Pinochle's bot (src/games/pinochle_bot.c, trickbot.h): the core's rules against the game's (the legal cards while
 * the stock lasts and in the strict play; four players: a revealed deal played by the core, the same tricks), the
 * determinised deals, sensible bids, the cancel, then self-play at every level, two players and four
 * (tests/ai_selfplay.h). Headless: this file plays the screen's part (pinochle_live(), pinochle_show()). --full for
 * the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/pinochle.h"

static pino_game T, U, S;
static int dealt, players = 2;
const pino_game *pinochle_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        pinochle_new(&T, players, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!pinochle_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int pinochle_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == PP_OVER ? pinochle_winner(&T.pos) : -1; }

static int random_action(const pino_pos *p, ai_rng *r)
{
    int L[64], n = 0;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (pinochle_legal(p, PA_PLAY | p->hand[p->turn][i])) L[n++] = PA_PLAY | p->hand[p->turn][i];
    for (int k = 0; k < PM_KINDS; k++)
        for (int s = 0; s < 4; s++)
            if (pinochle_legal(p, PA_MELD_OF(k, s))) L[n++] = PA_MELD_OF(k, s);
    static const int other[] = {PA_SKIP, PA_GO, PA_NEXT, PA_TRUMP, PA_TRUMP | 1, PA_TRUMP | 2, PA_TRUMP | 3};
    for (int k = 0; k < 7; k++)
        if (pinochle_legal(p, other[k])) L[n++] = other[k];
    if (p->phase == PP_BID) {                   /* mostly passes; a bid a little over the last */
        if (pinochle_legal(p, PA_PASS) && ai_rand_n(r, 3)) return PA_PASS;
        return PA_BID | ((p->high < PN_MIN_BID ? PN_MIN_BID : p->high + 1) + ai_rand_n(r, 3));
    }
    return n ? L[ai_rand_n(r, n)] : 0;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    int a = random_action(&T.pos, r);
    return a && pinochle_name(&T.pos, a, m, cap);
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase != PP_OVER && T.pos.phase != PP_ROUND; k++) {
        char m[BOT_MOVE_CAP];
        if (random_move(r, m, sizeof m)) one(m);
    }
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 12);
    tb_state v, w;
    for (players = 2; players <= 4; players += 2) {
        int same = 1, plays = 0, tricks_ok = 1, revealed = 0;
        for (int gn = 0; gn < 80 && same && tricks_ok; gn++) {
            pinochle_new(&T, players, 500 + gn);
            dealt = 1;
            int fresh = 1;
            while (T.pos.phase != PP_ROUND && T.pos.phase != PP_OVER && same && tricks_ok) {
                const pino_pos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == PP_PLAY) {
                    pnb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) {
                        int c = p->hand[p->turn][i], dup = 0;
                        for (int j = 0; j < i; j++) dup |= p->hand[p->turn][j] == c;
                        want += !dup && pinochle_can_play(p, c);
                    }
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = pinochle_can_play(p, L[i]);
                    if (players == 4 && fresh) {
                        w = v;
                        tt_reveal(&w, &p->hand[0][0], PN_HAND, p->n);
                        fresh = 0;
                        revealed++;
                    }
                    plays++;
                }
                if (!random_move(&r, m, sizeof m)) { same = 0; break; }
                int before = p->tricks, was = p->phase;
                if (players == 4 && was == PP_PLAY) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (players == 4 && was == PP_PLAY && p->tricks != before)
                    for (int q = 0; q < 4; q++)
                        tricks_ok &= w.won[q] == p->won[q] && (p->phase != PP_PLAY || w.turn == p->turn);
                if (players == 4 && was == PP_PLAY && p->phase == PP_ROUND)
                    tricks_ok &= tb_over(&w) && w.pts[0] + w.pts[2] == p->counters[0] &&
                                 w.pts[1] + w.pts[3] == p->counters[1];
            }
        }
        snprintf(name, sizeof name, "%d players: the core's legal cards = the game's (%d turns)", players, plays);
        tt_check(name, same && plays > 1000);
        if (players == 4) tt_check("4 players: revealed deals played by the core: the same tricks, counters, end",
                                   tricks_ok && revealed > 50);
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 40 && ok; gn++) {
            pinochle_new(&T, players, 900 + gn);
            dealt = 1;
            walk(&r, 8 + gn % 40);
            if (T.pos.phase != PP_PLAY) continue;
            pnb_view(&T, T.pos.turn, &v, &S);
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                tried++;
            }
        }
        snprintf(name, sizeof name, "%d players: determinised deals keep counts, voids, own hand (%d)", players, tried);
        tt_check(name, ok && tried > 100);
    }
    const bot *b = bot_find("pinochle");
    tt_check("registered: bot_find(\"pinochle\")", b && b->think && b->play == pinochle_show && b->actor);
    {   /* the bids: the auction ends with a bid near the hands' worth */
        int ok = 1, deals = 0, sum = 0;
        for (int gn = 0; gn < 100; gn++) {
            pinochle_new(&T, 4, 6000 + gn);
            for (int k = 0; k < 40 && T.pos.phase == PP_BID; k++) {
                int a = pnb_choose(&T, T.pos.turn, AI_HARD, 3, NULL);
                ok &= pinochle_play(&T, a);
            }
            ok &= T.pos.phase == PP_TRUMP;
            deals++;
            sum += T.pos.high;
        }
        snprintf(name, sizeof name, "bids: the auction ends, %.1f bid a deal on average", (double)sum / deals);
        tt_check(name, ok && sum / deals >= 20 && sum / deals <= 45);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        players = 2;
        pinochle_new(&T, 2, 5);
        dealt = 1;
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 2, AI_HARD, m, sizeof m));
    }
    for (players = 2; players <= 4; players += 2)
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int npos = 5, done = 0;
            for (int k = 0; k < 40 && done < npos; k++) {
                pinochle_new(&T, players, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k);
                walk(&r, 6 + k * 3);
                if (T.pos.phase != PP_PLAY) continue;
                ai_stats s;
                double t0 = ai_now_ms();
                pnb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes, done++;
                if (dt > worst) worst = dt;
            }
            printf("     %d players %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", players, sp_name(lv),
                   nodes / (done ? done : 1), ms / (done ? done : 1), worst);
        }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "pinochle", .max_plies = 6000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P2[] = {
        {AI_EASY, SP_RANDOM, 60, 70},
        {AI_MEDIUM, AI_EASY, 60, 50},
        {AI_HARD, AI_MEDIUM, 60, 45},
    };
    players = 2;
    int fail = tt_run(&G, P2, 3, argc, argv);
    static sp_pair P4[] = {
        {AI_EASY, SP_RANDOM, 60, 70},
        {AI_HARD, AI_EASY, 60, 55},
    };
    players = 4;
    return fail | tt_run(&G, P4, 2, argc, argv);
}
