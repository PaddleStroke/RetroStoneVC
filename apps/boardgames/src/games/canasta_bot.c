/*
 * Canasta's bot (bots.h), for two or four players: a player of rules and plans, from what its seat may see: its own
 * hand, every meld and red 3 on the table, the discard pile (each card was seen going there) and how many cards
 * each player holds. Each turn is several actions (draw or take, melds, a discard): each think() plans the turn
 * from the position as it is and gives its next action; a plan (the initial meld, the melds laid to take the pile,
 * going out) is tried in full on a copy of the position first, so the bot never starts what it cannot finish.
 * Runs on the worker thread: it copies the game at entry and reads only that copy. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   The pile: taken whenever the rules allow it without a wild card (with one only for a pile worth it); before the
 *   side's first meld, the bot lays the melds that bring the take to the initial count first.
 *   Going out: as soon as every card but one can be melded and the side holds its canastas (the canasta may be made
 *   on the way, with a wild card).
 *   Melds: the initial meld once a plan of melds reaches its count (natural sets first, a pair with a wild card
 *   then); after it, every natural on the side's melds and every natural set of three; a wild card only to make a
 *   canasta (or, the other side close to going out, wherever it fits). Pairs stay in hand: they take a frozen pile.
 *   The discard: a black 3 (it blocks the pile); else a single before a pair, a card of no meld of the side, the
 *   cheaper first; four players: the card the next player is least likely to take the pile with (a rank they have
 *   melded, with the pile open to them, the worse the bigger the pile; a rank much seen is safer), and a big pile
 *   left open to them gets frozen with a wild card. (For two, that care lost more games than it saved in self-play:
 *   the cards held back clog the hand.)
 * Levels: easy plays the rules' plain actions (they take the pile when a meld allows, meld every natural set, throw
 * the loneliest card), with a careless discard now and then; medium plays the plans above with a looser discard,
 * without the freezing, the melds laid to take the pile or the rush when the other side is about to go out; hard
 * plays them all. Going out with wild cards is most of the strength: every level does it (easy as the plain melds
 * happen to allow).
 * Its moves are the game's notation ("s", "t", "tKh.Kd", "mKh.Kd.Ks", "m8:2c", "d7c", "n"), played by canasta_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/canasta.h"

static const int ORDER[11] = {1, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4};    /* the meldable ranks, the dearest first */

typedef struct {                                /* a hand by ranks: naturals, wild cards, black 3s */
    uint8_t nat[CA_RANKS][CA_ACT], wild[CA_ACT], b3[8];
    int nn[CA_RANKS], nw, nb3;
} hsort;

static void sort_hand(const uint8_t *h, int n, hsort *s)
{
    memset(s, 0, sizeof *s);
    for (int i = 0; i < n; i++) {
        int c = h[i];
        if (ca_wild(c)) { if (s->nw < CA_ACT) s->wild[s->nw++] = (uint8_t)c; }
        else if (ca_black3(c)) { if (s->nb3 < 8) s->b3[s->nb3++] = (uint8_t)c; }
        else if (!ca_red3(c) && s->nn[CARD_RANK(c)] < CA_ACT) s->nat[CARD_RANK(c)][s->nn[CARD_RANK(c)]++] = (uint8_t)c;
    }
    /* the cheapest wild cards first (the 2s before the jokers) */
    for (int i = 1; i < s->nw; i++)
        for (int j = i; j > 0 && ca_value(s->wild[j]) < ca_value(s->wild[j - 1]); j--) {
            uint8_t t = s->wild[j];
            s->wild[j] = s->wild[j - 1];
            s->wild[j - 1] = t;
        }
}

static ca_act mk(int kind, int rank, const uint8_t *c, int n)
{
    ca_act a;
    memset(&a, 0, sizeof a);
    a.kind = (uint8_t)kind;
    a.rank = (uint8_t)rank;
    a.n = (uint8_t)(n > CA_ACT ? CA_ACT : n);
    if (n) memcpy(a.card, c, a.n);
    return a;
}

