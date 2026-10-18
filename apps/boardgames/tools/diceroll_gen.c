/*
 * Dice Roll's levels: src/games/diceroll_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of board (its size, its special tiles: plain tiles, number tiles, larger boards of them, odd and
 * even tiles, arrows, all of them) and makes many with the rules' maker (dcr_make, from fixed seeds: a symmetric board,
 * a mirror or a turn, its holes and special tiles laid in symmetric sets, changed while the fewest rolls do not drop).
 * A board is kept when the chapter's new tiles matter (made plain, the way gets shorter) and when its layout was not
 * met before (turned or mirrored either). The difficulty: the fewest rolls, a little more for the states the die can
 * reach, a little less when many shortest ways exist. Each chapter keeps 25 boards spread over its difficulties, from
 * an easy one (chapter 1 from the very easiest) to its hardest; of the boards near each place on the curve the one
 * that looks best is kept (the board well filled, near square, few lone spurs, one shortest way or few); the chapter's
 * last board, its showpiece, the best looking of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/dcrgen apps/boardgames/tools/diceroll_gen.c -lm && \
 *       /tmp/dcrgen > apps/boardgames/src/games/diceroll_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define DCR_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/diceroll_rules.c"

#define PER (DCR_LEVELS / DCR_CHAPTERS)
#define MAKES 700                               /* boards kept a chapter, at most */

typedef struct {
    char text[DCR_TEXT];
    int par, states, ways;
    double diff, look;
} cand;

/* a chapter's boards: size w x h (up to grow more each way), holes, special sets (numbers, odd/even, arrows), the
 * changes tried; the new tiles that must matter (a DCR_* kind, 0 none); lo..hi: the rolls */
typedef struct { int w, h, grow, holes, nums, parity, arrows, iters, from_easiest, need, lo, hi; } chapter;

static const chapter CH[DCR_CHAPTERS] = {
    {3, 3, 2, 2, 0, 0, 0, 60, 1, 0, 3, 11},          /* plain tiles */
    {4, 4, 1, 2, 2, 0, 0, 200, 0, DCR_NUM, 6, 14},   /* number tiles */
    {5, 5, 2, 3, 3, 0, 0, 400, 0, DCR_NUM, 9, 20},   /* larger boards of them */
    {5, 5, 2, 3, 1, 3, 0, 400, 0, DCR_ODD, 9, 22},   /* odd and even tiles */
    {5, 5, 2, 3, 1, 0, 3, 450, 0, DCR_ARROW, 10, 24},/* arrows */
    {6, 6, 2, 4, 2, 2, 2, 600, 0, -1, 13, 30},       /* all of them */
};

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

/* ---- layouts met before: the board's cells (not the die's faces), in each of its 8 turns and mirrors ---------- */
static char (*kept)[DCR_TEXT];
static int nkept;

static void layout_of(const dcr_map *m, int tr, char *out)
{
    int w = tr & 4 ? m->h : m->w, h = tr & 4 ? m->w : m->h, n = 0;
    char t[DCR_TEXT];
    dcr_text(m, t);
    const char *rows = t + 4;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx = tr & 4 ? y : x, sy = tr & 4 ? x : y;
            if (tr & 1) sx = m->w - 1 - sx;
            if (tr & 2) sy = m->h - 1 - sy;
            char c = rows[sx + sy * (m->w + 1)];
            out[n++] = strchr("^>v<", c) ? 'a' : c;   /* arrows alike whichever way */
        }
        out[n++] = '|';
    }
    out[n] = 0;
}

static int met(const dcr_map *m)
{
    char l[8][DCR_TEXT];
    for (int tr = 0; tr < 8; tr++) layout_of(m, tr, l[tr]);
    for (int i = 0; i < nkept; i++)
        for (int tr = 0; tr < 8; tr++)
            if (!strcmp(kept[i], l[tr])) return 1;
    strcpy(kept[nkept++], l[0]);
    return 0;
}

/* the chapter's new tiles matter: made plain, the fewest rolls drop. kind -1: at least two kinds of special tile */
static int matters(const dcr_map *m, int kind, int par)
{
    if (kind == 0) {
        for (int c = 0; c < m->w * m->h; c++)
            if (is_special(m->t[c])) return 0;
        return 1;
    }
    if (kind < 0) {
        int kinds = 0;
        for (int k = DCR_NUM; k <= DCR_ARROW; k++) {
            int has = 0;
            for (int c = 0; c < m->w * m->h; c++) has |= m->t[c] == k || (k == DCR_ODD && m->t[c] == DCR_EVEN);
            kinds += has && k != DCR_EVEN;
        }
        return kinds >= 2 && (matters(m, DCR_ARROW, par) || matters(m, DCR_ODD, par));
    }
    dcr_map p = *m;
    int n = 0;
    for (int c = 0; c < p.w * p.h; c++)
        if (p.t[c] == kind || (kind == DCR_ODD && p.t[c] == DCR_EVEN)) { p.t[c] = DCR_TILE; n++; }
    return n > 0 && par_of(&p) < par;
}

