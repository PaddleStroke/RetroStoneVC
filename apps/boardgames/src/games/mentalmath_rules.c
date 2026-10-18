/*
 * Mental Math: the rules (mentalmath.h): the sums, the drills made from a spec, the levels as text, the typing.
 * No SDL: tested by tests/mentalmath_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/mentalmath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

static const char SIGN[] = " +-*/";

int mmh_answer(const mmh_q *q) { return q->n[q->miss < 3 ? q->miss : 2]; }

int mmh_digits(int v)
{
    int n = 1;
    for (v = v < 0 ? -v : v; v >= 10; v /= 10) n++;
    return n;
}

int mmh_check(const mmh_q *q)
{
    int a = q->n[0], b = q->n[1], c = q->n[2];
    if (a < 0 || b < 0 || c < 0 || q->miss > 2) return 0;
    switch (q->op) {
    case MMH_ADD: return a + b == c;
    case MMH_SUB: return a - b == c;
    case MMH_MUL: return a * b == c;
    case MMH_DIV: return b > 0 && b * c == a;
    }
    return 0;
}

/* ---- a drill ----------------------------------------------------------------------------------------------------- */
static int between(uint32_t *s, int lo, int hi) { return hi <= lo ? lo : lo + pr_below(s, hi - lo + 1); }

static int table(const mmh_spec *sp, uint32_t *s)
{
    int t[16], n = 0;
    for (int k = 1; k <= 12; k++)
        if (sp->tables >> k & 1) t[n++] = k;
    return n ? t[pr_below(s, n)] : 2;
}

/* a sum of the spec */
static void one(const mmh_spec *sp, uint32_t *s, mmh_q *q)
{
    int ops[4], n = 0;
    for (int k = MMH_ADD; k <= MMH_DIV; k++)
        if (sp->ops >> k & 1) ops[n++] = k;
    q->op = (uint8_t)(n ? ops[pr_below(s, n)] : MMH_ADD);
    int a, b;
    switch (q->op) {
    case MMH_ADD: {
        int c;
        if (sp->form == 1) c = 10 * between(s, (sp->add_lo + 9) / 10, sp->add_hi / 10), a = 10 * between(s, 1, c / 10 - 1);
        else if (sp->form == 2) c = between(s, sp->add_lo, sp->add_hi), a = between(s, 1, 9);
        else if (sp->form == 3) c = between(s, sp->add_lo, sp->add_hi), a = between(s, 10, c - 10);
        else c = between(s, sp->add_lo, sp->add_hi), a = between(s, 1, c - 1);
        if (a >= c) a = c - 1;
        if (a < 1) a = 1, c = c < 2 ? 2 : c;
        b = c - a;
        if (pr_below(s, 2)) { int t = a; a = b; b = t; }
        q->n[0] = (int16_t)a, q->n[1] = (int16_t)b, q->n[2] = (int16_t)(a + b);
        break;
    }
    case MMH_SUB: {
        a = between(s, sp->sub_lo, sp->sub_hi);
        if (sp->form == 1) a = 10 * between(s, (sp->sub_lo + 9) / 10, sp->sub_hi / 10), b = 10 * between(s, 1, a / 10);
        else if (sp->form == 2) b = between(s, 1, 9);
        else if (sp->form == 3) b = between(s, 10, a - 10);
        else b = between(s, 1, a);
        if (b > a) b = a;
        q->n[0] = (int16_t)a, q->n[1] = (int16_t)b, q->n[2] = (int16_t)(a - b);
        break;
    }
    case MMH_MUL:
        a = table(sp, s), b = between(s, 1, sp->times_hi);
        if (pr_below(s, 2)) { int t = a; a = b; b = t; }
        q->n[0] = (int16_t)a, q->n[1] = (int16_t)b, q->n[2] = (int16_t)(a * b);
        break;
    default:
        b = table(sp, s), a = between(s, 1, sp->times_hi);
        q->n[0] = (int16_t)(a * b), q->n[1] = (int16_t)b, q->n[2] = (int16_t)a;
        break;
    }
    q->miss = (uint8_t)(pr_below(s, 100) < sp->missing ? pr_below(s, 2) : 2);
}

