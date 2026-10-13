/*
 * Crapette's bot (src/games/crapette_bot.c): its registry entry, a bot that never looks at the talons (both
 * shuffled: the same action), never breaks a priority itself and calls "Crapette!" on the other player's fault, then
 * self-play at every level (tests/ai_selfplay.h; the random player makes faults now and then: the bot calls them).
 * Headless: this file plays the screen's part (crapette_live(), crapette_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "cards.h"
#include "games/crapette.h"

int crapb_choose(const cgame *g, int seat, int level, uint64_t seed);
void crapb_name(int a, char *m, int cap);

static cgame T, U;
static int dealt;
const cgame *crapette_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        crap_new(&T, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!s[0] || strchr(".#", s[0]) || !crap_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int crapette_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void)
{
    if (!dealt) return 0;
    return T.pos.fault != CF_NONE ? 1 - T.pos.turn : T.pos.turn;
}
static int result(void)
{
    if (!dealt || T.pos.phase != CP_OVER) return -1;
    return T.pos.winner == CR_DRAW ? 2 : T.pos.winner;
}

/* a random player: any action but a call (the caller: the call) */
static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    if (T.pos.fault != CF_NONE) { snprintf(m, (size_t)cap, "x"); return 1; }
    int all[600], n = crap_moves(&T.pos, all), k = 0;
    for (int i = 0; i < n; i++)
        if (all[i] != CA_CALL) all[k++] = all[i];
    if (!k) return 0;
    crapb_name(all[ai_rand_n(r, k)], m, cap);
    return 1;
}

static int fails;
static void check(const char *what, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

static void shuffle_talons(cpos *p, uint32_t seed)
{
    for (int q = 0; q < 2; q++) cards_shuffle(p->talon[q], p->ntalon[q], seed + (uint32_t)q);
}

int main(int argc, char **argv)
{
    char what[160];
    ai_rng r;
    ai_seed(&r, 23);
    const bot *b = bot_find("crapette");
    check("registered: bot_find(\"crapette\")", b && b->think && b->actor && b->play == crapette_show);

    /* honest: both talons shuffled, the same action; never a fault; a fault of the other's called at once */
    {
        int same = 1, tried = 0, clean = 1, called = 1, faults = 0, games = 0;
        for (int gn = 0; gn < 24; gn++) {
            crap_new(&T, 50 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 3000 && T.pos.phase == CP_PLAY; k++) {
                const cpos *p = &T.pos;
                if (p->fault != CF_NONE) {
                    called &= crapb_choose(&T, 1 - p->turn, gn % 3, 1) == CA_CALL;
                    crap_play(&T, CA_CALL);
                    continue;
                }
                int lv = (gn + k) % 3, a = crapb_choose(&T, p->turn, lv, 7);
                if (k % 7 == 0) {
                    cgame *save = malloc(sizeof *save);
                    *save = T;
                    shuffle_talons(&T.pos, 900u + (uint32_t)k);
                    same &= crapb_choose(&T, T.pos.turn, lv, 7) == a;
                    T = *save;
                    free(save);
                    tried++;
                }
                clean &= crap_fault_of(p, a) == CF_NONE;
                if (k % 11 == 5) {              /* now and then a faulty move of a careless player */
                    int all[600], n = crap_moves(p, all);
                    for (int i = 0; i < n; i++)
                        if (crap_fault_of(p, all[i]) != CF_NONE && CA_KIND(all[i]) == CA_MOVE) {
                            a = all[i];
                            faults++;
                            break;
                        }
                }
                if (!crap_play(&T, a)) { clean = 0; break; }
            }
            games += T.pos.phase == CP_OVER;
        }
        snprintf(what, sizeof what, "the talons shuffled: the same action (%d positions)", tried);
        check(what, same && tried > 500);
        snprintf(what, sizeof what, "no fault of the bot; %d faults of the other called; %d of 24 games end",
                 faults, games);
        check(what, clean && called && faults > 20 && games >= 20);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "crapette", .max_plies = CR_MAX_ACTS - 8, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 85}, {AI_MEDIUM, AI_EASY, 200, 55}, {AI_HARD, AI_MEDIUM, 200, 50},
        {AI_HARD, AI_EASY, 200, 60},
    };
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
