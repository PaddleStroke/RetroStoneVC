/*
 * The climbing games' bot tests (tests/ai_bigtwo_test.c, ai_tienlen_test.c; games/shed_bot.h), shared: every play the
 * rules allow that the search lists is legal, the view keeps to what the seat may know (the others' cards shuffled
 * among them: the same move), the cancel, the levels' work, whole deals, then self-play by duplicate deals
 * (tests/ai_tricks.h): the first out's half of the table wins. Headless: the test plays the screen's part.
 * A test defines SHED_ID ("bigtwo"), SHED_RULES, SHED_LIVE, SHED_SHOW, SHED_CHOOSE, SHED_MINP, SHED_MAXP, SHED_USUAL
 * and includes this file.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef AI_SHED_H
#define AI_SHED_H

#include "ai_cardgames.h"

#include "games/cardbot.h"
#include "games/shed.h"
#include "games/shed_bot.h"

static shed_game T, U;
static int NP = SHED_USUAL, dealt;
const shed_game *SHED_LIVE(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        shed_new(&T, &SHED_RULES, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!shed_script(&U, &SHED_RULES, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int SHED_SHOW(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == SP_OVER ? T.pos.out[0] & 1 : -1; }

/* a random legal play: a pass, or a random set of 1..5 cards that the rules take (tried a few times), else the
 * lowest play */
static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const shed_pos *p = &T.pos;
    int mv[AI_MAX_MOVES];
    sb_st v;
    memset(&v, 0, sizeof v);
    v.p = *p;
    v.R = &SHED_RULES;
    int n = sb_a_moves(&v, mv);
    if (!n) return 0;
    uint64_t a = sb_real(&SHED_RULES, p, mv[ai_rand_n(r, n)]);
    return a && shed_legal(&SHED_RULES, p, a) && sb_write(&SHED_RULES, a, m, cap);
}

static int choose(int seat, int level, uint64_t seed, ai_stats *st) { return SHED_CHOOSE(&T, seat, level, seed, st) != 0; }

