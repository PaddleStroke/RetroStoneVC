/*
 * Gin Rummy: the rules (ginrummy.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/ginrummy.h"

int gin_value(int card)
{
    int r = CARD_RANK(card);
    return r > 10 ? 10 : r;
}

/* ---- melds ------------------------------------------------------------------------------------------------------- */
/* every meld the cards can make (bit i: cards[i]): sets of 3 and 4, runs of 3 and more */
static int candidates(const uint8_t *c, int n, uint16_t *m)
{
    int k = 0;
    for (int r = 1; r <= 13; r++) {             /* sets */
        uint16_t all = 0;
        int cnt = 0;
        for (int i = 0; i < n; i++)
            if (CARD_RANK(c[i]) == r) all |= (uint16_t)(1u << i), cnt++;
        if (cnt < 3) continue;
        m[k++] = all;
        if (cnt == 4)
            for (int i = 0; i < n; i++)
                if (all >> i & 1) m[k++] = (uint16_t)(all & ~(1u << i));
    }
    for (int s = 0; s < 4; s++) {               /* runs: every stretch of 3 or more in a row */
        int at[14];
        for (int r = 0; r < 14; r++) at[r] = -1;
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(c[i]) == s) at[CARD_RANK(c[i])] = i;
        for (int lo = 1; lo <= 11; lo++) {
            uint16_t run = 0;
            for (int r = lo; r <= 13 && at[r] >= 0; r++) {
                run |= (uint16_t)(1u << at[r]);
                if (r - lo >= 2) m[k++] = run;
            }
        }
    }
    return k;
}

static int dead_of(const uint8_t *c, int n, uint16_t used)
{
    int d = 0;
    for (int i = 0; i < n; i++)
        if (!(used >> i & 1)) d += gin_value(c[i]);
    return d;
}

/* the knocker's melds, for the layoffs: a set (its rank, how many) or a run (its suit, its ends) */
typedef struct { int run, rank, cnt, suit, lo, hi; } meld;

static meld meld_of(const uint8_t *c, int len)
{
    meld m = {0, CARD_RANK(c[0]), len, CARD_SUIT(c[0]), 99, 0};
    for (int i = 1; i < len; i++) m.run |= CARD_RANK(c[i]) != m.rank;
    for (int i = 0; i < len; i++) {
        int r = CARD_RANK(c[i]);
        m.lo = r < m.lo ? r : m.lo;
        m.hi = r > m.hi ? r : m.hi;
    }
    return m;
}

static int fits(const meld *m, int card)
{
    int r = CARD_RANK(card);
    if (!m->run) return r == m->rank && m->cnt < 4;
    return CARD_SUIT(card) == m->suit && (r == m->lo - 1 || r == m->hi + 1);
}

static void put(meld *m, int card)
{
    int r = CARD_RANK(card);
    if (!m->run) { m->cnt++; return; }
    m->lo = r < m->lo ? r : m->lo;
    m->hi = r > m->hi ? r : m->hi;
}

/* the search: the disjoint melds giving the least deadwood (after layoffs on `onto`, when given) */
typedef struct {
    const uint8_t *c;
    int n, ncand;
    uint16_t cand[64], pick[8], best_pick[8];
    int npick, best_n, best;
    const meld *onto;
    int nonto;
    uint16_t best_lay;                          /* the cards laid off at the best */
    uint8_t best_to[11];
} search;

static int layoffs(search *s, uint16_t used, uint16_t *lay, uint8_t *to)
{
    meld m[4];
    memcpy(m, s->onto, sizeof m[0] * (size_t)s->nonto);
    *lay = 0;
    for (int again = 1; again;) {               /* lay off while a card fits (a run may grow and take more) */
        again = 0;
        for (int i = 0; i < s->n; i++) {
            if ((used | *lay) >> i & 1) continue;
            /* a run first: a card that fits both a set and a run is the set's last one, while the run may grow on
             * (3h on 3c 3d 3s or on 4h 5h 6h: on the run, 2h follows it) */
            int at = -1;
            for (int k = 0; k < s->nonto; k++)
                if (fits(&m[k], s->c[i]) && (at < 0 || (m[k].run && !m[at].run))) at = k;
            if (at < 0) continue;
            put(&m[at], s->c[i]);
            *lay |= (uint16_t)(1u << i);
            to[i] = (uint8_t)at;
            again = 1;
        }
    }
    return dead_of(s->c, s->n, used | *lay);
}

