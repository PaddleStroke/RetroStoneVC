/*
 * Doppelkopf's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo
 * over deals of the unseen cards of the double pack that keep to the suits each player has shown void), the parties
 * playing for their side, the reward the deal's score as the rules count it (the game value, the announcements, the
 * extra points; a solo's). Runs on the worker thread: it copies the game at entry and reads only what its seat may
 * know: its hand, the tricks played, the reservations' words (healthy or not), the game once declared, the
 * announcements, the queens of clubs shown. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The hidden parties: in a normal game the Re players are the holders of the queens of clubs. Each dealt world
 * (dealt()) makes them so: a queen of clubs played, or dealt into a hand, makes its seat Re, the others Kontra; a
 * player who announced Re holds one (it is placed in his hand), one who announced Kontra, or is known Kontra, none.
 * Both queens of clubs in one unknown hand would be a silent solo: players holding both mostly marry, so 9 worlds in 10
 * move one of them to another hand (swapped with a trump, so the voids still hold). A declared solo, the silent solo
 * of the bot's own hand and a marriage are known parties; a marriage not yet settled is settled in the search as in
 * the rules (the first of the first three tricks another takes).
 *
 * The calls (rule-based from the hand, each checked by deals played out by the core's heuristic, tb_simulate-like):
 *   T = the hand's trumps in the game looked at, H = its top trumps (the Dullen, queens of clubs and of spades; in a
 *   queens' or jacks' solo the queens or jacks of clubs and spades), A = its aces of plain suits.
 * - Reservations: a suit solo in the suit of most trumps when T >= 9 and H >= 3, or T >= 10 and H >= 2; a queens'
 *   (jacks') solo with 5 queens (jacks) holding both of clubs or spades and A >= 3, or 6 and A >= 2; fleshless with
 *   6 aces, or 5 aces and 3 tens. Holding both queens of clubs: a silent solo (healthy said) when T >= 9 and H >= 4,
 *   else the marriage. A solo goes ahead only if its played-out deals win at least 62 in 100 (the best one if
 *   several); else healthy.
 * - Announcements, at the bot's turn while the rules allow them: Re or Kontra (or the reply) when T >= 6 and H >= 2,
 *   T >= 7 and H >= 1, or T >= 5, H >= 2 and A >= 2 (a solo's soloist: always looked at), and the played-out deals
 *   score for its party at least 66 in 100 (easy 75); no 90 (medium and hard) at 80 in 100 made, no 60, no 30 and
 *   schwarz (hard only) at 85, 90 and 95; Kontra's bars 5 higher. Each must also raise the expected score.
 * The deal's summary waits for a human tap (actor() -1); think() answers "N" there all the same (the tests).
 * Its moves are the game's notation ("g", "w", "q", "j", "f", "sh", "a", "Qc", "N"), played by doppelkopf_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/doppelkopf.h"
#include "games/trickbot.h"

#define DK_HARD_ITERS 20000                     /* about 0.2 s a card on a quiet PC (0.3 s at the first tricks) */
#define DK_QC CARD(SUIT_CLUBS, RANK_QUEEN)
#define DK_QS CARD(SUIT_SPADES, RANK_QUEEN)
#define DK_FOX CARD(SUIT_DIAMONDS, RANK_ACE)
#define DK_CHARLIE CARD(SUIT_CLUBS, RANK_JACK)
#define DK_DULLE CARD(SUIT_HEARTS, 10)

typedef struct {
    uint8_t game;                               /* the game as the observer knows it (another's silent solo: normal) */
    uint8_t bride;                              /* a marriage not yet settled: the bride (DKO_NONE: none) */
    uint8_t fixed;                              /* the parties known (a solo, a marriage): dealt() keeps them */
    uint8_t ann[2];                             /* Re's and Kontra's announcements */
    uint8_t qcp[TK4_P], qck[TK4_P];             /* queens of clubs each seat played; placed in its hand (announced) */
    uint8_t nfox, fox_from[2], fox_to[2];       /* the foxes taken: who played it, who took it */
    uint8_t doko[TK4_P];                        /* tricks of 40 or more each seat took */
    uint8_t karl;                               /* the last trick taken with the jack of clubs: by whom (DKO_NONE) */
} dkx;

#define DX(st) TB_XS(st, dkx)
#define DCX(st) TB_CXS(st, dkx)

