/*
 * Map Quiz's tests: every map loads and its fills cover its outlines, every place has a name in every language, no
 * quiz asks a disputed area, a tap on a region's marker point finds that region, every level's regions are valid, and
 * the quiz counts its prompts, misses and stars.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/mapquiz.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                                      printf("\n"); } } while (0)

static const char *LANGS[] = {"en", "cs", "da", "de", "el", "es", "fi", "fr", "id", "it", "ja", "ko", "nb", "nl", "pl",
                              "pt_BR", "pt_PT", "ru", "sv", "tr", "uk", "zh_CN"};

static double ring_area(const mq_map *m, const mq_ring *R)
{
    double a = 0;
    const float *p = m->rp + 2 * R->first;
    for (int i = 0, k = R->count - 1; i < R->count; k = i++) a += (double)p[2 * k] * p[2 * i + 1] - (double)p[2 * i] * p[2 * k + 1];
    return a / 2;
}

static void test_maps(void)
{
    clock_t t0 = clock();
    for (int mi = 0; mi < MQ_MAPS; mi++) {
        mq_map m;
        CHECK(mq_map_load(&m, mi), "map %d loads", mi);
        const mq_map_def *d = mq_def(mi);
        CHECK(m.nreg == d->nregions && m.nreg <= MQ_MAX_REG, "%s: its regions", d->id);
        int bad = 0, specks = 0, hits = 0, asked = 0;
        for (int r = 0; r < m.nreg; r++) {
            const mq_reg *g = &m.reg[r];
            double rings = 0, tris = 0;
            for (int j = 0; j < g->nring; j++)
                if (m.ring[g->ring0 + j].outer) rings += fabs(ring_area(&m, &m.ring[g->ring0 + j]));
            for (int i = g->i0; i < g->i0 + g->ni; i += 3) {
                const float *a = m.rp + 2 * m.fi[i], *b = m.rp + 2 * m.fi[i + 1], *c = m.rp + 2 * m.fi[i + 2];
                tris += fabs((double)(b[0] - a[0]) * (c[1] - a[1]) - (double)(b[1] - a[1]) * (c[0] - a[0])) / 2;
            }
            if (rings > 0 && fabs(tris - rings) > 0.02 * rings + 50) {
                bad++;
                printf("  %s %s: triangles %.0f, outline %.0f\n", d->id, g->code, tris, rings);
                if (getenv("MQ_DEBUG"))         /* each ring's own */
                    for (int j = 0; j < g->nring; j++) {
                        const mq_ring *R = &m.ring[g->ring0 + j];
                        if (!R->outer) continue;
                        int tri[3 * 4096];
                        if (R->count > 4096) continue;
                        double t = 0;
                        int k = mq_earclip(m.rp, R->first, R->count, tri);
                        for (int i = 0; i < k; i += 3) {
                            const float *a = m.rp + 2 * tri[i], *b = m.rp + 2 * tri[i + 1], *c = m.rp + 2 * tri[i + 2];
                            t += fabs((double)(b[0] - a[0]) * (c[1] - a[1]) - (double)(b[1] - a[1]) * (c[0] - a[0])) / 2;
                        }
                        double ar = fabs(ring_area(&m, R));
                        if (fabs(t - ar) > 0.02 * ar + 50) printf("    ring %d: %d points, area %.0f, triangles %.0f\n", j,
                                                                  R->count, ar, t);
                    }
            }
            if (!g->nring) specks++;
            /* its marker point finds it (a speck, a few grid units wide: by its marker only, the game's) */
            if (g->kind == MQ_ASK && g->nring && g->area > 2500) {
                asked++;
                int h = mq_hit(&m, g->lx, g->ly, 8);   /* a finger's slack: 8 units */
                if (h == r) hits++;
                else
                    printf("  %s %s: its point (%.0f, %.0f) finds %s (its box %.0f, %.0f .. %.0f, %.0f)\n", d->id,
                           g->code, g->lx, g->ly, h >= 0 ? m.reg[h].code : "the sea", g->x0, g->y0, g->x1, g->y1);
            }
            CHECK(g->lx >= 0 && g->ly >= 0 && g->lx <= m.w && g->ly <= m.h, "%s %s: its point on the map", d->id,
                  g->code);
            CHECK(strncmp(g->code, "D_", 2) || g->kind == MQ_DISPUTED, "%s %s: disputed", d->id, g->code);
            CHECK(g->kind != MQ_DISPUTED || g->name < 0, "%s %s: a disputed area has no name", d->id, g->code);
            CHECK(g->kind == MQ_DISPUTED || (g->name >= 0 && g->name < MQ_NAMES), "%s %s: a name", d->id, g->code);
        }
        CHECK(bad <= 1, "%s: %d regions' triangles do not cover them", d->id, bad);
        CHECK(hits == asked, "%s: %d of %d asked regions found at their point", d->id, hits, asked);
        /* the map's set: asked regions only */
        int16_t set[MQ_MAX_REG];
        int n = mq_set(&m, NULL, set);
        CHECK(n > 0, "%s: its set", d->id);
        for (int i = 0; i < n; i++)
            CHECK(m.reg[set[i]].kind == MQ_ASK, "%s: %s in its set", d->id, m.reg[set[i]].code);
        printf("%-10s regions %3d (asked %3d, set %3d, specks %2d)  rings %4d  points %6d  triangles %6d\n", d->id,
               m.nreg, asked, n, specks, m.nring, m.nrp, m.nfi / 3);
        mq_map_free(&m);
    }
    printf("all maps loaded in %.0f ms\n", 1000.0 * (clock() - t0) / CLOCKS_PER_SEC);
}

