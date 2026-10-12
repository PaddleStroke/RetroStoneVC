/*
 * Scopa: the rules (scopa.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/scopa.h"

int scopa_value(int card) { return CARD_RANK(card); }

int scopa_primiera_value(int card)
{
    static const uint8_t v[11] = {0, 16, 12, 13, 14, 15, 18, 21, 10, 10, 10};
    int r = CARD_RANK(card);
    return r >= 1 && r <= IT_RE ? v[r] : 0;
}

int scopa_side(const scpos *p, int player) { return p->players == 4 ? player & 1 : player; }

/* 3 cards to each hand from the end of the stock, from the player after the dealer */
static void deal_hands(scpos *p)
{
    for (int k = 0; k < p->players; k++) {
        int pl = (p->dealer + 1 + k) % p->players;
        for (int i = 0; i < SCOPA_HAND; i++) p->hand[pl][i] = p->stock[--p->nstock];
        p->n[pl] = SCOPA_HAND;
    }
    p->deals++;
    p->turn = (uint8_t)((p->dealer + 1) % p->players);
}

static void deal_round(scpos *p)
{
    p->dealer = (uint8_t)((p->round + p->players - 1) % p->players);
    for (uint32_t k = 0;; k++) {                /* three or four re on the table: dealt again */
        p->nstock = (uint8_t)cards_pack(PACK_40, p->stock);
        cards_shuffle(p->stock, p->nstock, it_mix(p->seed, 1000u * p->round + k));
        int re = 0;
        for (int i = 0; i < 4; i++) re += CARD_RANK(p->stock[p->nstock - 1 - p->players * SCOPA_HAND - i]) == IT_RE;
        if (re < 3) break;
    }
    p->deals = 0;
    deal_hands(p);
    p->ntable = 0;
    for (int i = 0; i < 4; i++) p->table[p->ntable++] = p->stock[--p->nstock];
    for (int pl = 0; pl < SCOPA_MAXP; pl++) p->ntook[pl] = p->scope[pl] = 0;
    p->last_taker = SCOPA_NONE;
    p->lp = SCOPA_NONE;
    p->nltook = p->nswept = p->lscopa = 0;
    p->sweeper = SCOPA_NONE;
    p->phase = SC_PLAY;
}

void scopa_new(scgame *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->players = (uint8_t)(players == 4 ? 4 : 2);
    g->seed = seed;
    scpos *p = &g->pos;
    p->players = g->players;
    p->seed = seed;
    deal_round(p);
}

/* the subsets of the table (at least two cards) adding up to v, from card i on; into masks */
static void sums(const scpos *p, int i, int v, uint32_t mask, int count, uint32_t *masks, int max, int *n)
{
    if (v == 0) {
        if (count >= 2 && *n < max) masks[(*n)++] = mask;
        return;
    }
    for (int k = i; k < p->ntable; k++) {
        int c = scopa_value(p->table[k]);
        if (c <= v) sums(p, k + 1, v - c, mask | 1u << k, count + 1, masks, max, n);
    }
}

int scopa_captures(const scpos *p, int card, uint32_t *masks, int max)
{
    int n = 0, v = scopa_value(card);
    for (int k = 0; k < p->ntable; k++)          /* a single card of its value comes first */
        if (scopa_value(p->table[k]) == v && n < max) masks[n++] = 1u << k;
    if (n) return n;
    sums(p, 0, v, 0, 0, masks, max, &n);
    return n;
}

static int in_hand(const scpos *p, int card)
{
    for (int i = 0; i < p->n[p->turn]; i++)
        if (p->hand[p->turn][i] == card) return i;
    return -1;
}

int scopa_legal(const scpos *p, uint32_t act)
{
    if (act == SCA_NEXT) return p->phase == SC_ROUND;
    if (p->phase != SC_PLAY || in_hand(p, SCA_CARD(act)) < 0) return 0;
    uint32_t masks[64];
    int n = scopa_captures(p, SCA_CARD(act), masks, 64);
    if (!n) return SCA_TAKE(act) == 0;
    for (int i = 0; i < n; i++)
        if (masks[i] == SCA_TAKE(act)) return 1;
    return 0;
}

int scopa_primiera(const scpos *p, int side)
{
    int best[4] = {0, 0, 0, 0};
    for (int pl = 0; pl < p->players; pl++) {
        if (scopa_side(p, pl) != side) continue;
        for (int i = 0; i < p->ntook[pl]; i++) {
            int c = p->took[pl][i], v = scopa_primiera_value(c);
            if (v > best[CARD_SUIT(c) & 3]) best[CARD_SUIT(c) & 3] = v;
        }
    }
    if (!best[0] || !best[1] || !best[2] || !best[3]) return 0;
    return best[0] + best[1] + best[2] + best[3];
}