void mmh_drill(const mmh_spec *sp, uint32_t seed, mmh_game *g)
{
    uint32_t s = pr_seed(seed);
    memset(g, 0, sizeof *g);
    int count = sp->count < 1 ? 1 : sp->count > MMH_MAXQ ? MMH_MAXQ : sp->count;
    for (int i = 0, tries = 0; i < count; tries++) {
        mmh_q q;
        one(sp, &s, &q);
        int bad = !mmh_check(&q) || (i && mmh_answer(&g->q[i - 1]) == mmh_answer(&q) && tries < 400);
        for (int j = 0; j < i && !bad && tries < 400; j++)   /* no sum twice */
            bad = g->q[j].op == q.op && g->q[j].n[0] == q.n[0] && g->q[j].n[1] == q.n[1];
        if (bad && tries < 2000) continue;
        g->q[i++] = q;
    }
    g->nq = count;
    g->par = (count * sp->tenths + 9) / 10;
}

mmh_spec mmh_variant_spec(int variant)
{
    static const mmh_spec V[MMH_VARIANTS] = {
        {1u << MMH_ADD, 2, 20, 0, 0, 0, 0, 0, 15, 12, 50},
        {1u << MMH_SUB, 0, 0, 2, 20, 0, 0, 0, 15, 12, 50},
        {1u << MMH_ADD | 1u << MMH_SUB, 20, 100, 20, 100, 0, 0, 0, 10, 12, 70},
        {1u << MMH_MUL, 0, 0, 0, 0, 0, 0x7fc, 10, 15, 15, 40},
        {1u << MMH_DIV, 0, 0, 0, 0, 0, 0x7fc, 10, 10, 15, 50},
        {1u << MMH_ADD | 1u << MMH_SUB | 1u << MMH_MUL | 1u << MMH_DIV, 5, 50, 5, 50, 0, 0x7fc, 10, 20, 15, 55},
    };
    return V[variant < 0 || variant >= MMH_VARIANTS ? 0 : variant];
}

/* ---- the levels as text ------------------------------------------------------------------------------------------ */
int mmh_encode(const mmh_game *g, char *out, int size)
{
    int len = snprintf(out, (size_t)size, "%d:", g->par);
    for (int i = 0; i < g->nq; i++) {
        const mmh_q *q = &g->q[i];
        int k = snprintf(out + len, (size_t)(size - len), "%s%s%d%c%s%d=%s%d", i ? "," : "", q->miss == 0 ? "?" : "",
                         q->n[0], SIGN[q->op], q->miss == 1 ? "?" : "", q->n[1], q->miss == 2 ? "?" : "", q->n[2]);
        if (k < 0 || len + k >= size) return 0;
        len += k;
    }
    return len;
}

static int number(const char **s, int *v, int *miss)
{
    char *e;
    *miss = **s == '?';
    if (*miss) (*s)++;
    long x = strtol(*s, &e, 10);
    if (e == *s || x < 0 || x > 9999) return 0;
    *v = (int)x;
    *s = e;
    return 1;
}

int mmh_decode(const char *s, mmh_game *g)
{
    memset(g, 0, sizeof *g);
    char *e;
    long par = strtol(s, &e, 10);
    if (*e != ':' || par < 1) return 0;
    g->par = (int)par;
    s = e + 1;
    while (*s && g->nq < MMH_MAXQ) {
        mmh_q *q = &g->q[g->nq];
        int v[3], m[3];
        const char *sign;
        if (!number(&s, &v[0], &m[0]) || !*s || !(sign = strchr(SIGN + 1, *s))) return 0;
        s++;
        if (!number(&s, &v[1], &m[1]) || *s++ != '=' || !number(&s, &v[2], &m[2])) return 0;
        if (m[0] + m[1] + m[2] != 1) return 0;
        q->op = (uint8_t)(sign - SIGN);
        q->miss = (uint8_t)(m[0] ? 0 : m[1] ? 1 : 2);
        for (int k = 0; k < 3; k++) q->n[k] = (int16_t)v[k];
        if (!mmh_check(q)) return 0;
        g->nq++;
        if (*s == ',') s++;
        else if (*s) return 0;
    }
    return g->nq > 0 && !*s;
}

