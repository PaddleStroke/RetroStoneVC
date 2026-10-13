/*
 * Skat's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards that keep to the suits each player has shown void), the declarer alone against the two
 * defenders who play for their side, the reward the deal's score as the rules count it (the game value with the
 * schneider and schwarz reached, overbid, the Seeger-Fabian 50s and 40s). Runs on the worker thread: it copies the
 * game at entry and reads only what its seat may know: its hand, the cards played, the declarer's open hand (ouvert),
 * and for the declarer who took up the skat the two cards he laid away (they count for him; the skat of a hand game
 * stays unknown to all, defenders never see it). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Skat has three players, one variant of play (the lists of 12 or 36 deals only change the length): no fourth player
 * sitting out, no Ramsch here (the rules have neither). Every call below is the game's legal action (checked; the
 * rules' plain action otherwise).
 *
 * ---- The hand's worth (rule-based) ---------------------------------------------------------------------------------
 * A ten-card playing hand (after the skat is made) gets a chance to win and a chance to reach 90 (schneider) in each
 * game, from a few counts of its strength, weighed by logistic weights fitted on deals played out by the bot itself
 * (medium level, every seat):
 *   suit game: trumps held, jacks, the club jack (and the spade jack with it), the trump ace and ten, side aces, side
 *              ace-tens, side tens without the ace, side losers (side cards that are not an ace or an ace's ten),
 *              side voids, the skat's points (the cards laid away; for a hand game the expected 2/22 of the points
 *              not held), forehand (the first lead);
 *   grand:     jacks, the club jack (and spade), aces, ace-tens, tens without the ace, standing cards (each side suit's
 *              unbroken run from the ace: A 10 K Q...), side losers, voids, the skat's points, forehand, the longest
 *              side suit;
 *   null:      the unsafe suits (a suit is safe when its k-th lowest card, A K Q J 10 9 8 7 counted 8..1, is at most
 *              2k-1: the 7; 7 and 9; 7, 9 and the jack...), how far over, the worst, voids, forehand.
 * The gain of a game is the score against the two others: won, value + 50; lost, -(2 x value + 50) and 40 to each
 * other player; value: base x (matadors with or without, the skat counted + 1 + hand + schneider...). Overbid: lost,
 * unless reaching 90 lifts the value over the bid (then the chance is the chance of schneider).
 *
 * ---- The calls ---------------------------------------------------------------------------------------------------
 * - Bidding: every possible skat (the 231 pairs of the 22 cards not held) is taken up in thought: the best two cards
 *   to lay away and the best game for each, at the value to say or hold; and every hand game with the skat unknown
 *   (with schneider announced, or schwarz and ouvert with a hand that wins every trick from forehand). The bot says
 *   the next value (or holds the value said) while the better of "take up the skat" (the mean over the skats) and
 *   "the best hand game" gains at least a margin: hard 0; medium a fixed random margin for the deal of +-25 points;
 *   easy +-60 (it overbids or gives up good hands now and then). Forehand, everyone passed: 18 by the same rule.
 * - The skat: hand when the best hand game (with its announcements) gains more than taking up the skat.
 * - Laying away: of the 66 pairs of the twelve cards, the two that give the game its best chance (suit games and
 *   grand never lay a trump or a jack away); the game named is the best gain over the four suits, grand, null and null
 *   ouvert (only when every suit is safe, and forehand holds a 7 to lead). A null worth less than the bid is not named.
 * - Announcements (hand games): schneider when its chance of 90 makes the higher value worth more than the plain
 *   game; schwarz always with ouvert, and only for a hand that takes every trick from forehand (its trumps the top ones
 *   in an unbroken run, as many as the others hold; each side suit an unbroken run from the ace). Null hand ouvert
 *   when every suit is safe.
 * The summary after a deal waits for a human tap (actor() -1); think() answers "N" there all the same (the tests).
 * Its moves are the game's notation ("b18", "h", "p", "s", "dTs", "gc", "ggS", "gnO", "Jc", "N"), played by
 * skat_show().
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/skat.h"
#include "games/trickbot.h"

#define SKAT_HARD_ITERS 25000                   /* about 0.15 s a card on a quiet PC (0.3 s at the first tricks) */

/* the search's state for the deal (tb_state.x) */
typedef struct {
    uint8_t type, decl, hand, flags, mat_known;
    int8_t mat;                                 /* the matadors (known to the declarer who took up the skat) */
    int16_t bid, scale;                         /* scale: the reward's (the gains of this deal over 0..1) */
    uint32_t played;                            /* the declarer's cards played this deal (bits: skt_bit()) */
} skx;

static int skt_bit(int c)
{
    int r = CARD_RANK(c);
    return CARD_SUIT(c) * 8 + (r == RANK_ACE ? 7 : r - 7);
}