/* how hand-laid a board looks: well filled, near square, few spurs, few shortest ways */
static double look_of(const dcr_map *m, int ways)
{
    int tiles = 0, spurs = 0;
    for (int c = 0; c < m->w * m->h; c++) {
        if (m->t[c] == DCR_VOID) continue;
        tiles++;
        int x = c % m->w, y = c / m->w, near = 0;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            near += nx >= 0 && ny >= 0 && nx < m->w && ny < m->h && m->t[nx + ny * m->w] != DCR_VOID;
        }
        spurs += near <= 1 && c != m->start && c != m->goal;
    }
    double fill = (double)tiles / (m->w * m->h);
    return 0.8 * fill - 0.3 * fabs(fill - 0.75) - 1.5 * spurs / tiles - 0.4 * abs(m->w - m->h) / (m->w + m->h) -
           0.08 * log((double)ways);
}

int main(void)
{
    kept = malloc((size_t)DCR_CHAPTERS * MAKES * 4 * sizeof *kept);
    static cand pool[MAKES];
    static char out[DCR_LEVELS][DCR_TEXT];
    static int outpar[DCR_LEVELS];
    printf("/* Dice Roll's levels: made by tools/diceroll_gen.c (our own boards, found by its search; do not edit).\n"
           " * Each board: the goal's number, the die's top and south faces, ':', its rows from the top split by '|'\n"
           " * (diceroll.h); and its par: the fewest rolls. */\n");
    for (int ch = 0; ch < DCR_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(9100u + (uint32_t)ch);
        int n = 0;
        for (int k = 0; k < 40 * MAKES && n < MAKES; k++) {
            int g = pr_below(&r, C->grow + 1);
            dcr_spec sp = {C->w + g, C->h + g + (pr_below(&r, 3) == 0 && C->h + g < DCR_MAXH ? 1 : 0),
                           C->holes + g, C->nums, C->parity, C->arrows,
                           C->from_easiest ? 10 + pr_below(&r, C->iters) : C->iters};
            dcr_map m;
            dcr_make(&m, &sp, pr_next(&r));
            dcr_state s = start_of(&m);
            int states, ways, par = dcr_solve(&m, &s, NULL, &states, &ways);
            if (par < C->lo || par > C->hi || !matters(&m, C->need, par)) continue;
            if (m.w < 3 || m.h < 3 || met(&m)) continue;
            dcr_text(&m, pool[n].text);
            pool[n].par = par;
            pool[n].states = states;
            pool[n].ways = ways;
            pool[n].diff = par + states / 250.0 - 0.3 * log((double)ways);
            pool[n].look = look_of(&m, ways);
            n++;
        }
        if (n < PER) { fprintf(stderr, "chapter %d: only %d boards\n", ch + 1, n); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_diff);
        int lo = C->from_easiest ? 0 : n / 6;
        double step = (double)(n - 1 - lo) / (PER - 1);
        for (int k = 0, last = -1; k < PER; k++) {
            int mid = lo + (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
            if (k == 0 && C->from_easiest) a = b = 0;
            if (k == PER - 1) a = n - 1 - n / 25 - 2, b = n - 1;   /* the showpiece: of the hardest few */
            if (a < lo) a = lo;
            if (a <= last) a = last + 1;
            if (b > n - 1) b = n - 1;
            if (b < a) b = a;
            for (int j = a; j <= b; j++)
                if (i < 0 || pool[j].look > pool[i].look) i = j;
            last = i;
            strcpy(out[ch * PER + k], pool[i].text);
            outpar[ch * PER + k] = pool[i].par;
        }
        fprintf(stderr, "chapter %d: %d boards, rolls %d .. %d\n", ch + 1, n, outpar[ch * PER],
                outpar[ch * PER + PER - 1]);
    }
    printf("const char *const dcr_levels[DCR_LEVELS] = {\n");
    for (int l = 0; l < DCR_LEVELS; l++) printf("    \"%s\",\n", out[l]);
    printf("};\n\nconst uint8_t dcr_level_par[DCR_LEVELS] = {");
    for (int l = 0; l < DCR_LEVELS; l++) printf("%s%d,", l % 25 ? " " : "\n    ", outpar[l]);
    printf("\n};\n");
    return 0;
}
