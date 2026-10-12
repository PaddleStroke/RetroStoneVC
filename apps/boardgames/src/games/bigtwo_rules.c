/*
 * Big Two: the rules (bigtwo.h; the play: games/shed.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/bigtwo.h"

/* the suits from low to high: diamonds, clubs, hearts, spades */
static int suit_index(int card)
{
    static const int8_t order[4] = {3, 2, 0, 1};   /* spades, hearts, diamonds, clubs */
    return order[CARD_SUIT(card)];
}

int bigt_power(int card) { return shed_rank_index(card) * 4 + suit_index(card); }

int bigt_penalty(int left) { return left >= 10 ? 2 * left : left; }

/* five ranks in a row (cards sorted): no 2 */
static int straight(const uint8_t *c)
{
    for (int i = 0; i < 5; i++)
        if (shed_rank_index(c[i]) != shed_rank_index(c[0]) + i || shed_rank_index(c[i]) == 12) return 0;
    return 1;
}

int bigt_classify(const uint8_t *c, int n, shed_combo *out)
{
    int r0 = shed_rank_index(c[0]), top = bigt_power(c[n - 1]);
    out->n = (uint8_t)n;
    switch (n) {
    case 1: out->kind = BIGT_SINGLE, out->key = (uint32_t)top; return 1;
    case 2:
        if (shed_rank_index(c[1]) != r0) return 0;
        out->kind = BIGT_PAIR, out->key = (uint32_t)top;
        return 1;
    case 3:
        if (shed_rank_index(c[1]) != r0 || shed_rank_index(c[2]) != r0) return 0;
        out->kind = BIGT_TRIPLE, out->key = (uint32_t)r0;
        return 1;
    case 5: {
        int flush = 1, run = straight(c), r[5];
        for (int i = 0; i < 5; i++) {
            r[i] = shed_rank_index(c[i]);
            if (CARD_SUIT(c[i]) != CARD_SUIT(c[0])) flush = 0;
        }
        uint32_t cls, sub;
        if (run && flush) cls = BIGT_SFLUSH, sub = (uint32_t)top;
        else if ((r[0] == r[3] || r[1] == r[4]) && r[1] == r[3]) cls = BIGT_FOUR, sub = (uint32_t)r[2];
        else if ((r[0] == r[2] && r[3] == r[4]) || (r[0] == r[1] && r[2] == r[4])) cls = BIGT_HOUSE, sub = (uint32_t)r[2];
        else if (flush)                         /* the ranks from the highest down, then the suit */
            cls = BIGT_FLUSH, sub = (uint32_t)(r[4] << 18 | r[3] << 14 | r[2] << 10 | r[1] << 6 | r[0] << 2) |
                                    (uint32_t)suit_index(c[0]);
        else if (run) cls = BIGT_STRAIGHT, sub = (uint32_t)top;
        else return 0;
        out->kind = BIGT_FIVE, out->key = cls << 24 | sub;
        return 1;
    }
    default: return 0;
    }
}

int bigt_beats(const shed_combo *a, const shed_combo *top) { return a->kind == top->kind && a->key > top->key; }

const shed_rules bigt_rules = {
    .power = bigt_power, .classify = bigt_classify, .beats = bigt_beats,
    .lockout = 0, .first_out_ends = 1, .deal_all = 1,
};