static int skt_card(int b) { return CARD(b >> 3, (b & 7) == 7 ? RANK_ACE : (b & 7) + 7); }

/* ---- the score ---------------------------------------------------------------------------------------------------- */
/* the declarer's score as the rules count it (deal_end): won, and the value (raised to the bid when overbid) */
static int skt_score(const skx *x, int dpts, int dtricks, int *won)
{
    if (x->type == SG_NULL) {
        *won = dtricks == 0;
        return skat_base(SG_NULL, x->flags, x->hand);
    }
    int ann_s = x->flags & SF_SCHNEIDER ? 1 : 0, ann_z = x->flags & SF_SCHWARZ ? 1 : 0;
    int ouv = x->flags & SF_OUVERT ? 1 : 0;
    int schneider = dpts >= 90 || dpts <= 30, schwarz = dtricks == 10 || dtricks == 0;
    *won = dpts >= 61 && (!ann_s || dpts >= 90) && (!ann_z || dtricks == 10);
    int base = skat_base(x->type, x->flags, x->hand);
    int v = base * (abs(x->mat) + 1 + x->hand + (schneider || ann_s) + ann_s + (schwarz || ann_z) + ann_z + ouv);
    if (v < x->bid) {
        *won = 0;
        v = (x->bid + base - 1) / base * base;
    }
    return v;
}

/* ---- the rules for the core --------------------------------------------------------------------------------------- */
static int k_suit(const tb_state *st, int c) { return skat_suit(c, TB_CXS(st, skx)->type); }

static int k_power(const tb_state *st, int c, int led)
{
    int t = TB_CXS(st, skx)->type, s = skat_suit(c, t);
    if (s == SUIT_TRUMP) return 100 + skat_power(c, t);
    return s == led ? skat_power(c, t) : 0;
}

static int k_order(const tb_state *st, int c) { return skat_power(c, TB_CXS(st, skx)->type); }

static int k_points(const tb_state *st, int c) { return TB_CXS(st, skx)->type == SG_NULL ? 0 : skat_points(c); }

static int k_over(const tb_state *st)            /* null: lost at the declarer's first trick */
{
    const skx *x = TB_CXS(st, skx);
    return x->type == SG_NULL && st->won[x->decl] > 0;
}

/* null: the declarer ducks, the defenders lead and follow low; else everyone wants tricks */
static int k_want(const tb_state *st, int seat)
{
    const skx *x = TB_CXS(st, skx);
    if (x->type != SG_NULL) return 1;
    return seat == x->decl ? -1 : 0;
}

/* a dealt world: the declarer's matadors from his hand, his cards played and the skat (rest) */
static void k_dealt(tb_state *st)
{
    skx *x = TB_XS(st, skx);
    if (x->mat_known || x->type == SG_NULL) return;
    uint8_t c[SKAT_HAND + TB_HAND];
    int n = 0, d = x->decl;
    for (int i = 0; i < st->n[d]; i++) c[n++] = st->hand[d][i];
    for (int b = 0; b < 32; b++)
        if (x->played >> b & 1) c[n++] = (uint8_t)skt_card(b);
    for (int i = 0; i < st->nrest && i < 2; i++) c[n++] = st->rest[i];
    x->mat = (int8_t)skat_matadors(c, n, x->type);
}

/* the deal's gain for the declarer against the two others, over 0..1 (the defenders: the rest); a little of the card
 * points beyond it */
static void k_reward(const tb_state *st, float *rw)
{
    const skx *x = TB_CXS(st, skx);
    int d = x->decl, def = 0, won;
    for (int s = 0; s < st->players; s++)
        if (s != d) def += st->pts[s];
    int dp = 120 - def, v = skt_score(x, dp, st->won[d], &won);
    float gain = won ? v + 50.0f : -(2.0f * v + 90.0f);
    float r = 0.5f + gain / x->scale;
    if (x->type != SG_NULL) r += 0.04f * (dp - 60) / 60.0f;
    for (int s = 0; s < st->players; s++) rw[s] = s == d ? r : 1 - r;
}

static const tb_rules RULES = {
    .suit = k_suit, .power = k_power, .order = k_order, .points = k_points, .over = k_over, .reward = k_reward,
    .want = k_want, .dealt = k_dealt,
};

/* ---- the view ------------------------------------------------------------------------------------------------------
 * Seat me's knowledge: its hand, the others' counts, the declarer's open hand (ouvert), the declarer's own skat laid
 * away, the deal's plays replayed for the voids (from the actions since the deal began). */
