/*
 * Mahjong's bot (bots.h), for the four-player table game (Hong Kong old style, both lengths): a hand builder aimed at
 * the game's own scoring, from what its seat may see: its own concealed tiles, every meld and flower on the table,
 * the discards lying in the ponds. The unseen tiles (the other hands and the wall) are only counted, kind by kind.
 * Runs on the worker thread: it copies the game at entry and reads only that copy. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   The worth of a hand: for each aim it may take (any hand; a half or full flush of one suit; all pungs; seven
 *   pairs), how many tiles it is from complete (the usual count of sets, partial sets and a pair) and how many
 *   unseen tiles would bring it closer, weighed by what the finished hand would score: the game asks for 3 faan at
 *   least (dragon and wind pungs, a concealed hand, no flowers, the flushes, all pungs...), so an aim short of them
 *   is worth little. The best aim gives the hand its worth.
 *   The discard: the tile whose loss leaves the best hand; hard also weighs the danger, a little, once another
 *   player looks close (three melds, or the wall nearly out): a tile already discarded by them, or of which every
 *   other copy is in sight, is safer. (Weighed more, it cost more hands than it saved in self-play.)
 *   Claims: a win always (on a discard, a robbed kong, self-drawn); a pung or kong of a dragon or of the bot's
 *   winds; any other pung, kong or chow when the hand after it (and its discard) is worth clearly more than
 *   without it; its own kongs when they cost the hand nothing.
 * Levels: easy plays the rules' plain actions (the fewest tiles away, the winds and dragons punged, a win taken),
 * with a careless discard now and then; medium plays the aims above on any hand and the best flush, without the
 * danger; hard weighs every aim and the danger.
 * Its moves are the game's notation ("x5b", "xR", "w", "u", "k", "c3o", "K7c", "p", "n"), played by mahjong4_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/mahjong4.h"

enum { AIM_ANY, AIM_HALF, AIM_FULL = AIM_HALF + 3, AIM_PUNGS = AIM_FULL + 3, AIM_PAIRS, AIMS };

typedef struct {
    int me, nmeld, nflower, seat, wind, concealed;   /* concealed: no exposed meld */
    uint8_t meld_type[4], meld_kind[4];
    int unseen[M4_KINDS];                       /* copies of each kind out of sight */
    int value_pungs;                            /* dragon / seat / round wind pungs already melded */
    int danger[M4_KINDS];                       /* hard: how risky each kind is to throw (0 safe) */
} mview;

static int chow_start(int k) { return k < M4_WINDS && k % 9 <= 6; }

/* ---- tiles away from a complete hand ----------------------------------------------------------------------------- */
static void away_rec(int *c, int k, int need, int sets, int part, int head, int pungs_only, int *best)
{
    while (k < M4_KINDS && !c[k]) k++;
    if (k == M4_KINDS) {
        int t = part < need - sets ? part : need - sets, s = 2 * (need - sets) - t - head;
        if (s < *best) *best = s;
        return;
    }
    if (c[k] >= 3) {
        c[k] -= 3;
        away_rec(c, k, need, sets + 1, part, head, pungs_only, best);
        c[k] += 3;
    }
    if (!pungs_only && chow_start(k) && c[k + 1] && c[k + 2]) {
        c[k]--, c[k + 1]--, c[k + 2]--;
        away_rec(c, k, need, sets + 1, part, head, pungs_only, best);
        c[k]++, c[k + 1]++, c[k + 2]++;
    }
    if (c[k] >= 2) {
        c[k] -= 2;
        if (!head) away_rec(c, k, need, sets, part, 1, pungs_only, best);
        away_rec(c, k, need, sets, part + 1, head, pungs_only, best);
        c[k] += 2;
    }
    if (!pungs_only)
        for (int g = 1; g <= 2; g++)
            if (k < M4_WINDS && k % 9 + g <= 8 && c[k + g]) {
                c[k]--, c[k + g]--;
                away_rec(c, k, need, sets, part + 1, head, pungs_only, best);
                c[k]++, c[k + g]++;
            }
    c[k]--;
    away_rec(c, k + 1, need, sets, part, head, pungs_only, best);
    c[k]++;
}

/* the kinds an aim keeps */
static int in_aim(int aim, int k)
{
    if (aim >= AIM_HALF && aim < AIM_FULL) return k >= M4_WINDS || k / 9 == aim - AIM_HALF;
    if (aim >= AIM_FULL && aim < AIM_PUNGS) return k < M4_WINDS && k / 9 == aim - AIM_FULL;
    return 1;
}

/* the melds allow the aim */
static int aim_open(const mview *v, int aim)
{
    if (aim == AIM_PAIRS) return v->nmeld == 0;
    for (int i = 0; i < v->nmeld; i++) {
        int t = v->meld_type[i], k = v->meld_kind[i];
        if (aim == AIM_PUNGS && t == MS_CHOW) return 0;
        if (!in_aim(aim, k)) return 0;
    }
    return 1;
}

