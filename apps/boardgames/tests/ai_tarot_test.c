/*
 * French Tarot's bot (src/games/tarot_bot.c, trickbot.h): the core's rules against the game's (the legal cards over
 * random deals of 3, 4 and 5 players; a revealed deal played by the core: the same tricks, points, oudlers, petit au
 * bout and the same score), the view keeps to what the seat may know (the true hands fit it; determinised deals keep
 * the counts, voids, caps, own hand; the others' hidden cards swapped: the same move), the calls (bids, the taker's
 * results, the discard, the call, the poignées), the cancel, the levels' time, then self-play at every level
 * (tests/ai_selfplay.h; 4 players, then 3 and 5). Headless: this file plays the screen's part (tarot_live(),
 * tarot_show()). --full for the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include <math.h>

#include "games/tarot.h"                        /* before trickbot.h (ai_tricks.h): its TB_NONE enum */
#include "ai_tricks.h"

void tarotb_view(const tgame *g, int me, tb_state *v, tgame *t);
int  tarotb_count(const tb_state *st, int *half, int *oud);
int  tarotb_choose(const tgame *g, int seat, int level, uint64_t seed, ai_stats *stats);
int  tarotb_format(int a, char *move, int cap);
int  tarotb_points(const uint8_t *h, int n, int handsize);
int  tarotb_discard_plan(const tpos *p, uint8_t *plan);

static tgame T, U, S;                           /* the "screen's" game; a scratch copy; the view's replay */
static int NP = 4, dealt, no_poignee;
const tgame *tarot_live(void) { return &T; }

/* one action of the notation, as tarot_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        tarot_new(&T, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!tarot_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int tarot_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

/* the winner's half of the table (even seats: the "a" level's in even games), 2 a tie across the halves */
static int result(void)
{
    if (!dealt || T.pos.phase != TP_OVER) return -1;
    int top = -(1 << 20), half = 0;
    for (int q = 0; q < NP; q++) top = T.pos.total[q] > top ? T.pos.total[q] : top;
    for (int q = 0; q < NP; q++)
        if (T.pos.total[q] == top) half |= 1 << (q & 1);
    return half == 3 ? 2 : half == 2;
}

/* a random legal action (never a card taken back from the discard); a poignée a fifth of the times it may */
static int random_act(const tpos *p, ai_rng *r)
{
    int cand[40], n = 0, pl = p->turn;
    switch (p->phase) {
    case TP_BID:
        for (int b = TB_PASS; b <= TB_CONTRE; b++)
            if (tarot_legal(p, TACT(TA_BID, b))) cand[n++] = TACT(TA_BID, b);
        break;
    case TP_CALL:
        for (int s = 0; s < 4; s++) cand[n++] = TACT(TA_CALL, CARD(s, tarot_call_rank(p)));
        break;
    case TP_DOG: return TACT(TA_TAKE, 0);
    case TP_DISCARD:
        if (p->necart == p->dogsize) return TACT(TA_DONE, 0);
        for (int i = 0; i < p->nhand[pl]; i++)
            if (tarot_legal(p, TACT(TA_DISCARD, p->hand[pl][i]))) cand[n++] = TACT(TA_DISCARD, p->hand[pl][i]);
        break;
    case TP_PLAY:
        if (!no_poignee && ai_rand_n(r, 5) == 0)
            for (int l = 3; l >= 1; l--)
                if (tarot_legal(p, TACT(TA_POIGNEE, l))) return TACT(TA_POIGNEE, l);
        for (int i = 0; i < p->nhand[pl]; i++)
            if (tarot_legal(p, TACT(TA_PLAY, p->hand[pl][i]))) cand[n++] = TACT(TA_PLAY, p->hand[pl][i]);
        break;
    case TP_SCORE:
    case TP_VOID: return TACT(TA_NEXT, 0);
    }
    return n ? cand[ai_rand_n(r, n)] : 0;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    int a = random_act(&T.pos, r);
    return a && tarotb_format(a, m, cap) > 0;
}

static int play_random(ai_rng *r)
{
    char m[BOT_MOVE_CAP];
    return random_move(r, m, sizeof m) && one(m);
}