void sktb_view(const skgame *g, int me, tb_state *v)
{
    const skpos *p = &g->pos;
    tb_init(v, &RULES, SKAT_P, me);
    skx *x = TB_XS(v, skx);
    int d = p->declarer, t = p->type;
    x->type = (uint8_t)t;
    x->decl = (uint8_t)d;
    x->hand = p->hand_game;
    x->flags = p->flags;
    x->bid = p->bid;
    v->trump = t == SG_NULL ? TB_NONE : SUIT_TRUMP;
    for (int s = 0; s < SKAT_P; s++) v->team[s] = s == d ? 0 : 1;
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
    for (int s = 0; s < SKAT_P; s++) {
        v->pts[s] = p->points[s];
        v->won[s] = p->won[s];
        if (s != me) tb_hidden(v, s, p->n[s]);
    }
    tb_mine(v, p->hand[me], p->n[me]);
    if ((p->flags & SF_OUVERT) && d != me)      /* the open hand: everyone sees it */
        for (int i = 0; i < p->n[d]; i++) tb_known(v, d, p->hand[d][i]);
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (SKA_KIND(g->act[i]) == SA_NEXT) start = i + 1;
    uint8_t tr[SKAT_P], who[SKAT_P];
    int nt = 0, lead = (p->dealer + 1) % SKAT_P;
    for (int i = start; i < g->n; i++) {         /* the plays: who played what, and the voids they showed */
        uint32_t a = g->act[i];
        if (SKA_KIND(a) != SA_PLAY || nt < 0 || nt >= SKAT_P) continue;
        int c = SKA_ARG(a), seat = nt ? (who[0] + nt) % SKAT_P : lead;
        tb_note(v, seat, c, tr, who, nt);
        if (seat == d) x->played |= 1u << skt_bit(c);
        tr[nt] = (uint8_t)c;
        who[nt++] = (uint8_t)seat;
        if (nt == SKAT_P) {
            lead = who[skat_trick_best(tr, SKAT_P, t)];
            for (int k = 0; k < SKAT_P; k++) tb_seen(v, tr[k]);
            nt = 0;
        }
    }
    for (int j = 0; j < nt; j++) tb_trick_add(v, who[j], tr[j]);
    int base = skat_base(t, p->flags, p->hand_game), v0 = base * (2 + p->hand_game);
    if (d == me && !p->hand_game) {             /* the declarer's skat: the two cards he laid away */
        tb_seen(v, p->skat[0]);
        tb_seen(v, p->skat[1]);
        x->mat = (int8_t)skat_matadors(p->cards12, SKAT_HAND, t);
        x->mat_known = 1;
        v0 = base * (abs(x->mat) + 1);
    }
    if (t == SG_NULL) v0 = base;
    if (v0 < p->bid) v0 = p->bid;
    x->scale = (int16_t)(4 * (2 * v0 + 90));
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_32, pack));
}

/* ---- the search: deals that agree with the auction ------------------------------------------------------------------
 * A defender's dealt worlds are kept with a weight from what the bid and the game said of the declarer's cards: in a
 * suit game his trumps (five or more: always; four: a third of the time; three: a tenth...), in grand his jacks, and
 * the matadors his bid needs (a bid over the game's value without them: a twentieth). Up to 24 deals are drawn until
 * one is kept (the last one otherwise: never stall). */
#ifndef SKT_INFER
#define SKT_INFER 1
#endif

static float skt_plausible(const tb_state *w)
{
    const skx *x = TB_CXS(w, skx);
    int d = x->decl, t = x->type, tr = 0, jacks = 0;
    for (int i = 0; i < w->n[d]; i++) tr += skat_trump(w->hand[d][i], t), jacks += CARD_RANK(w->hand[d][i]) == RANK_JACK;
    for (int b = 0; b < 32; b++)
        if (x->played >> b & 1) tr += skat_trump(skt_card(b), t), jacks += (b & 7) == RANK_JACK - 7;
    float a;
    if (t < SG_GRAND) a = tr >= 5 ? 1 : tr == 4 ? 0.35f : tr == 3 ? 0.1f : 0.02f;
    else a = jacks >= 2 ? 1 : jacks == 1 ? 0.3f : 0.1f;
    int base = skat_base(t, x->flags, x->hand), ann = 0;
    if (x->flags & SF_OUVERT) ann = 5;
    else if (x->flags & SF_SCHWARZ) ann = 4;
    else if (x->flags & SF_SCHNEIDER) ann = 2;
    int need = (x->bid + base - 1) / base - 1 - x->hand - ann;
    if (abs(x->mat) < need) a *= 0.05f;
    return a;
}

static void skt_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;
    tb_state *st = s;
    const skx *x = TB_CXS(st, skx);
    if (st->full) return;
    if (!SKT_INFER || x->type == SG_NULL || st->me == x->decl) { tb_determinise(st, r); return; }
    tb_state keep = *st;
    for (int k = 0; k < 24; k++) {
        if (k) *st = keep;
        tb_determinise(st, r);
        if (ai_rand_f(r) < skt_plausible(st)) return;
    }
}

