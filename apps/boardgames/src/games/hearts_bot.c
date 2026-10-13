/*
 * Hearts' bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards that keep to the suits each player has shown void), each player for himself, the reward
 * the deal's points (a moon counted). Runs on the worker thread: it copies the game at entry and reads only what
 * its seat may know (its hand, the cards played, the three cards it passed). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The pass (rule-based): the three most dangerous cards: the queen of spades, and the ace and king of spades that
 * catch her, unless the hand holds five spades or more to guard them; high hearts; then the high cards of a short
 * club or diamond suit (to void it). The round's summary waits for a human tap (actor() -1); think() answers "n"
 * there all the same (the self-play tests).
 * Its moves are the game's notation ("p2cQsAh", "Th", "n"), played by hearts_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/hearts.h"
#include "games/trickbot.h"

#define HEARTS_HARD_ITERS 16000                 /* about 0.3 s a card on a PC */

typedef struct { uint8_t broken; } hx;

static int h_points(const tb_state *st, int c) { (void)st; return hearts_points(c); }

static int h_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    int k = 0;
    if (!st->ntrick) {
        if (st->tricks == 0)                    /* the 2 of clubs leads */
            for (int i = 0; i < n; i++)
                if (h[i] == HEARTS_LEAD) { out[0] = h[i]; return 1; }
        int only = trick_only_suit(h, n, SUIT_HEARTS);
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(h[i]) != SUIT_HEARTS || TB_CXS(st, hx)->broken || only) out[k++] = h[i];
        return k;
    }
    k = tb_legal_follow(st, h, n, out);
    if (st->tricks == 0 && CARD_SUIT(out[0]) != CARD_SUIT(st->trick[0])) {   /* no points on the first trick */
        int m = 0;
        for (int i = 0; i < k; i++)
            if (!hearts_points(out[i])) out[m++] = out[i];
        if (m) k = m;
    }
    return k;
}

static void h_trick_end(tb_state *st, int w)
{
    (void)w;
    for (int i = 0; i < st->ntrick; i++)
        if (CARD_SUIT(st->trick[i]) == SUIT_HEARTS) TB_XS(st, hx)->broken = 1;
}

static void h_reward(const tb_state *st, float *rw)
{
    int moon = -1;
    for (int s = 0; s < st->players; s++)
        if (st->pts[s] == 26) moon = s;
    for (int s = 0; s < st->players; s++) {
        int score = moon < 0 ? st->pts[s] : s == moon ? 0 : 26;
        rw[s] = 1.0f - score / 26.0f;
    }
}

/* avoid tricks; but go for the moon holding every point so far (16 or more), and stop another from shooting it */
static int h_want(const tb_state *st, int seat)
{
    int total = 0;
    for (int s = 0; s < st->players; s++) total += st->pts[s];
    if (total >= 16 && total < 26)
        for (int s = 0; s < st->players; s++)
            if (st->pts[s] == total) return 1;  /* (s == seat: shoot; another: take one heart from him) */
    (void)seat;
    return -1;
}

static const tb_rules RULES = {
    .legal = h_legal, .points = h_points, .trick_end = h_trick_end, .reward = h_reward, .want = h_want,
};

static int removed(int players, int card)      /* as the rules: the low diamonds and spades out for 3, 5, 6 */
{
    int s = CARD_SUIT(card), r = CARD_RANK(card);
    if (players == 3) return card == CARD(SUIT_DIAMONDS, 2);
    if (players == 5) return r == 2 && (s == SUIT_DIAMONDS || s == SUIT_SPADES);
    if (players == 6) return (r == 2 || r == 3) && (s == SUIT_DIAMONDS || s == SUIT_SPADES);
    return 0;
}

/* the view of seat me: its hand, the others' counts, the three cards it passed (known where they went, until
 * played), the deal's plays replayed for the voids */
