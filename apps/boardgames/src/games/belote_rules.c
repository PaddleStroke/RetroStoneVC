/*
 * Belote: the rules (belote.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/belote.h"

/* ranks: 7..10, jack 11, queen 12, king 13, ace 1 (cards.h). Strength 1..8 and points, in trumps and out of them. */
int bel_rank(int card, int trump)
{
    static const uint8_t TRUMP[14] = {0, 6, 0, 0, 0, 0, 0, 1, 2, 7, 5, 8, 3, 4};
    static const uint8_t PLAIN[14] = {0, 8, 0, 0, 0, 0, 0, 1, 2, 3, 7, 4, 5, 6};
    int r = CARD_RANK(card);
    return CARD_SUIT(card) == trump ? TRUMP[r] : PLAIN[r];
}

int bel_points(int card, int trump)
{
    static const uint8_t TRUMP[14] = {0, 11, 0, 0, 0, 0, 0, 0, 0, 14, 10, 20, 3, 4};
    static const uint8_t PLAIN[14] = {0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 10, 2, 3, 4};
    int r = CARD_RANK(card);
    return CARD_SUIT(card) == trump ? TRUMP[r] : PLAIN[r];
}

int bel_target(const bpos *p) { return p->variant == BEL_COINCHE ? 1000 : 501; }

static int next(int pl) { return (pl + 1) % BEL_P; }

/* the trick's best card so far: the highest trump, else the highest of the suit led */
int bel_trick_best(const bpos *p)
{
    int best = 0;
    for (int i = 1; i < p->ntrick; i++) {
        int c = p->trick[i], b = p->trick[best];
        int ct = CARD_SUIT(c) == p->trump, bt = CARD_SUIT(b) == p->trump;
        if (ct && !bt) best = i;
        else if (ct == bt && CARD_SUIT(c) == CARD_SUIT(b) && bel_rank(c, p->trump) > bel_rank(b, p->trump)) best = i;
    }
    return best;
}

static int holds(const bpos *p, int pl, int card)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] == card) return 1;
    return 0;
}

static int has_suit(const bpos *p, int pl, int suit)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (CARD_SUIT(p->hand[pl][i]) == suit) return 1;
    return 0;
}

/* the trick's highest trump (its strength), 0: none */
static int high_trump(const bpos *p)
{
    int h = 0;
    for (int i = 0; i < p->ntrick; i++)
        if (CARD_SUIT(p->trick[i]) == p->trump && bel_rank(p->trick[i], p->trump) > h) h = bel_rank(p->trick[i], p->trump);
    return h;
}

static int can_overtrump(const bpos *p, int pl, int h)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (CARD_SUIT(p->hand[pl][i]) == p->trump && bel_rank(p->hand[pl][i], p->trump) > h) return 1;
    return 0;
}

/* may the player to move play this card? */
static int card_ok(const bpos *p, int card)
{
    int pl = p->turn, s = CARD_SUIT(card);
    if (!holds(p, pl, card)) return 0;
    if (!p->ntrick) return 1;
    int led = CARD_SUIT(p->trick[0]), h = high_trump(p);
    if (led == p->trump) {                      /* trumps led: a trump, higher than the best if you can */
        if (!has_suit(p, pl, led)) return 1;
        if (s != led) return 0;
        return !can_overtrump(p, pl, h) || bel_rank(card, p->trump) > h;
    }
    if (has_suit(p, pl, led)) return s == led;  /* follow suit */
    int master = (p->leader + bel_trick_best(p)) % BEL_P;
    if (bel_team(master) == bel_team(pl)) return 1;   /* your partner holds the trick: anything */
    if (!has_suit(p, pl, p->trump)) return 1;
    if (s != p->trump) return 0;                /* trump, over the best trump if you can */
    return !can_overtrump(p, pl, h) || bel_rank(card, p->trump) > h;
}

int bel_legal(const bpos *p, int act)
{
    int k = BA_KIND(act), a = BA_ARG(act);
    switch (p->phase) {
    case BP_BID1: return act == BA_PASS || act == BA_TAKE;
    case BP_BID2: return act == BA_PASS || (k == BA_NAME && a < 4 && a != CARD_SUIT(p->turned));
    case BP_AUCTION:
        if (p->coinche) return (k == BA_PASS && !a) || (k == BA_SURCOINCHE && !a && p->coinche == 1);
        if (k == BA_PASS) return !a;
        if (k == BA_BID) return (a >> 2) < BEL_LEVELS && (p->bidder == BEL_NONE || (a >> 2) > p->level);
        if (k == BA_COINCHE) return !a && p->bidder != BEL_NONE && bel_team(p->bidder) != bel_team(p->turn);
        return 0;
    case BP_PLAY: return k == BA_PLAY && card_ok(p, a);
    case BP_DEAL: return act == BA_NEXT;
    default: return 0;
    }
}

