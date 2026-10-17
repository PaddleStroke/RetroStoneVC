/*
 * Water Sort's levels (and Ball Sort's: tools/ballsort_gen.c is this file with SRT_ONE set): src/games/
 * watersort_levels.h. Our own levels: no published set is used. Six chapters, the colours growing: 3 and 4, 5 and 6, 7,
 * 8 and 9, 10, 11 and 12, each with two empty tubes; the very first level a warm-up of two colours.
 * Each level is drawn from a theme, so that a chapter is a designed mix and not twenty-five shuffles alike:
 *   - a shuffle: four layers of each colour into the tubes by chance (the rules' srt_fill);
 *   - rows: each layer across the rack holds every colour once (a tidy band of colours at every height);
 *   - rainbow: no tube holds a colour twice;
 *   - mirror: the rack reads the same from both ends (tube k and the k-th from the right alike);
 *   - the showpiece, each chapter's last level: rows and rainbow at once (a Latin rack), the hardest of its kind.
 * Every candidate: no tube complete, few layers on their own colour, its shortest way found by the rules' A*
 * (srt_solve) with plenty of room; one whose search could not finish is left out (its par would not be sure; with no
 * Latin rack left, a showpiece falls back to the hardest rows rack). Near-duplicates are left out too: the same tubes
 * in another order, or the same picture with the colours renamed. The difficulty: the
 * shortest way's length, more when the search had to look at many positions. A chapter's first half takes its smaller
 * colour count, its second half the larger; each half spreads its levels over that pool's difficulties (chapter 1
 * staying in the gentler part), sorted from the easiest, the showpiece last. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/wsgen apps/boardgames/tools/watersort_gen.c -lm && \
 *       /tmp/wsgen > apps/boardgames/src/games/watersort_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define SRT_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SRT_ONE
#define SRT_ONE 0
#define NAME "Water Sort"
#define FILE_ID "watersort"
#define PRE "ws"
#define UP "WS"
#define WHAT "pours"
#include "games/watersort_rules.c"
#endif

#define LEVELS 150
#define CHAPTERS 6
#define PER (LEVELS / CHAPTERS)
#define EACH 24                                 /* the candidates kept of each theme and colour count */
#define CAP 2500000                             /* the positions a candidate's search may make */

static const int COLOURS[CHAPTERS][2] = {{3, 4}, {5, 6}, {7, 7}, {8, 9}, {10, 10}, {11, 12}};

enum { T_SHUFFLE, T_ROWS, T_RAINBOW, T_MIRROR, T_LATIN, THEMES };
static const char *const THEME_NAME[THEMES] = {"shuffle", "rows", "rainbow", "mirror", "showpiece"};
/* the themes' turns within a chapter half (rainbow needs four colours: rows then) */
static const int TURN[6] = {T_SHUFFLE, T_ROWS, T_MIRROR, T_RAINBOW, T_SHUFFLE, T_MIRROR};

typedef struct {
    int colours, theme, par, looked, used;
    double diff;
    uint64_t key, sig;
    char text[SRT_TUBES * 5 + 1];
} cand;

