/*
 * Play by Ear's rules (games/melody.h): the levels, a phrase at random, the keys played back, the hint, the stars.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/melody.h"
#include "games/melody_levels.h"
#include "games/puzrand.h"

_Static_assert(MEL_LEVELS == MEL_LEVEL_COUNT && MEL_CHAPTERS == MEL_CHAPTER_COUNT, "melody.h's counts");

int mel_levels_count(void) { return MEL_LEVELS; }

const mel_level *mel_level_at(int level)
{
    return &mel_levels[level < 0 ? 0 : level >= MEL_LEVELS ? MEL_LEVELS - 1 : level];
}

const mel_tune *mel_tune_at(int tune) { return &mel_tunes[tune < 0 ? 0 : tune >= MEL_TUNES ? MEL_TUNES - 1 : tune]; }

int mel_top(int whites) { return whites <= 8 ? 72 : whites <= 12 ? 79 : 84; }

int mel_whites_for(int highest) { return highest <= 72 ? 8 : highest <= 79 ? 12 : 15; }

int mel_black(int pitch)
{
    int k = pitch % 12;
    return k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
}

int mel_variant_of(int notes) { return notes <= 8 ? MEL_SHORT : notes <= 16 ? MEL_MEDIUM : MEL_LONG; }

static void fresh(mel_game *g)
{
    g->pos = g->mistakes = g->hints = g->listens = 0;
    g->hinted = -1;
}

static void take(mel_game *g, int level, int shift)
{
    const mel_level *L = mel_level_at(level);
    memset(g, 0, sizeof *g);
    g->level = level;
    g->tune = L->tune;
    g->n = L->n;
    g->bpm = mel_tune_at(L->tune)->bpm;
    int top = 0;
    for (int i = 0; i < L->n; i++) {
        g->p[i] = (uint8_t)(L->p[i] + shift);
        g->d[i] = L->d[i];
        if (g->p[i] > top) top = g->p[i];
    }
    g->whites = shift ? mel_whites_for(top) : L->whites;
    fresh(g);
}

void mel_load(mel_game *g, int level) { take(g, level, 0); }

void mel_random(mel_game *g, int variant, uint32_t seed)
{
    uint32_t s = pr_seed(seed);
    int pool[MEL_LEVELS], np = 0;
    for (int i = 0; i < MEL_LEVELS; i++)
        if (mel_variant_of(mel_levels[i].n) == variant) pool[np++] = i;
    int level = np ? pool[pr_below(&s, np)] : 0;
    const mel_level *L = &mel_levels[level];
    int lo = 127, hi = 0;
    for (int i = 0; i < L->n; i++) {
        if (L->p[i] < lo) lo = L->p[i];
        if (L->p[i] > hi) hi = L->p[i];
    }
    int shifts[15], ns = 0;                     /* another key, within the keyboard: up to a fifth either way */
    for (int k = -7; k <= 7; k++)
        if (k && lo + k >= MEL_LOW && hi + k <= MEL_HIGH) shifts[ns++] = k;
    int shift = ns && pr_below(&s, 3) ? shifts[pr_below(&s, ns)] : 0;
    take(g, level, shift);
}

int mel_solved(const mel_game *g) { return g->n > 0 && g->pos >= g->n; }

int mel_next(const mel_game *g) { return mel_solved(g) ? -1 : g->p[g->pos]; }

int mel_press(mel_game *g, int pitch)
{
    if (mel_solved(g)) return MEL_OVER;
    if (pitch != g->p[g->pos]) {
        g->mistakes++;
        return MEL_WRONG;
    }
    g->pos++;
    return mel_solved(g) ? MEL_DONE : MEL_RIGHT;
}

int mel_hint(mel_game *g)
{
    if (mel_solved(g)) return -1;
    if (g->hinted != g->pos) {
        g->hints++;
        g->hinted = g->pos;
    }
    return g->p[g->pos];
}

void mel_listen(mel_game *g) { g->listens++; }

int mel_undo(mel_game *g)
{
    if (g->pos <= 0) return 0;
    g->pos--;
    return 1;
}

int mel_help(const mel_game *g)
{
    int extra = g->listens - MEL_LISTENS_FREE;
    return g->hints + (extra > 0 ? extra : 0);
}

int mel_stars(const mel_game *g)
{
    int h = mel_help(g), m = g->mistakes;
    return h == 0 && m == 0 ? 3 : h + m <= 2 ? 2 : 1;
}

int mel_ms(const mel_game *g, int i)
{
    int t = 0;
    for (int k = 0; k < i && k < g->n; k++) t += g->d[k];
    return t * 15000 / (g->bpm ? g->bpm : 100);   /* a sixteenth: 15 / bpm seconds */
}

int mel_len_ms(const mel_game *g, int i)
{
    return i < 0 || i >= g->n ? 0 : g->d[i] * 15000 / (g->bpm ? g->bpm : 100);
}

int mel_parse_note(const char *s, int *used)
{
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7};   /* A B C D E F G */
    int k = 0, m;
    if (s[k] < 'A' || s[k] > 'G') return -1;
    m = base[s[k++] - 'A'];
    if (s[k] == '#') { m++; k++; }
    else if (s[k] == 'b') { m--; k++; }
    if (s[k] < '0' || s[k] > '9') return -1;
    int octave = s[k++] - '0';
    if (used) *used = k;
    return 12 * (octave + 1) + m;
}

int mel_script(mel_game *g, const char *moves)
{
    const char *s = moves;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (*s == 'h' && (s[1] == ' ' || !s[1])) {
            mel_hint(g);
            s++;
            continue;
        }
        if (*s == 'l' && (s[1] == ' ' || !s[1])) {
            mel_listen(g);
            s++;
            continue;
        }
        int used, p = mel_parse_note(s, &used);
        if (p < 0) return 0;
        mel_press(g, p);
        s += used;
    }
    return 1;
}