int bel_moves(const bpos *p, int *acts)
{
    int n = 0;
    if (p->phase == BP_PLAY) {
        for (int i = 0; i < p->n[p->turn]; i++)
            if (card_ok(p, p->hand[p->turn][i])) acts[n++] = BA_PLAY | p->hand[p->turn][i];
        return n;
    }
    static const int fixed[] = {BA_PASS, BA_TAKE, BA_COINCHE, BA_SURCOINCHE, BA_NEXT};
    for (int i = 0; i < 5; i++)
        if (bel_legal(p, fixed[i])) acts[n++] = fixed[i];
    for (int s = 0; s < 4; s++)
        if (bel_legal(p, BA_NAME | s)) acts[n++] = BA_NAME | s;
    for (int l = 0; l < BEL_LEVELS; l++)
        for (int s = 0; s < 4; s++)
            if (bel_legal(p, BA_BIDOF(l, s))) acts[n++] = BA_BIDOF(l, s);
    return n;
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static void give(bpos *p, int pl, int k)
{
    while (k-- > 0 && p->top < 32) p->hand[pl][p->n[pl]++] = p->pack[p->top++];
}

int bel_cut(uint8_t *pack, uint32_t seed)
{
    uint32_t s = mix(seed, 4242u);
    int k = 3 + (int)(cards_rand(&s) % 27u);    /* at least 3 cards lifted, at least 3 left */
    uint8_t t[32];
    memcpy(t, pack + k, (size_t)(32 - k));      /* the lower part... */
    memcpy(t + 32 - k, pack, (size_t)k);        /* ...on the lifted one */
    memcpy(pack, t, 32);
    return k;
}

/* a hand as the screen shows it: spades, hearts, clubs, diamonds (black, red, black, red), the strongest first */
static int order_key(int c, int trump)
{
    static const int SUIT_ORDER[4] = {0, 1, 3, 2};
    return SUIT_ORDER[CARD_SUIT(c)] * 16 + (15 - bel_rank(c, trump == BEL_NONE ? -1 : trump));
}

void bel_sort(uint8_t *cards, int n, int trump)
{
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && order_key(cards[j], trump) < order_key(cards[j - 1], trump); j--) {
            uint8_t t = cards[j];
            cards[j] = cards[j - 1];
            cards[j - 1] = t;
        }
}

/* the deal: from this pack, top first, once cut (NULL: a fresh pack, shuffled: the warm-up) */
static void deal(bpos *p, const uint8_t *from)
{
    p->cutter = (uint8_t)((p->dealer + BEL_P - 1) % BEL_P);   /* the dealer's left at a French table */
    if (from) {
        memcpy(p->pack, from, 32);
        p->cut = (uint8_t)bel_cut(p->pack, mix(p->seed, 1000u + p->dealt));
    } else {
        cards_pack(PACK_32, p->pack);
        cards_shuffle(p->pack, 32, mix(p->seed, 1000u + p->dealt));
        p->cut = 0;
    }
    p->dealt++;
    p->top = 0;
    p->nheap[0] = p->nheap[1] = 0;
    memset(p->n, 0, sizeof p->n);
    memset(p->said, 0, sizeof p->said);
    int first = next(p->dealer);
    static const int classic[] = {3, 2}, coinche[] = {3, 3, 2};
    int rounds = p->variant == BEL_COINCHE ? 3 : 2;
    for (int r = 0; r < rounds; r++)
        for (int i = 0; i < BEL_P; i++) give(p, (first + i) % BEL_P, p->variant == BEL_COINCHE ? coinche[r] : classic[r]);
    p->turned = p->variant == BEL_COINCHE ? CARD_NONE : p->pack[p->top++];
    p->taker = p->trump = p->bidder = p->coincher = BEL_NONE;
    p->level = p->coinche = p->passes = 0;
    p->phase = p->variant == BEL_COINCHE ? BP_AUCTION : BP_BID1;
    p->turn = (uint8_t)first;
    p->ntrick = p->tricks = 0;
    p->last_leader = p->last_winner = BEL_NONE;
    memset(p->last, CARD_NONE, sizeof p->last);
    p->won[0] = p->won[1] = 0;
    p->points[0] = p->points[1] = 0;
    p->belote = BEL_NONE;
    p->belote_said = 0;
}

