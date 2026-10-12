/*
 * Piquet: the rules (piquet.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/piquet.h"
#include "games/tricks.h"

#define NOBODY 255

int piquet_value(int card)
{
    int r = CARD_RANK(card);
    return r == RANK_ACE ? 11 : r >= RANK_JACK ? 10 : r;
}

int piquet_index(int card)
{
    int r = trick_rank(card);                   /* 7..14 */
    return r >= 7 ? r - 7 : 0;
}

static int seq_points(int len) { return len < 3 ? 0 : len <= 4 ? len : len + 10; }

void piquet_declare(const uint8_t *h, int n, piquet_decl *d)
{
    int has[4][8] = {{0}}, cnt[8] = {0}, num[4] = {0}, val[4] = {0};
    memset(d, 0, sizeof *d);
    for (int i = 0; i < n; i++) {
        int s = CARD_SUIT(h[i]) & 3, k = piquet_index(h[i]);
        has[s][k] = 1, cnt[k]++, num[s]++, val[s] += piquet_value(h[i]);
    }
    for (int s = 0; s < 4; s++) {
        if (num[s] > d->pn || (num[s] == d->pn && val[s] > d->pv))
            d->pn = (uint8_t)num[s], d->pv = (uint8_t)val[s], d->psuit = (uint8_t)s;
        for (int k = 0, run = 0; k <= 8; k++) {   /* each run ends at a gap (or past the ace) */
            if (k < 8 && has[s][k]) { run++; continue; }
            if (run >= 3) {
                d->seqs = (uint8_t)(d->seqs + seq_points(run));
                if (run > d->sn || (run == d->sn && k - 1 > d->stop))
                    d->sn = (uint8_t)run, d->stop = (uint8_t)(k - 1), d->ssuit = (uint8_t)s;
            }
            run = 0;
        }
    }
    for (int k = 3; k < 8; k++) {               /* tens and higher */
        if (cnt[k] < 3) continue;
        d->sets = (uint8_t)(d->sets + (cnt[k] == 4 ? 14 : 3));
        if (cnt[k] > d->setn || (cnt[k] == d->setn && k > d->setrank))
            d->setn = (uint8_t)cnt[k], d->setrank = (uint8_t)k;
    }
}

static int cmp2(int a1, int a2, int b1, int b2)  /* (a1, a2) against (b1, b2) */
{
    if (a1 != b1) return a1 > b1 ? PQ_GOOD : PQ_NOT_GOOD;
    if (a2 != b2) return a2 > b2 ? PQ_GOOD : PQ_NOT_GOOD;
    return PQ_EQUAL;
}

int piquet_compare(const piquet_decl *e, const piquet_decl *y, int kind)
{
    switch (kind) {
    case PQ_POINT: return cmp2(e->pn, e->pv, y->pn, y->pv);
    case PQ_SEQUENCE: return !e->sn && !y->sn ? PQ_NONE : cmp2(e->sn, e->stop, y->sn, y->stop);
    default: return !e->setn && !y->setn ? PQ_NONE : cmp2(e->setn, e->setrank, y->setn, y->setrank);
    }
}

int piquet_kind_points(const piquet_decl *d, int kind)
{
    return kind == PQ_POINT ? d->pn : kind == PQ_SEQUENCE ? d->seqs : d->sets;
}

int piquet_kind_winner(const piquet_pos *p, int kind)
{
    return p->verdict[kind] == PQ_GOOD ? p->elder : p->verdict[kind] == PQ_NOT_GOOD ? 1 - p->elder : -1;
}