static void test_levels(void)
{
    mq_map m;
    int loaded = -1, total = 0;
    for (int l = 0; l < MQ_LEVELS; l++) {
        const mq_level_def *L = mq_level(l);
        if (L->map != loaded) {
            if (loaded >= 0) mq_map_free(&m);
            CHECK(mq_map_load(&m, L->map), "level %d's map", l + 1);
            loaded = L->map;
        }
        int16_t set[MQ_MAX_REG];
        int n = mq_set(&m, L->codes, set);
        CHECK(n > 0, "level %d (%s): its regions \"%s\"", l + 1, mq_def(L->map)->id, L->codes);
        for (int i = 0; i < n; i++) {
            CHECK(m.reg[set[i]].kind == MQ_ASK, "level %d: %s is asked", l + 1, m.reg[set[i]].code);
            for (int j = 0; j < i; j++) CHECK(set[i] != set[j], "level %d: %s twice", l + 1, m.reg[set[i]].code);
        }
        CHECK(l == 0 || mq_chapter_of(l) >= mq_chapter_of(l - 1), "level %d: its chapter", l + 1);
        total += n;
    }
    if (loaded >= 0) mq_map_free(&m);
    CHECK(mq_chapter_of(MQ_LEVELS - 1) == MQ_CHAPTERS - 1, "the last chapter");
    printf("%d levels, %d chapters, %d prompts\n", MQ_LEVELS, MQ_CHAPTERS, total);
}

