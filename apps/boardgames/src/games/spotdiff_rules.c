/*
 * Spot the Difference: the rules (spotdiff.h). The palette and its colour-vision checks, the drawings, the makers of
 * grids and differences, the levels' text, the play.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/spotdiff.h"

/* the Okabe-Ito colours (black a little softened, and white), the wood first (never drawn) */
const uint8_t sdf_rgb[SDF_COLOURS][3] = {
    {0, 0, 0},       {36, 32, 30},   {230, 159, 0},  {86, 180, 233}, {0, 158, 115},
    {240, 228, 66},  {0, 114, 178},  {213, 94, 0},   {204, 121, 167}, {246, 242, 232},
};

/* ---- colours for every eye ------------------------------------------------------------------------------------- */
static float lin(int v)
{
    float c = v / 255.0f;
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

static float luma(int colour)
{
    const uint8_t *c = sdf_rgb[colour];
    return 0.2126f * lin(c[0]) + 0.7152f * lin(c[1]) + 0.0722f * lin(c[2]);
}

int sdf_light_ink(int colour) { return luma(colour) < 0.33f; }

/* how protan, deutan and tritan eyes see linear RGB (Machado, Oliveira and Fernandes 2009, full severity) */
static const float CVD[3][9] = {
    {0.152286f, 1.052583f, -0.204868f, 0.114503f, 0.786281f, 0.099216f, -0.003882f, -0.048116f, 1.051998f},
    {0.367322f, 0.860646f, -0.227968f, 0.280085f, 0.672501f, 0.047413f, -0.011820f, 0.042940f, 0.968881f},
    {1.255528f, -0.076749f, -0.178779f, -0.078411f, 0.930809f, 0.147602f, 0.004733f, 0.691367f, 0.303900f},
};

static float lab_f(float t) { return t > 0.008856f ? cbrtf(t) : 7.787f * t + 16.0f / 116; }

/* a colour as one eye sees it (eye 0 normal, 1..3 the deficiencies), in CIELAB */
static void lab_of(int colour, int eye, float out[3])
{
    const uint8_t *c = sdf_rgb[colour];
    float r = lin(c[0]), g = lin(c[1]), b = lin(c[2]);
    if (eye > 0) {
        const float *m = CVD[eye - 1];
        float r2 = m[0] * r + m[1] * g + m[2] * b, g2 = m[3] * r + m[4] * g + m[5] * b, b2 = m[6] * r + m[7] * g + m[8] * b;
        r = fminf(1, fmaxf(0, r2));
        g = fminf(1, fmaxf(0, g2));
        b = fminf(1, fmaxf(0, b2));
    }
    float x = (0.4124f * r + 0.3576f * g + 0.1805f * b) / 0.95047f, y = 0.2126f * r + 0.7152f * g + 0.0722f * b,
          z = (0.0193f * r + 0.1192f * g + 0.9505f * b) / 1.08883f;
    float fx = lab_f(x), fy = lab_f(y), fz = lab_f(z);
    out[0] = 116 * fy - 16;
    out[1] = 500 * (fx - fy);
    out[2] = 200 * (fy - fz);
}

float sdf_cvd_dist(int a, int b)
{
    float least = 1e9f;
    for (int eye = 0; eye < 4; eye++) {
        float p[3], q[3];
        lab_of(a, eye, p);
        lab_of(b, eye, q);
        float d = sqrtf((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]));
        if (d < least) least = d;
    }
    return least;
}

#define PAIR_LEAST 22.0f                        /* a colour-only change: this far apart for every eye */

int sdf_pair_ok(int a, int b)
{
    return a > 0 && b > 0 && a < SDF_COLOURS && b < SDF_COLOURS && a != b && sdf_cvd_dist(a, b) >= PAIR_LEAST;
}

/* ---- the drawings: 8 x 8 pixel maps ('.' none; k black, o orange, s sky blue, g bluish green, y yellow, b blue,
 * r vermilion, p reddish purple, w white) ----------------------------------------------------------------------- */
