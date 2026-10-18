/*
 * Twin Walkers' levels: src/games/twinwalk_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its kind of map (its size, its special tiles: plain walls, larger floors, crumbling tiles, plates and
 * doors, ice, all of them) and makes many with the rules' maker (twk_make, from fixed seeds: walls laid with a
 * symmetry, then changed a pair at a time while the fewest moves grow; each one aimed at a par spread over the
 * chapter's range). A map is kept when its special tiles all matter (twk_make checks it) and when it was not met
 * before (nor its mirror). The look, for a hand-laid feel: the walls in a mirror or half-turn symmetry (a few maps
 * left free), floor no walker can ever reach walled up (a whole symmetric pair at a time), no map with much floor
 * wasted, walls neither too few nor too many, the walkers' starts and exits spread over the floor. The difficulty is
 * measured (measure()): the fewest moves, the share of mistaken moves along the way, and how many naive players
 * (moving at random, mostly bringing the walkers nearer their exits) bring both out. Each chapter keeps 25: up to
 * three easy ones first (chapter 1 from the very easiest) that show its new tiles, then maps no naive player solves,
 * spread from the chapter's floor to its hardest; of the maps near each place on the curve the best-looking one is
 * kept (another size than the level before when it can), the chapter's last map (its showpiece) the best-looking of
 * its hardest few. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/twkgen apps/boardgames/tools/twinwalk_gen.c -lm && \
 *       /tmp/twkgen > apps/boardgames/src/games/twinwalk_levels.h
 * The levels' difficulty table: the same with -DTWK_TABLE, run with no output file.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define TWK_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/twinwalk_rules.c"

#define PER (TWK_LEVELS / TWK_CHAPTERS)
#define TEXT (TWK_CELLS + TWK_MAXH + 1)

typedef struct { int par, states; double trap, first, naive, score; } metric;   /* measure() */

typedef struct {
    char text[TEXT];
    int par, states, w, h;
    double diff, look;
    metric m;
} cand;

/* a chapter: its sizes (w x h, up to grow more each way), walls, special tiles; lo..hi: the pars aimed at; makes:
 * the maps made; intro: its first levels, easy ones that show its new tiles (chapter 1 from the very easiest);
 * floor: the least score (measure()) of the others */
typedef struct {
    int w, h, grow, walls, crumble, plates, ice, lo, hi, iters, makes, intro, from_easiest;
    double floor;
} chapter;

static const chapter CH[TWK_CHAPTERS] = {
    {4, 4, 2, 20, 0, 0, 0, 2, 13, 120, 700, 3, 1, 9},       /* plain walls, small */
    {6, 6, 2, 22, 0, 0, 0, 10, 19, 220, 500, 0, 0, 15},     /* plain walls, larger floors, longer ways */
    {6, 6, 1, 18, 4, 0, 0, 9, 20, 240, 560, 2, 0, 17},      /* crumbling tiles */
    {6, 7, 1, 18, 0, 1, 0, 10, 22, 260, 640, 2, 0, 19},     /* plates and doors */
    {6, 7, 1, 14, 0, 0, 6, 10, 22, 260, 560, 2, 0, 20},     /* ice */
    {7, 8, 1, 16, 3, 2, 4, 15, 28, 300, 600, 1, 0, 26},     /* all of them */
};
#define NAIVE_MAX 0.05                          /* the most naive players a hard level may let win */
#define MAXC 1200                               /* the maps kept a chapter, at most */

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->text, ((const cand *)b)->text);
}

static char (*kept)[TEXT];
static int nkept;

static void mirror_text(const char *t, char *mir)   /* each row read from the right, the walkers' roles kept */
{
    strcpy(mir, t);
    for (char *row = mir; *row;) {
        char *end = strchr(row, '|');
        int n = end ? (int)(end - row) : (int)strlen(row);
        for (int i = 0; i < n / 2; i++) { char c = row[i]; row[i] = row[n - 1 - i]; row[n - 1 - i] = c; }
        row += n + (end ? 1 : 0);
        if (!end) break;
    }
}

static int met(const char *t)                   /* a text met before; its mirror too */
{
    char mir[TEXT];
    mirror_text(t, mir);
    for (int i = 0; i < nkept; i++)
        if (!strcmp(kept[i], t) || !strcmp(kept[i], mir)) return 1;
    strcpy(kept[nkept++], t);
    return 0;
}

