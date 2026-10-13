/*
 * Crazy Eights' bot (src/games/crazyeights_bot.c): the view keeps to what the seat may know (the others' cards and
 * the stock swapped among themselves: the same move), the determinised deals keep the counts and every card once, the
 * cancel, the levels' work, whole games to their end in both variants and every player count, then self-play by
 * duplicate deals (tests/ai_tricks.h): a "game" there is one round (the luck of a deal is evened by playing it twice,
 * the levels changing seats). Headless: this file plays the screen's part (crazyeights_live(), crazyeights_show()).
 * --full for the long run and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"

#include "games/cardbot.h"
#include "games/crazyeights.h"

static cgame T, U;
static int NP = 4, VAR = CE_EIGHTS, dealt, ROUND = 1;   /* ROUND: a game is the first round */
const cgame *crazyeights_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        ce_new(&T, VAR, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!ce_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int crazyeights_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

static int result(void)
{
    if (!dealt) return -1;
    const cpos *p = &T.pos;
    if (p->phase == CP_OVER || (ROUND && p->phase == CP_ROUND)) return p->winner & 1;
    return -1;
}

static void name(const cpos *p, int a, char *m, int cap)
{
    int i = 0;
    switch (CA_KIND(a)) {
    case CA_PLAY:
        while (i < p->n[p->turn] && p->hand[p->turn][i] != CA_ARG(a)) i++;
        snprintf(m, (size_t)cap, "p%d", i);
        break;
    case CA_CHOOSE: snprintf(m, (size_t)cap, "c%d", CA_ARG(a)); break;
    case CA_DRAW: snprintf(m, (size_t)cap, "d"); break;
    case CA_PASS: snprintf(m, (size_t)cap, "x"); break;
    case CA_CALL: snprintf(m, (size_t)cap, "l"); break;
    case CA_CATCH: snprintf(m, (size_t)cap, "k"); break;
    default: snprintf(m, (size_t)cap, "n"); break;
    }
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const cpos *p = &T.pos;
    int L[CE_HAND + 16], k = 0;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (ce_legal(p, CA_PLAY | p->hand[p->turn][i])) L[k++] = CA_PLAY | p->hand[p->turn][i];
    static const int other[] = {CA_CHOOSE, CA_CHOOSE | 1, CA_CHOOSE | 2, CA_CHOOSE | 3, CA_DRAW, CA_PASS, CA_CALL,
                                CA_CATCH, CA_NEXT};
    for (int i = 0; i < 9; i++)
        if (ce_legal(p, other[i])) L[k++] = other[i];
    if (!k) return 0;
    name(p, L[ai_rand_n(r, k)], m, cap);
    return 1;
}

static int choose(int seat, int level, uint64_t seed, ai_stats *st) { return ceb_choose(&T, seat, level, seed, st); }

static int setup(int k, ai_rng *r)
{
    NP = 2 + k % 5;
    VAR = k % 2;
    ce_new(&T, VAR, NP, 40 + (uint32_t)k);
    dealt = 1;
    for (int i = 0, n = ai_rand_n(r, 30); i < n && T.pos.phase == CP_PLAY; i++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return T.pos.phase == CP_PLAY || T.pos.phase == CP_CHOOSE ? T.pos.turn : -1;
}

int main(int argc, char **argv)
{
    char line[200];
    int full = cg_full(argc, argv);
    ai_rng r;
    ai_seed(&r, 11);
    const bot *b = bot_find("crazyeights");
    tt_check("registered: bot_find(\"crazyeights\")", b && b->think && b->play == crazyeights_show && b->actor);
    if (!b) return 1;

    /* honest: the others' cards and the stock shuffled among themselves (the counts kept): the same move */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = 2 + gn % 5;
            VAR = gn % 2;
            ce_new(&T, VAR, NP, 3000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 2 + gn % 12 && T.pos.phase == CP_PLAY; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            cpos *p = &T.pos;
            if (p->phase != CP_PLAY) continue;
            int me = p->turn, a1 = ceb_choose(&T, me, AI_MEDIUM, 77, NULL);
            uint8_t pool[CARDS_MAX * 2];
            int n = 0;
            for (int q = 0; q < NP; q++)
                if (q != me) memcpy(pool + n, p->hand[q], p->n[q]), n += p->n[q];
            memcpy(pool + n, p->stock, (size_t)p->nstock), n += p->nstock;
            cdb_shuffle(pool, n, &r);
            n = 0;
            for (int q = 0; q < NP; q++)
                if (q != me) memcpy(p->hand[q], pool + n, p->n[q]), n += p->n[q];
            memcpy(p->stock, pool + n, (size_t)p->nstock);
            p->seed ^= 0x5a5a5a5a;
            int a2 = ceb_choose(&T, me, AI_MEDIUM, 77, NULL);
            if (a2 != a1) printf("     game %d: %x then %x\n", gn, a1, a2);
            same &= a2 == a1;
            tried++;
        }
        snprintf(line, sizeof line, "the hidden cards shuffled, the seed changed: the same move (%d positions)", tried);
        tt_check(line, same && tried >= 20);
    }
    {   /* cancelled: no move, at once; not the seat to move: none */
        char m[BOT_MOVE_CAP] = "";
        NP = 4, VAR = CE_EIGHTS;
        ce_new(&T, VAR, NP, 5);
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
    cg_timing("crazyeights", full ? 40 : 10, setup, choose);

    static const sp_game G = {
        .id = "crazyeights", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {   /* whole games, both variants, every count of players */
        int ok = 1;
        ROUND = 0;
        for (VAR = 0; VAR < CE_VARIANTS; VAR++)
            for (NP = 2; NP <= 6; NP++) ok &= cg_whole(&G, b, 70 + (uint32_t)(VAR * 10 + NP), AI_EASY, SP_RANDOM, 6000);
        ROUND = 1;
        tt_check("whole games end (both variants, 2 to 6 players, easy and random)", ok);
    }
    if (tt_fails) return 1;

    /* the targets, by variant: crazy eights is mostly the deal's luck (even a random player wins often) */
    static sp_pair P[2][4] = {
        {{AI_EASY, SP_RANDOM, 60, 45}, {AI_MEDIUM, AI_EASY, 60, 50}, {AI_HARD, AI_MEDIUM, 60, 45}, {AI_HARD, AI_EASY, 40, 50}},
        {{AI_EASY, SP_RANDOM, 60, 85}, {AI_MEDIUM, AI_EASY, 60, 58}, {AI_HARD, AI_MEDIUM, 60, 45}, {AI_HARD, AI_EASY, 40, 58}},
    };
    static sp_pair Q[2][1] = {{{AI_MEDIUM, SP_RANDOM, 30, 45}}, {{AI_MEDIUM, SP_RANDOM, 30, 80}}};
    int bad = 0;
    for (int v = 0; v < 2; v++) cg_scale(P[v], 4), cg_scale(Q[v], 1);
    for (VAR = 0; VAR < CE_VARIANTS; VAR++)
        for (NP = 2; NP <= 6; NP++) {
            snprintf(line, sizeof line, "%s, %d players", VAR ? "last card" : "crazy eights", NP);
            if (!cg_only(line)) continue;
            printf("-- %s\n", line);
            bad |= NP == 2 ? tt_run(&G, P[VAR], 4, argc, argv) : tt_run(&G, Q[VAR], 1, argc, argv);
        }
    return bad;
}