static const char *const PIC[SDF_PICS][8] = {
    {"........", ".rr..rr.", "rwrrrrrr", "rrrrrrrr", ".rrrrrr.", "..rrrr..", "...rr...", "........"},   /* heart */
    {"...rr...", "..rrrr..", ".rrrrrr.", "rrrrrrrr", ".wwwwww.", ".wbwwbw.", ".wwwoow.", ".wwwoow."},   /* house */
    {"...gg...", "..gggg..", ".ggrggg.", "gggggggg", ".ggggrg.", "...kk...", "...kk...", "..kkkk.."},   /* tree */
    {"...w....", "...ww...", "...www..", "...wwww.", "...k....", "rrrrrrrr", ".rrrrrr.", "ssssssss"},   /* boat */
    {"o......o", "oo....oo", "oooooooo", "okooooko", "oooooooo", "oooppooo", ".oooooo.", "..oooo.."},   /* cat */
    {"........", "..bbb..b", ".bbbbbbb", "bkbbbbb.", "bbbbbbb.", ".bbbbbbb", "..bbb..b", "........"},   /* fish */
    {"...yy...", ".y.yy.y.", "..yyyy..", "yyyooyyy", "yyyooyyy", "..yyyy..", ".y.yy.y.", "...yy..."},   /* sun */
    {"..pppp..", ".ppyypp.", ".pyyyyp.", ".ppyypp.", "..pppp..", "...gg...", ".gggg...", "...gg..."},   /* flower */
    {"..rrrr..", ".rwrrwr.", "rrrrrrrr", "rwrrrrwr", "..wwww..", "..wwww..", "..wwww..", ".gggggg."},   /* mushroom */
    {"...k....", "...kgg..", ".rrrrrr.", "rrrrrrrr", "rwrrrrrr", "rrrrrrrr", ".rrrrrr.", "..rrrr.."},   /* apple */
    {"...yy...", "...yy...", "yyyyyyyy", ".yyyyyy.", "..yyyy..", ".yyyyyy.", ".yy..yy.", "y......y"},   /* star */
    {"..yyyy..", ".yyy....", "yyy.....", "yyy..w..", "yyy.....", "yyy....w", ".yyy....", "..yyyy.."},   /* moon */
    {"........", "..www...", ".wwwwww.", "wwwwwwww", ".wwwwww.", "........", ".b..b..b", "b..b..b."},   /* rain */
    {"........", ".ooo....", "o...o...", "o...oooo", "o...o.o.", ".ooo..o.", "........", "........"},   /* key */
    {"..w.w...", ".w.w....", "bbbbbb..", "bbbbbbb.", "bwbbbb.b", "bbbbbbb.", ".bbbb...", "kkkkkk.."},   /* cup */
    {"...rr...", ".rrrrrr.", "rrrrrrrr", "r..rr..r", "....k...", "....k...", ".k..k...", "..kk...."},   /* umbrella */
    {"........", "..ss....", ".skss...", "ossss..s", "..sssss.", "..ssss..", "...o.o..", "........"},   /* bird */
    {"pp....pp", "pyp..pyp", "pppkkppp", ".ppkkpp.", ".ppkkpp.", "pppkkppp", "pyp..pyp", "pp....pp"},   /* butterfly */
    {"........", "..rrrr..", ".rssrsr.", "rrrrrrrr", "rrrrrrry", "rrrrrrrr", ".kk..kk.", ".kk..kk."},   /* car */
    {"...ww...", "..wwww..", "..wbbw..", "..wwww..", "..wwww..", ".rwwwwr.", "rr.oo.rr", "...yy..."},   /* rocket */
    {"..wwww..", ".wwwwww.", "wwkwwkww", "wwkwwkww", "wwwwwwww", "wwwkkwww", "wwwwwwww", "ww.ww.ww"},   /* ghost */
    {"..yy....", ".yyky...", ".yyyyoo.", "..yy....", ".yyyyyy.", "yyyyyyyy", ".yyyyyy.", "ssssssss"},   /* duck */
    {"......gg", "....gggg", "..gggggg", ".ggggkgg", ".gggkggg", ".ggkggg.", "..kggg..", ".k......"},   /* leaf */
    {"........", "y..yy..y", "yy.yy.yy", "yyyyyyyy", "yyryyryy", "yyyyyyyy", "pppppppp", "........"},   /* crown */
    {"...yy...", "..yyyy..", ".yyyyyy.", ".ywyyyy.", ".yyyyyy.", "yyyyyyyy", "...oo...", "........"},   /* bell */
    {"..pppp..", ".pwpppp.", ".pppppp.", ".oooooo.", "..oooo..", "..oooo..", "...oo...", "...oo..."},   /* ice cream */
};

int sdf_pic_count(void) { return SDF_PICS; }

static int pic_colour(char ch)
{
    static const char KEY[] = ".kosgybrpw";
    const char *at = strchr(KEY, ch);
    return at && ch ? (int)(at - KEY) : 0;
}

