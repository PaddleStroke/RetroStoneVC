/*
 * Koi-Koi: the rules (koikoi.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/koikoi.h"

/* each month's four cards, from index 0: their types; the ribbons' kinds */
static const uint8_t TYPE[12][4] = {
    {HT_BRIGHT, HT_RIBBON, HT_PLAIN, HT_PLAIN},  {HT_ANIMAL, HT_RIBBON, HT_PLAIN, HT_PLAIN},   /* pine, plum */
    {HT_BRIGHT, HT_RIBBON, HT_PLAIN, HT_PLAIN},  {HT_ANIMAL, HT_RIBBON, HT_PLAIN, HT_PLAIN},   /* cherry, wisteria */
    {HT_ANIMAL, HT_RIBBON, HT_PLAIN, HT_PLAIN},  {HT_ANIMAL, HT_RIBBON, HT_PLAIN, HT_PLAIN},   /* iris, peony */
    {HT_ANIMAL, HT_RIBBON, HT_PLAIN, HT_PLAIN},  {HT_BRIGHT, HT_ANIMAL, HT_PLAIN, HT_PLAIN},   /* clover, pampas */
    {HT_ANIMAL, HT_RIBBON, HT_PLAIN, HT_PLAIN},  {HT_ANIMAL, HT_RIBBON, HT_PLAIN, HT_PLAIN},   /* chrysanth., maple */
    {HT_BRIGHT, HT_ANIMAL, HT_RIBBON, HT_PLAIN}, {HT_BRIGHT, HT_PLAIN, HT_PLAIN, HT_PLAIN},    /* willow, paulownia */
};
static const uint8_t RIBBON[12] = {HR_POETRY, HR_POETRY, HR_POETRY, HR_RED, HR_RED, HR_BLUE,
                                   HR_RED, HR_NONE, HR_BLUE, HR_BLUE, HR_RED, HR_NONE};

int kk_type(int c) { return c >= 0 && c < KK_CARDS ? TYPE[HANA_MONTH(c)][HANA_INDEX(c)] : HT_PLAIN; }
int kk_ribbon(int c) { return kk_type(c) == HT_RIBBON ? RIBBON[HANA_MONTH(c)] : HR_NONE; }

/* ---- the yaku ---------------------------------------------------------------------------------------------------- */
int kk_yaku(const uint8_t *cards, int n, int *pts)
{
    int y[YAKU_N] = {0}, has[KK_CARDS] = {0}, count[HTYPES] = {0}, rib[4] = {0};
    for (int i = 0; i < n; i++) {
        if (cards[i] >= KK_CARDS) continue;
        has[cards[i]] = 1;
        count[kk_type(cards[i])]++;
        rib[kk_ribbon(cards[i])]++;
    }
    int b = count[HT_BRIGHT], rain = has[KK_RAINMAN], sake = has[KK_SAKE];
    if (b == 5) y[Y_GOKO] = 10;
    else if (b == 4) y[rain ? Y_AMESHIKO : Y_SHIKO] = rain ? 7 : 8;
    else if (b == 3 && !rain) y[Y_SANKO] = 5;
    if (has[KK_BOAR] && has[KK_DEER] && has[KK_BUTTERFLIES]) y[Y_INOSHIKACHO] = 5;
    if (rib[HR_POETRY] == 3) y[Y_AKATAN] = 5;
    if (rib[HR_BLUE] == 3) y[Y_AOTAN] = 5;
    if (has[KK_CURTAIN] && sake) y[Y_HANAMI] = 5;
    if (has[KK_MOON] && sake) y[Y_TSUKIMI] = 5;
    if (count[HT_ANIMAL] >= 5) y[Y_TANE] = count[HT_ANIMAL] - 4;
    if (count[HT_RIBBON] >= 5) y[Y_TAN] = count[HT_RIBBON] - 4;
    int plains = count[HT_PLAIN] + sake;        /* the sake cup is a plain too */
    if (plains >= 10) y[Y_KASU] = plains - 9;
    int sum = 0;
    for (int i = 0; i < YAKU_N; i++) sum += y[i];
    if (pts) memcpy(pts, y, sizeof y);
    return sum;
}

int kk_instant(const uint8_t *hand, int n)
{
    int m[12] = {0}, pairs = 0;
    for (int i = 0; i < n; i++)
        if (hand[i] < KK_CARDS) m[HANA_MONTH(hand[i])]++;
    for (int i = 0; i < 12; i++) {
        if (m[i] == 4) return Y_TESHI;
        pairs += m[i] == 2;
    }
    return pairs == 4 ? Y_KUTTSUKI : -1;
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static void sort(uint8_t *c, int n)
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && c[j - 1] > c[j]; j--) {
            uint8_t t = c[j];
            c[j] = c[j - 1];
            c[j - 1] = t;
        }
}

static void round_end(kpos *p, int kind, int winner, int instant);

