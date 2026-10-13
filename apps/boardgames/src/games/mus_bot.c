/*
 * Mus' bot (bots.h): rule-based from the hand's strength, the strength measured by sampling: the other three hands
 * dealt at random from the cards the seat cannot see (the pack minus its own hand), kept only when they agree with
 * what the table has said (who speaks in pares and juego: who has them; punto: nobody has juego), and each lance
 * played out (mus_best). Runs on the worker thread: it copies the game at entry and reads only what its seat may know.
 * No SDL. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * - Mus or cut: cut with medias, duples, juego 31, or two good lances (the chance to win them, sampled); else mus
 *   (after three changes, cut).
 * - The cards laid away: of the fifteen ways to lay 1 to 4 cards away, the one whose expected hand (the new cards
 *   drawn at random) is the best: the reyes and the ases, pairs, a juego of 31.
 * - The envites (betting), from w: the chance the seat's side wins the lance. Open: an envido of 2 (more with a
 *   strong hand) from about 60 %, an ordago with a near sure lance; else pass. Facing a bet: quiero when the chance is
 *   worth the stake against the points a refusal gives away; raise with a strong hand; an ordago: quiero from about
 *   55 % (less when the other side is close to 40, more when the seat's side is). The others' bets tell: medium and
 *   hard take some chance off their w when the other side bets (more for a raise or an ordago).
 * - Honest bluffing: now and then (easy 4 %, medium 8 %, hard 10 % of the lances it opens) an envido with a weak hand;
 *   the bluff is a coin of the bot's own, never a look at another hand.
 * - Levels: easy plays the plain rules for mus and the cards laid away, misjudges its chances (noise) and weighs no
 *   stakes; medium and hard as above (hard samples more deals and reads a pass of the other side as weakness).
 * The next hand waits for a human tap (actor() -1); think() answers "N" there all the same.
 * Its moves are the game's notation ("m", "c", "d4o5c", "p", "b2", "o", "q", "n", "N"), played by mus_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/cardbot.h"
#include "games/mus.h"
#include "games/spanish.h"

typedef struct { int samples, noise, bluff, discount; } mus_level;

static mus_level levels(int level)
{
    static const mus_level L[AI_LEVELS] = {
        [AI_EASY] = {30, 25, 4, 0}, [AI_MEDIUM] = {300, 5, 8, 12}, [AI_HARD] = {1500, 0, 10, 20},
    };
    return L[level < 0 ? 0 : level > AI_HARD ? AI_HARD : level];
}

static int side(int pl) { return pl & 1; }

/* the cards the seat cannot see: the pack minus its hand */
static int unseen(const mpos *p, int me, uint8_t *pool)
{
    uint8_t pack[48];
    int n = cards_pack(PACK_ES40, pack);
    return cdb_unseen(pack, n, p->hand[me], 4, pool);
}

/* the chance that the seat's side wins `lance` (the others' hands sampled, agreeing with who speaks in it when the
 * lance is being bet: el, the players who have it; punto: nobody has juego) */
static float equity(const mpos *p, int me, int lance, int betting, int samples, ai_rng *r)
{
    uint8_t pool[40];
    int np = unseen(p, me, pool), wins = 0, done = 0;
    mpos *t = malloc(sizeof *t);
    if (!t) return 0.5f;
    *t = *p;
    for (int s = 0; s < samples && !ai_cancelled(); s++) {
        for (int tries = 0; tries < 40; tries++) {
            for (int i = 0; i < 12; i++) {      /* twelve cards drawn for the other three */
                int j = i + ai_rand_n(r, np - i);
                uint8_t x = pool[i];
                pool[i] = pool[j], pool[j] = x;
            }
            for (int k = 0, q = 0; q < MUS_P; q++) {
                if (q == me) continue;
                memcpy(t->hand[q], pool + 4 * k++, 4);
                mus_sort(t->hand[q]);
            }
            int ok = 1;
            if (betting && (lance == ML_PARES || lance == ML_JUEGO))
                for (int q = 0; q < MUS_P && ok; q++)
                    if (q != me) ok = mus_has(t, lance, q) == (p->eligible >> q & 1);
            if (ok) break;
        }
        unsigned el = 0;
        for (int q = 0; q < MUS_P; q++)
            if (mus_has(t, lance, q)) el |= 1u << q;
        if (!el) continue;
        done++;
        wins += side(mus_best(t, lance, el)) == side(me);
    }
    free(t);
    return done ? (float)wins / (float)done : 0.5f;
}