static void pic_colours(int pic, unsigned *used)
{
    *used = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) *used |= 1u << pic_colour(PIC[pic][y][x]);
}

/* the drawing laid on the grid at (ox, oy), mirrored or not; its empty pixels leave what is there */
static void pic_put(sdf_puzzle *p, int pic, int ox, int oy, int mirror)
{
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int c = pic_colour(PIC[pic][y][mirror ? 7 - x : x]), gx = ox + x, gy = oy + y;
            if (c && gx >= 0 && gy >= 0 && gx < p->w && gy < p->h) p->base[gy * p->w + gx] = (uint8_t)c;
        }
}

/* ---- the grids ------------------------------------------------------------------------------------------------- */
/* k colours of the palette by chance, all different (wood among them when wood is set) */
static void pick_colours(uint32_t *s, int k, int wood, int *out)
{
    int all[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    pr_shuffle(s, all, 9);
    for (int i = 0; i < k; i++) out[i] = all[i];
    if (wood) out[k - 1] = 0;
}

/* each cell takes the colour most of its neighbours have (itself counted), with a chance keep of staying */
static void smooth(sdf_puzzle *p, uint32_t *s, int keep)
{
    uint8_t was[SDF_CELLS];
    memcpy(was, p->base, sizeof was);
    for (int y = 0; y < p->h; y++)
        for (int x = 0; x < p->w; x++) {
            if (pr_below(s, 100) < keep) continue;
            int n[SDF_COLOURS] = {0}, best = was[y * p->w + x];
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (xx >= 0 && yy >= 0 && xx < p->w && yy < p->h) n[was[yy * p->w + xx]]++;
                }
            for (int c = 0; c < SDF_COLOURS; c++)
                if (n[c] > n[best]) best = c;
            p->base[y * p->w + x] = (uint8_t)best;
        }
}

void sdf_grid(sdf_puzzle *p, int kind, int w, int h, int pic, uint32_t seed)
{
    uint32_t s = pr_seed(seed);
    memset(p, 0, sizeof *p);
    p->w = w < 4 ? 4 : w > SDF_MAXW ? SDF_MAXW : w;
    p->h = h < 4 ? 4 : h > SDF_MAXH ? SDF_MAXH : h;
    w = p->w;
    h = p->h;
    if (pic < 0 || pic >= SDF_PICS) pic = pr_below(&s, SDF_PICS);
    int col[6];
    switch (kind) {
    case SDF_DRAWING:                           /* the drawing on the bare wood, in the middle */
        pic_put(p, pic, (w - 8) / 2, (h - 8) / 2, pr_below(&s, 2));
        break;
    case SDF_SCENE: {                           /* a sky and the ground under the drawing, a few dots in the sky */
        unsigned used;
        pic_colours(pic, &used);
        static const int SKY[] = {3, 6, 9, 5, 8, 1}, GROUND[] = {4, 5, 2, 6, 1, 7, 9};
        int sky = 0, ground = 0, dots = 0;
        for (int t = 0; t < 40 && !sky; t++) {
            int c = SKY[pr_below(&s, 6)];
            if (!(used & 1u << c)) sky = c;
        }
        for (int t = 0; t < 40 && !ground; t++) {
            int c = GROUND[pr_below(&s, 7)];
            if (!(used & 1u << c) && c != sky) ground = c;
        }
        for (int t = 0; t < 40 && !dots; t++) {
            int c = 1 + pr_below(&s, 9);
            if (c != sky && c != ground) dots = c;
        }
        int horizon = h - 2 - pr_below(&s, h / 4 + 1);
        for (int i = 0; i < w * h; i++) p->base[i] = (uint8_t)(i / w >= horizon ? ground : sky);
        for (int i = 0; i < w * (horizon - 1); i++)
            if (pr_below(&s, 100) < 7) p->base[i] = (uint8_t)dots;
        for (int i = horizon * w; i < w * h; i++)
            if (pr_below(&s, 100) < 10) p->base[i] = (uint8_t)dots;
        int ox = pr_below(&s, w - 7), oy = horizon - 8 + pr_below(&s, 2);
        if (oy < 0) oy = 0;
        if (oy > h - 8) oy = h - 8;
        pic_put(p, pic, ox, oy, pr_below(&s, 2));
        break;
    }
    case SDF_PATTERN: {                         /* a kaleidoscope: a quarter by chance, smoothed, mirrored */
        int k = 3 + pr_below(&s, 3), hw = (w + 1) / 2, hh = (h + 1) / 2;
        pick_colours(&s, k, pr_below(&s, 3) == 0, col);
        for (int i = 0; i < w * h; i++) p->base[i] = (uint8_t)col[pr_below(&s, k)];
        smooth(p, &s, 55);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int qx = x < hw ? x : w - 1 - x, qy = y < hh ? y : h - 1 - y;
                p->base[y * w + x] = p->base[qy * w + qx];
            }
        break;
    }
    case SDF_BLOBS: {                           /* blobs of colour: chance smoothed twice */
        int k = 4 + pr_below(&s, 3);
        pick_colours(&s, k, 0, col);
        for (int i = 0; i < w * h; i++) p->base[i] = (uint8_t)col[pr_below(&s, k)];
        smooth(p, &s, 30);
        smooth(p, &s, 60);
        break;
    }
    default: {                                  /* SDF_NOISE: chance, barely smoothed */
        int k = 4 + pr_below(&s, 2);
        pick_colours(&s, k, 0, col);
        for (int i = 0; i < w * h; i++) p->base[i] = (uint8_t)col[pr_below(&s, k)];
        smooth(p, &s, 85);
        break;
    }
    }
}

