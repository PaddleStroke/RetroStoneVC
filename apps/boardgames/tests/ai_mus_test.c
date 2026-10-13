/*
 * Mus' bot (src/games/mus_bot.c): the view keeps to what the seat may know (the other hands and the stock shuffled:
 * the same move), an ordago met by a sure hand accepted and by a hopeless one refused, the cancel, the levels' work,
 * whole games to 40, then self-play by duplicate deals (tests/ai_tricks.h), whole games again. Headless: this file
 * plays the screen's part (mus_live(), mus_show()). --full for the long run and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"

#include "games/cardbot.h"
#include "games/mus.h"
#include "games/spanish.h"

static mgame T, U;
static int dealt, ROUND = 1;
const mgame *mus_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        mus_new(&T, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!mus_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int mus_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

static int result(void)
{
    if (!dealt) return -1;
    const mpos *p = &T.pos;
    if (p->phase == MP_OVER) return p->winner;
    if (ROUND && p->phase == MP_HAND) return p->gained[0] == p->gained[1] ? 2 : p->gained[1] > p->gained[0];
    return -1;
}

static void name(const mpos *p, int a, char *m, int cap)
{
    switch (MA_KIND(a)) {
    case MA_MUS: snprintf(m, (size_t)cap, "m"); break;
    case MA_CUT: snprintf(m, (size_t)cap, "c"); break;
    case MA_DISCARD: {
        int k = 0;
        m[k++] = 'd';
        for (int i = 0; i < 4; i++)
            if (MA_ARG(a) >> i & 1) es_name(p->hand[p->turn][i], m + k), k += 2;
        m[k] = 0;
        break;
    }
    case MA_PASS: snprintf(m, (size_t)cap, "p"); break;
    case MA_BET: snprintf(m, (size_t)cap, "b%d", MA_ARG(a)); break;
    case MA_ORDAGO: snprintf(m, (size_t)cap, "o"); break;
    case MA_QUIERO: snprintf(m, (size_t)cap, "q"); break;
    case MA_NOQUIERO: snprintf(m, (size_t)cap, "n"); break;
    default: snprintf(m, (size_t)cap, "N"); break;
    }
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    int L[64], n = mus_actions(&T.pos, L);
    if (!n) return 0;
    name(&T.pos, L[ai_rand_n(r, n)], m, cap);
    return 1;
}

static int choose(int seat, int level, uint64_t seed, ai_stats *st)
{
    if (st) memset(st, 0, sizeof *st);
    return musb_choose(&T, seat, level, seed) != 0;
}

static int setup(int k, ai_rng *r)
{
    mus_new(&T, 40 + (uint32_t)k);
    dealt = 1;
    for (int i = 0, n = ai_rand_n(r, 12); i < n && T.pos.phase < MP_HAND; i++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return T.pos.phase < MP_HAND ? T.pos.turn : -1;
}

/* a position of the betting with a bet or an ordago facing the seat to move */
static int facing_ordago(uint32_t seed, ai_rng *r)
{
    mus_new(&T, seed);
    dealt = 1;
    for (int k = 0; k < 40 && T.pos.phase < MP_HAND; k++) {
        const mpos *p = &T.pos;
        if (p->phase == MP_MUS) { one("c"); continue; }
        if (p->phase == MP_BET) return one("o") && T.pos.phase == MP_BET;
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return 0;
}

int main(int argc, char **argv)
{
    char line[200];
    int full = cg_full(argc, argv);
    ai_rng r;
    ai_seed(&r, 11);
    const bot *b = bot_find("mus");
    tt_check("registered: bot_find(\"mus\")", b && b->think && b->play == mus_show && b->actor);
    if (!b) return 1;

    {   /* honest: the other hands and the stock shuffled (who has pares, juego kept: the table said it): same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 200 && tried < 40; gn++) {
            mus_new(&T, 3000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < gn % 10 && T.pos.phase < MP_HAND; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            mpos *p = &T.pos;
            if (p->phase >= MP_HAND) continue;
            int me = p->turn, a1 = musb_choose(&T, me, AI_MEDIUM, 77);
            mpos keep = *p;
            int ok = 0;
            for (int tries = 0; tries < 200 && !ok; tries++) {   /* a new deal of the others with the same speakers */
                uint8_t pool[48];
                int n = 0;
                for (int q = 0; q < MUS_P; q++)
                    if (q != me) memcpy(pool + n, keep.hand[q], 4), n += 4;
                memcpy(pool + n, keep.stock, keep.nstock), n += keep.nstock;
                cdb_shuffle(pool, n, &r);
                *p = keep;
                n = 0;
                for (int q = 0; q < MUS_P; q++)
                    if (q != me) memcpy(p->hand[q], pool + n, 4), mus_sort(p->hand[q]), n += 4;
                memcpy(p->stock, pool + n, keep.nstock);
                ok = 1;
                for (int q = 0; q < MUS_P; q++)
                    for (int l = ML_PARES; l <= ML_JUEGO; l++) ok &= mus_has(p, l, q) == mus_has(&keep, l, q);
                ok &= p->punto == keep.punto;
            }
            if (!ok) { *p = keep; continue; }
            p->seed ^= 0x5a5a5a5a;
            int a2 = musb_choose(&T, me, AI_MEDIUM, 77);
            if (a2 != a1) printf("     game %d: %x then %x\n", gn, a1, a2);
            same &= a2 == a1;
            tried++;
            *p = keep;
        }
        snprintf(line, sizeof line, "the hidden cards shuffled: the same move (%d positions)", tried);
        tt_check(line, same && tried >= 25);
    }
    {   /* an ordago: a sure lance accepts it, a hopeless one refuses (medium and hard) */
        int sure = 0, sure_q = 0, bad = 0, bad_n = 0;
        for (int gn = 0; gn < 600 && (sure < 15 || bad < 15); gn++) {
            if (!facing_ordago(7000 + (uint32_t)gn, &r)) continue;
            const mpos *p = &T.pos;
            int me = p->turn, l = p->lance;
            if (l != ML_GRANDE) continue;
            const uint8_t *h = p->hand[me];
            int top = mus_value(h[0]) == ES_REY && mus_value(h[1]) == ES_REY && mus_value(h[2]) == ES_REY;
            int low = mus_value(h[0]) <= 6;
            if (!top && !low) continue;
            int a = musb_choose(&T, me, AI_MEDIUM + (gn & 1), 3);
            if (top) sure++, sure_q += a == MA_QUIERO;
            if (low) bad++, bad_n += a == MA_NOQUIERO;
        }
        snprintf(line, sizeof line, "an ordago in grande: three reyes accept %d of %d, no figure refuses %d of %d", sure_q,
                 sure, bad_n, bad);
        tt_check(line, sure >= 5 && bad >= 5 && sure_q * 10 >= sure * 8 && bad_n * 10 >= bad * 9);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        mus_new(&T, 5);
        dealt = 1;
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % MUS_P, AI_HARD, m, sizeof m));
        tt_check("actor: the seat to move", b->actor() == T.pos.turn);
    }
    cg_timing("mus", full ? 60 : 20, setup, choose);

    static const sp_game G = {
        .id = "mus", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {
        int ok = 1;
        ROUND = 0;
        for (int k = 0; k < 3; k++) ok &= cg_whole(&G, b, 70 + (uint32_t)k, AI_EASY + k % 3, SP_RANDOM, 4000);
        for (int k = 0; k < 2; k++) ok &= cg_whole(&G, b, 80 + (uint32_t)k, AI_HARD, AI_EASY, 4000);
        ROUND = 1;
        tt_check("whole games to 40 end (bots against random and each other)", ok);
    }
    if (tt_fails) return 1;

    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 55},
        {AI_MEDIUM, AI_EASY, 60, 55},
        {AI_HARD, AI_MEDIUM, 60, 48},
        {AI_HARD, AI_EASY, 40, 50},
    };
    cg_scale(P, 4);
    ROUND = 0;                                  /* whole games: a hand alone weighs no points */
    return tt_run(&G, P, 4, argc, argv);
}
