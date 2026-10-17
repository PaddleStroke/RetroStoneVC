/*
 * Traffic Jam's levels: src/games/parking_levels.h. Our own levels: no published or commercial set is used. A lot:
 * vehicles parked by chance by the rules (pkj_fill: the red car in its row, then 5 to 14 cars and trucks where they
 * fit; each lot from a seed of its own); its whole family of positions found by the rules (pkj_family: every
 * position its vehicles can reach, and the fewest moves out of each, searched back from the solved ones). A family is
 * known by its farthest position whose text comes first (a family met twice is kept once). Its candidate: the one of
 * its farthest positions that looks best set out (the vehicles spread over the whole lot: their middle near the lot's,
 * no row or column left empty, the four quarters alike full; not all pushed to one side).
 * The levels: each chapter has its range of moves; its 25 levels climb evenly from the range's first to its last (or
 * the hardest found), each one the best set out at its number of moves, rarely with as many vehicles as the level
 * before (a varied mix), never fewer moves than the level before; the last of a chapter (its showpiece) is the
 * hardest. Chapter 1 wants few vehicles in the lot, few of them moving (a first look that is easy to read).
 * The lots: 40000 laid by chance, and the deep ones met before (KNOWN). Deterministic (fixed seeds).
 *   gcc -O2 -fopenmp -Iapps/boardgames/src -o /tmp/pkjgen apps/boardgames/tools/parking_gen.c -lm && \
 *       /tmp/pkjgen > apps/boardgames/src/games/parking_levels.h
 * (-fopenmp: on all the cores; without it the same levels, slower. /tmp/pkjgen stats [lots]: the candidates'
 * moves, counted.)
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define PKJ_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/parking_rules.c"

#define PER (PKJ_LEVELS / PKJ_CHAPTERS)
static int LOTS = 40000;
#define MAXC 400000

static const int LO[PKJ_CHAPTERS] = {3, 7, 11, 16, 21, 27}, HI[PKJ_CHAPTERS] = {6, 10, 15, 20, 26, 99};

/* deep lots met before (the hardest families a longer search of 150000 lots found: deep lots are rare), searched
 * again with the new lots */