int piquet_shown(const piquet_pos *p, int pl, int kind, uint8_t *out)
{
    const uint8_t *h = p->hand[pl];
    const piquet_decl *d = &p->decl[pl];
    int n = p->n[pl], k = 0, has[4][8] = {{0}}, cnt[8] = {0};
    for (int i = 0; i < n; i++) has[CARD_SUIT(h[i]) & 3][piquet_index(h[i])] = 1, cnt[piquet_index(h[i])]++;
    for (int i = 0; i < n; i++) {
        int s = CARD_SUIT(h[i]) & 3, x = piquet_index(h[i]), in = 0;
        if (kind == PQ_POINT) in = s == d->psuit;
        else if (kind == PQ_SETS) in = x >= 3 && cnt[x] >= 3;
        else {                                  /* in a run of three or more */
            int lo = x, hi = x;
            while (lo > 0 && has[s][lo - 1]) lo--;
            while (hi < 7 && has[s][hi + 1]) hi++;
            in = hi - lo + 1 >= 3;
        }
        if (in) out[k++] = h[i];
    }
    return k;
}

int piquet_blanche(const uint8_t *h, int n)
{
    for (int i = 0; i < n; i++) {
        int r = CARD_RANK(h[i]);
        if (r == RANK_JACK || r == RANK_QUEEN || r == RANK_KING) return 0;
    }
    return 1;
}

static void deal(piquet_pos *p)
{
    uint8_t deck[CARDS_MAX];
    int k = cards_pack(PACK_32, deck);
    cards_shuffle(deck, k, trick_mix(p->seed, 5000u + p->deal));
    p->dealer = (uint8_t)((p->deal + 1) % PIQUET_P);
    p->elder = (uint8_t)(1 - p->dealer);
    for (int pl = 0; pl < PIQUET_P; pl++) {
        p->n[pl] = PIQUET_HAND;
        memcpy(p->hand[pl], deck + (pl == p->elder ? 0 : PIQUET_HAND), PIQUET_HAND);
        trick_sort(p->hand[pl], PIQUET_HAND);
        p->blanche[pl] = (uint8_t)piquet_blanche(p->hand[pl], PIQUET_HAND);
        p->ex[pl] = p->won[pl] = 0;
        p->dpts[pl] = p->ppts[pl] = p->bonus[pl] = p->score[pl] = 0;
    }
    memcpy(p->talon, deck + 2 * PIQUET_HAND, PIQUET_TALON);
    memset(p->decl, 0, sizeof p->decl);
    memset(p->verdict, PQ_NONE, sizeof p->verdict);
    memset(p->last, CARD_NONE, sizeof p->last);
    p->top = p->step = p->ntrick = p->tricks = 0;
    p->last_winner = p->last_leader = TRICK_NONE;
    p->pique = p->repique = p->capot = NOBODY;
    p->phase = PQ_EXCHANGE;
    p->turn = p->leader = p->elder;
}

void piquet_new(piquet_game *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    deal(&g->pos);
}

int piquet_throw_min(const piquet_pos *p) { return p->phase == PQ_EXCHANGE && p->turn == p->elder ? 1 : 0; }

int piquet_throw_max(const piquet_pos *p)
{
    if (p->phase != PQ_EXCHANGE) return 0;
    int left = PIQUET_TALON - p->top;
    return p->turn == p->elder ? (left < 5 ? left : 5) : left;
}

