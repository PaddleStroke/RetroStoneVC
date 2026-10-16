/*
 * Go Problems' levels: src/games/goproblems_levels.h, made by the game's own solver (goproblems_rules.c) from corners
 * drawn at random here: an eye space along the edges (a few points joined, from the corner or the side), the
 * defender's stones round it, the attacker's wall round those (alive: it touches the outside), then a few changes (a
 * stone of the wall missing, a defender's stone missing or turned, stones inside the eye space). Black plays first:
 * a kill (White defends) or a life problem (Black defends). The classic shapes come up by themselves (the straight
 * three, the bent four, the bulky five...), as the solver finds them.
 * A corner is kept when exactly one first move wins, every other first move (and the pass) proven to lose, and its
 * main line (the quickest win against the longest resistance, kept with the level) takes no ko and needs no pass from
 * Black. Its depth is that line's plies; the six chapters hold 25 problems each of depth 1, 3, 5, 7, 9 and 11 or more
 * (a life may end one ply later), taken evenly from the easiest to the hardest by the solver's effort, kills and
 * lives in turn; a corner and its mirror image only once.
 *   gcc -O2 -DGPZ_NO_LEVELS -Iapps/boardgames/src -o /tmp/gpzgen apps/boardgames/tools/goproblems_gen.c \
 *       apps/boardgames/src/games/goproblems_rules.c apps/boardgames/src/games/go_rules.c \
 *       && /tmp/gpzgen > apps/boardgames/src/games/goproblems_levels.h
 * (about a quarter of an hour; "/tmp/gpzgen N v": N corners drawn, the progress shown).
 * Deterministic (fixed seeds). MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/goproblems.h"

#define DMAX GPZ_PROOF                          /* the deepest search */
#define PER 25
#define POOL 120                                /* the candidates kept a chapter */
#define TRIES 12000                             /* the corners drawn (the levels: 12000) */

static uint64_t rs = 0x1234567u;
static int rnd(int n)
{
    rs ^= rs << 13;
    rs ^= rs >> 7;
    rs ^= rs << 17;
    return n > 0 ? (int)(rs % (uint64_t)n) : 0;
}

enum { T_NONE, T_EYE, T_DEF, T_ATT, T_GAP };