/* dko_power() inline (the search calls it most): the strength of a card in its suit, trumps 70 and over */
static inline int dk_pow(int c, int g)
{
    static const int8_t PLAIN[14] = {0, 6, 0, 0, 0, 0, 0, 0, 0, 1, 5, 2, 3, 4};   /* A 10 K Q J 9 */
    static const int8_t HIGH[4] = {2, 1, 0, 3};
    int r = CARD_RANK(c) & 15, s = CARD_SUIT(c) & 3;
    r = r > 13 ? 0 : r;
    if (g <= DKG_SILENT || g >= DKG_SUIT) {
        if (s == SUIT_HEARTS && r == 10) return 100;
        if (r == RANK_QUEEN) return 90 + HIGH[s];
        if (r == RANK_JACK) return 80 + HIGH[s];
        return s == (g >= DKG_SUIT ? g - DKG_SUIT : SUIT_DIAMONDS) ? 70 + PLAIN[r] : PLAIN[r];
    }
    if (g == DKG_QUEENS && r == RANK_QUEEN) return 90 + HIGH[s];
    if (g == DKG_JACKS && r == RANK_JACK) return 80 + HIGH[s];
    return PLAIN[r];
}

/* the tests: the same as the rules */
int dkb_power(int c, int g) { return dk_pow(c, g); }

static int d_suit(const tb_state *st, int c) { return dk_pow(c, DCX(st)->game) >= 70 ? SUIT_TRUMP : CARD_SUIT(c); }

static int d_power(const tb_state *st, int c, int led)
{
    int p = dk_pow(c, DCX(st)->game);
    return p >= 70 || CARD_SUIT(c) == led ? p : 0;
}

static int d_points(const tb_state *st, int c) { (void)st; return dko_points(c); }

static void d_trick_end(tb_state *st, int w)
{
    dkx *x = DX(st);
    int eyes = 0;
    for (int i = 0; i < st->ntrick; i++) {
        eyes += dko_points(st->trick[i]);
        if (st->trick[i] == DK_FOX && x->nfox < 2) {
            x->fox_from[x->nfox] = st->who[i];
            x->fox_to[x->nfox++] = (uint8_t)w;
        }
    }
    if (eyes >= 40) x->doko[w]++;
    if (st->tricks == DKO_HAND - 1 && st->trick[tb_best(st)] == DK_CHARLIE) x->karl = (uint8_t)w;
    if (x->bride != DKO_NONE) {                 /* the marriage: the first trick another takes, of the first three */
        if (w != x->bride) st->team[w] = st->team[x->bride], x->bride = DKO_NONE;
        else if (st->tricks + 1 >= 3) x->bride = DKO_NONE;
    }
}

/* the party's game made (as the rules' reached()) */
static int d_reached(const uint8_t *ann, const int *eyes, const int *tricks, int k)
{
    static const int under[3] = {90, 60, 30};
    int o = 1 - k, only_kontra = ann[1] && !ann[0];
    if (ann[k] == DKL_BLACK) return tricks[o] == 0;
    if (ann[k] >= DKL_90) return eyes[o] < under[ann[k] - DKL_90];
    return k == 0 ? eyes[0] >= (only_kontra ? 120 : 121) : eyes[1] >= (only_kontra ? 121 : 120);
}

/* each Re player's points (a solo: the opponents' unit), as the rules count them; *solo: one Re player */
static int d_re_points(const tb_state *st, const uint8_t *ann, int *solo, int *eyes_re)
{
    const dkx *x = DCX(st);
    int eyes[2] = {0, 0}, tricks[2] = {0, 0}, nre = 0;
    for (int s = 0; s < TK4_P; s++) {
        int k = st->team[s] != 0;
        nre += !k;
        eyes[k] += st->pts[s];
        tricks[k] += st->won[s];
    }
    *solo = nre == 1;
    *eyes_re = eyes[0];
    int re_ok = d_reached(ann, eyes, tricks, 0), ko_ok = d_reached(ann, eyes, tricks, 1);
    int a0 = ann[0] >= DKL_90, a1 = ann[1] >= DKL_90, w;
    if (!a1) w = re_ok ? 0 : 1;
    else if (!a0) w = ko_ok ? 1 : 0;
    else w = re_ok ? 0 : ko_ok ? 1 : -1;
    int v = 0;
    if (w >= 0) {                               /* the game value */
        int l = 1 - w;
        v = 1 + (eyes[l] < 90) + (eyes[l] < 60) + (eyes[l] < 30) + (tricks[l] == 0);
        for (int k = 0; k < 2; k++) v += (ann[k] ? 2 : 0) + (ann[k] > DKL_PARTY ? ann[k] - DKL_PARTY : 0);
        v += (ann[l] >= DKL_90 && eyes[w] >= 120) + (ann[l] >= DKL_60 && eyes[w] >= 90) +
             (ann[l] >= DKL_30 && eyes[w] >= 60) + (ann[l] >= DKL_BLACK && eyes[w] >= 30);
        if (w == 1) v = -v;
    }
    if (!*solo) {                               /* the extra points */
        for (int i = 0; i < x->nfox; i++) {
            int k = st->team[x->fox_to[i]] != 0;
            if ((st->team[x->fox_from[i]] != 0) != k) v += k ? -1 : 1;
        }
        for (int s = 0; s < TK4_P; s++) v += st->team[s] ? -x->doko[s] : x->doko[s];
        if (x->karl != DKO_NONE) v += st->team[x->karl] ? -1 : 1;
        v -= w == 1;                            /* against the Alten */
    }
    return v;
}

