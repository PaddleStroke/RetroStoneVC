/*
 * Durak's bot (src/games/durak_bot.c): the view keeps to what the seat may know (the unknown cards shuffled among the
 * others' hands and the talon: the same move; open hands: only the talon), a card picked up stays known, the cancel,
 * the levels' work, whole games (podkidnoy and perevodnoy, hidden and open hands, 2 to 6 players), then self-play by
 * duplicate deals (tests/ai_tricks.h): the durak's half of the table loses. Headless: this file plays the screen's
 * part (durak_live(), durak_show(), durak_open_hands()). --full for the long run and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"

#include "games/cardbot.h"
#include "games/durak.h"

static dgame T, U;
static int NP = 2, VAR = DUR_CLASSIC, OPEN, dealt;
const dgame *durak_live(void) { return &T; }
int durak_open_hands(void) { return OPEN; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        dur_new(&T, VAR, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!dur_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int durak_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

static int result(void)
{
    if (!dealt || T.pos.phase != DP_OVER) return -1;
    return T.pos.fool == DUR_NONE ? 2 : !(T.pos.fool & 1);
}

static void name(const dpos *p, uint64_t a, char *m, int cap)
{
    char c[3], d[3];
    switch (DA_KIND(a)) {
    case DA_PLAY: {
        int k = 0;
        for (int i = 0; i < DUR_CARDS; i++)
            if (DA_MASK(a) >> i & 1) trick_name(dur_card(i), m + k), k += 2;
        m[k] = 0;
        break;
    }
    case DA_BEAT: trick_name(DA_CARD(a), c), trick_name(p->att[DA_SLOT(a)], d), snprintf(m, (size_t)cap, "%s/%s", c, d); break;
    case DA_SHIFT: trick_name(DA_CARD(a), c), snprintf(m, (size_t)cap, "x%s", c); break;
    case DA_TAKE: snprintf(m, (size_t)cap, "t"); break;
    default: snprintf(m, (size_t)cap, "p"); break;
    }
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const dpos *p = &T.pos;
    uint64_t L[256];
    int k = 0, pl = p->turn;
    for (int i = 0; i < p->n[pl]; i++) {        /* single cards, and every card of a rank together */
        int c = p->hand[pl][i];
        uint64_t one_card = DA_PLAY | 1ull << dur_index(c), all = DA_PLAY;
        for (int j = 0; j < p->n[pl]; j++)
            if (CARD_RANK(p->hand[pl][j]) == CARD_RANK(c)) all |= 1ull << dur_index(p->hand[pl][j]);
        if (dur_legal(p, one_card)) L[k++] = one_card;
        if (all != one_card && dur_legal(p, all)) L[k++] = all;
        for (int s = 0; s < p->natt; s++)
            if (dur_legal(p, DA_BEATS(c, s))) L[k++] = DA_BEATS(c, s);
        if (dur_legal(p, DA_SHIFT | (uint64_t)c)) L[k++] = DA_SHIFT | (uint64_t)c;
    }
    if (dur_legal(p, DA_TAKE)) L[k++] = DA_TAKE;
    if (dur_legal(p, DA_PASS)) L[k++] = DA_PASS;
    if (!k) return 0;
    name(p, L[ai_rand_n(r, k)], m, cap);
    return 1;
}

static int choose(int seat, int level, uint64_t seed, ai_stats *st)
{
    return durb_choose(&T, seat, OPEN, level, seed, st) != 0;
}

