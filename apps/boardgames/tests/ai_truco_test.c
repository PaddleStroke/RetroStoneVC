/*
 * Truco's bot (src/games/truco_bot.c): the view keeps to what the seat may know (the others' unplayed cards shuffled,
 * the flors and the declared envido kept: the same move), a truco met by the two best cards accepted and by low cards
 * refused, the cancel, the levels' work, whole games (2 and 4 players, to 15 and 30, with and without flor), then
 * self-play by duplicate deals (tests/ai_tricks.h): whole games to 15. Headless: this file plays the screen's part
 * (truco_live(), truco_show()). --full for the long run and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"

#include "games/cardbot.h"
#include "games/spanish.h"
#include "games/truco.h"

static tgame T, U;
static int NP = 2, VAR, dealt, ROUND = 1;
const tgame *truco_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        truco_new(&T, NP, VAR & 1 ? 30 : 15, VAR >= 2, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!truco_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int truco_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? truco_actor(&T.pos) : 0; }

static int result(void)
{
    if (!dealt) return -1;
    const tpos *p = &T.pos;
    if (p->phase == TP_OVER && !ROUND) return p->winner;
    if (!ROUND || p->phase == TP_PLAY) return -1;
    int g0 = p->gained[0][0] + p->gained[0][1] + p->gained[0][2], g1 = p->gained[1][0] + p->gained[1][1] + p->gained[1][2];
    return g0 == g1 ? 2 : g1 > g0;
}

static void name(int a, char *m, int cap)
{
    static const char ch[] = "??tefrqnmN";
    if (TA_KIND(a) == TA_PLAY) es_name(TA_ARG(a), m);
    else {
        int k = TA_KIND(a) >> 8;
        snprintf(m, (size_t)cap, "%c", k == 3 ? 'e' : k == 4 ? 'r' : k == 5 ? 'f' : ch[k]);
    }
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const tpos *p = &T.pos;
    int L[32], n = 0;
    if (p->phase == TP_HAND) { snprintf(m, (size_t)cap, "N"); return 1; }
    int pl = truco_actor(p);
    for (int i = 0; i < p->n[pl]; i++)
        if (truco_legal(p, TA_PLAY | p->hand[pl][i])) L[n++] = TA_PLAY | p->hand[pl][i];
    for (int k = 2; k <= 8; k++)
        if (k != 8 && truco_legal(p, k << 8)) L[n++] = k << 8;   /* (never the mazo: a random player would quit) */
    if (!n && truco_legal(p, TA_MAZO)) L[n++] = TA_MAZO;
    if (!n) return 0;
    name(L[ai_rand_n(r, n)], m, cap);
    return 1;
}

static int choose(int seat, int level, uint64_t seed, ai_stats *st) { return trcb_choose(&T, seat, level, seed, st) != 0; }