/* ---- the typing -------------------------------------------------------------------------------------------------- */
int mmh_done(const mmh_game *g) { return g->nq > 0 && g->cur >= g->nq; }

int mmh_type(mmh_game *g, int d)
{
    if (mmh_done(g) || d < 0 || d > 9) return MMH_MORE;
    const mmh_q *q = &g->q[g->cur];
    int ans = mmh_answer(q), need = mmh_digits(ans);
    if (g->ndig == 1 && g->typed == 0) g->ndig = 0;   /* a lone 0 typed first gives way */
    g->typed = g->typed * 10 + d;
    g->ndig++;
    if (g->typed == ans && g->ndig >= need) {
        g->cur++;
        g->typed = g->ndig = 0;
        return MMH_RIGHT;
    }
    if (g->ndig >= need) {
        g->mistakes++;
        if (g->slips[g->cur] < 250) g->slips[g->cur]++;
        g->typed = g->ndig = 0;
        return MMH_WRONG;
    }
    return MMH_MORE;
}

int mmh_back(mmh_game *g)
{
    if (!g->ndig || mmh_done(g)) return 0;
    g->typed /= 10;
    g->ndig--;
    return 1;
}

int mmh_hint(mmh_game *g)
{
    if (mmh_done(g)) return 0;
    g->hints++;
    if (g->slips[g->cur] < 250) g->slips[g->cur]++;
    g->cur++;
    g->typed = g->ndig = 0;
    return MMH_RIGHT;
}

int mmh_stars(const mmh_game *g, int tenths)
{
    int t = tenths <= g->par * 10 ? 3 : tenths * 2 <= g->par * 30 ? 2 : 1;
    int slips = g->hints + g->mistakes, cap = !slips ? 3 : slips <= 2 ? 2 : 1;
    return t < cap ? t : cap;
}

int mmh_script(mmh_game *g, const char *s)
{
    while (*s) {
        char c = *s++;
        if (c == ' ' || c == ',') continue;
        if (c >= '0' && c <= '9') mmh_type(g, c - '0');
        else if (c == 'b') { if (!mmh_back(g)) return 0; }
        else if (c == 'h') { if (!mmh_hint(g)) return 0; }
        else if (c == '=' || c == '+') {
            int k = 1;
            if (c == '+') for (k = 0; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (; k > 0 && !mmh_done(g); k--) {
                g->typed = g->ndig = 0;
                char d[8];
                int n = snprintf(d, sizeof d, "%d", mmh_answer(&g->q[g->cur]));
                for (int i = 0; i < n; i++) mmh_type(g, d[i] - '0');
            }
        } else {
            return 0;
        }
    }
    return 1;
}

/* ---- the clock --------------------------------------------------------------------------------------------------- */
static int away(const mmh_clock *c, uint32_t real) { return c->last && real - c->real > MMH_AWAY_MS; }

void mmh_clock_start(mmh_clock *c, uint32_t now, uint32_t real)
{
    memset(c, 0, sizeof *c);
    c->on = 1;
    c->t0 = c->last = now;
    c->real = real;
}

void mmh_clock_frame(mmh_clock *c, uint32_t now, uint32_t real)
{
    if (!c->on || c->over) return;
    if (away(c, real)) c->t0 += now - c->last;  /* away a while: the clock waited */
    c->last = now;
    c->real = real;
}

uint32_t mmh_clock_ms(const mmh_clock *c, uint32_t now, uint32_t real)
{
    if (!c->on) return 0;
    if (c->over) return c->end - c->t0;
    return (away(c, real) ? c->last : now) - c->t0;
}

void mmh_clock_stop(mmh_clock *c, uint32_t now, uint32_t real)
{
    if (!c->on || c->over) return;
    mmh_clock_frame(c, now, real);
    c->end = now;
    c->over = 1;
}
