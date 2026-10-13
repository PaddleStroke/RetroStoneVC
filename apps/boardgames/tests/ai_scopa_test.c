/*
 * Scopa's bot (src/games/scopa_bot.c): the view keeps to what the seat may know (the others' cards and the stock
 * shuffled among themselves: the same move), a sweep taken when it is there, the cancel, the levels' work, whole games
 * to 11 (2 and 4 players), then self-play by duplicate deals (tests/ai_tricks.h): a "game" there is one round, won by
 * the side with more of its points. Headless: this file plays the screen's part (scopa_live(), scopa_show()). --full
 * for the long run and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"

#include "games/cardbot.h"
#include "games/scopa.h"

static scgame T, U;
static int NP = 2, dealt, ROUND = 1;
const scgame *scopa_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        scopa_new(&T, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!scopa_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int scopa_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

static int result(void)
{
    if (!dealt) return -1;
    const scpos *p = &T.pos;
    if (ROUND && p->phase != SC_PLAY) return p->pts[0] == p->pts[1] ? 2 : p->pts[1] > p->pts[0];
    return p->phase == SC_OVER ? scopa_winner(p) : -1;
}

static void name(const scpos *p, uint32_t a, char *m)
{
    if (a == SCA_NEXT) { strcpy(m, "n"); return; }
    it_name(SCA_CARD(a), m);
    int k = 2;
    if (SCA_TAKE(a)) {
        m[k++] = ':';
        for (int t = 0; t < p->ntable; t++)
            if (SCA_TAKE(a) >> t & 1) it_name(p->table[t], m + k), k += 2;
    }
    m[k] = 0;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    (void)cap;
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const scpos *p = &T.pos;
    if (p->phase == SC_ROUND) { strcpy(m, "n"); return 1; }
    uint32_t L[256];
    int k = 0;
    for (int i = 0; i < p->n[p->turn]; i++) {
        uint32_t mk[64];
        int c = p->hand[p->turn][i], n = scopa_captures(p, c, mk, 64);
        if (!n) L[k++] = SCA(c, 0);
        for (int j = 0; j < n && k < 256; j++) L[k++] = SCA(c, mk[j]);
    }
    if (!k) return 0;
    name(p, L[ai_rand_n(r, k)], m);
    return 1;
}

static int choose(int seat, int level, uint64_t seed, ai_stats *st) { return scob_choose(&T.pos, seat, level, seed, st) != 0; }

static int setup(int k, ai_rng *r)
{
    NP = k % 2 ? 4 : 2;
    scopa_new(&T, NP, 40 + (uint32_t)k);
    dealt = 1;
    for (int i = 0, n = ai_rand_n(r, 30); i < n && T.pos.phase == SC_PLAY; i++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return T.pos.phase == SC_PLAY ? T.pos.turn : -1;
}

int main(int argc, char **argv)
{
    char line[200];
    int full = cg_full(argc, argv);
    ai_rng r;
    ai_seed(&r, 11);
    const bot *b = bot_find("scopa");
    tt_check("registered: bot_find(\"scopa\")", b && b->think && b->play == scopa_show && b->actor);
    if (!b) return 1;

    {   /* honest: the others' cards and the stock shuffled among themselves: the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = gn % 2 ? 4 : 2;
            scopa_new(&T, NP, 3000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < gn % 25 && T.pos.phase == SC_PLAY; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            scpos *p = &T.pos;
            if (p->phase != SC_PLAY) continue;
            int me = p->turn;
            uint32_t a1 = scob_choose(p, me, AI_MEDIUM, 77, NULL);
            uint8_t pool[IT_CARDS];
            int n = 0;
            for (int q = 0; q < NP; q++)
                if (q != me) memcpy(pool + n, p->hand[q], p->n[q]), n += p->n[q];
            memcpy(pool + n, p->stock, p->nstock), n += p->nstock;
            cdb_shuffle(pool, n, &r);
            n = 0;
            for (int q = 0; q < NP; q++)
                if (q != me) memcpy(p->hand[q], pool + n, p->n[q]), n += p->n[q];
            memcpy(p->stock, pool + n, p->nstock);
            p->seed ^= 0x5a5a5a5a;
            uint32_t a2 = scob_choose(p, me, AI_MEDIUM, 77, NULL);
            if (a2 != a1) printf("     game %d: %x then %x\n", gn, a1, a2);
            same &= a2 == a1;
            tried++;
        }
        snprintf(line, sizeof line, "the hidden cards shuffled: the same move (%d positions)", tried);
        tt_check(line, same && tried >= 40);
    }
    {   /* a sweep (scopa) on offer is taken (medium and hard) */
        int found = 0, swept = 0;
        for (int gn = 0; gn < 2000 && found < 30; gn++) {
            NP = 2;
            scopa_new(&T, NP, 9000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 2 + gn % 20 && T.pos.phase == SC_PLAY; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            const scpos *p = &T.pos;
            if (p->phase != SC_PLAY || !p->nstock || p->ntable < 2) continue;
            int can = 0;
            for (int i = 0; i < p->n[p->turn]; i++) {
                uint32_t mk[64];
                int n = scopa_captures(p, p->hand[p->turn][i], mk, 64);
                for (int j = 0; j < n; j++) can |= mk[j] == (1u << p->ntable) - 1;
            }
            if (!can) continue;
            found++;
            uint32_t a = scob_choose(p, p->turn, AI_MEDIUM + (found & 1), 3, NULL);
            swept += SCA_TAKE(a) == (1u << p->ntable) - 1;
        }
        snprintf(line, sizeof line, "a sweep on offer is taken: %d of %d", swept, found);
        tt_check(line, found >= 10 && swept * 10 >= found * 9);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        NP = 4;
        scopa_new(&T, NP, 5);
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
    cg_timing("scopa", full ? 40 : 10, setup, choose);

    static const sp_game G = {
        .id = "scopa", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {
        int ok = 1;
        ROUND = 0;
        for (NP = 2; NP <= 4; NP += 2) ok &= cg_whole(&G, b, 70 + (uint32_t)NP, AI_EASY, SP_RANDOM, 4000);
        ROUND = 1;
        tt_check("whole games to 11 end (2 and 4 players, easy and random)", ok);
    }
    if (tt_fails) return 1;

    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 62},
        {AI_MEDIUM, AI_EASY, 60, 55},
        {AI_HARD, AI_MEDIUM, 60, 40},
        {AI_HARD, AI_EASY, 40, 45},
    };
    int bad = 0;
    cg_scale(P, 4);
    for (NP = 2; NP <= 4; NP += 2) {
        snprintf(line, sizeof line, "%d players", NP);
        if (!cg_only(line)) continue;
        printf("-- %s\n", line);
        bad |= tt_run(&G, P, 4, argc, argv);
    }
    return bad;
}
