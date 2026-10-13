/*
 * The trick bots' tests (tests/ai_<id>_test.c for the games on games/trickbot.h): what they share. A check line, a
 * view revealed (a test may look at every hand: the deal as the core plays it), a determinised deal checked against
 * its view (the counts, the observer's hand, the known cards, the voids and caps, every card once).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef AI_TRICKS_H
#define AI_TRICKS_H

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/trickbot.h"

static int tt_fails;
static inline void tt_check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    tt_fails += !ok;
}

/* every hand known: hands[s] (n[s] cards) */
static inline void tt_reveal(tb_state *v, const uint8_t *hands, int stride, const uint8_t *n)
{
    for (int s = 0; s < v->players; s++) {
        memcpy(v->hand[s], hands + (size_t)s * stride, n[s]);
        v->n[s] = v->nknown[s] = n[s];
    }
    v->full = 1;
}

/* a determinised deal w of the view v: the counts kept, the known cards where they were, the caps kept, and the
 * cards dealt are the unseen ones (each once) */
static inline int tt_dealt_ok(const tb_state *v, const tb_state *w)
{
    int ok = w->full, cnt[256] = {0};
    for (int s = 0; s < v->players; s++) {
        ok &= w->n[s] == v->n[s];
        for (int i = 0; i < v->nknown[s]; i++) ok &= w->hand[s][i] == v->hand[s][i];
        for (int i = v->nknown[s]; i < w->n[s]; i++) {
            int c = w->hand[s][i];
            cnt[c]++;
            ok &= tb_fits(v, s, c);
        }
    }
    for (int i = v->nstock_known; i < w->nstock; i++) cnt[w->stock[i]]++;
    for (int i = 0; i < w->nrest; i++) cnt[w->rest[i]]++;
    for (int i = 0; i < v->nunseen; i++) cnt[v->unseen[i]]--;
    for (int c = 0; c < 256; c++) ok &= cnt[c] == 0;
    return ok;
}

#ifdef AI_SELFPLAY_H
/* the short run (make test: a few games a pairing) checks the moves, the ends and the time; the scores need the full
 * run's games (card games are luck as well), so their targets apply only with --full */
static inline void tt_targets(sp_pair *P, int n, int argc, char **argv)
{
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    if (!full)
        for (int i = 0; i < n; i++) P[i].min_pct = 0;
}

/* ---- duplicate deals (--full) --------------------------------------------------------------------------------------
 * The luck of the deal outweighs the players' skill over a few dozen games of cards, so the full run plays each deal
 * twice, the levels changing seats (even seats and odd seats), and scores the pairs: what is left is the levels'
 * difference. Every move is still checked (the game's script()), every game must end, every move keep to its level's
 * time. The game's script() takes "D<seed>" as its first action: the deal (the tests' notation). */
typedef struct { int done, ok, score; char err[160]; } tt_res;   /* score: a's, 2 a win, 1 a draw, 0 a loss */

/* the time a move took on the processor (the tests run many games at once, on a machine others use too: the wall
 * clock would count the waits; the search's own ms cap is the wall clock's, in play) */
static inline double tt_cpu_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static inline void tt_game(const sp_game *g, const bot *b, int la, int lb, int swap, uint32_t deal, uint64_t seed,
                           tt_res *out, double *ms_sum, double *ms_max, int *moves)
{
    ai_rng rng;
    ai_seed(&rng, seed);
    g->reset();
    char m[BOT_MOVE_CAP];
    snprintf(m, sizeof m, "D%u", deal);
    if (!g->script(m)) { snprintf(out->err, sizeof out->err, "the deal refused"); out->done = 1; return; }
    for (int ply = 1;; ply++) {
        int res = g->result();
        if (res >= 0) {
            out->score = res == 2 ? 1 : res == swap ? 2 : 0;
            out->ok = 1;
            break;
        }
        if (ply >= g->max_plies) { snprintf(out->err, sizeof out->err, "no end after %d plies", ply); break; }
        int seat = g->actor(), level = ((seat & 1) ^ swap) ? lb : la;
        m[0] = 0;
        if (level == SP_RANDOM) {
            if (!g->random_move(&rng, m, sizeof m)) { snprintf(out->err, sizeof out->err, "no random move"); break; }
        } else {
            ai_set_seed(ai_mix(seed, (uint64_t)ply));
            double t0 = tt_cpu_ms();
            int ok = b->think(seat, level, m, (int)sizeof m);
            double dt = tt_cpu_ms() - t0;
            moves[level]++;
            ms_sum[level] += dt;
            if (dt > ms_max[level]) ms_max[level] = dt;
            if (!ok || !m[0]) {
                snprintf(out->err, sizeof out->err, "ply %d: %s gave no move", ply, sp_name(level));
                break;
            }
            if (dt > ai_level_budget(level).ms + 50) {
                snprintf(out->err, sizeof out->err, "ply %d: %s took %.0f ms", ply, sp_name(level), dt);
                break;
            }
        }
        if (!g->script(m)) { snprintf(out->err, sizeof out->err, "ply %d: \"%s\" refused", ply, m); break; }
    }
    out->done = 1;
}

