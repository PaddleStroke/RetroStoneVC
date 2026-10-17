/*
 * The party games' tests: their word lists in the 22 languages (help/<lang>/<key>.txt: a title line, then a word a
 * line; <id>_variants.txt: no title), every language with every line, none empty, none repeated. Run from the
 * repository's root (make test).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef PARTY_LISTS_H
#define PARTY_LISTS_H

#include <stdio.h>
#include <string.h>

static const char *const PL_LANGS[22] = {"en", "cs", "da", "de", "el", "es", "fi", "fr", "id", "it", "ja", "ko",
                                         "nb", "nl", "pl", "pt_BR", "pt_PT", "ru", "sv", "tr", "uk", "zh_CN"};

/* the lines after the title in a language's file (-1: no file); *dups: lines seen twice, *empty: empty lines */
static int pl_lines(const char *lang, const char *key, int *dups, int *empty)
{
    char path[256], line[512];
    static char seen[512][512];
    snprintf(path, sizeof path, "apps/boardgames/help/%s/%s.txt", lang, key);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int n = strstr(key, "_variants") ? 0 : -1;  /* (a picker's names: no title line) */
    *dups = *empty = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (n++ < 0) continue;                  /* the title */
        if (!line[0]) (*empty)++;
        for (int i = 0; i < n - 1 && i < 512; i++)
            if (!strcmp(seen[i], line)) (*dups)++;
        if (n - 1 < 512) snprintf(seen[n - 1], sizeof seen[0], "%s", line);
    }
    fclose(f);
    return n < 0 ? 0 : n;
}

/* every language: exactly `want` lines (none empty, none twice unless allowed); returns the failures (printed) */
static int pl_check(const char *key, int want, int allow_dups)
{
    int bad = 0;
    for (int l = 0; l < 22; l++) {
        int dups, empty, n = pl_lines(PL_LANGS[l], key, &dups, &empty);
        if (n != want || empty || (dups && !allow_dups)) {
            printf("FAIL %s/%s: %d lines (want %d), %d empty, %d repeated\n", PL_LANGS[l], key, n, want, empty, dups);
            bad++;
        }
    }
    return bad;
}

/* the lists are there to check (run from the repository's root) */
static int pl_found(const char *key)
{
    int d, e;
    return pl_lines("en", key, &d, &e) >= 0;
}

#endif
