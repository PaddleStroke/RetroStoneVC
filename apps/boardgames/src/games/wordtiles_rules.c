/*
 * Word Tiles: the rules (wordtiles.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The letter sets: for each language its letters (accents folded where the language's custom ignores them: French,
 * Spanish, Italian, Portuguese, Greek), with how often each appears in running text. From that frequency f (in
 * percent): the counts, 100 letter tiles in proportion to f^0.9 (a little evened out), at least one of each, the
 * largest remainders rounded up; the values, 1 + round(1.25 * log2(6 / f)) from 1 to 10 (1 for f of 6 and more).
 * The rare letters a language borrows (Italian's J, K, W, X, Y...) are left out. Two blanks besides.
 */
#include <string.h>

#include "cards.h"
#include "games/wordtiles.h"

/* the sets, generated from the frequency tables (letter, count, value) */
static const wset SETS[] = {
    {"en", 26, {
        {0x0041, 8, 1}, {0x0042, 2, 3}, {0x0043, 3, 2}, {0x0044, 4, 2}, {0x0045, 11, 1}, {0x0046, 2, 3},
        {0x0047, 2, 3}, {0x0048, 6, 1}, {0x0049, 7, 1}, {0x004A, 1, 8}, {0x004B, 1, 5}, {0x004C, 4, 2},
        {0x004D, 3, 3}, {0x004E, 6, 1}, {0x004F, 7, 1}, {0x0050, 2, 3}, {0x0051, 1, 8}, {0x0052, 6, 1},
        {0x0053, 6, 1}, {0x0054, 8, 1}, {0x0055, 3, 2}, {0x0056, 1, 4}, {0x0057, 2, 3}, {0x0058, 1, 8},
        {0x0059, 2, 3}, {0x005A, 1, 9}
    }},
    {"fr", 26, {
        {0x0041, 7, 1}, {0x0042, 1, 4}, {0x0043, 4, 2}, {0x0044, 4, 2}, {0x0045, 13, 1}, {0x0046, 1, 4},
        {0x0047, 1, 4}, {0x0048, 1, 5}, {0x0049, 7, 1}, {0x004A, 1, 5}, {0x004B, 1, 10}, {0x004C, 6, 1},
        {0x004D, 3, 2}, {0x004E, 7, 1}, {0x004F, 5, 1}, {0x0050, 3, 2}, {0x0051, 2, 4}, {0x0052, 6, 1},
        {0x0053, 8, 1}, {0x0054, 7, 1}, {0x0055, 6, 1}, {0x0056, 2, 3}, {0x0057, 1, 10}, {0x0058, 1, 6},
        {0x0059, 1, 6}, {0x005A, 1, 8}
    }},
    {"de", 29, {
        {0x0041, 6, 1}, {0x00C4, 1, 5}, {0x0042, 2, 3}, {0x0043, 3, 2}, {0x0044, 5, 1}, {0x0045, 14, 1},
        {0x0046, 2, 3}, {0x0047, 3, 2}, {0x0048, 4, 1}, {0x0049, 7, 1}, {0x004A, 1, 7}, {0x004B, 1, 4},
        {0x004C, 3, 2}, {0x004D, 3, 3}, {0x004E, 9, 1}, {0x004F, 3, 3}, {0x00D6, 1, 6}, {0x0050, 1, 5},
        {0x0051, 1, 10}, {0x0052, 6, 1}, {0x0053, 7, 1}, {0x0054, 6, 1}, {0x0055, 4, 2}, {0x00DC, 1, 5},
        {0x0056, 1, 5}, {0x0057, 2, 3}, {0x0058, 1, 10}, {0x0059, 1, 10}, {0x005A, 1, 4}
    }},
    {"es", 25, {
        {0x0041, 11, 1}, {0x0042, 2, 4}, {0x0043, 5, 1}, {0x0044, 6, 1}, {0x0045, 12, 1}, {0x0046, 1, 5},
        {0x0047, 1, 4}, {0x0048, 1, 5}, {0x0049, 6, 1}, {0x004A, 1, 6}, {0x004C, 5, 1}, {0x004D, 3, 2},
        {0x004E, 6, 1}, {0x00D1, 1, 6}, {0x004F, 8, 1}, {0x0050, 3, 3}, {0x0051, 1, 4}, {0x0052, 7, 1},
        {0x0053, 7, 1}, {0x0054, 5, 1}, {0x0055, 4, 2}, {0x0056, 1, 4}, {0x0058, 1, 7}, {0x0059, 1, 4},
        {0x005A, 1, 5}
    }},
    {"it", 21, {
        {0x0041, 11, 1}, {0x0042, 1, 4}, {0x0043, 5, 2}, {0x0044, 4, 2}, {0x0045, 11, 1}, {0x0046, 1, 4},
        {0x0047, 2, 3}, {0x0048, 2, 3}, {0x0049, 11, 1}, {0x004C, 6, 1}, {0x004D, 3, 3}, {0x004E, 7, 1},
        {0x004F, 9, 1}, {0x0050, 3, 2}, {0x0051, 1, 5}, {0x0052, 6, 1}, {0x0053, 5, 1}, {0x0054, 6, 1},
        {0x0055, 3, 2}, {0x0056, 2, 3}, {0x005A, 1, 5}
    }},
    {"pt", 24, {
        {0x0041, 13, 1}, {0x0042, 1, 4}, {0x0043, 4, 2}, {0x00C7, 1, 5}, {0x0044, 5, 1}, {0x0045, 11, 1},
        {0x0046, 1, 4}, {0x0047, 2, 4}, {0x0048, 2, 4}, {0x0049, 6, 1}, {0x004A, 1, 6}, {0x004C, 3, 2},
        {0x004D, 5, 1}, {0x004E, 5, 1}, {0x004F, 10, 1}, {0x0050, 3, 3}, {0x0051, 1, 4}, {0x0052, 6, 1},
        {0x0053, 7, 1}, {0x0054, 4, 2}, {0x0055, 5, 1}, {0x0056, 2, 3}, {0x0058, 1, 7}, {0x005A, 1, 5}
    }},
    {"nl", 26, {
        {0x0041, 7, 1}, {0x0042, 2, 3}, {0x0043, 1, 4}, {0x0044, 6, 1}, {0x0045, 16, 1}, {0x0046, 1, 5},
        {0x0047, 3, 2}, {0x0048, 3, 3}, {0x0049, 6, 1}, {0x004A, 2, 3}, {0x004B, 2, 3}, {0x004C, 4, 2},
        {0x004D, 2, 3}, {0x004E, 9, 1}, {0x004F, 6, 1}, {0x0050, 2, 3}, {0x0051, 1, 10}, {0x0052, 6, 1},
        {0x0053, 4, 2}, {0x0054, 6, 1}, {0x0055, 2, 3}, {0x0056, 3, 2}, {0x0057, 2, 3}, {0x0058, 1, 10},
        {0x0059, 1, 10}, {0x005A, 2, 4}
    }},
    {"da", 25, {
        {0x0041, 6, 1}, {0x0042, 2, 3}, {0x0043, 1, 5}, {0x0044, 6, 1}, {0x0045, 14, 1}, {0x0046, 3, 3},
        {0x0047, 4, 2}, {0x0048, 2, 3}, {0x0049, 6, 1}, {0x004A, 1, 5}, {0x004B, 4, 2}, {0x004C, 5, 1},
        {0x004D, 3, 2}, {0x004E, 7, 1}, {0x004F, 5, 1}, {0x0050, 2, 3}, {0x0052, 8, 1}, {0x0053, 6, 1},
        {0x0054, 7, 1}, {0x0055, 2, 3}, {0x0056, 2, 3}, {0x0059, 1, 5}, {0x00C6, 1, 4}, {0x00D8, 1, 4},
        {0x00C5, 1, 4}
    }},
    {"nb", 25, {
        {0x0041, 6, 1}, {0x0042, 2, 3}, {0x0043, 1, 8}, {0x0044, 5, 2}, {0x0045, 14, 1}, {0x0046, 2, 3},
        {0x0047, 4, 2}, {0x0048, 2, 3}, {0x0049, 6, 1}, {0x004A, 1, 4}, {0x004B, 3, 2}, {0x004C, 5, 1},
        {0x004D, 4, 2}, {0x004E, 8, 1}, {0x004F, 5, 1}, {0x0050, 2, 3}, {0x0052, 8, 1}, {0x0053, 6, 1},
        {0x0054, 7, 1}, {0x0055, 2, 3}, {0x0056, 3, 3}, {0x0059, 1, 5}, {0x00C6, 1, 7}, {0x00D8, 1, 4},
        {0x00C5, 1, 4}
    }},
    {"sv", 27, {
        {0x0041, 9, 1}, {0x0042, 2, 3}, {0x0043, 2, 3}, {0x0044, 5, 1}, {0x0045, 9, 1}, {0x0046, 2, 3},
        {0x0047, 3, 2}, {0x0048, 2, 3}, {0x0049, 6, 1}, {0x004A, 1, 5}, {0x004B, 3, 2}, {0x004C, 5, 1},
        {0x004D, 4, 2}, {0x004E, 8, 1}, {0x004F, 4, 2}, {0x0050, 2, 3}, {0x0052, 8, 1}, {0x0053, 6, 1},
        {0x0054, 7, 1}, {0x0055, 2, 3}, {0x0056, 3, 3}, {0x0058, 1, 8}, {0x0059, 1, 5}, {0x005A, 1, 10},
        {0x00C5, 1, 4}, {0x00C4, 2, 3}, {0x00D6, 1, 4}
    }},
    {"fi", 23, {
        {0x0041, 11, 1}, {0x0042, 1, 6}, {0x0044, 1, 4}, {0x0045, 8, 1}, {0x0046, 1, 7}, {0x0047, 1, 6},
        {0x0048, 2, 3}, {0x0049, 10, 1}, {0x004A, 2, 3}, {0x004B, 5, 1}, {0x004C, 6, 1}, {0x004D, 3, 2},
        {0x004E, 8, 1}, {0x004F, 6, 1}, {0x0050, 2, 3}, {0x0052, 3, 2}, {0x0053, 7, 1}, {0x0054, 8, 1},
        {0x0055, 5, 1}, {0x0056, 3, 3}, {0x0059, 2, 3}, {0x00C4, 4, 2}, {0x00D6, 1, 6}
    }},
    {"pl", 32, {
        {0x0041, 8, 1}, {0x0104, 1, 4}, {0x0042, 2, 3}, {0x0043, 4, 2}, {0x0106, 1, 6}, {0x0044, 3, 2},
        {0x0045, 7, 1}, {0x0118, 1, 4}, {0x0046, 1, 6}, {0x0047, 2, 4}, {0x0048, 1, 4}, {0x0049, 7, 1},
        {0x004A, 2, 3}, {0x004B, 3, 2}, {0x004C, 2, 3}, {0x0141, 2, 3}, {0x004D, 3, 2}, {0x004E, 5, 1},
        {0x0143, 1, 7}, {0x004F, 7, 1}, {0x00D3, 1, 5}, {0x0050, 3, 2}, {0x0052, 5, 1}, {0x0053, 4, 2},
        {0x015A, 1, 5}, {0x0054, 4, 2}, {0x0055, 3, 3}, {0x0057, 5, 1}, {0x0059, 4, 2}, {0x005A, 5, 1},
        {0x0179, 1, 9}, {0x017B, 1, 5}
    }},
    {"cs", 39, {
        {0x0041, 6, 1}, {0x00C1, 2, 3}, {0x0042, 2, 3}, {0x0043, 2, 3}, {0x010C, 1, 4}, {0x0044, 3, 2},
        {0x010E, 1, 10}, {0x0045, 7, 1}, {0x00C9, 2, 4}, {0x011A, 2, 4}, {0x0046, 1, 6}, {0x0047, 1, 8},
        {0x0048, 1, 4}, {0x0049, 4, 2}, {0x00CD, 3, 2}, {0x004A, 2, 3}, {0x004B, 4, 2}, {0x004C, 4, 2},
        {0x004D, 3, 2}, {0x004E, 6, 1}, {0x0147, 1, 8}, {0x004F, 8, 1}, {0x00D3, 1, 10}, {0x0050, 3, 2},
        {0x0052, 3, 2}, {0x0158, 1, 4}, {0x0053, 4, 1}, {0x0160, 1, 5}, {0x0054, 5, 1}, {0x0164, 1, 8},
        {0x0055, 2, 3}, {0x00DA, 1, 8}, {0x016E, 1, 5}, {0x0056, 4, 1}, {0x0058, 1, 10}, {0x0059, 2, 3},
        {0x00DD, 1, 4}, {0x005A, 2, 3}, {0x017D, 1, 4}
    }},
    {"ru", 32, {
        {0x0410, 7, 1}, {0x0411, 2, 3}, {0x0412, 4, 2}, {0x0413, 2, 3}, {0x0414, 3, 2}, {0x0415, 8, 1},
        {0x0416, 1, 4}, {0x0417, 2, 3}, {0x0418, 7, 1}, {0x0419, 1, 4}, {0x041A, 3, 2}, {0x041B, 4, 2},
        {0x041C, 3, 2}, {0x041D, 6, 1}, {0x041E, 10, 1}, {0x041F, 3, 2}, {0x0420, 4, 1}, {0x0421, 5, 1},
        {0x0422, 6, 1}, {0x0423, 3, 2}, {0x0424, 1, 7}, {0x0425, 1, 4}, {0x0426, 1, 6}, {0x0427, 2, 4},
        {0x0428, 1, 5}, {0x0429, 1, 6}, {0x042A, 1, 10}, {0x042B, 2, 3}, {0x042C, 2, 3}, {0x042D, 1, 6},
        {0x042E, 1, 5}, {0x042F, 2, 3}
    }},
    {"uk", 33, {
        {0x0410, 7, 1}, {0x0411, 2, 3}, {0x0412, 5, 1}, {0x0413, 2, 3}, {0x0490, 1, 10}, {0x0414, 3, 2},
        {0x0415, 5, 1}, {0x0404, 1, 5}, {0x0416, 1, 4}, {0x0417, 2, 3}, {0x0418, 6, 1}, {0x0406, 5, 1},
        {0x0407, 1, 4}, {0x0419, 1, 4}, {0x041A, 4, 2}, {0x041B, 4, 2}, {0x041C, 3, 2}, {0x041D, 7, 1},
        {0x041E, 8, 1}, {0x041F, 3, 2}, {0x0420, 4, 1}, {0x0421, 4, 2}, {0x0422, 5, 1}, {0x0423, 4, 2},
        {0x0424, 1, 6}, {0x0425, 1, 4}, {0x0426, 1, 5}, {0x0427, 2, 4}, {0x0428, 1, 5}, {0x0429, 1, 6},
        {0x042C, 2, 3}, {0x042E, 1, 5}, {0x042F, 2, 3}
    }},
    {"el", 24, {
        {0x0391, 11, 1}, {0x0392, 1, 5}, {0x0393, 2, 3}, {0x0394, 2, 3}, {0x0395, 8, 1}, {0x0396, 1, 6},
        {0x0397, 5, 1}, {0x0398, 1, 4}, {0x0399, 8, 1}, {0x039A, 4, 2}, {0x039B, 3, 2}, {0x039C, 4, 2},
        {0x039D, 6, 1}, {0x039E, 1, 6}, {0x039F, 9, 1}, {0x03A0, 4, 2}, {0x03A1, 5, 2}, {0x03A3, 8, 1},
        {0x03A4, 8, 1}, {0x03A5, 4, 2}, {0x03A6, 1, 5}, {0x03A7, 1, 4}, {0x03A8, 1, 8}, {0x03A9, 2, 3}
    }},
    {"tr", 29, {
        {0x0041, 11, 1}, {0x0042, 3, 2}, {0x0043, 1, 4}, {0x00C7, 1, 4}, {0x0044, 5, 1}, {0x0045, 8, 1},
        {0x0046, 1, 5}, {0x0047, 1, 4}, {0x011E, 1, 4}, {0x0048, 1, 4}, {0x0049, 5, 1}, {0x0130, 8, 1},
        {0x004A, 1, 10}, {0x004B, 5, 1}, {0x004C, 6, 1}, {0x004D, 4, 2}, {0x004E, 7, 1}, {0x004F, 3, 3},
        {0x00D6, 1, 5}, {0x0050, 1, 4}, {0x0052, 7, 1}, {0x0053, 3, 2}, {0x015E, 2, 3}, {0x0054, 3, 2},
        {0x0055, 3, 2}, {0x00DC, 2, 3}, {0x0056, 1, 4}, {0x0059, 3, 2}, {0x005A, 2, 3}
    }},
    {"id", 24, {
        {0x0041, 17, 1}, {0x0042, 3, 3}, {0x0043, 1, 5}, {0x0044, 4, 2}, {0x0045, 8, 1}, {0x0046, 1, 7},
        {0x0047, 4, 2}, {0x0048, 3, 3}, {0x0049, 8, 1}, {0x004A, 1, 4}, {0x004B, 5, 2}, {0x004C, 3, 2},
        {0x004D, 4, 2}, {0x004E, 9, 1}, {0x004F, 3, 2}, {0x0050, 3, 3}, {0x0052, 4, 2}, {0x0053, 4, 2},
        {0x0054, 5, 1}, {0x0055, 5, 1}, {0x0056, 1, 8}, {0x0057, 1, 5}, {0x0059, 2, 3}, {0x005A, 1, 10}
    }},
};
#define NSETS ((int)(sizeof SETS / sizeof SETS[0]))

