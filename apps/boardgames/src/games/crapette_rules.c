/*
 * Crapette: the rules (crapette.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/crapette.h"

static int red(int c) { return CARD_SUIT(c) == SUIT_HEARTS || CARD_SUIT(c) == SUIT_DIAMONDS; }
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

void crap_new(cgame *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    cpos *p = &g->pos;
    g->seed = p->seed = seed;
    for (int q = 0; q < 2; q++) {               /* each pack dealt from its top (the end of the array) */
        uint8_t pack[CARDS_MAX];
        int n = cards_pack(PACK_52, pack);
        cards_shuffle(pack, n, mix(seed, 17u + (uint32_t)q));
        for (int i = 0; i < 13; i++) p->crap[q][i] = pack[--n];
        p->ncrap[q] = 13;
        for (int k = 0; k < 4; k++) {
            p->col[4 * q + k][0] = pack[--n];
            p->ncol[4 * q + k] = 1;
        }
        for (int i = 0; i < n; i++) p->talon[q][i] = pack[i];
        p->ntalon[q] = (uint8_t)n;
    }
    for (int f = 0; f < CR_FOUND; f++) p->found[f] = CARD_NONE;
    p->held = CARD_NONE;
    p->winner = CR_NONE;
    /* who starts: the higher crapette card, then the higher house cards in order */
    int first = 0, d = CARD_RANK(p->crap[1][12]) - CARD_RANK(p->crap[0][12]);
    for (int k = 0; !d && k < 4; k++) d = CARD_RANK(p->col[4 + k][0]) - CARD_RANK(p->col[k][0]);
    if (d > 0) first = 1;
    p->first = p->turn = (uint8_t)first;
}

static int other(const cpos *p) { return 1 - p->turn; }

int crap_card_at(const cpos *p, int place)
{
    int me = p->turn, op = other(p);
    if (place < CL_FOUND) return p->ncol[place] ? p->col[place][p->ncol[place] - 1] : CARD_NONE;
    if (place < CL_CRAP) return p->found[place - CL_FOUND];
    switch (place) {
    case CL_CRAP: return p->ncrap[me] ? p->crap[me][p->ncrap[me] - 1] : CARD_NONE;
    case CL_HELD: return p->held;
    case CL_OCRAP: return p->ncrap[op] ? p->crap[op][p->ncrap[op] - 1] : CARD_NONE;
    case CL_OWASTE: return p->nwaste[op] ? p->waste[op][p->nwaste[op] - 1] : CARD_NONE;
    }
    return CARD_NONE;
}

int crap_fits(const cpos *p, int c, int place)
{
    if (c == CARD_NONE) return 0;
    int r = CARD_RANK(c);
    if (place < CL_FOUND) {
        if (!p->ncol[place]) return 1;
        if (p->ncol[place] >= CR_COLMAX) return 0;
        int t = p->col[place][p->ncol[place] - 1];
        return CARD_RANK(t) == r + 1 && red(t) != red(c);
    }
    if (place < CL_CRAP) {
        int t = p->found[place - CL_FOUND];
        if (t == CARD_NONE) return r == RANK_ACE;
        return CARD_SUIT(t) == CARD_SUIT(c) && CARD_RANK(t) + 1 == r;
    }
    if (place == CL_OCRAP || place == CL_OWASTE) {
        int t = crap_card_at(p, place), n = place == CL_OCRAP ? p->ncrap[other(p)] : p->nwaste[other(p)];
        if (t == CARD_NONE || n >= CR_PILE) return 0;
        return CARD_SUIT(t) == CARD_SUIT(c) && (CARD_RANK(t) == r + 1 || CARD_RANK(t) + 1 == r);
    }
    return 0;
}

int crap_found_for(const cpos *p, int c)
{
    for (int f = 0; f < CR_FOUND; f++)          /* the suit's own foundation first, then the first empty one */
        if (p->found[f] != CARD_NONE && crap_fits(p, c, CL_FOUND + f)) return CL_FOUND + f;
    for (int f = 0; f < CR_FOUND; f++)
        if (crap_fits(p, c, CL_FOUND + f)) return CL_FOUND + f;
    return -1;
}

static int is_source(int s) { return s < CL_FOUND || s == CL_CRAP || s == CL_HELD; }

static int move_ok(const cpos *p, int s, int d)
{
    if (!is_source(s) || d >= CL_N || d == CL_CRAP || d == CL_HELD || s == d) return 0;
    return crap_fits(p, crap_card_at(p, s), d);
}

int crap_legal(const cpos *p, int a)
{
    if (p->phase != CP_PLAY) return 0;
    int me = p->turn;
    switch (CA_KIND(a)) {
    case CA_MOVE: return (a & ~0x1fff) == 0 && move_ok(p, CA_SRC(a), CA_DST(a));
    case CA_TURN: return a == CA_TURN && p->held == CARD_NONE && (p->ntalon[me] || p->nwaste[me]);
    case CA_DISCARD: return a == CA_DISCARD && p->held != CARD_NONE && p->nwaste[me] < CR_PILE;
    case CA_PASS: return a == CA_PASS && p->held == CARD_NONE && !p->ntalon[me] && !p->nwaste[me];
    case CA_CALL: return a == CA_CALL && p->fault != CF_NONE;
    }
    return 0;
}