/* ---- the cards laid away ---------------------------------------------------------------------------------------- */
/* a hand's worth for the four lances (higher better) */
static float worth(const uint8_t *h4)
{
    uint8_t h[4];
    memcpy(h, h4, 4);
    mus_sort(h);
    float g = 0, c = 0;
    static const float w[4] = {8, 4, 2, 1};
    for (int i = 0; i < 4; i++) g += w[i] * (float)mus_value(h[i]), c += w[i] * (float)(13 - mus_value(h[3 - i]));
    g /= 180, c /= 180;
    int key, t = mus_pares(h, &key), pts = mus_points(h);
    float pa = t == MUS_NO_PARES ? 0 : t == MUS_PAR ? 0.35f + key / 40.0f
             : t == MUS_MEDIAS   ? 0.75f + key / 60.0f : 1.1f;
    float ju = pts == 31 ? 1.2f : pts == 32 ? 0.8f : pts >= 33 ? 0.55f : (float)(pts - 20) / 40.0f;
    return g * 1.2f + c * 0.8f + pa * 1.6f + ju * 1.5f;
}

static int discard_choice(const mpos *p, int me, int samples, ai_rng *r)
{
    uint8_t pool[40];
    int np = unseen(p, me, pool), best = 8;
    float bv = -1e9f;
    for (int m = 1; m <= 15; m++) {
        int k = __builtin_popcount((unsigned)m);
        float sum = 0;
        int n = samples / 8 + 4;
        for (int s = 0; s < n; s++) {
            uint8_t h[4];
            int j = 0;
            for (int i = 0; i < 4; i++)
                if (!(m >> i & 1)) h[j++] = p->hand[me][i];
            for (int i = 0; i < k; i++) {
                int x = i + ai_rand_n(r, np - i);
                uint8_t y = pool[i];
                pool[i] = pool[x], pool[x] = y;
                h[j++] = pool[i];
            }
            sum += worth(h);
        }
        float v = sum / (float)n;
        if (v > bv) bv = v, best = m;
    }
    return best;
}

/* ---- the envites ------------------------------------------------------------------------------------------------ */
static int bet_choice(const mpos *p, int me, int level, ai_rng *r)
{
    mus_level L = levels(level);
    int l = p->lance, facing = p->bet > 0 || p->ordago, us = side(me);
    float w = equity(p, me, l, 1, L.samples, r);
    if (L.noise) w += (float)(ai_rand_n(r, 2 * L.noise + 1) - L.noise) / 100.0f;
    int ours = p->score[us], theirs = p->score[1 - us];
    if (level <= AI_EASY) {                     /* easy: by its rough chance alone, no stakes weighed */
        if (!facing) return w >= 0.65f || (w < 0.4f && ai_rand_n(r, 100) < L.bluff) ? MA_BET | 2 : MA_PASS;
        return w >= (p->ordago ? 0.5f : 0.45f) ? MA_QUIERO : MA_NOQUIERO;
    }
    if (level >= AI_HARD)                       /* an opponent who passed in this lance: weaker */
        for (int q = 0; q < MUS_P; q++)
            if (side(q) != us && p->word[q] == MW_PASS) w += (1 - w) * 0.12f;
    if (!facing) {
        if (w >= 0.94f && level >= AI_MEDIUM && ai_rand_n(r, 100) < 30) return MA_ORDAGO;
        if (w >= 0.6f) {
            int a = w >= 0.85f ? 5 : w >= 0.75f ? 3 : 2;
            if (a > MUS_MAX_BET) a = MUS_MAX_BET;
            return MA_BET | a;
        }
        if (w < 0.4f && ai_rand_n(r, 100) < L.bluff) return MA_BET | 2;   /* a bluff */
        return MA_PASS;
    }
    float d = L.discount / 100.0f;             /* their bet tells of a strong hand */
    if (p->ordago || p->prev) d *= 1.5f;
    w *= 1 - d;
    if (p->ordago) {
        float need = 0.55f;
        if (theirs >= 30) need -= 0.1f;
        if (ours >= 30) need += 0.1f;
        return w >= need ? MA_QUIERO : MA_NOQUIERO;
    }
    int B = p->bet, deje = p->prev ? p->prev : 1;
    if (w >= 0.8f && B + 2 <= MUS_MAX_BET && level >= AI_MEDIUM) return MA_BET | (w >= 0.9f ? 5 : 2);
    if (w >= 0.93f && level >= AI_MEDIUM) return MA_ORDAGO;
    float need = (float)(B - deje) / (float)(2 * B);
    return w >= need ? MA_QUIERO : MA_NOQUIERO;
}