int wt_sets(void) { return NSETS; }
const wset *wt_set(int i) { return &SETS[i >= 0 && i < NSETS ? i : 0]; }

int wt_set_for(const char *lang)
{
    if (!lang) return 0;
    for (int i = 0; i < NSETS; i++) {
        size_t n = strlen(SETS[i].lang);
        if (!strncmp(lang, SETS[i].lang, n) && (lang[n] == 0 || lang[n] == '_' || lang[n] == '-')) return i;
    }
    return 0;                                   /* ja, ko, zh_CN and the others: English */
}

/* ---- the board: our own premium squares, one quarter mirrored both ways --------------------------------------- */
static const char QUARTER[8][9] = {
    "l...W..d",                                 /* l triple letter, d double letter, w double word, W triple word */
    "...d....",
    "......w.",
    ".d.....l",
    "W...w.d.",
    ".....d..",
    "..w.d...",
    "d..l...*",
};

int wt_premium(int cell)
{
    if (cell < 0 || cell >= WT_CELLS) return WQ_NONE;
    int r = cell / WT_N, c = cell % WT_N;
    r = r < WT_N - 1 - r ? r : WT_N - 1 - r;
    c = c < WT_N - 1 - c ? c : WT_N - 1 - c;
    switch (QUARTER[r][c]) {
    case 'd': return WQ_DL;
    case 'l': return WQ_TL;
    case 'w': return WQ_DW;
    case 'W': return WQ_TW;
    case '*': return WQ_STAR;
    default: return WQ_NONE;
    }
}

