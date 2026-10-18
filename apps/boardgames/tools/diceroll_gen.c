/*
 * Dice Roll's levels: src/games/diceroll_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of board (its size, its special tiles: plain tiles, number tiles, larger boards of them, odd and
 * even tiles, arrows, all of them) and makes many with the rules' maker (dcr_make, from fixed seeds: a symmetric board,
 * a mirror or a turn, its holes and special tiles laid in symmetric sets, changed while the fewest rolls do not drop).
 * A board is kept when the chapter's new tiles matter (made plain, the way gets shorter) and when its layout was not
 * met before (turned or mirrored either). The difficulty is measured (measure()): the fewest rolls, the share of
 * mistaken rolls along the way, and how many naive players (rolling at random, mostly towards the goal) solve it.
 * Each chapter keeps 25 boards: one to three easy ones first (chapter 1 from the very easiest) that show its new
 * tiles, then boards no naive player solves, spread from the chapter's floor to its hardest; of the boards near
 * each place on the curve the one that looks best is kept (the board well filled, near square, few lone spurs, one
 * shortest way or few, another size than the level before); the chapter's last board, its showpiece, the best
 * looking of its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/dcrgen apps/boardgames/tools/diceroll_gen.c -lm && \
 *       /tmp/dcrgen > apps/boardgames/src/games/diceroll_levels.h
 * The levels' difficulty table: the same with -DDCR_TABLE, run with no output file.
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

/* a chapter's boards: size w x h (up to grow more each way), holes, special sets (numbers, odd/even, arrows), the
 * changes tried; the new tiles that must matter (a DCR_* kind, 0 none); lo..hi: the rolls; intro: its first levels,
 * easy ones that show its new tiles; floor: the least score (measure()) of the others */
typedef struct {
    int w, h, grow, holes, nums, parity, arrows, iters, from_easiest, need, lo, hi, intro;
    double floor;
} chapter;

static const chapter CH[DCR_CHAPTERS] = {
    {3, 3, 2, 2, 0, 0, 0, 120, 1, 0, 3, 14, 3, 9},           /* plain tiles */
    {4, 4, 1, 2, 2, 0, 0, 200, 0, DCR_NUM, 6, 18, 2, 15},     /* number tiles */
    {5, 5, 2, 3, 3, 0, 0, 400, 0, DCR_NUM, 10, 24, 1, 21},    /* larger boards of them */
    {5, 5, 2, 3, 1, 3, 0, 400, 0, DCR_ODD, 9, 26, 2, 23},     /* odd and even tiles */
    {5, 5, 2, 3, 1, 0, 3, 450, 0, DCR_ARROW, 10, 28, 2, 26},  /* arrows */
    {6, 6, 2, 4, 2, 2, 2, 600, 0, -1, 16, 34, 1, 33},         /* all of them */
};
#define NAIVE_MAX 0.05                          /* the most naive players a hard level may let win */


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

/* ---- how hard a board is, measured ------------------------------------------------------------------------------
 * trap: along a shortest way, the share of the legal rolls that are mistakes (they do not bring the goal a roll
 * nearer); first: the share of the first rolls that are mistakes; naive: the share of 300 naive players who solve it
 * within one and a half times its par (each roll at random, but three times in four one that brings the die no
 * farther from the goal tile, never straight back); detour: the rolls over the start's distance to the goal. */
typedef struct { int par, states, ways, detour; double trap, first, naive, score; } metric;

typedef struct {
    char text[DCR_TEXT];
    metric m;
    int w, h;
    double look;
} cand;

