/*
 * Dominoes: the rules (dominoes.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "cards.h"
#include "games/dominoes.h"

static uint8_t LO[DOM_BONES], HI[DOM_BONES];

static void bones(void)
{
    if (HI[DOM_BONES - 1]) return;
    for (int a = 0, i = 0; a <= 6; a++)
        for (int b = a; b <= 6; b++, i++) LO[i] = (uint8_t)a, HI[i] = (uint8_t)b;
}

int dom_lo(int bone) { bones(); return LO[bone % DOM_BONES]; }
int dom_hi(int bone) { bones(); return HI[bone % DOM_BONES]; }

int dom_bone(int a, int b)
{
    if (a > b) { int t = a; a = b; b = t; }
    if (a < 0 || b > 6) return -1;
    return a * 7 - a * (a - 1) / 2 + (b - a);
}

int dom_hand_pips(const dpos *p, int pl)
{
    int s = 0;
    for (int i = 0; i < p->nhand[pl]; i++) s += dom_pips(p->hand[pl][i]);
    return s;
}

/* ---- the line ---------------------------------------------------------------------------------------------------- */
int dom_open(const dpos *p, int end)
{
    if (p->center == DOM_NONE) return end == END_C;
    if (end == END_W || end == END_E) return 1;
    if (end == END_N || end == END_S) return p->spinner && p->narm[END_W] && p->narm[END_E];
    return 0;
}

int dom_end(const dpos *p, int end)
{
    if (p->center == DOM_NONE || end < 0 || end >= ENDS) return -1;
    if (p->narm[end]) return p->out[end][p->narm[end] - 1];
    return end == END_E ? p->ce : p->cw;        /* N, S: the spinner's (a double) */
}

int dom_fits(const dpos *p, int bone, int end)
{
    if (p->center == DOM_NONE) return end == END_C && (p->lead == DOM_NONE || bone == p->lead);
    if (end < 0 || end >= ENDS || !dom_open(p, end)) return 0;
    int v = dom_end(p, end);
    return dom_lo(bone) == v || dom_hi(bone) == v;
}

static int in_hand(const dpos *p, int pl, int bone)
{
    for (int i = 0; i < p->nhand[pl]; i++)
        if (p->hand[pl][i] == bone) return i;
    return -1;
}

int dom_can_play(const dpos *p)
{
    if (p->phase != DP_PLAY) return 0;
    for (int i = 0; i < p->nhand[p->turn]; i++)
        for (int e = 0; e < ENDS; e++)
            if (dom_fits(p, p->hand[p->turn][i], e)) return 1;
    if (p->center == DOM_NONE)
        for (int i = 0; i < p->nhand[p->turn]; i++)
            if (dom_fits(p, p->hand[p->turn][i], END_C)) return 1;
    return 0;
}

/* the open ends added up: a double at an end counts both halves; the spinner's short sides only once played on */
int dom_count(const dpos *p)
{
    if (p->center == DOM_NONE) return 0;
    if (!p->narm[END_W] && !p->narm[END_E]) return p->cw + p->ce;
    int s = 0, cdouble = p->cw == p->ce;
    for (int e = 0; e < ENDS; e++) {
        int n = p->narm[e];
        if (n) {
            int b = p->arm[e][n - 1];
            s += dom_double(b) ? 2 * dom_lo(b) : p->out[e][n - 1];
        } else if (e == END_W || e == END_E) {
            s += cdouble ? 2 * p->cw : e == END_W ? p->cw : p->ce;
        }
    }
    return s;
}

/* All Fives: the first double, laid on an end, becomes the spinner: the line is drawn again from it, the bones
 * already laid making its arm back (west when it was laid on the east end) */
static void reroot(dpos *p, int e, int bone)
{
    uint8_t arm[DOM_BONES], out[DOM_BONES];
    int n = 0, back = e == END_E ? END_W : END_E;
    int side = e == END_E ? p->ce : p->cw, far = e == END_E ? p->cw : p->ce;
    for (int i = p->narm[e] - 1; i >= 0; i--) {  /* the arm it was laid on, from it back to the middle: turned round */
        arm[n] = p->arm[e][i];
        out[n++] = (uint8_t)(i ? p->out[e][i - 1] : side);
    }
    arm[n] = p->center;                         /* the old middle bone, its far side out */
    out[n++] = (uint8_t)far;
    for (int i = 0; i < p->narm[back]; i++) {   /* then the other arm, as it was */
        arm[n] = p->arm[back][i];
        out[n++] = p->out[back][i];
    }
    memset(p->narm, 0, sizeof p->narm);
    memcpy(p->arm[back], arm, (size_t)n);
    memcpy(p->out[back], out, (size_t)n);
    p->narm[back] = (uint8_t)n;
    p->center = (uint8_t)bone;
    p->cw = p->ce = (uint8_t)dom_lo(bone);
    p->spinner = 1;
}

