/*
 * Jigsaw's rules: the pictures (the tarot's, the paintings: their files there at the size the list says, their
 * artists dead over 70 years), every level (its picture, a cut near its count with cells near square, the chapters'
 * counts rising), the cut (every outline closed, inside its cell's margin, one piece's tab another's blank, no
 * outline crossing itself), playing (pieces snapping to their places and to each other, groups moving and turning
 * together, the tray, the hint, undo), the stars' rule and the script, and random puzzles over many seeds.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/jigsaw.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static jig_game G;

/* a baseline or progressive JPEG's size from its frame header; 0: not a JPEG */
static int jpeg_size(const char *path, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char b[4];
    int ok = 0;
    if (fread(b, 1, 2, f) == 2 && b[0] == 0xff && b[1] == 0xd8)
        for (;;) {
            int c = fgetc(f);
            if (c != 0xff) break;
            int m = fgetc(f);
            if (fread(b, 1, 2, f) != 2) break;
            int len = b[0] << 8 | b[1];
            if (m == 0xc0 || m == 0xc1 || m == 0xc2) {
                unsigned char s[5];
                if (fread(s, 1, 5, f) != 5) break;
                *h = s[1] << 8 | s[2];
                *w = s[3] << 8 | s[4];
                ok = 1;
                break;
            }
            fseek(f, len - 2, SEEK_CUR);
        }
    fclose(f);
    return ok;
}

static void test_pictures(void)
{
    int tarot[38] = {0}, paintings = 0;
    for (int i = 0; i < jig_pictures(); i++) {
        const jig_picture *P = jig_pic(i);
        CHECK(P->w > 0 && P->h > 0 && P->set < JS_SETS);
        if (P->tarot >= 0) {
            CHECK(P->tarot < 38 && !P->file);
            if (P->tarot < 38) tarot[P->tarot]++;
            CHECK(P->set == JS_TRUMPS || P->set == JS_COURTS);
            continue;
        }
        paintings++;
        CHECK(P->file && P->died > 0 && P->died <= 1955);   /* public domain: died more than 70 years ago */
        char path[256];
        int w = 0, h = 0;
        snprintf(path, sizeof path, "apps/boardgames/art/jigsaw/%s.jpg", P->file);
        if (!jpeg_size(path, &w, &h)) {
            printf("missing or not a JPEG: %s\n", path);
            fails++;
            continue;
        }
        CHECK(w == P->w && h == P->h);
        CHECK(w <= 1024 && h <= 1024 && (w == 1024 || h == 1024 || w >= 1000));
        FILE *f = fopen(path, "rb");
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        CHECK(size > 60000 && size < 260000);   /* the APK's budget */
    }
    for (int t = 0; t < 38; t++) CHECK(tarot[t] == 1);
    CHECK(paintings >= 40);
}

static void test_grids(void)
{
    for (int i = 0; i < jig_pictures(); i++)
        for (int c = 0; c < JIG_COUNTS; c++) {
            const jig_picture *P = jig_pic(i);
            int cols, rows;
            jig_grid(P->w, P->h, jig_counts[c], &cols, &rows);
            float n = (float)(cols * rows), shape = ((float)P->w / cols) / ((float)P->h / rows);
            CHECK(cols * rows <= JIG_MAX);
            CHECK(n >= 0.8f * jig_counts[c] && n <= 1.25f * jig_counts[c]);
            CHECK(shape > 0.75f && shape < 1.33f);   /* the picture cut a little to make the cells square */
        }
}

static void test_levels(void)
{
    int n = jig_levels(), used[256] = {0};
    CHECK(n >= 120 && n <= 160);
    CHECK(jig_chapters() >= 6 && jig_chapters() <= 11);
    CHECK(jig_level_count(0) == 0 && !jig_level_rotate(0));   /* a first win: 12 pieces, upright */
    for (int l = 0; l < 40; l++) CHECK(jig_level_count(l) <= 2 && !jig_level_rotate(l));   /* the free edition's */
    for (int l = 0; l < n; l++) {
        int p = jig_level_pic(l);
        CHECK(p >= 0 && p < jig_pictures());
        if (p >= 0 && p < 256) used[p]++;
        int c = jig_chapter_of(l);
        CHECK(c >= 0 && c < jig_chapters());
        if (l) CHECK(c >= jig_chapter_of(l - 1));
        if (l && c == jig_chapter_of(l - 1)) CHECK(jig_level_count(l) >= jig_level_count(l - 1));
        if (l && c == jig_chapter_of(l - 1)) CHECK(jig_level_rotate(l) == jig_level_rotate(l - 1));
    }
    for (int c = 0; c < jig_chapters(); c++) CHECK(jig_chapter_of(jig_chapter_first(c)) == c);
    for (int p = 0; p < jig_pictures(); p++) CHECK(used[p] >= 1);   /* every picture somewhere */
    CHECK(jig_level_rotate(n - 1));             /* the last chapter: turned pieces */
}