static int by_score(const void *a, const void *b)
{
    double d = ((const cand *)a)->m.score - ((const cand *)b)->m.score;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

static int dist_of(const dcr_map *m, const dcr_state *s) { return dcr_solve(m, s, NULL, NULL, NULL); }

static void measure(const dcr_map *m, metric *mt)
{
    dcr_state s = start_of(m);
    mt->par = dcr_solve(m, &s, NULL, &mt->states, &mt->ways);
    int gx = m->goal % m->w, gy = m->goal / m->w;
    mt->detour = mt->par - (abs(s.x - gx) + abs(s.y - gy));
    int legal = 0, bad = 0, legal0 = 0, bad0 = 0;
    dcr_state c = s;
    for (int left = mt->par; left > 0; left--) {
        int dir = -1;
        dcr_solve(m, &c, &dir, NULL, NULL);
        for (int k = 0; k < 4; k++) {
            dcr_state n;
            if (!dcr_try(m, &c, k, &n)) continue;
            int d = dist_of(m, &n), mis = d < 0 || d != left - 1;
            legal++;
            bad += mis;
            if (left == mt->par) { legal0++; bad0 += mis; }
        }
        dcr_try(m, &c, dir, &c);
    }
    mt->trap = legal ? (double)bad / legal : 0;
    mt->first = legal0 ? (double)bad0 / legal0 : 0;
    uint32_t r = pr_seed(77);
    int wins = 0;
    for (int t = 0; t < 300; t++) {
        dcr_state p = s;
        int back = -1;
        for (int k = 0; k < mt->par + mt->par / 2 + 1 && !dcr_solved(m, &p); k++) {
            int ok[4], nok = 0, near[4], nnear = 0, here = abs(p.x - gx) + abs(p.y - gy);
            for (int d = 0; d < 4; d++) {
                dcr_state n;
                if (d == back || !dcr_try(m, &p, d, &n)) continue;
                ok[nok++] = d;
                if (abs(n.x - gx) + abs(n.y - gy) <= here) near[nnear++] = d;
            }
            if (!nok) break;
            int d = nnear && pr_below(&r, 4) ? near[pr_below(&r, nnear)] : ok[pr_below(&r, nok)];
            dcr_try(m, &p, d, &p);
            back = (d + 2) & 3;
        }
        wins += dcr_solved(m, &p);
    }
    mt->naive = wins / 300.0;
    /* the score: the rolls, worth more where the mistakes are many and the naive players fail */
    mt->score = mt->par * (0.6 + mt->trap) * (1.2 - mt->naive) + 0.15 * mt->detour - 0.4 * log((double)mt->ways);
}

#ifdef DCR_TABLE                                /* the shipped levels' table: -DDCR_TABLE */
#include "games/diceroll_levels.h"
static void table_row(int lv, const char *text, const metric *mt)
{
    printf("%4d %3d %5d %5d %4d %5.2f %5.2f %5.2f %6.2f  %s\n", lv + 1, mt->par, mt->states, mt->ways, mt->detour,
           mt->trap, mt->first, mt->naive, mt->score, text);
}

int main(void)
{
    (void)CH, (void)by_score, (void)look_of, (void)met, (void)matters;
    double sum[DCR_CHAPTERS][5] = {{0}};
    int trivial = 0;
    printf("lvl par states ways detour trap first naive score  board\n");
    for (int lv = 0; lv < DCR_LEVELS; lv++) {
        dcr_map m;
        metric mt;
        dcr_parse(&m, dcr_levels[lv]);
        measure(&m, &mt);
        table_row(lv, dcr_levels[lv], &mt);
        double *s = sum[lv / PER];
        s[0] += mt.par; s[1] += mt.trap; s[2] += mt.naive; s[3] += mt.score; s[4] += mt.naive > 0.1 || mt.par <= 6;
        trivial += lv >= 3 && (mt.naive > 0.1 || mt.par <= 6);
    }
    printf("\nchapter  par  trap naive score trivial\n");
    for (int c = 0; c < DCR_CHAPTERS; c++)
        printf("%7d %5.1f %5.2f %5.2f %5.1f %4.0f\n", c + 1, sum[c][0] / PER, sum[c][1] / PER, sum[c][2] / PER,
               sum[c][3] / PER, sum[c][4]);
    printf("trivial levels from level 4 (naive > 0.1 or par <= 6): %d\n", trivial);
    return 0;
}
#else
/* a chapter's levels from its pool (sorted by score): its first `intro` from the easy end (the chapter's new tiles
 * shown plainly), the others spread from `floor` to the hardest over the boards no naive player solves, the last of
 * the hardest few; of the boards near each place on the curve the best looking, another size than the last level's
 * when it can */
static int pick(cand *pool, int n, const chapter *C, int *chosen)
{
    static int hard[MAKES];
    int nh = 0, used[MAKES] = {0};
    for (int i = 0; i < n; i++)
        if (pool[i].m.naive <= NAIVE_MAX && pool[i].m.score >= C->floor) hard[nh++] = i;
    if (nh < PER - C->intro + 2) { fprintf(stderr, "only %d hard boards\n", nh); return 0; }
    int lw = 0, lh = 0, last = -1;
    for (int k = 0; k < C->intro; k++) {
        int span = n / 5 > C->intro ? n / 5 : C->intro, i = -1;
        int a = k * span / C->intro, b = (k + 1) * span / C->intro - 1;
        if (C->from_easiest && k == 0) b = a + 2;
        if (a <= last) a = last + 1;
        if (b < a) b = a;
        for (int j = a; j <= b && j < n; j++)       /* short ones: a long easy way is a dull one */
            if (i < 0 || (pool[j].m.par <= C->lo + 6 &&
                          (pool[i].m.par > C->lo + 6 || pool[j].look > pool[i].look))) i = j;
        used[i] = 1;
        last = i;
        chosen[k] = i;
        lw = pool[i].w, lh = pool[i].h;
    }
    double step = (double)(nh - 1) / (PER - C->intro - 1);
    last = -1;
    for (int k = 0; k < PER - C->intro; k++) {
        int mid = (int)(k * step + 0.5), a = mid - (int)(step / 2), b = mid + (int)(step / 2), i = -1;
        if (k == PER - C->intro - 1) a = nh - 1 - nh / 25 - 2, b = nh - 1;   /* the showpiece: of the hardest few */
        if (a <= last) a = last + 1;
        if (b > nh - 1) b = nh - 1;
        if (b < a) b = a;
        double best = -1e9;
        for (int j = a; j <= b; j++) {
            const cand *c = &pool[hard[j]];
            double v = c->look - (c->w == lw && c->h == lh ? 0.15 : 0);
            if (!used[hard[j]] && (i < 0 || v > best)) { best = v; i = j; }
        }
        if (i < 0) i = a;
        last = i;
        used[hard[i]] = 1;
        chosen[C->intro + k] = hard[i];
        lw = pool[hard[i]].w, lh = pool[hard[i]].h;
    }
    return 1;
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
            measure(&m, &pool[n].m);
            pool[n].w = m.w;
            pool[n].h = m.h;
            pool[n].look = look_of(&m, ways);
            n++;
        }
        qsort(pool, (size_t)n, sizeof *pool, by_score);
        int chosen[PER];
        if (n < PER || !pick(pool, n, C, chosen)) {
            fprintf(stderr, "chapter %d: only %d boards\n", ch + 1, n);
            return 1;
        }
        for (int k = 0; k < PER; k++) {
            strcpy(out[ch * PER + k], pool[chosen[k]].text);
            outpar[ch * PER + k] = pool[chosen[k]].m.par;
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
#endif