/* ---- the rounds -------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static void deal(dpos *p)
{
    uint8_t all[DOM_BONES];
    for (int i = 0; i < DOM_BONES; i++) all[i] = (uint8_t)i;
    cards_shuffle(all, DOM_BONES, mix(p->seed, 1000u + p->round));
    int each = p->players == 2 ? 7 : 5, k = 0;
    for (int pl = 0; pl < p->players; pl++) {
        p->nhand[pl] = (uint8_t)each;
        for (int i = 0; i < each; i++) p->hand[pl][i] = all[k++];
    }
    p->nyard = 0;
    while (k < DOM_BONES) p->yard[p->nyard++] = all[k++];
    p->center = DOM_NONE;
    p->cw = p->ce = 0;
    p->spinner = 0;
    memset(p->narm, 0, sizeof p->narm);
    p->passes = 0;
    p->winner = DOM_NONE;
    p->scored = 0;
    memset(p->made, 0, sizeof p->made);
    p->lead = DOM_NONE;
    if (p->round == 0) {                        /* the highest double leads (none: the heaviest bone) */
        int best = -1, who = 0;
        for (int pl = 0; pl < p->players; pl++)
            for (int i = 0; i < p->nhand[pl]; i++) {
                int b = p->hand[pl][i], key = (dom_double(b) ? 100 : 0) + dom_pips(b) * 8 + dom_hi(b);
                if (key > best) best = key, who = pl, p->lead = (uint8_t)b;
            }
        p->leader = (uint8_t)who;
    }
    p->turn = p->leader;
    p->phase = DP_PLAY;
}

void dom_new(dgame *g, int variant, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    dpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = p->variant = (uint8_t)(variant >= DOM_BLOCK && variant < DOM_VARIANTS ? variant : DOM_BLOCK);
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > DOM_MAXP ? DOM_MAXP : players);
    deal(p);
}

static int someone_reached(const dpos *p)
{
    for (int pl = 0; pl < p->players; pl++)
        if (p->total[pl] >= DOM_TARGET) return 1;
    return 0;
}

/* the round is over: `winner` laid their last bone, or DOM_NONE (blocked: the lowest pip count, if alone) */
static void round_end(dpos *p, int winner)
{
    int pips[DOM_MAXP], low = 1000, nlow = 0;
    for (int pl = 0; pl < p->players; pl++) {
        pips[pl] = dom_hand_pips(p, pl);
        if (pips[pl] < low) low = pips[pl], nlow = 0;
        if (pips[pl] == low) nlow++;
    }
    if (winner == DOM_NONE && nlow == 1)
        for (int pl = 0; pl < p->players; pl++)
            if (pips[pl] == low) winner = pl;
    memset(p->last, 0, sizeof p->last);
    p->winner = (uint8_t)winner;
    if (winner != DOM_NONE) {
        int pts = 0;
        for (int pl = 0; pl < p->players; pl++)
            if (pl != winner) pts += pips[pl];
        if (p->variant == DOM_FIVES) pts = (pts + 2) / 5 * 5;
        p->last[winner] = (int16_t)pts;
        p->total[winner] = (int16_t)(p->total[winner] + pts);
        p->leader = (uint8_t)winner;
    } else {
        p->leader = (uint8_t)((p->leader + 1) % p->players);
    }
    p->round++;
    p->phase = someone_reached(p) ? DP_OVER : DP_ROUND;
}

int dom_legal(const dpos *p, int act)
{
    int k = DA_KIND(act);
    if (k == DA_NEXT) return p->phase == DP_ROUND;
    if (p->phase != DP_PLAY) return 0;
    switch (k) {
    case DA_PLAY: {
        int b = DA_BONE(act);
        return b < DOM_BONES && in_hand(p, p->turn, b) >= 0 && dom_fits(p, b, DA_END(act));
    }
    case DA_DRAW: return p->variant != DOM_BLOCK && p->nyard > 0 && !dom_can_play(p);
    case DA_PASS: return (p->variant == DOM_BLOCK || p->nyard == 0) && !dom_can_play(p);
    default: return 0;
    }
}