static void deal(kpos *p)
{
    for (;;) {
        for (int i = 0; i < KK_CARDS; i++) p->deck[i] = (uint8_t)i;
        cards_shuffle(p->deck, KK_CARDS, mix(p->seed, 1000u + 97u * p->round + p->redeals));
        int n = KK_CARDS, d = p->dealer, o = 1 - d;
        memset(p->field, KK_NONE, sizeof p->field);
        for (int k = 0; k < 4; k++) {           /* two cards at a time: the dealer's hand, the other's, the field */
            for (int j = 0; j < 2; j++) p->hand[d][2 * k + j] = p->deck[--n];
            for (int j = 0; j < 2; j++) p->hand[o][2 * k + j] = p->deck[--n];
            for (int j = 0; j < 2; j++) p->field[2 * k + j] = p->deck[--n];
        }
        p->ndeck = (uint8_t)n;
        int m[12] = {0}, four = 0;
        for (int i = 0; i < 8; i++) four |= ++m[HANA_MONTH(p->field[i])] == 4;
        if (!four) break;
        p->redeals++;                           /* four of a month on the field: dealt again */
    }
    p->nh[0] = p->nh[1] = KK_HAND;
    sort(p->hand[0], KK_HAND);
    sort(p->hand[1], KK_HAND);
    p->ncap[0] = p->ncap[1] = 0;
    p->koi[0] = p->koi[1] = p->base[0] = p->base[1] = 0;
    p->held = p->drawn = KK_NONE;
    p->from_deck = 0;
    p->turn = p->dealer;
    p->phase = KP_PLAY;
    memset(&p->res, 0, sizeof p->res);
    p->res.winner = KK_NONE;
    for (int k = 0; k < 2; k++) {               /* teshi, kuttsuki: the dealer first */
        int pl = k ? 1 - p->dealer : p->dealer, y = kk_instant(p->hand[pl], KK_HAND);
        if (y >= 0) { round_end(p, KR_INSTANT, pl, y); return; }
    }
}

void kk_new(kgame *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    kpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = (uint8_t)(variant == KK_HALF ? KK_HALF : KK_FULL);
    p->rounds = g->variant == KK_HALF ? 6 : 12;
    p->dealer = 0;
    deal(p);
}

/* ---- playing ----------------------------------------------------------------------------------------------------- */
int kk_matches(const kpos *p, int card, int *slots)
{
    int n = 0;
    for (int s = 0; s < KK_FIELD; s++)
        if (p->field[s] != KK_NONE && HANA_MONTH(p->field[s]) == HANA_MONTH(card)) {
            if (slots) slots[n] = s;
            n++;
        }
    return n;
}

int kk_points(const kpos *p, int pl) { return kk_yaku(p->cap[pl], p->ncap[pl], NULL); }

static int in_hand(const kpos *p, int pl, int card)
{
    for (int i = 0; i < p->nh[pl]; i++)
        if (p->hand[pl][i] == card) return i;
    return -1;
}

int kk_legal(const kpos *p, int act)
{
    int k = KA_KIND(act), c = KA_CARD(act);
    switch (p->phase) {
    case KP_PLAY: return k == KA_PLAY && c < KK_CARDS && in_hand(p, p->turn, c) >= 0;
    case KP_CHOOSE:
        if (k != KA_TAKE || c >= KK_CARDS || HANA_MONTH(c) != HANA_MONTH(p->held)) return 0;
        for (int s = 0; s < KK_FIELD; s++)
            if (p->field[s] == c) return 1;
        return 0;
    case KP_DRAW: return k == KA_DRAW && p->ndeck > 0;
    case KP_DECIDE: return k == KA_KOI || k == KA_STOP;
    case KP_ROUND: return k == KA_NEXT;
    default: return 0;
    }
}

static void capture(kpos *p, int pl, int card) { p->cap[pl][p->ncap[pl]++] = (uint8_t)card; }

static void round_end(kpos *p, int kind, int winner, int instant)
{
    kk_result *r = &p->res;
    memset(r, 0, sizeof *r);
    r->kind = (uint8_t)kind;
    r->winner = (uint8_t)(kind == KR_DRAW ? KK_NONE : winner);
    if (kind == KR_STOP) {
        int y[YAKU_N];
        int sum = kk_yaku(p->cap[winner], p->ncap[winner], y);
        for (int i = 0; i < YAKU_N; i++) r->pts[i] = (int8_t)y[i];
        r->base = (uint8_t)sum;
        r->seven = sum >= 7;
        r->koi = p->koi[1 - winner] > 0;
        r->points = (int16_t)(sum << r->seven << r->koi);
    } else if (kind == KR_INSTANT) {
        r->pts[instant] = KK_INSTANT;
        r->base = KK_INSTANT;
        r->points = KK_INSTANT;
    }
    if (r->winner != KK_NONE) {
        p->score[winner] = (int16_t)(p->score[winner] + r->points);
        p->dealer = (uint8_t)winner;            /* the winner deals the next round; a drawn round: the same dealer */
    }
    p->held = KK_NONE;
    p->phase = p->round + 1 >= p->rounds ? KP_OVER : KP_ROUND;
}

static void next_turn(kpos *p)
{
    if (!p->nh[0] && !p->nh[1]) { round_end(p, KR_DRAW, 0, 0); return; }
    p->turn = (uint8_t)(1 - p->turn);
    p->phase = KP_PLAY;
}

