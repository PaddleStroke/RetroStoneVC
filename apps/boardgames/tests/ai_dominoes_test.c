/*
 * Dominoes' bot (src/games/dominoes_bot.c): its registry entry, a view that keeps to what the seat may know (the
 * other hands and the boneyard dealt again, keeping to what each was seen to lack: the same move), the bot's own
 * model of the line against the rules' (the open ends and All Fives' count, move by move), games of three and four
 * bots to the end, then self-play at every level for two, in each variant (tests/ai_selfplay.h). Headless: this file
 * plays the screen's part (dominoes_live(), dominoes_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "cards.h"
#include "games/dominoes.h"

int domb_choose(const dgame *g, int seat, int level, uint64_t seed, ai_stats *stats);
int domb_step_ok(const dgame *before, int act, const dgame *after);

static dgame T, U;
static int dealt, NP = 2, VAR = DOM_BLOCK;
const dgame *dominoes_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        dom_new(&T, VAR, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!s[0] || strchr("a#*", s[0]) || !dom_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int dominoes_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void)
{
    if (!dealt || T.pos.phase != DP_OVER) return -1;
    unsigned w = dom_winners(&T.pos);
    return w == 3 ? 2 : w == 1 ? 0 : 1;
}

static void name(int a, char *m, int cap)
{
    static const char *ends[] = {"w", "e", "n", "s"};
    int b = DA_BONE(a), e = DA_END(a);
    switch (DA_KIND(a)) {
    case DA_PLAY: snprintf(m, (size_t)cap, "p%d%d%s", dom_lo(b), dom_hi(b), e == END_C ? "" : ends[e]); break;
    case DA_DRAW: snprintf(m, (size_t)cap, "d"); break;
    case DA_PASS: snprintf(m, (size_t)cap, "x"); break;
    default: snprintf(m, (size_t)cap, "n");
    }
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const dpos *p = &T.pos;
    int L[200], k = 0;
    for (int i = 0; i < p->nhand[p->turn]; i++)
        for (int e = 0; e < 8; e++)
            if (dom_legal(p, DA(DA_PLAY, p->hand[p->turn][i], e))) L[k++] = DA(DA_PLAY, p->hand[p->turn][i], e);
    if (dom_legal(p, DA_DRAW)) L[k++] = DA_DRAW;
    if (dom_legal(p, DA_PASS)) L[k++] = DA_PASS;
    if (dom_legal(p, DA_NEXT)) L[k++] = DA_NEXT;
    if (!k) return 0;
    name(L[ai_rand_n(r, k)], m, cap);
    return 1;
}

static int fails;
static void check(const char *what, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

int main(int argc, char **argv)
{
    char what[160];
    ai_rng r;
    ai_seed(&r, 7);
    const bot *b = bot_find("dominoes");
    check("registered: bot_find(\"dominoes\")", b && b->think && b->actor && b->play == dominoes_show);

    /* honest: the others' bones and the boneyard pooled and dealt again (each keeps its count): the same move as
     * long as the deal keeps to what was seen (here: positions where nobody has passed or drawn yet) */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = 2 + gn % 3;
            VAR = gn % DOM_VARIANTS;
            dealt = 0;
            char m[BOT_MOVE_CAP];
            snprintf(m, sizeof m, "D%d", 300 + gn);
            one(m);
            int clean = 1;
            for (int k = 0; k < 1 + gn % 6 && T.pos.phase == DP_PLAY; k++) {
                random_move(&r, m, sizeof m);
                clean &= m[0] == 'p';
                one(m);
            }
            const dpos *p = &T.pos;
            if (p->phase != DP_PLAY || !clean) continue;
            int me = p->turn, acts[3];
            for (int lv = 0; lv < 3; lv++) acts[lv] = domb_choose(&T, me, lv, 5, NULL);
            dgame save = T;
            dpos *w = &T.pos;
            uint8_t pool[DOM_BONES];
            int n = 0;
            for (int q = 0; q < NP; q++)
                if (q != me)
                    for (int i = 0; i < w->nhand[q]; i++) pool[n++] = w->hand[q][i];
            for (int i = 0; i < w->nyard; i++) pool[n++] = w->yard[i];
            cards_shuffle(pool, n, 99u + (uint32_t)gn);
            int k = 0;
            for (int q = 0; q < NP; q++)
                if (q != me)
                    for (int i = 0; i < w->nhand[q]; i++) w->hand[q][i] = pool[k++];
            for (int i = 0; i < w->nyard; i++) w->yard[i] = pool[k++];
            for (int lv = 0; lv < 3; lv++) same &= domb_choose(&T, me, lv, 5, NULL) == acts[lv];
            T = save;
            tried++;
        }
        snprintf(what, sizeof what, "the hidden bones dealt again: the same move at every level (%d positions)", tried);
        check(what, same && tried > 25);
    }
    /* the bot's model of the line: the same open ends, count, points and turns as the rules, move by move */
    {
        int ok = 1, steps = 0;
        for (int gn = 0; gn < 120 && ok; gn++) {
            NP = 2 + gn % 3;
            VAR = gn % DOM_VARIANTS;
            dom_new(&T, VAR, NP, 7000 + (uint32_t)gn);
            dealt = 1;
            while (T.pos.phase != DP_OVER && T.n < DOM_MAX_ACTS - 4 && ok) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                dgame before = T;
                one(m);
                if (before.pos.phase == DP_PLAY) {
                    ok &= domb_step_ok(&before, T.act[T.n - 1], &T);
                    if (!ok) printf("     game %d variant %d: after %s\n", gn, VAR, m);
                    steps++;
                }
            }
        }
        snprintf(what, sizeof what, "the model's line, count and points = the rules' (%d moves, every variant)", steps);
        check(what, ok);
    }
    /* three and four bots, every variant, to the end */
    {
        int ok = 1, games = 0;
        for (int gn = 0; gn < 18 && ok; gn++) {
            NP = 3 + gn % 2;
            VAR = gn % DOM_VARIANTS;
            dom_new(&T, VAR, NP, 40 + (uint32_t)gn);
            dealt = 1;
            while (T.pos.phase != DP_OVER && T.n < DOM_MAX_ACTS - 4) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(ai_mix((uint64_t)gn, (uint64_t)T.n));
                if (!b->think(T.pos.turn, gn % 3, m, sizeof m) || !one(m)) {
                    printf("     game %d: \"%s\" refused\n", gn, m);
                    ok = 0;
                    break;
                }
            }
            ok &= T.pos.phase == DP_OVER;
            games++;
        }
        snprintf(what, sizeof what, "%d games of three and four bots, every variant, end", games);
        check(what, ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "dominoes", .max_plies = DOM_MAX_ACTS - 8, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 75}, {AI_MEDIUM, AI_EASY, 200, 55}, {AI_HARD, AI_MEDIUM, 200, 50},
        {AI_HARD, AI_EASY, 200, 58},
    };
    static const char *vname[] = {"block", "draw", "all fives"};
    int bad = 0;
    NP = 2;
    for (VAR = 0; VAR < DOM_VARIANTS; VAR++) {
        printf("-- %s\n", vname[VAR]);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
