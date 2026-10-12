/*
 * Durak: the rules (durak.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/durak.h"
#include "games/tricks.h"

/* ---- helpers ----------------------------------------------------------------------------------------------------- */
static int find(const dpos *p, int pl, int card)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] == card) return i;
    return -1;
}

static void take_out(dpos *p, int pl, int card)
{
    int i = find(p, pl, card);
    if (i < 0) return;
    memmove(&p->hand[pl][i], &p->hand[pl][i + 1], (size_t)(p->n[pl] - i - 1));
    p->n[pl]--;
}

static int is_out(const dpos *p, int pl)
{
    for (int i = 0; i < p->nout; i++)
        if (p->out[i] == pl) return 1;
    return 0;
}

int dur_active(const dpos *p, int pl) { return pl < p->players && !is_out(p, pl); }

int dur_next(const dpos *p, int pl)
{
    for (int i = 1; i <= p->players; i++) {
        int q = (pl + i) % p->players;
        if (dur_active(p, q)) return q;
    }
    return pl;
}

int dur_beats(const dpos *p, int card, int attack)
{
    if (CARD_SUIT(card) == CARD_SUIT(attack)) return dur_strength(card) > dur_strength(attack);
    return CARD_SUIT(card) == p->trump;
}

int dur_open(const dpos *p)
{
    int k = 0;
    for (int i = 0; i < p->natt; i++) k += p->def[i] == DUR_NONE;
    return k;
}

int dur_room(const dpos *p)
{
    int lim = p->start_n < DUR_FULL ? p->start_n : DUR_FULL;
    return lim > p->natt ? lim - p->natt : 0;
}

int dur_can_throw(const dpos *p, int card)
{
    for (int i = 0; i < p->natt; i++)
        if (CARD_RANK(p->att[i]) == CARD_RANK(card) || (p->def[i] != DUR_NONE && CARD_RANK(p->def[i]) == CARD_RANK(card)))
            return 1;
    return 0;
}

int dur_can_shift(const dpos *p, int card)
{
    if (p->variant != DUR_TRANSFER || p->phase != DP_DEFEND || p->taking || !p->natt) return 0;
    if (find(p, p->defender, card) < 0) return 0;
    for (int i = 0; i < p->natt; i++)
        if (p->def[i] != DUR_NONE || CARD_RANK(p->att[i]) != CARD_RANK(card)) return 0;
    int nd = dur_next(p, p->defender);
    return nd != p->defender && p->natt + 1 <= DUR_FULL && p->natt + 1 <= p->n[nd];
}

/* a thrower: in the game, not the defender, holding cards */
static int thrower(const dpos *p, int q) { return q != p->defender && dur_active(p, q) && p->n[q] > 0; }

/* the next thrower in turn from `from` (itself first when self) who has not had their turn in this round; -1: none */
static int next_thrower(const dpos *p, int from, int self)
{
    for (int i = self ? 0 : 1; i <= p->players; i++) {
        int q = (from + i) % p->players;
        if (thrower(p, q) && !(p->passed >> q & 1)) return q;
    }
    return -1;
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
void dur_sort(const dpos *p, uint8_t *c, int n)
{
    for (int i = 1; i < n; i++) {
        uint8_t x = c[i];
        int kx = (CARD_SUIT(x) == p->trump ? 4 : trick_suit_order(CARD_SUIT(x))) * 16 + dur_strength(x), j = i;
        while (j > 0) {
            int y = c[j - 1], ky = (CARD_SUIT(y) == p->trump ? 4 : trick_suit_order(CARD_SUIT(y))) * 16 + dur_strength(y);
            if (ky <= kx) break;
            c[j] = c[j - 1];
            j--;
        }
        c[j] = x;
    }
}

void dur_new(dgame *g, int variant, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    dpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > DUR_MAXP ? DUR_MAXP : players);
    g->variant = p->variant = (uint8_t)(variant == DUR_TRANSFER ? DUR_TRANSFER : DUR_CLASSIC);
    uint8_t pack[DUR_CARDS];
    for (int i = 0; i < DUR_CARDS; i++) pack[i] = (uint8_t)dur_card(i);
    cards_shuffle(pack, DUR_CARDS, seed);
    int k = 0;
    for (int r = 0; r < DUR_FULL; r++)
        for (int q = 0; q < p->players; q++) p->hand[q][p->n[q]++] = pack[k++];
    p->ntalon = (uint8_t)(DUR_CARDS - k);
    for (int i = 0; i < p->ntalon; i++) p->talon[i] = pack[DUR_CARDS - 1 - i];
    p->trump_card = pack[DUR_CARDS - 1];
    p->trump = (uint8_t)CARD_SUIT(p->trump_card);
    /* the lowest trump attacks first */
    int first = 0, low = 99;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->n[q]; i++)
            if (CARD_SUIT(p->hand[q][i]) == p->trump && dur_strength(p->hand[q][i]) < low)
                low = dur_strength(p->hand[q][i]), first = q;
    p->attacker = (uint8_t)first;
    p->defender = (uint8_t)dur_next(p, first);
    p->start_n = p->n[p->defender];
    p->turn = p->attacker;
    p->phase = DP_ATTACK;
    p->fool = DUR_NONE;
    p->last_by = DUR_NONE;
}