/* the hand's distance to complete for an aim (counts c of the concealed tiles; off-aim tiles left out) */
static int distance(const mview *v, const int *c0, int aim)
{
    int c[M4_KINDS];
    for (int k = 0; k < M4_KINDS; k++) c[k] = in_aim(aim, k) ? c0[k] : 0;
    if (aim == AIM_PAIRS) {
        int pairs = 0, kinds = 0;
        for (int k = 0; k < M4_KINDS; k++) pairs += c[k] >= 2, kinds += c[k] > 0;
        return 6 - pairs + (kinds < 7 ? 7 - kinds : 0);
    }
    int best = 8;
    away_rec(c, 0, 4 - v->nmeld, 0, 0, 0, aim == AIM_PUNGS, &best);
    return best;
}

static int is_value(const mview *v, int k)
{
    return k >= M4_DRAGONS || k == M4_WINDS + v->seat || k == M4_WINDS + v->wind;
}

/* a tile of kind k can change the hand's distance: one of its kind or a near neighbour in hand */
static int near(const int *c, int k)
{
    if (c[k]) return 1;
    if (k >= M4_WINDS) return 0;
    for (int d = -2; d <= 2; d++)
        if (d && k % 9 + d >= 0 && k % 9 + d <= 8 && c[k + d]) return 1;
    return 0;
}

/* an aim the hand is too far from to weigh */
static int hopeless(const int *c, int aim)
{
    int in = 0, pairs = 0;
    for (int k = 0; k < M4_KINDS; k++) in += in_aim(aim, k) ? c[k] : 0, pairs += c[k] >= 2;
    if (aim >= AIM_HALF && aim < AIM_FULL) return in < 8;
    if (aim >= AIM_FULL && aim < AIM_PUNGS) return in < 9;
    if (aim == AIM_PUNGS || aim == AIM_PAIRS) return pairs < 3;
    return 0;
}

/* the faan the aim can expect (a rough count) */
static double faan_of(const mview *v, const int *c, int aim)
{
    double f = v->value_pungs;
    for (int k = M4_WINDS; k < M4_KINDS; k++) {
        if (!is_value(v, k)) continue;
        int w = (k == M4_WINDS + v->seat) + (k == M4_WINDS + v->wind) + (k >= M4_DRAGONS);
        if (c[k] >= 3) f += w;
        else if (c[k] == 2 && v->unseen[k]) f += 0.45 * w;
    }
    if (v->concealed) f += 1.4;                 /* concealed, and a fair chance of drawing the last tile */
    else f += 0.3;
    if (!v->nflower) f += 1;
    if (aim >= AIM_HALF && aim < AIM_FULL) f += 3;
    if (aim >= AIM_FULL && aim < AIM_PUNGS) f += 7;
    if (aim == AIM_PUNGS) f += 3;
    if (aim == AIM_PAIRS) f += 4 - 1.4 + 1.0;   /* (seven pairs: always concealed) */
    return f;
}

/* the worth of a hand (counts of the concealed tiles; one tile to draw yet): the best aim */
static double hand_worth(const mview *v, const int *c, int level, int *aim_out)
{
    double best = -1e9;
    int bestaim = AIM_ANY;
    for (int aim = 0; aim < AIMS; aim++) {
        if (level == AI_MEDIUM && aim != AIM_ANY && (aim < AIM_HALF || aim >= AIM_FULL)) continue;
        if (!aim_open(v, aim)) continue;
        if (hopeless(c, aim)) continue;
        int d = distance(v, c, aim);
        /* the unseen tiles that bring it closer */
        int ukeire = 0, cc[M4_KINDS];
        memcpy(cc, c, sizeof cc);
        for (int k = 0; k < M4_KINDS; k++) {
            if (!v->unseen[k] || !in_aim(aim, k) || !near(c, k)) continue;
            cc[k]++;
            if (distance(v, cc, aim) < d) ukeire += v->unseen[k];
            cc[k]--;
        }
        double f = faan_of(v, c, aim);
        double w = -10.0 * d + 0.12 * ukeire;
        w += f >= M4_MIN ? 2.0 + 0.5 * (f - M4_MIN > 4 ? 4 : f - M4_MIN) : -6.0 * (M4_MIN - f);
        if (w > best) best = w, bestaim = aim;
    }
    if (aim_out) *aim_out = bestaim;
    return best;
}