static void dfs(search *s, int from, uint16_t used)
{
    uint16_t lay = 0;
    uint8_t to[11];
    int d = s->onto ? layoffs(s, used, &lay, to) : dead_of(s->c, s->n, used);
    if (d < s->best) {
        s->best = d;
        s->best_n = s->npick;
        memcpy(s->best_pick, s->pick, sizeof s->pick);
        s->best_lay = lay;
        memcpy(s->best_to, to, sizeof to);
    }
    if (s->npick >= 4) return;
    for (int i = from; i < s->ncand; i++)
        if (!(s->cand[i] & used)) {
            s->pick[s->npick++] = s->cand[i];
            dfs(s, i + 1, used | s->cand[i]);
            s->npick--;
        }
}

static void sort_rank(uint8_t *c, int n, int by_suit)   /* sets by suit, runs and deadwood by rank */
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            int a = by_suit ? CARD_SUIT(c[j - 1]) * 16 + CARD_RANK(c[j - 1]) : CARD_RANK(c[j - 1]) * 8 +
                                                                                CARD_SUIT(c[j - 1]);
            int b = by_suit ? CARD_SUIT(c[j]) * 16 + CARD_RANK(c[j]) : CARD_RANK(c[j]) * 8 + CARD_SUIT(c[j]);
            if (a <= b) break;
            uint8_t t = c[j];
            c[j] = c[j - 1];
            c[j - 1] = t;
        }
}

/* the arrangement found: the melds by their lowest rank, each in order, then the deadwood (not laid off) */
static void arrange_out(const search *s, uint16_t lay, gin_arr *out)
{
    uint16_t picks[8];
    int np = s->best_n;
    memcpy(picks, s->best_pick, sizeof picks);
    for (int i = 1; i < np; i++)                /* the melds: the lowest first */
        for (int j = i; j > 0; j--) {
            int lo_a = 99, lo_b = 99;
            for (int k = 0; k < s->n; k++) {
                if (picks[j - 1] >> k & 1) lo_a = CARD_RANK(s->c[k]) < lo_a ? CARD_RANK(s->c[k]) : lo_a;
                if (picks[j] >> k & 1) lo_b = CARD_RANK(s->c[k]) < lo_b ? CARD_RANK(s->c[k]) : lo_b;
            }
            if (lo_a <= lo_b) break;
            uint16_t t = picks[j];
            picks[j] = picks[j - 1];
            picks[j - 1] = t;
        }
    memset(out, 0, sizeof *out);
    uint16_t used = 0;
    for (int m = 0; m < np; m++) {
        int len = 0;
        uint8_t *dst = out->card + out->n;
        for (int k = 0; k < s->n; k++)
            if (picks[m] >> k & 1) dst[len++] = s->c[k];
        sort_rank(dst, len, 0);
        out->mlen[out->nm++] = (uint8_t)len;
        out->n = (uint8_t)(out->n + len);
        used |= picks[m];
    }
    int nd = 0;
    uint8_t *dst = out->card + out->n;
    for (int k = 0; k < s->n; k++)
        if (!((used | lay) >> k & 1)) dst[nd++] = s->c[k];
    sort_rank(dst, nd, 0);
    out->n = (uint8_t)(out->n + nd);
    out->dead = (uint8_t)dead_of(s->c, s->n, used | lay);
}

static void search_init(search *s, const uint8_t *c, int n, const meld *onto, int nonto)
{
    memset(s, 0, sizeof *s);
    s->c = c;
    s->n = n;
    s->ncand = candidates(c, n, s->cand);
    s->best = 1 << 20;
    s->onto = onto;
    s->nonto = nonto;
}

int gin_arrange(const uint8_t *cards, int n, gin_arr *out)
{
    search s;
    search_init(&s, cards, n, NULL, 0);
    dfs(&s, 0, 0);
    if (out) arrange_out(&s, 0, out);
    return s.best;
}

int gin_without(const uint8_t *cards, int n, int card)
{
    uint8_t c[11];
    int k = 0;
    for (int i = 0; i < n; i++)
        if (cards[i] != card || k < i) c[k++] = cards[i];
    if (k == n) return 1 << 20;                 /* not in the hand */
    return gin_arrange(c, k, NULL);
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static void deal(gnpos *p)
{
    int n = cards_pack(PACK_52, p->stock);
    cards_shuffle(p->stock, n, mix(p->seed, 2000u + p->hands));
    p->nstock = (uint8_t)n;
    int first = 1 - p->dealer;
    for (int k = 0; k < 10; k++)
        for (int i = 0; i < 2; i++) p->hand[i ? p->dealer : first][k] = p->stock[--p->nstock];
    p->n[0] = p->n[1] = 10;
    p->npile = 0;
    p->pile[p->npile++] = p->stock[--p->nstock];
    p->turn = (uint8_t)first;
    p->phase = GN_OFFER;
    p->taken = CARD_NONE;
    p->passes = 0;
    p->hands++;
}

void gin_new(gngame *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    gnpos *p = &g->pos;
    g->seed = p->seed = seed;
    p->dealer = 1;
    deal(p);
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static int in_hand(const gnpos *p, int pl, int card)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] == card) return 1;
    return 0;
}

