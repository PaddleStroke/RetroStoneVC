/*
 * Golf's bot (src/games/golf_bot.c): the view keeps to what the seat may know (the face-down cards and the stock
 * shuffled among themselves: the same move), a low card on the discard taken, the cancel, the levels' work, whole
 * games to their end (both variants, 2 to 6 players), then self-play by duplicate deals (tests/ai_tricks.h): a
 * "game" there is one round (hole), won by the lowest score. Headless: this file plays the screen's part (golf_live(),
 * golf_show()). --full for the long run and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"

#include "games/cardbot.h"
#include "games/golf.h"

static ggame T, U;
static int NP = 2, VAR = GOLF_CLASSIC, dealt, ROUND = 1;
const ggame *golf_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        golf_new(&T, VAR, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!golf_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int golf_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

/* the lowest's half of the table (2: both halves) */
static int result(void)
{
    if (!dealt) return -1;
    const gpos *p = &T.pos;
    if (p->phase != GP_OVER && !(ROUND && p->phase == GP_ROUND)) return -1;
    const int16_t *sc = ROUND ? p->last : p->total;
    int low = 1 << 20, half = 0;
    for (int q = 0; q < NP; q++) low = sc[q] < low ? sc[q] : low;
    for (int q = 0; q < NP; q++)
        if (sc[q] == low) half |= 1 << (q & 1);
    return half == 3 ? 2 : half == 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const gpos *p = &T.pos;
    static const int kinds[] = {GA_REVEAL, GA_SWAP, GA_FLIP};
    static const char ch[] = "rwf";
    char L[40][8];
    int k = 0;
    if (p->phase == GP_ROUND) { snprintf(m, (size_t)cap, "n"); return 1; }
    if (golf_legal(p, GA_STOCK)) snprintf(L[k++], 8, "s");
    if (golf_legal(p, GA_TAKE)) snprintf(L[k++], 8, "t");
    if (golf_legal(p, GA_DISCARD)) snprintf(L[k++], 8, "d");
    for (int i = 0; i < 3; i++)
        for (int s = 0; s < p->slots; s++)
            if (golf_legal(p, kinds[i] | s)) snprintf(L[k++], 8, "%c%d", ch[i], s);
    if (!k) return 0;
    snprintf(m, (size_t)cap, "%s", L[ai_rand_n(r, k)]);
    return 1;
}

static int choose(int seat, int level, uint64_t seed, ai_stats *st) { return glfb_choose(&T.pos, seat, level, seed, st); }