static float area(const float *xy, int n)
{
    float a = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += xy[2 * i] * xy[2 * j + 1] - xy[2 * j] * xy[2 * i + 1];
    }
    return a / 2;
}

static int cross(const float *a, const float *b, const float *c, const float *d)   /* segments ab, cd cross */
{
    float d1 = (d[0] - c[0]) * (a[1] - c[1]) - (d[1] - c[1]) * (a[0] - c[0]);
    float d2 = (d[0] - c[0]) * (b[1] - c[1]) - (d[1] - c[1]) * (b[0] - c[0]);
    float d3 = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    float d4 = (b[0] - a[0]) * (d[1] - a[1]) - (b[1] - a[1]) * (d[0] - a[0]);
    return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
}

/* every outline of g: inside its margin, its area one cell give or take its knobs, the pieces' areas adding up to
 * the picture's; `simple`: no edge of it crossing another */
static void check_cut(const jig_game *g, int simple)
{
    float xy[2 * JIG_OUTLINE], total = 0;
    for (int k = 0; k < g->n; k++) {
        int n = jig_outline(g, k, xy), i = k % g->cols, j = k / g->cols;
        CHECK(n >= 4 && n <= JIG_OUTLINE);
        for (int q = 0; q < n; q++) {
            CHECK(xy[2 * q] >= i - JIG_MARGIN && xy[2 * q] <= i + 1 + JIG_MARGIN);
            CHECK(xy[2 * q + 1] >= j - JIG_MARGIN && xy[2 * q + 1] <= j + 1 + JIG_MARGIN);
        }
        float a = area(xy, n);
        CHECK(a > 0.5f && a < 1.5f);            /* clockwise on the screen (y down), about one cell */
        total += a;
        if (!simple) continue;
        int bad = 0;
        for (int p = 0; p < n && !bad; p++)
            for (int q = p + 2; q < n && !bad; q++) {
                if (p == 0 && q == n - 1) continue;
                bad = cross(xy + 2 * p, xy + 2 * ((p + 1) % n), xy + 2 * q, xy + 2 * ((q + 1) % n));
            }
        CHECK(!bad);
    }
    CHECK(fabsf(total - g->n) < 0.01f * g->n);  /* tabs and blanks: what one gains its neighbour loses */
}

static void test_cut(void)
{
    for (int l = 0; l < jig_levels(); l++) {
        jig_load(&G, l);
        CHECK(G.n >= 9 && G.n <= JIG_MAX && G.s.ntray == G.n && G.s.placed == 0);
        check_cut(&G, l < 3 || l % 17 == 0);
    }
    jig_new(&G, 0, 0, 0, 1);                    /* a tab and the blank facing it are the same curve */
    int tabs = 0;
    for (int e = 0; e < (G.rows - 1) * G.cols; e++) tabs += G.hk[e].sign > 0;
    CHECK(tabs > 0 && tabs < (G.rows - 1) * G.cols);
}

static void place_all(jig_game *g)
{
    for (int k = 0; k < g->n; k++) {
        float x, y;
        jig_home(g, k, &x, &y);
        while (g->s.p[k].rot && g->s.p[k].where != JIG_PLACED) jig_rotate(g, k);
        if (g->s.p[k].where != JIG_PLACED) jig_drop(g, k, x + 0.1f, y - 0.1f);
    }
}