/* help/<lang>/mapquiz_words.txt: a title, the prompt (with %s), then a name a line, none empty */
static void test_names(void)
{
    for (size_t k = 0; k < sizeof LANGS / sizeof LANGS[0]; k++) {
        char path[256], line[512];
        snprintf(path, sizeof path, "apps/boardgames/help/%s/mapquiz_words.txt", LANGS[k]);
        FILE *f = fopen(path, "rb");
        CHECK(f != NULL, "%s", path);
        if (!f) continue;
        int n = 0, empty = 0, prompt = 0;
        while (fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (n == 1) prompt = strstr(line, "%s") != NULL && !strchr(strstr(line, "%s") + 2, '%');
            if (n >= 2 && !line[0]) empty++;
            n++;
        }
        fclose(f);
        CHECK(prompt, "%s: the prompt has one %%s", LANGS[k]);
        CHECK(n == MQ_NAMES + 2, "%s: %d lines, %d names", LANGS[k], n, MQ_NAMES);
        CHECK(!empty, "%s: %d names empty", LANGS[k], empty);
        snprintf(path, sizeof path, "apps/boardgames/help/%s/mapquiz_variants.txt", LANGS[k]);
        f = fopen(path, "rb");
        CHECK(f != NULL, "%s", path);
        if (!f) continue;
        n = 0;
        while (fgets(line, sizeof line, f)) n += line[0] != '\n' && line[0] != '\r';
        fclose(f);
        CHECK(n == MQ_VARIANTS, "%s: %d variants' names", LANGS[k], n);
    }
}

static void test_quiz(void)
{
    mq_map m;
    CHECK(mq_map_load(&m, 1), "europe");
    int16_t set[MQ_MAX_REG];
    int n = mq_set(&m, "FRA DEU ITA ESP GBR", set);
    CHECK(n == 5, "five");
    mq_quiz q;
    mq_quiz_start(&q, &m, set, n, 7);
    int seen[MQ_MAX_REG] = {0};
    for (int i = 0; i < n; i++) seen[q.ask[i]]++;
    for (int i = 0; i < n; i++) CHECK(seen[set[i]] == 1, "each asked once");
    int t = mq_target(&q);
    int other = q.ask[1];
    CHECK(mq_tap(&q, &m, mq_find(&m, "POL")) == MQ_OUT && q.misses == 0, "land not asked: nothing counted");
    CHECK(mq_tap(&q, &m, -1) == MQ_NOTHING, "the sea");
    CHECK(mq_tap(&q, &m, other) == MQ_WRONG && q.misses == 1 && mq_target(&q) == t, "a miss");
    CHECK(mq_tap(&q, &m, t) == MQ_RIGHT && q.state[t] == MQ_FOUND, "found");
    t = mq_target(&q);
    int w = q.ask[0];
    CHECK(mq_tap(&q, &m, w) == MQ_WRONG, "a miss on a found one");
    CHECK(mq_tap(&q, &m, w) == MQ_WRONG, "two");
    CHECK(mq_tap(&q, &m, w) == MQ_GIVEN && q.state[t] == MQ_MISSED && q.given == 1, "three: shown");
    while (!mq_done(&q)) CHECK(mq_tap(&q, &m, mq_target(&q)) == MQ_RIGHT, "the rest");
    CHECK(mq_tap(&q, &m, t) == MQ_NOTHING, "done");
    CHECK(mq_stars(&q, 10000) == 1, "4 misses of 5: one star (%d)", mq_stars(&q, 10000));
    mq_quiz_start(&q, &m, set, n, 7);
    while (!mq_done(&q)) mq_tap(&q, &m, mq_target(&q));
    CHECK(mq_stars(&q, 20000) == 3 && mq_stars(&q, mq_time_limit(5) + 1) == 2, "no miss: 3 stars, slow: 2");
    /* a disputed area is never asked, never a miss */
    int kos = mq_find(&m, "D_KOS"), cri = mq_find(&m, "D_CRIMEA");
    CHECK(kos >= 0 && cri >= 0 && m.reg[kos].kind == MQ_DISPUTED, "Kosovo and Crimea grey");
    mq_quiz_start(&q, &m, set, n, 9);
    CHECK(mq_tap(&q, &m, kos) == MQ_NOTHING && q.misses == 0, "a tap on a disputed area");
    CHECK(mq_set(&m, "FRA D_KOS", set) == 0, "a disputed area in a level: refused");
    mq_map_free(&m);
}

int main(void)
{
    test_maps();
    test_levels();
    test_names();
    test_quiz();
    if (fails) {
        printf("mapquiz: %d FAILED\n", fails);
        return 1;
    }
    printf("mapquiz: all tests passed\n");
    return 0;
}
