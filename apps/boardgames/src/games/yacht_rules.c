/*
 * Yacht: the rules (yacht.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <ctype.h>
#include <string.h>

#include "games/yacht.h"

static const int CATS[YA_VARIANTS][YA_MAX_CATS + 1] = {
    {YC_ONES, YC_TWOS, YC_THREES, YC_FOURS, YC_FIVES, YC_SIXES, YC_THREE_KIND, YC_FOUR_KIND, YC_FULL, YC_SMALL,
     YC_LARGE, YC_YACHT, YC_CHANCE, -1},
    {YC_ONES, YC_TWOS, YC_THREES, YC_FOURS, YC_FIVES, YC_SIXES, YC_PAIR, YC_TWO_PAIRS, YC_THREE_KIND, YC_FOUR_KIND,
     YC_SMALL, YC_LARGE, YC_FULL, YC_CHANCE, YC_YACHT, -1},
    {YC_ONES, YC_TWOS, YC_THREES, YC_FOURS, YC_FIVES, YC_SIXES, YC_STRAIGHT, YC_FULL, YC_FOUR_KIND, YC_YACHT,
     YC_DOUBLE, -1},
};

int ya_cats(int variant, int *cats)
{
    int n = 0;
    if (variant < 0 || variant >= YA_VARIANTS) variant = YA_CLASSIC;
    while (CATS[variant][n] >= 0) {
        if (cats) cats[n] = CATS[variant][n];
        n++;
    }
    return n;
}

static int in_variant(int variant, int cat)
{
    for (int i = 0; CATS[variant][i] >= 0; i++)
        if (CATS[variant][i] == cat) return 1;
    return 0;
}

int ya_is_upper(int cat) { return cat >= YC_ONES && cat <= YC_SIXES; }
int ya_bonus_at(int variant) { return variant == YA_CLASSIC ? 35 : variant == YA_NORDIC ? 50 : 0; }

static void counts(const uint8_t d[YA_DICE], int c[7], int *sum)
{
    memset(c, 0, 7 * sizeof c[0]);
    *sum = 0;
    for (int i = 0; i < YA_DICE; i++) {
        c[d[i] <= 6 ? d[i] : 0]++;
        *sum += d[i];
    }
}

int ya_five(const uint8_t d[YA_DICE])
{
    for (int i = 1; i < YA_DICE; i++)
        if (d[i] != d[0]) return 0;
    return d[0] >= 1 && d[0] <= 6;
}

/* the longest run of faces in a row */
static int run(const int c[7])
{
    int best = 0, k = 0;
    for (int v = 1; v <= 6; v++) {
        k = c[v] ? k + 1 : 0;
        if (k > best) best = k;
    }
    return best;
}

static int full(const int c[7])                 /* three of one face and two of another */
{
    int three = 0, two = 0;
    for (int v = 1; v <= 6; v++) {
        three |= c[v] == 3;
        two |= c[v] == 2;
    }
    return three && two;
}

static int kind(const int c[7], int n)          /* the highest face found n times or more (0: none) */
{
    for (int v = 6; v >= 1; v--)
        if (c[v] >= n) return v;
    return 0;
}

int ya_base(int variant, int cat, const uint8_t d[YA_DICE])
{
    int c[7], sum;
    counts(d, c, &sum);
    if (c[0]) return 0;                         /* not thrown */
    if (ya_is_upper(cat)) return c[cat + 1] * (cat + 1);
    int five = ya_five(d);
    switch (cat) {
    case YC_PAIR: return 2 * kind(c, 2);
    case YC_TWO_PAIRS: {
        int a = 0, b = 0;
        for (int v = 6; v >= 1; v--)
            if (c[v] >= 2) {
                if (!a) a = v;
                else if (!b) b = v;
            }
        return a && b ? 2 * (a + b) : 0;
    }
    case YC_THREE_KIND:
        return variant == YA_NORDIC ? 3 * kind(c, 3) : kind(c, 3) ? sum : 0;
    case YC_FOUR_KIND:
        if (variant == YA_NORDIC) return 4 * kind(c, 4);
        if (variant == YA_GENERALA) return kind(c, 4) ? 40 : 0;
        return kind(c, 4) ? sum : 0;
    case YC_FULL:
        if (!full(c)) return 0;
        return variant == YA_NORDIC ? sum : variant == YA_GENERALA ? 30 : 25;
    case YC_SMALL:
        if (variant == YA_NORDIC) return c[1] && c[2] && c[3] && c[4] && c[5] ? 15 : 0;
        return run(c) >= 4 ? 30 : 0;
    case YC_LARGE:
        if (variant == YA_NORDIC) return c[2] && c[3] && c[4] && c[5] && c[6] ? 20 : 0;
        return run(c) == 5 ? 40 : 0;
    case YC_STRAIGHT:                           /* 1-2-3-4-5, 2-3-4-5-6, 3-4-5-6-1 */
        return run(c) == 5 || (c[1] && c[3] && c[4] && c[5] && c[6]) ? 20 : 0;
    case YC_CHANCE: return sum;
    case YC_YACHT: return five ? 50 : 0;
    case YC_DOUBLE: return five ? 100 : 0;
    }
    return 0;
}