static int wilds_on(const capos *p, int s, int r)
{
    int w = 0;
    for (int i = 0; i < p->mn[s][r]; i++) w += ca_wild(p->meld[s][r][i]);
    return w;
}

/* a scratch game to try a plan on (the position copied; its actions list unused) */
typedef struct { cagame *g; } scratch;

static int try_seq(cagame *g, const capos *p, const ca_act *seq, int n)
{
    g->pos = *p;
    g->n = 0;
    for (int i = 0; i < n; i++)
        if (!ca_play(g, &seq[i])) return 0;
    return 1;
}

/* ---- the turn's plans -------------------------------------------------------------------------------------------- */
/* melds from the hand reaching `need` points (naturals on the side's melds laid this turn, natural sets, then pairs
 * with a wild card, then wild cards added), without the cards of rank `skip`, keeping `keep` cards in hand; into
 * seq; how many (0: none reaches it) */
static int initial_plan(const capos *p, int pl, int need, int skip, int keep, ca_act *seq)
{
    int s = ca_side(p, pl);
    hsort h;
    sort_hand(p->hand[pl], p->n[pl], &h);
    int v = 0, k = 0, avail = p->n[pl] - keep, wu = 0;
    int slot[CA_RANKS], wild[CA_RANKS];
    for (int r = 0; r < CA_RANKS; r++) slot[r] = -1, wild[r] = p->mn[s][r] ? wilds_on(p, s, r) : 3;
    for (int i = 0; i < 11 && v < need; i++) {
        int r = ORDER[i];
        if (r == skip || !h.nn[r] || avail < h.nn[r]) continue;
        if (p->mn[s][r]) slot[r] = k, seq[k++] = mk(CA_MELD_IT, r, h.nat[r], h.nn[r]);
        else if (h.nn[r] >= 3) slot[r] = k, seq[k++] = mk(CA_MELD_IT, 0, h.nat[r], h.nn[r]), wild[r] = 0;
        else continue;
        v += h.nn[r] * ca_value(h.nat[r][0]);
        avail -= h.nn[r];
    }
    for (int i = 0; i < 11 && v < need; i++) {
        int r = ORDER[i];
        if (r == skip || h.nn[r] != 2 || p->mn[s][r] || wu >= h.nw || avail < 3) continue;
        uint8_t c[3] = {h.nat[r][0], h.nat[r][1], h.wild[wu++]};
        slot[r] = k;
        seq[k++] = mk(CA_MELD_IT, 0, c, 3);
        v += 2 * ca_value(c[0]) + ca_value(c[2]);
        avail -= 3;
        wild[r] = 1;
    }
    for (int i = 0; i < 11 && v < need && wu < h.nw && avail > 0; i++) {   /* wild cards on the sets */
        int r = ORDER[i];
        if (r == skip) continue;
        while (wild[r] < 3 && wu < h.nw && avail > 0 && v < need) {
            if (slot[r] < 0) slot[r] = k, seq[k++] = mk(CA_MELD_IT, r, NULL, 0);
            ca_act *a = &seq[slot[r]];
            if (a->n >= CA_ACT) break;
            a->card[a->n++] = h.wild[wu];
            v += ca_value(h.wild[wu++]);
            wild[r]++;
            avail--;
        }
    }
    return v >= need ? k : 0;
}

/* every card but one melded (onto the side's melds, new sets, wild cards where they fit, black 3s): the sequence of
 * melds; 1 when it empties the hand to one card or none with the canastas going out asks */