int wt_value(const wpos *p, int tile)
{
    if (!tile || tile & WT_BLANK) return 0;
    const wset *s = wt_set(p->set);
    return tile <= s->n ? s->l[tile - 1].value : 0;
}

uint32_t wt_char(const wpos *p, int tile)
{
    const wset *s = wt_set(p->set);
    int l = WT_LETTER(tile);
    return l >= 1 && l <= s->n ? s->l[l - 1].cp : 0;
}

int wt_rack_value(const wpos *p, int pl)
{
    int v = 0;
    for (int i = 0; i < WT_RACK; i++) v += wt_value(p, p->rack[pl][i]);
    return v;
}

int wt_rack_count(const wpos *p, int pl)
{
    int n = 0;
    for (int i = 0; i < WT_RACK; i++) n += p->rack[pl][i] != 0;
    return n;
}

/* ---- the bag ----------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static void shake(wpos *p)                      /* the bag shuffled (after an exchange, a challenge) */
{
    p->mixes++;
    cards_shuffle(p->bag, p->nbag, mix(p->seed, 77u + p->mixes));
}

static void refill(wpos *p, int pl, const uint8_t *slots, int n, uint8_t *drawn)
{
    for (int i = 0; i < n; i++) {
        uint8_t t = p->nbag > 0 ? p->bag[--p->nbag] : 0;
        p->rack[pl][slots[i]] = t;
        if (drawn) drawn[i] = t;
    }
}

