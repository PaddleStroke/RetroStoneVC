/*
 * Président: the rules (president.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/president.h"

int pres_active(const ppos *p, int pl) { return p->n[pl] > 0 && pres_place(p, pl) < 0; }

int pres_place(const ppos *p, int pl)
{
    for (int i = 0; i < p->nout; i++)
        if (p->out[i] == pl) return i;
    return -1;
}

int pres_points(int players, int place) { return players - 1 - place; }

static int count_active(const ppos *p)
{
    int k = 0;
    for (int q = 0; q < p->players; q++) k += pres_active(p, q);
    return k;
}

static int next_active(const ppos *p, int pl)
{
    for (int i = 1; i <= p->players; i++) {
        int q = (pl + i) % p->players;
        if (pres_active(p, q)) return q;
    }
    return pl;
}

static int find(const ppos *p, int pl, int card)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] == card) return i;
    return -1;
}

static void take_out(ppos *p, int pl, int card)
{
    int i = find(p, pl, card);
    if (i < 0) return;
    memmove(&p->hand[pl][i], &p->hand[pl][i + 1], (size_t)(p->n[pl] - i - 1));
    p->n[pl]--;
}

int pres_best(const ppos *p, int pl)
{
    int best = 0;
    for (int i = 1; i < p->n[pl]; i++)
        if (pres_strength(CARD_RANK(p->hand[pl][i])) > pres_strength(CARD_RANK(p->hand[pl][best]))) best = i;
    return best;
}

static int popcount4(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) + (m >> 3 & 1); }

int pres_legal(const ppos *p, int act)
{
    int k = PA_KIND(act);
    switch (p->phase) {
    case PP_GIVE: return k == PA_GIVE && !(act & 0xf00) && find(p, p->turn, act & 0xff) >= 0;
    case PP_PLAY:
        if (act == PA_PASS) return p->top_count > 0;
        if (k != PA_PLAY) return 0;
        {
            int r = PA_RANK(act), m = PA_MASK(act), c = popcount4(m);
            if (r < 1 || r > 13 || !m) return 0;
            for (int s = 0; s < 4; s++)
                if (m >> s & 1 && find(p, p->turn, CARD(s, r)) < 0) return 0;
            if (!p->top_count) return 1;
            return c == p->top_count && pres_strength(r) > pres_strength(p->top_rank);
        }
    case PP_DEAL: return act == PA_NEXT;
    default: return 0;
    }
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static int who(const ppos *p, int role)
{
    for (int q = 0; q < p->players; q++)
        if (p->role[q] == role) return q;
    return -1;
}

/* the receiver gets the giver's best card */
static void give_best(ppos *p, int from, int to)
{
    int c = p->hand[from][pres_best(p, from)];
    take_out(p, from, c);
    p->hand[to][p->n[to]++] = (uint8_t)c;
    p->got[to][p->ngot[to]++] = (uint8_t)c;
}

static void clear_trick(ppos *p)
{
    p->top_count = p->top_rank = 0;
    p->top_player = PRES_NONE;
    p->passes = p->passed = 0;
    p->npile = p->nplays = 0;
}

/* the exchange's next step: the Président gives back, then the Vice-président; then the Trou leads */
static void exchange_next(ppos *p)
{
    int pres = who(p, PR_PRES), vice = who(p, PR_VICE);
    if (p->giver == PRES_NONE && pres >= 0) {
        p->giver = (uint8_t)pres, p->receiver = (uint8_t)who(p, PR_TROU), p->give_left = 2;
    } else if (p->giver == pres && vice >= 0) {
        p->giver = (uint8_t)vice, p->receiver = (uint8_t)who(p, PR_VICETROU), p->give_left = 1;
    } else {
        p->phase = PP_PLAY;
        int trou = who(p, PR_TROU);
        p->turn = (uint8_t)(trou >= 0 ? trou : 0);
        return;
    }
    p->phase = PP_GIVE;
    p->turn = p->giver;
}

