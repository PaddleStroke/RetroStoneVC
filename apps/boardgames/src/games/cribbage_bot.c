/*
 * Cribbage's bot (bots.h), from what its seat may know: its own six (or five) cards, the starter, the cards played,
 * and the "go"s heard (a player passed over holds nothing small enough for the count then, nor since). The other
 * hands are dealt at random from the unseen cards, keeping to those "go"s. Runs on the worker thread: it copies the
 * game at entry and reads only that copy. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   The crib: every way to lay the cards away, worth the hand kept (its show averaged over every starter it may
 *   meet) plus the crib's (sampled: the others' cards to it and the starter at random), counted for the dealer and
 *   against the others; a little for low cards kept for the play.
 *   The play: each card it may lay, worth the points it pegs and the points pegged after it to the end of the play,
 *   both sides playing their best (an exhaustive search of the cards left), averaged over deals of the other hands.
 * Levels: easy keeps the hand that shows best with a few starters and ignores the crib, and pegs what it sees at once
 * (a slip now and then); medium samples the crib a little and searches the play on a few deals; hard samples more
 * and searches on many deals.
 * Its moves are the game's notation ("d03", "c", "p2", "n"), played by cribbage_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/cribbage.h"

/* ---- the play, simulated ----------------------------------------------------------------------------------------- */
typedef struct {
    uint8_t np, turn, last, count, nseq, seq[16];
    uint8_t hand[CRIB_MAXP][4], n[CRIB_MAXP];
    int8_t pts[CRIB_MAXP];
} peg_st;

static int fits(const peg_st *s, int q)
{
    for (int i = 0; i < s->n[q]; i++)
        if (s->count + crib_value(s->hand[q][i]) <= 31) return 1;
    return 0;
}

/* the points of the card just laid (as the rules' play_points) */
static int laid_points(const peg_st *s)
{
    int n = s->nseq, sum = 0;
    if (s->count == 15 || s->count == 31) sum += 2;
    int k = 1;
    while (k < n && CARD_RANK(s->seq[n - 1 - k]) == CARD_RANK(s->seq[n - 1])) k++;
    if (k >= 2) sum += k * (k - 1);
    for (int len = n; len >= 3; len--) {
        int seen = 0, lo = 99, hi = 0, ok = 1;
        for (int i = n - len; i < n && ok; i++) {
            int r = CARD_RANK(s->seq[i]);
            ok = !(seen >> r & 1);
            seen |= 1 << r;
            lo = r < lo ? r : lo;
            hi = r > hi ? r : hi;
        }
        if (ok && hi - lo + 1 == len) { sum += len; break; }
    }
    return sum;
}

/* lays hand[turn][i]; then the go, a new count (as the rules' advance); 0: the play is over */
static int peg_lay(peg_st *s, int i)
{
    int q = s->turn, c = s->hand[q][i];
    s->hand[q][i] = s->hand[q][--s->n[q]];
    s->seq[s->nseq++] = (uint8_t)c;
    s->count = (uint8_t)(s->count + crib_value(c));
    s->last = (uint8_t)q;
    s->pts[q] = (int8_t)(s->pts[q] + laid_points(s));
    int np = s->np;
    if (s->count < 31)
        for (int k = 1; k <= np; k++) {
            int o = (q + k) % np;
            if (fits(s, o)) { s->turn = (uint8_t)o; return 1; }
        }
    int any = 0;
    for (int o = 0; o < np; o++) any |= s->n[o] > 0;
    if (s->count < 31) s->pts[q]++;
    s->nseq = 0;
    s->count = 0;
    for (int k = 1; k <= np; k++) {
        int o = (q + k) % np;
        if (s->n[o]) { s->turn = (uint8_t)o; return 1; }
    }
    (void)any;
    return 0;
}

/* the points each player pegs from here to the end, every one playing for their own lead */
static void peg_search(const peg_st *s, int *out)
{
    int q = s->turn, np = s->np, best = -1000;
    for (int o = 0; o < np; o++) out[o] = 0;
    uint8_t tried[14] = {0};
    for (int i = 0; i < s->n[q]; i++) {
        int c = s->hand[q][i], r = CARD_RANK(c);
        if (s->count + crib_value(c) > 31 || tried[r]) continue;
        tried[r] = 1;                           /* suits never count in the play */
        peg_st t = *s;
        int v[CRIB_MAXP] = {0};
        int more = peg_lay(&t, i);
        for (int o = 0; o < np; o++) v[o] = t.pts[o] - s->pts[o];
        if (more) {
            int w[CRIB_MAXP];
            peg_search(&t, w);
            for (int o = 0; o < np; o++) v[o] += w[o];
        }
        int others = 0;
        for (int o = 0; o < np; o++) others += o == q ? 0 : v[o];
        int score = v[q] * (np - 1) - others;
        if (score > best) {
            best = score;
            for (int o = 0; o < np; o++) out[o] = v[o];
        }
    }
}

