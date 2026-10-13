/*
 * Président's bot (src/games/president_bot.c): the view keeps to what the seat may know (the others' cards shuffled
 * among themselves: the same move; the cards it gave in the exchange where they went), the cancel, the levels' work,
 * whole games of five deals to their end for 3 to 6 players, then self-play by duplicate deals (tests/ai_tricks.h): a
 * "game" there is the first deal, won by the half of the table whose places score more. Headless: this file plays
 * the screen's part (president_live(), president_show()). --full for the long run and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"

#include "games/cardbot.h"
#include "games/president.h"

static pgame T, U;
static int NP = 4, dealt, ROUND = 1;
const pgame *president_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        pres_new(&T, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!pres_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int president_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

static int result(void)
{
    if (!dealt) return -1;
    const ppos *p = &T.pos;
    if (ROUND && (p->phase == PP_DEAL || p->phase == PP_OVER)) {   /* the places' points of each half of the table */
        int pts[2] = {0, 0};
        for (int q = 0; q < NP; q++) pts[q & 1] += p->last[q];
        return pts[0] == pts[1] ? 2 : pts[1] > pts[0];
    }
    if (p->phase != PP_OVER) return -1;
    unsigned w = pres_winners(p), half = 0;
    for (int q = 0; q < NP; q++)
        if (w >> q & 1) half |= 1u << (q & 1);
    return half == 3 ? 2 : half == 2;
}