int crap_moves(const cpos *p, int *acts)
{
    int n = 0;
    static const int src[] = {CL_CRAP, CL_HELD, 0, 1, 2, 3, 4, 5, 6, 7};
    for (unsigned i = 0; i < sizeof src / sizeof src[0]; i++)
        for (int d = 0; d < CL_N; d++)
            if (move_ok(p, src[i], d) && p->phase == CP_PLAY) acts[n++] = CA_MOVEOF(src[i], d);
    static const int other_acts[] = {CA_TURN, CA_DISCARD, CA_PASS, CA_CALL};
    for (int i = 0; i < 4; i++)
        if (crap_legal(p, other_acts[i])) acts[n++] = other_acts[i];
    return n;
}

/* a card available to the player to move that can go to a foundation (the crapette's first); CARD_NONE: none */
static int to_foundation(const cpos *p, int *crapette)
{
    int c = crap_card_at(p, CL_CRAP);
    *crapette = 0;
    if (c != CARD_NONE && crap_found_for(p, c) >= 0) { *crapette = 1; return c; }
    c = crap_card_at(p, CL_HELD);
    if (c != CARD_NONE && crap_found_for(p, c) >= 0) return c;
    for (int k = 0; k < CR_COLS; k++) {
        c = crap_card_at(p, k);
        if (c != CARD_NONE && crap_found_for(p, c) >= 0) return c;
    }
    return CARD_NONE;
}

static int crapette_playable(const cpos *p)
{
    for (int d = 0; d < CL_N; d++)
        if (move_ok(p, CL_CRAP, d)) return 1;
    return 0;
}

/* the priority an action breaks; the card it passed over into *card. Ending the turn (the talon card to the waste, a
 * pass) is never a fault the other may call: the turn is theirs anyway, with that card still to play. */
static int fault_card(const cpos *p, int a, int *card)
{
    int k = CA_KIND(a), crapette, c = to_foundation(p, &crapette);
    *card = CARD_NONE;
    if (k == CA_DISCARD || k == CA_PASS || k == CA_CALL) return CF_NONE;
    int s = CA_SRC(a), d = CA_DST(a), to_found = k == CA_MOVE && d >= CL_FOUND && d < CL_CRAP;
    if (c != CARD_NONE && crapette && !(to_found && s == CL_CRAP)) { *card = c; return CF_CRAP_FOUND; }
    if (c != CARD_NONE && !to_found) { *card = c; return CF_FOUND; }
    if (k == CA_TURN && crapette_playable(p)) { *card = crap_card_at(p, CL_CRAP); return CF_CRAP_FIRST; }
    return CF_NONE;
}

int crap_fault_of(const cpos *p, int a)
{
    int c;
    return fault_card(p, a, &c);
}

int crap_left(const cpos *p, int q)
{
    return p->ncrap[q] + p->ntalon[q] + p->nwaste[q] + (q == p->turn && p->held != CARD_NONE);
}

static void take(cpos *p, int s)                /* the card leaves its place */
{
    int me = p->turn;
    if (s < CL_FOUND) p->ncol[s]--;
    else if (s == CL_CRAP) p->ncrap[me]--;
    else if (s == CL_HELD) p->held = CARD_NONE;
}

static void put(cpos *p, int c, int d)
{
    int op = other(p);
    if (d < CL_FOUND) p->col[d][p->ncol[d]++] = (uint8_t)c;
    else if (d < CL_CRAP) p->found[d - CL_FOUND] = (uint8_t)c;
    else if (d == CL_OCRAP) p->crap[op][p->ncrap[op]++] = (uint8_t)c;
    else if (d == CL_OWASTE) p->waste[op][p->nwaste[op]++] = (uint8_t)c;
}

static void check_over(cpos *p)
{
    int me = p->turn;
    if (!p->ncrap[me] && !p->ntalon[me] && !p->nwaste[me] && p->held == CARD_NONE) {
        p->phase = CP_OVER;
        p->winner = (uint8_t)me;
    }
}

static void end_turn(cpos *p)
{
    p->idle = p->moved ? 0 : (uint16_t)(p->idle + 1);
    p->moved = 0;
    p->fault = CF_NONE;
    p->fault_card = CARD_NONE;
    p->turn = (uint8_t)other(p);
    /* blocked: nothing has happened while each player went through all their talon and waste twice */
    int most = 0;
    for (int q = 0; q < 2; q++) {
        int n = p->ntalon[q] + p->nwaste[q];
        if (n > most) most = n;
    }
    if (p->idle >= 4 * most + 4) {
        int a = crap_left(p, 0), b = crap_left(p, 1);
        p->phase = CP_OVER;
        p->blocked = 1;
        p->winner = a < b ? 0 : b < a ? 1 : CR_DRAW;
    }
}