static int setup(int k, ai_rng *r)
{
    NP = 2 + k % 5;
    VAR = k % 2;
    OPEN = k / 2 % 2;
    dur_new(&T, VAR, NP, 40 + (uint32_t)k);
    dealt = 1;
    for (int i = 0, n = ai_rand_n(r, 40); i < n && T.pos.phase != DP_OVER; i++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return T.pos.phase != DP_OVER ? T.pos.turn : -1;
}

int main(int argc, char **argv)
{
    char line[200];
    int full = cg_full(argc, argv);
    ai_rng r;
    ai_seed(&r, 11);
    const bot *b = bot_find("durak");
    tt_check("registered: bot_find(\"durak\")", b && b->think && b->play == durak_show && b->actor);
    if (!b) return 1;

    /* honest: the cards the seat cannot know shuffled (the others' hands but the cards they picked up, and the
     * talon but its trump; open hands: the talon only): the same move */
    for (OPEN = 0; OPEN <= 1; OPEN++) {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = 2 + gn % 5;
            VAR = gn % 2;
            dur_new(&T, VAR, NP, 3000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 2 + gn % 40 && T.pos.phase != DP_OVER; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            dpos *p = &T.pos;
            if (p->phase == DP_OVER) continue;
            int me = p->turn;
            uint64_t a1 = durb_choose(&T, me, OPEN, AI_MEDIUM, 77, NULL);
            /* the cards picked up: replayed as the bot does (a card a taker picked up stays) */
            uint64_t known[DUR_MAXP] = {0}, table = 0;
            dgame t;
            dur_new(&t, VAR, NP, T.seed);
            for (int i = 0; i < T.n; i++) {
                uint64_t a = T.act[i];
                int pl = t.pos.turn, bouts = t.pos.bouts;
                uint64_t cards = DA_KIND(a) == DA_PLAY ? DA_MASK(a)
                               : DA_KIND(a) == DA_BEAT || DA_KIND(a) == DA_SHIFT ? 1ull << dur_index(DA_CARD(a)) : 0;
                known[pl] &= ~cards;
                table |= cards;
                dur_play(&t, a);
                if (t.pos.bouts != bouts) {
                    if (t.pos.last_end == 2) known[t.pos.last_by] |= table;
                    table = 0;
                }
            }
            uint8_t pool[DUR_CARDS];
            int n = 0;
            for (int q = 0; q < NP && !OPEN; q++)
                for (int i = 0; q != me && i < p->n[q]; i++)
                    if (!(known[q] >> dur_index(p->hand[q][i]) & 1) && p->hand[q][i] != p->trump_card)
                        pool[n++] = p->hand[q][i];
            for (int i = 1; i < p->ntalon; i++) pool[n++] = p->talon[i];
            cdb_shuffle(pool, n, &r);
            n = 0;
            for (int q = 0; q < NP && !OPEN; q++)
                for (int i = 0; q != me && i < p->n[q]; i++)
                    if (!(known[q] >> dur_index(p->hand[q][i]) & 1) && p->hand[q][i] != p->trump_card)
                        p->hand[q][i] = pool[n++];
            for (int i = 1; i < p->ntalon; i++) p->talon[i] = pool[n++];
            p->seed ^= 0x5a5a5a5a;
            uint64_t a2 = durb_choose(&T, me, OPEN, AI_MEDIUM, 77, NULL);
            if (a2 != a1) printf("     game %d: %llx then %llx\n", gn, (unsigned long long)a1, (unsigned long long)a2);
            same &= a2 == a1;
            tried++;
        }
        snprintf(line, sizeof line, "%s hands: the unknown cards shuffled: the same move (%d positions)",
                 OPEN ? "open" : "hidden", tried);
        tt_check(line, same && tried >= 30);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        NP = 3, VAR = DUR_CLASSIC, OPEN = 0;
        dur_new(&T, VAR, NP, 5);
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
    cg_timing("durak", full ? 40 : 12, setup, choose);
    {   /* open hands, the talon out, two players: the alpha-beta endgame gives a legal move in time */
        int ok = 1, tried = 0;
        double worst = 0;
        OPEN = 1;
        for (int gn = 0; gn < 200 && tried < 20; gn++) {
            NP = 2, VAR = gn % 2;
            dur_new(&T, VAR, NP, 6000 + (uint32_t)gn);
            dealt = 1;
            while (T.pos.phase != DP_OVER && T.pos.ntalon) { char m[BOT_MOVE_CAP]; random_move(&r, m, sizeof m); one(m); }
            if (T.pos.phase == DP_OVER) continue;
            for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
                double t0 = cg_cpu_ms();
                uint64_t a = durb_choose(&T, T.pos.turn, OPEN, lv, 9, NULL);
                double dt = cg_cpu_ms() - t0;
                worst = dt > worst ? dt : worst;
                ok &= a && dur_legal(&T.pos, a);
            }
            tried++;
        }
        OPEN = 0;
        snprintf(line, sizeof line, "open hands, the talon out: alpha-beta moves legal (%d endgames, worst %.0f ms cpu)",
                 tried, worst);
        tt_check(line, ok && tried >= 10 && worst < ai_level_budget(AI_HARD).ms);
    }

    static const sp_game G = {
        .id = "durak", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {
        int ok = 1;
        for (OPEN = 0; OPEN <= 1; OPEN++)
            for (VAR = 0; VAR < DUR_VARIANTS; VAR++)
                for (NP = 2; NP <= 6; NP++)
                    ok &= cg_whole(&G, b, 70 + (uint32_t)(OPEN * 100 + VAR * 10 + NP), AI_EASY, SP_RANDOM, 3000);
        tt_check("whole games end (both variants, hidden and open, 2 to 6 players, easy and random)", ok);
    }
    if (tt_fails) return 1;

    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 85},
        {AI_MEDIUM, AI_EASY, 60, 65},
        {AI_HARD, AI_MEDIUM, 60, 60},
        {AI_HARD, AI_EASY, 40, 80},
    };
    static sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 30, 85}};
    int bad = 0;
    cg_scale(P, 4), cg_scale(Q, 1);
    for (OPEN = 0; OPEN <= 1; OPEN++)
        for (VAR = 0; VAR < DUR_VARIANTS; VAR++)
            for (NP = 2; NP <= 6; NP++) {
                snprintf(line, sizeof line, "%s, %s hands, %d players", VAR ? "perevodnoy" : "podkidnoy",
                         OPEN ? "open" : "hidden", NP);
                if (!cg_only(line)) continue;
                printf("-- %s\n", line);
                bad |= NP == 2 ? tt_run(&G, P, 4, argc, argv) : tt_run(&G, Q, 1, argc, argv);
            }
    return bad;
}
