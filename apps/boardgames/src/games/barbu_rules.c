/*
 * Barbu: the rules (barbu.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/barbu.h"
#include "games/tricks.h"

static const int8_t DOMINO_PTS[BARBU_P] = {45, 20, 5, -5};

int barbu_declarer_of(int variant, int deal) { return BARBU_IS_SHORT(variant) ? deal % BARBU_P : deal / 7; }

static void deal_cards(barbu_pos *p)
{
    uint8_t deck[CARDS_MAX];
    int k = cards_pack(PACK_52, deck);
    cards_shuffle(deck, k, trick_mix(p->seed, 3000u + p->deal));
    p->declarer = (uint8_t)barbu_declarer_of(p->variant, p->deal);
    for (int pl = 0; pl < BARBU_P; pl++) {
        p->n[pl] = 0, p->won[pl] = 0, p->pts[pl] = 0;
        memset(p->dbl[pl], 0, sizeof p->dbl[pl]);
    }
    for (int i = 0; i < k; i++) {
        int pl = (p->declarer + 1 + i) % BARBU_P;
        p->hand[pl][p->n[pl]++] = deck[i];
    }
    for (int pl = 0; pl < BARBU_P; pl++) trick_sort(p->hand[pl], p->n[pl]);
    memset(p->last, CARD_NONE, sizeof p->last);
    memset(p->lo, 0, sizeof p->lo);
    memset(p->hi, 0, sizeof p->hi);
    p->ntrick = p->tricks = p->taken = p->nout = 0;
    p->last_winner = p->last_leader = TRICK_NONE;
    p->contract = 255, p->trump = 255, p->rank = 0;
    p->phase = BBP_CHOOSE;
    p->turn = p->leader = p->declarer;
}

void barbu_new(barbu_game *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    barbu_pos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = p->variant = (uint8_t)(variant < 0 || variant >= BARBU_VARIANTS ? 0 : variant);
    p->deals = BARBU_IS_SHORT(p->variant) ? BARBU_CONTRACTS : BARBU_P * BARBU_CONTRACTS;
    deal_cards(p);
}

int barbu_can_choose(const barbu_pos *p, int c)
{
    if (p->phase != BBP_CHOOSE || c < 0 || c >= BARBU_CONTRACTS) return 0;
    return !((BARBU_IS_SHORT(p->variant) ? p->used : p->chosen[p->declarer]) >> c & 1);
}

int barbu_card_points(int contract, int card)
{
    switch (contract) {
    case BB_NOHEARTS: return CARD_SUIT(card) != SUIT_HEARTS ? 0 : CARD_RANK(card) == RANK_ACE ? -6 : -2;
    case BB_NOQUEENS: return CARD_RANK(card) == RANK_QUEEN ? -6 : 0;
    case BB_KING: return card == BARBU_KING ? -20 : 0;
    default: return 0;
    }
}

/* ---- dominoes ---------------------------------------------------------------------------------------------------- */
static int lays(const barbu_pos *p, int card)
{
    int s = CARD_SUIT(card), r = trick_rank(card);
    if (!p->lo[s]) return r == p->rank;
    return r == p->lo[s] - 1 || r == p->hi[s] + 1;
}

int barbu_must_pass(const barbu_pos *p)
{
    if (p->phase != BBP_PLAY || p->contract != BB_DOMINO) return 0;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (lays(p, p->hand[p->turn][i])) return 0;
    return 1;
}

/* ---- tricks ------------------------------------------------------------------------------------------------------ */
static int best_trump(const barbu_pos *p)       /* the best trump in the trick (0: none) */
{
    int b = 0;
    for (int i = 0; i < p->ntrick; i++)
        if (CARD_SUIT(p->trick[i]) == p->trump && trick_rank(p->trick[i]) > b) b = trick_rank(p->trick[i]);
    return b;
}

static int holds_over(const uint8_t *h, int n, int suit, int over)   /* a card of the suit above `over` */
{
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == suit && trick_rank(h[i]) > over) return 1;
    return 0;
}