static int out_plan(const capos *p, int pl, ca_act *seq, int *nseq)
{
    int s = ca_side(p, pl);
    hsort h;
    sort_hand(p->hand[pl], p->n[pl], &h);
    int k = 0, size[CA_RANKS], wild[CA_RANKS], left = 0;
    for (int r = 0; r < CA_RANKS; r++) size[r] = p->mn[s][r], wild[r] = wilds_on(p, s, r);
    int slot[CA_RANKS];
    for (int r = 0; r < CA_RANKS; r++) slot[r] = -1;
    for (int i = 0; i < 11; i++) {
        int r = ORDER[i];
        if (!h.nn[r]) continue;
        if (size[r] || h.nn[r] >= 3) {
            slot[r] = k;
            seq[k++] = mk(CA_MELD_IT, size[r] ? r : 0, h.nat[r], h.nn[r]);
            size[r] += h.nn[r];
        } else left += h.nn[r];
    }
    int wu = 0;
    /* wild cards: first a pair of naturals with one (a new set), then onto the sets, the canastas nearest first */
    for (int i = 0; i < 11 && wu < h.nw; i++) {
        int r = ORDER[i];
        if (size[r] || h.nn[r] != 2) continue;
        uint8_t c[3] = {h.nat[r][0], h.nat[r][1], h.wild[wu++]};
        slot[r] = k;
        seq[k++] = mk(CA_MELD_IT, 0, c, 3);
        size[r] = 3;
        wild[r] = 1;
        left -= 2;
    }
    while (wu < h.nw) {
        int best = -1;
        for (int i = 0; i < 11; i++) {
            int r = ORDER[i];
            if (size[r] && wild[r] < 3 && size[r] < CA_MELD && (best < 0 || (size[r] < 7 && (size[best] >= 7 ||
                                                                                          size[r] > size[best]))))
                best = r;
        }
        if (best < 0) break;
        uint8_t c = h.wild[wu++];
        if (slot[best] >= 0 && seq[slot[best]].n < CA_ACT) seq[slot[best]].card[seq[slot[best]].n++] = c;
        else {
            slot[best] = k;
            seq[k++] = mk(CA_MELD_IT, best, &c, 1);
        }
        size[best]++;
        wild[best]++;
    }
    left += h.nw - wu;
    if (h.nb3 >= 3) seq[k++] = mk(CA_MELD_IT, 3, h.b3, h.nb3);
    else left += h.nb3;
    int canastas = 0;
    for (int r = 0; r < CA_RANKS; r++) canastas += size[r] >= 7;
    *nseq = k;
    return left <= 1 && canastas >= (p->players == 2 ? 2 : 1);
}

/* ---- the discard ------------------------------------------------------------------------------------------------- */
/* how many of each rank this seat has seen (its hand, every meld, the pile) */
static void seen_ranks(const capos *p, int pl, int *seen)
{
    memset(seen, 0, sizeof(int) * 16);
    for (int i = 0; i < p->n[pl]; i++) seen[CARD_RANK(p->hand[pl][i]) & 15]++;
    for (int s = 0; s < 2; s++)
        for (int r = 0; r < CA_RANKS; r++)
            for (int i = 0; i < p->mn[s][r]; i++)
                if (!ca_wild(p->meld[s][r][i])) seen[r]++;
    for (int i = 0; i < p->npile; i++) seen[CARD_RANK(p->pile[i]) & 15]++;
}

/* the cost of throwing card c (lower is better): the danger the next player takes the pile with it, and what it
 * is worth keeping */
static int discard_cost(const capos *p, int pl, int c, const hsort *h, const int *seen, int careful)
{
    if (ca_black3(c)) return -1000;
    if (ca_wild(c)) return 100000 + ca_value(c);
    int r = CARD_RANK(c), nxt = (pl + 1) % p->players, o = ca_side(p, nxt), cost = 0;
    int pile = p->npile + 1;
    if (careful) {
        int open = !ca_frozen_for(p, o) && !(p->frozen);
        if (p->mn[o][r] && open) cost += 300 * pile;
        else if (open) cost += (8 - seen[r] > 1 ? 400 : 60) + 10 * pile;
        else cost += (8 - seen[r] > 1 ? 120 : 10) + 3 * pile;
        if (p->players == 4 && p->mn[o][r]) cost += 100;   /* the other opponent too, later */
    }
    cost += 700 * (h->nn[r] - 1);               /* a pair (or more) kept: it takes the pile, it melds */
    int s = ca_side(p, pl);
    if (p->mn[s][r]) cost += 250;               /* it goes on the side's meld */
    cost += ca_value(c) * 3;
    return cost;
}