void ya_new(ygame *g, int variant, int players)
{
    memset(g, 0, sizeof *g);
    ypos *p = &g->pos;
    p->variant = (uint8_t)(variant >= 0 && variant < YA_VARIANTS ? variant : YA_CLASSIC);
    p->players = (uint8_t)(players < 1 ? 1 : players > YA_PLAYERS ? YA_PLAYERS : players);
    memset(p->box, 0xff, sizeof p->box);        /* -1: empty */
    p->winner = -1;
}

/* classic's joker: a five of a kind once the five-of-a-kind box is filled. 0: no joker; else where it may go: 1 its
 * upper box only, 2 any empty lower box, 3 any empty upper box */
static int joker(const ypos *p)
{
    const int16_t *b = p->box[p->turn];
    if (p->variant != YA_CLASSIC || !ya_five(p->dice) || b[YC_YACHT] < 0) return 0;
    if (b[p->dice[0] - 1] < 0) return 1;
    static const int lower[] = {YC_THREE_KIND, YC_FOUR_KIND, YC_FULL, YC_SMALL, YC_LARGE, YC_CHANCE};
    for (int i = 0; i < 6; i++)
        if (b[lower[i]] < 0) return 2;
    return 3;
}

int ya_points(const ypos *p, int cat)
{
    if (p->over || !p->throws || cat < 0 || cat >= YC_N || !in_variant(p->variant, cat) || p->box[p->turn][cat] >= 0)
        return -1;
    int j = joker(p);
    if (j == 1 && cat != p->dice[0] - 1) return -1;
    if (j == 2 && ya_is_upper(cat)) return -1;
    if (j == 3 && !ya_is_upper(cat)) return -1;
    if (j == 2) {                               /* the joker: these score in full */
        if (cat == YC_FULL) return 25;
        if (cat == YC_SMALL) return 30;
        if (cat == YC_LARGE) return 40;
    }
    int pts = ya_base(p->variant, cat, p->dice);
    if (p->variant == YA_GENERALA) {
        if (cat == YC_DOUBLE && p->box[p->turn][YC_YACHT] != 50) pts = 0;   /* a double needs the generala first */
        if (p->throws == 1 && pts && (cat == YC_STRAIGHT || cat == YC_FULL || cat == YC_FOUR_KIND)) pts += 5;
    }
    return pts;
}

static void push(ygame *g, uint16_t ev)
{
    g->past[g->n] = g->pos;
    g->ev[g->n++] = ev;
}

int ya_throw(ygame *g, unsigned kept, const int v[YA_DICE])
{
    ypos *p = &g->pos;
    if (p->over || p->throws >= YA_THROWS || g->n >= YA_MAX_EVENTS) return 0;
    if (!p->throws) kept = 0;                   /* the first throw: all five */
    kept &= (1u << YA_DICE) - 1;
    if (kept == (1u << YA_DICE) - 1) return 0;  /* nothing to throw */
    for (int i = 0; i < YA_DICE; i++)
        if (!(kept >> i & 1) && (v[i] < 1 || v[i] > 6)) return 0;
    uint16_t ev = YA_THROW_EV;
    uint8_t d[YA_DICE];
    for (int i = 0; i < YA_DICE; i++) {
        d[i] = (uint8_t)(kept >> i & 1 ? p->dice[i] : v[i]);
        ev |= (uint16_t)(d[i] << (3 * i));
    }
    push(g, ev);
    memcpy(p->dice, d, sizeof d);
    p->kept = (uint8_t)kept;
    p->throws++;
    if (p->variant == YA_GENERALA && p->throws == 1 && ya_five(p->dice)) {   /* a served generala: won */
        p->over = 1;
        p->served = 1;
        p->winner = (int8_t)p->turn;
        return 2;
    }
    return 1;
}

