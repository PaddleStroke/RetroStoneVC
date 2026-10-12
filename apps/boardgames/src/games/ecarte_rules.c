/*
 * Écarté: the rules (ecarte.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/ecarte.h"
#include "games/tricks.h"

int ecarte_rank(int card)
{
    static const int8_t r[14] = {0, 5, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8};   /* ace between jack and 10 */
    int k = CARD_RANK(card);
    return k <= 13 ? r[k] : 0;
}

static int key(int card) { return trick_suit_order(CARD_SUIT(card)) * 16 + ecarte_rank(card); }

void ecarte_sort(uint8_t *hand, int n)
{
    for (int i = 1; i < n; i++) {
        uint8_t c = hand[i];
        int j = i;
        while (j > 0 && key(hand[j - 1]) > key(c)) { hand[j] = hand[j - 1]; j--; }
        hand[j] = c;
    }
}

static int8_t clamp_total(int v) { return (int8_t)(v > 99 ? 99 : v); }

static void check_over(ecarte_pos *p)
{
    if (p->total[0] >= ECARTE_GOAL || p->total[1] >= ECARTE_GOAL) p->phase = EC_OVER;
}

static void deal(ecarte_pos *p)
{
    uint8_t deck[CARDS_MAX];
    int k = cards_pack(PACK_32, deck);
    cards_shuffle(deck, k, trick_mix(p->seed, 4000u + p->deal));
    p->dealer = (uint8_t)((p->deal + 1) % ECARTE_P);
    p->elder = (uint8_t)(1 - p->dealer);
    for (int pl = 0; pl < ECARTE_P; pl++) {
        p->n[pl] = ECARTE_HAND;
        for (int i = 0; i < ECARTE_HAND; i++) p->hand[pl][i] = deck[(pl == p->elder ? 0 : 5) + i];
        ecarte_sort(p->hand[pl], ECARTE_HAND);
        p->won[pl] = p->king[pl] = p->drew[pl] = 0;
        memset(p->got[pl], CARD_NONE, ECARTE_HAND);
        p->pts[pl] = p->score[pl] = 0;
    }
    p->turned = deck[10];
    p->trump = (uint8_t)CARD_SUIT(p->turned);
    p->nstock = ECARTE_STOCK;
    for (int i = 0; i < ECARTE_STOCK; i++) p->stock[i] = deck[11 + i];
    p->thrown = p->asked = p->refused = p->forced = 0;
    p->ntrick = p->tricks = 0;
    memset(p->last, CARD_NONE, sizeof p->last);
    p->last_winner = p->last_leader = TRICK_NONE;
    p->phase = EC_PROPOSE;
    p->turn = p->leader = p->elder;
    if (CARD_RANK(p->turned) == RANK_KING) {    /* a king turned: the dealer's point */
        p->king[p->dealer] = 1;
        p->score[p->dealer] = 1;
        p->total[p->dealer] = clamp_total(p->total[p->dealer] + 1);
        check_over(p);
    }
}

void ecarte_new(ecarte_game *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    deal(&g->pos);
}

int ecarte_throw_max(const ecarte_pos *p)
{
    if (p->phase != EC_DISCARD) return 0;
    return p->nstock < ECARTE_HAND ? p->nstock : ECARTE_HAND;
}

int ecarte_throw_min(const ecarte_pos *p) { return p->phase == EC_DISCARD && p->turn == p->elder ? 1 : 0; }

int ecarte_trick_winner(const uint8_t *t, int trump)
{
    int s0 = CARD_SUIT(t[0]), s1 = CARD_SUIT(t[1]);
    if (s1 == s0) return ecarte_rank(t[1]) > ecarte_rank(t[0]);
    return s1 == trump;
}

int ecarte_can_play(const ecarte_pos *p, int card)
{
    if (p->phase != EC_PLAY && !(p->phase == EC_PROPOSE && p->turn == p->elder)) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    if (trick_find(h, n, card) < 0) return 0;
    if (p->ntrick == 0) return 1;
    int led = CARD_SUIT(p->trick[0]), s = CARD_SUIT(card);
    if (trick_has_suit(h, n, led)) {            /* follow, and beat the card led if possible */
        if (s != led) return 0;
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(h[i]) == led && ecarte_rank(h[i]) > ecarte_rank(p->trick[0]))
                return ecarte_rank(card) > ecarte_rank(p->trick[0]);
        return 1;
    }
    if (trick_has_suit(h, n, p->trump)) return s == p->trump;   /* trump */
    return 1;
}

int ecarte_legal(const ecarte_pos *p, int act)
{
    int arg = ECA_ARG(act);
    switch (ECA_KIND(act)) {
    case ECA_ASK: return p->phase == EC_PROPOSE && arg == 0 && p->nstock > 0;
    case ECA_YES:
    case ECA_NO: return p->phase == EC_ANSWER && arg == 0;
    case ECA_DISCARD: {
        if (p->phase != EC_DISCARD || arg >> p->n[p->turn]) return 0;
        int k = 0;
        for (int i = 0; i < ECARTE_HAND; i++) k += arg >> i & 1;
        return k >= ecarte_throw_min(p) && k <= ecarte_throw_max(p);
    }
    case ECA_PLAY: return ecarte_can_play(p, arg);
    case ECA_NEXT: return p->phase == EC_ROUND && arg == 0;
    default: return 0;
    }
}

static void start_play(ecarte_pos *p)
{
    p->phase = EC_PLAY;
    p->turn = p->leader = p->elder;
    int k = CARD(p->trump, RANK_KING);
    for (int pl = 0; pl < ECARTE_P; pl++)
        if (trick_find(p->hand[pl], p->n[pl], k) >= 0) {   /* the king of trumps held: 1 */
            p->king[pl] = 1;
            p->score[pl] = (int8_t)(p->score[pl] + 1);
            p->total[pl] = clamp_total(p->total[pl] + 1);
        }
    check_over(p);
}