static int pick_discard(const capos *p, int pl, int level, ai_rng *r, ca_act *out)
{
    hsort h;
    sort_hand(p->hand[pl], p->n[pl], &h);
    int seen[16];
    seen_ranks(p, pl, seen);
    int best = 1 << 30, bc = -1;
    for (int i = 0; i < p->n[pl]; i++) {
        int c = p->hand[pl][i];
        int careful = p->players == 4;           /* (two players: the danger cost more than it saved) */
        int cost = discard_cost(p, pl, c, &h, seen, careful);
        if (level == AI_MEDIUM) cost += ai_rand_n(r, 800);
        uint8_t one = (uint8_t)c;
        ca_act a = mk(CA_DISCARD, 0, &one, 1);
        if (cost < best && ca_legal(p, &a)) best = cost, bc = c;
    }
    /* freeze a big pile left open to the next player, when every natural would feed it */
    if (level == AI_HARD && bc >= 0 && !ca_wild(bc) && !p->frozen && p->npile >= 8 && best >= 300 * (p->npile + 1) &&
        h.nw) {
        uint8_t one = h.wild[0];
        ca_act a = mk(CA_DISCARD, 0, &one, 1);
        if (ca_legal(p, &a)) bc = one;
    }
    if (bc < 0) return 0;
    uint8_t one = (uint8_t)bc;
    *out = mk(CA_DISCARD, 0, &one, 1);
    return 1;
}

/* ---- a turn's action --------------------------------------------------------------------------------------------- */
/* CP_DRAW: take the pile (maybe after the melds the take needs), else the stock */
static int draw_action(cagame *sc, const capos *p, int pl, int level, ca_act *out)
{
    int s = ca_side(p, pl);
    if (!p->npile) goto stock;
    {
        int top = p->pile[p->npile - 1], r = CARD_RANK(top);
        hsort h;
        sort_hand(p->hand[pl], p->n[pl], &h);
        ca_act tries[6];
        int nt = 0;
        tries[nt++] = mk(CA_TAKE, 0, NULL, 0);
        if (h.nn[r] >= 2) tries[nt++] = mk(CA_TAKE, 0, h.nat[r], 2);
        if (h.nn[r] > 2) tries[nt++] = mk(CA_TAKE, 0, h.nat[r], h.nn[r]);
        int wild_ok = p->npile >= 5 || ca_forced(p);
        if (h.nn[r] >= 1 && h.nw && wild_ok) {
            uint8_t c[2] = {h.nat[r][0], h.wild[0]};
            tries[nt++] = mk(CA_TAKE, 0, c, 2);
        }
        if (h.nn[r] >= 2 && h.nw && wild_ok) {
            uint8_t c[3] = {h.nat[r][0], h.nat[r][1], h.wild[0]};
            tries[nt++] = mk(CA_TAKE, 0, c, 3);
        }
        for (int i = 0; i < nt; i++)
            if (ca_legal(p, &tries[i])) { *out = tries[i]; return 1; }
        /* hard, the side not open yet: the melds that bring the take to the count, laid first */
        if (level == AI_HARD && !p->opened[s] && p->npile >= 3 && h.nn[r] >= 2 && !ca_wild(top) && !ca_black3(top)) {
            int need = ca_need(p, s) - ca_meld_points(p, s) - ca_value(top) - h.nn[r] * ca_value(top);
            ca_act seq[16];
            int k = need > 0 ? initial_plan(p, pl, need, r, 2 + h.nn[r], seq) : 0;
            if (k) {
                seq[k] = mk(CA_TAKE, 0, h.nat[r], h.nn[r]);
                if (try_seq(sc, p, seq, k + 1)) { *out = seq[0]; return 1; }
            }
        }
    }
stock:
    *out = mk(CA_STOCK, 0, NULL, 0);
    return ca_legal(p, out);
}

