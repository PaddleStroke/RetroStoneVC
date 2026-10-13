/*
 * Écarté's bot (src/games/ecarte_bot.c, trickbot.h): the core's rules against the game's (the legal cards: follow and
 * win if you can, else trump; the tricks' winners; the deal's points with the vole and the penalties, card by card over
 * random deals), the determinised deals (the king of trumps where its point says), the view keeps to what the seat may
 * know (the other's hidden cards and the stock swapped: the same move), sensible proposals, answers and discards, the
 * cancel, the levels' work, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the
 * screen's part (ecarte_live(), ecarte_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/ecarte.h"

static ecarte_game T, U, S;                     /* the "screen's" game; a scratch copy; the view's replay */
static int dealt;
const ecarte_game *ecarte_live(void) { return &T; }

/* one action of the notation, as ecarte_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        ecarte_new(&T, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!ecarte_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int ecarte_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == EC_OVER ? ecarte_winner(&T.pos) : -1; }

static int random_card(ai_rng *r, char *m, int cap)
{
    const ecarte_pos *p = &T.pos;
    uint8_t L[ECARTE_HAND];
    int k = 0;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (ecarte_can_play(p, p->hand[p->turn][i])) L[k++] = p->hand[p->turn][i];
    if (!k) return 0;
    char c[3];
    trick_name(L[ai_rand_n(r, k)], c);
    snprintf(m, (size_t)cap, "%s", c);
    return 1;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const ecarte_pos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    switch (p->phase) {
    case EC_PROPOSE:
        if (p->nstock > 0 && ai_rand_n(r, 2)) { snprintf(m, (size_t)cap, "q"); return 1; }
        return random_card(r, m, cap);
    case EC_ANSWER: snprintf(m, (size_t)cap, ai_rand_n(r, 2) ? "y" : "x"); return 1;
    case EC_DISCARD: {
        int lo = ecarte_throw_min(p), hi = ecarte_throw_max(p), k = lo + ai_rand_n(r, hi - lo + 1), len = 1;
        uint8_t ix[ECARTE_HAND];
        char c[3];
        for (int i = 0; i < n; i++) ix[i] = (uint8_t)i;
        for (int i = 0; i < k; i++) {
            int j = i + ai_rand_n(r, n - i);
            uint8_t t = ix[i]; ix[i] = ix[j]; ix[j] = t;
        }
        m[0] = 'e';
        for (int i = 0; i < k && len + 3 < cap; i++) trick_name(h[ix[i]], c), memcpy(m + len, c, 2), len += 2;
        m[len] = 0;
        return 1;
    }
    case EC_PLAY: return random_card(r, m, cap);
    case EC_ROUND: snprintf(m, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static void walk(ai_rng *r, int plies)          /* random actions (not past the game's end) */
{
    for (int k = 0; k < plies && T.pos.phase != EC_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

/* the seat to move may play a card (the elder may play at once instead of proposing) */
static int in_play(const ecarte_pos *p) { return p->phase == EC_PLAY || (p->phase == EC_PROPOSE && p->n[p->turn]); }

int main(int argc, char **argv)
{
    char name[220];
    ai_rng r;
    ai_seed(&r, 12);
    tb_state v, w;
    {   /* the core's rules are the game's */
        int same = 1, plays = 0, tricks_ok = 1, points_ok = 1, fresh = 1, deals = 0, voles = 0, twos = 0;
        for (int gn = 0; gn < 300 && same && tricks_ok; gn++) {
            ecarte_new(&T, 500 + gn);
            dealt = 1;
            fresh = 1;
            while (T.pos.phase != EC_OVER && T.pos.deal < 4 && same && tricks_ok) {
                const ecarte_pos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                int was_play = in_play(p) && m[0] != 'q';
                if (was_play) {
                    ecb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += ecarte_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = ecarte_can_play(p, L[i]);
                    if (fresh) {                /* every hand known; the reward table marked: r0[w][v] = 3w + v */
                        w = v, tt_reveal(&w, &p->hand[0][0], ECARTE_HAND, p->n), fresh = 0;
                        for (int q = 0; q < 6; q++) ((float *)(void *)w.x)[q] = (float)q;
                    }
                    plays++;
                }
                int before = p->tricks;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->phase == EC_OVER && p->tricks == 0) { fresh = 1; continue; }   /* a king's 5th */
                if (was_play && p->tricks != before)
                    for (int s = 0; s < ECARTE_P; s++)
                        tricks_ok &= w.won[s] == p->won[s] && (p->phase != EC_PLAY || w.turn == p->turn);
                if (was_play && p->phase >= EC_ROUND) {
                    float rw[2];
                    int win = p->pts[0] ? 0 : 1, pts = p->pts[win];
                    tricks_ok &= tb_over(&w);
                    w.R->reward(&w, rw);
                    points_ok &= pts >= 1 && pts <= 2 && rw[0] == (float)(3 * win + pts);
                    deals++;
                    voles += p->won[win] == ECARTE_HAND;
                    twos += pts == 2 && p->won[win] < ECARTE_HAND;
                    fresh = 1;
                }
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns)", plays);
        tt_check(name, same);
        tt_check("a revealed deal played by the core: the same tricks, turns and end", tricks_ok);
        snprintf(name, sizeof name, "the core's deal points = the game's (%d deals, %d voles, %d penalties of 2)",
                 deals, voles, twos);
        tt_check(name, points_ok && deals > 500 && voles > 0 && twos > 0);
    }
    {   /* the determinisation; the king of trumps where its point says */
        int ok = 1, tried = 0, king_ok = 1, kings = 0;
        for (int gn = 0; gn < 120 && ok; gn++) {
            ecarte_new(&T, 900 + gn);
            dealt = 1;
            walk(&r, 2 + gn % 14);
            const ecarte_pos *p = &T.pos;
            if (!in_play(p)) continue;
            int me = p->turn, o = 1 - me, K = CARD(p->trump, RANK_KING);
            ecb_view(&T, me, &v, &S);
            int known = p->phase == EC_PLAY && CARD_RANK(p->turned) != RANK_KING && p->king[o] &&
                        trick_find(p->hand[o], p->n[o], K) >= 0;
            kings += known;
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                int has = trick_find(w.hand[o], w.n[o], K) >= 0;
                if (known) king_ok &= has;
                if (p->phase == EC_PLAY && CARD_RANK(p->turned) != RANK_KING && !p->king[o]) king_ok &= !has;
                tried++;
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, voids, own hand (%d); the king where its point"
                 " says (%d views knew it)", tried, kings);
        tt_check(name, ok && king_ok && tried > 100 && kings > 0);
    }
    const bot *b = bot_find("ecarte");
    tt_check("registered: bot_find(\"ecarte\")", b && b->think && b->play == ecarte_show && b->actor);
    {   /* honest: the other's cards and the stock swapped: the same move */
        int same = 1, tried = 0, calls = 0;
        for (int gn = 0; gn < 80; gn++) {
            ecarte_new(&T, 3000 + gn);
            dealt = 1;
            walk(&r, gn % 12);
            ecarte_pos *p = &T.pos;
            if (p->phase >= EC_ROUND || !p->nstock) continue;
            int me = p->turn, o = 1 - me, K = CARD(p->trump, RANK_KING);
            int i = ai_rand_n(&r, p->n[o]), j = ai_rand_n(&r, p->nstock);
            if (p->hand[o][i] == K || p->stock[j] == K) continue;   /* the king's point is public */
            uint8_t t = p->hand[o][i];
            p->hand[o][i] = p->stock[j];
            p->stock[j] = t;
            ecarte_sort(p->hand[o], p->n[o]);
            int m1 = ecb_choose(&T, me, AI_MEDIUM, 77, NULL);
            ecarte_game keep = T;
            ecarte_new(&T, 3000 + gn);
            for (int k = 0; k < keep.n; k++) ecarte_play(&T, keep.act[k]);
            same &= m1 == ecb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
            calls += p->phase != EC_PLAY;
        }
        snprintf(name, sizeof name, "the other's hidden cards and the stock swapped: the same move (%d positions, %d"
                 " calls)", tried, calls);
        tt_check(name, same && tried >= 30 && calls >= 10);
    }
    {   /* the calls: how often the elder plays at once, the dealer refuses, how many each throws; who wins then */
        int ok = 1, deals = 0, at_once = 0, at_once_won = 0, asked = 0, refused = 0, refused_won = 0, ex[2] = {0, 0};
        int nex[2] = {0, 0};
        double ms = 0, worst = 0;
        long ncalls = 0;
        for (int gn = 0; gn < 300; gn++) {
            ecarte_new(&T, 6000 + gn);
            dealt = 1;
            const ecarte_pos *p = &T.pos;
            int first = 1, e = p->elder;
            while (p->phase < EC_PLAY && ok) {
                int me = p->turn;
                double t0 = ai_now_ms();
                int a = ecb_choose(&T, me, AI_HARD, 3 + (uint64_t)gn, NULL);
                double dt = ai_now_ms() - t0;
                ms += dt, ncalls++;
                if (dt > worst) worst = dt;
                if (first && p->phase == EC_PROPOSE) at_once += ECA_KIND(a) == ECA_PLAY, asked += ECA_KIND(a) == ECA_ASK;
                if (first && p->phase == EC_ANSWER) refused += ECA_KIND(a) == ECA_NO, first = 0;
                if (ECA_KIND(a) == ECA_DISCARD) {
                    int k = __builtin_popcount((unsigned)ECA_ARG(a));
                    ex[me == e ? 0 : 1] += k, nex[me == e ? 0 : 1]++;
                }
                ok &= ecarte_play(&T, a);
                if (ECA_KIND(a) == ECA_PLAY) first = 0;
            }
            if (p->phase == EC_OVER) continue;
            ecarte_script(&T, "#");             /* the rest played plainly: the hands' worth, roughly */
            if (T.pos.forced) at_once_won += T.pos.won[e] >= 3;
            if (T.pos.refused) refused_won += T.pos.won[1 - e] >= 3;
            deals++;
        }
        snprintf(name, sizeof name, "calls: the elder plays at once %d%% (won %d of %d), the dealer refuses %d%% of"
                 " proposals (won %d of %d); thrown: the elder %.2f, the dealer %.2f; %.1f ms (worst %.1f)",
                 100 * at_once / deals, at_once_won, at_once, asked ? 100 * refused / asked : 0, refused_won, refused,
                 nex[0] ? (double)ex[0] / nex[0] : 0, nex[1] ? (double)ex[1] / nex[1] : 0, ms / ncalls, worst);
        tt_check(name, ok && at_once * 20 > deals && at_once * 10 < deals * 8 && refused > 0 && refused < asked &&
                 ex[0] >= nex[0] && ex[0] <= 4 * nex[0] && at_once_won * 2 > at_once);
    }
    {   /* cancelled: no move, at once; the summary */
        char m[BOT_MOVE_CAP] = "";
        ecarte_new(&T, 5);
        dealt = 1;
        while (T.pos.phase != EC_PLAY) one(".");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think(1 - T.pos.turn, AI_HARD, m, sizeof m));
        ecarte_script(&T, "#");
        ok = T.pos.phase != EC_ROUND || (b->actor() == -1 && b->think(T.pos.turn, AI_EASY, m, sizeof m) &&
                                          !strcmp(m, "n"));
        tt_check("the summary: actor -1, \"n\"", ok && T.pos.phase == EC_ROUND);
    }
    {   /* the levels' work */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                ecarte_new(&T, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                one("q"), one("x");     /* refused: the play from its first card */
                walk(&r, k % 6);
                if (T.pos.phase != EC_PLAY) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                ecb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "ecarte", .max_plies = 1500, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    /* five cards a deal: the luck of the deal and a short search already near the best play; hard is little
     * better than medium (the full run: 49%), a little better than easy (53%) */
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 75},
        {AI_MEDIUM, AI_EASY, 200, 52},
        {AI_HARD, AI_MEDIUM, 200, 45},
        {AI_HARD, AI_EASY, 200, 50},
    };
    tt_targets(P, (int)(sizeof P / sizeof P[0]), argc, argv);
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