/* a corner drawn at random into pz (its depth and first move unknown yet); 0: not a good one */
static int draw_corner(gpz_problem *pz)
{
    int t[GPZ_MAXW + 2][GPZ_MAXW + 2];
    memset(t, 0, sizeof t);
    int size = 2 + rnd(6), x0, y0;
    if (rnd(5) < 2) x0 = y0 = 0;
    else if (rnd(2)) { x0 = rnd(4); y0 = 0; }
    else { x0 = 0; y0 = rnd(3); }
    int maxx = 6 + rnd(2), maxy = 2 + rnd(2);
    if (y0 > 0 && x0 == 0) { int s = maxx; maxx = maxy; maxy = s; }   /* along the left side */
    t[x0][y0] = T_EYE;
    for (int n = 1, guard = 0; n < size && guard < 200; guard++) {
        int x = rnd(maxx + 1), y = rnd(maxy + 1);
        if (t[x][y]) continue;
        int nb = (x > 0 && t[x - 1][y]) || t[x + 1][y] || (y > 0 && t[x][y - 1]) || t[x][y + 1];
        if (!nb) continue;
        t[x][y] = T_EYE;
        n++;
    }
    int skip_diag = rnd(3);                     /* a diagonal link left out now and then: a cutting point */
    for (int pass = 0; pass < 2; pass++) {      /* the defender round the eyes, the attacker round the defender */
        int from = pass ? T_DEF : T_EYE, to = pass ? T_ATT : T_DEF, mark[GPZ_MAXW + 2][GPZ_MAXW + 2] = {{0}};
        for (int x = 0; x < GPZ_MAXW; x++)
            for (int y = 0; y < GPZ_MAXW; y++) {
                if (t[x][y]) continue;
                int orth = 0, diag = 0;
                for (int dx = -1; dx <= 1; dx++)
                    for (int dy = -1; dy <= 1; dy++) {
                        int u = x + dx, v = y + dy;
                        if ((!dx && !dy) || u < 0 || v < 0 || t[u][v] != from) continue;
                        if (dx && dy) diag = 1; else orth = 1;
                    }
                if (orth || (diag && !(pass == 0 && skip_diag == 2 && rnd(3) == 0))) mark[x][y] = 1;
            }
        for (int x = 0; x < GPZ_MAXW; x++)
            for (int y = 0; y < GPZ_MAXW; y++)
                if (mark[x][y]) t[x][y] = to;
    }
    int w = 0, h = 0;
    for (int x = 0; x < GPZ_MAXW + 2; x++)
        for (int y = 0; y < GPZ_MAXW + 2; y++)
            if (t[x][y]) { if (x + 2 > w) w = x + 2; if (y + 2 > h) h = y + 2; }
    if (w > GPZ_MAXW || h > GPZ_MAXW) return 0;
    if (w < 4) w = 4;
    if (h < 4) h = 4;
    /* the changes: a cell of a kind turned into a gap (empty, or a stone) */
    int st[GPZ_MAXW + 2][GPZ_MAXW + 2];         /* the stones: 0, 1 the defender's, 2 the attacker's */
    memset(st, 0, sizeof st);
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) st[x][y] = t[x][y] == T_DEF ? 1 : t[x][y] == T_ATT ? 2 : 0;
    int changes[5][3] = {{T_DEF, 0, rnd(3)},                     /* defender's stones missing */
                         {T_DEF, 2, rnd(4) == 0},                /* one turned into the attacker's */
                         {T_ATT, 0, rnd(4) == 0 ? 1 + rnd(2) : 0},   /* the wall broken */
                         {T_EYE, 2, rnd(3)},                     /* the attacker's stones inside */
                         {T_EYE, 1, rnd(3) == 0}};               /* a defender's stone inside */
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < changes[i][2]; j++) {
            int cells[GPZ_MAXW * GPZ_MAXW], n = 0;
            for (int x = 0; x < w; x++)
                for (int y = 0; y < h; y++)
                    if (t[x][y] == changes[i][0]) cells[n++] = x * 32 + y;
            if (!n) break;
            int c = cells[rnd(n)];
            t[c / 32][c % 32] = T_GAP;
            st[c / 32][c % 32] = changes[i][1];
        }
    /* the key: a stone of the defender's biggest group, the nearest the corner */
    int best = -1, bestn = 0, bestd = 99, seen[GPZ_MAXW + 2][GPZ_MAXW + 2] = {{0}};
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (st[x][y] != 1 || seen[x][y]) continue;
            int size = 0, stack[256], sp = 0, near = 99, at = -1;
            stack[sp++] = x * 32 + y;
            seen[x][y] = 1;
            while (sp) {
                int c = stack[--sp], cx = c / 32, cy = c % 32;
                size++;
                if (cx + cy < near) { near = cx + cy; at = c; }
                static const int D4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (int d = 0; d < 4; d++) {
                    int u = cx + D4[d][0], v = cy + D4[d][1];
                    if (u < 0 || v < 0 || u >= w || v >= h || seen[u][v] || st[u][v] != 1) continue;
                    seen[u][v] = 1;
                    stack[sp++] = u * 32 + v;
                }
            }
            if (size > bestn || (size == bestn && near < bestd)) { bestn = size; bestd = near; best = at; }
        }
    if (best < 0 || bestn < 2) return 0;
    int kind = rnd(2) ? GPZ_KILL : GPZ_LIVE, def = kind == GPZ_KILL ? GPZ_WHITE : GPZ_BLACK;
    int first = -1;
    for (int y = 0; y < h && first < 0; y++)
        for (int x = 0; x < w && first < 0; x++)
            if (t[x][y] && !st[x][y]) first = x * 32 + y;
    if (first < 0) return 0;
    char s[600];
    int k = snprintf(s, sizeof s, "%c %d %d 1 %d,%d %d,%d ", kind == GPZ_KILL ? 'K' : 'L', w, h, best / 32, best % 32,
                     first / 32, first % 32);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++)
            s[k++] = !t[x][y] ? ',' : st[x][y] == 0 ? '.' : (st[x][y] == 1) == (def == GPZ_BLACK) ? 'X' : 'O';
        if (y < h - 1) s[k++] = '/';
    }
    s[k] = 0;
    return gpz_parse(pz, s);
}

typedef struct { char text[600]; long effort; int kind, depth; char canon[600]; } cand;
static cand pool[GPZ_CHAPTERS][POOL];
static int npool[GPZ_CHAPTERS], nkind[GPZ_CHAPTERS][2];