void hb_view(const hgame *g, int me, tb_state *v, hgame *t)
{
    const hpos *p = &g->pos;
    tb_init(v, &RULES, p->players, me);
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
    TB_XS(v, hx)->broken = p->broken;
    for (int s = 0; s < p->players; s++) {
        v->pts[s] = p->taken[s];
        if (s != me) tb_hidden(v, s, p->n[s]);
    }
    tb_mine(v, p->hand[me], p->n[me]);
    /* the replay: the deal's plays in order (only the cards played and whose turn it was are read) */
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (HA_KIND(g->act[i]) == HA_NEXT) start = i + 1;
    hearts_new(t, g->players, g->seed);
    int plays = p->tricks * p->players;         /* the cards of finished tricks: gone */
    for (int i = 0, k = 0; i < g->n; i++) {
        uint32_t a = g->act[i];
        if (i >= start && HA_KIND(a) == HA_PLAY) {
            const hpos *q = &t->pos;
            uint8_t who[HEARTS_MAXP];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % q->players);
            tb_note(v, q->turn, HA_CARD(a, 2), q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, HA_CARD(a, 2));
        }
        hearts_play(t, a);
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % p->players, p->trick[j]);
    if (p->dir != PASS_HOLD) {
        int to = hearts_target(p, me);
        for (int k = 0; k < 3; k++) {
            int c = p->pass[me][k], gone = 0;
            for (int j = 0; j < v->ngone; j++) gone |= v->gone[j] == c;
            for (int j = 0; j < v->ntrick; j++) gone |= v->trick[j] == c;
            if (c != CARD_NONE && !gone && to != me) tb_known(v, to, c);
        }
    }
    uint8_t pack[CARDS_MAX], deck[CARDS_MAX];
    int n = cards_pack(PACK_52, pack), k = 0;
    for (int i = 0; i < n; i++)
        if (!removed(p->players, pack[i])) deck[k++] = pack[i];
    tb_finish(v, deck, k);
}

/* the pass: the three most dangerous cards */
static uint32_t hb_pass(const hpos *p, int me)
{
    const uint8_t *h = p->hand[me];
    int n = p->n[me], spades = tb_count_suit(h, n, SUIT_SPADES), score[HEARTS_HAND];
    for (int i = 0; i < n; i++) {
        int c = h[i], s = CARD_SUIT(c), r = trick_rank(c);
        if (s == SUIT_SPADES) score[i] = r >= 12 ? (spades >= 5 ? r : 100 + r) : r;
        else if (s == SUIT_HEARTS) score[i] = r >= 10 ? 40 + 2 * r : r;
        else score[i] = 2 * r + (tb_count_suit(h, n, s) <= 3 ? 20 : 0);
    }
    int pick[3];
    for (int k = 0; k < 3; k++) {
        int b = -1;
        for (int i = 0; i < n; i++) {
            int used = 0;
            for (int j = 0; j < k; j++) used |= pick[j] == i;
            if (!used && (b < 0 || score[i] > score[b])) b = i;
        }
        pick[k] = b;
    }
    return HA_PASS3(h[pick[0]], h[pick[1]], h[pick[2]]);
}

/* the action for seat at a level (0: nothing to do) */
uint32_t hb_choose(const hgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const hpos *p = &g->pos;
    if (p->phase == HP_ROUND) return HA_NEXT;
    if (p->phase == HP_OVER || p->turn != seat) return 0;
    if (p->phase == HP_PASS) return hb_pass(p, seat);
    tb_state *v = malloc(sizeof *v);
    hgame *t = malloc(sizeof *t);
    uint32_t a = 0;
    if (v && t) {
        hb_view(g, seat, v, t);
        int c = tb_choose(v, level, seed, HEARTS_HARD_ITERS, stats);
        if (c != TB_NONE && hearts_can_play(p, c)) a = HA_PLAY | (uint32_t)c;
    }
    free(v);
    free(t);
    return a ? a : hearts_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    hgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *hearts_live();                        /* the copy, at entry: nothing else is read */
    uint32_t a = hb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    char c[3][3];
    switch (HA_KIND(a)) {
    case HA_PASS:
        for (int i = 0; i < 3; i++) trick_name(HA_CARD(a, i), c[i]);
        snprintf(move, (size_t)cap, "p%s%s%s", c[0], c[1], c[2]);
        return 1;
    case HA_PLAY:
        trick_name(HA_CARD(a, 2), c[0]);
        snprintf(move, (size_t)cap, "%s", c[0]);
        return 1;
    case HA_NEXT: snprintf(move, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static int actor(void)
{
    const hpos *p = &hearts_live()->pos;
    return p->phase == HP_PASS || p->phase == HP_PLAY ? p->turn : -1;
}

static const bot hearts_bot = {.think = think, .actor = actor, .play = hearts_show};
REGISTER_BOT(hearts, &hearts_bot)