static void begin(bgame *g, int variant, uint32_t seed, const uint8_t *pack)
{
    uint8_t from[32];
    if (pack) memcpy(from, pack, 32);           /* it may be g->start (undo) */
    memset(g, 0, sizeof *g);
    bpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = p->variant = (uint8_t)(variant == BEL_COINCHE ? BEL_COINCHE : BEL_CLASSIC);
    p->dealer = BEL_P - 1;
    if (pack) memcpy(g->start, from, 32);
    deal(p, pack ? g->start : NULL);
}

void bel_start(bgame *g, int variant, uint32_t seed, const uint8_t *pack)
{
    uint8_t ok[CARDS_MAX];                      /* a pack of the 32 cards, once each, or a fresh one */
    int seen[256] = {0}, good = pack != NULL;
    cards_pack(PACK_32, ok);
    for (int i = 0; good && i < 32; i++) good = ++seen[pack[i]] == 1;
    for (int i = 0; good && i < 32; i++) good = seen[ok[i]] == 1;
    begin(g, variant, seed, good ? pack : ok);
}

void bel_new(bgame *g, int variant, uint32_t seed)
{
    uint8_t pack[32];
    bel_warmup(pack, variant, seed);
    bel_start(g, variant, seed, pack);
}

/* the contract is set: classic, the rest of the pack dealt (the taker: the turned card and 2); then the play */
static void begin_play(bpos *p, int taker, int trump)
{
    p->taker = (uint8_t)taker;
    p->trump = (uint8_t)trump;
    if (p->variant == BEL_CLASSIC) {
        int first = next(p->dealer);
        for (int i = 0; i < BEL_P; i++) {
            int q = (first + i) % BEL_P;
            if (q == taker) p->hand[q][p->n[q]++] = p->turned;
            give(p, q, q == taker ? 2 : 3);
        }
    }
    for (int q = 0; q < BEL_P; q++)
        if (holds(p, q, CARD(trump, RANK_KING)) && holds(p, q, CARD(trump, RANK_QUEEN))) p->belote = (uint8_t)q;
    p->phase = BP_PLAY;
    p->turn = p->leader = (uint8_t)next(p->dealer);
    p->ntrick = 0;
}

/* the next deal: no shuffle, the cards gathered as they lie, cut, dealt */
static void redeal(bpos *p)
{
    uint8_t pack[32];
    bel_gather(p, pack);
    p->dealer = (uint8_t)next(p->dealer);
    deal(p, pack);
}

/* ---- the score of a deal ----------------------------------------------------------------------------------------- */
static void score(bpos *p)
{
    int t = bel_team(p->taker), d = 1 - t, bel[2] = {0, 0}, pts[2];
    if (p->belote != BEL_NONE) bel[bel_team(p->belote)] = 20;
    for (int e = 0; e < 2; e++) pts[e] = p->won[e] == 8 ? 250 : p->won[1 - e] == 8 ? 0 : p->points[e];
    p->capot = p->won[0] == 8 ? 1 : p->won[1] == 8 ? 2 : 0;
    int pt = pts[t] + bel[t], pd = pts[d] + bel[d];
    p->made_pts[t] = (int16_t)pt;
    p->made_pts[d] = (int16_t)pd;
    if (p->variant == BEL_CLASSIC) {
        p->made = pt > pd;
        p->score[t] = (int16_t)(p->made ? pt : bel[t]);
        p->score[d] = (int16_t)(p->made ? pd : (p->won[d] == 8 ? 250 : 162) + bel[d]);
    } else {
        int v = bel_value(p->level), mult = 1 << p->coinche;
        p->made = p->level == BEL_CAPOT ? p->won[t] == 8 : pt >= v && pt > 81;
        if (!p->coinche) {
            p->score[t] = (int16_t)(p->made ? v + pt : bel[t]);
            p->score[d] = (int16_t)(p->made ? pd : 160 + v + bel[d]);
        } else {
            int win = p->made ? t : d;
            p->score[win] = (int16_t)((160 + v) * mult + bel[win]);
            p->score[1 - win] = (int16_t)bel[1 - win];
        }
    }
    p->total[0] = (int16_t)(p->total[0] + p->score[0]);
    p->total[1] = (int16_t)(p->total[1] + p->score[1]);
    p->deal++;
    int top = p->total[0] > p->total[1] ? p->total[0] : p->total[1];
    p->phase = top >= bel_target(p) && p->total[0] != p->total[1] ? BP_OVER : BP_DEAL;
}