/* the card to play: trickbot's tb_choose() with the deals above */
static int skt_search(const tb_state *v, int level, uint64_t seed, long hard_iters, ai_stats *stats)
{
    tb_state *root = malloc(sizeof *root);
    if (!root) return TB_NONE;
    *root = *v;
    tb_level_t lv = tb_level(level, hard_iters);
    root->eps = (uint8_t)lv.eps;
    uint8_t L[TB_HAND];
    int n = tb_over(root) ? 0 : tb_legal(root, L), card = TB_NONE;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, (uint64_t)root->tricks * 131 + root->ntrick * 7 + root->turn + (uint64_t)n * 977));
    if (stats) memset(stats, 0, sizeof *stats);
    if (n == 1) card = L[0];
    else if (n > 1 && lv.slip && ai_rand_n(&rng, 100) < lv.slip) {
        tb_state w = *root;                     /* a slip: the heuristic's move in one dealt world, noisy */
        skt_determinise(&w, w.turn, &rng);
        w.eps = 40;
        card = tb_heuristic(&w, L, n, &rng);
    } else if (n > 1) {
        ai_game g = {
            .size = sizeof(tb_state), .moves = tb_a_moves, .make = tb_a_make, .side = tb_a_side,
            .players = root->players, .result = tb_a_result, .playout = tb_a_playout,
            .determinise = skt_determinise,
        };
        ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .uct_c = 0.7f};
        o.budget.nodes = lv.iters;
        int m = ai_mcts(&g, root, &o, stats);
        card = m == AI_NONE ? L[0] : m;
    }
    free(root);
    return card;
}

/* ---- the hand's worth --------------------------------------------------------------------------------------------- */
enum { KC_SUIT, KC_GRAND, KC_NULL, KC_N };
enum { G_GRAND = 4, G_NULL = 5, G_NULLO = 6, G_N };   /* the games weighed: the suits, grand, null, null ouvert */
#define NF 12