/* ---- the bout ---------------------------------------------------------------------------------------------------- */
static void draw_up(dpos *p, int q)
{
    while (p->n[q] < DUR_FULL && p->ntalon > 0) p->hand[q][p->n[q]++] = p->talon[--p->ntalon];
}

static void bout_end(dpos *p)
{
    int a = p->attacker, d = p->defender, took = p->taking;
    if (took) {
        for (int i = 0; i < p->natt; i++) {
            p->hand[d][p->n[d]++] = p->att[i];
            if (p->def[i] != DUR_NONE) p->hand[d][p->n[d]++] = p->def[i];
        }
    } else {
        for (int i = 0; i < p->natt; i++) p->discards = (uint8_t)(p->discards + 1 + (p->def[i] != DUR_NONE));
    }
    p->last_end = (uint8_t)(took ? 2 : 1);
    p->last_by = (uint8_t)d;
    p->natt = 0;
    p->taking = 0;
    p->passed = 0;
    p->bouts++;
    /* draw back up: the attacker first, the others in turn, the defender last */
    for (int i = 0; i < p->players; i++) {
        int q = (a + i) % p->players;
        if (q != d && dur_active(p, q)) draw_up(p, q);
    }
    draw_up(p, d);
    if (!p->ntalon)                             /* out: no cards and none to draw (in the order of the draw) */
        for (int i = 0; i < p->players; i++) {
            int q = (a + i) % p->players;
            if (q == d) continue;
            if (dur_active(p, q) && !p->n[q]) p->out[p->nout++] = (uint8_t)q;
        }
    if (!p->ntalon && dur_active(p, d) && !p->n[d]) p->out[p->nout++] = (uint8_t)d;
    int left = 0, last = -1;
    for (int q = 0; q < p->players; q++)
        if (dur_active(p, q)) left++, last = q;
    if (left <= 1) {
        p->fool = (uint8_t)(left == 1 ? last : DUR_NONE);
        p->phase = DP_OVER;
        return;
    }
    /* beaten off: the defender attacks next; taken: the player after them */
    int na = took ? dur_next(p, d) : dur_active(p, d) ? d : dur_next(p, d);
    p->attacker = (uint8_t)na;
    p->defender = (uint8_t)dur_next(p, na);
    p->start_n = p->n[p->defender];
    p->phase = DP_ATTACK;
    p->turn = (uint8_t)na;
}

/* a round of throwers from the attacker (or the bout's end: no room, nobody to throw) */
static void throwers_round(dpos *p)
{
    p->passed = 0;
    int q = dur_room(p) ? next_thrower(p, p->attacker, 1) : -1;
    if (q < 0) { bout_end(p); return; }
    p->phase = DP_THROW;
    p->turn = (uint8_t)q;
}

/* the thrower has had their turn: the next one, or the bout's end */
static void throwers_next(dpos *p)
{
    p->passed |= (uint8_t)(1u << p->turn);
    int q = dur_room(p) ? next_thrower(p, p->turn, 0) : -1;
    if (q < 0) bout_end(p);
    else p->turn = (uint8_t)q;
}

static int play_ok(const dpos *p, uint64_t m)
{
    int pl = p->turn, k = 0, rank = -1;
    if (!m || m >> DUR_CARDS) return 0;
    for (int i = 0; i < DUR_CARDS; i++) {
        if (!(m >> i & 1)) continue;
        int c = dur_card(i);
        if (find(p, pl, c) < 0) return 0;
        if (p->phase == DP_ATTACK) {
            if (rank >= 0 && CARD_RANK(c) != rank) return 0;
            rank = CARD_RANK(c);
        } else if (!dur_can_throw(p, c)) return 0;
        k++;
    }
    return k <= dur_room(p);
}

int dur_legal(const dpos *p, uint64_t a)
{
    switch (p->phase) {
    case DP_ATTACK: return DA_KIND(a) == DA_PLAY && play_ok(p, DA_MASK(a));
    case DP_THROW:
        if (a == DA_PASS) return 1;
        return DA_KIND(a) == DA_PLAY && play_ok(p, DA_MASK(a));
    case DP_DEFEND:
        if (a == DA_TAKE) return !p->taking && dur_open(p) > 0;
        if (DA_KIND(a) == DA_SHIFT) return (a >> 8 & 0xffffffffffull) == 0 && dur_can_shift(p, DA_CARD(a));
        if (DA_KIND(a) == DA_BEAT) {
            int c = DA_CARD(a), s = DA_SLOT(a);
            if ((a >> 16 & 0xffffffffffull) || s >= p->natt || p->def[s] != DUR_NONE) return 0;
            return find(p, p->defender, c) >= 0 && dur_beats(p, c, p->att[s]);
        }
        return 0;
    default: return 0;
    }
}

