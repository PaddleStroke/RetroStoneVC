/*
 * Gin Rummy's bot (bots.h): rule-based play on a one-draw look-ahead, from what its seat may know: its own hand, the
 * discard pile (every card in it, buried or not), and the cards the other player took from the pile and still holds
 * (the hand's public actions replayed). The unseen cards are the rest.
 * Runs on the worker thread: it copies the game at entry and reads only that copy. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   The worth of a hand of ten (lower is better): its deadwood, and the deadwood it can expect after one more draw
 *   (each unseen card drawn in turn, the best discard after it): a hand with pairs and near runs scores better than
 *   one of the same deadwood made of loose high cards.
 *   The discard: the card leaving the best hand; less a danger: a card that melds with the cards the other player is
 *   known to hold (taken from the pile), more so late in the hand; one they threw away (or of a rank they threw) is
 *   safer.
 *   The draw: the pile's top card when it goes into a meld at once, or (hard) when it cuts the deadwood well below
 *   what a draw from the stock can be expected to leave.
 *   The knock: as soon as it may, except late in the hand with more than a little deadwood against a player who has
 *   been taking cards (an undercut): then it plays on for a lower count or gin.
 * Levels: easy plays the rules' plain moves (the least deadwood, a knock as soon as it can), with a slip now and then
 * (the second best discard); medium looks one draw ahead on half the unseen cards and takes the pile only for a meld;
 * hard looks ahead on all of them, weighs the danger and times its knock.
 * Its moves are the game's notation ("s", "t", "p", "d7h", "kQs", "n"), played by ginrummy_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/ginrummy.h"

typedef struct {
    uint8_t me, hand[11], n;
    uint8_t unseen[52], nunseen;                /* the cards nowhere in sight */
    uint8_t known[11], nknown;                  /* the other player holds these (taken from the pile) */
    uint8_t thrown[128];                        /* the other player discarded it this hand */
} gview;

/* the view of seat me: the hand's public actions replayed (only the pile's cards and who took them are read) */
static void view_of(const gngame *g, int me, gview *v)
{
    const gnpos *p = &g->pos;
    memset(v, 0, sizeof *v);
    v->me = (uint8_t)me;
    v->n = p->n[me];
    memcpy(v->hand, p->hand[me], v->n);
    gngame *t = malloc(sizeof *t);
    uint8_t held[128] = {0};
    if (t) {
        gin_new(t, g->seed);
        for (int i = 0; i < g->n; i++) {
            const gnpos *q = &t->pos;
            int a = g->act[i], k = GNA_KIND(a), c = GNA_CARD(a);
            if (k == GNA_NEXT) memset(held, 0, sizeof held), memset(v->thrown, 0, sizeof v->thrown);
            else if (q->turn != me && k == GNA_TAKE && q->npile) held[q->pile[q->npile - 1]] = 1;
            else if (q->turn != me && (k == GNA_DISCARD || k == GNA_KNOCK)) held[c] = 0, v->thrown[c] = 1;
            gin_play(t, a);
        }
        free(t);
    }
    uint8_t seen[128] = {0};
    for (int i = 0; i < v->n; i++) seen[v->hand[i]] = 1;
    for (int i = 0; i < p->npile; i++) seen[p->pile[i]] = 1;
    for (int c = 0; c < 128; c++)
        if (held[c] && !seen[c]) v->known[v->nknown++] = (uint8_t)c, seen[c] = 1;
    uint8_t pack[52];
    int n = cards_pack(PACK_52, pack);
    for (int i = 0; i < n; i++)
        if (!seen[pack[i]]) v->unseen[v->nunseen++] = pack[i];
}

static int without(const uint8_t *c, int n, int i, uint8_t *out)   /* the n cards less the i-th */
{
    int k = 0;
    for (int j = 0; j < n; j++)
        if (j != i) out[k++] = c[j];
    return k;
}