/* the reward: Re's points over 8 a side (a solo's soloist alike: three times the unit), a little of the card points */
static float d_re_reward(const tb_state *st, const uint8_t *ann)
{
    int solo, eyes;
    int v = d_re_points(st, ann, &solo, &eyes);
    return 0.5f + 0.5f * (float)v / 8.0f + 0.03f * (float)(eyes - 120) / 120.0f;
}

static void d_reward(const tb_state *st, float *rw)
{
    float r = d_re_reward(st, DCX(st)->ann);
    for (int s = 0; s < st->players; s++) rw[s] = st->team[s] ? 1 - r : r;
}

/* each dealt world: the parties from the queens of clubs (a silent solo in an unknown hand mostly undone) */
static void d_dealt(tb_state *st)
{
    dkx *x = DX(st);
    if (x->fixed) return;
    int cnt[TK4_P];
    unsigned h = 0;
    for (int s = 0; s < TK4_P; s++) {
        cnt[s] = x->qcp[s];
        for (int i = 0; i < st->n[s]; i++) {
            cnt[s] += st->hand[s][i] == DK_QC;
            h = h * 31u + st->hand[s][i];
        }
    }
    for (int s = 0; s < TK4_P; s++) {
        if (s == st->me || cnt[s] < 2 || x->qcp[s] + x->qck[s] >= 2 || (h >> 7) % 10 == 0) continue;
        int i = st->n[s] - 1;                   /* the dealt one: after the known ones */
        while (i >= 0 && st->hand[s][i] != DK_QC) i--;
        int done = 0;
        for (int q = 0; q < TK4_P && !done && i >= 0; q++) {
            if (q == s || q == st->me || !tb_fits(st, q, DK_QC)) continue;
            for (int j = st->n[q] - 1; j >= x->qck[q] && !done; j--) {
                int c = st->hand[q][j];
                if (c == DK_QC || !dko_trump(c, x->game) || !tb_fits(st, s, c)) continue;
                st->hand[q][j] = DK_QC;
                st->hand[s][i] = (uint8_t)c;
                cnt[s]--, cnt[q]++;
                done = 1;
            }
        }
    }
    for (int s = 0; s < TK4_P; s++) st->team[s] = cnt[s] ? 0 : 1;
}

static const tb_rules RULES = {
    .suit = d_suit, .power = d_power, .points = d_points, .trick_end = d_trick_end, .reward = d_reward,
    .dealt = d_dealt,
};

/* the cards in play: 9 to ace of each suit, twice */
static int pack48(uint8_t *out)
{
    static const int ranks[6] = {RANK_ACE, 9, 10, RANK_JACK, RANK_QUEEN, RANK_KING};
    int n = 0;
    for (int k = 0; k < 2; k++)
        for (int s = 0; s < 4; s++)
            for (int r = 0; r < 6; r++) out[n++] = CARD(s, ranks[r]);
    return n;
}

static int count_card(const uint8_t *h, int n, int c)
{
    int k = 0;
    for (int i = 0; i < n; i++) k += h[i] == c;
    return k;
}