/* ---- the view ---------------------------------------------------------------------------------------------------- */
static void view_of(const m4pos *p, int me, mview *v)
{
    memset(v, 0, sizeof *v);
    v->me = me;
    v->seat = m4_seat(p, me);
    v->wind = p->wind;
    v->nflower = p->nflower[me];
    v->nmeld = p->nmeld[me];
    v->concealed = 1;
    for (int k = 0; k < M4_KINDS; k++) v->unseen[k] = 4;
    for (int i = 0; i < p->nhand[me]; i++) {
        int k = m4_kind(p->hand[me][i]);
        if (k < M4_KINDS) v->unseen[k]--;
    }
    for (int q = 0; q < M4_P; q++) {
        for (int i = 0; i < p->nmeld[q]; i++) {
            const m4meld *m = &p->meld[q][i];
            for (int j = 0; j < m->n; j++) {
                int k = m4_kind(m->tile[j]);
                if (m->tile[j] != M4_NONE && k < M4_KINDS) v->unseen[k]--;
            }
        }
        for (int i = 0; i < p->npond[q]; i++) {
            int k = m4_kind(p->pond[q][i]);
            if (k < M4_KINDS) v->unseen[k]--;
        }
    }
    for (int k = 0; k < M4_KINDS; k++) v->unseen[k] = v->unseen[k] < 0 ? 0 : v->unseen[k];
    for (int i = 0; i < v->nmeld; i++) {
        const m4meld *m = &p->meld[me][i];
        v->meld_type[i] = m->type;
        v->meld_kind[i] = m->kind;
        if (m->from != M4_NONE) v->concealed = 0;
        if (m->type != MS_CHOW && is_value(v, m->kind))
            v->value_pungs += (m->kind == M4_WINDS + v->seat) + (m->kind == M4_WINDS + v->wind) +
                              (m->kind >= M4_DRAGONS);
    }
    /* the danger: the others' melds and the wall's end */
    int live = m4_live(p);
    for (int q = 0; q < M4_P; q++) {
        if (q == me) continue;
        int threat = (p->nmeld[q] >= 3 ? 2 : 0) + (live < 12 ? 1 : 0);
        if (!threat) continue;
        for (int k = 0; k < M4_KINDS; k++) {
            int safe = 0;
            for (int i = 0; i < p->npond[q] && !safe; i++) safe = m4_kind(p->pond[q][i]) == k;
            int d = safe ? 1 : !v->unseen[k] ? 0 : k >= M4_WINDS && v->unseen[k] <= 1 ? 1 : 3;
            v->danger[k] += threat * d;
        }
    }
}

static void counts(const m4pos *p, int q, int *c)
{
    memset(c, 0, sizeof(int) * M4_KINDS);
    for (int i = 0; i < p->nhand[q]; i++) {
        int k = m4_kind(p->hand[q][i]);
        if (k < M4_KINDS) c[k]++;
    }
}