/* the deadwood of ten cards after one more card and the best discard */
static int after_draw(const uint8_t *ten, int card)
{
    uint8_t c[11];
    memcpy(c, ten, 10);
    c[10] = (uint8_t)card;
    int best = 1 << 20;
    for (int i = 0; i < 11; i++) {
        uint8_t d[10];
        without(c, 11, i, d);
        int x = gin_arrange(d, 10, NULL);
        if (x < best) best = x;
        if (!best) break;
    }
    return best;
}

/* the expected deadwood after a draw from the unseen cards (every `step`-th of them), times 4 */
static int expect(const gview *v, const uint8_t *ten, int step, int from)
{
    long sum = 0;
    int k = 0;
    for (int i = from % step; i < v->nunseen; i += step) sum += after_draw(ten, v->unseen[i]), k++;
    return k ? (int)(4 * sum / k) : 4 * gin_arrange(ten, 10, NULL);
}

/* the worth of ten cards, x4 (lower is better) */
static int worth(const gview *v, const uint8_t *ten, int step)
{
    return 2 * gin_arrange(ten, 10, NULL) + expect(v, ten, step, 0) / 2;
}

/* a discard's danger, x4: it melds with the other player's known cards */
static int danger(const gview *v, int card, int stock)
{
    int r = CARD_RANK(card), s = CARD_SUIT(card), d = 0;
    for (int i = 0; i < v->nknown; i++) {
        int k = v->known[i], kr = CARD_RANK(k);
        if (kr == r) d += 6;
        else if (CARD_SUIT(k) == s && abs(kr - r) <= 2) d += abs(kr - r) == 1 ? 6 : 3;
    }
    for (int c = 0; c < 128; c++)               /* thrown away: they did not want it */
        if (v->thrown[c] && (CARD_RANK(c) == r || (CARD_SUIT(c) == s && abs(CARD_RANK(c) - r) == 1))) d -= 2;
    if (d < 0) d = 0;
    return stock < 20 ? d * 2 : d;              /* late in the hand they are closer */
}

/* the pile's top card goes into a meld of the best ten kept */
static int melds_top(const uint8_t *hand, int n, int top)
{
    uint8_t c[11];
    memcpy(c, hand, (size_t)n);
    c[n] = (uint8_t)top;
    int best = 1 << 20, bi = -1;
    for (int i = 0; i < n; i++) {               /* the best discard, the top kept */
        uint8_t d[10];
        without(c, n + 1, i, d);
        int x = gin_arrange(d, n, NULL);
        if (x < best) best = x, bi = i;
    }
    if (bi < 0) return 0;
    uint8_t d[10];
    without(c, n + 1, bi, d);
    gin_arr a;
    gin_arrange(d, n, &a);
    int melded = 0;
    for (int m = 0; m < a.nm; m++) melded += a.mlen[m];
    for (int i = 0; i < melded; i++)
        if (a.card[i] == top) return best + 1;  /* (the deadwood + 1: never 0) */
    return 0;
}

/* GN_OFFER / GN_DRAW: take the pile's top card? */
static int take_it(const gnpos *p, const gview *v, int level)
{
    if (!p->npile || (p->phase == GN_DRAW && p->passes >= 2)) return 0;
    int top = p->pile[p->npile - 1];
    if (level == AI_EASY) return gin_auto(p) == GNA_TAKE;
    if (melds_top(v->hand, v->n, top)) return 1;
    if (level < AI_HARD) return 0;
    /* hard: a sure low deadwood against the stock's expected one */
    uint8_t c[11];
    memcpy(c, v->hand, 10);
    int took = after_draw(c, top), stock = expect(v, c, 1, 0);
    return 4 * took + 12 <= stock;
}