static void test_play(void)
{
    jig_load(&G, 10);
    int n = G.n, c = G.cols;
    float x, y;
    /* two neighbours dropped near each other, away from their places: they join */
    CHECK(jig_drop(&G, 0, 20, 20) == 0);
    CHECK(G.s.p[0].where == JIG_TABLE && G.s.ntray == n - 1 && G.s.nz == 1);
    CHECK(jig_drop(&G, 1, 21.2f, 19.9f) & JIG_SNAP);
    CHECK(G.s.p[1].group == G.s.p[0].group && fabsf(G.s.p[1].x - 21) < 1e-4f && fabsf(G.s.p[1].y - 20) < 1e-4f);
    CHECK(!jig_to_tray(&G, 1, 0));              /* a group stays on the table */
    CHECK(jig_drop(&G, 0, 25, 25) == 0);        /* the group moves as one */
    CHECK(fabsf(G.s.p[1].x - 26) < 1e-4f && fabsf(G.s.p[1].y - 25) < 1e-4f);
    jig_home(&G, 0, &x, &y);                    /* dropped near its place: the group locks in */
    int r = jig_drop(&G, 1, x + 1.2f, y + 0.1f);
    CHECK((r & JIG_PLACE) && G.s.placed == 2 && G.s.nz == 0);
    CHECK(G.s.p[0].where == JIG_PLACED && G.s.p[1].where == JIG_PLACED);
    CHECK(jig_drop(&G, 0, 3, 3) == 0);          /* a placed piece stays */
    /* a lone piece back into the tray, then out again */
    CHECK(jig_drop(&G, c, 30, 30) == 0);
    CHECK(jig_to_tray(&G, c, 0) && G.s.tray[0] == c && G.s.p[c].where == JIG_TRAY);
    /* undo, all the way */
    while (jig_undo(&G)) {}
    CHECK(G.s.placed == 0 && G.s.ntray == n && G.s.nz == 0);
    /* every piece dropped near its place: solved */
    place_all(&G);
    CHECK(jig_solved(&G) && G.s.placed == n && G.s.ntray == 0);

    /* turned pieces: not upright, they do not lock in; turned upright they do */
    jig_new(&G, 3, 1, 1, 99);
    for (int k = 0; k < G.n; k++) CHECK(G.s.p[k].rot >= 1 && G.s.p[k].rot <= 3);
    jig_home(&G, 5, &x, &y);
    CHECK(!(jig_drop(&G, 5, x, y) & JIG_PLACE) && G.s.p[5].where == JIG_TABLE);
    while (G.s.p[5].rot != 0 && G.s.p[5].where != JIG_PLACED) jig_rotate(&G, 5);
    CHECK(G.s.p[5].where == JIG_PLACED);        /* turned upright where it lay: its place */
    /* two neighbours turned alike join, then turn together round one of them */
    jig_new(&G, 3, 1, 1, 7);
    int a = 0, b = 1;
    while (G.s.p[a].rot != 1) jig_rotate(&G, a);
    while (G.s.p[b].rot != 1) jig_rotate(&G, b);
    jig_drop(&G, a, 20, 20);                    /* turned once: b (to a's right) lies below a */
    CHECK(jig_drop(&G, b, 20.1f, 21.1f) & JIG_SNAP);
    CHECK(G.s.p[a].group == G.s.p[b].group);
    jig_rotate(&G, a);                          /* turned again: b to a's left */
    CHECK(G.s.p[a].rot == 2 && G.s.p[b].rot == 2);
    CHECK(fabsf(G.s.p[b].x - 19) < 1e-4f && fabsf(G.s.p[b].y - 20) < 1e-4f);
    jig_rotate(&G, a);
    jig_rotate(&G, a);                          /* upright, b to a's right again */
    CHECK(G.s.p[a].rot == 0 && fabsf(G.s.p[b].x - 21) < 1e-4f && fabsf(G.s.p[b].y - 20) < 1e-4f);
    jig_home(&G, a, &x, &y);
    CHECK(jig_drop(&G, a, x, y) & JIG_PLACE);
    CHECK(G.s.placed == 2);
    CHECK(!jig_rotate(&G, a));                  /* placed: no turning */

    /* the hint: a corner first, then the border, each by the pieces placed; counted */
    jig_load(&G, 30);
    int k = jig_hint(&G);
    CHECK(k >= 0 && G.s.p[k].where == JIG_PLACED && G.hints == 1);
    int i = k % G.cols, j = k / G.cols;
    CHECK((i == 0 || i == G.cols - 1) && (j == 0 || j == G.rows - 1));
    for (int h = 0; h < 2 * (G.cols + G.rows) - 5; h++) {
        k = jig_hint(&G);
        CHECK(k >= 0 && jig_is_edge(&G, k));
    }
    int guard = 0;
    while (!jig_solved(&G) && guard++ < 400) CHECK(jig_hint(&G) >= 0);
    CHECK(jig_solved(&G) && jig_hint(&G) == -1);
    CHECK(jig_undo(&G) && !jig_solved(&G) && G.hints == G.n);   /* undo keeps the hints counted */
}