/* floor no walker reaches walled up, a symmetric pair at a time (when both are out of reach): kept if the par stays */
static void wall_unseen(twk_map *m, int par)
{
    int cells = m->w * m->h;
    int16_t *far = malloc(sizeof *far * (size_t)cells * cells);
    uint8_t seen[TWK_CELLS];
    twk_state s = {{(uint8_t)m->start[0], (uint8_t)m->start[1]}, 0};
    if (!far) return;
    if (search(m, &s, NULL, NULL, far, seen) >= 0) {
        twk_map t = *m;
        for (int sym = 1; sym <= 3; sym++) {    /* the map's own symmetry, the first that fits its walls */
            int ok = 1;
            for (int c = 0; c < cells && ok; c++) {
                int o[2], no = orbit(m->w, m->h, sym, c, o);
                if (no == 2 && (m->t[o[0]] == TWK_WALL) != (m->t[o[1]] == TWK_WALL)) ok = 0;
            }
            if (!ok && sym < 3) continue;
            for (int c = 0; c < cells; c++) {
                int o[2], no = orbit(m->w, m->h, ok ? sym : 0, c, o), all = 1;
                for (int i = 0; i < no; i++)
                    if (seen[o[i]] || m->t[o[i]] == TWK_WALL || o[i] == m->exit[0] || o[i] == m->exit[1]) all = 0;
                if (all)
                    for (int i = 0; i < no; i++) { t.t[o[i]] = TWK_WALL; t.grp[o[i]] = 0; }
            }
            break;
        }
        number_crumbles(&t);
        if (par_of(&t) == par) *m = t;
    }
    free(far);
}