/* ---- the view ---------------------------------------------------------------------------------------------------- */
typedef struct {
    uint8_t me, np;
    uint8_t mine[6], nmine;                     /* the cards dealt to me */
    uint8_t unseen[52], nunseen;
    uint8_t low[CRIB_MAXP];                     /* a "go" heard: the player's cards are all worth more than this */
} cview;

static void view_of(const cgame *g, int me, cview *v)
{
    const cpos *p = &g->pos;
    memset(v, 0, sizeof *v);
    v->me = (uint8_t)me;
    v->np = p->players;
    cgame *t = malloc(sizeof *t);
    if (t) {                                    /* the hand's actions replayed: my cards, the "go"s */
        crib_new(t, g->players, g->seed);
        for (int i = 0; i < g->n; i++) {
            const cpos *q = &t->pos;
            if (q->phase == CP_SHOW || q->phase == CP_DISCARD) memset(v->low, 0, sizeof v->low);
            crib_play(t, g->act[i]);
            if (q->phase == CP_PLAY)
                for (int o = 0; o < q->players; o++)
                    if (o != me && crib_said_go(q, o) && 31 - q->count > v->low[o]) v->low[o] = (uint8_t)(31 - q->count);
        }
        free(t);
    }
    if (p->phase == CP_DISCARD) {
        v->nmine = p->nhand[me];
        memcpy(v->mine, p->hand[me], v->nmine);
    } else {                                    /* the four kept, and the cards laid away (my own: I know them) */
        v->nmine = p->ndeal;
        memcpy(v->mine, p->hand[me], v->nmine);
    }
    uint8_t seen[128] = {0};
    for (int i = 0; i < v->nmine; i++) seen[v->mine[i]] = 1;
    if (p->starter != CARD_NONE) seen[p->starter] = 1;
    for (int o = 0; o < p->players; o++)
        if (p->phase >= CP_PLAY)
            for (int s = 0; s < 4; s++)
                if (p->used[o] >> s & 1) seen[p->kept[o][s]] = 1;
    uint8_t pack[52];
    int n = cards_pack(PACK_52, pack);
    for (int i = 0; i < n; i++)
        if (!seen[pack[i]]) v->unseen[v->nunseen++] = pack[i];
}

/* ---- the crib ---------------------------------------------------------------------------------------------------- */
static int show4(const uint8_t *four, int starter, int crib)
{
    crib_ev it[CRIB_MAX_EV];
    int n;
    return crib_show(four, starter, crib, it, &n);
}

static int pick_away(const cpos *p, const cview *v, int level, ai_rng *r)
{
    int n = v->nmine, away = crib_away(p), me = v->me, dealer = me == p->dealer;
    int samples = level == AI_EASY ? 0 : level == AI_MEDIUM ? 24 : 160;
    int starters = level == AI_EASY ? 6 : 0;
    double best = -1e9;
    int bm = away == 2 ? 3 : 1;
    for (int m = 1; m < 1 << n; m++) {
        if (__builtin_popcount((unsigned)m) != away) continue;
        uint8_t keep[4], lay[2], pool[52];
        int nk = 0, nl = 0, np = 0;
        for (int i = 0; i < n; i++) {
            if (m >> i & 1) lay[nl++] = v->mine[i];
            else keep[nk++] = v->mine[i];
        }
        for (int i = 0; i < v->nunseen; i++) pool[np++] = v->unseen[i];
        double hand = 0;
        if (starters) {
            for (int k = 0; k < starters; k++) hand += show4(keep, pool[ai_rand_n(r, np)], 0);
            hand /= starters;
        } else {
            for (int i = 0; i < np; i++) hand += show4(keep, pool[i], 0);
            hand /= np;
        }
        double crib = 0;
        for (int k = 0; k < samples; k++) {     /* the others' cards to the crib and the starter, at random */
            uint8_t c[4], st;
            int got = nl;
            memcpy(c, lay, (size_t)nl);
            for (int j = 0; j < np; j++) {      /* a partial shuffle of the pool */
                int x = j + ai_rand_n(r, np - j);
                uint8_t t = pool[j];
                pool[j] = pool[x];
                pool[x] = t;
            }
            for (int j = 0; got < 4; j++) c[got++] = pool[j];
            st = pool[4];
            crib += show4(c, st, 1);
        }
        if (samples) crib /= samples;
        double low = 0;                         /* low cards kept help the play a little */
        for (int i = 0; i < nk; i++) low += crib_value(keep[i]) <= 5 ? 0.15 : 0;
        double w = hand + (dealer ? crib : -crib) + low;
        if (level == AI_EASY) w += ai_rand_f(r) * 1.5;
        if (w > best) best = w, bm = m;
    }
    return bm;
}