/* the logistic weights (bias first), fitted on deals played out by the bot: the chance to win, to reach 90 */
static const float W_WIN[KC_N][NF] = {
    {-3.631f, 0.513f, 0.577f, 0.520f, 0.766f, 1.099f, -0.601f, 0.484f, -1.011f, 0.879f, 1.170f, 0.004f},
    {-4.427f, 1.518f, 0.405f, 2.457f, 1.036f, 0.592f, -0.843f, -0.675f, 0.788f, 1.524f, 0.099f, -0.036f},
    {2.743f, -0.491f, -0.376f, -0.466f, -0.327f, -0.109f, 0, 0, 0, 0, 0, 0},
};
static const float W_SCH[KC_N][NF] = {
    {-6.943f, 0.481f, 0.833f, 0.499f, 0.791f, 0.757f, 0.115f, 0.620f, -1.353f, 1.058f, 0.820f, 0.088f},
    {-12.252f, 1.521f, 0.446f, 2.207f, 0.959f, 0.549f, -0.672f, -0.849f, 1.076f, 1.140f, 0.210f, 1.159f},
    {-20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int has(const uint8_t *h, int n, int c) { return trick_find(h, n, c) >= 0; }

/* the order of a suit's cards outside the trumps, strongest first (no jacks) */
static const int SIDE_RANKS[7] = {RANK_ACE, 10, RANK_KING, RANK_QUEEN, 9, 8, 7};

static void feat_suit(const uint8_t *h, int n, int t, float sp, int fh, float *f)
{
    int T = 0, J = 0, hi = 0, len[4] = {0}, ace[4] = {0}, ten[4] = {0};
    int jc = has(h, n, CARD(SUIT_CLUBS, RANK_JACK)), js = has(h, n, CARD(SUIT_SPADES, RANK_JACK));
    for (int i = 0; i < n; i++) {
        int c = h[i], r = CARD_RANK(c), s = CARD_SUIT(c);
        if (r == RANK_JACK) { J++, T++; continue; }
        if (s == t) { T++, hi += r == RANK_ACE || r == 10; continue; }
        len[s]++;
        ace[s] |= r == RANK_ACE;
        ten[s] |= r == 10;
    }
    int aces = 0, at = 0, lone = 0, losers = 0, voids = 0;
    for (int s = 0; s < 4; s++) {
        if (s == t) continue;
        aces += ace[s];
        at += ace[s] && ten[s];
        lone += ten[s] && !ace[s];
        voids += !len[s];
        losers += len[s] - ace[s] - (ace[s] && ten[s]);
    }
    float v[NF] = {1, (float)T, (float)J, (float)(jc + (jc && js)), (float)hi, (float)aces, (float)at, (float)lone,
                   (float)losers, (float)voids, sp / 10, (float)fh};
    memcpy(f, v, sizeof v);
}

static void feat_grand(const uint8_t *h, int n, float sp, int fh, float *f)
{
    int J = 0, aces = 0, at = 0, lone = 0, stand = 0, losers = 0, voids = 0, longest = 0;
    int jc = has(h, n, CARD(SUIT_CLUBS, RANK_JACK)), js = has(h, n, CARD(SUIT_SPADES, RANK_JACK));
    for (int i = 0; i < n; i++) J += CARD_RANK(h[i]) == RANK_JACK;
    for (int s = 0; s < 4; s++) {
        int len = 0, run = 0, a = has(h, n, CARD(s, RANK_ACE)), t = has(h, n, CARD(s, 10));
        for (int k = 0; k < 7; k++)
            if (has(h, n, CARD(s, SIDE_RANKS[k]))) len++, run += run == k;
        aces += a;
        at += a && t;
        lone += t && !a;
        stand += run;
        losers += len - run;
        voids += !len;
        longest = len > longest ? len : longest;
    }
    float v[NF] = {1, (float)J, (float)(jc + (jc && js)), (float)aces, (float)at, (float)lone, (float)stand,
                   (float)losers, (float)voids, sp / 10, (float)fh, (float)longest};
    memcpy(f, v, sizeof v);
}

/* null: each suit's danger (how far its k-th lowest card is over 2k-1); 0: safe */
static void feat_null(const uint8_t *h, int n, int fh, float *f)
{
    int unsafe = 0, sum = 0, worst = 0, voids = 0;
    for (int s = 0; s < 4; s++) {
        int q[8], k = 0, d = 0;
        for (int r = 1; r <= 8; r++)            /* 7 8 9 10 J Q K A: null powers 1..8, lowest first */
            for (int i = 0; i < n; i++)
                if (CARD_SUIT(h[i]) == s && skat_power(h[i], SG_NULL) == r) q[k++] = r;
        if (!k) { voids++; continue; }
        for (int i = 0; i < k; i++) d = q[i] - (2 * i + 1) > d ? q[i] - (2 * i + 1) : d;
        unsafe += d > 0;
        sum += d;
        worst = d > worst ? d : worst;
    }
    float v[NF] = {1, (float)unsafe, (float)sum, (float)worst, (float)voids, (float)fh, 0, 0, 0, 0, 0, 0};
    memcpy(f, v, sizeof v);
}

/* a ten-card hand's counts for game g (0..3, G_GRAND, G_NULL); its class */
static int skt_feat(const uint8_t *h, int n, int g, float sp, int fh, float *f)
{
    if (g < G_GRAND) { feat_suit(h, n, g, sp, fh, f); return KC_SUIT; }
    if (g == G_GRAND) { feat_grand(h, n, sp, fh, f); return KC_GRAND; }
    feat_null(h, n, fh, f);
    return KC_NULL;
}

static float skt_logit(const float *w, const float *f)
{
    float z = 0;
    for (int k = 0; k < NF; k++) z += w[k] * f[k];
    return 1.0f / (1.0f + expf(-z));
}

static int hand_points(const uint8_t *h, int n)
{
    int t = 0;
    for (int i = 0; i < n; i++) t += skat_points(h[i]);
    return t;
}

/* a hand's chances in game g (0..5): win, 90; null: safe (every suit) */
static float skt_chance(const uint8_t *h, int n, int g, float sp, int fh, float *ps, int *safe)
{
    float f[NF];
    int k = skt_feat(h, n, g, sp, fh, f);
    if (ps) *ps = skt_logit(W_SCH[k], f);
    if (safe) *safe = k == KC_NULL && f[1] == 0;
    return skt_logit(W_WIN[k], f);
}

/* null ouvert: every suit safe and, leading first, a 7 to lead */
static int null_open_ok(const uint8_t *h, int n, int safe, int fh)
{
    if (!safe) return 0;
    if (!fh) return 1;
    for (int i = 0; i < n; i++)
        if (CARD_RANK(h[i]) == 7) return 1;
    return 0;
}

/* the best two of twelve cards to lay away for game g (0..5): the highest chance to win (suit games and grand never
 * lay a trump away, unless the hand holds nothing else); d[2] the cards, *ps the chance of 90, *safe (null) */
static float skt_discard(const uint8_t *h12, int g, int fh, uint8_t *d, float *ps, int *safe)
{
    float best = -1;
    for (int pass = 0; pass < 2 && best < 0; pass++)
        for (int i = 0; i < SKAT_HAND; i++)
            for (int j = i + 1; j < SKAT_HAND; j++) {
                if (!pass && g <= G_GRAND && (skat_trump(h12[i], g) || skat_trump(h12[j], g))) continue;
                uint8_t h[SKAT_HAND];
                int n = 0;
                for (int k = 0; k < SKAT_HAND; k++)
                    if (k != i && k != j) h[n++] = h12[k];
                float s, p;
                int sf;
                p = skt_chance(h, n, g, (float)(skat_points(h12[i]) + skat_points(h12[j])), fh, &s, &sf);
                if (p > best) {
                    best = p;
                    d[0] = h12[i], d[1] = h12[j];
                    if (ps) *ps = s;
                    if (safe) *safe = null_open_ok(h, n, sf, fh);
                }
            }
    return best;
}

/* a hand that takes every trick leading first: its trumps the top ones unbroken, as many as the others hold; each side
 * suit an unbroken run from the ace */
static int sure_all(const uint8_t *h, int n, int g)
{
    static const int jacks[4] = {SUIT_CLUBS, SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS};
    int mine = 0, run = 1, total = g == G_GRAND ? 4 : 11;
    for (int k = 0; k < total; k++) {
        int c = k < 4 ? CARD(jacks[k], RANK_JACK) : CARD(g, SIDE_RANKS[k - 4]);
        if (has(h, n, c)) {
            if (!run) return 0;
            mine++;
        } else
            run = 0;
    }
    if (mine < total - mine) return 0;
    for (int s = 0; s < 4; s++) {
        if (s == g) continue;
        run = 1;
        for (int k = 0; k < 7; k++) {
            if (has(h, n, CARD(s, SIDE_RANKS[k]))) {
                if (!run) return 0;
            } else
                run = 0;
        }
    }
    return 1;
}

/* the gain of a game against the two others (see the header): g 0..6, announced flags, hand, matadors, the bid,
 * the chances to win and to reach 90 */
static float skt_gain(int g, int flags, int hand, int mat, int bid, float p, float ps)
{
    if (g >= G_NULL) {
        int v = skat_base(SG_NULL, g == G_NULLO ? SF_OUVERT : 0, hand);
        if (v < bid) return -1e6f;
        return p * (v + 50) - (1 - p) * (2.0f * v + 90);
    }
    int base = skat_base(g, 0, hand), lv = abs(mat) + 1 + hand;
    float pw = p, extra = ps * base;
    if (flags & SF_OUVERT) lv += 5, pw = 0.98f, extra = 0;   /* schneider, schwarz, both announced, ouvert */
    else if (flags & SF_SCHNEIDER) lv += 2, pw = ps, extra = 0;
    int v = base * lv;
    if (v < bid) {                              /* overbid: only schneider lifts it */
        if (flags || v + base < bid) return -(2.0f * ((bid + base - 1) / base * base) + 90);
        v += base;
        pw = ps;
        extra = 0;
    }
    return pw * (v + 50) + extra - (1 - pw) * (2.0f * v + 90);
}

/* ---- the bidding and the skat: every skat weighed ---------------------------------------------------------------- */
#define NSKATS 231                              /* the pairs of the 22 cards not held */

typedef struct {
    int ns;
    float p[NSKATS][G_N], ps[NSKATS][G_N];      /* taking up each skat: each game's chances (its best skat laid away) */
    int8_t mat[NSKATS][G_GRAND + 1];            /* the matadors with that skat */
    float hp[G_N], hps[G_N];                    /* hand: the chances, the skat unknown */
    int sure;                                   /* hand: the games that take every trick (bits) */
} skweigh;

static void skt_weigh(const uint8_t *h10, int fh, skweigh *E)
{
    uint8_t pack[CARDS_MAX], un[32], h12[SKAT_HAND], d[2];
    int np = cards_pack(PACK_32, pack), nu = 0;
    for (int i = 0; i < np; i++)
        if (!has(h10, 10, pack[i])) un[nu++] = pack[i];
    memcpy(h12, h10, 10);
    E->ns = 0;
    for (int a = 0; a < nu; a++)
        for (int b = a + 1; b < nu && E->ns < NSKATS; b++) {
            int s = E->ns++, safe = 0;
            h12[10] = un[a], h12[11] = un[b];
            for (int g = 0; g <= G_NULL; g++) {
                E->p[s][g] = skt_discard(h12, g, fh, d, &E->ps[s][g], &safe);
                if (g <= G_GRAND) E->mat[s][g] = (int8_t)skat_matadors(h12, SKAT_HAND, g);
            }
            E->p[s][G_NULLO] = safe ? E->p[s][G_NULL] : 0;
            E->ps[s][G_NULLO] = 0;
        }
    float sp = 2.0f * (120 - hand_points(h10, 10)) / 22;
    int safe = 0;
    E->sure = 0;
    for (int g = 0; g <= G_NULL; g++) {
        E->hp[g] = skt_chance(h10, 10, g, sp, fh, &E->hps[g], &safe);
        if (g <= G_GRAND && fh && sure_all(h10, 10, g)) E->sure |= 1 << g;
    }
    E->hp[G_NULLO] = null_open_ok(h10, 10, safe, fh) ? E->hp[G_NULL] : 0;
    E->hps[G_NULLO] = 0;
}

/* at a bid: the mean gain of taking up the skat, the best hand game's (its game and announcements) */
static void skt_options(const skweigh *E, int bid, float *pick, float *hand, int *hg, int *hf)
{
    float sum = 0;
    for (int s = 0; s < E->ns; s++) {
        float best = -1e9f;
        for (int g = 0; g < G_N; g++) {
            float e = skt_gain(g, 0, 0, g <= G_GRAND ? E->mat[s][g] : 0, bid, E->p[s][g], E->ps[s][g]);
            best = e > best ? e : best;
        }
        sum += best;
    }
    *pick = E->ns ? sum / E->ns : -1e9f;
    *hand = -1e9f, *hg = SUIT_CLUBS, *hf = 0;
    for (int g = 0; g < G_N; g++) {
        static const int ann[3] = {0, SF_SCHNEIDER, SF_SCHNEIDER | SF_SCHWARZ | SF_OUVERT};
        for (int k = 0; k < 3; k++) {
            if (k && g > G_GRAND) break;
            if (k == 2 && !(E->sure >> g & 1)) continue;
            float m = 0;
            if (g > G_GRAND) m = skt_gain(g, 0, 1, 0, bid, E->hp[g], 0);
            else {
                for (int s = 0; s < E->ns; s++) m += skt_gain(g, ann[k], 1, E->mat[s][g], bid, E->hp[g], E->hps[g]);
                m /= E->ns ? E->ns : 1;
            }
            if (g == G_NULLO && E->hp[g] <= 0) continue;
            if (m > *hand) *hand = m, *hg = g, *hf = ann[k];
        }
    }
}

/* the bot's margin for this deal (easy and medium: a random one, the same through the deal) */
static float skt_margin(const skpos *p, int me, int level)
{
    if (level >= AI_HARD) return 0;
    uint64_t key = (uint64_t)p->deal * 977 + (uint64_t)me * 31 + (uint64_t)level;
    for (int i = 0; i < p->n[me]; i++) key = ai_mix(key, p->hand[me][i]);
    ai_rng r;
    ai_seed(&r, key);
    float span = level <= AI_EASY ? 60 : 25;
    return (float)(ai_rand_f(&r) * 2 - 1) * span;
}

static int forehand_of(const skpos *p) { return (p->dealer + 1) % SKAT_P; }

static uint32_t skt_call(const skpos *p, int me, int level)
{
    skweigh *E = malloc(sizeof *E);
    if (!E) return skat_auto(p);
    skt_weigh(p->hand[me], me == forehand_of(p), E);
    float pick, hand, margin = skt_margin(p, me, level);
    int hg, hf;
    uint32_t a;
    if (p->phase == SK_BID) {
        int b = skat_saying(p) ? skat_next_bid(p->bid) : p->bid;
        int go = 0;
        if (b) {
            skt_options(E, b, &pick, &hand, &hg, &hf);
            go = (pick > hand ? pick : hand) >= margin;
        }
        a = skat_saying(p) ? (go ? SA_SAY | (uint32_t)b : SA_PASS) : go ? SA_HOLD : SA_PASS;
    } else {                                    /* SK_SKAT: hand, or take it up */
        skt_options(E, p->bid, &pick, &hand, &hg, &hf);
        int type = hg == G_NULLO ? SG_NULL : hg, flags = hg == G_NULLO ? SF_OUVERT : hf;
        a = hand > pick && skat_game_ok(p, type, flags) ? SKA_GAME(type, flags) : SA_PICKUP;
    }
    free(E);
    return a;
}

/* the skat taken up: the cards to lay away, then the game (its chances with the ten cards kept) */
static uint32_t skt_lay(const skpos *p, int me)
{
    uint8_t h12[SKAT_HAND], d[2], bd[2] = {0, 0};
    int n = 0, fh = me == forehand_of(p);
    for (int i = 0; i < p->n[me]; i++) h12[n++] = p->hand[me][i];
    if (p->nskat == 2) {                        /* laid away: the best game for the ten kept */
        float sp = (float)(skat_points(p->skat[0]) + skat_points(p->skat[1])), best = -1e9f;
        for (int i = 0; i < 2; i++) h12[n++] = p->skat[i];
        int bg = SUIT_CLUBS;
        for (int g = 0; g <= G_NULLO; g++) {
            float ps, p0;
            int safe;
            p0 = skt_chance(p->hand[me], p->n[me], g == G_NULLO ? G_NULL : g, sp, fh, &ps, &safe);
            if (g == G_NULLO && !null_open_ok(p->hand[me], p->n[me], safe, fh)) continue;
            int t = g == G_NULLO ? SG_NULL : g;
            float e = skt_gain(g, 0, 0, t < SG_NULL ? skat_matadors(h12, SKAT_HAND, t) : 0, p->bid, p0, ps);
            if (e > best && skat_game_ok(p, t, g == G_NULLO ? SF_OUVERT : 0)) best = e, bg = g;
        }
        return bg == G_NULLO ? SKA_GAME(SG_NULL, SF_OUVERT) : SKA_GAME(bg, 0);
    }
    for (int i = 0; i < p->nskat; i++) h12[n++] = p->skat[i];
    if (n != SKAT_HAND) return skat_auto(p);
    for (int i = 1; i < n; i++)                 /* in a fixed order: the same plan whatever lies in the skat */
        for (int j = i; j > 0 && h12[j] < h12[j - 1]; j--) {
            uint8_t t = h12[j]; h12[j] = h12[j - 1]; h12[j - 1] = t;
        }
    float best = -1e9f;
    for (int g = 0; g <= G_NULLO; g++) {
        float ps;
        int safe;
        float p0 = skt_discard(h12, g == G_NULLO ? G_NULL : g, fh, d, &ps, &safe);
        if (g == G_NULLO && !safe) continue;
        int t = g == G_NULLO ? SG_NULL : g;
        float e = skt_gain(g, 0, 0, t < SG_NULL ? skat_matadors(h12, SKAT_HAND, t) : 0, p->bid, p0, ps);
        if (t == SG_NULL && skat_base(SG_NULL, g == G_NULLO ? SF_OUVERT : 0, 0) < p->bid) continue;
        if (e > best) best = e, bd[0] = d[0], bd[1] = d[1];
    }
    for (int i = 0; i < p->nskat; i++)          /* a card in the skat not meant for it: back to the hand */
        if (p->skat[i] != bd[0] && p->skat[i] != bd[1]) return SA_TAKE | p->skat[i];
    for (int i = 0; i < 2; i++)
        if (trick_find(p->skat, p->nskat, bd[i]) < 0) return SA_PUT | bd[i];
    return skat_auto(p);
}

/* the action for seat at a level (0: nothing to do) */
uint32_t sktb_choose(const skgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const skpos *p = &g->pos;
    if (stats) memset(stats, 0, sizeof *stats);
    if (p->phase == SK_DEAL) return SA_NEXT;
    if (p->phase == SK_OVER || p->turn != seat) return 0;
    uint32_t a = 0;
    if (p->phase == SK_BID || p->phase == SK_SKAT) a = skt_call(p, seat, level);
    else if (p->phase == SK_DISCARD) a = skt_lay(p, seat);
    else {
        tb_state *v = malloc(sizeof *v);
        if (v) {
            sktb_view(g, seat, v);
            int c = skt_search(v, level, seed, SKAT_HARD_ITERS, stats);
            if (c != TB_NONE) a = SA_PLAY | (uint32_t)c;
        }
        free(v);
    }
    return a && skat_legal(p, a) ? a : skat_auto(p);
}

/* an action in the notation */
static int skt_name(uint32_t a, char *move, int cap)
{
    char c[3];
    int k = SKA_ARG(a);
    switch (SKA_KIND(a)) {
    case SA_SAY: snprintf(move, (size_t)cap, "b%d", k); return 1;
    case SA_HOLD: snprintf(move, (size_t)cap, "h"); return 1;
    case SA_PASS: snprintf(move, (size_t)cap, "p"); return 1;
    case SA_PICKUP: snprintf(move, (size_t)cap, "s"); return 1;
    case SA_PUT: trick_name(k, c); snprintf(move, (size_t)cap, "d%s", c); return 1;
    case SA_TAKE: trick_name(k, c); snprintf(move, (size_t)cap, "u%s", c); return 1;
    case SA_GAME: {
        int f = k >> 4;
        snprintf(move, (size_t)cap, "g%c%s%s%s", "shdcgn"[(k & 15) < 6 ? (k & 15) : 0], f & SF_SCHNEIDER ? "S" : "",
                 f & SF_SCHWARZ ? "Z" : "", f & SF_OUVERT ? "O" : "");
        return 1;
    }
    case SA_PLAY: trick_name(k, c); snprintf(move, (size_t)cap, "%s", c); return 1;
    case SA_NEXT: snprintf(move, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static int think(int seat, int level, char *move, int cap)
{
    skgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *skat_live();                          /* the copy, at entry: nothing else is read */
    uint32_t a = sktb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    return skt_name(a, move, cap);
}

static int actor(void)
{
    const skpos *p = &skat_live()->pos;
    return p->phase < SK_DEAL ? p->turn : -1;
}

static const bot skat_bot = {.think = think, .actor = actor, .play = skat_show};
REGISTER_BOT(skat, &skat_bot)