int barbu_can_play(const barbu_pos *p, int card)
{
    if (p->phase != BBP_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn], s = CARD_SUIT(card);
    if (trick_find(h, n, card) < 0) return 0;
    if (p->contract == BB_DOMINO) return lays(p, card);
    if (p->ntrick == 0) {                       /* leading */
        if ((p->contract == BB_NOHEARTS || p->contract == BB_KING) && s == SUIT_HEARTS)
            return trick_only_suit(h, n, SUIT_HEARTS);
        return 1;
    }
    int led = CARD_SUIT(p->trick[0]);
    if (trick_has_suit(h, n, led)) {
        if (s != led) return 0;
        if (p->contract == BB_TRUMPS && led == p->trump) {   /* trumps led: beat the best one if possible */
            int b = best_trump(p);
            return !holds_over(h, n, led, b) || trick_rank(card) > b;
        }
        return 1;
    }
    if (p->contract == BB_KING && trick_find(h, n, BARBU_KING) >= 0) return card == BARBU_KING;
    if (p->contract == BB_TRUMPS && trick_has_suit(h, n, p->trump)) {
        if (s != p->trump) return 0;
        int b = best_trump(p);
        return !holds_over(h, n, s, b) || trick_rank(card) > b;
    }
    return 1;
}

unsigned barbu_double_mask(const barbu_pos *p)
{
    int me = p->turn;
    unsigned m = 0;
    if (p->phase == BBP_DOUBLE) {
        for (int q = 0; q < BARBU_P; q++) {
            if (q == me || p->dbl[q][me]) continue;
            if (p->contract >= BB_TRUMPS && q != p->declarer) continue;
            m |= 1u << q;
        }
    } else if (p->phase == BBP_REDOUBLE) {
        for (int q = 0; q < BARBU_P; q++)
            if (p->dbl[q][me] == 1) m |= 1u << q;
    }
    return m;
}

int barbu_legal(const barbu_pos *p, int act)
{
    int arg = BBA_ARG(act);
    switch (BBA_KIND(act)) {
    case BBA_CHOOSE: {
        int c = arg & 15, x = arg >> 4;
        if (!barbu_can_choose(p, c)) return 0;
        if (c == BB_TRUMPS) return x < 4;
        if (c == BB_DOMINO) return x >= 2 && x <= 14;
        return x == 0;
    }
    case BBA_DOUBLE:
    case BBA_REDOUBLE:
        if (p->phase != (BBA_KIND(act) == BBA_DOUBLE ? BBP_DOUBLE : BBP_REDOUBLE)) return 0;
        return (arg & ~barbu_double_mask(p)) == 0;
    case BBA_PLAY: return arg < 256 && barbu_can_play(p, arg);
    case BBA_PASS: return arg == 0 && barbu_must_pass(p);
    case BBA_NEXT: return p->phase == BBP_ROUND && act == BBA_NEXT;
    default: return 0;
    }
}

/* the deal is over: the doubles settled, the totals and the table, the next deal */
static void deal_end(barbu_pos *p)
{
    for (int q = 0; q < BARBU_P; q++) p->settle[q] = 0;
    for (int a = 0; a < BARBU_P; a++)
        for (int b = 0; b < BARBU_P; b++) {
            if (!p->dbl[a][b]) continue;
            int d = (p->pts[a] - p->pts[b]) * p->dbl[a][b];
            p->settle[a] = (int16_t)(p->settle[a] + d);
            p->settle[b] = (int16_t)(p->settle[b] - d);
        }
    for (int q = 0; q < BARBU_P; q++) {
        p->score[q] = (int16_t)(p->pts[q] + p->settle[q]);
        p->total[q] = (int16_t)(p->total[q] + p->score[q]);
        p->table[q][p->contract] = (int16_t)(p->table[q][p->contract] + p->score[q]);
    }
    p->chosen[p->declarer] |= (uint8_t)(1u << p->contract);
    p->used |= (uint8_t)(1u << p->contract);
    p->deal++;
    p->turn = (uint8_t)barbu_declarer_of(p->variant, p->deal < p->deals ? p->deal : 0);
    p->phase = p->deal >= p->deals ? BBP_OVER : BBP_ROUND;
}

static int done(const barbu_pos *p)             /* every penalty card is out, or every trick played */
{
    if (p->tricks == BARBU_HAND) return 1;
    switch (p->contract) {
    case BB_NOHEARTS: return p->taken == 13;
    case BB_NOQUEENS: return p->taken == 4;
    case BB_KING: return p->taken == 1;
    default: return 0;
    }
}