static const char *const KNOWN[] = {
    ".BBBCC...DEEAAFD....FGGG..HHHI.....I", "BB.CD.E..CD.E.AAFGHHI.FG..IJJJKKLLMM", ".BCC.D.BEEEDFAAGHIF..GHIF.JKKKLLJ...",
    "BBCD....CDEEF.AAGHF...GHIIJJJH..KKLL", "BBC....DC.EF.DAAEFGGGHEF...HII...JJJ", ".BCCC..BD...AADEF.GGGEFHIJJKKHILLMMM",
    "BC..DDBCEEF.BAAGFHIIJGFH..JKKL.....L", ".BB.CD.E..CD.EAAFGHHI.FGJ.IKLLJMMKNN", "BBC.....CD..AAED.F..EDGF.HIIGJ.HKKGJ",
    "B.CCDEB...DEBFGAAEHFGIJJH.KILLMMKNNN", "BCCDD.BEEFF.AAGH..IIGHJK.LMMJK.LNNNK", "....BBC.DDE.C.AAEFG.HHEFG.IJKKLLIJ..",
    "BBCCCD...EED..AAFGHIIJFGH.KJLLMMKJ..", "B.CCDDB..EFFAAGE..HIGEJJHIKKLM.NN.LM", "BBCCCDE....DEAAFGHIIJFGH..JKKK..LL..",
    "BBC..D.EC..DFEAAGDFHHIG.JK.ILLJKMMNN", ".B..CCDB.EEFDBAAGFHIIJGFH.KJLLMMKNN.", "..BCCCDDB.E.F.AAE.F...GGHHIIIJKKK..J",
    "BBCC.DEEEFFDAA..GHI.JJGHI.K.LLMMK.NN", "BBCDDDE.CFGGEAAF..HH.IIJKKLL.J...MMM", "B.CCDDB.EEF.AAG.F.HHGIIJKKKL.J.MML.J",
    "....BBCCCDDEAAF..E..FGHIJKKGHIJ.LLMM", "BCCCDDBEFF.G.EAA.GHIIJ.GH.KJLL..KMMM", "BCDDEEBCFFGHBAA.GH..IJJH..IKLLMMMKNN",
    "BCCDD.B.EEFFAAGHI.JJGHIK.LMMMK.LNNNK", ".B.CCCDB.EF.DAAEF.G.HHF.G.IJJKLLI..K", "..BBBC.DDEFCAAGEF.H.GEIIH.JJKLMM..KL",
    "BBBC...DDCE.AAFGE.H.FGIIH.JJKK...LL.", "BBBCCD.EFF.D.EAA.DGHHI..G.JIKKG.JLLL", "...BBBCCCDDEAAF..EGGF..HIJJKLHIMMKL.",
    ".BCCDD.BE.FFAAE..GH.IIJGHKKLJMNN.LJM", "BBC..D..C..DE.AA.FEGHIIFJGHKLLJMMK..", "BCCDD.B.EFG.AAEFG..HIIJJ.HKKKL.MMM.L",
    "BBBCC...D.EEAAD.F.GGHHF.I..JJKI.LLLK", "BCCD..B..DEEAAFD...GF.HH.GIIJKLLMMJK", "..BBBC..DDECAAFGECH.FGIIH.JJKK...LLL",
    "..BBCCDDDEEFAAG..FHHG..IJKKLMIJNNLM.", "BBBCCDEEFFGD..AAGH..IJJHK.ILMMKNNLOO", "BCCCDDB..EFFBAAE.GHHIEJG.KI.JL.KMMML",
    "...BCCDD.B.EFAAGHEFIIGHJK.LMMJK.L.NN", "BBB.C.....C..AA.DEFGHHDEFGI.JJKKI...", "B.CDDDB.CEEEFAAG.HFI.G.HFIJJKLMMM.KL",
    "BBBC....DCEEAAD..FGGHH.FIJJJKKI.LLMM", "BC.DDEBCFFGEHAAIGJH..I.JH.KLLLMMK.NN", "B.CCDDB.EEFGAAH.FGIIHJJKLLLM.K.NNM..",
    "B.CCDDB....EAAF..EGGFHHIJJJKLIMMMKL.", "...B.....BCCAADEF.GGDEFHIJJJKHI.LLKH", "BBB.CD.EE.CD.AA.FGHIJJFGHIKLLGMMK...",
    ".BBCD.EEECD.AA.CFGH.IIFGH.JKKLMMJ..L", "B..CCCBDDEFGAA.EFGHHIJFG..IJKK.LLMM.",
};
#define NKNOWN ((int)(sizeof KNOWN / sizeof *KNOWN))

typedef struct {
    char text[PKJ_N * PKJ_N + 1];
    int par, movers, cars;
    double ugly;
} cand;

static cand *cands;
static int ncand;

/* the family: its farthest position whose text comes first (its name), and the best set out of its farthest */
typedef struct { int far; double ugly; char text[PKJ_N * PKJ_N + 1], pick[PKJ_N * PKJ_N + 1]; } look;

/* a position's text with its vehicles lettered in the order their first bays come (the same lot, any order) */
static void canon(const pkj_game *g, char *out)
{
    pkj_text(g, out);
    char map[26];
    memset(map, 0, sizeof map);
    char next = 'B';
    for (int c = 0; c < PKJ_N * PKJ_N; c++) {
        if (out[c] == '.' || out[c] == 'A') continue;
        int l = out[c] - 'A';
        if (!map[l]) map[l] = next++;
        out[c] = map[l];
    }
}

/* how badly the lot is set out (0: well): its vehicles' middle away from the lot's, rows and columns left empty, the
 * quarters unlike */