static int setup(int k, ai_rng *r)
{
    NP = 2 + k % 5;
    VAR = k % 2;
    golf_new(&T, VAR, NP, 40 + (uint32_t)k);
    dealt = 1;
    for (int i = 0, n = ai_rand_n(r, 40); i < n && T.pos.phase < GP_ROUND; i++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return T.pos.phase < GP_ROUND ? T.pos.turn : -1;
}

int main(int argc, char **argv)
{
    char line[200];
    int full = cg_full(argc, argv);
    ai_rng r;
    ai_seed(&r, 11);
    const bot *b = bot_find("golf");
    tt_check("registered: bot_find(\"golf\")", b && b->think && b->play == golf_show && b->actor);
    if (!b) return 1;

    {   /* honest: the face-down cards and the stock shuffled among themselves: the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = 2 + gn % 5;
            VAR = gn % 2;
            golf_new(&T, VAR, NP, 3000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 2 + gn % 30 && T.pos.phase < GP_ROUND; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            gpos *p = &T.pos;
            if (p->phase >= GP_ROUND) continue;
            int me = p->turn, a1 = glfb_choose(p, me, AI_MEDIUM, 77, NULL);
            uint8_t pool[CARDS_MAX * 2];
            int n = 0;
            for (int q = 0; q < NP; q++)
                for (int s = 0; s < p->slots; s++)
                    if (!(p->up[q] >> s & 1) && !(p->gone[q] >> s & 1)) pool[n++] = p->card[q][s];
            memcpy(pool + n, p->stock, (size_t)p->nstock), n += p->nstock;
            cdb_shuffle(pool, n, &r);
            n = 0;
            for (int q = 0; q < NP; q++)
                for (int s = 0; s < p->slots; s++)
                    if (!(p->up[q] >> s & 1) && !(p->gone[q] >> s & 1)) p->card[q][s] = pool[n++];
            memcpy(p->stock, pool + n, (size_t)p->nstock);
            p->seed ^= 0x5a5a5a5a;
            int a2 = glfb_choose(p, me, AI_MEDIUM, 77, NULL);
            if (a2 != a1) printf("     game %d: %x then %x\n", gn, a1, a2);
            same &= a2 == a1;
            tried++;
        }
        snprintf(line, sizeof line, "the face-down cards and the stock shuffled: the same move (%d positions)", tried);
        tt_check(line, same && tried >= 30);
    }
    {   /* a king (0) on the discard, a high card face up: taken (classic, medium and hard) */
        int found = 0, took = 0;
        for (int gn = 0; gn < 400 && found < 30; gn++) {
            NP = 2, VAR = GOLF_CLASSIC;
            golf_new(&T, VAR, NP, 9000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 4; k++) { char m[BOT_MOVE_CAP]; random_move(&r, m, sizeof m); one(m); }
            gpos *p = &T.pos;
            if (p->phase != GP_DRAW) continue;
            int pl = p->turn, high = 0;
            for (int s = 0; s < p->slots; s++)
                if (p->up[pl] >> s & 1) high |= golf_value(p, p->card[pl][s]) >= 9;
            p->pile[p->npile - 1] = CARD(SUIT_SPADES, RANK_KING);
            int dup = 0;                        /* the king of spades must not be elsewhere too */
            for (int q = 0; q < NP; q++)
                for (int s = 0; s < p->slots; s++) dup |= p->card[q][s] == CARD(SUIT_SPADES, RANK_KING);
            if (!high || dup) continue;
            found++;
            took += glfb_choose(p, pl, AI_MEDIUM + (found & 1), 3, NULL) == GA_TAKE;
        }
        snprintf(line, sizeof line, "a king on the discard over a high card: taken %d of %d", took, found);
        tt_check(line, found >= 20 && took * 10 >= found * 9);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        NP = 3, VAR = GOLF_CLASSIC;
        golf_new(&T, VAR, NP, 5);
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
    cg_timing("golf", full ? 40 : 10, setup, choose);

    static const sp_game G = {
        .id = "golf", .max_plies = 6000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {
        int ok = 1;
        ROUND = 0;
        for (VAR = 0; VAR < GOLF_VARIANTS; VAR++)
            for (NP = 2; NP <= 6; NP++) ok &= cg_whole(&G, b, 70 + (uint32_t)(VAR * 10 + NP), AI_EASY, SP_RANDOM, 20000);
        ROUND = 1;
        tt_check("whole games end (both variants, 2 to 6 players, easy and random)", ok);
    }
    if (tt_fails) return 1;

    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 85},
        {AI_MEDIUM, AI_EASY, 60, 45},
        {AI_HARD, AI_MEDIUM, 60, 40},
        {AI_HARD, AI_EASY, 40, 60},
    };
    static sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 30, 85}};
    int bad = 0;
    cg_scale(P, 4), cg_scale(Q, 1);
    for (VAR = 0; VAR < GOLF_VARIANTS; VAR++)
        for (NP = 2; NP <= 6; NP++) {
            snprintf(line, sizeof line, "%s, %d players", VAR ? "number golf" : "six-card golf", NP);
            if (!cg_only(line)) continue;
            printf("-- %s\n", line);
            bad |= NP == 2 ? tt_run(&G, P, 4, argc, argv) : tt_run(&G, Q, 1, argc, argv);
        }
    return bad;
}
