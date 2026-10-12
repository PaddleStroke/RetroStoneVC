/*
 * Tiến lên: the rules (tienlen.h; the play: games/shed.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/tienlen.h"

/* the suits from low to high: spades, clubs, diamonds, hearts */
static int suit_index(int card)
{
    static const int8_t order[4] = {0, 3, 2, 1};   /* spades, hearts, diamonds, clubs */
    return order[CARD_SUIT(card)];
}

int tln_power(int card) { return shed_rank_index(card) * 4 + suit_index(card); }

static int same_rank(const uint8_t *c, int n)
{
    for (int i = 1; i < n; i++)
        if (shed_rank_index(c[i]) != shed_rank_index(c[0])) return 0;
    return 1;
}

int tln_classify(const uint8_t *c, int n, shed_combo *out)
{
    out->n = (uint8_t)n;
    out->key = (uint32_t)tln_power(c[n - 1]);   /* the highest card */
    if (n <= 4 && same_rank(c, n)) {
        out->kind = (uint8_t)(TLN_SINGLE + n - 1);
        return 1;
    }
    if (shed_rank_index(c[n - 1]) == 12) return 0;   /* no 2 in a sequence */
    if (n >= 3) {                               /* a sequence: ranks one apart */
        int ok = 1;
        for (int i = 1; i < n && ok; i++) ok = shed_rank_index(c[i]) == shed_rank_index(c[0]) + i;
        if (ok) { out->kind = TLN_SEQ; return 1; }
    }
    if (n >= 6 && n % 2 == 0) {                 /* pairs one apart */
        int ok = 1;
        for (int i = 0; i < n && ok; i += 2)
            ok = shed_rank_index(c[i]) == shed_rank_index(c[0]) + i / 2 && shed_rank_index(c[i + 1]) == shed_rank_index(c[i]);
        if (ok) { out->kind = TLN_DSEQ; return 1; }
    }
    return 0;
}

int tln_bomb(const shed_combo *c)
{
    if (c->kind == TLN_QUAD) return 2;
    if (c->kind != TLN_DSEQ) return 0;
    return c->n == 6 ? 1 : c->n == 8 ? 3 : 4;
}

/* the 2s a bomb class beats: a single (1 and up), a pair (3 and up), three (4) */
static int twos_need(const shed_combo *top)
{
    if (top->key < 48) return 0;                /* not 2s */
    return top->kind == TLN_SINGLE ? 1 : top->kind == TLN_PAIR ? 3 : top->kind == TLN_TRIPLE ? 4 : 0;
}

int tln_beats(const shed_combo *a, const shed_combo *top)
{
    if (a->kind == top->kind && a->n == top->n) return a->key > top->key;
    int b = tln_bomb(a), tb = tln_bomb(top), need = twos_need(top);
    if (!b) return 0;
    if (need) return b >= need;
    return tb && b > tb;
}

const shed_rules tln_rules = {
    .power = tln_power, .classify = tln_classify, .beats = tln_beats,
    .lockout = 1, .first_out_ends = 0, .deal_all = 0,
};