static void test_stars(void)
{
    jig_load(&G, 0);
    int t = jig_target_s(&G);
    CHECK(t == G.n * 8);
    CHECK(jig_stars(&G, t) == 3 && jig_stars(&G, t + 1) == 2 && jig_stars(&G, 2 * t) == 2);
    CHECK(jig_stars(&G, 2 * t + 1) == 1);
    G.hints = 1;
    CHECK(jig_stars(&G, 10) == 2 && jig_stars(&G, t + 1) == 2 && jig_stars(&G, 2 * t + 1) == 1);
    G.hints = 3;
    CHECK(jig_stars(&G, 0) == 1);
    jig_new(&G, 0, 0, 1, 1);
    CHECK(jig_target_s(&G) == G.n * 12);
}

static void test_script(void)
{
    jig_load(&G, 5);
    CHECK(jig_script(&G, "p0 p1 m7@20,20 m8@21,20"));
    CHECK(G.s.placed == 2 && G.s.p[7].group == G.s.p[8].group);
    CHECK(jig_script(&G, "t3@0") == 0);         /* in the tray already */
    CHECK(jig_script(&G, "u u"));
    CHECK(G.s.p[8].where == JIG_TRAY);
    CHECK(jig_script(&G, "E"));
    for (int k = 0; k < G.n; k++) CHECK(!jig_is_edge(&G, k) || G.s.p[k].where == JIG_PLACED);
    CHECK(jig_script(&G, "h"));
    CHECK(G.hints == 1);
    CHECK(jig_script(&G, "N500") && jig_solved(&G));
    CHECK(!jig_script(&G, "x1"));
    jig_new(&G, 2, 1, 1, 3);                    /* turned: p turns it upright first */
    CHECK(jig_script(&G, "p4") && G.s.p[4].where == JIG_PLACED);
}

static void test_random(void)
{
    clock_t t0 = clock();
    int pics[256] = {0};
    for (uint32_t seed = 1; seed <= 300; seed++) {
        int v = (int)(seed % JIG_VARIANTS);
        jig_random(&G, v, seed);
        CHECK(G.count == v / 2 && G.rotate == v % 2);
        CHECK(G.n <= JIG_MAX && G.s.ntray == G.n);
        if (G.pic < 256) pics[G.pic]++;
        if (seed % 25 == 0) check_cut(&G, 0);
        place_all(&G);
        CHECK(jig_solved(&G));
    }
    int seen = 0;
    for (int p = 0; p < jig_pictures(); p++) seen += pics[p] > 0;
    CHECK(seen > jig_pictures() / 2);           /* the pictures drawn at random: many of them */
    jig_game *g = malloc(sizeof *g);
    clock_t t1 = clock();
    for (int i = 0; i < 20; i++) jig_new(g, 50, 4, 1, (uint32_t)i);   /* the biggest: quick */
    float ms = 1000.0f * (clock() - t1) / CLOCKS_PER_SEC / 20;
    CHECK(ms < 20);
    free(g);
    printf("jigsaw: 300 random puzzles solved, %d pictures drawn, %.2f ms a 150-piece cut (%.1f s in all)\n", seen,
           ms, (double)(clock() - t0) / CLOCKS_PER_SEC);
}

int main(void)
{
    test_pictures();
    test_grids();
    test_levels();
    test_cut();
    test_play();
    test_stars();
    test_script();
    test_random();
    printf("jigsaw: %d pictures, %d levels in %d chapters\n", jig_pictures(), jig_levels(), jig_chapters());
    if (fails) {
        printf("jigsaw: %d failures\n", fails);
        return 1;
    }
    printf("jigsaw: ok\n");
    return 0;
}