/* ---- the differences ------------------------------------------------------------------------------------------- */
int sdf_busy(const sdf_puzzle *p, int i)
{
    int x = i % p->w, y = i / p->w, n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int xx = x + dx, yy = y + dy;
            if ((dx || dy) && xx >= 0 && yy >= 0 && xx < p->w && yy < p->h && p->base[yy * p->w + xx] != p->base[i])
                n++;
        }
    return n;
}

/* a colour among the cell's neighbours other than its own (subtle: one that blends in); 0 when none */
static int neighbour_colour(const sdf_puzzle *p, int i, uint32_t *s, int want_ok)
{
    int x = i % p->w, y = i / p->w, cand[8], n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int xx = x + dx, yy = y + dy;
            if (xx < 0 || yy < 0 || xx >= p->w || yy >= p->h) continue;
            int c = p->base[yy * p->w + xx];
            if (c == p->base[i] || (want_ok && !sdf_pair_ok(p->base[i], c))) continue;
            if (n < 8) cand[n++] = c;
        }
    return n ? cand[pr_below(s, n)] : -1;
}

int sdf_differ(sdf_puzzle *p, int n, float subtle, uint32_t seed)
{
    uint32_t s = pr_seed(seed ^ 0x51f7d1ffu);
    if (n > SDF_MAXD) n = SDF_MAXD;
    p->n = 0;
    int cells = p->w * p->h;
    for (int k = 0; k < n; k++) {
        /* the place: weighed by how busy it is (subtle) or by the drawing (plain), never next to another */
        float wt[SDF_CELLS], total = 0;
        for (int i = 0; i < cells; i++) {
            int near = 0;
            for (int j = 0; j < p->n && !near; j++) {
                int a = p->d[j].at;
                near = abs(a % p->w - i % p->w) <= 1 && abs(a / p->w - i / p->w) <= 1;
            }
            float b = sdf_busy(p, i) / 8.0f;
            wt[i] = near ? 0 : 0.2f + subtle * 3 * b * b + (1 - subtle) * (p->base[i] ? 1.5f : 0.3f);
            total += wt[i];
        }
        if (total <= 0) return 0;
        float r = (pr_next(&s) % 100000) / 100000.0f * total;
        int at = 0;
        while (at < cells - 1 && (r -= wt[at]) > 0) at++;
        while (at > 0 && wt[at] <= 0) at--;
        if (wt[at] <= 0) return 0;
        int c = p->base[at], roll = pr_below(&s, 100), kind = SDF_BOTH;
        if (c && roll < (int)(50 * subtle)) kind = SDF_SYMBOL;
        else if (c && roll < (int)(50 * subtle + 45 * fmaxf(0, subtle - 0.2f))) kind = SDF_COLOUR;
        sdf_diff d = {(int16_t)at, (uint8_t)pr_below(&s, 2), 0, 0};
        if (kind == SDF_SYMBOL) {
            int t = 1 + pr_below(&s, 8);
            d.c = (uint8_t)c;
            d.s = (uint8_t)(t >= c ? t + 1 : t);
        } else if (kind == SDF_COLOUR) {
            int to = pr_below(&s, 100) < (int)(100 * subtle) ? neighbour_colour(p, at, &s, 1) : -1;
            for (int t = 0; t < 60 && to < 0; t++) {
                int x = 1 + pr_below(&s, 9);
                if (sdf_pair_ok(c, x)) to = x;
            }
            if (to < 0) {
                kind = SDF_BOTH;
            } else {
                d.c = (uint8_t)to;
                d.s = (uint8_t)c;
            }
        }
        if (kind == SDF_BOTH) {
            int to = -1;
            if (pr_below(&s, 100) < (int)(100 * subtle)) to = neighbour_colour(p, at, &s, 0);
            if (to < 0 && c && pr_below(&s, 100) < 30) to = 0;   /* a cell emptied */
            for (int t = 0; to < 0 && t < 60; t++) {   /* plain: a colour far from it */
                int x = 1 + pr_below(&s, 9);
                if (x != c && (subtle > 0.5f || sdf_cvd_dist(c, x) > 25 || c == 0)) to = x;
            }
            if (to < 0) to = c == 1 ? 9 : 1;
            d.c = (uint8_t)to;
            d.s = (uint8_t)to;
        }
        p->d[p->n++] = d;
    }
    return p->n == n;
}