static int setup(int k, ai_rng *r)
{
    NP = SHED_MINP + k % (SHED_MAXP - SHED_MINP + 1);
    shed_new(&T, &SHED_RULES, NP, 40 + (uint32_t)k);
    dealt = 1;
    for (int i = 0, n = ai_rand_n(r, 25); i < n && T.pos.phase == SP_PLAY; i++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return T.pos.phase == SP_PLAY ? T.pos.turn : -1;
}

/* every listed play is legal; every legal play of up to 3 cards (and, leading, of 4) is listed or played the same
 * (a pair or three by its top card) */
static void check_moves(ai_rng *r)
{
    int legal = 1, cover = 1, positions = 0;
    for (int gn = 0; gn < 200; gn++) {
        NP = SHED_MINP + gn % (SHED_MAXP - SHED_MINP + 1);
        shed_new(&T, &SHED_RULES, NP, 100 + (uint32_t)gn);
        dealt = 1;
        for (int k = 0; k < 60 && T.pos.phase == SP_PLAY; k++) {
            const shed_pos *p = &T.pos;
            sb_st v;
            memset(&v, 0, sizeof v);
            v.p = *p;
            v.R = &SHED_RULES;
            int mv[AI_MAX_MOVES], n = sb_a_moves(&v, mv);
            positions++;
            uint64_t got[AI_MAX_MOVES];
            for (int i = 0; i < n; i++) {
                got[i] = sb_real(&SHED_RULES, p, mv[i]);
                legal &= got[i] && shed_legal(&SHED_RULES, p, got[i]);
            }
            for (int size = 1; size <= 2; size++) {   /* singles and pairs: a play of each top card is listed */
                uint64_t a = shed_search(&SHED_RULES, p, size);
                if (!a) continue;
                uint8_t c[52];
                int cn = shed_cards(&SHED_RULES, a, c), top = SHED_RULES.power(c[cn - 1]), found = 0;
                for (int i = 0; i < n; i++) {
                    uint8_t d[52];
                    int dn = got[i] && got[i] != SHED_PASS ? shed_cards(&SHED_RULES, got[i], d) : 0;
                    found |= dn == cn && SHED_RULES.power(d[dn - 1]) == top;
                }
                if (!found) {
                    cover = 0;
                    if (getenv("DEBUG")) printf("     not listed: a play of %d (top %d)\n", cn, top);
                }
            }
            char m[BOT_MOVE_CAP];
            random_move(r, m, sizeof m);
            if (!one(m)) { legal = 0; break; }
        }
    }
    char line[160];
    snprintf(line, sizeof line, "the search's plays are legal, the plain plays among them (%d positions)", positions);
    tt_check(line, legal && cover);
}

static int shed_main(int argc, char **argv)
{
    char line[200];
    int full = cg_full(argc, argv);
    ai_rng r;
    ai_seed(&r, 11);
    const bot *b = bot_find(SHED_ID);
    snprintf(line, sizeof line, "registered: bot_find(\"%s\")", SHED_ID);
    tt_check(line, b && b->think && b->play == SHED_SHOW && b->actor);
    if (!b) return 1;
    check_moves(&r);
    {   /* honest: the others' cards shuffled among them (and the cards never dealt): the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = SHED_MINP + gn % (SHED_MAXP - SHED_MINP + 1);
            shed_new(&T, &SHED_RULES, NP, 3000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 1 + gn % 25 && T.pos.phase == SP_PLAY; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            shed_pos *p = &T.pos;
            if (p->phase != SP_PLAY) continue;
            int me = p->turn;
            uint64_t a1 = SHED_CHOOSE(&T, me, AI_MEDIUM, 77, NULL);
            uint8_t pool[52];
            int n = 0;
            for (int q = 0; q < NP; q++)
                if (q != me) memcpy(pool + n, p->hand[q], p->n[q]), n += p->n[q];
            cdb_shuffle(pool, n, &r);
            n = 0;
            for (int q = 0; q < NP; q++)
                if (q != me) {
                    memcpy(p->hand[q], pool + n, p->n[q]), n += p->n[q];
                    shed_sort(&SHED_RULES, p->hand[q], p->n[q]);
                }
            p->seed ^= 0x5a5a5a5a;
            uint64_t a2 = SHED_CHOOSE(&T, me, AI_MEDIUM, 77, NULL);
            if (a2 != a1) printf("     game %d: %llx then %llx\n", gn, (unsigned long long)a1, (unsigned long long)a2);
            same &= a2 == a1;
            tried++;
        }
        snprintf(line, sizeof line, "the others' cards shuffled: the same move (%d positions)", tried);
        tt_check(line, same && tried >= 30);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        NP = SHED_USUAL;
        shed_new(&T, &SHED_RULES, NP, 5);
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
    cg_timing(SHED_ID, full ? 40 : 10, setup, choose);

    static const sp_game G = {
        .id = SHED_ID, .max_plies = 2000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {
        int ok = 1;
        for (NP = SHED_MINP; NP <= SHED_MAXP; NP++) ok &= cg_whole(&G, b, 70 + (uint32_t)NP, AI_EASY, SP_RANDOM, 2000);
        tt_check("whole deals end (every count of players, easy and random)", ok);
    }
    if (tt_fails) return 1;

    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 48},
        {AI_HARD, AI_EASY, 40, 55},
    };
    static sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 30, 80}};
    int bad = 0;
    cg_scale(P, 4), cg_scale(Q, 1);
    for (NP = SHED_MINP; NP <= SHED_MAXP; NP++) {
        snprintf(line, sizeof line, "%d players", NP);
        if (!cg_only(line)) continue;
        printf("-- %s\n", line);
        bad |= NP == SHED_USUAL ? tt_run(&G, P, 4, argc, argv) : tt_run(&G, Q, 1, argc, argv);
    }
    return bad;
}

#endif