/* ---- the play ---------------------------------------------------------------------------------------------------- */
/* a deal of the others' cards left, keeping to the "go"s heard: 0 if none found */
static int deal_others(const cpos *p, const cview *v, peg_st *s, ai_rng *r)
{
    uint8_t pool[52];
    int np = v->nunseen;
    for (int tries = 0; tries < 40; tries++) {
        memcpy(pool, v->unseen, (size_t)np);
        int k = 0, ok = 1;
        for (int o = 0; o < p->players && ok; o++) {
            if (o == v->me) continue;
            for (int i = 0; i < s->n[o] && ok; i++) {
                int found = -1;
                for (int a = 0; a < 8 && found < 0; a++) {
                    int x = k + ai_rand_n(r, np - k);
                    if (crib_value(pool[x]) > v->low[o]) found = x;
                }
                if (found < 0)
                    for (int x = k; x < np && found < 0; x++)
                        if (crib_value(pool[x]) > v->low[o]) found = x;
                if (found < 0) { ok = 0; break; }
                uint8_t t = pool[k];
                pool[k] = pool[found];
                pool[found] = t;
                s->hand[o][i] = pool[k++];
            }
        }
        if (ok) return 1;
    }
    return 0;
}

static int pick_card(const cpos *p, const cview *v, int level, ai_rng *r)
{
    int me = v->me, np = p->players;
    peg_st s;
    memset(&s, 0, sizeof s);
    s.np = (uint8_t)np;
    s.turn = (uint8_t)me;
    s.last = p->last;
    s.count = p->count;
    s.nseq = p->nseq;
    memcpy(s.seq, p->seq, p->nseq);
    for (int o = 0; o < np; o++) s.n[o] = (uint8_t)(4 - __builtin_popcount(p->used[o]));
    uint8_t slot_of[4];
    int k = 0;
    for (int i = 0; i < 4; i++)
        if (!(p->used[me] >> i & 1)) slot_of[k] = (uint8_t)i, s.hand[me][k++] = p->kept[me][i];
    double val[4] = {0};
    int legal[4] = {0}, nl = 0;
    for (int i = 0; i < k; i++) legal[i] = p->count + crib_value(s.hand[me][i]) <= 31, nl += legal[i];
    if (nl <= 1 || level == AI_EASY) {
        int bi = -1;
        double bv = -1e9;
        for (int i = 0; i < k; i++) {
            if (!legal[i]) continue;
            peg_st t = s;
            peg_lay(&t, i);
            double w = t.pts[me] - (t.count == 5 || t.count == 21 ? 0.8 : 0) + ai_rand_f(r) * 0.9;
            if (level == AI_EASY && ai_rand_n(r, 100) < 15) w = ai_rand_f(r) * 4;
            if (w > bv) bv = w, bi = i;
        }
        return CA_PLAY + slot_of[bi];
    }
    int deals = level == AI_MEDIUM ? 6 : 64, done = 0;
    for (int d = 0; d < deals; d++) {
        if (ai_cancelled()) break;
        peg_st t = s;
        if (!deal_others(p, v, &t, r)) continue;
        done++;
        for (int i = 0; i < k; i++) {
            if (!legal[i]) continue;
            peg_st u = t;
            int more = peg_lay(&u, i), w[CRIB_MAXP] = {0};
            if (more) peg_search(&u, w);
            int others = 0;
            for (int o = 0; o < np; o++)
                if (o != me) others += u.pts[o] + w[o];
            val[i] += u.pts[me] + w[me] - (double)others / (np - 1);
        }
    }
    int bi = -1;
    double bv = -1e9;
    for (int i = 0; i < k; i++) {
        if (!legal[i]) continue;
        double w = done ? val[i] / done : 0;
        w -= crib_value(s.hand[me][i]) * 0.001;   /* ties: the lower card */
        if (w > bv) bv = w, bi = i;
    }
    return CA_PLAY + slot_of[bi];
}

/* the action for seat at a level (0: nothing to do) */
int cribb_choose(const cgame *g, int seat, int level, uint64_t seed)
{
    const cpos *p = &g->pos;
    if (p->phase == CP_SHOW) return CA_NEXT;
    if (p->phase == CP_OVER || p->turn != seat) return 0;
    if (p->phase == CP_CUT) return CA_CUT;
    cview *v = malloc(sizeof *v);
    if (!v) return crib_auto(p);
    view_of(g, seat, v);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 13 + (uint64_t)seat));
    int a = p->phase == CP_DISCARD ? pick_away(p, v, level, &r) : pick_card(p, v, level, &r);
    free(v);
    return crib_legal(p, a) ? a : crib_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    cgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *cribbage_live();                      /* the copy, at entry: nothing else is read */
    int a = cribb_choose(g, seat, level, ai_session_seed());
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    if (a == CA_NEXT) snprintf(move, (size_t)cap, "n");
    else if (a == CA_CUT) snprintf(move, (size_t)cap, "c");
    else if (a >= CA_PLAY) snprintf(move, (size_t)cap, "p%d", a - CA_PLAY);
    else {
        int k = 0;
        move[k++] = 'd';
        for (int i = 0; i < 6; i++)
            if (a >> i & 1) move[k++] = (char)('0' + i);
        move[k] = 0;
    }
    return 1;
}

static int actor(void)
{
    const cpos *p = &cribbage_live()->pos;
    return p->phase == CP_DISCARD || p->phase == CP_CUT || p->phase == CP_PLAY ? p->turn : -1;
}

static const bot cribbage_bot = {.think = think, .actor = actor, .play = cribbage_show};
REGISTER_BOT(cribbage, &cribbage_bot)