int gin_legal(const gnpos *p, int act)
{
    int k = GNA_KIND(act), c = GNA_CARD(act), pl = p->turn;
    switch (p->phase) {
    case GN_OFFER: return (k == GNA_TAKE || k == GNA_PASS) && !c;
    case GN_DRAW: return !c && ((k == GNA_STOCK && p->nstock > 0) || (k == GNA_TAKE && p->passes < 2 && p->npile));
    case GN_DISCARD:
        if ((k != GNA_DISCARD && k != GNA_KNOCK) || c == p->taken || !in_hand(p, pl, c)) return 0;
        return k == GNA_DISCARD || gin_without(p->hand[pl], p->n[pl], c) <= GIN_KNOCK;
    case GN_END: return act == GNA_NEXT;
    default: return 0;
    }
}

int gin_can_knock(const gnpos *p)
{
    if (p->phase != GN_DISCARD) return 0;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (gin_legal(p, GNA_KNOCK | p->hand[p->turn][i])) return 1;
    return 0;
}

static void remove_card(gnpos *p, int pl, int card)
{
    int k = 0;
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] != card) p->hand[pl][k++] = p->hand[pl][i];
    p->n[pl] = (uint8_t)k;
}

/* the knock: the hands shown, the layoffs, the points, maybe the end of the game */
static void knock(gnpos *p, int kn)
{
    gin_result *r = &p->res;
    int df = 1 - kn;
    memset(r, 0, sizeof *r);
    r->knocker = (uint8_t)kn;
    search s;
    search_init(&s, p->hand[kn], p->n[kn], NULL, 0);
    dfs(&s, 0, 0);
    arrange_out(&s, 0, &r->arr[kn]);
    int K = s.best;
    meld onto[4];
    int off = 0;
    for (int m = 0; m < r->arr[kn].nm; m++) {
        onto[m] = meld_of(r->arr[kn].card + off, r->arr[kn].mlen[m]);
        off += r->arr[kn].mlen[m];
    }
    search d;
    search_init(&d, p->hand[df], p->n[df], K ? onto : NULL, K ? r->arr[kn].nm : 0);
    dfs(&d, 0, 0);
    arrange_out(&d, 0, &r->arr[df]);            /* the defender's own melds; the laid off cards among its deadwood */
    int D = d.best;
    if (K) {
        uint16_t laid = d.best_lay;
        for (int i = 0; i < d.n; i++)
            if (laid >> i & 1) r->lay[r->nlay] = d.c[i], r->layto[r->nlay++] = d.best_to[i];
    }
    r->dead[kn] = (uint8_t)K;
    r->dead[df] = (uint8_t)D;
    if (!K) r->kind = GR_GIN, r->winner = (uint8_t)kn, r->bonus = GIN_BONUS, r->points = (int16_t)(GIN_BONUS + D);
    else if (D <= K)
        r->kind = GR_UNDERCUT, r->winner = (uint8_t)df, r->bonus = GIN_BONUS, r->points = (int16_t)(GIN_BONUS + K - D);
    else r->kind = GR_KNOCK, r->winner = (uint8_t)kn, r->points = (int16_t)(D - K);
    int w = r->winner;
    p->score[w] = (int16_t)(p->score[w] + r->points);
    p->boxes[w]++;
    p->turn = (uint8_t)w;
    if (p->score[w] < GIN_GOAL) { p->phase = GN_END; return; }
    p->phase = GN_OVER;                         /* the game: the bonuses */
    for (int q = 0; q < 2; q++) p->final[q] = (int16_t)(p->score[q] + GIN_BOX * p->boxes[q]);
    p->final[w] = (int16_t)(p->final[w] + GIN_GAME_BONUS);
    p->shutout = p->boxes[1 - w] == 0;
    if (p->shutout) p->final[w] = (int16_t)(p->final[w] * 2);
}