static int setup(int k, ai_rng *r)
{
    NP = k % 2 ? 4 : 2;
    VAR = k / 2 % 4;
    truco_new(&T, NP, VAR & 1 ? 30 : 15, VAR >= 2, 40 + (uint32_t)k);
    dealt = 1;
    for (int i = 0, n = ai_rand_n(r, 8); i < n && T.pos.phase == TP_PLAY; i++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return T.pos.phase == TP_PLAY ? truco_actor(&T.pos) : -1;
}

int main(int argc, char **argv)
{
    char line[200];
    int full = cg_full(argc, argv);
    ai_rng r;
    ai_seed(&r, 11);
    const bot *b = bot_find("truco");
    tt_check("registered: bot_find(\"truco\")", b && b->think && b->play == truco_show && b->actor);
    if (!b) return 1;

    {   /* honest: the others' unplayed cards shuffled (the flors and the declared envido as they were): same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 400 && tried < 40; gn++) {
            NP = gn % 2 ? 4 : 2;
            VAR = gn / 2 % 4;
            truco_new(&T, NP, VAR & 1 ? 30 : 15, VAR >= 2, 3000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < gn % 6 && T.pos.phase == TP_PLAY; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            tpos *p = &T.pos;
            if (p->phase != TP_PLAY) continue;
            int me = truco_actor(p), a1 = trcb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tpos keep = *p;
            int ok = 0;
            for (int tries = 0; tries < 300 && !ok; tries++) {
                uint8_t pool[48];
                int n = 0;
                *p = keep;
                for (int q = 0; q < NP; q++)
                    if (q != me) memcpy(pool + n, p->hand[q], p->n[q]), n += p->n[q];
                cdb_shuffle(pool, n, &r);
                n = 0;
                ok = 1;
                for (int q = 0; q < NP; q++) {
                    if (q == me) continue;
                    for (int i = 0; i < p->n[q]; i++) {   /* the dealt hand: the played cards and the new ones */
                        int c = p->hand[q][i];
                        for (int j = 0; j < 3; j++)
                            if (p->dealt[q][j] == c) p->dealt[q][j] = pool[n];
                        p->hand[q][i] = pool[n++];
                    }
                    p->env_value[q] = (uint8_t)truco_envido(p->dealt[q]);
                    if (p->flor) ok &= truco_has_flor(p->dealt[q]) == (p->flor_mask >> q & 1);
                    if (p->env_winner == q) ok &= p->env_value[q] == keep.env_value[q];
                }
            }
            if (!ok) { *p = keep; continue; }
            p->seed ^= 0x5a5a5a5a;
            int a2 = trcb_choose(&T, me, AI_MEDIUM, 77, NULL);
            if (a2 != a1) printf("     game %d: %x then %x\n", gn, a1, a2);
            same &= a2 == a1;
            tried++;
            *p = keep;
        }
        snprintf(line, sizeof line, "the others' cards shuffled: the same move (%d positions)", tried);
        tt_check(line, same && tried >= 30);
    }
    {   /* a truco called at the first card: the as of espadas and of bastos accept it; three low cards refuse */
        int top = 0, top_q = 0, low = 0, low_n = 0;
        for (int gn = 0; gn < 3000 && (top < 12 || low < 12); gn++) {
            NP = 2, VAR = 0;
            truco_new(&T, NP, 15, 0, 7000 + (uint32_t)gn);
            dealt = 1;
            if (!one("t")) continue;            /* the mano calls truco at once */
            const tpos *p = &T.pos;
            int me = truco_actor(p), aces = 0, maxp = 0;
            for (int i = 0; i < 3; i++) {
                int w = truco_power(p->hand[me][i]);
                aces += w >= 13;
                maxp = w > maxp ? w : maxp;
            }
            if (aces < 2 && maxp > 5) continue;
            if (truco_legal(p, TA_ENVIDO) && p->env_value[me] >= 26) continue;
            int a = trcb_choose(&T, me, AI_MEDIUM + (gn & 1), 3, NULL);
            if (aces == 2) top++, top_q += a == TA_QUIERO || a == TA_TRUCO;
            else low++, low_n += a == TA_NOQUIERO;
        }
        snprintf(line, sizeof line, "a truco: two top aces accept %d of %d, low cards refuse %d of %d", top_q, top, low_n,
                 low);
        tt_check(line, top >= 5 && low >= 5 && top_q == top && low_n * 10 >= low * 8);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        NP = 4, VAR = 0;
        truco_new(&T, NP, 15, 0, 5);
        dealt = 1;
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(truco_actor(&T.pos), AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to act: no move", !b->think((truco_actor(&T.pos) + 1) % NP, AI_HARD, m, sizeof m));
        tt_check("actor: the seat to act", b->actor() == truco_actor(&T.pos));
    }
    cg_timing("truco", full ? 60 : 20, setup, choose);

    static const sp_game G = {
        .id = "truco", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {
        int ok = 1;
        ROUND = 0;
        for (VAR = 0; VAR < 4; VAR++)
            for (NP = 2; NP <= 4; NP += 2) ok &= cg_whole(&G, b, 70 + (uint32_t)(VAR * 10 + NP), AI_EASY, SP_RANDOM, 4000);
        ROUND = 1;
        tt_check("whole games end (2 and 4 players, to 15 and 30, with and without flor)", ok);
    }
    if (tt_fails) return 1;

    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 62},
        {AI_MEDIUM, AI_EASY, 60, 50},
        {AI_HARD, AI_MEDIUM, 60, 45},
        {AI_HARD, AI_EASY, 40, 52},
    };
    int bad = 0;
    cg_scale(P, 4);
    ROUND = 0;                                  /* whole games: a hand alone weighs no points */
    for (VAR = 0; VAR < 4; VAR += 2)
        for (NP = 2; NP <= 4; NP += 2) {
            snprintf(line, sizeof line, "%d players, %s flor", NP, VAR >= 2 ? "with" : "without");
            if (!cg_only(line)) continue;
            printf("-- %s\n", line);
            bad |= tt_run(&G, P, 4, argc, argv);
        }
    return bad;
}