/* CP_MELD: going out, the initial meld, the melds, then the discard */
static int meld_action(cagame *sc, const capos *p, int pl, int level, ai_rng *r, ca_act *out)
{
    int s = ca_side(p, pl);
    ca_act seq[24];
    int k;
    /* going out: the whole plan tried, then a discard if a card is left */
    if (out_plan(p, pl, seq, &k) && k && try_seq(sc, p, seq, k)) {
        const capos *q = &sc->pos;
        if (q->phase == CP_HAND || q->phase == CP_OVER || (q->n[pl] == 1 && pick_discard(q, pl, level, r, &seq[k]) &&
                                                         ca_legal(q, &seq[k]))) {
            *out = seq[0];
            return 1;
        }
    }
    hsort h;
    sort_hand(p->hand[pl], p->n[pl], &h);
    if (!p->opened[s]) {
        int need = ca_need(p, s) - ca_meld_points(p, s);
        if (need > 0) {
            k = initial_plan(p, pl, need, -1, 1, seq);
            if (k && try_seq(sc, p, seq, k) && pick_discard(&sc->pos, pl, level, r, &seq[k])) {
                *out = seq[0];
                return 1;
            }
            if (pick_discard(p, pl, level, r, out)) return 1;
            /* melds laid short of the count (should not happen): the dearest meld the rules allow, until it is */
            int best = 0;
            for (int rk = 1; rk < CA_RANKS; rk++)
                for (int w = 0; w <= 3 && w <= h.nw; w++) {
                    uint8_t c[CA_ACT];
                    int n = 0, v = 0;
                    for (int i = 0; i < h.nn[rk] && n < CA_ACT; i++) c[n++] = h.nat[rk][i], v += ca_value(h.nat[rk][i]);
                    for (int i = 0; i < w && n < CA_ACT; i++) c[n++] = h.wild[i], v += ca_value(h.wild[i]);
                    ca_act a = mk(CA_MELD_IT, p->mn[s][rk] ? rk : 0, c, n);
                    if (n && v > best && ca_legal(p, &a)) best = v, *out = a;
                }
            return best > 0;
        }
    }
    /* the other side close to going out: meld what can be melded, wild cards too */
    int hurry = 0;
    for (int q = 0; q < p->players; q++)
        if (ca_side(p, q) != s && p->n[q] <= 3 && ca_can_out(p, ca_side(p, q))) hurry = 1;
    for (int i = 0; i < 11; i++) {              /* naturals on the side's melds */
        int rk = ORDER[i];
        if (!h.nn[rk] || !p->mn[s][rk]) continue;
        ca_act a = mk(CA_MELD_IT, rk, h.nat[rk], h.nn[rk]);
        if (ca_legal(p, &a)) { *out = a; return 1; }
        if (h.nn[rk] > 1) {
            a = mk(CA_MELD_IT, rk, h.nat[rk], h.nn[rk] - 1);
            if (ca_legal(p, &a)) { *out = a; return 1; }
        }
    }
    for (int i = 0; i < 11; i++) {              /* natural sets */
        int rk = ORDER[i];
        if (h.nn[rk] < 3 || p->mn[s][rk]) continue;
        ca_act a = mk(CA_MELD_IT, 0, h.nat[rk], h.nn[rk]);
        if (ca_legal(p, &a)) { *out = a; return 1; }
    }
    for (int i = 0; i < 11 && h.nw; i++) {      /* a wild card that makes a canasta (or any, in a hurry) */
        int rk = ORDER[i], m = p->mn[s][rk];
        if (!m || m >= 7 || wilds_on(p, s, rk) >= 3) continue;
        if (m == 6 || (hurry && level == AI_HARD)) {
            ca_act a = mk(CA_MELD_IT, rk, h.wild, 1);
            if (ca_legal(p, &a)) { *out = a; return 1; }
        }
    }
    if (hurry && level == AI_HARD)
        for (int i = 0; i < 11 && h.nw; i++) {  /* a pair with a wild card */
            int rk = ORDER[i];
            if (h.nn[rk] != 2 || p->mn[s][rk]) continue;
            uint8_t c[3] = {h.nat[rk][0], h.nat[rk][1], h.wild[0]};
            ca_act a = mk(CA_MELD_IT, 0, c, 3);
            if (ca_legal(p, &a)) { *out = a; return 1; }
        }
    return pick_discard(p, pl, level, r, out);
}