int piquet_can_play(const piquet_pos *p, int card)
{
    if (p->phase != PQ_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    if (trick_find(h, n, card) < 0) return 0;
    return trick_follows(h, n, p->ntrick ? CARD_SUIT(p->trick[0]) : -1, card);
}

int piquet_legal(const piquet_pos *p, int act)
{
    int arg = PQA_ARG(act);
    switch (PQA_KIND(act)) {
    case PQA_EXCHANGE: {
        if (p->phase != PQ_EXCHANGE || arg >> p->n[p->turn]) return 0;
        int k = 0;
        for (int i = 0; i < PIQUET_HAND; i++) k += arg >> i & 1;
        return k >= piquet_throw_min(p) && k <= piquet_throw_max(p);
    }
    case PQA_DECLARE: return p->phase == PQ_DECLARE && arg == 0;
    case PQA_PLAY: return arg < 256 && piquet_can_play(p, arg);
    case PQA_NEXT: return p->phase == PQ_ROUND && arg == 0;
    default: return 0;
    }
}

/* the declarations scored in order (carte blanche, point, sequences, sets): a repique for 30 before the other scores */
static void add_declared(piquet_pos *p, int pl, int v, int *s)
{
    if (pl < 0 || v <= 0) return;
    s[pl] += v;
    p->dpts[pl] = (int16_t)(p->dpts[pl] + v);
    if (p->repique == NOBODY && s[pl] >= 30 && s[1 - pl] == 0) {
        p->repique = (uint8_t)pl;
        p->bonus[pl] = (int16_t)(p->bonus[pl] + 60);
    }
}

/* the hands' declarations and the answers, known once the cards are changed */
static void compare(piquet_pos *p)
{
    int e = p->elder, y = 1 - e;
    for (int pl = 0; pl < PIQUET_P; pl++) piquet_declare(p->hand[pl], p->n[pl], &p->decl[pl]);
    for (int k = 0; k < PQ_KINDS; k++) p->verdict[k] = (uint8_t)piquet_compare(&p->decl[e], &p->decl[y], k);
}

static void declarations(piquet_pos *p)
{
    int e = p->elder, y = 1 - e, s[2] = {0, 0};
    compare(p);
    add_declared(p, e, p->blanche[e] ? 10 : 0, s);
    add_declared(p, y, p->blanche[y] ? 10 : 0, s);
    for (int k = 0; k < PQ_KINDS; k++) {
        int w = piquet_kind_winner(p, k);
        if (w >= 0) add_declared(p, w, piquet_kind_points(&p->decl[w], k), s);
    }
}

/* points in the play; a pique: the elder at 30 (declarations and the tricks' points, not the cards' nor capot's, which
 * come once the play is over) while the younger has nothing */
static void add_played(piquet_pos *p, int pl, int v, int pique)
{
    p->ppts[pl] = (int16_t)(p->ppts[pl] + v);
    int e = p->elder, y = 1 - e;
    if (!pique || p->pique != NOBODY || p->repique != NOBODY) return;
    if (p->dpts[y] + p->ppts[y] == 0 && p->dpts[e] + p->ppts[e] >= 30) {
        p->pique = (uint8_t)e;
        p->bonus[e] = (int16_t)(p->bonus[e] + 30);
    }
}

static void deal_end(piquet_pos *p)
{
    for (int pl = 0; pl < PIQUET_P; pl++) {
        p->score[pl] = (int16_t)(p->dpts[pl] + p->ppts[pl] + p->bonus[pl]);
        p->total[pl] = (int16_t)(p->total[pl] + p->score[pl]);
    }
    p->deal++;
    p->turn = (uint8_t)(p->deal % PIQUET_P);    /* the next deal's elder */
    if (p->deal < PIQUET_DEALS) { p->phase = PQ_ROUND; return; }
    p->phase = PQ_OVER;
    int w = piquet_winner(p);
    if (w < 0) { p->final = 0, p->rubicon = 0; return; }
    int l = 1 - w;
    p->rubicon = p->total[l] < 100;
    p->final = (int16_t)(p->rubicon ? p->total[w] + p->total[l] + 100 : p->total[w] - p->total[l] + 100);
}

static void apply(piquet_pos *p, int act)
{
    int pl = p->turn, arg = PQA_ARG(act);
    switch (PQA_KIND(act)) {
    case PQA_EXCHANGE: {
        uint8_t keep[PIQUET_HAND];
        int k = 0, out = 0;
        for (int i = 0; i < p->n[pl]; i++) {
            if (arg >> i & 1) out++;
            else keep[k++] = p->hand[pl][i];
        }
        for (int i = 0; i < out; i++) keep[k++] = p->talon[p->top++];
        memcpy(p->hand[pl], keep, PIQUET_HAND);
        trick_sort(p->hand[pl], PIQUET_HAND);
        p->ex[pl] = (uint8_t)out;
        if (pl == p->elder) p->turn = (uint8_t)(1 - pl);
        else p->phase = PQ_DECLARE, p->turn = p->elder, compare(p);
        break;
    }
    case PQA_DECLARE:
        if (pl == p->elder) { p->turn = (uint8_t)(1 - pl); break; }
        declarations(p);
        p->phase = PQ_PLAY;
        p->turn = p->leader = p->elder;
        break;
    case PQA_PLAY:
        trick_remove(p->hand[pl], &p->n[pl], arg);
        p->trick[p->ntrick++] = (uint8_t)arg;
        if (p->ntrick == 1) {
            if (trick_rank(arg) >= 10) add_played(p, pl, 1, 1);   /* a lead of 10 or higher */
            p->turn = (uint8_t)(1 - pl);
            break;
        }
        {
            int w = (p->leader + trick_winner(p->trick, PIQUET_P, -1)) % PIQUET_P;
            if (w != p->leader && trick_rank(p->trick[1]) >= 10) add_played(p, w, 1, 1);   /* the other's lead won */
            memcpy(p->last, p->trick, PIQUET_P);
            p->won[w]++;
            p->last_leader = p->leader;
            p->last_winner = (uint8_t)w;
            p->ntrick = 0;
            p->tricks++;
            p->turn = p->leader = (uint8_t)w;
            if (p->tricks < PIQUET_HAND) break;
            add_played(p, w, 1, 1);             /* the last trick */
            for (int q = 0; q < PIQUET_P; q++) {
                if (p->won[q] == PIQUET_HAND) p->capot = (uint8_t)q, add_played(p, q, 40, 0);
                else if (p->won[q] > PIQUET_HAND / 2) add_played(p, q, 10, 0);   /* the cards */
            }
            deal_end(p);
        }
        break;
    case PQA_NEXT:
        deal(p);
        break;
    }
}

int piquet_play(piquet_game *g, int act)
{
    if (g->n >= PIQUET_MAX_ACTS || !piquet_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int piquet_undo(piquet_game *g)
{
    if (!g->n) return 0;
    static uint16_t acts[PIQUET_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    piquet_new(g, g->seed);
    for (int i = 0; i < n; i++) piquet_play(g, acts[i]);
    return 1;
}

int piquet_winner(const piquet_pos *p)
{
    if (p->deal < PIQUET_DEALS || p->total[0] == p->total[1]) return -1;
    return p->total[1] > p->total[0];
}

int piquet_auto(const piquet_pos *p)
{
    const uint8_t *h = p->hand[p->turn];
    switch (p->phase) {
    case PQ_EXCHANGE: {
        if (!piquet_throw_min(p)) return PQA_EXCHANGE;
        int low = 0;                            /* the elder throws the lowest card */
        for (int i = 1; i < p->n[p->turn]; i++)
            if (trick_rank(h[i]) < trick_rank(h[low])) low = i;
        return PQA_EXCHANGE | 1 << low;
    }
    case PQ_DECLARE: return PQA_DECLARE;
    case PQ_PLAY:
        for (int i = 0; i < p->n[p->turn]; i++)
            if (piquet_can_play(p, h[i])) return PQA_PLAY | h[i];
        return 0;
    case PQ_ROUND: return PQA_NEXT;
    default: return 0;
    }
}

int piquet_script(piquet_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (g->pos.phase != PQ_OVER && (all || g->pos.phase != PQ_ROUND))
                if (!piquet_play(g, piquet_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; act = piquet_auto(&g->pos); }
        else if (*s == 'n') { s++; act = PQA_NEXT; }
        else if (*s == 'k') { s++; act = PQA_DECLARE; }
        else if (*s == 'e') {
            s++;
            int m = 0;
            const piquet_pos *p = &g->pos;
            while (*s && *s != ' ') {
                int c = trick_parse(&s), i = c == CARD_NONE ? -1 : trick_find(p->hand[p->turn], p->n[p->turn], c);
                if (i < 0) return 0;
                m |= 1 << i;
            }
            act = PQA_EXCHANGE | m;
        } else {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            act = PQA_PLAY | c;
        }
        if (!piquet_play(g, act)) return 0;
    }
    return 1;
}
