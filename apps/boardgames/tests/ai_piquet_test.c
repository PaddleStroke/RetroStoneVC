/*
 * Piquet's bot (src/games/piquet_bot.c, trickbot.h): the core's rules against the game's (the legal cards, the
 * tricks' winners, the play's points, the pique, the cards and capot, card by card over random deals), the determinised
 * deals (and how often they keep to the other's declarations), the view keeps to what the seat may know (the other's
 * hidden cards and the talon swapped: the same move), sensible exchanges, the cancel, the levels' work, then self-play
 * at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part (piquet_live(), piquet_show()).
 * --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/piquet.h"

static piquet_game T, U, S;                     /* the "screen's" game; a scratch copy; the view's replay */
static int dealt;
const piquet_game *piquet_live(void) { return &T; }

/* one action of the notation, as piquet_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        piquet_new(&T, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!piquet_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int piquet_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void)
{
    if (!dealt || T.pos.phase != PQ_OVER) return -1;
    int w = piquet_winner(&T.pos);
    return w < 0 ? 2 : w;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const piquet_pos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    char c[3];
    switch (p->phase) {
    case PQ_EXCHANGE: {
        int lo = piquet_throw_min(p), hi = piquet_throw_max(p), k = lo + ai_rand_n(r, hi - lo + 1), len = 1;
        uint8_t ix[PIQUET_HAND];
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
    case PQ_DECLARE: snprintf(m, (size_t)cap, "k"); return 1;
    case PQ_PLAY: {
        uint8_t L[PIQUET_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (piquet_can_play(p, h[i])) L[k++] = h[i];
        if (!k) return 0;
        trick_name(L[ai_rand_n(r, k)], c);
        snprintf(m, (size_t)cap, "%s", c);
        return 1;
    }
    case PQ_ROUND: snprintf(m, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static void walk(ai_rng *r, int plies)          /* random actions (not past the game's end) */
{
    for (int k = 0; k < plies && T.pos.phase != PQ_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

/* a plain exchange: the lowest cards, five for the elder, three for the younger (as many as allowed) */
static int plain_throw(const piquet_pos *p)
{
    int k = p->turn == p->elder ? 5 : 3, hi = piquet_throw_max(p), m = 0;
    if (k > hi) k = hi;
    for (int t = 0; t < k; t++) {
        int low = -1;
        for (int i = 0; i < p->n[p->turn]; i++)
            if (!(m >> i & 1) && (low < 0 || trick_rank(p->hand[p->turn][i]) < trick_rank(p->hand[p->turn][low])))
                low = i;
        m |= 1 << low;
    }
    return m;
}

/* the core's points for the deal, read back from a state's x (the struct's first fields: dp[2], bonus[2]) */
static int core_points(const tb_state *w, int s)
{
    const int16_t *x = (const int16_t *)(const void *)w->x;
    return x[s] + x[2 + s];
}

int main(int argc, char **argv)
{
    char name[200];
    ai_rng r;
    ai_seed(&r, 12);
    tb_state v, w;
    {   /* the core's rules are the game's */
        int same = 1, plays = 0, tricks_ok = 1, points_ok = 1, fresh = 1, deals = 0, piques = 0, capots = 0;
        for (int gn = 0; gn < 300 && same && tricks_ok; gn++) {
            piquet_new(&T, 500 + gn);
            dealt = 1;
            fresh = 1;
            while (T.pos.phase != PQ_OVER && T.pos.deal < 2 && same && tricks_ok) {
                const piquet_pos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == PQ_PLAY) {
                    pqb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += piquet_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = piquet_can_play(p, L[i]);
                    if (fresh) w = v, tt_reveal(&w, &p->hand[0][0], PIQUET_HAND, p->n), fresh = 0;
                    plays++;
                }
                random_move(&r, m, sizeof m);
                if (gn % 3 == 0 && p->phase == PQ_PLAY) {   /* a third of the deals: capot and pique more often */
                    int best = -1;
                    for (int i = 0; i < p->n[p->turn]; i++) {
                        int c = p->hand[p->turn][i];
                        if (piquet_can_play(p, c) && (best < 0 || trick_rank(c) > trick_rank(best))) best = c;
                    }
                    if (best >= 0 && p->turn == p->elder) trick_name(best, m);
                }
                int before = p->tricks, was_play = p->phase == PQ_PLAY;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->tricks != before)
                    for (int s = 0; s < PIQUET_P; s++)
                        tricks_ok &= w.won[s] == p->won[s] && (p->phase != PQ_PLAY || w.turn == p->turn);
                if (was_play && p->phase >= PQ_ROUND) {
                    tricks_ok &= tb_over(&w);
                    for (int s = 0; s < PIQUET_P; s++)
                        points_ok &= core_points(&w, s) == p->dpts[s] + p->ppts[s] + p->bonus[s];
                    deals++;
                    piques += p->pique != 255;
                    capots += p->capot != 255;
                    fresh = 1;
                }
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns)", plays);
        tt_check(name, same);
        tt_check("a revealed deal played by the core: the same tricks, turns and end", tricks_ok);
        snprintf(name, sizeof name, "the core's points = the game's: play, last trick, cards, capot, pique (%d deals,"
                 " %d piques, %d capots)", deals, piques, capots);
        tt_check(name, points_ok && deals > 300 && piques > 0 && capots > 0);
    }
    {   /* the determinisation */
        int ok = 1, tried = 0, match = 0;
        for (int gn = 0; gn < 80 && ok; gn++) {
            piquet_new(&T, 900 + gn);
            dealt = 1;
            walk(&r, 4 + gn % 26);
            const piquet_pos *p = &T.pos;
            if (p->phase != PQ_PLAY) continue;
            int me = p->turn, o = 1 - me, np = 0, start = 0;
            pqb_view(&T, me, &v, &S);
            uint8_t played[PIQUET_HAND];        /* the other's cards played this deal (the game replayed) */
            for (int i = 0; i < T.n; i++)
                if (PQA_KIND(T.act[i]) == PQA_NEXT) start = i + 1;
            piquet_new(&U, T.seed);
            for (int i = 0; i < T.n; i++) {
                if (i >= start && PQA_KIND(T.act[i]) == PQA_PLAY && U.pos.turn == o && np < PIQUET_HAND)
                    played[np++] = (uint8_t)PQA_ARG(T.act[i]);
                piquet_play(&U, T.act[i]);
            }
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                tried++;
                uint8_t H[PIQUET_HAND * 2];     /* the other's twelve at the declarations: dealt + played */
                int nh = w.n[o];
                memcpy(H, w.hand[o], (size_t)nh);
                for (int i = 0; i < np; i++) H[nh++] = played[i];
                piquet_decl d;
                piquet_declare(H, nh, &d);
                const piquet_decl *e = &p->decl[o];
                match += nh == PIQUET_HAND && d.pn == e->pn && d.pv == e->pv && d.sn == e->sn &&
                         (!d.sn || (d.stop == e->stop && d.ssuit == e->ssuit)) && d.setn == e->setn &&
                         (!d.setn || d.setrank == e->setrank);
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, voids, own hand (%d); the other's declarations"
                 " kept in %d%%", tried, tried ? 100 * match / tried : 0);
        tt_check(name, ok && tried > 100 && match * 2 > tried);
    }
    const bot *b = bot_find("piquet");
    tt_check("registered: bot_find(\"piquet\")", b && b->think && b->play == piquet_show && b->actor);
    {   /* honest: the other's cards and the talon's swapped: the same move */
        int same = 1, tried = 0, ex = 0;
        for (int gn = 0; gn < 60; gn++) {
            piquet_new(&T, 3000 + gn);
            dealt = 1;
            walk(&r, gn % 3 == 0 ? 0 : gn % 3 == 1 ? 1 : 6 + gn % 20);
            piquet_pos *p = &T.pos;
            if (p->phase != PQ_PLAY && p->phase != PQ_EXCHANGE) continue;
            int me = p->turn, o = 1 - me, top = p->top;
            if (top >= PIQUET_TALON) continue;
            int i = ai_rand_n(&r, p->n[o]), j = top + ai_rand_n(&r, PIQUET_TALON - top);
            uint8_t t = p->hand[o][i];
            p->hand[o][i] = p->talon[j];
            p->talon[j] = t;
            trick_sort(p->hand[o], p->n[o]);
            int m1 = pqb_choose(&T, me, AI_MEDIUM, 77, NULL);
            piquet_game keep = T;
            piquet_new(&T, 3000 + gn);
            for (int k = 0; k < keep.n; k++) piquet_play(&T, keep.act[k]);
            same &= m1 == pqb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
            ex += p->phase == PQ_EXCHANGE;
        }
        snprintf(name, sizeof name, "the other's hidden cards and the talon swapped: the same move (%d positions, %d"
                 " exchanges)", tried, ex);
        tt_check(name, same && tried - ex >= 10 && ex >= 5);
    }
    {   /* the exchanges: how many each throws; the declarations each role then scores, against a plain throw (the
         * lowest cards, as many as the role usually takes) by the same role on the same deal, the other plain */
        int ok = 1, deals = 0, thrown[2] = {0, 0};
        double net[2][2] = {{0, 0}, {0, 0}}, ms = 0, worst = 0;   /* [role][bot, plain] */
        for (int gn = 0; gn < 120; gn++, deals++)
            for (int role = 0; role < 2; role++)
                for (int who = 0; who < 2; who++) {   /* the role's exchange: the bot's, the plain one */
                    piquet_new(&T, 6000 + gn);
                    dealt = 1;
                    int e = T.pos.elder;
                    for (int k = 0; k < 2; k++) {
                        const piquet_pos *p = &T.pos;
                        int me = p->turn, mine = (me == e) == (role == 0), a;
                        if (mine && !who) {
                            double t0 = ai_now_ms();
                            a = pqb_choose(&T, me, AI_HARD, 3 + (uint64_t)gn, NULL);
                            double dt = ai_now_ms() - t0;
                            ms += dt;
                            if (dt > worst) worst = dt;
                            ok &= PQA_KIND(a) == PQA_EXCHANGE;
                        } else a = PQA_EXCHANGE | plain_throw(p);
                        ok &= piquet_play(&T, a);
                        if (mine && !who) thrown[role] += T.pos.ex[me];
                    }
                    while (T.pos.phase == PQ_DECLARE) piquet_play(&T, PQA_DECLARE);
                    int s = role == 0 ? e : 1 - e;
                    net[role][who] += T.pos.dpts[s] + T.pos.bonus[s] - T.pos.dpts[1 - s] - T.pos.bonus[1 - s];
                }
        double e = (double)thrown[0] / deals, y = (double)thrown[1] / deals;
        snprintf(name, sizeof name, "exchanges: the elder throws %.2f, the younger %.2f; declarations net: the elder"
                 " %+.1f (plain %+.1f), the younger %+.1f (plain %+.1f); %.1f ms (worst %.1f)", e, y,
                 net[0][0] / deals, net[0][1] / deals, net[1][0] / deals, net[1][1] / deals, ms / (2 * deals), worst);
        tt_check(name, ok && e >= 2.5 && e <= 5 && y >= 0.5 && y <= 3.5 && net[0][0] > net[0][1] &&
                 net[1][0] > net[1][1]);
    }
    {   /* cancelled: no move, at once; the declarations, the summary */
        char m[BOT_MOVE_CAP] = "";
        piquet_new(&T, 5);
        dealt = 1;
        while (T.pos.phase != PQ_PLAY) one(".");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think(1 - T.pos.turn, AI_HARD, m, sizeof m));
        piquet_new(&T, 6);
        one(".");
        one(".");
        ok = b->think(T.pos.turn, AI_EASY, m, sizeof m) && !strcmp(m, "k") && b->actor() == T.pos.turn;
        piquet_script(&T, "#");
        ok &= b->actor() == -1 && b->think(T.pos.turn, AI_EASY, m, sizeof m) && !strcmp(m, "n");
        tt_check("declaring: \"k\"; the summary: actor -1, \"n\"", ok);
    }
    {   /* the levels' work */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                piquet_new(&T, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk(&r, 4 + k % 12 + retry % 3);
                if (T.pos.phase != PQ_PLAY) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                pqb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "piquet", .max_plies = 600, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 85},
        {AI_MEDIUM, AI_EASY, 200, 52},
        {AI_HARD, AI_MEDIUM, 200, 45},
        {AI_HARD, AI_EASY, 200, 55},
    };
    return tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