/* the deal as the core plays it, every hand known: the taker's view revealed, the aside put out of play, the sides */
static void reveal(tb_state *w, const tgame *g)
{
    const tpos *p = &g->pos;
    tarotb_view(g, p->taker, w, &S);
    tt_reveal(w, &p->hand[0][0], TAROT_HAND, p->nhand);
    if (p->contract >= TB_SANS) {
        memcpy(w->rest, p->ecart, p->necart);
        w->nrest = p->necart;
    }
    for (int q = 0; q < p->players; q++) w->team[q] = (uint8_t)tarot_attack(p, q);
}

static int legal_cards(const tpos *p, uint8_t *out)
{
    int k = 0;
    for (int i = 0; i < p->nhand[p->turn]; i++)
        if (tarot_legal(p, TACT(TA_PLAY, p->hand[p->turn][i]))) out[k++] = p->hand[p->turn][i];
    return k;
}

static void card_str(int c, char *o)
{
    if (c == TAROT_EXCUSE) snprintf(o, 8, "e");
    else snprintf(o, 8, "%c%d", "shdcaeee"[CARD_SUIT(c) & 7], CARD_RANK(c));
}

/* duplicate deals (--dup N a b): each deal played twice, level a on the even seats then on the odd ones, the bids and
 * the discard alike (they are the same at every level): what level a's seats score, a deal, on average. The cards'
 * luck cancels out; the card play's worth shows (four players, one deal each). */
extern int tarotb_exp_rw[2];   /* EXPERIMENT */
static int VA = -1, VB = -1;
static double dup_deal(uint32_t seed, int la, int lb, int swap, int *ok)
{
    tarotb_exp_rw[swap] = VA, tarotb_exp_rw[!swap] = VB;
    NP = 4;
    tarot_new(&T, NP, seed);
    dealt = 1;
    for (int guard = 0; guard < 400; guard++) {
        const tpos *p = &T.pos;
        if (p->phase == TP_SCORE || p->phase == TP_OVER || p->phase == TP_VOID) break;
        int seat = p->turn, lv = ((seat & 1) ^ swap) ? lb : la;
        if (!tarot_play(&T, tarotb_choose(&T, seat, lv, ai_mix(seed, (uint64_t)guard), NULL))) break;
    }
    const tpos *p = &T.pos;
    *ok = p->phase == TP_SCORE || p->phase == TP_OVER;
    double s = 0;
    for (int q = 0; q < NP; q++)
        if (((q & 1) ^ swap) == 0) s += p->score[q];
    return s;
}