/* the end of a turn: a yaku formed or raised: stop or koi-koi (no card left: stop) */
static void end_turn(kpos *p)
{
    int pl = p->turn, y = kk_points(p, pl);
    if (y > p->base[pl]) {
        if (!p->nh[pl]) round_end(p, KR_STOP, pl, 0);
        else p->phase = KP_DECIDE;
        return;
    }
    next_turn(p);
}

/* a card from the hand or the stock meets the field */
static void meet(kpos *p, int card, int from_deck)
{
    int s[4], n = kk_matches(p, card, s), pl = p->turn;
    if (n == 2) {                               /* a choice */
        p->held = (uint8_t)card;
        p->from_deck = (uint8_t)from_deck;
        p->phase = KP_CHOOSE;
        return;
    }
    if (n == 0) {                               /* it stays: the first empty place */
        for (int i = 0; i < KK_FIELD; i++)
            if (p->field[i] == KK_NONE) { p->field[i] = (uint8_t)card; break; }
    } else {                                    /* one, or three: all of them */
        capture(p, pl, card);
        for (int i = 0; i < n; i++) {
            capture(p, pl, p->field[s[i]]);
            p->field[s[i]] = KK_NONE;
        }
    }
    if (from_deck) end_turn(p);
    else p->phase = KP_DRAW;
}

static void apply(kpos *p, int act)
{
    int k = KA_KIND(act), c = KA_CARD(act), pl = p->turn;
    switch (k) {
    case KA_PLAY: {
        int i = in_hand(p, pl, c);
        memmove(&p->hand[pl][i], &p->hand[pl][i + 1], (size_t)(p->nh[pl] - i - 1));
        p->nh[pl]--;
        p->drawn = KK_NONE;
        meet(p, c, 0);
        break;
    }
    case KA_TAKE:
        for (int s = 0; s < KK_FIELD; s++)
            if (p->field[s] == c) p->field[s] = KK_NONE;
        capture(p, pl, p->held);
        capture(p, pl, c);
        p->held = KK_NONE;
        if (p->from_deck) end_turn(p);
        else p->phase = KP_DRAW;
        break;
    case KA_DRAW:
        p->drawn = p->deck[--p->ndeck];
        meet(p, p->drawn, 1);
        break;
    case KA_KOI:
        p->koi[pl]++;
        p->base[pl] = (uint8_t)kk_points(p, pl);
        next_turn(p);
        break;
    case KA_STOP:
        round_end(p, KR_STOP, pl, 0);
        break;
    case KA_NEXT:
        p->round++;
        deal(p);
        break;
    }
}

int kk_play(kgame *g, int act)
{
    if (g->n >= KK_MAX_ACTS || !kk_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int kk_undo(kgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint16_t acts[KK_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof *acts);
    kk_new(g, g->variant, g->seed);
    for (int i = 0; i < n; i++) kk_play(g, acts[i]);
    return 1;
}

int kk_winner(const kpos *p)
{
    if (p->phase != KP_OVER || p->score[0] == p->score[1]) return -1;
    return p->score[1] > p->score[0];
}

int kk_auto(const kpos *p)
{
    int s[4];
    switch (p->phase) {
    case KP_PLAY: return KA_PLAY | p->hand[p->turn][0];
    case KP_CHOOSE: kk_matches(p, p->held, s); return KA_TAKE | p->field[s[0]];
    case KP_DRAW: return KA_DRAW;
    case KP_DECIDE: return KA_STOP;
    case KP_ROUND: return KA_NEXT;
    default: return 0;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int kk_card_name(const char *s, int *card)
{
    int m = 0, n = 0;
    while (s[n] >= '0' && s[n] <= '9' && n < 2) m = m * 10 + (s[n++] - '0');
    if (!n || m < 1 || m > 12 || s[n] < 'a' || s[n] > 'd') return 0;
    *card = HANA(m - 1, s[n] - 'a');
    return n + 1;
}

int kk_script(kgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int card = 0, act;
        if (c == '.') {                         /* one plain action */
            if (!kk_play(g, kk_auto(&g->pos))) return 0;
            continue;
        }
        if (c == '#' || c == '*') {             /* plain moves to the end of the round, or of the game */
            for (;;) {
                int ph = g->pos.phase;
                if (ph == KP_OVER || (ph == KP_ROUND && c == '#')) break;
                if (!kk_play(g, kk_auto(&g->pos))) return 0;
            }
            continue;
        }
        if (c == 'p' || c == 't') {
            int k = kk_card_name(s, &card);
            if (!k) return 0;
            s += k;
        }
        switch (c) {
        case 'p': act = KA_PLAY | card; break;
        case 't': act = KA_TAKE | card; break;
        case 'd': act = KA_DRAW; break;
        case 'k': act = KA_KOI; break;
        case 's': act = KA_STOP; break;
        case 'n': act = KA_NEXT; break;
        default: return 0;
        }
        if (!kk_play(g, act)) return 0;
    }
    return 1;
}