static void deal_end(ecarte_pos *p)
{
    int w = p->won[0] >= 3 ? 0 : 1, v = p->won[w] == ECARTE_HAND ? 2 : 1;
    if (w == p->dealer && p->forced) v = 2;     /* the elder played without proposing, and lost */
    if (w == p->elder && p->refused) v = 2;     /* the dealer refused, and lost */
    p->pts[w] = (int8_t)v;
    p->score[w] = (int8_t)(p->score[w] + v);
    p->total[w] = clamp_total(p->total[w] + v);
    p->deal++;
    p->phase = EC_ROUND;
    p->turn = (uint8_t)(p->deal % ECARTE_P);    /* the next deal's elder */
    check_over(p);
}

static void apply(ecarte_pos *p, int act)
{
    int pl = p->turn, arg = ECA_ARG(act);
    switch (ECA_KIND(act)) {
    case ECA_ASK:
        p->asked++;
        p->phase = EC_ANSWER;
        p->turn = p->dealer;
        break;
    case ECA_YES:
        p->phase = EC_DISCARD;
        p->turn = p->elder;
        break;
    case ECA_NO:
        if (p->asked == 1) p->refused = 1;
        start_play(p);
        break;
    case ECA_DISCARD: {
        uint8_t keep[ECARTE_HAND];
        int k = 0, out = 0;
        for (int i = 0; i < p->n[pl]; i++) {
            if (arg >> i & 1) out++;
            else keep[k++] = p->hand[pl][i];
        }
        memset(p->got[pl], CARD_NONE, ECARTE_HAND);
        for (int i = 0; i < out; i++) p->got[pl][i] = keep[k++] = p->stock[--p->nstock];
        memcpy(p->hand[pl], keep, ECARTE_HAND);
        ecarte_sort(p->hand[pl], ECARTE_HAND);
        p->drew[pl] = (uint8_t)out;
        p->thrown = (uint8_t)(p->thrown + out);
        if (pl == p->elder) p->turn = p->dealer;
        else if (p->nstock > 0) p->phase = EC_PROPOSE, p->turn = p->elder;
        else start_play(p);
        break;
    }
    case ECA_PLAY:
        if (p->phase == EC_PROPOSE) {           /* the elder plays at once */
            if (p->asked == 0) p->forced = 1;
            start_play(p);
            if (p->phase == EC_OVER) break;     /* a king's point ended the game */
        }
        trick_remove(p->hand[pl], &p->n[pl], arg);
        p->trick[p->ntrick++] = (uint8_t)arg;
        if (p->ntrick < ECARTE_P) { p->turn = (uint8_t)(1 - pl); break; }
        {
            int w = (p->leader + ecarte_trick_winner(p->trick, p->trump)) % ECARTE_P;
            memcpy(p->last, p->trick, ECARTE_P);
            p->won[w]++;
            p->last_leader = p->leader;
            p->last_winner = (uint8_t)w;
            p->ntrick = 0;
            p->tricks++;
            p->turn = p->leader = (uint8_t)w;
            if (p->tricks == ECARTE_HAND) deal_end(p);
        }
        break;
    case ECA_NEXT:
        deal(p);
        break;
    }
}

int ecarte_play(ecarte_game *g, int act)
{
    if (g->n >= ECARTE_MAX_ACTS || !ecarte_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int ecarte_undo(ecarte_game *g)
{
    if (!g->n) return 0;
    static uint16_t acts[ECARTE_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    ecarte_new(g, g->seed);
    for (int i = 0; i < n; i++) ecarte_play(g, acts[i]);
    return 1;
}

int ecarte_winner(const ecarte_pos *p)
{
    if (p->phase != EC_OVER) return -1;
    return p->total[1] > p->total[0];
}

int ecarte_auto(const ecarte_pos *p)
{
    const uint8_t *h = p->hand[p->turn];
    switch (p->phase) {
    case EC_PROPOSE:
        if (p->asked == 0 && p->nstock > 0) return ECA_ASK;
        /* fall through */
    case EC_PLAY:
        for (int i = 0; i < p->n[p->turn]; i++)
            if (ecarte_can_play(p, h[i])) return ECA_PLAY | h[i];
        return 0;
    case EC_ANSWER: return p->asked == 1 ? ECA_YES : ECA_NO;
    case EC_DISCARD: return ECA_DISCARD | (ecarte_throw_min(p) ? 1 : 0);   /* the elder throws the lowest card */
    case EC_ROUND: return ECA_NEXT;
    default: return 0;
    }
}

int ecarte_script(ecarte_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (g->pos.phase != EC_OVER && (all || g->pos.phase != EC_ROUND))
                if (!ecarte_play(g, ecarte_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; act = ecarte_auto(&g->pos); }
        else if (*s == 'n') { s++; act = ECA_NEXT; }
        else if (*s == 'q') { s++; act = ECA_ASK; }
        else if (*s == 'y') { s++; act = ECA_YES; }
        else if (*s == 'x') { s++; act = ECA_NO; }
        else if (*s == 'e') {
            s++;
            int m = 0;
            const ecarte_pos *p = &g->pos;
            while (*s && *s != ' ') {
                int c = trick_parse(&s), i = c == CARD_NONE ? -1 : trick_find(p->hand[p->turn], p->n[p->turn], c);
                if (i < 0) return 0;
                m |= 1 << i;
            }
            act = ECA_DISCARD | m;
        } else {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            act = ECA_PLAY | c;
        }
        if (!ecarte_play(g, act)) return 0;
    }
    return 1;
}