static void apply(dpos *p, int act)
{
    int k = DA_KIND(act), pl = p->turn;
    p->scored = 0;
    switch (k) {
    case DA_PLAY: {
        int b = DA_BONE(act), e = DA_END(act), i = in_hand(p, pl, b);
        memmove(&p->hand[pl][i], &p->hand[pl][i + 1], (size_t)(p->nhand[pl] - i - 1));
        p->nhand[pl]--;
        p->lead = DOM_NONE;
        p->passes = 0;
        if (p->center == DOM_NONE) {
            p->center = (uint8_t)b;
            p->cw = (uint8_t)dom_lo(b);
            p->ce = (uint8_t)dom_hi(b);
            p->spinner = p->variant == DOM_FIVES && dom_double(b);
        } else {
            int v = dom_end(p, e), o = dom_lo(b) == v ? dom_hi(b) : dom_lo(b);
            if (p->variant == DOM_FIVES && !p->spinner && dom_double(b)) reroot(p, e, b);
            else {
                p->arm[e][p->narm[e]] = (uint8_t)b;
                p->out[e][p->narm[e]++] = (uint8_t)o;
            }
        }
        if (p->variant == DOM_FIVES) {
            int c = dom_count(p);
            if (c > 0 && c % 5 == 0) {
                p->scored = (int16_t)c;
                p->made[pl] = (int16_t)(p->made[pl] + c);
                p->total[pl] = (int16_t)(p->total[pl] + c);
            }
        }
        if (!p->nhand[pl]) round_end(p, pl);
        else if (p->variant == DOM_FIVES && p->total[pl] >= DOM_TARGET) {   /* All Fives: the game ends at once */
            memset(p->last, 0, sizeof p->last);
            p->winner = (uint8_t)pl;
            p->round++;
            p->phase = DP_OVER;
        } else p->turn = (uint8_t)((pl + 1) % p->players);
        break;
    }
    case DA_DRAW:
        p->hand[pl][p->nhand[pl]++] = p->yard[--p->nyard];
        break;
    case DA_PASS:
        if (++p->passes >= p->players) round_end(p, DOM_NONE);
        else p->turn = (uint8_t)((pl + 1) % p->players);
        break;
    case DA_NEXT:
        deal(p);
        break;
    }
}

int dom_play(dgame *g, int act)
{
    if (g->n >= DOM_MAX_ACTS || !dom_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int dom_undo(dgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint16_t acts[DOM_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    dom_new(g, g->variant, g->players, g->seed);
    for (int i = 0; i < n; i++) dom_play(g, acts[i]);
    return 1;
}

int dom_auto(const dpos *p)
{
    if (p->phase == DP_ROUND) return DA_NEXT;
    if (p->phase != DP_PLAY) return 0;
    for (int i = 0; i < p->nhand[p->turn]; i++) {
        int b = p->hand[p->turn][i];
        if (p->center == DOM_NONE) {
            if (dom_fits(p, b, END_C)) return DA(DA_PLAY, b, END_C);
            continue;
        }
        for (int e = 0; e < ENDS; e++)
            if (dom_fits(p, b, e)) return DA(DA_PLAY, b, e);
    }
    return dom_legal(p, DA_DRAW) ? DA_DRAW : DA_PASS;
}

unsigned dom_winners(const dpos *p)
{
    if (p->phase != DP_OVER) return 0;
    int top = -1;
    unsigned m = 0;
    for (int pl = 0; pl < p->players; pl++) top = p->total[pl] > top ? p->total[pl] : top;
    for (int pl = 0; pl < p->players; pl++) m |= (unsigned)(p->total[pl] == top) << pl;
    return m;
}

int dom_script(dgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int act;
        switch (c) {
        case 'p': {
            if (s[0] < '0' || s[0] > '6' || s[1] < '0' || s[1] > '6') return 0;
            int b = dom_bone(s[0] - '0', s[1] - '0'), e = END_C;
            s += 2;
            if (*s == 'w') e = END_W;
            else if (*s == 'e') e = END_E;
            else if (*s == 'n') e = END_N;
            else if (*s == 's') e = END_S;
            if (e != END_C) s++;
            act = DA(DA_PLAY, b, e);
            break;
        }
        case 'd': act = DA_DRAW; break;
        case 'x': act = DA_PASS; break;
        case 'n': act = DA_NEXT; break;
        case 'a': act = dom_auto(&g->pos); break;
        case '#':
        case '*':                               /* to the end of the round, or of the game */
            while (g->pos.phase == DP_PLAY || (c == '*' && g->pos.phase == DP_ROUND))
                if (!dom_play(g, dom_auto(&g->pos))) return 0;
            continue;
        default: return 0;
        }
        if (!dom_play(g, act)) return 0;
    }
    return 1;
}