static void deal(ppos *p)
{
    uint8_t pack[CARDS_MAX];
    int n = cards_pack(PACK_52, pack);
    cards_shuffle(pack, n, mix(p->seed, 1000u + p->shuffles++));
    int trou = who(p, PR_TROU), first = trou >= 0 ? trou : 0;
    memset(p->n, 0, sizeof p->n);
    memset(p->ngot, 0, sizeof p->ngot);
    for (int i = 0; i < n; i++) {
        int q = (first + i) % p->players;
        p->hand[q][p->n[q]++] = pack[i];
    }
    p->nout = 0;
    clear_trick(p);
    p->giver = PRES_NONE;
    if (trou >= 0) {                            /* the best cards go up at once */
        give_best(p, trou, who(p, PR_PRES));
        give_best(p, trou, who(p, PR_PRES));
        int vt = who(p, PR_VICETROU), vice = who(p, PR_VICE);
        if (vt >= 0 && vice >= 0) give_best(p, vt, vice);
    }
    exchange_next(p);
}

void pres_new(pgame *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    ppos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players < 3 ? 3 : players > PRES_MAXP ? PRES_MAXP : players);
    deal(p);
}

/* the deal is over: places, points, the roles for the next one */
static void deal_end(ppos *p)
{
    for (int q = 0; q < p->players; q++)
        if (pres_place(p, q) < 0) p->out[p->nout++] = (uint8_t)q;
    int np = p->players;
    for (int i = 0; i < np; i++) {
        int q = p->out[i];
        p->last[q] = (int16_t)pres_points(np, i);
        p->score[q] = (int16_t)(p->score[q] + p->last[q]);
        p->role[q] = i == 0 ? PR_PRES : i == np - 1 ? PR_TROU : np >= 4 && i == 1 ? PR_VICE
                   : np >= 4 && i == np - 2 ? PR_VICETROU : PR_NEUTRAL;
    }
    p->deal++;
    p->phase = p->deal >= PRES_DEALS ? PP_OVER : PP_DEAL;
}

/* the trick is over: the last player who played leads, or the next one still in */
static void trick_end(ppos *p)
{
    int lead = p->top_player;
    clear_trick(p);
    p->turn = (uint8_t)(pres_active(p, lead) ? lead : next_active(p, lead));
}

static void apply(ppos *p, int act)
{
    int pl = p->turn;
    switch (PA_KIND(act)) {
    case PA_GIVE: {
        int c = act & 0xff, to = p->receiver;
        take_out(p, pl, c);
        p->hand[to][p->n[to]++] = (uint8_t)c;
        p->got[to][p->ngot[to]++] = (uint8_t)c;
        if (--p->give_left == 0) exchange_next(p);
        break;
    }
    case PA_PLAY: {
        int r = PA_RANK(act), m = PA_MASK(act), c = 0;
        for (int s = 0; s < 4; s++)
            if (m >> s & 1) take_out(p, pl, CARD(s, r)), p->pile[p->npile++] = (uint8_t)CARD(s, r), c++;
        p->plays[p->nplays++] = (uint8_t)c;
        p->top_rank = (uint8_t)r;
        p->top_count = (uint8_t)c;
        p->top_player = (uint8_t)pl;
        p->passes = p->passed = 0;
        if (!p->n[pl]) p->out[p->nout++] = (uint8_t)pl;
        if (count_active(p) <= 1) { deal_end(p); break; }
        if (r == 2) trick_end(p);               /* a 2 ends the trick */
        else p->turn = (uint8_t)next_active(p, pl);
        break;
    }
    case PA_PASS: {
        p->passes++;
        p->passed |= (uint8_t)(1u << pl);
        int act_n = count_active(p), need = pres_active(p, p->top_player) ? act_n - 1 : act_n;
        if (p->passes >= need) trick_end(p);
        else p->turn = (uint8_t)next_active(p, pl);
        break;
    }
    case PA_NEXT: deal(p); break;
    }
}