int ya_keep(ygame *g, unsigned kept)
{
    ypos *p = &g->pos;
    if (p->over || !p->throws || p->throws >= YA_THROWS) return 0;
    p->kept = (uint8_t)(kept & ((1u << YA_DICE) - 1));
    return 1;
}

int ya_upper(const ypos *p, int player)
{
    int s = 0;
    for (int c = YC_ONES; c <= YC_SIXES; c++) s += p->box[player][c] > 0 ? p->box[player][c] : 0;
    return s;
}

int ya_bonus(const ypos *p, int player)
{
    return ya_upper(p, player) >= YA_BONUS_NEEDS ? ya_bonus_at(p->variant) : 0;
}

int ya_total(const ypos *p, int player)
{
    int s = ya_bonus(p, player) + 100 * p->extra[player];
    for (int c = 0; c < YC_N; c++) s += p->box[player][c] > 0 ? p->box[player][c] : 0;
    return s;
}

int ya_filled(const ypos *p, int player)
{
    int n = 0;
    for (int c = 0; c < YC_N; c++) n += p->box[player][c] >= 0;
    return n;
}

int ya_score(ygame *g, int cat)
{
    ypos *p = &g->pos;
    int pts = ya_points(p, cat);
    if (pts < 0 || g->n >= YA_MAX_EVENTS) return 0;
    push(g, (uint16_t)cat);
    if (p->variant == YA_CLASSIC && ya_five(p->dice) && p->box[p->turn][YC_YACHT] == 50) p->extra[p->turn]++;
    p->box[p->turn][cat] = (int16_t)pts;
    p->turns++;
    p->throws = 0;
    p->kept = 0;
    memset(p->dice, 0, sizeof p->dice);
    int ncat = ya_cats(p->variant, NULL);
    for (int k = 1; k <= p->players; k++) { /* the next player with categories to fill */
        int q = (p->turn + k) % p->players;
        if (ya_filled(p, q) < ncat) {
            p->turn = (uint8_t)q;
            return 1;
        }
    }
    p->over = 1;                                /* every box filled: the highest total wins */
    if (p->players > 1) {
        int best = -1, n = 0;
        for (int q = 0; q < p->players; q++) {
            int t = ya_total(p, q);
            if (t > best) best = t, n = 0, p->winner = (int8_t)q;
            n += t == best;
        }
        if (n > 1) p->winner = -1;
    }
    return 2;
}

int ya_undo(ygame *g)
{
    int k = g->n - 1;
    while (k >= 0 && (g->ev[k] & YA_THROW_EV)) k--;
    if (k < 0) return 0;
    g->pos = g->past[k];
    g->n = k;
    return 1;
}

int ya_cat_by_name(const char *s, int len)
{
    static const struct { const char *name; int cat; } names[] = {
        {"1", YC_ONES}, {"2", YC_TWOS}, {"3", YC_THREES}, {"4", YC_FOURS}, {"5", YC_FIVES}, {"6", YC_SIXES},
        {"p", YC_PAIR}, {"pp", YC_TWO_PAIRS}, {"3k", YC_THREE_KIND}, {"4k", YC_FOUR_KIND}, {"fh", YC_FULL},
        {"ss", YC_SMALL}, {"ls", YC_LARGE}, {"st", YC_STRAIGHT}, {"c", YC_CHANCE}, {"y", YC_YACHT},
        {"yy", YC_DOUBLE},
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if ((int)strlen(names[i].name) == len && !strncmp(names[i].name, s, (size_t)len)) return names[i].cat;
    return -1;
}

int ya_script(ygame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int len = 0;
        while (s[len] && s[len] != ' ') len++;
        if (*s == 'd') {                        /* a throw: five places, a face or "-" (kept) */
            int v[YA_DICE] = {0};
            unsigned kept = 0;
            if (len != 1 + YA_DICE) return 0;
            for (int i = 0; i < YA_DICE; i++) {
                char c = s[1 + i];
                if (c == '-') kept |= 1u << i;
                else if (c >= '1' && c <= '6') v[i] = c - '0';
                else return 0;
            }
            if (!g->pos.throws && kept) return 0;
            if (!ya_throw(g, kept, v)) return 0;
        } else {
            int cat = ya_cat_by_name(s, len);
            if (cat < 0 || !ya_score(g, cat)) return 0;
        }
        s += len;
    }
    return 1;
}