static void apply(gnpos *p, int act)
{
    int k = GNA_KIND(act), c = GNA_CARD(act), pl = p->turn;
    switch (k) {
    case GNA_PASS:
        if (++p->passes == 1) p->turn = p->dealer;
        else p->turn = (uint8_t)(1 - p->dealer), p->phase = GN_DRAW;
        break;
    case GNA_TAKE:
        p->taken = p->pile[--p->npile];
        p->hand[pl][p->n[pl]++] = p->taken;
        p->phase = GN_DISCARD;
        break;
    case GNA_STOCK:
        p->hand[pl][p->n[pl]++] = p->stock[--p->nstock];
        p->taken = CARD_NONE;
        p->phase = GN_DISCARD;
        break;
    case GNA_DISCARD:
    case GNA_KNOCK:
        remove_card(p, pl, c);
        p->pile[p->npile++] = (uint8_t)c;
        p->taken = CARD_NONE;
        p->passes = 2;                          /* the first upcard's offer is over */
        if (k == GNA_KNOCK) { knock(p, pl); break; }
        if (p->nstock <= 2) {                   /* a dead hand */
            memset(&p->res, 0, sizeof p->res);
            p->res.kind = GR_DEAD;
            p->phase = GN_END;
            break;
        }
        p->passes = 0;
        p->turn = (uint8_t)(1 - pl);
        p->phase = GN_DRAW;
        break;
    case GNA_NEXT:
        if (p->res.kind != GR_DEAD) p->dealer = (uint8_t)(1 - p->dealer);
        deal(p);
        break;
    }
}

int gin_play(gngame *g, int act)
{
    if (g->n >= GIN_MAX_ACTS || !gin_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int gin_undo(gngame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint16_t acts[GIN_MAX_ACTS];
    memcpy(acts, g->act, sizeof acts[0] * (size_t)n);
    gin_new(g, g->seed);
    for (int i = 0; i < n; i++) gin_play(g, acts[i]);
    return 1;
}

int gin_winner(const gnpos *p) { return p->phase == GN_OVER ? p->res.winner : -1; }

/* ---- plain play -------------------------------------------------------------------------------------------------- */
/* the discard leaving the least deadwood (the highest card among equals); its deadwood */
static int best_discard(const gnpos *p, int *card)
{
    int pl = p->turn, best = 1 << 20;
    *card = CARD_NONE;
    for (int i = 0; i < p->n[pl]; i++) {
        int c = p->hand[pl][i];
        if (c == p->taken) continue;
        int d = gin_without(p->hand[pl], p->n[pl], c);
        if (d < best || (d == best && gin_value(c) > gin_value(*card))) best = d, *card = c;
    }
    return best;
}

/* taking the top of the discard pile would lower the deadwood */
static int worth_taking(const gnpos *p)
{
    int pl = p->turn, top = p->pile[p->npile - 1];
    uint8_t c[11];
    memcpy(c, p->hand[pl], p->n[pl]);
    c[p->n[pl]] = (uint8_t)top;
    int now = gin_arrange(p->hand[pl], p->n[pl], NULL), best = 1 << 20;
    for (int i = 0; i < p->n[pl]; i++) {        /* keep the card taken, throw another */
        int d = gin_without(c, p->n[pl] + 1, p->hand[pl][i]);
        best = d < best ? d : best;
    }
    return best < now;
}

int gin_auto(const gnpos *p)
{
    int card;
    switch (p->phase) {
    case GN_OFFER: return worth_taking(p) ? GNA_TAKE : GNA_PASS;
    case GN_DRAW: return p->passes < 2 && p->npile && worth_taking(p) ? GNA_TAKE : GNA_STOCK;
    case GN_DISCARD: return (best_discard(p, &card) <= GIN_KNOCK ? GNA_KNOCK : GNA_DISCARD) | card;
    case GN_END: return GNA_NEXT;
    default: return 0;
    }
}

int gin_card_name(const char *s, int *card)
{
    static const char ranks[] = "A23456789TJQK", suits[] = "shdc";
    const char *r = s[0] ? strchr(ranks, s[0]) : NULL, *u = r && s[1] ? strchr(suits, s[1]) : NULL;
    if (!r || !u) return 0;
    *card = CARD(u - suits, r - ranks + 1);
    return 2;
}

int gin_script(gngame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int act = 0, card = 0;
        if (c == '#') {                         /* plain moves to the end of the hand */
            int end = g->pos.phase == GN_END;
            while (g->pos.phase != GN_OVER && (end ? g->pos.phase == GN_END : g->pos.phase != GN_END))
                if (!gin_play(g, gin_auto(&g->pos))) return 0;
            if (end)
                while (g->pos.phase != GN_OVER && g->pos.phase != GN_END)
                    if (!gin_play(g, gin_auto(&g->pos))) return 0;
            continue;
        }
        switch (c) {
        case 's': act = GNA_STOCK; break;
        case 't': act = GNA_TAKE; break;
        case 'p': act = GNA_PASS; break;
        case 'n': act = GNA_NEXT; break;
        case 'd':
        case 'k': {
            int k = gin_card_name(s, &card);
            if (!k) return 0;
            s += k;
            act = (c == 'd' ? GNA_DISCARD : GNA_KNOCK) | card;
            break;
        }
        default: return 0;
        }
        if (!gin_play(g, act)) return 0;
    }
    return 1;
}