static void trick_done(barbu_pos *p)
{
    int w = (p->leader + trick_winner(p->trick, BARBU_P, p->contract == BB_TRUMPS ? p->trump : -1)) % BARBU_P;
    memcpy(p->last, p->trick, BARBU_P);
    p->won[w]++;
    p->tricks++;
    int pts = 0;
    for (int i = 0; i < BARBU_P; i++) {
        int v = barbu_card_points(p->contract, p->trick[i]);
        pts += v;
        p->taken = (uint8_t)(p->taken + (v != 0));
    }
    if (p->contract == BB_NOTRICKS) pts = -2;
    if (p->contract == BB_TRUMPS) pts = 5;
    if (p->contract == BB_LASTTWO) pts = p->tricks == 12 ? -10 : p->tricks == 13 ? -20 : 0;
    p->pts[w] = (int16_t)(p->pts[w] + pts);
    p->last_leader = p->leader;
    p->last_winner = (uint8_t)w;
    p->ntrick = 0;
    p->turn = p->leader = (uint8_t)w;
    if (done(p)) deal_end(p);
}

static int next_in(const barbu_pos *p, int pl)  /* dominoes: the next player still in */
{
    for (int k = 1; k <= BARBU_P; k++) {
        int q = (pl + k) % BARBU_P;
        if (p->n[q]) return q;
    }
    return pl;
}

/* the doubling's next player: the others in turn from the declarer's left; then those doubled (the declarer last) */
static void next_say(barbu_pos *p)
{
    while (p->phase == BBP_DOUBLE || p->phase == BBP_REDOUBLE) {
        p->step++;
        if (p->phase == BBP_DOUBLE && p->step >= BARBU_P - 1) p->phase = BBP_REDOUBLE, p->step = 0;
        if (p->phase == BBP_REDOUBLE && p->step >= BARBU_P) break;
        p->turn = (uint8_t)((p->declarer + 1 + p->step) % BARBU_P);
        if (p->phase == BBP_DOUBLE || barbu_double_mask(p)) return;
    }
    p->phase = BBP_PLAY;
    p->turn = p->leader = p->declarer;
}

static void apply(barbu_pos *p, int act)
{
    int pl = p->turn, arg = BBA_ARG(act);
    switch (BBA_KIND(act)) {
    case BBA_CHOOSE:
        p->contract = (uint8_t)(arg & 15);
        if (p->contract == BB_TRUMPS) p->trump = (uint8_t)(arg >> 4);
        if (p->contract == BB_DOMINO) p->rank = (uint8_t)(arg >> 4);
        if (BARBU_DOUBLING(p->variant)) {
            p->phase = BBP_DOUBLE;
            p->step = 0;
            p->turn = (uint8_t)((p->declarer + 1) % BARBU_P);
        } else {
            p->phase = BBP_PLAY;
            p->turn = p->leader = p->declarer;
        }
        break;
    case BBA_DOUBLE:
        for (int q = 0; q < BARBU_P; q++)
            if (arg >> q & 1) p->dbl[pl][q] = 1;
        next_say(p);
        break;
    case BBA_REDOUBLE:
        for (int q = 0; q < BARBU_P; q++)
            if (arg >> q & 1) p->dbl[q][pl] = 2;
        next_say(p);
        break;
    case BBA_PLAY: {
        trick_remove(p->hand[pl], &p->n[pl], arg);
        if (p->contract != BB_DOMINO) {
            p->trick[p->ntrick++] = (uint8_t)arg;
            if (p->ntrick < BARBU_P) p->turn = (uint8_t)((pl + 1) % BARBU_P);
            else trick_done(p);
            break;
        }
        int s = CARD_SUIT(arg), r = trick_rank(arg);
        if (!p->lo[s]) p->lo[s] = p->hi[s] = (uint8_t)r;
        else if (r < p->lo[s]) p->lo[s] = (uint8_t)r;
        else p->hi[s] = (uint8_t)r;
        if (!p->n[pl]) {
            p->pts[pl] = DOMINO_PTS[p->nout];
            p->out[p->nout++] = (uint8_t)pl;
        }
        if (p->nout == BARBU_P - 1) {           /* the last one left */
            int last = next_in(p, pl);
            p->pts[last] = DOMINO_PTS[BARBU_P - 1];
            p->out[p->nout++] = (uint8_t)last;
            deal_end(p);
            break;
        }
        p->turn = (uint8_t)next_in(p, pl);
        break;
    }
    case BBA_PASS:
        p->turn = (uint8_t)next_in(p, pl);
        break;
    case BBA_NEXT:
        deal_cards(p);
        break;
    }
}