/* the view of seat me, the game played as `game` with this soloist (the calls look at games not yet played) */
static void view(const dkpos *p, int me, int game, int soloist, tb_state *v)
{
    const tk4_table *t = &p->t;
    tb_init(v, &RULES, TK4_P, me);
    dkx *x = DX(v);
    x->bride = x->karl = DKO_NONE;
    x->game = (uint8_t)game;
    v->trump = game == DKG_FLESHLESS ? TB_NONE : SUIT_TRUMP;
    v->turn = (uint8_t)tk4_turn(t);
    v->leader = t->leader;
    v->tricks = t->tricks;
    for (int s = 0; s < TK4_P; s++) {
        v->pts[s] = t->eyes[s];
        v->won[s] = t->won[s];
        x->qcp[s] = p->qc[s];
        if (s != me) tb_hidden(v, s, t->n[s]);
    }
    tb_mine(v, t->hand[me], t->n[me]);
    x->ann[0] = p->ann[0];
    x->ann[1] = p->ann[1];
    /* the deal's tricks replayed (they are on the table's record): the voids, the extra points so far */
    for (int k = 0; k <= t->tricks && k < DKO_HAND; k++) {
        int lead = p->hist_leader[k], m = k < t->tricks ? TK4_P : t->ntrick;
        uint8_t who[TK4_P];
        for (int j = 0; j < m; j++) who[j] = (uint8_t)((lead + j) % TK4_P);
        for (int j = 0; j < m; j++) tb_note(v, who[j], p->hist[k][j], p->hist[k], who, j);
        if (m < TK4_P) break;
        const uint8_t *c = p->hist[k];
        int w = who[dko_winner(c, TK4_P, game)], eyes = 0;
        for (int j = 0; j < TK4_P; j++) {
            tb_seen(v, c[j]);
            eyes += dko_points(c[j]);
            if (c[j] == DK_FOX && x->nfox < 2) x->fox_from[x->nfox] = who[j], x->fox_to[x->nfox++] = (uint8_t)w;
        }
        if (eyes >= 40) x->doko[w]++;
        if (k == DKO_HAND - 1 && c[dko_winner(c, TK4_P, game)] == DK_CHARLIE) x->karl = (uint8_t)w;
    }
    for (int j = 0; j < t->ntrick; j++) tb_trick_add(v, (t->leader + j) % TK4_P, t->trick[j]);
    /* the parties */
    if (dko_is_solo(game) || game == DKG_SILENT) {
        x->fixed = 1;
        for (int s = 0; s < TK4_P; s++) v->team[s] = s != soloist;
    } else if (game == DKG_WEDDING) {
        x->fixed = 1;
        int open = p->phase == DKP_PLAY && p->open;
        for (int s = 0; s < TK4_P; s++) v->team[s] = open ? (uint8_t)dko_party(p, s) : s != soloist;
        if (!open) x->bride = (uint8_t)soloist;
        if (soloist != me)
            for (int k = p->qc[soloist]; k < 2; k++) tb_known(v, soloist, DK_QC);
    } else {
        int placed = count_card(t->hand[me], t->n[me], DK_QC);
        for (int s = 0; s < TK4_P; s++) placed += p->qc[s];
        for (int q = 0; q < TK4_P; q++) {
            if (q == me) continue;
            int k = p->phase == DKP_PLAY ? dko_known(p, q) : -1;   /* the table's knowledge */
            if (k == 1) tb_exclude(v, q, DK_QC);
            else if (k == 0 && !p->qc[q] && placed < 2) {
                tb_known(v, q, DK_QC);
                x->qck[q]++;
                placed++;
            }
        }
        for (int s = 0; s < TK4_P; s++) v->team[s] = s != me;   /* (each world sets them) */
    }
    uint8_t pack[48];
    tb_finish(v, pack, pack48(pack));
}

/* the game as seat me knows it: another's silent solo looks like a normal game */
static int known_game(const dkpos *p, int me)
{
    if (p->phase != DKP_PLAY) return DKG_NORMAL;
    return p->game == DKG_SILENT && p->soloist != me ? DKG_NORMAL : p->game;
}

void dkb_view(const dkgame *g, int me, tb_state *v)
{
    const dkpos *p = &g->pos;
    view(p, me, known_game(p, me), p->soloist, v);
}

/* the tests: each Re player's points as the core counts a deal played out (a dealt or revealed state); ann: the
 * announcements (NULL: the state's) */
int dkb_re_points(const tb_state *st, const uint8_t *ann)
{
    int solo, eyes;
    return d_re_points(st, ann ? ann : DCX(st)->ann, &solo, &eyes);
}

/* ---- the calls -------------------------------------------------------------------------------------------------- */
typedef struct { int T, H, A, tens, Q, J, QH, JH; } dkhand;