void wt_new(wgame *g, int set, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    wpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->set = p->set = (uint8_t)(set >= 0 && set < NSETS ? set : 0);
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > WT_MAXP ? WT_MAXP : players);
    const wset *s = wt_set(p->set);
    int n = 0;
    for (int i = 0; i < s->n; i++)
        for (int k = 0; k < s->l[i].count && n < WT_TILES; k++) p->bag[n++] = (uint8_t)(i + 1);
    for (int k = 0; k < WT_BLANKS && n < WT_TILES; k++) p->bag[n++] = WT_BLANK;
    p->nbag = (int16_t)n;
    cards_shuffle(p->bag, n, mix(seed, 1));
    for (int pl = 0; pl < p->players; pl++) {
        const uint8_t all[WT_RACK] = {0, 1, 2, 3, 4, 5, 6};
        refill(p, pl, all, WT_RACK, NULL);
    }
    p->ender = 255;
    p->phase = WP_PLAY;
}

/* ---- a play: where its tiles go, the words it makes, their score ------------------------------------------------ */
static int tile_at(const uint8_t *b, int r, int c)
{
    return r >= 0 && c >= 0 && r < WT_N && c < WT_N ? b[r * WT_N + c] : 0;
}

/* the word through (r, c) along (dr, dc) on board b: its first cell and its length */
static int word_at(const uint8_t *b, int r, int c, int dr, int dc, int *at)
{
    while (tile_at(b, r - dr, c - dc)) r -= dr, c -= dc;
    *at = r * WT_N + c;
    int n = 0;
    while (tile_at(b, r, c)) r += dr, c += dc, n++;
    return n;
}