static void score_round(scpos *p)
{
    memset(p->count, 0, sizeof p->count);
    memset(p->won, 0, sizeof p->won);
    memset(p->rscope, 0, sizeof p->rscope);
    for (int pl = 0; pl < p->players; pl++) {
        int s = scopa_side(p, pl);
        p->rscope[s] += p->scope[pl];
        for (int i = 0; i < p->ntook[pl]; i++) {
            int c = p->took[pl][i];
            p->count[s][SCORE_CARDS]++;
            if (CARD_SUIT(c) == IT_COINS) p->count[s][SCORE_COINS]++;
            if (c == CARD(IT_COINS, 7)) p->count[s][SCORE_SETTEBELLO] = 1;
        }
    }
    for (int s = 0; s < 2; s++) p->count[s][SCORE_PRIMIERA] = (uint8_t)scopa_primiera(p, s);
    for (int k = 0; k < SCORE_KINDS; k++)
        if (p->count[0][k] != p->count[1][k]) p->won[p->count[1][k] > p->count[0][k]][k] = 1;
    for (int s = 0; s < 2; s++) {
        p->pts[s] = p->rscope[s];
        for (int k = 0; k < SCORE_KINDS; k++) p->pts[s] += p->won[s][k];
        p->score[s] += p->pts[s];
    }
    int top = p->score[0] >= SCOPA_TARGET || p->score[1] >= SCOPA_TARGET;
    p->phase = top && p->score[0] != p->score[1] ? SC_OVER : SC_ROUND;
}

static void apply(scpos *p, uint32_t act)
{
    if (act == SCA_NEXT) {
        p->round++;
        deal_round(p);
        return;
    }
    int pl = p->turn, card = SCA_CARD(act), i = in_hand(p, card);
    uint32_t take = SCA_TAKE(act);
    for (; i < p->n[pl] - 1; i++) p->hand[pl][i] = p->hand[pl][i + 1];
    p->n[pl]--;
    p->lp = (uint8_t)pl;
    p->lcard = (uint8_t)card;
    p->nltook = p->lscopa = p->nswept = 0;
    p->sweeper = SCOPA_NONE;
    int last = !p->nstock;                      /* the very last card of the round: no scopa with it */
    for (int q = 0; q < p->players; q++) last &= p->n[q] == 0;
    if (take) {
        int k = 0;
        for (int t = 0; t < p->ntable; t++) {
            if (take >> t & 1) p->ltook[p->nltook++] = p->table[t];
            else p->table[k++] = p->table[t];
        }
        p->ntable = (uint8_t)k;
        p->took[pl][p->ntook[pl]++] = (uint8_t)card;
        for (int t = 0; t < p->nltook; t++) p->took[pl][p->ntook[pl]++] = p->ltook[t];
        p->last_taker = (uint8_t)pl;
        if (!p->ntable && !last) {
            p->scopa_card[pl][p->scope[pl]++] = (uint8_t)card;
            p->lscopa = 1;
        }
    } else p->table[p->ntable++] = (uint8_t)card;
    p->turn = (uint8_t)((pl + 1) % p->players);
    int empty = 1;
    for (int q = 0; q < p->players; q++) empty &= p->n[q] == 0;
    if (!empty) return;
    if (p->nstock) { deal_hands(p); return; }
    if (p->last_taker != SCOPA_NONE) {          /* the table's last cards to the side that took last */
        int q = p->last_taker;
        for (int t = 0; t < p->ntable; t++) p->swept[p->nswept++] = p->took[q][p->ntook[q]++] = p->table[t];
        p->sweeper = (uint8_t)q;
        p->ntable = 0;
    }
    score_round(p);
}

int scopa_play(scgame *g, uint32_t act)
{
    if (!scopa_legal(&g->pos, act) || g->n >= SCOPA_MAX_ACTS) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = act;
    return 1;
}

int scopa_undo(scgame *g)
{
    if (!g->n) return 0;
    static uint32_t acts[SCOPA_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    scopa_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) scopa_play(g, acts[i]);
    return 1;
}

int scopa_winner(const scpos *p)
{
    if (p->phase != SC_OVER) return -1;
    return p->score[1] > p->score[0];
}

uint32_t scopa_auto(const scpos *p)
{
    if (p->phase == SC_ROUND) return SCA_NEXT;
    if (p->phase != SC_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    uint32_t m;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (scopa_captures(p, h[i], &m, 1)) return SCA(h[i], m);
    return SCA(h[0], 0);
}

int scopa_script(scgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        uint32_t act;
        if (*s == '#' || *s == '!') {           /* plain actions to the end of the round (!: of the game) */
            int all = *s++ == '!';
            while (g->pos.phase != SC_OVER && (all || g->pos.phase == SC_PLAY))
                if (!scopa_play(g, scopa_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; act = scopa_auto(&g->pos); }
        else if (*s == 'n') { s++; act = SCA_NEXT; }
        else {
            int c = it_parse(&s);
            if (c == CARD_NONE) return 0;
            uint32_t take = 0;
            if (*s == ':') {                    /* the cards it takes, named */
                s++;
                while (*s && *s != ' ') {
                    int t = it_parse(&s), k = 0;
                    if (t == CARD_NONE) return 0;
                    while (k < g->pos.ntable && g->pos.table[k] != t) k++;
                    if (k == g->pos.ntable) return 0;
                    take |= 1u << k;
                }
            } else {
                uint32_t m;
                if (scopa_captures(&g->pos, c, &m, 1)) take = m;
            }
            act = SCA(c, take);
        }
        if (!scopa_play(g, act)) return 0;
    }
    return 1;
}