static dkhand strength(const uint8_t *h, int n, int game)
{
    dkhand s = {0};
    for (int i = 0; i < n; i++) {
        int c = h[i], r = CARD_RANK(c), u = CARD_SUIT(c);
        int top = c == DK_DULLE || c == DK_QC || c == DK_QS;
        if (game == DKG_QUEENS) top = r == RANK_QUEEN && (u == SUIT_CLUBS || u == SUIT_SPADES);
        if (game == DKG_JACKS) top = r == RANK_JACK && (u == SUIT_CLUBS || u == SUIT_SPADES);
        if (dko_trump(c, game)) s.T++, s.H += top;
        else s.A += r == RANK_ACE, s.tens += r == 10;
        s.Q += r == RANK_QUEEN;
        s.J += r == RANK_JACK;
        s.QH += r == RANK_QUEEN && (u == SUIT_CLUBS || u == SUIT_SPADES);
        s.JH += r == RANK_JACK && (u == SUIT_CLUBS || u == SUIT_SPADES);
    }
    return s;
}

/* deals of the unseen cards played out by the heuristic (everyone for his party): seat me's average reward, and how
 * often its party scored; with `ann` given, the same deals scored with those announcements as well */
static void playouts(const tb_state *v, int samples, ai_rng *r, const uint8_t *ann, float *rw0, float *win0,
                     float *rw1, float *win1)
{
    float a0 = 0, a1 = 0, w0 = 0, w1 = 0;
    int done = 0;
    tb_state *w = malloc(sizeof *w);
    if (w)
        for (; done < samples && !ai_cancelled(); done++) {
            *w = *v;
            tb_determinise(w, r);
            uint8_t L[TB_HAND];
            for (int guard = 0; guard < 200 && !tb_over(w); guard++) {
                int n = tb_legal(w, L);
                if (!n) break;
                tb_play(w, tb_heuristic(w, L, n, r));
            }
            int mine = w->team[w->me] == 0;
            float x0 = d_re_reward(w, DX(w)->ann);
            x0 = mine ? x0 : 1 - x0;
            a0 += x0, w0 += x0 > 0.5f;
            if (ann) {
                float x1 = d_re_reward(w, ann);
                x1 = mine ? x1 : 1 - x1;
                a1 += x1, w1 += x1 > 0.5f;
            }
        }
    free(w);
    float d = done ? (float)done : 1;
    if (rw0) *rw0 = a0 / d;
    if (win0) *win0 = w0 / d;
    if (rw1) *rw1 = a1 / d;
    if (win1) *win1 = w1 / d;
}

/* the reservation: a solo, the marriage, or healthy (a silent solo too) */
static int reserve(const dkpos *p, int me, int level, uint64_t seed)
{
    const uint8_t *h = p->t.hand[me];
    int n = p->t.n[me], cand[8], nc = 0, qc = count_card(h, n, DK_QC);
    int best_suit = -1, best_T = -1;
    for (int s = 0; s < 4; s++) {
        dkhand k = strength(h, n, DKG_SUIT + s);
        if (k.T > best_T) best_T = k.T, best_suit = s;
    }
    dkhand k = strength(h, n, DKG_SUIT + best_suit);
    if ((k.T >= 9 && k.H >= 3) || (k.T >= 10 && k.H >= 2)) cand[nc++] = DKG_SUIT + best_suit;
    k = strength(h, n, DKG_QUEENS);
    if ((k.Q >= 5 && k.QH >= 2 && k.A >= 3) || (k.Q >= 6 && k.A >= 2)) cand[nc++] = DKG_QUEENS;
    k = strength(h, n, DKG_JACKS);
    if ((k.J >= 5 && k.JH >= 2 && k.A >= 3) || (k.J >= 6 && k.A >= 2)) cand[nc++] = DKG_JACKS;
    k = strength(h, n, DKG_FLESHLESS);
    if (k.A >= 6 || (k.A >= 5 && k.tens >= 3)) cand[nc++] = DKG_FLESHLESS;
    k = strength(h, n, DKG_NORMAL);
    if (qc == 2 && k.T >= 9 && k.H >= 4) cand[nc++] = DKG_SILENT;
    int choice = qc == 2 ? DKG_WEDDING : DKG_NORMAL;
    if (nc) {
        tb_state *v = malloc(sizeof *v);
        float best = 0;
        if (v) {
            ai_rng r;
            ai_seed(&r, ai_mix(seed, (uint64_t)p->deal * 16 + me));
            int samples = level <= AI_EASY ? 24 : level == AI_MEDIUM ? 48 : 96;
            for (int i = 0; i < nc; i++) {
                float rw, win;
                view(p, me, cand[i], me, v);
                playouts(v, samples, &r, NULL, &rw, &win, NULL, NULL);
                if (win >= 0.62f && rw > best) best = rw, choice = cand[i];
            }
        }
        free(v);
    }
    if (choice == DKG_SILENT) choice = DKG_NORMAL;   /* healthy said, both queens of clubs kept */
    return DKA_SAY | choice;
}

