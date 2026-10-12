/*
 * Go's rules for RetroStone Board (go.h): the stones, the captures, the superko, the passes, the counting.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/go.h"

static const char COLS[] = "abcdefghjklmnopqrst";   /* go's letters skip the i */

static uint32_t hash(const uint8_t *pt, int n)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < n * n; i++) h = (h ^ pt[i]) * 16777619u;
    return h;
}

/* the up to four neighbours of p */
static int around(int n, int p, int nb[4])
{
    int x = p % n, y = p / n, k = 0;
    if (x > 0) nb[k++] = p - 1;
    if (x < n - 1) nb[k++] = p + 1;
    if (y > 0) nb[k++] = p - n;
    if (y < n - 1) nb[k++] = p + n;
    return k;
}

/* the group (the stones joined to p's, all of one colour) into grp; its liberties' count */
static int group(int n, const uint8_t *pt, int p, int *grp, int *ngrp)
{
    uint8_t seen[GO_PTS] = {0};
    int c = pt[p], k = 0, libs = 0, nb[4];
    grp[k++] = p;
    seen[p] = 1;
    for (int i = 0; i < k; i++)
        for (int m = around(n, grp[i], nb), j = 0; j < m; j++) {
            int q = nb[j];
            if (seen[q]) continue;
            seen[q] = 1;
            if (pt[q] == c) grp[k++] = q;
            else if (pt[q] == GO_EMPTY) libs++;
        }
    *ngrp = k;
    return libs;
}

void go_new(gogame *g, int n)
{
    memset(g, 0, sizeof *g);
    g->n = n == 9 || n == 13 ? n : 19;
    g->pos[0].hash = hash(g->pos[0].pt, g->n);
}

const gopos *go_now(const gogame *g) { return &g->pos[g->nmoves]; }

int go_last(const gogame *g) { return g->nmoves ? g->move[g->nmoves - 1] : GO_NONE; }

int go_try(const gogame *g, int p, gopos *out)
{
    int n = g->n, grp[GO_PTS], k, nb[4], took = 0;
    const gopos *now = go_now(g);
    if (g->phase != GO_PLAY || g->nmoves >= GO_HIST || p < 0 || p >= n * n || now->pt[p] != GO_EMPTY) return -1;
    *out = *now;
    int me = now->side + 1, foe = 2 - now->side;
    out->pt[p] = (uint8_t)me;
    for (int m = around(n, p, nb), j = 0; j < m; j++)   /* the neighbour groups left without a liberty go */
        if (out->pt[nb[j]] == foe && group(n, out->pt, nb[j], grp, &k) == 0) {
            for (int i = 0; i < k; i++) out->pt[grp[i]] = GO_EMPTY;
            took += k;
        }
    if (group(n, out->pt, p, grp, &k) == 0) return -1;   /* suicide */
    out->hash = hash(out->pt, n);
    for (int i = 0; i <= g->nmoves; i++)        /* superko: no position comes back */
        if (g->pos[i].hash == out->hash && !memcmp(g->pos[i].pt, out->pt, (size_t)(n * n))) return -1;
    out->caps[now->side] += took;
    out->side = !now->side;
    return took;
}

void go_new_capture(gogame *g, int n)
{
    go_new(g, n == 13 ? 13 : 9);
    g->capture = 1;
}

int go_can_play(const gogame *g)
{
    gopos next;
    for (int p = 0; p < g->n * g->n; p++)
        if (go_now(g)->pt[p] == GO_EMPTY && go_try(g, p, &next) >= 0) return 1;
    return 0;
}

int go_play(gogame *g, int p)
{
    gopos next;
    int took = go_try(g, p, &next);
    if (took < 0) return -1;
    g->move[g->nmoves++] = p;
    g->pos[g->nmoves] = next;
    if (g->capture && (took || !go_can_play(g))) g->phase = GO_OVER;   /* the first capture wins; nowhere to play */
    return took;
}

