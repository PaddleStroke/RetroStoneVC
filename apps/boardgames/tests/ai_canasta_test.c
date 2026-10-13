/*
 * Canasta's bot (src/games/canasta_bot.c): its registry entry, a view that keeps to what the seat may see (the other
 * hands and the stock shuffled together: the same action), a turn that goes out when it can, then self-play at every
 * level, for two and for four in partnerships (tests/ai_selfplay.h: seats 0 and 2 play one level, 1 and 3 the
 * other). Headless: this file plays the screen's part (canasta_live(), canasta_show()). --full for the 200 games a
 * pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "cards.h"
#include "games/canasta.h"

int canb_choose(const cagame *g, int seat, int level, uint64_t seed, ca_act *out);
void canb_name(const ca_act *a, char *m);

static cagame T, U;
static int dealt, NP = 2;
const cagame *canasta_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        ca_new(&T, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!s[0] || strchr("a#*", s[0]) || !ca_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int canasta_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void)
{
    if (!dealt || T.pos.phase != CP_OVER) return -1;
    int w = ca_winner(&T.pos);
    return w < 0 ? 2 : w;
}

/* a random player: the stock or the plain take; then a plain meld now and then, else any legal discard */
static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const capos *p = &T.pos;
    ca_act a, L[CA_CARDS + 4];
    int k = 0;
    memset(&a, 0, sizeof a);
    if (p->phase == CP_HAND) a.kind = CA_NEXT, L[k++] = a;
    else if (p->phase == CP_DRAW) {
        a.kind = CA_STOCK;
        if (ca_legal(p, &a)) L[k++] = a;
        if (ca_auto(p, &a) && a.kind == CA_TAKE) L[k++] = a;
    } else if (p->phase == CP_MELD) {
        if (ai_rand_n(r, 3) == 0 && ca_auto(p, &a) && a.kind == CA_MELD_IT) L[k++] = a;
        else
            for (int i = 0; i < p->n[p->turn]; i++) {
                memset(&a, 0, sizeof a);
                a.kind = CA_DISCARD;
                a.n = 1;
                a.card[0] = p->hand[p->turn][i];
                if (ca_legal(p, &a)) L[k++] = a;
            }
    }
    if (!k && ca_auto(p, &a)) L[k++] = a;
    if (!k || cap < 64) return 0;
    canb_name(&L[ai_rand_n(r, k)], m);
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
    ai_seed(&r, 13);
    const bot *b = bot_find("canasta");
    check("registered: bot_find(\"canasta\")", b && b->think && b->actor && b->play == canasta_show);

    /* honest: the others' hands and the stock shuffled together (each keeps its count): the same action */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 50; gn++) {
            NP = gn % 2 ? 4 : 2;
            dealt = 0;
            char m[BOT_MOVE_CAP];
            snprintf(m, sizeof m, "D%d", 800 + gn);
            one(m);
            for (int k = 0; k < 3 + gn * 3 && T.pos.phase != CP_OVER; k++) {
                ca_act a;
                if (!canb_choose(&T, T.pos.turn, AI_HARD, 1, &a)) break;
                canb_name(&a, m);
                one(m);
            }
            const capos *p = &T.pos;
            if (p->phase > CP_MELD) continue;
            int me = p->turn;
            char m0[3][BOT_MOVE_CAP], m1[BOT_MOVE_CAP];
            for (int lv = 0; lv < 3; lv++) {
                ca_act a;
                canb_choose(&T, me, lv, 2, &a);
                canb_name(&a, m0[lv]);
            }
            cagame *save = malloc(sizeof *save);
            *save = T;
            capos *w = &T.pos;
            uint8_t pool[CA_CARDS];
            int n = 0;
            for (int q = 0; q < NP; q++)
                if (q != me)
                    for (int i = 0; i < w->n[q]; i++) pool[n++] = w->hand[q][i];
            for (int i = 0; i < w->nstock; i++) pool[n++] = w->stock[i];
            cards_shuffle(pool, n, 5u + (uint32_t)gn);
            int k = 0;
            for (int q = 0; q < NP; q++)
                if (q != me)
                    for (int i = 0; i < w->n[q]; i++) w->hand[q][i] = pool[k++];
            for (int i = 0; i < w->nstock; i++) w->stock[i] = pool[k++];
            for (int lv = 0; lv < 3; lv++) {
                ca_act a;
                canb_choose(&T, me, lv, 2, &a);
                canb_name(&a, m1);
                if (strcmp(m0[lv], m1)) printf("     game %d level %d: %s, then %s\n", gn, lv, m0[lv], m1), same = 0;
            }
            T = *save;
            free(save);
            tried++;
        }
        snprintf(what, sizeof what, "the hidden cards shuffled: the same action at every level (%d positions)", tried);
        check(what, same && tried > 30);
    }
    /* going out: a side with a canasta, a hand of three kings and a 7: melds the kings, then throws the 7 */
    {
        NP = 2;
        ca_new(&T, 2, 3);
        dealt = 1;
        capos *p = &T.pos;
        p->phase = CP_MELD;
        p->turn = 0;
        p->opened[0] = 1;
        p->mn[0][9] = 7;
        for (int i = 0; i < 7; i++) p->meld[0][9][i] = CARD(i % 4, 9);
        p->mn[0][10] = 7;
        for (int i = 0; i < 7; i++) p->meld[0][10][i] = CARD(i % 4, 10);
        memcpy(p->mn0, p->mn, sizeof p->mn);
        static const uint8_t h[4] = {CARD(0, 13), CARD(1, 13), CARD(2, 13), CARD(3, 7)};
        memcpy(p->hand[0], h, 4);
        p->n[0] = 4;
        ca_act a;
        int ok = canb_choose(&T, 0, AI_HARD, 1, &a) && a.kind == CA_MELD_IT && a.n == 3;
        char m[BOT_MOVE_CAP];
        if (ok) canb_name(&a, m), ok = one(m);
        ok = ok && canb_choose(&T, 0, AI_HARD, 1, &a) && a.kind == CA_DISCARD && a.card[0] == CARD(3, 7);
        if (ok) canb_name(&a, m), ok = one(m) && T.pos.out == 0;
        check("going out: the kings melded, the 7 thrown, out", ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "canasta", .max_plies = CA_MAX_ACTS - 8, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    /* (two players: hard plays medium about evenly, see canasta_bot.c) */
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 85}, {AI_MEDIUM, AI_EASY, 200, 55}, {AI_HARD, AI_MEDIUM, 200, 44},
        {AI_HARD, AI_EASY, 200, 58},
    };
    int bad = 0;
    for (NP = 2; NP <= 4; NP += 2) {
        printf("-- %d players\n", NP);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