/* the rows and columns of wall alone round the map cut away */
static void crop(twk_map *m)
{
    int x0 = m->w, y0 = m->h, x1 = -1, y1 = -1;
    for (int c = 0; c < m->w * m->h; c++)
        if (m->t[c] != TWK_WALL) {
            int x = c % m->w, y = c / m->w;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    if (x1 < 0) return;
    twk_map t = *m;
    t.w = x1 - x0 + 1;
    t.h = y1 - y0 + 1;
    memset(t.t, 0, sizeof t.t);
    memset(t.grp, 0, sizeof t.grp);
    for (int y = 0; y < t.h; y++)
        for (int x = 0; x < t.w; x++) {
            t.t[x + y * t.w] = m->t[x + x0 + (y + y0) * m->w];
            t.grp[x + y * t.w] = m->grp[x + x0 + (y + y0) * m->w];
        }
    for (int i = 0; i < 2; i++) {
        t.start[i] = m->start[i] % m->w - x0 + (m->start[i] / m->w - y0) * t.w;
        t.exit[i] = m->exit[i] % m->w - x0 + (m->exit[i] / m->w - y0) * t.w;
    }
    number_crumbles(&t);
    *m = t;
}

/* how good a map looks:its walls' symmetry (mirror or half turn), a fair share of walls, little floor out of reach,
 * the starts and the exits far apart */
static double look_of(const twk_map *m)
{
    int cells = m->w * m->h, best = 0, walls = 0;
    for (int sym = 1; sym <= 3; sym++) {
        int same = 0;
        for (int c = 0; c < cells; c++) {
            int o[2], no = orbit(m->w, m->h, sym, c, o);
            if ((m->t[c] == TWK_WALL) == (m->t[o[no - 1]] == TWK_WALL)) same++;
        }
        if (same > best) best = same;
    }
    for (int c = 0; c < cells; c++) walls += m->t[c] == TWK_WALL;
    double sym = (double)best / cells, wf = (double)walls / cells;
    double look = 4 * sym - 6 * fabs(wf - 0.28);
    if (wf > 0.45) look -= 3;
    int open = 0;                               /* wide open squares of floor: a bare look */
    for (int y = 0; y + 1 < m->h; y++)
        for (int x = 0; x + 1 < m->w; x++) {
            int c = x + y * m->w;
            open += m->t[c] != TWK_WALL && m->t[c + 1] != TWK_WALL && m->t[c + m->w] != TWK_WALL &&
                    m->t[c + m->w + 1] != TWK_WALL;
        }
    look -= 3.0 * open / cells;
    int16_t *far = malloc(sizeof *far * (size_t)cells * cells);
    uint8_t seen[TWK_CELLS];
    twk_state s = {{(uint8_t)m->start[0], (uint8_t)m->start[1]}, 0};
    if (far && search(m, &s, NULL, NULL, far, seen) >= 0) {
        int unseen = 0;
        for (int c = 0; c < cells; c++) unseen += m->t[c] != TWK_WALL && !seen[c];
        look -= 8.0 * unseen / cells;
    }
    free(far);
    int w = m->w;                               /* the starts and the exits spread: not all huddled */
    double spread = 0;
    int pts[4] = {m->start[0], m->start[1], m->exit[0], m->exit[1]};
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            spread += abs(pts[i] % w - pts[j] % w) + abs(pts[i] / w - pts[j] / w);
    look += 0.08 * spread / (m->w + m->h);
    return look;
}

/* ---- how hard a map is, measured --------------------------------------------------------------------------------
 * trap: along a shortest way, the share of the moves that are mistakes (they do not bring the exits a move nearer,
 * or lose them); first: the same for the first move; naive: the share of 300 naive players who bring both walkers out
 * within one and a half times its par (each move at random, but three times in four one that brings the walkers no
 * farther from their exits, counted together) */
static int gap(const twk_map *m, const twk_state *s)
{
    int d = 0;
    for (int i = 0; i < 2; i++)
        d += abs(s->p[i] % m->w - m->exit[i] % m->w) + abs(s->p[i] / m->w - m->exit[i] / m->w);
    return d;
}

static void measure(const twk_map *m, metric *mt)
{
    twk_state s = {{(uint8_t)m->start[0], (uint8_t)m->start[1]}, 0};
    mt->par = twk_solve(m, &s, NULL, &mt->states);
    int legal = 0, bad = 0, legal0 = 0, bad0 = 0;
    twk_state c = s;
    for (int left = mt->par; left > 0; left--) {
        int dir = -1;
        twk_solve(m, &c, &dir, NULL);
        for (int k = 0; k < 4; k++) {
            twk_state n;
            if (!twk_try(m, &c, k, &n, NULL)) continue;
            int mis = twk_solve(m, &n, NULL, NULL) != left - 1;
            legal++;
            bad += mis;
            if (left == mt->par) { legal0++; bad0 += mis; }
        }
        twk_try(m, &c, dir, &c, NULL);
    }
    mt->trap = legal ? (double)bad / legal : 0;
    mt->first = legal0 ? (double)bad0 / legal0 : 0;
    uint32_t r = pr_seed(77);
    int wins = 0;
    for (int t = 0; t < 300; t++) {
        twk_state p = s;
        for (int k = 0; k < mt->par + mt->par / 2 + 1 && !twk_solved(m, &p); k++) {
            int ok[4], nok = 0, near[4], nnear = 0, here = gap(m, &p);
            twk_state n[4];
            for (int d = 0; d < 4; d++) {
                if (!twk_try(m, &p, d, &n[d], NULL)) continue;
                ok[nok++] = d;
                if (gap(m, &n[d]) <= here) near[nnear++] = d;
            }
            if (!nok) break;
            p = n[nnear && pr_below(&r, 4) ? near[pr_below(&r, nnear)] : ok[pr_below(&r, nok)]];
        }
        wins += twk_solved(m, &p);
    }
    mt->naive = wins / 300.0;
    /* the score: the moves, worth more where the mistakes are many and the naive players fail */
    mt->score = mt->par * (0.6 + mt->trap) * (1.2 - mt->naive) + 0.3 * log((double)mt->states + 1);
}

#ifdef TWK_TABLE                                /* the shipped levels' table: -DTWK_TABLE */
#include "games/twinwalk_levels.h"
int main(void)
{
    (void)CH, (void)by_diff, (void)look_of, (void)met, (void)crop, (void)wall_unseen;
    double sum[TWK_CHAPTERS][5] = {{0}};
    int trivial = 0;
    printf("lvl par states trap first naive score  map\n");
    for (int lv = 0; lv < TWK_LEVELS; lv++) {
        twk_map m;
        metric mt;
        twk_parse(&m, twk_levels[lv]);
        measure(&m, &mt);
        printf("%4d %3d %6d %5.2f %5.2f %5.2f %6.2f  %s\n", lv + 1, mt.par, mt.states, mt.trap, mt.first, mt.naive,
               mt.score, twk_levels[lv]);
        int triv = mt.naive > 0.1 || mt.par <= 4;
        double *s = sum[lv / PER];
        s[0] += mt.par; s[1] += mt.trap; s[2] += mt.naive; s[3] += mt.score; s[4] += triv;
        trivial += lv >= 3 && triv;
    }
    printf("\nchapter  par  trap naive score trivial\n");
    for (int c = 0; c < TWK_CHAPTERS; c++)
        printf("%7d %5.1f %5.2f %5.2f %5.1f %4.0f\n", c + 1, sum[c][0] / PER, sum[c][1] / PER, sum[c][2] / PER,
               sum[c][3] / PER, sum[c][4]);
    printf("trivial levels from level 4 (naive > 0.1 or par <= 4): %d\n", trivial);
    return 0;
}
#else
/* a chapter's levels from its maps (sorted by score): its first `intro` from the easy end (the chapter's new tiles
 * shown plainly), the others spread from `floor` to the hardest over the maps no naive player solves, the last of
 * the hardest few; of the maps near each place on the curve the best looking, another size than the last level's
 * when it can */
static int pick(const cand *pool, int n, const chapter *C, int *chosen)
{
    static int hard[MAXC];
    int nh = 0, used[MAXC] = {0};
    for (int i = 0; i < n; i++)
        if (pool[i].m.naive <= NAIVE_MAX && pool[i].m.score >= C->floor) hard[nh++] = i;
    fprintf(stderr, "  %d hard maps\n", nh);
    if (nh < PER - C->intro + 2) return 0;
    int lw = 0, lh = 0, last = -1;
    for (int k = 0; k < C->intro; k++) {
        int span = n / 5 > C->intro ? n / 5 : C->intro, i = -1;
        int a = k * span / C->intro, b = (k + 1) * span / C->intro - 1;
        if (C->from_easiest && k == 0) b = a + 1;
        if (a <= last) a = last + 1;
        if (b < a) b = a;
        for (int j = a; j <= b && j < n; j++)       /* short ones: a long easy way is a dull one */
            if (i < 0 || (pool[j].par <= C->lo + 5 && (pool[i].par > C->lo + 5 || pool[j].look > pool[i].look))) i = j;
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
            double v = c->look - (c->w == lw && c->h == lh ? 0.6 : 0);
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
    kept = malloc(sizeof *kept * 8000);
    cand *c = malloc(sizeof *c * MAXC);
    if (!kept || !c) return 1;
    printf("/* Twin Walkers' levels: made by tools/twinwalk_gen.c (our own maps, found by its search; do not edit).\n"
           " * Each map's rows from the top split by '|' (twinwalk.h), and its par: the fewest moves. */\n");
    static char out[TWK_LEVELS][TEXT];
    static int outpar[TWK_LEVELS];
    for (int ch = 0; ch < TWK_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        int n = 0;
        for (int k = 0; k < C->makes; k++) {
            uint32_t seed = 1000003u * (uint32_t)(ch + 1) + 7919u * (uint32_t)k;
            twk_spec sp = {C->w, C->h, -1, C->walls, C->crumble, C->plates, C->ice, 0, 0, C->iters, 0};
            uint32_t rs = pr_seed(seed ^ 0xabcdefu);
            int big = k * (C->grow + 1) / C->makes;    /* the sizes grow along the chapter's makes */
            sp.w += big;
            sp.h += big + (pr_below(&rs, 3) == 0);
            if (sp.w > 8) sp.w = 8;
            if (sp.h > 10) sp.h = 10;
            if (pr_below(&rs, 6) == 0) sp.sym = 0;     /* a few left free */
            sp.lo = sp.hi = C->lo + k * (C->hi - C->lo + 1) / C->makes;
            if (C->plates > 1) sp.plates = 1 + pr_below(&rs, C->plates);
            twk_map m;
            if (!twk_make(&m, &sp, seed)) continue;
            int par = par_of(&m);
            wall_unseen(&m, par);
            crop(&m);
            if (m.w < C->w - 1 || m.h < C->h - 1) continue;   /* walled up to a corner of what it was */
            int walls = 0;
            for (int q = 0; q < m.w * m.h; q++) walls += m.t[q] == TWK_WALL;
            if (walls * 100 > 52 * m.w * m.h) continue;   /* more wall than floor: a heavy look */
            int states;
            twk_state s0 = {{(uint8_t)m.start[0], (uint8_t)m.start[1]}, 0};
            if (twk_solve(&m, &s0, NULL, &states) != par) continue;
            cand *x = &c[n];
            twk_text(&m, x->text);
            if (met(x->text)) continue;
            x->par = par;
            x->states = states;
            measure(&m, &x->m);
            x->diff = x->m.score;
            x->w = m.w;
            x->h = m.h;
            x->look = look_of(&m);
            n++;
            if (n >= MAXC) break;
        }
        qsort(c, (size_t)n, sizeof *c, by_diff);
        fprintf(stderr, "chapter %d: %d maps, par %d..%d\n", ch + 1, n, n ? c[0].par : 0, n ? c[n - 1].par : 0);
        int chosen[PER];
        if (n < PER || !pick(c, n, C, chosen)) { fprintf(stderr, "too few\n"); return 1; }
        for (int i = 0; i < PER; i++) {
            strcpy(out[ch * PER + i], c[chosen[i]].text);
            outpar[ch * PER + i] = c[chosen[i]].par;
        }
    }
    printf("const char *const twk_levels[TWK_LEVELS] = {\n");
    for (int i = 0; i < TWK_LEVELS; i++) printf("    \"%s\",\n", out[i]);
    printf("};\n\nconst uint8_t twk_level_par[TWK_LEVELS] = {");
    for (int i = 0; i < TWK_LEVELS; i++) printf("%s%d,", i % 25 ? " " : "\n    ", outpar[i]);
    printf("\n};\n");
    for (int ch = 0; ch < TWK_CHAPTERS; ch++) {
        fprintf(stderr, "ch%d pars:", ch + 1);
        for (int i = 0; i < PER; i++) fprintf(stderr, " %d", outpar[ch * PER + i]);
        fprintf(stderr, "\n");
    }
    free(kept);
    free(c);
    return 0;
}
#endif