int pres_play(pgame *g, int act)
{
    if (g->n >= PRES_MAX_ACTS || !pres_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int pres_undo(pgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint16_t acts[PRES_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    pres_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) pres_play(g, acts[i]);
    return 1;
}

unsigned pres_winners(const ppos *p)
{
    if (p->phase != PP_OVER) return 0;
    int best = -1;
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) best = p->score[q] > best ? p->score[q] : best;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->score[q] == best) << q;
    return m;
}

/* the suits of `rank` the player holds (a mask) */
static int suits_held(const ppos *p, int pl, int rank)
{
    int m = 0;
    for (int i = 0; i < p->n[pl]; i++)
        if (CARD_RANK(p->hand[pl][i]) == rank) m |= 1 << CARD_SUIT(p->hand[pl][i]);
    return m;
}

/* the lowest `count` suits of a mask */
static int lowest(int m, int count)
{
    int out = 0;
    for (int s = 0; s < 4 && count > 0; s++)
        if (m >> s & 1) out |= 1 << s, count--;
    return out;
}

/* plain play: give the lowest card; lead all of the lowest rank; follow with the lowest rank that beats, else pass */
int pres_auto(const ppos *p)
{
    int pl = p->turn;
    switch (p->phase) {
    case PP_GIVE: {
        int lo = 0;
        for (int i = 1; i < p->n[pl]; i++)
            if (pres_strength(CARD_RANK(p->hand[pl][i])) < pres_strength(CARD_RANK(p->hand[pl][lo]))) lo = i;
        return PA_GIVE | p->hand[pl][lo];
    }
    case PP_PLAY:
        for (int st = 1; st <= 13; st++) {
            int r = st == 12 ? 1 : st == 13 ? 2 : st + 2, m = suits_held(p, pl, r), c = popcount4(m);
            if (!c) continue;
            if (!p->top_count) return PA_PLAYOF(r, m);
            if (c >= p->top_count && st > pres_strength(p->top_rank)) return PA_PLAYOF(r, lowest(m, p->top_count));
        }
        return PA_PASS;
    case PP_DEAL: return PA_NEXT;
    default: return 0;
    }
}

static int rank_of(char c)
{
    switch (c) {
    case 'A': return RANK_ACE;
    case 'K': return RANK_KING;
    case 'Q': return RANK_QUEEN;
    case 'J': return RANK_JACK;
    case 'T': return 10;
    default: return c >= '2' && c <= '9' ? c - '0' : -1;
    }
}

static int suit_of(char c)
{
    switch (c) {
    case 's': return SUIT_SPADES;
    case 'h': return SUIT_HEARTS;
    case 'd': return SUIT_DIAMONDS;
    case 'c': return SUIT_CLUBS;
    default: return -1;
    }
}

int pres_script(pgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int act = -1;
        if (c == '#') {
            while (g->pos.phase == PP_GIVE || g->pos.phase == PP_PLAY)
                if (!pres_play(g, pres_auto(&g->pos))) return 0;
            continue;
        }
        if (c == '.') act = pres_auto(&g->pos);
        else if (c == 'p') act = PA_PASS;
        else if (c == 'N') act = PA_NEXT;
        else if (c == 'g') {
            int r = rank_of(s[0]), su = s[0] ? suit_of(s[1]) : -1;
            if (r < 0 || su < 0) return 0;
            s += 2;
            act = PA_GIVE | CARD(su, r);
        } else if (rank_of(c) > 0) {
            int r = rank_of(c), k = 1;
            while (*s == c) s++, k++;
            int m = suits_held(&g->pos, g->pos.turn, r);
            if (popcount4(m) < k) return 0;
            act = PA_PLAYOF(r, lowest(m, k));
        }
        if (act < 0 || !pres_play(g, act)) return 0;
    }
    return 1;
}