/* the corner as a string of its stones, its mirror's (the diagonal) the same: the smaller of the two */
static void canon(const gpz_problem *pz, char *out)
{
    char a[400], b[400];
    int k = 0;
    for (int y = 0; y < pz->h; y++, a[k++] = '/')
        for (int x = 0; x < pz->w; x++) a[k++] = ".XO?,"[pz->start.c[GPZ_CELL(x, y)]];
    a[k] = 0;
    k = 0;
    for (int x = 0; x < pz->w; x++, b[k++] = '/')
        for (int y = 0; y < pz->h; y++) b[k++] = ".XO?,"[pz->start.c[GPZ_CELL(x, y)]];
    b[k] = 0;
    sprintf(out, "%c%s", pz->kind == GPZ_KILL ? 'K' : 'L', strcmp(a, b) < 0 ? a : b);
}

/* the main line from b (side to move): the quickest win for Black, the longest resistance for White; 0: a ko on
 * the way (a single stone taken by a single stone left with one liberty), or it does not end as it should */
static int main_line(gpz_problem *pz, int depth)
{
    pz->nline = 0;
    gpz_board b = pz->start;
    uint64_t path[64];
    int np = 0, side = GPZ_BLACK, passes = 0;
    path[np++] = b.hash;
    int bwin = pz->kind == GPZ_KILL ? GPZ_AWIN : GPZ_DWIN;
    for (int ply = 0; ply < depth + 2; ply++) {
        gpz_eval ev[GPZ_MAXMOVES];
        int n = gpz_analyse(pz, &b, side, passes, path, np, depth - ply, ev, NULL), best = -1;
        for (int i = 0; i < n; i++) {
            if (side == GPZ_BLACK) {
                if (ev[i].val == bwin && (best < 0 || ev[i].depth < ev[best].depth)) best = i;
            } else if (ev[i].val == bwin && (best < 0 || ev[i].depth > ev[best].depth ||
                                             (ev[i].depth == ev[best].depth && ev[best].move == GPZ_PASS)))
                best = i;
        }
        if (best < 0) return 0;
        for (int i = 0; side == GPZ_WHITE && i < n; i++)
            if (ev[i].val != bwin) return 0;    /* White has a way out: not a line */
        int m = ev[best].move;
        if (pz->nline >= GPZ_LINE || (m == GPZ_PASS && side == GPZ_BLACK)) return 0;   /* Black never passes */
        pz->line[pz->nline++] = (uint8_t)m;
        if (m == GPZ_PASS) {
            if (passes) return 1;
            passes = 1;
        } else {
            gpz_board nb = b;
            int took = gpz_play(&nb, m, side) - 1;
            if (took == 1) {                    /* a ko's shape? */
                int libs = 0, alone = 1;
                static const int D[4] = {1, -1, GPZ_S, -GPZ_S};
                for (int d = 0; d < 4; d++) {
                    int q = m + D[d], v = nb.c[q];
                    if (v == side) alone = 0;
                    if (v == GPZ_EMPTY) libs++;
                }
                if (alone && libs == 1) return 0;
            }
            b = nb;
            path[np++] = b.hash;
            passes = 0;
            if (gpz_status(pz, &b)) return gpz_status(pz, &b) == bwin && ply + 1 <= depth;
        }
        if (pz->kind == GPZ_LIVE && side == GPZ_WHITE && m == GPZ_PASS) return 1;
        side = 3 - side;
        if (np >= 60) return 0;
    }
    return 0;
}

static int by_effort(const void *a, const void *b)
{
    const cand *x = *(const cand *const *)a, *y = *(const cand *const *)b;
    return x->effort < y->effort ? -1 : x->effort > y->effort ? 1 : strcmp(x->text, y->text);
}