void sdf_variant_size(int variant, int *w, int *h)
{
    static const int SIDE[SDF_VARIANTS] = {8, 10, 12, 14};
    *w = *h = SIDE[variant < 0 ? 0 : variant >= SDF_VARIANTS ? SDF_VARIANTS - 1 : variant];
}

void sdf_make_variant(sdf_puzzle *p, int variant, uint32_t seed)
{
    static const int KIND[SDF_VARIANTS] = {SDF_DRAWING, SDF_SCENE, SDF_PATTERN, SDF_BLOBS}, N[] = {3, 4, 5, 7};
    static const float SUBTLE[SDF_VARIANTS] = {0, 0.3f, 0.55f, 0.85f};
    int v = variant < 0 ? 0 : variant >= SDF_VARIANTS ? SDF_VARIANTS - 1 : variant, w, h;
    sdf_variant_size(v, &w, &h);
    for (uint32_t t = 0;; t++) {
        sdf_grid(p, KIND[v], w, h, -1, seed + t * 7919u);
        if (sdf_differ(p, N[v], SUBTLE[v], seed + t * 7919u) && sdf_check(p)) return;
    }
}

float sdf_effort(const sdf_puzzle *p)
{
    float e = 0.02f * p->w * p->h;
    for (int k = 0; k < p->n; k++) {
        static const float KW[SDF_KINDS] = {1, 1.8f, 1.5f};
        int kind = sdf_kind(p, k), i = p->d[k].at;
        float w = KW[kind] * (1 + 0.8f * sdf_busy(p, i) / 8.0f);
        if (kind == SDF_BOTH && (p->d[k].c == 0 || p->base[i] == 0)) w *= 0.8f;   /* a cell emptied or painted */
        e += w;
    }
    return e;
}

/* ---- a puzzle's cells ------------------------------------------------------------------------------------------ */
int sdf_diff_at(const sdf_puzzle *p, int i)
{
    for (int k = 0; k < p->n; k++)
        if (p->d[k].at == i) return k;
    return -1;
}

void sdf_cell(const sdf_puzzle *p, int side, int i, int *c, int *s)
{
    *c = *s = p->base[i];
    int k = sdf_diff_at(p, i);
    if (k >= 0 && p->d[k].side == side) {
        *c = p->d[k].c;
        *s = p->d[k].s;
    }
}

int sdf_kind(const sdf_puzzle *p, int k)
{
    const sdf_diff *d = &p->d[k];
    int b = p->base[d->at];
    return d->c == b ? SDF_SYMBOL : d->s == b && d->c != d->s ? SDF_COLOUR : SDF_BOTH;
}

int sdf_check(const sdf_puzzle *p)
{
    if (p->w < 4 || p->h < 4 || p->w > SDF_MAXW || p->h > SDF_MAXH || p->n < 1 || p->n > SDF_MAXD) return 0;
    for (int i = 0; i < p->w * p->h; i++)
        if (p->base[i] >= SDF_COLOURS) return 0;
    for (int k = 0; k < p->n; k++) {
        const sdf_diff *d = &p->d[k];
        if (d->at < 0 || d->at >= p->w * p->h || d->side > 1 || d->c >= SDF_COLOURS || d->s >= SDF_SYMBOLS) return 0;
        if (sdf_diff_at(p, d->at) != k) return 0;   /* two on one cell */
        int b = p->base[d->at];
        if ((d->c == 0) != (d->s == 0)) return 0;   /* the bare wood carries no symbol, a colour always one */
        if (d->c == b && d->s == b) return 0;       /* no change */
        switch (sdf_kind(p, k)) {
        case SDF_SYMBOL: if (!b) return 0; break;
        case SDF_COLOUR: if (!sdf_pair_ok(b, d->c)) return 0; break;
        default: if (d->c != d->s) return 0; break;
        }
    }
    return 1;
}