static int by_diff(const void *a, const void *b)
{
    double d = (*(cand *const *)a)->diff - (*(cand *const *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

/* ---- the themes ------------------------------------------------------------------------------------------------- */
static int tidy(const srt_pos *p, int n)        /* no tube complete, few layers on their own colour */
{
    int same = 0;
    for (int t = 0; t < n; t++) {
        if (srt_complete(p, t)) return 0;
        for (int i = 1; i < SRT_CAP; i++) same += p->c[t][i] == p->c[t][i - 1];
    }
    return same <= n / 3;
}

static int distinct_tube(const srt_pos *p, int t, int upto)
{
    for (int i = 0; i < upto; i++)
        for (int j = 0; j < i; j++)
            if (p->c[t][i] == p->c[t][j]) return 0;
    return 1;
}

static int fill_theme(int theme, int n, uint32_t *s, srt_pos *p)
{
    int bag[SRT_COLOURS * SRT_CAP];
    for (int tries = 0; tries < 20000; tries++) {
        memset(p, 0, sizeof *p);
        for (int t = 0; t < n; t++) p->h[t] = SRT_CAP;
        if (theme == T_SHUFFLE) {
            srt_fill(n, s, p);
            return 1;
        } else if (theme == T_ROWS || theme == T_LATIN) {
            int ok = 1;
            for (int i = 0; i < SRT_CAP && ok; i++) {
                int got = 0;
                for (int k = 0; k < 400 && !got; k++) {
                    for (int c = 0; c < n; c++) bag[c] = c + 1;
                    pr_shuffle(s, bag, n);
                    got = 1;
                    for (int t = 0; t < n; t++) p->c[t][i] = (uint8_t)bag[t];
                    if (theme == T_LATIN)
                        for (int t = 0; t < n && got; t++) got = distinct_tube(p, t, i + 1);
                }
                ok = got;
            }
            if (!ok) continue;
        } else if (theme == T_RAINBOW) {
            for (int i = 0; i < n * SRT_CAP; i++) bag[i] = 1 + i / SRT_CAP;
            pr_shuffle(s, bag, n * SRT_CAP);
            int ok = 1;
            for (int t = 0; t < n && ok; t++) {
                for (int i = 0; i < SRT_CAP; i++) p->c[t][i] = (uint8_t)bag[t * SRT_CAP + i];
                ok = distinct_tube(p, t, SRT_CAP);
            }
            if (!ok) continue;
        } else {                                /* mirror: pairs of tubes alike, a middle tube of two colours twice */
            int half = n / 2, k = 0;
            for (int c = 0; c < n; c++) bag[2 * c] = bag[2 * c + 1] = c + 1;
            pr_shuffle(s, bag, 2 * n);
            for (int t = 0; t < half; t++)
                for (int i = 0; i < SRT_CAP; i++) p->c[t][i] = p->c[n - 1 - t][i] = (uint8_t)bag[k++];
            if (n & 1) {
                int a = bag[k], b = bag[k + 1], m = half;
                if (a == b) continue;
                p->c[m][0] = p->c[m][2] = (uint8_t)a;
                p->c[m][1] = p->c[m][3] = (uint8_t)b;
            }
        }
        if (tidy(p, n)) return 1;
    }
    return 0;
}

/* a signature blind to the tubes' order and to the colours' names: each tube's pattern (which layers match), each
 * colour's heights and the patterns of the tubes it lies in */
static uint64_t mix(uint64_t h, uint64_t v) { return (h ^ v) * 1099511628211ull; }
static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

static uint64_t signature(const srt_pos *p, int n)
{
    uint64_t pat[SRT_TUBES], col[SRT_COLOURS + 1][SRT_CAP], cs[SRT_COLOURS], ts[SRT_TUBES];
    int nk[SRT_COLOURS + 1] = {0};
    for (int t = 0; t < n; t++) {
        int code = 0;
        for (int i = 0; i < SRT_CAP; i++) {
            int j = 0;
            while (p->c[t][j] != p->c[t][i]) j++;
            code = code * 4 + j;
        }
        pat[t] = (uint64_t)code;
        ts[t] = pat[t];
    }
    for (int t = 0; t < n; t++)
        for (int i = 0; i < SRT_CAP; i++) {
            int c = p->c[t][i];
            col[c][nk[c]++] = pat[t] * 8 + (uint64_t)i;
        }
    for (int c = 1; c <= n; c++) {
        qsort(col[c], SRT_CAP, sizeof(uint64_t), cmp_u64);
        uint64_t h = 1469598103934665603ull;
        for (int i = 0; i < SRT_CAP; i++) h = mix(h, col[c][i] + 1);
        cs[c - 1] = h;
    }
    qsort(ts, (size_t)n, sizeof *ts, cmp_u64);
    qsort(cs, (size_t)n, sizeof *cs, cmp_u64);
    uint64_t h = 1469598103934665603ull;
    for (int t = 0; t < n; t++) h = mix(h, ts[t] + 7);
    for (int c = 0; c < n; c++) h = mix(h, cs[c]);
    return h ? h : 1;
}

/* ---- the candidates --------------------------------------------------------------------------------------------- */
#define MAXC 8000
static cand all[MAXC];
static int nall;

static int seen(uint64_t key, uint64_t sig)
{
    for (int i = 0; i < nall; i++)
        if (all[i].key == key || all[i].sig == sig) return 1;
    return 0;
}

static void draw(int theme, int colours, int want, uint32_t seed)
{
    static srt_move way[SRT_PATH];
    uint32_t s = pr_seed(seed);
    int got = 0;
    for (int tries = 0; tries < want * 20 && got < want && nall < MAXC; tries++) {
        srt_pos p;
        if (!fill_theme(theme, colours, &s, &p)) break;
        int tubes = colours + 2;
        uint16_t codes[SRT_TUBES];
        for (int t = 0; t < tubes; t++) codes[t] = srt_code(&p, t);
        cand c;
        memset(&c, 0, sizeof c);
        c.key = srt_key(codes, tubes);
        c.sig = signature(&p, colours);
        if (seen(c.key, c.sig)) continue;
        int full;
        c.par = srt_solve(&p, tubes, SRT_ONE, CAP, 4, way, &c.looked, &full);
        if (c.par <= 0) continue;               /* no way, or not sure to be the shortest */
        c.colours = colours;
        c.theme = theme;
        c.diff = c.par + 1.5 * log2(1.0 + c.looked);
        srt_text(&p, tubes, c.text);
        all[nall++] = c;
        got++;
    }
}

/* the unused candidate of a theme (-1: any) and colour count whose difficulty is nearest to d */
static cand *nearest(int theme, int colours, double d)
{
    cand *best = NULL;
    for (int i = 0; i < nall; i++) {
        cand *c = &all[i];
        if (c->used || (theme >= 0 && c->theme != theme) || c->colours != colours) continue;
        if (!best || fabs(c->diff - d) < fabs(best->diff - d)) best = c;
    }
    return best;
}

/* the difficulty at quantile q of a colour count's candidates */
static double quantile(int colours, double q)
{
    static double d[MAXC];
    int n = 0;
    for (int i = 0; i < nall; i++)
        if (all[i].colours == colours && all[i].theme != T_LATIN) d[n++] = all[i].diff;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && d[j - 1] > d[j]; j--) {
            double t = d[j];
            d[j] = d[j - 1], d[j - 1] = t;
        }
    return n ? d[(int)(q * (n - 1) + 0.5)] : 0;
}

int main(void)
{
    static cand *out[LEVELS];
    int nout = 0;
    for (int ch = 0; ch < CHAPTERS; ch++) {
        for (int k = 0; k < 2; k++) {
            int n = COLOURS[ch][k];
            if (k && n == COLOURS[ch][0]) break;
            for (int th = 0; th < THEMES; th++) {
                if ((th == T_RAINBOW || th == T_LATIN) && n < 4) continue;
                draw(th, n, th == T_LATIN ? EACH / 2 : EACH,
                     7000u + 100u * SRT_ONE + 13u * (uint32_t)n + 1000u * (uint32_t)th);
            }
        }
        if (ch == 0) draw(T_SHUFFLE, 2, 2, 7999u + SRT_ONE);
        int lo = COLOURS[ch][0], hi = COLOURS[ch][1], first = nout, show = hi < 4 ? 4 : hi;
        int slots = PER - 1, split = lo == hi ? slots : slots / 2;
        if (ch == 0) {                          /* the warm-up: two colours, the smallest way */
            cand *w = nearest(T_SHUFFLE, 2, 0);
            w->used = 1;
            out[nout++] = w;
            slots--;
        }
        for (int i = 0; i < slots; i++) {
            int high = i >= split, n = high ? hi : lo;
            int a = high ? split : 0, b = high ? slots : split, m = b - a, j = i - a;
            double qlo = ch == 0 ? (high ? 0.1 : 0.0) : (high ? 0.25 : 0.03);
            double qhi = ch == 0 ? (high ? 0.7 : 0.55) : (high ? 0.95 : 0.75);
            double q = m > 1 ? qlo + (qhi - qlo) * j / (m - 1) : qlo;
            int th = TURN[i % 6];
            if (th == T_RAINBOW && n < 4) th = T_ROWS;
            cand *c = nearest(th, n, quantile(n, q));
            if (!c) c = nearest(-1, n, quantile(n, q));
            c->used = 1;
            out[nout++] = c;
        }
        /* each half from the easiest */
        int start = first + (ch == 0), mid = first + (ch == 0) + split;
        if (mid > nout) mid = nout;
        qsort(out + start, (size_t)(mid - start), sizeof *out, by_diff);
        qsort(out + mid, (size_t)(nout - mid), sizeof *out, by_diff);
        /* the showpiece: a Latin rack, the hardest of its kind (chapter 1: of four colours, the easiest) */
        cand *sp = NULL;
        for (int i = 0; i < nall; i++) {
            cand *c = &all[i];
            if (c->used || c->theme != T_LATIN || c->colours != show) continue;
            if (!sp || (ch == 0 ? c->diff < sp->diff : c->diff > sp->diff)) sp = c;
        }
        if (!sp) sp = nearest(T_ROWS, hi, 1e9);  /* no Latin rack whose search finished (large Ball Sort ones) */
        if (!sp) sp = nearest(-1, hi, 1e9);
        sp->used = 1;
        out[nout++] = sp;
        fprintf(stderr, "chapter %d: %d .. %d " WHAT "\n", ch + 1, out[first]->par, out[nout - 1]->par);
    }
    printf("/*\n"
           " * " NAME "'s levels: our own, made by tools/" FILE_ID "_gen.c (each from a theme: a shuffle, rows of every\n"
           " * colour, rainbow tubes, a mirrored rack, a Latin rack to end each chapter; the shortest way found by the\n"
           " * rules' search; six chapters of 25 from the easiest). Each: the tubes from the left ('|' between them), each\n"
           " * one's colours from the bottom (1..9 a b c); in the comment the colours, the theme, the shortest way and the\n"
           " * positions the search made; the shortest ways again in " PRE "_level_par (the tests check them).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const " PRE "_levels[" UP "_LEVELS] = {\n");
    for (int i = 0; i < nout; i++)
        printf("    \"%s\", /* %d: %d colours, %s, %d " WHAT ", %d looked at */\n", out[i]->text, i + 1,
               out[i]->colours, THEME_NAME[out[i]->theme], out[i]->par, out[i]->looked);
    printf("};\n\nconst uint8_t " PRE "_level_par[" UP "_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 25 ? " " : "\n    ", out[i]->par);
    printf("\n};\n");
    return nout == LEVELS ? 0 : 1;
}