/* 25 a chapter, evenly from the easiest to the hardest, kills and lives in turn */
static void emit(void)
{
    static const char *what[GPZ_CHAPTERS] = {"one move", "three plies", "five plies", "seven plies", "nine plies",
                                             "eleven plies or more"};
    printf("/*\n * Go Problems' levels: 150 corners in six chapters of 25 by the depth of their main line, made by\n"
           " * tools/goproblems_gen.c (its header says how): the kind (K kill, L live), the window, the depth,\n"
           " * the key stone, the first move, the rows from the edge up, the main line (goproblems.h). Do not edit.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const char *const gpz_level_text[GPZ_LEVELS] = {\n");
    for (int ch = 0; ch < GPZ_CHAPTERS; ch++) {
        const cand *k[2][POOL], *pick[PER];
        int nk[2] = {0, 0}, np = 0;
        for (int i = 0; i < npool[ch]; i++) k[pool[ch][i].kind][nk[pool[ch][i].kind]++] = &pool[ch][i];
        for (int t = 0; t < 2; t++) qsort(k[t], (size_t)nk[t], sizeof k[t][0], by_effort);
        int want[2] = {PER / 2 + 1, PER / 2};
        if (nk[0] < want[0]) { want[1] += want[0] - nk[0]; want[0] = nk[0]; }
        if (nk[1] < want[1]) { want[0] += want[1] - nk[1]; want[1] = nk[1]; }
        for (int t = 0; t < 2; t++)
            for (int i = 0; i < want[t] && i < nk[t]; i++)
                pick[np++] = k[t][want[t] > 1 ? (int)((long)i * (nk[t] - 1) / (want[t] - 1)) : 0];
        qsort(pick, (size_t)np, sizeof pick[0], by_effort);
        printf("    /* chapter %d: %s */\n", ch + 1, what[ch]);
        for (int i = 0; i < PER; i++) printf("    \"%s\",\n", i < np ? pick[i]->text : "");
        if (np < PER) fprintf(stderr, "chapter %d: only %d\n", ch + 1, np);
    }
    printf("};\n");
}

int main(int argc, char **argv)
{
    long tries = 0, kept = 0, most = argc > 1 ? atol(argv[1]) : TRIES;
    for (tries = 0; tries < most; tries++) {
        if (argc > 2 && tries % 1000 == 0) {
            fprintf(stderr, "%ld:", tries);
            for (int c = 0; c < GPZ_CHAPTERS; c++) fprintf(stderr, " %d/%d", nkind[c][0], nkind[c][1]);
            fprintf(stderr, "\n");
        }
        int full = 1;
        for (int c = 0; c < GPZ_CHAPTERS; c++) full &= nkind[c][0] >= POOL / 2 && nkind[c][1] >= POOL / 2;
        if (full) break;
        gpz_problem pz;
        if (!draw_corner(&pz)) continue;
        int empties = 0;
        for (int c = 0; c < GPZ_CELLS; c++) empties += pz.start.c[c] == GPZ_EMPTY;
        if (empties < 2 || empties > 14 || gpz_status(&pz, &pz.start)) continue;
        gpz_new_search();
        gpz_eval ev[GPZ_MAXMOVES];
        long nodes = 0;
        int n = gpz_analyse(&pz, &pz.start, GPZ_BLACK, 0, &pz.start.hash, 1, DMAX, ev, &nodes);
        int bwin = pz.kind == GPZ_KILL ? GPZ_AWIN : GPZ_DWIN, wins = 0, w = -1, unk = 0;
        for (int i = 0; i < n; i++) {
            if (ev[i].val == bwin) { wins++; w = i; }
            else if (ev[i].val == GPZ_UNK) unk++;
        }
        if (wins != 1 || unk || ev[w].move == GPZ_PASS) continue;
        pz.first = ev[w].move;
        pz.depth = ev[w].depth;
        int ch = pz.depth >= 11 ? 5 : (pz.depth - 1) / 2;
        if (ch < 0 || ch >= GPZ_CHAPTERS || nkind[ch][pz.kind] >= POOL / 2) continue;
        if (!main_line(&pz, pz.depth)) continue;
        cand *c = &pool[ch][npool[ch]];
        canon(&pz, c->canon);
        int dup = 0;
        for (int i = 0; i < npool[ch] && !dup; i++) dup = !strcmp(pool[ch][i].canon, c->canon);
        if (dup) continue;
        gpz_write(&pz, c->text, sizeof c->text);
        c->effort = nodes;
        c->kind = pz.kind;
        c->depth = pz.depth;
        npool[ch]++;
        nkind[ch][pz.kind]++;
        kept++;
    }
    fprintf(stderr, "%ld tries, %ld kept:", tries, kept);
    for (int c = 0; c < GPZ_CHAPTERS; c++) fprintf(stderr, " %d/%d", nkind[c][0], nkind[c][1]);
    fprintf(stderr, "\n");
    emit();
    return 0;
}