static double ugly_of(const char *t)
{
    int row[PKJ_N] = {0}, col[PKJ_N] = {0}, quarter[4] = {0}, n = 0;
    double cx = 0, cy = 0;
    for (int c = 0; c < PKJ_N * PKJ_N; c++) {
        if (t[c] == '.') continue;
        int x = c % PKJ_N, y = c / PKJ_N;
        row[y]++;
        col[x]++;
        quarter[(x >= PKJ_N / 2) + 2 * (y >= PKJ_N / 2)]++;
        cx += x;
        cy += y;
        n++;
    }
    if (!n) return 99;
    cx = cx / n - (PKJ_N - 1) / 2.0;
    cy = cy / n - (PKJ_N - 1) / 2.0;
    int empty = 0, qlo = 99, qhi = 0;
    for (int i = 0; i < PKJ_N; i++) empty += !row[i] + !col[i];
    for (int q = 0; q < 4; q++) {
        if (quarter[q] < qlo) qlo = quarter[q];
        if (quarter[q] > qhi) qhi = quarter[q];
    }
    return 3 * (fabs(cx) + fabs(cy)) + 1.5 * empty + 0.25 * (qhi - qlo);
}

static void each(const pkj_game *pos, int dist, void *ctx)
{
    look *L = ctx;
    if (dist < L->far) return;
    char t[PKJ_N * PKJ_N + 1];
    canon(pos, t);
    double u = ugly_of(t);
    if (dist > L->far) {
        L->far = dist;
        strcpy(L->text, t);
        strcpy(L->pick, t);
        L->ugly = u;
        return;
    }
    if (strcmp(t, L->text) < 0) strcpy(L->text, t);
    if (u < L->ugly - 1e-9 || (u < L->ugly + 1e-9 && strcmp(t, L->pick) < 0)) {
        strcpy(L->pick, t);
        L->ugly = u;
    }
}