static void dup_run(int n, int la, int lb)
{
    double *R = mmap(NULL, sizeof(double) * 2 * (size_t)n, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int *next = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (R == MAP_FAILED || next == MAP_FAILED) return;
    *next = 0;
    int jobs = (int)sysconf(_SC_NPROCESSORS_ONLN);
    fflush(stdout);
    for (int j = 0; j < jobs; j++)
        if (!fork()) {
            for (;;) {
                int i = __atomic_fetch_add(next, 1, __ATOMIC_SEQ_CST), ok1, ok2;
                if (i >= n) _exit(0);
                double a = dup_deal(20000u + (uint32_t)i, la, lb, 0, &ok1);
                double b = dup_deal(20000u + (uint32_t)i, la, lb, 1, &ok2);
                R[2 * i] = ok1 && ok2 ? a + b : 0, R[2 * i + 1] = ok1 && ok2;
            }
        }
    while (wait(NULL) > 0) {}
    double sum = 0, sq = 0;
    int k = 0;
    for (int i = 0; i < n; i++)
        if (R[2 * i + 1]) sum += R[2 * i], sq += R[2 * i] * R[2 * i], k++;
    double m = k ? sum / k : 0, sd = k > 1 ? sqrt((sq - k * m * m) / (k - 1)) : 0;
    printf("     duplicate %s-%s: %d deals, %s's seats score %+.1f a deal pair (+- %.1f)\n", sp_name(la), sp_name(lb), k,
           sp_name(la), m, k ? sd / sqrt(k) : 0);
    munmap(R, sizeof(double) * 2 * (size_t)n);
    munmap(next, sizeof(int));
}

int main(int argc, char **argv)
{
    char name[200];
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    for (int i = 1; i + 3 < argc; i++)
        if (!strcmp(argv[i], "--dup")) {
            dup_run(atoi(argv[i + 1]), atoi(argv[i + 2]), atoi(argv[i + 3]));
            return 0;
        }
    for (int i = 1; i + 4 < argc; i++)
        if (!strcmp(argv[i], "--dupv")) {
            VA = atoi(argv[i + 3]), VB = atoi(argv[i + 4]);
            dup_run(atoi(argv[i + 1]), atoi(argv[i + 2]), atoi(argv[i + 2]));
            return 0;
        }
    ai_rng r;
    ai_seed(&r, 11);
    static tb_state v, w, w2;
    /* the core's rules are the game's: the legal cards at every turn; a revealed deal played card by card keeps the
     * game's tricks and turns, and ends on its points, oudlers, petit au bout and score */
    {
        int same = 1, plays = 0, tricks_ok = 1, count_ok = 1, deals = 0, lasts = 0, pabs = 0, sweeps = 0;
        for (int gn = 0; gn < 240 && same && tricks_ok && count_ok; gn++) {
            NP = 3 + gn % 3;
            tarot_new(&T, NP, 500 + gn);
            dealt = 1;
            int fresh = 1, fresh2 = 1;
            for (int guard = 0; guard < 4000 && T.pos.phase != TP_OVER && same && tricks_ok && count_ok; guard++) {
                const tpos *p = &T.pos;
                int a = random_act(p, &r);
                if (p->phase == TP_PLAY && TA_KIND(a) == TA_PLAY) {
                    tarotb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND], G[TAROT_HAND];
                    int k = tb_legal(&v, L), want = legal_cards(p, G);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = tarot_legal(p, TACT(TA_PLAY, L[i]));
                    if (!same) {
                        char cs[8];
                        printf("     game %d: core", gn);
                        for (int i = 0; i < k; i++) printf(" %s", (card_str(L[i], cs), cs));
                        printf("; game");
                        for (int i = 0; i < want; i++) printf(" %s", (card_str(G[i], cs), cs));
                        printf("; trick");
                        for (int i = 0; i < p->ntrick; i++) printf(" %s", (card_str(p->trick[i], cs), cs));
                        printf("; view hand");
                        for (int i = 0; i < v.n[v.turn]; i++) printf(" %s", (card_str(v.hand[v.turn][i], cs), cs));
                        printf(" turn %d/%d\n", v.turn, p->turn);
                    }
                    if (fresh) reveal(&w, &T), fresh = 0;
                    if (fresh2 && p->tricks == p->handsize - 1 && p->ntrick == 0) reveal(&w2, &T), fresh2 = 0;
                    plays++;
                    tb_play(&w, TA_ARG(a));
                    if (!fresh2 && p->tricks == p->handsize - 1) tb_play(&w2, TA_ARG(a));
                }
                int before = p->tricks, ph = p->phase;
                tarot_play(&T, a);
                if (ph == TP_PLAY && TA_KIND(a) == TA_PLAY && p->tricks != before && p->phase == TP_PLAY) {
                    for (int q = 0; q < NP; q++) tricks_ok &= w.won[q] == p->won[q];
                    tricks_ok &= w.turn == p->turn;
                }
                if (ph == TP_PLAY && (p->phase == TP_SCORE || p->phase == TP_OVER)) {
                    int half, oud, unit = tarotb_count(&w, &half, &oud), h2, o2, unit2 = tarotb_count(&w2, &h2, &o2);
                    const tresult *res = &p->res;
                    for (int q = 0; q < NP; q++) tricks_ok &= w.won[q] == p->won[q];
                    tricks_ok &= tb_over(&w);
                    count_ok &= half == res->half && oud == res->oud && (res->poignee || unit == res->unit);
                    count_ok &= h2 == res->half && o2 == res->oud && unit2 == res->unit;
                    deals++, lasts++;
                    pabs += res->pab != 0;
                    sweeps += res->chelem != 0;
                    if (!count_ok)
                        printf("     game %d (%d players): half %d/%d/%d oud %d/%d/%d unit %d/%d/%d (poignee %d)\n", gn,
                               NP, half, h2, res->half, oud, o2, res->oud, unit, unit2, res->unit, res->poignee);
                }
                if (p->phase == TP_SCORE || p->phase == TP_VOID) fresh = fresh2 = 1;
                if (!tricks_ok) printf("     game %d: tricks %d turn %d/%d\n", gn, p->tricks, w.turn, p->turn);
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns, 3 to 5 players)", plays);
        tt_check(name, same && plays > 10000);
        tt_check("a revealed deal played by the core: the same tricks won and turns", tricks_ok);
        snprintf(name, sizeof name, "... and the same points, oudlers and score (%d deals, %d from the last trick, %d "
                 "petits au bout, %d chelems)", deals, lasts, pabs, sweeps);
        tt_check(name, count_ok && deals > 100);
    }
    /* the view: the true hands fit what it knows; determinised deals keep the counts, voids, caps and own hand, deal
     * the discard no trump, king nor Excuse, and the called card's holder is the partner */
    {
        int ok = 1, truth = 1, aside = 1, partner = 1, tried = 0, relaxed = 0;
        for (int gn = 0; gn < 150 && ok && truth && aside && partner; gn++) {
            NP = 3 + gn % 3;
            tarot_new(&T, NP, 900 + gn);
            dealt = 1;
            int stop = 10 + gn % 60, plays = 0;
            for (int k = 0; k < 600 && T.pos.phase != TP_OVER; k++) {
                if (T.pos.phase == TP_PLAY && plays++ >= stop) break;
                play_random(&r);
            }
            const tpos *p = &T.pos;
            if (p->phase != TP_PLAY) continue;
            int me = p->turn;
            tarotb_view(&T, me, &v, &S);
            for (int q = 0; q < NP; q++) {      /* every card a seat truly holds fits the view */
                if (q == me) continue;
                for (int i = 0; i < p->nhand[q]; i++) {
                    int c = p->hand[q][i], known = 0;
                    for (int j = 0; j < v.nknown[q]; j++) known |= v.hand[q][j] == c;
                    if (!known && !tb_fits(&v, q, c) && !(q == p->taker && c == p->called)) {   /* (the taker's own king called: never assumed) */
                        char cs[8];
                        card_str(c, cs);
                        printf("     game %d: seat %d holds %s, the view of %d says not\n", gn, q, cs, me);
                        truth = 0;
                    }
                }
            }
            if (v.sitting != TB_NONE)           /* the discard's unseen cards fit its seat */
                for (int i = 0; i < p->necart; i++)
                    if (CARD_SUIT(p->ecart[i]) != SUIT_TRUMP) truth &= tb_fits(&v, v.sitting, p->ecart[i]);
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                tried++;
                if (!tt_dealt_ok(&v, &w)) {     /* the core's dealer gave up the caps (a tight end): counts kept */
                    relaxed++;
                    tb_state v2 = v;
                    memset(v2.cap, TB_NOCAP, sizeof v2.cap);
                    memset(v2.ex, 0, sizeof v2.ex);
                    ok &= tt_dealt_ok(&v2, &w);
                }
                for (int i = 0; i < p->nhand[me]; i++) ok &= w.hand[me][i] == p->hand[me][i];
                if (w.sitting != TB_NONE)
                    for (int i = 0; i < w.n[w.sitting]; i++) {
                        int c = w.hand[w.sitting][i];
                        aside &= CARD_SUIT(c) != SUIT_TRUMP && CARD_RANK(c) != TAROT_KING;
                    }
                if (NP == 5) {
                    int holder = p->taker;
                    for (int q = 0; q < NP; q++)
                        for (int i = 0; i < w.n[q]; i++)
                            if (w.hand[q][i] == p->called && q != p->taker) holder = q;
                    for (int q = 0; q < NP; q++)
                        if (!p->revealed) partner &= w.team[q] == (q == p->taker || q == holder);
                    for (int q = 0; q < NP; q++)
                        if (p->revealed) partner &= w.team[q] == tarot_attack(p, q);
                }
            }
        }
        tt_check("the true hands fit the view (voids, caps, the dog, the poignées, the discard)", truth);
        snprintf(name, sizeof name, "determinised deals keep counts, voids, caps, own hand (%d; the caps let go in %d)",
                 tried, relaxed);
        tt_check(name, ok && tried > 1000 && relaxed * 100 <= tried);
        tt_check("the discard unseen is dealt no trump, king nor Excuse", aside);
        tt_check("five players: the called card's holder is the partner in each dealt world", partner);
    }

    const bot *b = bot_find("tarot");
    tt_check("registered: bot_find(\"tarot\")", b && b->think && b->play == tarot_show && b->actor);

    /* honest: two other seats' hidden cards swapped (the same suit): the same move */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            NP = 3 + gn % 3;
            tarot_new(&T, NP, 3000 + gn);
            dealt = 1;
            int stop = 4 + gn % 40, plays = 0;
            for (int k = 0; k < 600 && T.pos.phase != TP_OVER; k++) {
                if (T.pos.phase == TP_PLAY && plays++ >= stop) break;
                play_random(&r);
            }
            tpos *p = &T.pos;
            if (p->phase != TP_PLAY) continue;
            int me = p->turn, a = (me + 1) % NP, c = (me + 2) % NP, swapped = 0;
            for (int i = 0; i < p->nhand[a] && !swapped; i++)
                for (int j = 0; j < p->nhand[c] && !swapped; j++)
                    if (CARD_SUIT(p->hand[a][i]) == CARD_SUIT(p->hand[c][j]) && p->hand[a][i] != p->called &&
                        p->hand[c][j] != p->called) {
                        uint8_t t = p->hand[a][i];
                        p->hand[a][i] = p->hand[c][j];
                        p->hand[c][j] = t;
                        swapped = 1;
                    }
            if (!swapped) continue;
            tgame keep = T;
            int m1 = tarotb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tarot_new(&T, NP, 3000 + gn);       /* back to the true deal */
            for (int i = 0; i < keep.n; i++) tarot_play(&T, keep.act[i]);
            int m2 = tarotb_choose(&T, me, AI_MEDIUM, 77, NULL);
            same &= m1 == m2;
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 30);
    }

    /* the calls: bot seats bid, call, discard and show (easy card play, for time): the bids made, the taker's
     * results, a discard never under the rules' limits, the poignées */
    {
        int ndeals = full ? 400 : 48, bids[5] = {0}, made[5] = {0}, taken[5] = {0}, passed = 0, scored = 0;
        int own_call = 0, calls = 0, poig = 0, poig_won = 0, bad = 0, dealsn[6] = {0};
        long half_sum = 0;
        for (int dn = 0; dn < ndeals && !bad; dn++) {
            NP = 3 + dn % 3;
            tarot_new(&T, NP, 12000 + dn);
            dealt = 1;
            dealsn[NP]++;
            int shown = 0;
            for (int guard = 0; guard < 600; guard++) {
                const tpos *p = &T.pos;
                if (p->phase == TP_SCORE || p->phase == TP_OVER || p->phase == TP_VOID) break;
                int a = tarotb_choose(&T, p->turn, AI_EASY, 1000 + (uint64_t)dn * 977 + guard, NULL);
                if (TA_KIND(a) == TA_BID) bids[TA_ARG(a)]++;
                if (TA_KIND(a) == TA_CALL) {
                    calls++;
                    for (int i = 0; i < p->nhand[p->turn]; i++) own_call += p->hand[p->turn][i] == TA_ARG(a);
                }
                if (TA_KIND(a) == TA_POIGNEE) poig++, shown |= 1 << p->turn;
                if (!tarot_play(&T, a)) { bad = 1; printf("     deal %d: the bot's action refused\n", dn); break; }
            }
            const tpos *p = &T.pos;
            if (p->phase == TP_VOID) { passed += p->sec == TAROT_NONE; continue; }
            if (p->phase != TP_SCORE && p->phase != TP_OVER) { bad = 1; break; }
            scored++;
            taken[p->contract]++;
            made[p->contract] += p->res.made;
            half_sum += p->res.half;
            for (int q = 0; q < NP; q++)
                if (shown >> q & 1) poig_won += tarot_attack(p, q) == p->res.made;
        }
        printf("     bids: pass %d, Petite %d, Garde %d, Garde sans %d, Garde contre %d; all passed %d of %d deals\n",
               bids[0], bids[1], bids[2], bids[3], bids[4], passed, ndeals);
        printf("     the taker made it: Petite %d/%d, Garde %d/%d, Garde sans %d/%d, Garde contre %d/%d; "
               "the taker's side's points %.1f on average\n", made[1], taken[1], made[2], taken[2], made[3], taken[3],
               made[4], taken[4], scored ? half_sum / 2.0 / scored : 0);
        printf("     five players: %d calls, %d of its own card; poignées shown %d, on the winning side %d\n", calls,
               own_call, poig, poig_won);
        int t = taken[1] + taken[2] + taken[3] + taken[4], m = made[1] + made[2] + made[3] + made[4];
        tt_check("the bots' calls are legal, every deal ends", !bad);
        snprintf(name, sizeof name, "a taker in most deals (%d of %d), who makes it often (%d%%)", t, ndeals,
                 t ? 100 * m / t : 0);
        tt_check(name, t * 2 >= ndeals && m * 100 >= t * 40 && !own_call);
    }
    /* the discard: no king, no oudler; trumps only when forced; and the plan is the rules' */
    {
        int ok = 1, voids = 0, deals = 0;
        for (int dn = 0; dn < 300 && ok; dn++) {
            NP = 3 + dn % 3;
            tarot_new(&T, NP, 15000 + dn);
            dealt = 1;
            if (T.pos.phase != TP_BID) continue;
            one("b1");
            while (T.pos.phase == TP_BID) one("p");
            if (T.pos.phase == TP_CALL) one(".");
            one("t");
            const tpos *p = &T.pos;
            if (p->phase != TP_DISCARD) { ok = 0; break; }
            deals++;
            int len0[4] = {0}, len1[4] = {0};
            for (int i = 0; i < p->nhand[p->taker]; i++)
                if (CARD_SUIT(p->hand[p->taker][i]) < 4) len0[CARD_SUIT(p->hand[p->taker][i])]++;
            while (p->phase == TP_DISCARD) {
                int a = tarotb_choose(&T, p->taker, AI_HARD, 1, NULL);
                if (TA_KIND(a) == TA_DISCARD)
                    ok &= !tarot_oudler(TA_ARG(a)) && CARD_RANK(TA_ARG(a)) != TAROT_KING;
                if (!tarot_play(&T, a)) { ok = 0; break; }
            }
            for (int i = 0; i < p->nhand[p->taker]; i++)
                if (CARD_SUIT(p->hand[p->taker][i]) < 4) len1[CARD_SUIT(p->hand[p->taker][i])]++;
            for (int s = 0; s < 4; s++) voids += len0[s] && !len1[s];
        }
        snprintf(name, sizeof name, "the discard: legal, no king nor oudler (%d deals, %d suits voided)", deals, voids);
        tt_check(name, ok && deals > 200 && voids > deals / 2);
    }
    {   /* cancelled: no move, at once; not the seat to act: no move */
        char m[BOT_MOVE_CAP] = "";
        NP = 4;
        tarot_new(&T, NP, 5);
        dealt = 1;
        while (T.pos.phase != TP_PLAY && T.pos.phase != TP_OVER) play_random(&r);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to act: no move", !b->think((T.pos.turn + 1) % NP, AI_HARD, m, sizeof m));
        tt_check("the seat to act: a move", b->think(T.pos.turn, AI_EASY, m, sizeof m) && m[0]);
    }
    {   /* the levels' work: iterations and time on positions of random deals */
        int npos = full ? 40 : 10;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int done = 0;
            for (int k = 0; done < npos && k < 50 * npos; k++) {
                NP = 3 + k % 3;
                tarot_new(&T, NP, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k);
                int stop = 1 + k % 50, plays = 0;
                for (int i = 0; i < 600 && T.pos.phase != TP_OVER; i++) {
                    if (T.pos.phase == TP_PLAY && plays++ >= stop) break;
                    play_random(&r);
                }
                if (T.pos.phase != TP_PLAY) continue;
                uint8_t L[TAROT_HAND];
                if (legal_cards(&T.pos, L) < 2) continue;
                ai_stats s;
                double t0 = ai_now_ms();
                tarotb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes, done++;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f), a choice of 2 cards or more\n", sp_name(lv),
                   nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "tarot", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 75},
        {AI_MEDIUM, AI_EASY, 200, 52},
        {AI_HARD, AI_MEDIUM, 200, 45},
        {AI_HARD, AI_EASY, 200, 55},
    };
    NP = 4;
    int bad = tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 40, 60}};
    for (NP = 3; NP <= 5; NP += 2) {
        printf("-- %d players\n", NP);
        bad |= tt_run(&G, Q, 1, argc, argv);
    }
    return bad;
}