static void apply(cpos *p, int a)
{
    int me = p->turn;
    switch (CA_KIND(a)) {
    case CA_MOVE: {
        int s = CA_SRC(a), d = CA_DST(a), c = crap_card_at(p, s);
        take(p, s);
        put(p, c, d);
        if ((d >= CL_FOUND && d < CL_CRAP) || s == CL_CRAP || s == CL_HELD) p->moved = 1;
        check_over(p);
        break;
    }
    case CA_TURN:
        if (!p->ntalon[me]) {                   /* the waste turned over: its first card on top */
            int n = p->nwaste[me];
            for (int i = 0; i < n; i++) p->talon[me][i] = p->waste[me][n - 1 - i];
            p->ntalon[me] = (uint8_t)n;
            p->nwaste[me] = 0;
        }
        p->held = p->talon[me][--p->ntalon[me]];
        break;
    case CA_DISCARD:
        p->waste[me][p->nwaste[me]++] = p->held;
        p->held = CARD_NONE;
        check_over(p);
        if (p->phase == CP_PLAY) end_turn(p);
        break;
    case CA_PASS:
        end_turn(p);
        break;
    }
}

int crap_play(cgame *g, int a)
{
    cpos *p = &g->pos;
    if (g->n >= CR_MAX_ACTS || !crap_legal(p, a)) return 0;
    if (a == CA_CALL) {                         /* back to before the fault; the talon card to the waste; the turn
                                                 * to the caller */
        *p = g->saved;
        int me = p->turn;
        if (p->held != CARD_NONE) {
            p->waste[me][p->nwaste[me]++] = p->held;
            p->held = CARD_NONE;
        }
        p->moved = 1;
        end_turn(p);
    } else {
        int c, f = fault_card(p, a, &c), first = f != CF_NONE && p->fault == CF_NONE;
        if (first) g->saved = *p;
        apply(p, a);
        if (first && p->phase == CP_PLAY && CA_KIND(a) != CA_DISCARD && CA_KIND(a) != CA_PASS) {
            p->fault = (uint8_t)f;
            p->fault_card = (uint8_t)c;
        }
    }
    g->act[g->n++] = (uint16_t)a;
    return 1;
}

int crap_undo(cgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint16_t acts[CR_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    crap_new(g, g->seed);
    for (int i = 0; i < n; i++) crap_play(g, acts[i]);
    return 1;
}

/* a plain action, keeping the priorities: to the foundations, the crapette, the talon card, then the talon */
int crap_auto(const cpos *p)
{
    if (p->phase != CP_PLAY) return 0;
    int crapette, c = to_foundation(p, &crapette);
    if (c != CARD_NONE) {
        if (crapette) return CA_MOVEOF(CL_CRAP, crap_found_for(p, c));
        if (crap_card_at(p, CL_HELD) == c) return CA_MOVEOF(CL_HELD, crap_found_for(p, c));
        for (int k = 0; k < CR_COLS; k++)
            if (crap_card_at(p, k) == c) return CA_MOVEOF(k, crap_found_for(p, c));
    }
    for (int d = 0; d < CL_N; d++)
        if (move_ok(p, CL_CRAP, d)) return CA_MOVEOF(CL_CRAP, d);
    if (p->held != CARD_NONE) {
        for (int d = 0; d < CL_N; d++)
            if (d >= CL_OCRAP && move_ok(p, CL_HELD, d)) return CA_MOVEOF(CL_HELD, d);
        for (int d = 0; d < CL_N; d++)
            if (d < CL_FOUND && p->ncol[d] && move_ok(p, CL_HELD, d)) return CA_MOVEOF(CL_HELD, d);
        return CA_DISCARD;
    }
    if (crap_legal(p, CA_TURN)) return CA_TURN;
    return CA_PASS;
}

static int place_of(const char **s)
{
    char c = **s;
    if (c >= '0' && c <= '7') { (*s)++; return c - '0'; }
    (*s)++;
    switch (c) {
    case 'f':
        if (**s >= '0' && **s <= '7') return CL_FOUND + (*(*s)++ - '0');
        return -2;                              /* the one that fits */
    case 'c': return CL_CRAP;
    case 'h': return CL_HELD;
    case 'C': return CL_OCRAP;
    case 'W': return CL_OWASTE;
    }
    return -1;
}

int crap_script(cgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int a;
        switch (*s) {
        case 't': a = CA_TURN; s++; break;
        case 'd': a = CA_DISCARD; s++; break;
        case 'p': a = CA_PASS; s++; break;
        case 'x': a = CA_CALL; s++; break;
        case '.': a = crap_auto(&g->pos); s++; break;
        case '#':
            s++;
            for (int k = 0; k < 20000 && g->pos.phase == CP_PLAY; k++)
                if (!crap_play(g, crap_auto(&g->pos))) return 0;
            continue;
        default: {
            int from = place_of(&s);
            if (from < 0 || *s != '-') return 0;
            s++;
            int to = place_of(&s);
            if (to == -2) to = crap_found_for(&g->pos, crap_card_at(&g->pos, from));
            if (to < 0) return 0;
            a = CA_MOVEOF(from, to);
        }
        }
        if (!crap_play(g, a)) return 0;
    }
    return 1;
}