/* a word's score: its letters (a letter premium under a new tile), times the word premiums under the new tiles */
static int word_score(const wpos *p, const uint8_t *b, const uint8_t *fresh, int at, int down, int len)
{
    int sum = 0, mul = 1;
    for (int i = 0; i < len; i++) {
        int cell = at + i * (down ? WT_N : 1), v = wt_value(p, b[cell]);
        if (fresh[cell]) switch (wt_premium(cell)) {
            case WQ_DL: v *= 2; break;
            case WQ_TL: v *= 3; break;
            case WQ_DW: case WQ_STAR: mul *= 2; break;
            case WQ_TW: mul *= 3; break;
            }
        sum += v;
    }
    return sum * mul;
}

int wt_eval(const wpos *p, const wact *a, wscore *s)
{
    wscore dummy;
    if (!s) s = &dummy;
    memset(s, 0, sizeof *s);
    if (p->phase != WP_PLAY || a->kind != WA_PLAY) return WE_TURN;
    int n = a->n, pl = p->turn;
    if (n < 1 || n > WT_RACK) return WE_TILES;
    uint8_t b[WT_CELLS], fresh[WT_CELLS] = {0};
    int used = 0, empty = 1;
    memcpy(b, p->board, sizeof b);
    for (int i = 0; i < WT_CELLS; i++) empty &= !b[i];
    for (int i = 0; i < n; i++) {
        int sl = a->slot[i], cell = a->cell[i];
        if (sl >= WT_RACK || used >> sl & 1 || !p->rack[pl][sl]) return WE_TILES;
        used |= 1 << sl;
        if (cell >= WT_CELLS || b[cell]) return WE_CELL;
        int t = p->rack[pl][sl];
        if (t == WT_BLANK) {
            if (a->letter[i] < 1 || a->letter[i] > wt_set(p->set)->n) return WE_TILES;
            t = WT_BLANK | a->letter[i];
        }
        b[cell] = (uint8_t)t;
        fresh[cell] = 1;
    }
    /* one line: a row or a column */
    int r0 = a->cell[0] / WT_N, c0 = a->cell[0] % WT_N, row = 1, col = 1, lo = WT_CELLS, hi = -1;
    for (int i = 0; i < n; i++) {
        row &= a->cell[i] / WT_N == r0;
        col &= a->cell[i] % WT_N == c0;
        lo = a->cell[i] < lo ? a->cell[i] : lo;
        hi = a->cell[i] > hi ? a->cell[i] : hi;
    }
    if (!row && !col) return WE_LINE;
    int down = !row, step = down ? WT_N : 1;
    for (int cell = lo; cell <= hi; cell += step)           /* no gap: the tiles between are on the board */
        if (!b[cell]) return WE_GAP;
    if (empty) {
        if (!fresh[WT_CENTRE]) return WE_CENTRE;
        if (n < 2) return WE_ONE;
    } else {
        int touch = 0;
        for (int i = 0; i < n && !touch; i++) {
            int r = a->cell[i] / WT_N, c = a->cell[i] % WT_N;
            static const int d[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (int k = 0; k < 4; k++) {
                int rr = r + d[k][0], cc = c + d[k][1];
                if (rr >= 0 && cc >= 0 && rr < WT_N && cc < WT_N && p->board[rr * WT_N + cc]) touch = 1;
            }
        }
        if (!touch) return WE_ALONE;
    }
    /* the words: along the line (a single tile: both ways), and across it at each new tile */
    for (int dir = 0; dir < 2; dir++) {
        int along = dir == down;
        if (!along && n > 1) {
            for (int i = 0; i < n; i++) {
                int at, len = word_at(b, a->cell[i] / WT_N, a->cell[i] % WT_N, dir, !dir, &at);
                if (len < 2 || s->words >= WT_WORDS) continue;
                int v = word_score(p, b, fresh, at, dir, len);
                s->w[s->words].at = at, s->w[s->words].down = dir, s->w[s->words].len = len, s->w[s->words].score = v;
                s->words++;
                s->score += v;
            }
            continue;
        }
        int at, len = word_at(b, lo / WT_N, lo % WT_N, dir, !dir, &at);
        if (len < 2 || s->words >= WT_WORDS) continue;
        int v = word_score(p, b, fresh, at, dir, len);
        /* the main word first */
        memmove(&s->w[1], &s->w[0], sizeof s->w[0] * (size_t)s->words);
        s->w[0].at = at, s->w[0].down = dir, s->w[0].len = len, s->w[0].score = v;
        s->words++;
        s->score += v;
    }
    if (n == WT_RACK) s->bingo = WT_BINGO, s->score += WT_BINGO;
    return s->score;
}

int wt_can_challenge(const wpos *p)
{
    return (p->phase == WP_PLAY || p->phase == WP_FINAL) && p->last.kind == WA_PLAY && p->last.player != p->turn;
}

int wt_can_exchange(const wpos *p) { return p->nbag >= WT_RACK; }

int wt_legal(const wpos *p, const wact *a)
{
    switch (a->kind) {
    case WA_PLAY: return wt_eval(p, a, NULL) >= 0;
    case WA_EXCHANGE: {
        if (p->phase != WP_PLAY || a->n > WT_RACK) return 0;
        if (a->n && !wt_can_exchange(p)) return 0;
        int used = 0;
        for (int i = 0; i < a->n; i++) {
            int sl = a->slot[i];
            if (sl >= WT_RACK || used >> sl & 1 || !p->rack[p->turn][sl]) return 0;
            used |= 1 << sl;
        }
        return 1;
    }
    case WA_CHALLENGE: return wt_can_challenge(p);
    case WA_ACCEPT: return p->phase == WP_FINAL;
    default: return 0;
    }
}

/* the end: each player loses the tiles left on their rack; the one who went out gains them all */
static void finish(wpos *p, int ender)
{
    int sum = 0;
    for (int q = 0; q < p->players; q++) {
        int v = wt_rack_value(p, q);
        p->left[q] = (int16_t)-v;
        sum += v;
    }
    if (ender >= 0 && ender < p->players) p->left[ender] = (int16_t)(p->left[ender] + sum);
    for (int q = 0; q < p->players; q++) p->score[q] = (int16_t)(p->score[q] + p->left[q]);
    p->ender = (uint8_t)(ender >= 0 ? ender : 255);
    p->phase = WP_OVER;
}

static void next_turn(wpos *p) { p->turn = (uint8_t)((p->turn + 1) % p->players); }

static void apply(wpos *p, const wact *a)
{
    int pl = p->turn;
    switch (a->kind) {
    case WA_PLAY: {
        wscore s;
        int v = wt_eval(p, a, &s);
        p->last.kind = WA_PLAY;
        p->last.player = (uint8_t)pl;
        p->last.n = a->n;
        p->last.scoreless = p->scoreless;
        p->last.score = (int16_t)v;
        for (int i = 0; i < a->n; i++) {
            int t = p->rack[pl][a->slot[i]];
            p->last.cell[i] = a->cell[i];
            p->last.slot[i] = a->slot[i];
            p->last.tile[i] = (uint8_t)t;
            p->board[a->cell[i]] = (uint8_t)(t == WT_BLANK ? WT_BLANK | a->letter[i] : t);
            p->rack[pl][a->slot[i]] = 0;
        }
        p->score[pl] = (int16_t)(p->score[pl] + v);
        refill(p, pl, a->slot, a->n, p->last.drawn);
        p->scoreless = v ? 0 : (uint8_t)(p->scoreless + 1);
        next_turn(p);
        if (!wt_rack_count(p, pl)) p->phase = WP_FINAL;     /* out: the next player may still challenge */
        else if (p->scoreless >= WT_SCORELESS) finish(p, -1);
        break;
    }
    case WA_EXCHANGE: {
        uint8_t old[WT_RACK];
        for (int i = 0; i < a->n; i++) old[i] = p->rack[pl][a->slot[i]];
        refill(p, pl, a->slot, a->n, NULL);     /* the new tiles first, then the old ones go back in */
        for (int i = 0; i < a->n; i++) p->bag[p->nbag++] = old[i];
        if (a->n) shake(p);
        p->last.kind = WA_EXCHANGE;
        p->last.player = (uint8_t)pl;
        p->last.n = a->n;
        p->scoreless++;
        next_turn(p);
        if (p->scoreless >= WT_SCORELESS) finish(p, -1);
        break;
    }
    case WA_CHALLENGE: {                        /* the last play taken back: its player has lost that turn */
        int q = p->last.player;
        for (int i = 0; i < p->last.n; i++) {
            p->board[p->last.cell[i]] = 0;
            uint8_t *slot = &p->rack[q][p->last.slot[i]];
            if (*slot) p->bag[p->nbag++] = *slot;
            *slot = p->last.tile[i];
        }
        shake(p);
        p->score[q] = (int16_t)(p->score[q] - p->last.score);
        p->scoreless = (uint8_t)(p->last.scoreless + 1);
        p->last.kind = WA_CHALLENGE;
        p->phase = WP_PLAY;                     /* the challenger plays on: the turn stays theirs */
        if (p->scoreless >= WT_SCORELESS) finish(p, -1);
        break;
    }
    case WA_ACCEPT:
        p->last.kind = WA_ACCEPT;
        finish(p, p->last.player);
        break;
    }
}

int wt_play(wgame *g, const wact *a)
{
    if (g->n >= WT_MAX_ACTS || !wt_legal(&g->pos, a)) return 0;
    apply(&g->pos, a);
    g->act[g->n++] = *a;
    return 1;
}

int wt_undo(wgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static wact acts[WT_MAX_ACTS];
    memcpy(acts, g->act, sizeof acts[0] * (size_t)n);
    wt_new(g, g->set, g->players, g->seed);
    for (int i = 0; i < n; i++) wt_play(g, &acts[i]);
    return 1;
}

void wt_arrange(wpos *p, int pl, const uint8_t perm[WT_RACK])
{
    uint8_t r[WT_RACK];
    for (int i = 0; i < WT_RACK; i++) r[i] = p->rack[pl][perm[i] % WT_RACK];
    memcpy(p->rack[pl], r, sizeof r);
}

int wt_winner(const wpos *p)
{
    if (p->phase != WP_OVER) return -1;
    unsigned w = wt_winners(p);
    if (w & (w - 1)) return -1;
    for (int q = 0; q < p->players; q++)
        if (w >> q & 1) return q;
    return -1;
}

unsigned wt_winners(const wpos *p)
{
    if (p->phase != WP_OVER) return 0;
    int best = -100000;
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) best = p->score[q] > best ? p->score[q] : best;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->score[q] == best) << q;
    return m;
}