/* ---- mus or cut ------------------------------------------------------------------------------------------------- */
static int mus_choice(const mpos *p, int me, int level, ai_rng *r)
{
    mus_level L = levels(level);
    if (level <= AI_EASY) return mus_auto(p);   /* easy: the plain rule (mus without pares nor juego) */
    const uint8_t *h = p->hand[me];
    int key, t = mus_pares(h, &key), pts = mus_points(h);
    if (t >= MUS_MEDIAS || pts == 31 || p->rounds >= 3) return MA_CUT;
    int good = 0;
    for (int l = 0; l < ML_N; l++) {
        if ((l == ML_PARES && t == MUS_NO_PARES) || (l == ML_JUEGO && pts < 31)) continue;
        good += equity(p, me, l, 0, L.samples / 4 + 10, r) >= 0.62f;
    }
    if (L.noise && ai_rand_n(r, 100) < L.noise) return ai_rand_n(r, 2) ? MA_CUT : MA_MUS;
    return good >= 2 ? MA_CUT : MA_MUS;
}

int musb_choose(const mgame *g, int seat, int level, uint64_t seed)
{
    const mpos *p = &g->pos;
    if (p->phase == MP_HAND) return MA_NEXT;
    if (p->phase == MP_OVER || p->turn != seat) return 0;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 131 + (uint64_t)seat));
    int a;
    switch (p->phase) {
    case MP_MUS: a = mus_choice(p, seat, level, &r); break;
    case MP_DISCARD:                            /* easy: the plain rule (the low single cards away) */
        a = level <= AI_EASY ? mus_auto(p) : MA_DISCARD | discard_choice(p, seat, levels(level).samples, &r);
        break;
    case MP_BET: a = bet_choice(p, seat, level, &r); break;
    default: a = 0;
    }
    return a && mus_legal(p, a) ? a : mus_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    mgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *mus_live();                           /* the copy, at entry: nothing else is read */
    const mpos *p = &g->pos;
    int a = musb_choose(g, seat, level, ai_mix(ai_session_seed(), (uint64_t)g->n)), ok = 1;
    if (!a || ai_cancelled() || cap < 12) ok = 0;
    else
        switch (MA_KIND(a)) {
        case MA_MUS: snprintf(move, (size_t)cap, "m"); break;
        case MA_CUT: snprintf(move, (size_t)cap, "c"); break;
        case MA_DISCARD: {
            int k = 0;
            move[k++] = 'd';
            for (int i = 0; i < 4; i++)
                if (MA_ARG(a) >> i & 1) es_name(p->hand[seat][i], move + k), k += 2;
            move[k] = 0;
            break;
        }
        case MA_PASS: snprintf(move, (size_t)cap, "p"); break;
        case MA_BET: snprintf(move, (size_t)cap, "b%d", MA_ARG(a)); break;
        case MA_ORDAGO: snprintf(move, (size_t)cap, "o"); break;
        case MA_QUIERO: snprintf(move, (size_t)cap, "q"); break;
        case MA_NOQUIERO: snprintf(move, (size_t)cap, "n"); break;
        case MA_NEXT: snprintf(move, (size_t)cap, "N"); break;
        default: ok = 0;
        }
    free(g);
    return ok;
}

static int actor(void)
{
    const mpos *p = &mus_live()->pos;
    return p->phase < MP_HAND ? p->turn : -1;
}

static const bot mus_bot = {.think = think, .actor = actor, .play = mus_show};
REGISTER_BOT(mus, &mus_bot)
