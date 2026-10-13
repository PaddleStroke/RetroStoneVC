/*
 * Rikiki's bot (src/games/rikiki_bot.c, trickbot.h): the core's rules against the game's (the legal cards, the tricks'
 * winners with and without trumps, card by card over random deals, 3 to 6 players), the determinised deals, the view
 * keeps to what the seat may know (the others' hidden cards swapped: the same move), the bids (legal, the dealer's
 * rule, near the tricks taken), the cancel, then self-play at every level (tests/ai_selfplay.h; 4 players, then 3, 5
 * and 6 briefly; the even seats one level, the odd the other). Headless: this file plays the screen's part
 * (rikiki_live(), rikiki_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/rikiki.h"

static rikiki_game T, U, S;
static int NP = 4, dealt;
const rikiki_game *rikiki_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        rikiki_new(&T, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!rikiki_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int rikiki_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

/* the winner's half of the table (even seats, odd seats), 2 a tie across the halves */
static int result(void)
{
    if (!dealt || T.pos.phase != RK_OVER) return -1;
    unsigned w = rikiki_winners(&T.pos);
    int half = 0;
    for (int q = 0; q < NP; q++)
        if (w >> q & 1) half |= 1 << (q & 1);
    return half == 3 ? 2 : half == 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const rikiki_pos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    switch (p->phase) {
    case RK_BID: {
        int b;
        do b = ai_rand_n(r, p->size + 1); while (!rikiki_can_bid(p, b));
        snprintf(m, (size_t)cap, "b%d", b);
        return 1;
    }
    case RK_PLAY: {
        uint8_t L[RIKIKI_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (rikiki_can_play(p, h[i])) L[k++] = h[i];
        if (!k) return 0;
        char c[3];
        trick_name(L[ai_rand_n(r, k)], c);
        snprintf(m, (size_t)cap, "%s", c);
        return 1;
    }
    case RK_ROUND: snprintf(m, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase != RK_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 13);
    tb_state v, w;
    {   /* the core's rules are the game's */
        int same = 1, plays = 0, tricks_ok = 1, fresh = 1;
        for (int gn = 0; gn < 40 && same && tricks_ok; gn++) {
            NP = 3 + gn % 4;
            rikiki_new(&T, NP, 500 + gn);
            dealt = 1;
            fresh = 1;
            while (T.pos.phase != RK_OVER && same && tricks_ok) {
                const rikiki_pos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == RK_PLAY) {
                    rkb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += rikiki_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = rikiki_can_play(p, L[i]);
                    if (fresh) w = v, tt_reveal(&w, &p->hand[0][0], RIKIKI_HAND, p->n), fresh = 0;
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int before = p->tricks, was_play = p->phase == RK_PLAY;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->tricks != before)
                    for (int s = 0; s < NP; s++)
                        tricks_ok &= w.won[s] == p->won[s] && (p->phase != RK_PLAY || w.turn == p->turn);
                if (p->phase == RK_ROUND) tricks_ok &= tb_over(&w), fresh = 1;
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns, 3 to 6 players)", plays);
        tt_check(name, same);
        tt_check("a revealed deal played by the core: the same tricks, turns and end", tricks_ok);
    }
    {   /* the determinisation */
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 80 && ok; gn++) {
            NP = 3 + gn % 4;
            rikiki_new(&T, NP, 900 + gn);
            dealt = 1;
            walk(&r, 30 + gn * 7);
            if (T.pos.phase != RK_PLAY) continue;
            rkb_view(&T, T.pos.turn, &v, &S);
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                tried++;
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, voids, own hand (%d)", tried);
        tt_check(name, ok && tried > 100);
    }
    const bot *b = bot_find("rikiki");
    tt_check("registered: bot_find(\"rikiki\")", b && b->think && b->play == rikiki_show && b->actor);
    {   /* honest: the others' hidden cards swapped (same suits): the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = 4;
            rikiki_new(&T, NP, 3000 + gn);
            dealt = 1;
            walk(&r, 40 + gn * 5);
            rikiki_pos *p = &T.pos;
            if (p->phase != RK_PLAY) continue;
            int me = p->turn, a = (me + 1) % NP, c = (me + 2) % NP, swapped = 0;
            for (int i = 0; i < p->n[a] && !swapped; i++)
                for (int j = 0; j < p->n[c] && !swapped; j++)
                    if (CARD_SUIT(p->hand[a][i]) == CARD_SUIT(p->hand[c][j]) && p->hand[a][i] != p->hand[c][j]) {
                        uint8_t t = p->hand[a][i];
                        p->hand[a][i] = p->hand[c][j];
                        p->hand[c][j] = t;
                        swapped = 1;
                    }
            if (!swapped) continue;
            int m1 = rkb_choose(&T, me, AI_MEDIUM, 77, NULL);
            rikiki_game keep = T;
            rikiki_new(&T, NP, 3000 + gn);
            for (int i = 0; i < keep.n; i++) rikiki_play(&T, keep.act[i]);
            same &= m1 == rkb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    {   /* the bids: always legal (the dealer's rule); on the whole near the tricks the bidder then takes (hard
         * against hard) */
        int ok = 1, deals = 0, exact = 0, off = 0;
        for (int gn = 0; gn < 40; gn++) {
            NP = 4;
            rikiki_new(&T, NP, 6000 + gn);
            dealt = 1;
            for (int d = 0; d < 6 && T.pos.phase != RK_OVER; d++) {
                while (T.pos.phase == RK_BID || T.pos.phase == RK_PLAY) {
                    int a = rkb_choose(&T, T.pos.turn, AI_MEDIUM, 3 + d, NULL);
                    ok &= a && rikiki_legal(&T.pos, a);
                    rikiki_play(&T, a);
                }
                for (int s = 0; s < NP; s++) {
                    exact += T.pos.bid[s] == T.pos.won[s];
                    off += abs(T.pos.bid[s] - T.pos.won[s]);
                }
                deals++;
                if (T.pos.phase == RK_ROUND) rikiki_play(&T, RKA_NEXT);
            }
        }
        snprintf(name, sizeof name, "bids legal; made exactly %d of %d, %.2f tricks off on average", exact, deals * NP,
                 (double)off / (deals * NP));
        tt_check(name, ok && exact * 2 > deals * NP);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        NP = 4;
        rikiki_new(&T, NP, 5);
        dealt = 1;
        walk(&r, 40);
        while (T.pos.phase != RK_PLAY) walk(&r, 1);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", (!ok || T.pos.n[T.pos.turn] == 1) && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % NP, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                NP = 4;
                rikiki_new(&T, NP, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk(&r, 200 + k * 13 + retry);
                if (T.pos.phase != RK_PLAY) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                rkb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "rikiki", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 50},
        {AI_HARD, AI_EASY, 40, 55},
    };
    tt_targets(P, (int)(sizeof P / sizeof P[0]), argc, argv);
    NP = 4;
    int bad = sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static const sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 20, 0}};
    for (NP = 3; NP <= 6; NP++) {
        if (NP == 4) continue;
        printf("-- %d players\n", NP);
        bad |= sp_run(&G, Q, 1, argc, argv);
    }
    return bad;
}