/* the run: short (make test), sp_run's few games; --full: each pairing's games as duplicate deals. 0: all passed */
static inline int tt_run(const sp_game *g, sp_pair *P, int npairs, int argc, char **argv)
{
    int full = 0, jobs = (int)sysconf(_SC_NPROCESSORS_ONLN) * 3 / 4;
    for (int i = 1; i < argc; i++) {
        full |= !strcmp(argv[i], "--full");
        if (!strcmp(argv[i], "-j") && i + 1 < argc) jobs = atoi(argv[i + 1]);
    }
    if (!full) {
        tt_targets(P, npairs, argc, argv);
        return sp_run(g, P, npairs, argc, argv);
    }
    const bot *b = bot_find(g->id);
    if (!b || !b->think || npairs > 32) { printf("FAIL %s: no bot registered\n", g->id); return 1; }
    int first[33], total = 0;
    for (int i = 0; i < npairs; i++) first[i] = total, total += (P[i].games + 1) / 2 * 2;
    first[npairs] = total;
    typedef struct { tt_res r; double ms_sum[AI_LEVELS], ms_max[AI_LEVELS]; int moves[AI_LEVELS]; } slot;
    slot *R = mmap(NULL, sizeof(slot) * (size_t)total, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int *next = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (R == MAP_FAILED || next == MAP_FAILED) { printf("FAIL mmap\n"); return 1; }
    memset(R, 0, sizeof(slot) * (size_t)total);
    *next = 0;
    if (jobs < 1) jobs = 1;
    double t0 = ai_now_ms();
    fflush(stdout);
    for (int w = 0; w < jobs; w++) {
        pid_t pid = fork();
        if (pid < 0) break;
        if (pid) continue;
        for (;;) {
            int i = __atomic_fetch_add(next, 1, __ATOMIC_SEQ_CST);
            if (i >= total) _exit(0);
            int pi = 0;
            while (i >= first[pi + 1]) pi++;
            int k = i - first[pi], deal_no = k / 2;
            uint64_t s = ai_mix(0xd0b + (uint64_t)pi * 1000, (uint64_t)deal_no);
            tt_game(g, b, P[pi].a, P[pi].b, k & 1, (uint32_t)(s >> 34), s, &R[i].r, R[i].ms_sum, R[i].ms_max,
                    R[i].moves);
        }
    }
    while (wait(NULL) > 0) {}
    printf("%s: %d games, duplicate deals (each deal twice, the levels changing seats), %d at once, %.1f s\n", g->id,
           total, jobs, (ai_now_ms() - t0) / 1000);
    printf("  %-16s %5s %5s %5s %5s %6s  %s\n", "pairing (a-b)", "deals", "a", "b", "draw", "a %",
           "a won both / lost both");
    int fails = 0;
    double lv_ms[AI_LEVELS] = {0}, lv_max[AI_LEVELS] = {0};
    long lv_n[AI_LEVELS] = {0};
    for (int pi = 0; pi < npairs; pi++) {
        int wa = 0, wb = 0, dr = 0, both = 0, none = 0, sum = 0, n = 0;
        for (int i = first[pi]; i < first[pi + 1]; i++) {
            tt_res *r = &R[i].r;
            if (!r->ok) {
                printf("FAIL %s %s-%s game %d: %s\n", g->id, sp_name(P[pi].a), sp_name(P[pi].b), i - first[pi],
                       r->done ? r->err : "the worker died");
                fails++;
                continue;
            }
            wa += r->score == 2, wb += r->score == 0, dr += r->score == 1, sum += r->score, n++;
            if (((i - first[pi]) & 1) && R[i - 1].r.ok) {
                both += r->score == 2 && R[i - 1].r.score == 2;
                none += r->score == 0 && R[i - 1].r.score == 0;
            }
            for (int l = 0; l < AI_LEVELS; l++) {
                lv_n[l] += R[i].moves[l];
                lv_ms[l] += R[i].ms_sum[l];
                if (R[i].ms_max[l] > lv_max[l]) lv_max[l] = R[i].ms_max[l];
            }
        }
        int pct = n ? 50 * sum / n : 0;
        char name[40];
        snprintf(name, sizeof name, "%s-%s", sp_name(P[pi].a), sp_name(P[pi].b));
        printf("  %-16s %5d %5d %5d %5d %5d%%  %d / %d\n", name, (first[pi + 1] - first[pi]) / 2, wa, wb, dr, pct, both,
               none);
        if (pct < P[pi].min_pct) {
            printf("FAIL %s %s: a scored %d%%, under %d%%\n", g->id, name, pct, P[pi].min_pct);
            fails++;
        }
    }
    printf("  %-8s %8s %9s %9s   (processor time)\n", "level", "moves", "ms/move", "max ms");
    for (int l = 0; l < AI_LEVELS; l++)
        if (lv_n[l])
            printf("  %-8s %8ld %9.1f %9.1f   (budget %d ms)\n", sp_name(l), lv_n[l], lv_ms[l] / lv_n[l], lv_max[l],
                   ai_level_budget(l).ms);
    munmap(R, sizeof(slot) * (size_t)total);
    munmap(next, sizeof(int));
    printf("%s: %s\n", g->id, fails ? "FAILED" : "all games legal, ended, in time");
    return fails != 0;
}
#endif

#endif