static char rank_char(int r) { return "?A23456789TJQK"[r]; }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const ppos *p = &T.pos;
    int pl = p->turn;
    switch (p->phase) {
    case PP_GIVE: {
        int c = p->hand[pl][ai_rand_n(r, p->n[pl])];
        snprintf(m, (size_t)cap, "g%c%c", rank_char(CARD_RANK(c)), "shdc"[CARD_SUIT(c)]);
        return 1;
    }
    case PP_PLAY: {
        int L[64], k = 0;
        for (int rk = 1; rk <= 13; rk++) {
            int cnt = 0;
            for (int i = 0; i < p->n[pl]; i++) cnt += CARD_RANK(p->hand[pl][i]) == rk;
            for (int c = 1; c <= cnt; c++) {
                int mask = 0, left = c;
                for (int s = 0; s < 4 && left; s++)
                    for (int i = 0; i < p->n[pl]; i++)
                        if (p->hand[pl][i] == CARD(s, rk) && left) mask |= 1 << s, left--;
                if (pres_legal(p, PA_PLAYOF(rk, mask))) L[k++] = rk * 8 + c;
            }
        }
        if (pres_legal(p, PA_PASS)) L[k++] = 0;
        if (!k) return 0;
        int x = L[ai_rand_n(r, k)];
        if (!x) { snprintf(m, (size_t)cap, "p"); return 1; }
        int i = 0;
        for (; i < x % 8; i++) m[i] = rank_char(x / 8);
        m[i] = 0;
        return 1;
    }
    case PP_DEAL: snprintf(m, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static int choose(int seat, int level, uint64_t seed, ai_stats *st) { return prb_choose_act(&T, seat, level, seed, st); }

static int setup(int k, ai_rng *r)
{
    NP = 3 + k % 4;
    pres_new(&T, NP, 40 + (uint32_t)k);
    dealt = 1;
    if (k % 3 == 0) {                           /* a second deal: the exchange */
        while (T.pos.phase == PP_PLAY) { char m[BOT_MOVE_CAP]; random_move(r, m, sizeof m); one(m); }
        one("N");
    }
    for (int i = 0, n = ai_rand_n(r, 30); i < n && (T.pos.phase == PP_PLAY || T.pos.phase == PP_GIVE); i++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return T.pos.phase == PP_PLAY ? T.pos.turn : -1;
}

int main(int argc, char **argv)
{
    char line[200];
    int full = cg_full(argc, argv);
    ai_rng r;
    ai_seed(&r, 11);
    const bot *b = bot_find("president");
    tt_check("registered: bot_find(\"president\")", b && b->think && b->play == president_show && b->actor);
    if (!b) return 1;

    /* honest: the cards the seat may not know shuffled among the other hands (the counts kept, the cards it gave
     * left where they went): the same move */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = 3 + gn % 4;
            pres_new(&T, NP, 3000 + (uint32_t)gn);
            dealt = 1;
            if (gn & 1) {
                while (T.pos.phase == PP_PLAY) { char m[BOT_MOVE_CAP]; random_move(&r, m, sizeof m); one(m); }
                one("N");
            }
            for (int k = 0; k < 2 + gn % 20 && T.pos.phase != PP_DEAL; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            ppos *p = &T.pos;
            if (p->phase != PP_PLAY) continue;
            int me = p->turn, a1 = prb_choose_act(&T, me, AI_MEDIUM, 77, NULL);
            uint8_t pool[64];
            int n = 0;
            for (int q = 0; q < NP; q++) {      /* a card it may know of (one it gave) stays */
                if (q == me) continue;
                for (int i = 0; i < p->n[q]; i++) {
                    int known = 0;
                    for (int x = 0; x < NP; x++)
                        for (int j = 0; j < p->ngot[x]; j++) known |= p->got[x][j] == p->hand[q][i];
                    if (!known) pool[n++] = p->hand[q][i];
                }
            }
            cdb_shuffle(pool, n, &r);
            for (int q = 0, k = 0; q < NP; q++) {
                if (q == me) continue;
                for (int i = 0; i < p->n[q]; i++) {
                    int known = 0;
                    for (int x = 0; x < NP; x++)
                        for (int j = 0; j < p->ngot[x]; j++) known |= p->got[x][j] == p->hand[q][i];
                    if (!known) p->hand[q][i] = pool[k++];
                }
            }
            p->seed ^= 0x5a5a5a5a;
            int a2 = prb_choose_act(&T, me, AI_MEDIUM, 77, NULL);
            if (a2 != a1) printf("     game %d: %x then %x\n", gn, a1, a2);
            same &= a2 == a1;
            tried++;
        }
        snprintf(line, sizeof line, "the hidden cards shuffled, the seed changed: the same move (%d positions)", tried);
        tt_check(line, same && tried >= 20);
    }
    {   /* the Président gives back low cards */
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 30; gn++) {
            NP = 4;
            pres_new(&T, NP, 500 + (uint32_t)gn);
            dealt = 1;
            while (T.pos.phase == PP_PLAY) { char m[BOT_MOVE_CAP]; random_move(&r, m, sizeof m); one(m); }
            one("N");
            if (T.pos.phase != PP_GIVE) continue;
            int a = prb_choose_act(&T, T.pos.turn, AI_HARD, 1, NULL);
            ok &= PA_KIND(a) == PA_GIVE && pres_strength(CARD_RANK(a & 0xff)) <= 8;
            tried++;
        }
        snprintf(line, sizeof line, "the cards given back are low ones (%d exchanges)", tried);
        tt_check(line, ok && tried >= 20);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        NP = 4;
        pres_new(&T, NP, 5);
        dealt = 1;
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % NP, AI_HARD, m, sizeof m));
        tt_check("actor: the seat to move", b->actor() == T.pos.turn);
    }
    cg_timing("president", full ? 40 : 10, setup, choose);

    static const sp_game G = {
        .id = "president", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {
        int ok = 1;
        ROUND = 0;
        for (NP = 3; NP <= 6; NP++) ok &= cg_whole(&G, b, 70 + (uint32_t)NP, AI_EASY, SP_RANDOM, 4000);
        ROUND = 1;
        tt_check("whole games of five deals end (3 to 6 players, easy and random)", ok);
    }
    if (tt_fails) return 1;

    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 85},
        {AI_MEDIUM, AI_EASY, 60, 50},
        {AI_HARD, AI_MEDIUM, 60, 42},
        {AI_HARD, AI_EASY, 40, 52},
    };
    static sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 30, 85}};
    int bad = 0;
    NP = 4;
    printf("-- %d players\n", NP);
    bad |= tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    for (NP = 3; NP <= 6; NP++) {
        if (NP == 4) continue;
        printf("-- %d players\n", NP);
        bad |= tt_run(&G, Q, 1, argc, argv);
    }
    return bad;
}