/* the best discard from counts c (14 tiles): its kind, the worth left */
static int best_discard(const mview *v, int *c, int level, double *worth_out)
{
    double best = -1e18;
    int bk = -1;
    for (int k = 0; k < M4_KINDS; k++) {
        if (!c[k]) continue;
        c[k]--;
        double w = hand_worth(v, c, level, NULL);
        c[k]++;
        if (level == AI_HARD) w -= 0.15 * v->danger[k];
        if (w > best) best = w, bk = k;
    }
    if (worth_out) *worth_out = best;
    return bk;
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static int discard_act(const m4pos *p, int q, int kind)
{
    if (p->drawn != M4_NONE && m4_kind(p->drawn) == kind) return MA_DISCARD | p->drawn;
    for (int i = 0; i < p->nhand[q]; i++)
        if (m4_kind(p->hand[q][i]) == kind) return MA_DISCARD | p->hand[q][i];
    return 0;
}

/* the action for seat at a level (0: nothing to do) */
int m4b_choose(const m4game *g, int seat, int level, uint64_t seed)
{
    const m4pos *p = &g->pos;
    if (p->phase == MP_SCORE) return MA_NEXT;
    if (p->phase == MP_OVER || m4_actor(p) != seat) return 0;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 29 + (uint64_t)seat));
    if (m4_legal(p, MA_WIN)) return MA_WIN;
    if (level == AI_EASY) {
        int a = m4_auto(p);
        if (MA_KIND(a) == MA_DISCARD && ai_rand_n(&r, 100) < 15) {   /* a careless discard: any loose tile */
            int i = ai_rand_n(&r, p->nhand[seat]);
            return MA_DISCARD | p->hand[seat][i];
        }
        return a;
    }
    mview *v = malloc(sizeof *v);
    if (!v) return m4_auto(p);
    view_of(p, seat, v);
    int c[M4_KINDS], act = 0;
    counts(p, seat, c);
    if (p->phase == MP_DISCARD) {
        double keep;
        int k = best_discard(v, c, level, &keep);
        act = k >= 0 ? discard_act(p, seat, k) : 0;
        int ks[8], nk = m4_kongs(p, ks);
        for (int i = 0; i < nk; i++) {          /* a kong of its own: when the hand loses nothing by it */
            mview w = *v;
            int cc[M4_KINDS];
            memcpy(cc, c, sizeof cc);
            int concealed_kong = cc[ks[i]] == 4;
            cc[ks[i]] -= concealed_kong ? 4 : 1;
            if (concealed_kong) {
                w.meld_type[w.nmeld] = MS_KONG;
                w.meld_kind[w.nmeld++] = (uint8_t)ks[i];
                if (is_value(&w, ks[i])) w.value_pungs++;
            }
            if (hand_worth(&w, cc, level, NULL) >= keep - 0.5) { act = MA_KONG | ks[i]; break; }
        }
    } else {                                    /* a claim: on a discard, or a kong to rob */
        int ch[3], nch, o = m4_options(p, ch, &nch), k = m4_kind(p->tile);
        act = MA_PASS;
        double now = hand_worth(v, c, level, NULL), best = now + 1.5;
        if (o & (MO_PUNG | MO_KONG)) {
            mview w = *v;
            w.meld_type[w.nmeld] = MS_PUNG;
            w.meld_kind[w.nmeld++] = (uint8_t)k;
            w.concealed = 0;
            if (is_value(&w, k)) w.value_pungs += (k == M4_WINDS + w.seat) + (k == M4_WINDS + w.wind) +
                                                  (k >= M4_DRAGONS);
            int cc[M4_KINDS];
            memcpy(cc, c, sizeof cc);
            cc[k] -= 2;
            double after;
            best_discard(&w, cc, level, &after);
            if (is_value(v, k)) after += 2;     /* a faan in hand */
            if (after > best) best = after, act = MA_PUNG;
            if (o & MO_KONG) {
                cc[k]--;
                double kong = hand_worth(&w, cc, level, NULL) + (is_value(v, k) ? 2 : 0);
                if (kong >= best - 0.3) best = kong, act = MA_KONG | k;
            }
        }
        for (int i = 0; i < nch; i++) {
            mview w = *v;
            w.meld_type[w.nmeld] = MS_CHOW;
            w.meld_kind[w.nmeld++] = (uint8_t)ch[i];
            w.concealed = 0;
            int cc[M4_KINDS];
            memcpy(cc, c, sizeof cc);
            for (int j = ch[i]; j < ch[i] + 3; j++)
                if (j != k) cc[j]--;
            double after;
            best_discard(&w, cc, level, &after);
            if (after > best) best = after, act = MA_CHOW | ch[i];
        }
    }
    free(v);
    return act && m4_legal(p, act) ? act : m4_auto(p);
}

static int kind_text(int k, char *o)
{
    if (k < M4_WINDS) return o[0] = (char)('1' + k % 9), o[1] = "obc"[k / 9], o[2] = 0, 2;
    o[0] = "ESWNRGP"[k - M4_WINDS];
    o[1] = 0;
    return 1;
}

/* an action in the notation */
void m4b_name(const m4pos *p, int a, char *m, int cap)
{
    char t[4];
    switch (MA_KIND(a)) {
    case MA_DISCARD: kind_text(m4_kind(MA_ARG(a)), t), snprintf(m, (size_t)cap, "x%s", t); break;
    case MA_WIN: snprintf(m, (size_t)cap, "w"); break;
    case MA_PUNG: snprintf(m, (size_t)cap, "u"); break;
    case MA_KONG:
        if (p->phase == MP_DISCARD) kind_text(MA_ARG(a), t), snprintf(m, (size_t)cap, "K%s", t);
        else snprintf(m, (size_t)cap, "k");
        break;
    case MA_CHOW: kind_text(MA_ARG(a), t), snprintf(m, (size_t)cap, "c%s", t); break;
    case MA_PASS: snprintf(m, (size_t)cap, "p"); break;
    default: snprintf(m, (size_t)cap, "n"); break;
    }
}

static int think(int seat, int level, char *move, int cap)
{
    m4game *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *mahjong4_live();                      /* the copy, at entry: nothing else is read */
    int a = m4b_choose(g, seat, level, ai_session_seed());
    if (a && !ai_cancelled() && cap >= 8) m4b_name(&g->pos, a, move, cap);
    free(g);
    return a && !ai_cancelled() && cap >= 8;
}

static int actor(void)
{
    const m4pos *p = &mahjong4_live()->pos;
    return p->phase == MP_SCORE || p->phase == MP_OVER ? -1 : m4_actor(p);
}

static const bot mahjong4_bot = {.think = think, .actor = actor, .play = mahjong4_show};
REGISTER_BOT(mahjong4, &mahjong4_bot)