static int seen(const char *t)                  /* a hash of the texts kept */
{
    static char (*tab)[PKJ_N * PKJ_N + 1];
    enum { SIZE = 1 << 20 };
    if (!tab) tab = calloc(SIZE, sizeof *tab);
    unsigned h = 2166136261u;
    for (const char *p = t; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
    for (h &= SIZE - 1; tab[h][0]; h = (h + 1) & (SIZE - 1))
        if (!strcmp(tab[h], t)) return 1;
    strcpy(tab[h], t);
    return 0;
}

static int by_par(const void *a, const void *b)
{
    const cand *p = a, *q = b;
    if (p->par != q->par) return p->par - q->par;
    double d = p->ugly - q->ugly;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(p->text, q->text);
}

static int fits(const cand *c, int ch)
{
    if (c->par < LO[ch] || c->par > HI[ch]) return 0;
    if (ch == 0 && (c->cars > 8 || c->movers > 4)) return 0;   /* few vehicles, few of them moving */
    if (ch == 1 && c->cars > 11) return 0;
    return 1;
}

int main(int argc, char **argv)
{
    int stats = argc > 1 && !strcmp(argv[1], "stats");
    if (argc > 2) LOTS = atoi(argv[2]);
    cands = malloc(MAXC * sizeof *cands);
    static int hist[128];
    /* each lot from its own seed: the lots can be looked at in any order, on several threads (-fopenmp), and the
     * candidates come out the same (sorted by moves, then looks, then text) */
#pragma omp parallel for schedule(dynamic, 64)
    for (int lot = 0; lot < LOTS + NKNOWN; lot++) {
        uint32_t rng = pr_seed(20261016u + 7919u * (uint32_t)lot);
        pkj_game g;
        if (lot >= LOTS) pkj_parse(&g, KNOWN[lot - LOTS]);
        else pkj_fill(&g, 5 + pr_below(&rng, 10), &rng);
        look L = {-1, 99, "", ""};
        if (!pkj_family(&g, 30000, each, &L) && !pkj_family(&g, PKJ_FAMILY, each, &L)) continue;
        if (L.far < LO[0]) continue;
        int fresh;
#pragma omp critical(kept)
        fresh = !seen(L.text);
        if (!fresh) continue;
        cand c;
        strcpy(c.text, L.pick);
        c.par = L.far;
        c.ugly = L.ugly;
        pkj_game p;                             /* (from its text: the same whichever lot found it) */
        pkj_parse(&p, L.pick);
        c.movers = pkj_movers(&p);
        c.cars = p.nc;
#pragma omp critical(kept)
        if (ncand < MAXC) {
            cands[ncand++] = c;
            hist[c.par < 127 ? c.par : 127]++;
        }
    }
    if (stats) {
        for (int d = 0; d < 128; d++)
            if (hist[d]) fprintf(stderr, "%3d moves: %d\n", d, hist[d]);
        return 0;
    }
    qsort(cands, (size_t)ncand, sizeof *cands, by_par);
    printf("/* Traffic Jam's levels: made by tools/parking_gen.c (our own lots, found by its search; do not edit). "
           "Each\n"
           " * lot's 36 bays row by row, and its par: the fewest moves. */\n");
    static cand out[PKJ_LEVELS];
    static char used[MAXC];
    for (int ch = 0; ch < PKJ_CHAPTERS; ch++) {
        int lo = -1, top = 0;
        for (int i = 0; i < ncand; i++)
            if (fits(&cands[i], ch)) {
                if (lo < 0) lo = cands[i].par;
                if (cands[i].par > top) top = cands[i].par;
            }
        if (lo < 0) { fprintf(stderr, "chapter %d: no candidates\n", ch + 1); return 1; }
        int prev_par = 0, prev_cars = 0, left[128] = {0};   /* left[p]: the candidates not taken at p moves or more */
        for (int i = 0; i < ncand; i++)
            if (fits(&cands[i], ch))
                for (int p = 0; p <= cands[i].par && p < 128; p++) left[p]++;
        for (int k = 0; k < PER; k++) {         /* climbing evenly from the range's first moves to the hardest */
            double want = lo + (double)(top - lo) * k / (PER - 1), best_score = 1e9;
            int best = -1;
            for (int i = 0; i < ncand; i++) {
                const cand *c = &cands[i];
                if (used[i] || c->par < prev_par || !fits(c, ch)) continue;
                if (left[c->par < 127 ? c->par : 127] - 1 < PER - 1 - k) continue;   /* enough left for the rest */
                double score = 4 * fabs(c->par - want) + c->ugly + (c->cars == prev_cars ? 0.8 : 0);
                if (ch == 0) score += 0.3 * c->cars + 0.3 * c->movers;
                if (score < best_score) { best_score = score; best = i; }
            }
            if (best < 0) { fprintf(stderr, "chapter %d: only %d levels\n", ch + 1, k); return 1; }
            used[best] = 1;
            for (int p = 0; p <= cands[best].par && p < 128; p++) left[p]--;
            out[ch * PER + k] = cands[best];
            prev_par = cands[best].par;
            prev_cars = cands[best].cars;
        }
        double u = 0;
        for (int k = 0; k < PER; k++) u += out[ch * PER + k].ugly;
        fprintf(stderr, "chapter %d: moves %d .. %d, looks %.2f a level\n", ch + 1, out[ch * PER].par,
                out[ch * PER + PER - 1].par, u / PER);
    }
    printf("const char *const pkj_levels[PKJ_LEVELS] = {\n");
    for (int l = 0; l < PKJ_LEVELS; l++) printf("    \"%s\",\n", out[l].text);
    printf("};\n\nconst uint8_t pkj_level_par[PKJ_LEVELS] = {");
    for (int l = 0; l < PKJ_LEVELS; l++) printf("%s%d,", l % 25 ? " " : "\n    ", out[l].par);
    printf("\n};\n");
    return 0;
}