int go_pass(gogame *g)
{
    if (g->capture) return 0;                   /* capture go: no pass */
    if (g->phase == GO_COUNT) { g->phase = GO_OVER; return 1; }
    if (g->phase != GO_PLAY || g->nmoves >= GO_HIST) return 0;
    g->pos[g->nmoves + 1] = g->pos[g->nmoves];
    g->pos[g->nmoves + 1].side = !g->pos[g->nmoves].side;
    g->move[g->nmoves++] = GO_PASS;
    if (g->nmoves >= 2 && g->move[g->nmoves - 2] == GO_PASS) {
        g->phase = GO_COUNT;
        memset(g->dead, 0, sizeof g->dead);
    }
    return 1;
}

int go_undo(gogame *g)
{
    if (g->phase == GO_OVER && g->capture) g->phase = GO_PLAY;   /* capture go: the move that won */
    if (g->phase == GO_OVER) { g->phase = GO_COUNT; return 1; }
    if (g->phase == GO_COUNT) g->phase = GO_PLAY;
    else if (!g->nmoves) return 0;
    g->nmoves--;
    return 1;
}

int go_toggle(gogame *g, int p)
{
    int grp[GO_PTS], k;
    const gopos *now = go_now(g);
    if (g->phase != GO_COUNT || p < 0 || p >= g->n * g->n || now->pt[p] == GO_EMPTY) return 0;
    group(g->n, now->pt, p, grp, &k);
    uint8_t d = !g->dead[p];
    for (int i = 0; i < k; i++) g->dead[grp[i]] = d;
    return 1;
}

void go_score(const gogame *g, int8_t owner[GO_PTS], float score[2])
{
    int n = g->n, nb[4], stack[GO_PTS], region[GO_PTS];
    uint8_t pt[GO_PTS], seen[GO_PTS] = {0};
    const gopos *now = go_now(g);
    for (int p = 0; p < n * n; p++) pt[p] = g->phase != GO_PLAY && g->dead[p] ? GO_EMPTY : now->pt[p];
    score[0] = 0;
    score[1] = GO_KOMI;
    for (int p = 0; p < n * n; p++) {
        if (pt[p] != GO_EMPTY) {
            owner[p] = (int8_t)(pt[p] - 1);
            score[pt[p] - 1] += 1;
            continue;
        }
        if (seen[p]) continue;
        int top = 0, k = 0, touch = 0;          /* an empty region: whose stones does it touch? */
        stack[top++] = p;
        seen[p] = 1;
        while (top) {
            int q = stack[--top];
            region[k++] = q;
            for (int m = around(n, q, nb), j = 0; j < m; j++) {
                int r = nb[j];
                if (pt[r] != GO_EMPTY) touch |= pt[r];
                else if (!seen[r]) { seen[r] = 1; stack[top++] = r; }
            }
        }
        int8_t who = touch == GO_EBONY ? 0 : touch == GO_IVORY ? 1 : -1;
        for (int i = 0; i < k; i++) owner[region[i]] = who;
        if (who >= 0) score[who] += (float)k;
    }
}

int go_winner(const gogame *g)
{
    if (g->phase != GO_OVER) return -1;
    if (g->capture) return !go_now(g)->side;    /* the last to play: they captured, or the other cannot play */
    int8_t owner[GO_PTS];
    float score[2];
    go_score(g, owner, score);
    return score[0] > score[1] ? 0 : 1;
}

int go_point(int n, const char *s, int *len)
{
    *len = 0;
    if (!strncmp(s, "pass", 4)) { *len = 4; return GO_PASS; }
    const char *c = s[0] ? strchr(COLS, s[0] | 0x20) : NULL;
    if (!c || s[1] < '1' || s[1] > '9') return GO_NONE;
    int x = (int)(c - COLS), y = s[1] - '0', k = 2;
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[k++] - '0';
    if (x >= n || y < 1 || y > n) return GO_NONE;
    *len = k;
    return x + (y - 1) * n;
}

int go_script(gogame *g, const char *s)
{
    for (;;) {
        while (*s == ' ') s++;
        if (!*s) return 1;
        int len, p = go_point(g->n, s, &len);
        if (p == GO_NONE || (s[len] && s[len] != ' ')) return 0;
        if (p == GO_PASS ? !go_pass(g) : go_play(g, p) < 0) return 0;
        s += len;
    }
}