static void apply(dpos *p, uint64_t a)
{
    int pl = p->turn;
    switch (DA_KIND(a)) {
    case DA_PLAY:
        for (int i = 0; i < DUR_CARDS; i++)
            if (DA_MASK(a) >> i & 1) {
                int c = dur_card(i);
                take_out(p, pl, c);
                p->att[p->natt] = (uint8_t)c;
                p->def[p->natt++] = DUR_NONE;
            }
        if (p->phase == DP_THROW && p->taking) { throwers_next(p); break; }
        p->phase = DP_DEFEND;
        p->turn = p->defender;
        break;
    case DA_BEAT:
        take_out(p, pl, DA_CARD(a));
        p->def[DA_SLOT(a)] = (uint8_t)DA_CARD(a);
        if (!dur_open(p)) throwers_round(p);
        break;
    case DA_TAKE:
        p->taking = 1;
        throwers_round(p);
        break;
    case DA_SHIFT: {
        int c = DA_CARD(a), nd = dur_next(p, p->defender);
        take_out(p, pl, c);
        p->att[p->natt] = (uint8_t)c;
        p->def[p->natt++] = DUR_NONE;
        p->attacker = p->defender;
        p->defender = (uint8_t)nd;
        p->start_n = p->n[nd];
        p->turn = (uint8_t)nd;
        break;
    }
    case DA_PASS: throwers_next(p); break;
    }
}

int dur_play(dgame *g, uint64_t a)
{
    if (g->n >= DUR_MAX_ACTS || !dur_legal(&g->pos, a)) return 0;
    apply(&g->pos, a);
    g->act[g->n++] = a;
    return 1;
}

int dur_undo(dgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint64_t acts[DUR_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    dur_new(g, g->variant, g->players, g->seed);
    for (int i = 0; i < n; i++) dur_play(g, acts[i]);
    return 1;
}

unsigned dur_winners(const dpos *p)
{
    if (p->phase != DP_OVER) return 0;
    unsigned all = (1u << p->players) - 1;
    return p->fool == DUR_NONE ? all : all & ~(1u << p->fool);
}

/* ---- plain play -------------------------------------------------------------------------------------------------- */
/* a card's cost to give away: the trumps last */
static int cost(const dpos *p, int c) { return (CARD_SUIT(c) == p->trump) * 20 + dur_strength(c); }

uint64_t dur_auto(const dpos *p)
{
    int pl = p->turn, best = -1;
    switch (p->phase) {
    case DP_ATTACK:
        for (int i = 0; i < p->n[pl]; i++)
            if (best < 0 || cost(p, p->hand[pl][i]) < cost(p, best)) best = p->hand[pl][i];
        return DA_PLAY | 1ull << dur_index(best);
    case DP_DEFEND:
        if (p->taking) return 0;
        for (int s = 0; s < p->natt; s++) {
            if (p->def[s] != DUR_NONE) continue;
            for (int i = 0; i < p->n[pl]; i++) {
                int c = p->hand[pl][i];
                if (dur_beats(p, c, p->att[s]) && (best < 0 || cost(p, c) < cost(p, best))) best = c;
            }
            return best < 0 ? DA_TAKE : DA_BEATS(best, s);
        }
        return 0;
    case DP_THROW:
        if (p->taking || !dur_room(p)) return DA_PASS;
        for (int i = 0; i < p->n[pl]; i++) {
            int c = p->hand[pl][i];
            if (CARD_SUIT(c) != p->trump && dur_can_throw(p, c) && (best < 0 || cost(p, c) < cost(p, best))) best = c;
        }
        return best < 0 ? DA_PASS : DA_PLAY | 1ull << dur_index(best);
    default: return 0;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int dur_script(dgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        uint64_t a = 0;
        if (*s == '#') {
            s++;
            while (g->pos.phase != DP_OVER)
                if (!dur_play(g, dur_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') s++, a = dur_auto(&g->pos);
        else if (*s == 't') s++, a = DA_TAKE;
        else if (*s == 'p') s++, a = DA_PASS;
        else if (*s == 'x') {
            s++;
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            a = DA_SHIFT | (uint64_t)c;
        } else {
            uint64_t m = 0;
            int k = 0, first = CARD_NONE;
            for (int c; (c = trick_parse(&s)) != CARD_NONE; k++) {
                if (CARD_RANK(c) >= 2 && CARD_RANK(c) <= 5) return 0;
                if (!k) first = c;
                m |= 1ull << dur_index(c);
            }
            if (!k) return 0;
            const dpos *p = &g->pos;
            if (p->phase == DP_DEFEND) {        /* a defence: the first open card it beats, or the one named */
                if (k != 1) return 0;
                int slot = -1;
                if (*s == '/') {
                    s++;
                    int t = trick_parse(&s);
                    for (int i = 0; i < p->natt; i++)
                        if (p->att[i] == t) slot = i;
                } else
                    for (int i = p->natt - 1; i >= 0; i--)
                        if (p->def[i] == DUR_NONE && dur_beats(p, first, p->att[i])) slot = i;
                if (slot < 0) return 0;
                a = DA_BEATS(first, slot);
            } else a = DA_PLAY | m;
        }
        if (!a || !dur_play(g, a)) return 0;
    }
    return 1;
}