/* ---- the levels' text ------------------------------------------------------------------------------------------ */
int sdf_encode(const sdf_puzzle *p, char *out, int size)
{
    int need = 8 + p->w * p->h + 6 * p->n + 1;
    if (size < need) return 0;
    int n = sprintf(out, "%dx%d:", p->w, p->h);
    for (int i = 0; i < p->w * p->h; i++) out[n++] = (char)('0' + p->base[i]);
    out[n++] = ':';
    for (int k = 0; k < p->n; k++)
        n += sprintf(out + n, "%03d%d%d%d", p->d[k].at, p->d[k].side, p->d[k].c, p->d[k].s);
    out[n] = 0;
    return n;
}

int sdf_decode(const char *s, sdf_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int w, h, used;
    if (!s || sscanf(s, "%dx%d:%n", &w, &h, &used) != 2 || w < 4 || h < 4 || w > SDF_MAXW || h > SDF_MAXH) return 0;
    p->w = w;
    p->h = h;
    s += used;
    for (int i = 0; i < w * h; i++, s++) {
        if (*s < '0' || *s > '9') return 0;
        p->base[i] = (uint8_t)(*s - '0');
    }
    if (*s++ != ':') return 0;
    while (*s) {
        for (int j = 0; j < 6; j++)
            if (s[j] < '0' || s[j] > '9') return 0;
        if (p->n >= SDF_MAXD) return 0;
        sdf_diff *d = &p->d[p->n++];
        d->at = (int16_t)((s[0] - '0') * 100 + (s[1] - '0') * 10 + s[2] - '0');
        d->side = (uint8_t)(s[3] - '0');
        d->c = (uint8_t)(s[4] - '0');
        d->s = (uint8_t)(s[5] - '0');
        s += 6;
    }
    return sdf_check(p);
}

/* ---- the play -------------------------------------------------------------------------------------------------- */
void sdf_load(sdf_game *g, const sdf_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
}

int sdf_solved(const sdf_game *g) { return g->p.n > 0 && g->nfound == g->p.n; }

static void found(sdf_game *g, int k, int how)
{
    g->found[k] = (uint8_t)how;
    g->hist[g->nfound++] = (int8_t)k;
}

int sdf_tap(sdf_game *g, int i)
{
    if (i < 0 || i >= g->p.w * g->p.h || sdf_solved(g)) return -2;
    int k = sdf_diff_at(&g->p, i);
    if (k >= 0 && g->found[k]) return -2;
    g->taps++;
    if (k < 0) {
        g->mistakes++;
        return -1;
    }
    found(g, k, 1);
    return k;
}

int sdf_hint(sdf_game *g)
{
    for (int k = 0; k < g->p.n; k++)
        if (!g->found[k]) {
            g->hints++;
            found(g, k, 2);
            return k;
        }
    return -1;
}

int sdf_undo(sdf_game *g)
{
    if (!g->nfound) return -1;
    int k = g->hist[--g->nfound];
    g->found[k] = 0;
    return k;
}

int sdf_script(sdf_game *g, const char *moves)
{
    const char *s = moves;
    int ok = 1;
    while (s && *s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == 'h') {
            if (sdf_hint(g) < 0) ok = 0;
            s++;
        } else if (*s == '+') {
            int n = 0, used = 0;
            if (sscanf(s + 1, "%d%n", &n, &used) != 1) return 0;
            for (int k = 0; k < g->p.n && n > 0; k++)
                if (!g->found[k]) {
                    sdf_tap(g, g->p.d[k].at);
                    n--;
                }
            s += 1 + used;
        } else if (*s >= 'a' && *s < 'a' + g->p.w) {
            int x = *s++ - 'a', y = 0;
            while (*s >= '0' && *s <= '9') y = y * 10 + *s++ - '0';
            if (y < 1 || y > g->p.h) return 0;
            sdf_tap(g, (y - 1) * g->p.w + x);
        } else {
            return 0;
        }
    }
    return ok;
}