/* GN_DISCARD: the card to throw, and whether to knock with it */
static int discard(const gnpos *p, const gview *v, int level, ai_rng *r)
{
    int n = v->n, stock = p->nstock;
    if (level == AI_EASY) {
        int a = gin_auto(p);
        if (ai_rand_n(r, 100) < 22) {           /* a slip: the second best discard (no knock) */
            int best = 1 << 20, second = 1 << 20, b1 = -1, b2 = -1;
            for (int i = 0; i < n; i++) {
                if (v->hand[i] == p->taken) continue;
                int d = gin_without(v->hand, n, v->hand[i]);
                if (d < best) second = best, b2 = b1, best = d, b1 = i;
                else if (d < second) second = d, b2 = i;
            }
            if (b2 >= 0 && second <= best + 6) return GNA_DISCARD | v->hand[b2];
        }
        return a;
    }
    int step = level == AI_MEDIUM ? 2 : 1, best = 1 << 30, bc = CARD_NONE, bdead = 99;
    for (int i = 0; i < n; i++) {
        int c = v->hand[i];
        if (c == p->taken) continue;
        uint8_t ten[10];
        without(v->hand, n, i, ten);
        int dead = gin_arrange(ten, 10, NULL);
        int w = (dead == 0 ? -1000 : 0) + worth(v, ten, step);
        if (level == AI_HARD) w += danger(v, c, stock);
        else w += ai_rand_n(r, 3);
        if (w < best || (w == best && gin_value(c) > gin_value(bc))) best = w, bc = c, bdead = dead;
    }
    if (bc == CARD_NONE) return gin_auto(p);
    int knock = bdead <= GIN_KNOCK;
    if (knock && level == AI_HARD && bdead > 0) {
        /* late, with more than a little deadwood, against a player who has been taking cards: play on */
        int risky = bdead > 6 && stock < 14 && v->nknown >= 2;
        if (risky && stock > 4) knock = 0;
    }
    if (knock && !gin_legal(p, GNA_KNOCK | bc)) knock = 0;
    return (knock ? GNA_KNOCK : GNA_DISCARD) | bc;
}

/* the action for seat at a level (0: nothing to do) */
int ginb_choose(const gngame *g, int seat, int level, uint64_t seed)
{
    const gnpos *p = &g->pos;
    if (p->phase == GN_END) return GNA_NEXT;
    if (p->phase == GN_OVER || p->turn != seat) return 0;
    gview *v = malloc(sizeof *v);
    if (!v) return gin_auto(p);
    view_of(g, seat, v);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 7 + (uint64_t)seat));
    int a = 0;
    switch (p->phase) {
    case GN_OFFER: a = take_it(p, v, level) ? GNA_TAKE : GNA_PASS; break;
    case GN_DRAW: a = take_it(p, v, level) ? GNA_TAKE : GNA_STOCK; break;
    case GN_DISCARD: a = discard(p, v, level, &r); break;
    }
    free(v);
    return a && gin_legal(p, a) ? a : gin_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    gngame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *ginrummy_live();                      /* the copy, at entry: nothing else is read */
    int a = ginb_choose(g, seat, level, ai_session_seed());
    free(g);
    if (!a || ai_cancelled() || cap < 4) return 0;
    static const char ranks[] = "A23456789TJQK", suits[] = "shdc";
    int c = GNA_CARD(a);
    switch (GNA_KIND(a)) {
    case GNA_STOCK: snprintf(move, (size_t)cap, "s"); break;
    case GNA_TAKE: snprintf(move, (size_t)cap, "t"); break;
    case GNA_PASS: snprintf(move, (size_t)cap, "p"); break;
    case GNA_NEXT: snprintf(move, (size_t)cap, "n"); break;
    case GNA_DISCARD:
    case GNA_KNOCK:
        snprintf(move, (size_t)cap, "%c%c%c", GNA_KIND(a) == GNA_KNOCK ? 'k' : 'd', ranks[CARD_RANK(c) - 1],
                 suits[CARD_SUIT(c)]);
        break;
    default: return 0;
    }
    return 1;
}

static int actor(void)
{
    const gnpos *p = &ginrummy_live()->pos;
    return p->phase == GN_OFFER || p->phase == GN_DRAW || p->phase == GN_DISCARD ? p->turn : -1;
}

static const bot ginrummy_bot = {.think = think, .actor = actor, .play = ginrummy_show};
REGISTER_BOT(ginrummy, &ginrummy_bot)