/* the action for seat at a level (0: nothing to do) */
int canb_choose(const cagame *g, int seat, int level, uint64_t seed, ca_act *out)
{
    const capos *p = &g->pos;
    memset(out, 0, sizeof *out);
    if (p->phase == CP_HAND) { out->kind = CA_NEXT; return 1; }
    if (p->phase == CP_OVER || p->turn != seat) return 0;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 17 + (uint64_t)seat));
    if (level == AI_EASY) {                     /* the plain actions, a careless discard now and then */
        if (p->phase == CP_MELD && ai_rand_n(&r, 100) < 25) {
            ca_act a;
            if (ca_auto(p, &a) && a.kind == CA_DISCARD) {
                int i = ai_rand_n(&r, p->n[seat]), c = p->hand[seat][i];
                if (!ca_wild(c)) {
                    uint8_t one = (uint8_t)c;
                    ca_act b = mk(CA_DISCARD, 0, &one, 1);
                    if (ca_legal(p, &b)) { *out = b; return 1; }
                }
            }
        }
        return ca_auto(p, out);
    }
    cagame *sc = malloc(sizeof *sc);
    int ok = 0;
    if (sc) {
        sc->seed = g->seed;
        sc->players = g->players;
        ok = p->phase == CP_DRAW ? draw_action(sc, p, seat, level, out) : meld_action(sc, p, seat, level, &r, out);
        free(sc);
    }
    if (!ok || !ca_legal(p, out)) ok = ca_auto(p, out);
    return ok;
}

static int card_text(int c, char *o)
{
    if (CARD_SUIT(c) == SUIT_JOKER) { o[0] = 'X'; return 1; }
    o[0] = "A23456789TJQK"[CARD_RANK(c) - 1];
    o[1] = "shdc"[CARD_SUIT(c)];
    return 2;
}

/* an action in the notation (cap >= 64) */
void canb_name(const ca_act *a, char *m)
{
    int k = 0;
    m[k++] = "?stmdn"[a->kind <= CA_NEXT ? a->kind : 0];
    if (a->kind == CA_MELD_IT && a->rank) {
        m[k++] = "?A23456789TJQK"[a->rank];
        m[k++] = ':';
    }
    for (int i = 0; i < a->n; i++) {
        if (i) m[k++] = '.';
        k += card_text(a->card[i], m + k);
    }
    m[k] = 0;
}

static int think(int seat, int level, char *move, int cap)
{
    cagame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *canasta_live();                       /* the copy, at entry: nothing else is read */
    ca_act a;
    int ok = canb_choose(g, seat, level, ai_session_seed(), &a);
    free(g);
    if (!ok || ai_cancelled() || cap < 64) return 0;
    canb_name(&a, move);
    return 1;
}

static int actor(void)
{
    const capos *p = &canasta_live()->pos;
    return p->phase == CP_DRAW || p->phase == CP_MELD ? p->turn : -1;
}

static const bot canasta_bot = {.think = think, .actor = actor, .play = canasta_show};
REGISTER_BOT(canasta, &canasta_bot)