/* ---- a random legal action: tiles in a line from a random cell, else an exchange or a pass --------------------- */
static uint32_t rnd(uint32_t *s, uint32_t n) { return cards_rand(s) % (n ? n : 1); }

int wt_auto(const wpos *p, uint32_t *rng, wact *a)
{
    memset(a, 0, sizeof *a);
    if (p->phase == WP_OVER) return 0;
    if (p->phase == WP_FINAL || (wt_can_challenge(p) && rnd(rng, 12) == 0)) {
        a->kind = p->phase == WP_FINAL && rnd(rng, 3) ? WA_ACCEPT : WA_CHALLENGE;
        return 1;
    }
    int pl = p->turn, slots[WT_RACK], k = 0;
    for (int i = 0; i < WT_RACK; i++)
        if (p->rack[pl][i]) slots[k++] = i;
    for (int i = k - 1; i > 0; i--) {           /* a random order of the tiles */
        int j = (int)rnd(rng, (uint32_t)i + 1), t = slots[i];
        slots[i] = slots[j];
        slots[j] = t;
    }
    for (int tries = 0; tries < 400 && k; tries++) {
        int n = 1 + (int)rnd(rng, (uint32_t)k), down = (int)rnd(rng, 2), r = (int)rnd(rng, WT_N),
            c = (int)rnd(rng, WT_N), m = 0;
        if (tries < 40 && rnd(rng, 4)) n = n > 3 ? 1 + (int)rnd(rng, 3) : n;   /* mostly short plays */
        if (!p->board[WT_CENTRE]) {             /* the first play: through the centre, two tiles or more */
            n = n < 2 ? (k < 2 ? k : 2) : n;
            r = down ? 7 - (int)rnd(rng, (uint32_t)n) : 7;
            c = down ? 7 : 7 - (int)rnd(rng, (uint32_t)n);
        }
        a->kind = WA_PLAY;
        while (m < n && r < WT_N && c < WT_N) {
            if (!p->board[r * WT_N + c]) {
                a->slot[m] = (uint8_t)slots[m];
                a->cell[m] = (uint8_t)(r * WT_N + c);
                a->letter[m] = (uint8_t)(1 + rnd(rng, (uint32_t)wt_set(p->set)->n));
                m++;
            }
            if (down) r++;
            else c++;
        }
        a->n = (uint8_t)m;
        if (m == n && wt_eval(p, a, NULL) >= 0) return 1;
    }
    memset(a, 0, sizeof *a);
    a->kind = WA_EXCHANGE;
    if (wt_can_exchange(p) && rnd(rng, 2))
        for (int i = 0; i < k; i++)
            if (rnd(rng, 2)) a->slot[a->n++] = (uint8_t)slots[i];
    return 1;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static uint32_t utf8(const char **s)
{
    const unsigned char *u = (const unsigned char *)*s;
    uint32_t c = *u;
    int n = c < 0x80 ? 0 : c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
    if (n) c &= 0x3f >> n;
    (*s)++;
    for (int i = 0; i < n && **s; i++) c = c << 6 | (*(const unsigned char *)(*s)++ & 0x3f);
    return c;
}

static int letter_of(const wpos *p, uint32_t cp)
{
    const wset *s = wt_set(p->set);
    if (cp >= 'a' && cp <= 'z') cp -= 32;
    for (int i = 0; i < s->n; i++)
        if (s->l[i].cp == cp) return i + 1;
    return 0;
}

/* the first slot holding tile t not used yet */
static int find_slot(const wpos *p, int t, unsigned *used)
{
    for (int i = 0; i < WT_RACK; i++)
        if (!(*used >> i & 1) && p->rack[p->turn][i] == t) { *used |= 1u << i; return i; }
    return -1;
}

static int token(wgame *g, const char *s, const char *end)
{
    const wpos *p = &g->pos;
    wact a;
    memset(&a, 0, sizeof a);
    unsigned used = 0;
    if (end - s == 1 && (*s == 'c' || *s == 'a' || *s == '-')) {
        a.kind = *s == 'c' ? WA_CHALLENGE : *s == 'a' ? WA_ACCEPT : WA_EXCHANGE;
        return wt_play(g, &a);
    }
    if (*s == '#') {
        uint32_t rng = g->seed ^ (uint32_t)(g->n * 2654435761u) ^ 0x5bd1e995u;
        if (p->phase == WP_OVER) return 1;      /* the game over: nothing more */
        return wt_auto(p, &rng, &a) && wt_play(g, &a);
    }
    if (*s == 'x') {                            /* an exchange */
        a.kind = WA_EXCHANGE;
        for (s++; s < end && a.n < WT_RACK;) {
            uint32_t cp = utf8(&s);
            int t = cp == '?' ? WT_BLANK : letter_of(p, cp), sl = t ? find_slot(p, t, &used) : -1;
            if (sl < 0) return 0;
            a.slot[a.n++] = (uint8_t)sl;
        }
        return wt_play(g, &a);
    }
    if (*s < 'a' || *s > 'o') return 0;        /* a play: h8>WORD */
    int c = *s++ - 'a', r = 0;
    while (s < end && *s >= '0' && *s <= '9') r = r * 10 + (*s++ - '0');
    if (r < 1 || r > WT_N || s >= end || (*s != '>' && *s != 'v')) return 0;
    int down = *s++ == 'v';
    r--;
    a.kind = WA_PLAY;
    while (s < end) {
        if (r >= WT_N || c >= WT_N) return 0;
        int blank = *s == '?';
        if (blank) s++;
        if (s >= end) return 0;
        int l = letter_of(p, utf8(&s)), cell = r * WT_N + c;
        if (!l) return 0;
        if (p->board[cell]) {                   /* a tile already there: it must be that letter */
            if (WT_LETTER(p->board[cell]) != l) return 0;
        } else {
            int sl = find_slot(p, blank ? WT_BLANK : l, &used);
            if (sl < 0 || a.n >= WT_RACK) return 0;
            a.slot[a.n] = (uint8_t)sl;
            a.cell[a.n] = (uint8_t)cell;
            a.letter[a.n] = (uint8_t)(blank ? l : 0);
            a.n++;
        }
        if (down) r++;
        else c++;
    }
    return wt_play(g, &a);
}

int wt_script(wgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != ' ') e++;
        if (!token(g, s, e)) return 0;
        s = e;
    }
    return 1;
}