/* the next announcement for the seat's party, or not */
static int announce(const dkgame *g, int me, int level, uint64_t seed)
{
    const dkpos *p = &g->pos;
    if (!dko_announce_ok(p)) return 0;
    int k = dko_party(p, me), L = p->ann[k] + 1, game = known_game(p, me);
    if (L >= DKL_90 && level <= AI_EASY) return 0;
    if (L >= DKL_60 && level == AI_MEDIUM) return 0;
    static const float need[6] = {0, 0.66f, 0.80f, 0.85f, 0.90f, 0.95f};
    if (L == DKL_PARTY) {
        int soloist = dko_is_solo(game) || game == DKG_SILENT ? p->soloist == me : 0;
        dkhand s = strength(p->t.hand[me], p->t.n[me], game);
        if (!soloist && !((s.T >= 6 && s.H >= 2) || (s.T >= 7 && s.H >= 1) || (s.T >= 5 && s.H >= 2 && s.A >= 2)))
            return 0;
    }
    tb_state *v = malloc(sizeof *v);
    if (!v) return 0;
    dkb_view(g, me, v);
    uint8_t ann[2] = {p->ann[0], p->ann[1]};
    ann[k] = (uint8_t)L;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->deal * 64 + p->t.tricks * 4 + (uint64_t)L));
    float rw0, rw1, win1;
    playouts(v, level <= AI_EASY ? 32 : level == AI_MEDIUM ? 64 : 128, &r, ann, &rw0, NULL, &rw1, &win1);
    free(v);
    float bar = need[L] + (level <= AI_EASY ? 0.09f : 0) + (k ? 0.05f : 0);   /* Kontra: a little surer */
    return win1 >= bar && rw1 > rw0 + 0.005f ? DKA_ANNOUNCE | L : 0;
}

/* the action for seat at a level (0: nothing to do) */
int dkb_choose(const dkgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const dkpos *p = &g->pos;
    if (p->phase == DKP_DEAL) return DKA_NEXT;
    if (p->phase == DKP_OVER || p->turn != seat) return 0;
    int a = 0;
    if (p->phase == DKP_RESERVE) a = reserve(p, seat, level, seed);
    else if (!(a = announce(g, seat, level, seed))) {
        tb_state *v = malloc(sizeof *v);
        if (v) {
            dkb_view(g, seat, v);
            int c = tb_choose(v, level, seed, DK_HARD_ITERS, stats);
            if (c != TB_NONE && dko_can_play(p, c)) a = DKA_PLAY | c;
        }
        free(v);
    }
    return a && dko_legal(p, a) ? a : dko_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    dkgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *doppelkopf_live();                    /* the copy, at entry: nothing else is read */
    int a = dkb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    static const char SAY[DKG_TYPES] = {'g', 'w', 'g', 'q', 'j', 'f', 's', 's', 's', 's'};
    static const char SUITS[4] = {'s', 'h', 'd', 'c'};
    int x = DKA_ARG(a);
    char c[3];
    switch (DKA_KIND(a)) {
    case DKA_SAY:
        if (x >= DKG_SUIT) snprintf(move, (size_t)cap, "s%c", SUITS[x - DKG_SUIT]);
        else snprintf(move, (size_t)cap, "%c", SAY[x < DKG_TYPES ? x : 0]);
        return 1;
    case DKA_ANNOUNCE: snprintf(move, (size_t)cap, "a"); return 1;
    case DKA_PLAY: trick_name(x, c); snprintf(move, (size_t)cap, "%s", c); return 1;
    case DKA_NEXT: snprintf(move, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static int actor(void)
{
    const dkpos *p = &doppelkopf_live()->pos;
    return p->phase == DKP_RESERVE || p->phase == DKP_PLAY ? p->turn : -1;
}

static const bot doppelkopf_bot = {.think = think, .actor = actor, .play = doppelkopf_show};
REGISTER_BOT(doppelkopf, &doppelkopf_bot)