int bel_winner_team(const bpos *p)
{
    if (p->phase != BP_OVER) return -1;
    return p->total[1] > p->total[0];
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void play_card(bpos *p, int card)
{
    int pl = p->turn;
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] == card) {
            memmove(&p->hand[pl][i], &p->hand[pl][i + 1], (size_t)(p->n[pl] - i - 1));
            p->n[pl]--;
            break;
        }
    if (pl == p->belote && CARD_SUIT(card) == p->trump &&
        (CARD_RANK(card) == RANK_KING || CARD_RANK(card) == RANK_QUEEN))
        p->belote_said++;
    p->trick[p->ntrick++] = (uint8_t)card;
    if (p->ntrick < BEL_P) { p->turn = (uint8_t)next(pl); return; }
    int w = (p->leader + bel_trick_best(p)) % BEL_P, sum = 0;
    for (int i = 0; i < BEL_P; i++) sum += bel_points(p->trick[i], p->trump);
    memcpy(p->last, p->trick, sizeof p->last);
    p->last_leader = p->leader;
    p->last_winner = (uint8_t)w;
    for (int i = 0; i < BEL_P; i++) {           /* onto the winners' pile, in the order of play (bel_gather) */
        int t = bel_team(w);
        p->heap[t][p->nheap[t]++] = p->trick[i];
    }
    p->won[bel_team(w)]++;
    p->points[bel_team(w)] = (int16_t)(p->points[bel_team(w)] + sum + (p->tricks == 7 ? 10 : 0));
    p->tricks++;
    p->ntrick = 0;
    p->turn = p->leader = (uint8_t)w;
    if (p->tricks == BEL_HAND) score(p);
}

static void apply(bpos *p, int act)
{
    int k = BA_KIND(act), a = BA_ARG(act), pl = p->turn;
    if (p->phase != BP_PLAY && p->phase != BP_DEAL) p->said[pl] = (uint16_t)act;
    switch (k) {
    case BA_TAKE: begin_play(p, pl, CARD_SUIT(p->turned)); break;
    case BA_NAME: begin_play(p, pl, a); break;
    case BA_PASS:
        if (p->phase == BP_AUCTION && p->coinche) { begin_play(p, p->bidder, p->trump); break; }
        p->passes++;
        p->turn = (uint8_t)next(pl);
        if (p->phase == BP_BID1 && p->passes == BEL_P) p->phase = BP_BID2, p->passes = 0;
        else if (p->phase == BP_BID2 && p->passes == BEL_P) redeal(p);
        else if (p->phase == BP_AUCTION && p->bidder == BEL_NONE && p->passes == BEL_P) redeal(p);
        else if (p->phase == BP_AUCTION && p->bidder != BEL_NONE && p->passes == BEL_P - 1)
            begin_play(p, p->bidder, p->trump);
        break;
    case BA_BID:
        p->bidder = (uint8_t)pl;
        p->level = (uint8_t)(a >> 2);
        p->trump = (uint8_t)(a & 3);
        p->passes = 0;
        p->turn = (uint8_t)next(pl);
        break;
    case BA_COINCHE:
        p->coinche = 1;
        p->coincher = (uint8_t)pl;
        p->turn = p->bidder;                    /* the bidder may surcoinche */
        break;
    case BA_SURCOINCHE:
        p->coinche = 2;
        begin_play(p, p->bidder, p->trump);
        break;
    case BA_PLAY: play_card(p, a); break;
    case BA_NEXT: redeal(p); break;
    }
}

