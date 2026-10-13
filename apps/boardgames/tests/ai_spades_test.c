/*
 * Spades' bot (src/games/spades_bot.c, trickbot.h): the core's rules against the game's (the legal cards, the tricks'
 * winners, card by card over random deals), the determinised deals, the view keeps to what the seat may know (the
 * others' hidden cards swapped: the same move), sensible bids, the cancel, then self-play at every level by sides
 * (tests/ai_selfplay.h: seats 0 and 2 one level, 1 and 3 the other). Headless: this file plays the screen's part
 * (spades_live(), spades_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/spades.h"

static sgame T, U, S;
static int dealt;
const sgame *spades_live(void) { return &T; }

/* one action of the notation, as spades_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        spades_new(&T, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!spades_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int spades_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == SP_OVER ? spades_winner(&T.pos) : -1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const spos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    switch (p->phase) {
    case SP_BID: snprintf(m, (size_t)cap, "b%d", ai_rand_n(r, 6)); return 1;
    case SP_PLAY: {
        uint8_t L[SPADES_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (spades_can_play(p, h[i])) L[k++] = h[i];
        if (!k) return 0;
        char c[3];
        trick_name(L[ai_rand_n(r, k)], c);
        snprintf(m, (size_t)cap, "%s", c);
        return 1;
    }
    case SP_ROUND: snprintf(m, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static void walk(ai_rng *r, int plies)          /* random actions (not past the game's end) */
{
    for (int k = 0; k < plies && T.pos.phase != SP_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 12);
    tb_state v, w;
    {   /* the core's rules are the game's */
        int same = 1, plays = 0, tricks_ok = 1, fresh = 1;
        for (int gn = 0; gn < 200 && same && tricks_ok; gn++) {
            spades_new(&T, 500 + gn);
            dealt = 1;
            fresh = 1;
            while (T.pos.phase != SP_OVER && T.pos.round < 3 && same && tricks_ok) {
                const spos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == SP_PLAY) {
                    spb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += spades_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = spades_can_play(p, L[i]);
                    if (fresh) w = v, tt_reveal(&w, &p->hand[0][0], SPADES_HAND, p->n), fresh = 0;
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int before = p->tricks, was_play = p->phase == SP_PLAY;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->tricks != before)
                    for (int s = 0; s < SPADES_P; s++)
                        tricks_ok &= w.won[s] == p->won[s] && (p->phase != SP_PLAY || w.turn == p->turn);
                if (p->phase == SP_ROUND) tricks_ok &= tb_over(&w), fresh = 1;
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns)", plays);
        tt_check(name, same);
        tt_check("a revealed deal played by the core: the same tricks, turns and end", tricks_ok);
    }
    {   /* the determinisation */
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 60 && ok; gn++) {
            spades_new(&T, 900 + gn);
            dealt = 1;
            walk(&r, 4 + gn % 50);
            if (T.pos.phase != SP_PLAY) continue;
            spb_view(&T, T.pos.turn, &v, &S);
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
    const bot *b = bot_find("spades");
    tt_check("registered: bot_find(\"spades\")", b && b->think && b->play == spades_show && b->actor);
    {   /* honest: the others' hidden cards swapped (same suits): the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 40; gn++) {
            spades_new(&T, 3000 + gn);
            dealt = 1;
            walk(&r, 6 + gn);
            spos *p = &T.pos;
            if (p->phase != SP_PLAY) continue;
            int me = p->turn, a = (me + 1) % 4, c = (me + 2) % 4, swapped = 0;
            for (int i = 0; i < p->n[a] && !swapped; i++)
                for (int j = 0; j < p->n[c] && !swapped; j++)
                    if (CARD_SUIT(p->hand[a][i]) == CARD_SUIT(p->hand[c][j])) {
                        uint8_t t = p->hand[a][i];
                        p->hand[a][i] = p->hand[c][j];
                        p->hand[c][j] = t;
                        swapped = 1;
                    }
            if (!swapped) continue;
            int m1 = spb_choose(&T, me, AI_MEDIUM, 77, NULL);
            sgame keep = T;
            spades_new(&T, 3000 + gn);
            for (int i = 0; i < keep.n; i++) spades_play(&T, keep.act[i]);
            same &= m1 == spb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    {   /* the bids: 1..13 or nil; a side's bids near its tricks on the whole */
        int ok = 1, nils = 0, deals = 0;
        double bid_sum = 0;
        for (int gn = 0; gn < 200; gn++) {
            spades_new(&T, 6000 + gn);
            dealt = 1;
            for (int k = 0; k < 4; k++) {
                int a = spb_choose(&T, T.pos.turn, AI_HARD, 3, NULL);
                ok &= SA_KIND(a) == SA_BID && SA_ARG(a) <= 13;
                nils += SA_ARG(a) == 0;
                bid_sum += SA_ARG(a);
                spades_play(&T, a);
            }
            deals++;
        }
        snprintf(name, sizeof name, "bids: %.2f tricks bid a deal (of 13), %d nils in %d hands", bid_sum / deals, nils,
                 deals * 4);
        tt_check(name, ok && bid_sum / deals > 9 && bid_sum / deals < 13.5 && nils < deals * 4 / 5);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        spades_new(&T, 5);
        dealt = 1;
        while (T.pos.phase == SP_BID) one(".");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 4, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                spades_new(&T, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk(&r, 6 + k % 20 + retry);
                if (T.pos.phase != SP_PLAY) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                spb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "spades", .max_plies = 6000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 50},
        {AI_HARD, AI_EASY, 40, 55},
    };
    tt_targets(P, (int)(sizeof P / sizeof P[0]), argc, argv);
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