int barbu_play(barbu_game *g, int act)
{
    if (g->n >= BARBU_MAX_ACTS || !barbu_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int barbu_undo(barbu_game *g)
{
    if (!g->n) return 0;
    static uint16_t acts[BARBU_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    barbu_new(g, g->variant, g->seed);
    for (int i = 0; i < n; i++) barbu_play(g, acts[i]);
    return 1;
}

unsigned barbu_winners(const barbu_pos *p)
{
    if (p->phase != BBP_OVER) return 0;
    int top = -32000;
    unsigned m = 0;
    for (int q = 0; q < BARBU_P; q++) top = p->total[q] > top ? p->total[q] : top;
    for (int q = 0; q < BARBU_P; q++) m |= (unsigned)(p->total[q] == top) << q;
    return m;
}

int barbu_auto(const barbu_pos *p)
{
    const uint8_t *h = p->hand[p->turn];
    switch (p->phase) {
    case BBP_CHOOSE:
        for (int c = 0; c < BARBU_CONTRACTS; c++) {
            if (!barbu_can_choose(p, c)) continue;
            if (c == BB_DOMINO) return BBA_CONTRACT(c, RANK_JACK);
            if (c != BB_TRUMPS) return BBA_CONTRACT(c, 0);
            int best = 0, most = -1;            /* trumps: the longest suit */
            for (int s = 0; s < 4; s++) {
                int k = 0;
                for (int i = 0; i < p->n[p->turn]; i++) k += CARD_SUIT(h[i]) == s;
                if (k > most) most = k, best = s;
            }
            return BBA_CONTRACT(c, best);
        }
        return 0;
    case BBP_DOUBLE: return BBA_DOUBLE;
    case BBP_REDOUBLE: return BBA_REDOUBLE;
    case BBP_PLAY:
        for (int i = 0; i < p->n[p->turn]; i++)
            if (barbu_can_play(p, h[i])) return BBA_PLAY | h[i];
        return barbu_must_pass(p) ? BBA_PASS : 0;
    case BBP_ROUND: return BBA_NEXT;
    default: return 0;
    }
}

static int players_of(const char **s)           /* digits 0..3 after d or r: a mask */
{
    int m = 0;
    while (**s >= '0' && **s <= '3') m |= 1 << (*(*s)++ - '0');
    return m;
}

int barbu_script(barbu_game *g, const char *s)
{
    static const char ranks[] = "23456789TJQKA", suits[] = "shdc";
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {          /* plain actions to the end of the deal (!: of the game) */
            int all = *s++ == '!';
            while (g->pos.phase != BBP_OVER && (all || g->pos.phase != BBP_ROUND))
                if (!barbu_play(g, barbu_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; act = barbu_auto(&g->pos); }
        else if (*s == 'n') { s++; act = BBA_NEXT; }
        else if (*s == 'p') { s++; act = BBA_PASS; }
        else if (*s == 'd') { s++; act = BBA_DOUBLE | players_of(&s); }
        else if (*s == 'r') { s++; act = BBA_REDOUBLE | players_of(&s); }
        else if (*s == 'c') {
            s++;
            if (*s < '0' || *s > '6') return 0;
            int c = *s++ - '0', x = 0;
            if (c == BB_TRUMPS || c == BB_DOMINO) {
                const char *set = c == BB_TRUMPS ? suits : ranks, *f = *s ? strchr(set, *s) : NULL;
                if (!f) return 0;
                x = (int)(f - set) + (c == BB_DOMINO ? 2 : 0);
                s++;
            }
            act = BBA_CONTRACT(c, x);
        } else {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            act = BBA_PLAY | c;
        }
        if (!barbu_play(g, act)) return 0;
    }
    return 1;
}