int bel_play(bgame *g, int act)
{
    if (g->n >= BEL_MAX_ACTS || !bel_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int bel_undo(bgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint16_t acts[BEL_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    begin(g, g->variant, g->seed, g->start);
    for (int i = 0; i < n; i++) bel_play(g, acts[i]);
    return 1;
}

/* ---- the pack between deals -------------------------------------------------------------------------------------- */
static void push(uint8_t *b, int *n, const uint8_t *c, int k)
{
    for (int i = 0; i < k && *n < 32; i++) b[(*n)++] = c[i];
}

void bel_gather(const bpos *p, uint8_t *pack)
{
    uint8_t b[32];                              /* bottom first */
    int n = 0, t = p->taker == BEL_NONE ? 0 : bel_team(p->taker);
    push(b, &n, p->heap[t], p->nheap[t]);       /* the takers' pile, the defenders' on it */
    push(b, &n, p->heap[1 - t], p->nheap[1 - t]);
    if (p->phase == BP_PLAY) push(b, &n, p->trick, p->ntrick);
    for (int k = 0; k < BEL_P; k++) {           /* the hands as held, the dealer's next first */
        int pl = (p->dealer + 1 + k) % BEL_P;
        uint8_t h[BEL_HAND];
        memcpy(h, p->hand[pl], p->n[pl]);
        bel_sort(h, p->n[pl], p->trump);
        push(b, &n, h, p->n[pl]);
    }
    int rest = p->top - (p->variant == BEL_CLASSIC && p->taker == BEL_NONE);   /* the turned card: on the rest */
    for (int i = 31; i >= rest; i--) push(b, &n, &p->pack[i], 1);
    int seen[256] = {0}, good = n == 32;
    for (int i = 0; good && i < 32; i++) good = ++seen[b[i]] == 1;
    if (!good) {                                /* never: a fresh pack rather than a wrong one */
        cards_pack(PACK_32, pack);
        return;
    }
    for (int i = 0; i < 32; i++) pack[i] = b[31 - i];
}

/* the session's first pack: a whole deal from a shuffled pack, played by random legal choices, never seen */
int bel_warmup(uint8_t *pack, int variant, uint32_t seed)
{
    static bgame w;
    begin(&w, variant, mix(seed, 9000u), NULL);
    bpos *p = &w.pos;
    uint32_t s = mix(seed, 31337u);
    int taker = (int)(cards_rand(&s) % BEL_P);  /* the player who takes (classic) or bids 80 (coinche) */
    for (int steps = 0; p->phase != BP_DEAL && p->phase != BP_OVER && steps < 200; steps++) {
        int act = BA_PASS, acts[64], k;
        if (p->phase == BP_BID1 && p->turn == taker) act = BA_TAKE;
        else if (p->phase == BP_AUCTION && p->bidder == BEL_NONE && p->turn == taker)
            act = BA_BIDOF(0, (int)(cards_rand(&s) % 4u));
        else if (p->phase == BP_PLAY && (k = bel_moves(p, acts)) > 0) act = acts[cards_rand(&s) % (uint32_t)k];
        if (!bel_play(&w, act)) break;
    }
    bel_gather(p, pack);
    return p->phase == BP_DEAL || p->phase == BP_OVER;
}

/* plain play: the first player takes (classic) or bids 80 in the suit of their first card (coinche), the others
 * pass; the first card allowed */
int bel_auto(const bpos *p)
{
    switch (p->phase) {
    case BP_BID1: return BA_TAKE;
    case BP_BID2: return BA_PASS;
    case BP_AUCTION:
        return p->bidder == BEL_NONE && !p->coinche ? BA_BIDOF(0, CARD_SUIT(p->hand[p->turn][0])) : BA_PASS;
    case BP_PLAY: {
        int acts[64];
        return bel_moves(p, acts) ? acts[0] : 0;
    }
    case BP_DEAL: return BA_NEXT;
    default: return 0;
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

static int rank_of(char c)
{
    switch (c) {
    case 'A': return RANK_ACE;
    case 'K': return RANK_KING;
    case 'Q': return RANK_QUEEN;
    case 'J': return RANK_JACK;
    case 'T': return 10;
    default: return c >= '7' && c <= '9' ? c - '0' : -1;
    }
}

int bel_script(bgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int act = -1;
        if (c == '#') {                         /* plain actions to the end of the deal */
            while (g->pos.phase != BP_DEAL && g->pos.phase != BP_OVER)
                if (!bel_play(g, bel_auto(&g->pos))) return 0;
            continue;
        }
        if (c == '.') act = bel_auto(&g->pos);
        else if (c == 't') act = BA_TAKE;
        else if (c == 'p') act = BA_PASS;
        else if (c == 'N') act = BA_NEXT;
        else if (c == 'x') {
            act = *s == 'x' ? (s++, BA_SURCOINCHE) : BA_COINCHE;
        } else if (c == 'n') {
            int su = suit_of(*s);
            if (su < 0) return 0;
            s++;
            act = BA_NAME | su;
        } else if (c == 'b') {
            int v = 0;
            while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
            int su = suit_of(*s);
            if (su < 0) return 0;
            s++;
            int level = v == 250 ? BEL_CAPOT : (v - 80) / 10;
            if (v != 250 && (v < 80 || v > 160 || v % 10)) return 0;
            act = BA_BIDOF(level, su);
        } else if (rank_of(c) > 0) {
            int su = suit_of(*s);
            if (su < 0) return 0;
            s++;
            act = BA_PLAY | CARD(su, rank_of(c));
        }
        if (act < 0 || !bel_play(g, act)) return 0;
    }
    return 1;
}
