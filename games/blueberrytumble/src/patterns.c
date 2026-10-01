/*
 * Blueberry Tumble: the patterns. Each is ONE idea, 1 to 4 bars long, written as a small function that places
 * cells from a parameter vector. Every parameter has a range per tempo tier, so a pattern's whole space is
 * enumerable: tests/validate.c proves every combination completable (windows >= 3 frames) at every tier.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 *
 * Conventions: cell 0 is on a beat; a key obstacle sits one 16th after a beat (cell 4k + 1), so the press it
 * needs falls on the beat; the pattern ends on the ground in normal mode. Rows: 0 stands on the ground.
 */
#include "bt.h"
#include <string.h>

/* ---- builders ---------------------------------------------------------------------------------------------------- */
static void put(pbuild *b, int c, int r, int k)
{
    if (c >= 0 && c < PAT_MAX_LEN && r >= 0 && r < ROWS) b->col[c].cell[r] = (uint8_t)k;
}
static void thorns(pbuild *b, int c, int n) { for (int i = 0; i < n; i++) put(b, c + i, 0, K_THORN); }
static void pillar(pbuild *b, int c, int h, int k) { for (int r = 0; r < h; r++) put(b, c, r, k); }
static void ground(pbuild *b, int c, int n, int g)
{
    for (int i = 0; i < n; i++)
        if (c + i >= 0 && c + i < PAT_MAX_LEN) b->col[c + i].ground = (uint8_t)g;
}
static void flag(pbuild *b, int c, int f) { if (c >= 0 && c < PAT_MAX_LEN) b->col[c].flags |= (uint8_t)f; }
static void cone(pbuild *b, int c, int k256)
{
    if (b->ncone >= PAT_MAX_CONES) return;
    b->cone[b->ncone].meet = c * CELL;          /* on a 16th note */
    b->cone[b->ncone].k256 = (uint8_t)k256;
    b->ncone++;
}
static void coin(pbuild *b, int c, int r) { if (b->coin) put(b, c, r, K_COIN); }
static void end(pbuild *b, int len) { b->len = len; }

/* ---- the micro-patterns: half a bar to a bar, one small idea each, chained closely ----------------------------------- *
 * "sub" is the 16th of the first beat the key obstacle sits on (1: the press falls on the beat, 2 and 3: after it). */

/* thorn: one bush, one or two wide */
static void m_thorn(pbuild *b, const int *v)
{
    int s = v[0], w = v[1];
    thorns(b, s, w);
    end(b, s + w + 3);
}

/* pebbles: a low pile of pebbles, one or two cells */
static void m_pebble(pbuild *b, const int *v)
{
    int s = v[0], n = v[1];
    for (int i = 0; i < n; i++) put(b, s + i, 0, K_PEBBLE);
    end(b, s + n + 3);
}

/* two bushes: their widths and the gap between them (on the beat or off it; one more cell on the fast tiers) */
static void m_two(pbuild *b, const int *v)
{
    static const int gap[4] = {3, 4, 5, 7};
    int s = v[0], g = gap[v[1]] + (b->tier >= 2), w1 = v[2], w2 = v[3];
    thorns(b, s, w1);
    thorns(b, s + w1 + g, w2);
    end(b, s + w1 + g + w2 + 3);
}

/* three: three single things a beat (or a beat and a 16th, or 1.5 beats) apart; one of them may be a pebble pile */
static void m_three(pbuild *b, const int *v)
{
    static const int sp[3] = {4, 5, 6};
    int s = v[0], d = sp[v[1]] + (b->tier >= 3 && v[1] == 0), peb = v[2];
    for (int i = 0; i < 3; i++) put(b, s + i * d, 0, peb == i + 1 ? K_PEBBLE : K_THORN);
    end(b, s + 2 * d + 4);
}

/* row: a long thorn row cleared in one jump */
static void m_row(pbuild *b, const int *v)
{
    int s = v[0], w = v[1];
    thorns(b, s, w);
    end(b, s + w + 3);
}

/* block: hop onto a rock (1 to 4 long), roll off; a thorn just after it, or not */
static void m_block(pbuild *b, const int *v)
{
    int s = v[0], L = v[1], th = v[2];
    for (int i = 0; i < L; i++) pillar(b, s + i, 1, K_BLOCK);
    if (th) thorns(b, s + L + 1, 1);
    coin(b, s + L - 1, 3);
    end(b, s + L + 1 + th * 2 + 3);
}

/* steps: climb two or three rock steps, drop off the top (over a thorn at the bottom, or not) */
static void m_steps(pbuild *b, const int *v)
{
    int n = v[0], L = v[1], drop_thorn = v[2], c = 1;
    for (int s = 0; s < n; s++) {
        for (int i = 0; i < L + (s == n - 1); i++) pillar(b, c + i, s + 1, K_BLOCK);
        c += L + (s == n - 1);
    }
    if (drop_thorn) thorns(b, c + 1, 1);
    coin(b, c - 1, n + 2);
    end(b, c + 4);
}

/* gap: a crevasse two or three wide */
static void m_gap(pbuild *b, const int *v)
{
    int s = v[0], w = v[1];
    ground(b, s, w, G_GAP);
    end(b, s + w + 3);
}

/* gap and thorn: a crevasse and a bush, in either order, a little apart */
static void m_gapthorn(pbuild *b, const int *v)
{
    int order = v[0], sp = 3 + v[1] + (b->tier >= 2), w = v[2];
    if (order == 0) {
        ground(b, 1, w, G_GAP);
        thorns(b, 1 + w + sp, 1);
        end(b, 1 + w + sp + 4);
    } else {
        thorns(b, 1, 1);
        ground(b, 2 + sp, w, G_GAP);
        end(b, 2 + sp + w + 3);
    }
}

/* pillar: hop onto a rock pillar (one or two high) over thorns, hop off over the thorns behind it */
static void m_pillar(pbuild *b, const int *v)
{
    int h = v[0], before = v[1], after = v[2];
    thorns(b, 1, before);
    pillar(b, 1 + before, h, K_BLOCK);
    thorns(b, 2 + before, after);
    if (after == 2) coin(b, 1 + before, h + 2);     /* (over three thorns the hop off the top comes too early) */
    end(b, 2 + before + after + 3);
}

/* two logs with a thorn pit between them (forest and below) */
static void m_logpit(pbuild *b, const int *v)
{
    int L1 = v[0], pit = v[1], L2 = v[2], c = 1;
    for (int j = 0; j < L1; j++) put(b, c + j, 0, K_LOG);
    c += L1;
    thorns(b, c, pit);
    c += pit;
    for (int j = 0; j < L2; j++) put(b, c + j, 0, K_LOG);
    c += L2;
    end(b, c + 3);
}

/* pad: a mushroom launches the berry over a tall wall or a long thorn field (no press needed) */
static void m_pad(pbuild *b, const int *v)
{
    int kind = v[0], size = v[1];
    put(b, 1, 0, K_PAD);
    if (kind == 0) pillar(b, 4, 1 + size, K_BLOCK);
    else thorns(b, 2, 1 + size);                /* right after the pad: the launch clears it */
    if (kind == 0) coin(b, 3, 4);
    end(b, 12);
}

/* dew drop: jump into a crevasse's air and press on the drop to jump again */
static void m_orb(pbuild *b, const int *v)
{
    int w = v[0], far = v[1];
    ground(b, 2, w, G_GAP);
    put(b, 2 + w / 2, 2, K_ORB);
    if (far) thorns(b, 2 + w + 3, 1);           /* a bush on the far side */
    coin(b, 2 + w / 2 + 2, 4);                  /* near the top of the drop's jump */
    end(b, 2 + w + 5);
}

/* cone: one pine cone rolls down at the berry (forest and below) */
static void m_cone(pbuild *b, const int *v)
{
    int s = v[0], fast = v[1];
    cone(b, 2 + s, fast ? CONE_FAST : CONE_SLOW);
    end(b, 2 + s + 4);
}

/* overhang: hanging thorns over the path: do NOT jump under them; a bush just after them, or not */
static void m_overhang(pbuild *b, const int *v)
{
    int L = v[0], th = v[1], d = v[2];
    for (int i = 0; i < L; i++) { put(b, 1 + i, 3, K_BLOCK); put(b, 1 + i, 2, K_HANG); }
    if (th) thorns(b, 1 + L + 1 + d, 1);
    end(b, 1 + L + (th ? 2 + d : 0) + 3);
}

/* ledge: jump onto a rock ledge, hop the bush on it, roll off (over two thorns, or not) */
static void m_ledge(pbuild *b, const int *v)
{
    int L = v[0], th = v[1], down = v[2];
    for (int i = 0; i < L; i++) pillar(b, 1 + i, 1, K_BLOCK);
    if (th) put(b, 1 + L / 2, 1, K_THORN);
    if (down) thorns(b, 1 + L, 2);
    end(b, 1 + L + 5);
}

/* four: four things a beat apart (or a beat and a 16th): a jump on every beat; one of them may be a double bush */
static void m_four(pbuild *b, const int *v)
{
    int s = v[0], d = 4 + v[1], dbl = v[2], c = s;
    for (int i = 0; i < 4; i++) {
        thorns(b, c, dbl == i + 1 ? 2 : 1);
        c += d + (dbl == i + 1);
    }
    end(b, c + 3);
}

/* rock between bushes: hop over a bush onto a rock, then over the bush behind it */
static void m_rockthorns(pbuild *b, const int *v)
{
    int before = v[0], L = v[1], after = v[2];
    thorns(b, 1, before);
    for (int i = 0; i < L; i++) pillar(b, 1 + before + i, 1, K_BLOCK);
    thorns(b, 1 + before + L, after);
    end(b, 1 + before + L + after + 3);
}

/* two crevasses close together: two quick jumps */
static void m_gapgap(pbuild *b, const int *v)
{
    int w1 = v[0], sp = 2 + v[1], w2 = v[2];
    ground(b, 1, w1, G_GAP);
    ground(b, 1 + w1 + sp, w2, G_GAP);
    end(b, 1 + w1 + sp + w2 + 3);
}

/* ---- the set pieces: longer signature sections, rarer ---------------------------------------------------------------- */
/* logs: roll along fallen logs; thorns in the pits between them */
static void p_logs(pbuild *b, const int *v)
{
    int n = v[0], L = v[1], pit = v[2], c = 1;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < L; j++) put(b, c + j, 0, K_LOG);
        c += L;
        if (i < n - 1) { thorns(b, c, pit); c += pit; }
    }
    coin(b, c - 2, 3);
    end(b, c + 3);
}

/* pillars: hop from rock pillar to rock pillar over thorns (flat, up-down or rising) */
static void p_pillars(pbuild *b, const int *v)
{
    int n = v[0], shape = v[1], c = 1;
    for (int i = 0; i < n; i++) {
        int h = shape == 0 ? 1 : shape == 1 ? 1 + (i & 1) : (i == 0 ? 1 : 2);
        int hn = shape == 0 ? 1 : shape == 1 ? 1 + ((i + 1) & 1) : 2;
        /* the step to the next pillar: up takes 3 cells, down 4, level the spacing (one more on the fast tiers) */
        int s = hn > h ? 3 : hn < h ? 4 : v[2] + (b->tier >= 2);
        pillar(b, c, h, K_BLOCK);
        if (i < n - 1) thorns(b, c + 1, s - 1);
        c += i < n - 1 ? s : 1;
    }
    end(b, c + 2);
}

/* islands: stepping stones across a long crevasse */
static void p_islands(pbuild *b, const int *v)
{
    int n = v[0], s = v[1] + (b->tier >= 3), c = 1;                  /* faster tiers: one more cell */
    ground(b, c, n * s + s - 1, G_GAP);
    for (int i = 0; i < n; i++) ground(b, c + s - 1 + i * s, 1, G_GROUND);
    coin(b, c + s - 1 + (n / 2) * s - s / 2, 2);                     /* over a gap, at the apex of a hop */
    end(b, c + n * s + s + 2);
}

/* pad steps: a pad throws the berry onto a high ledge; roll along it and drop off over thorns */
static void p_padsteps(pbuild *b, const int *v)
{
    int h = v[0], L = v[1], th = v[2];
    put(b, 1, 0, K_PAD);
    for (int i = 0; i < L; i++) pillar(b, 4 + i, h, K_BLOCK);
    if (th) thorns(b, 4 + L, 2);
    coin(b, 4 + L - 1, h + 2);
    end(b, 4 + L + 4);
}

/* dew chain: a rhythm of drops across a long crevasse (level, zigzag or rising) */
static void p_orbchain(pbuild *b, const int *v)
{
    /* the drops one jump apart: a jump lasts 21 frames, 3 cells at the slow tiers, 4 at the fast ones */
    static const int base[NTIERS] = {3, 3, 3, 3, 4};
    int n = v[0], shape = v[1], s = base[b->tier], far = v[2];
    int w = (n + 1) * s;
    ground(b, 2, w, G_GAP);
    for (int i = 0; i < n; i++) {
        int row = shape == 0 ? 2 : shape == 1 ? 2 + (i & 1) : 1 + (i < 3 ? i : 2);
        put(b, 2 + (i + 1) * s - 1, row, K_ORB);
    }
    coin(b, 2 + (n / 2 + 1) * s, 4 + (shape == 1));
    if (far) thorns(b, 2 + w + 3, 1);
    end(b, 2 + w + 5);
}

/* pine cones: cones roll down at the berry; jump each on its beat */
static void p_cones(pbuild *b, const int *v)
{
    int n = v[0], s = v[1], fast = v[2];
    for (int i = 0; i < n; i++) cone(b, 2 + i * s, fast ? CONE_FAST : CONE_SLOW);
    end(b, 2 + (n - 1) * s + 4);
}

/* cone hop: a thorn bush and a cone, in either order */
static void p_conehop(pbuild *b, const int *v)
{
    int order = v[0], s = v[1], w = v[2];
    if (order == 0) { thorns(b, 1, w); cone(b, 1 + s, CONE_SLOW); }
    else { cone(b, 2, CONE_SLOW); thorns(b, 2 + s, w); }
    end(b, 2 + s + w + 3);
}

/* ice: jump onto an icy patch, slide (no grip, no jump) under icicles, jump as soon as it ends */
static void p_ice(pbuild *b, const int *v)
{
    int L = v[0], icicles = v[1], after = v[2];
    thorns(b, 1, 1);
    ground(b, 3, L, G_ICE);
    if (icicles)
        for (int i = 1; i < L - 1; i += 2) { put(b, 3 + i, 3, K_BLOCK); put(b, 3 + i, 2, K_HANG); }
    thorns(b, 3 + L + after, 1);
    end(b, 3 + L + after + 4);
}

/* snow smash: roll through snow, grow into a snowberry, smash a row of small things, wash off in the stream */
static void p_snowsmash(pbuild *b, const int *v)
{
    int n = v[0], rocks = v[1], cones = v[2], c = 8;
    ground(b, 1, 6, G_SNOW);
    for (int i = 0; i < n; i++) {
        if (cones && (i & 1)) cone(b, c, CONE_SLOW);
        else put(b, c, 0, (i & 1) ? K_PEBBLE : K_THORN);
        c += 2;
    }
    for (int i = 0; i < rocks; i++) { pillar(b, c + 1, 1, K_BLOCK); c += 5; }
    ground(b, c + 1, 3, G_WATER);
    end(b, c + 6);
}

/* snow jumps: the heavy snowberry's low jumps over crevasses and rocks */
static void p_snowgaps(pbuild *b, const int *v)
{
    int n = v[0], rock = v[1], s = v[2] + (b->tier <= 1), c = 8;     /* the slow tiers: one more cell */
    ground(b, 1, 6, G_SNOW);
    for (int i = 0; i < n; i++) {
        if (rock && (i & 1)) pillar(b, c, 1, K_BLOCK);
        else ground(b, c, 2, G_GAP);
        c += s;
    }
    ground(b, c, 3, G_WATER);
    end(b, c + 5);
}

/* leaf tunnel: ride the maple leaf between floor thorns and hanging thorns */
static void p_glidetunnel(pbuild *b, const int *v)
{
    int n = v[0], H = v[1], first = v[2], c = 5;
    flag(b, 1, F_LEAF);
    int len = 5 + n * 6 + 4;
    for (int i = 0; i < len - 2; i++) put(b, 1 + i, H, K_BLOCK);
    for (int i = 0; i < n; i++) {
        if (((i + first) & 1) == 0) thorns(b, c, 2);
        else { put(b, c, H - 1, K_HANG); put(b, c + 1, H - 1, K_HANG); }
        c += 6;
    }
    coin(b, c - 3, H - 2);
    flag(b, len - 3, F_LEAF_END);
    end(b, len + 2);
}

/* leaf weave: glide through openings in log fences, low and high in turn */
static void p_glideweave(pbuild *b, const int *v)
{
    int n = v[0], open = v[1], s = v[2], c = 6;
    flag(b, 1, F_LEAF);
    for (int i = 0; i < n; i++) {
        int lo = (i & 1) ? 4 : 1;                   /* the opening's bottom row */
        for (int r = 0; r < GLIDE_CEIL / CELL; r++)     /* up to the ceiling: only the openings go through */
            if (r < lo || r >= lo + open) put(b, c, r, K_LOG);
        c += s;
    }
    coin(b, c - s + 2, (((n - 1) & 1) ? 4 : 1) + open - 1);    /* just past the last fence, at the top of its opening */
    flag(b, c, F_LEAF_END);
    end(b, c + 5);
}

/* breather: open ground and the scenery, sometimes one pebble; a golden blueberry up in the air */
static void p_breather(pbuild *b, const int *v)
{
    int bars = v[0], pebble = v[1];
    if (pebble) put(b, 9, 0, K_PEBBLE);
    coin(b, 9, 2);
    end(b, bars * BAR_CELLS);
}

#define T5(a) {a, a, a, a, a}
/* name, idea, biomes (bit b: biome b; the night takes any), breather, coin spot, weight (how often the director
 * picks it among those that fit: micro-patterns are common, set pieces rare), parameters, builder */
const pattern_def bt_patterns[] = {
    /* the micro-patterns */
    {"thorn", "one bush, on a 16th of the beat", 15, 0, 0, 8, 2,
     {{"sub", 1, T5(3)}, {"width", 1, T5(2)}}, m_thorn},
    {"pebbles", "a low pile of pebbles", 15, 0, 0, 6, 2,
     {{"sub", 1, T5(3)}, {"count", 1, T5(2)}}, m_pebble},
    {"two", "two bushes, on or off the beat", 15, 0, 0, 8, 4,
     {{"sub", 1, T5(2)}, {"gap", 0, T5(3)}, {"width1", 1, T5(2)}, {"width2", 1, T5(2)}}, m_two},
    {"three", "three things a beat apart", 15, 0, 0, 6, 3,
     {{"sub", 1, T5(2)}, {"spacing", 0, T5(2)}, {"pebble", 0, T5(3)}}, m_three},
    {"row", "a thorn row in one jump", 15, 0, 0, 5, 2,
     {{"sub", 1, T5(2)}, {"width", 2, {2, 2, 3, 3, 3}}}, m_row},
    {"block", "hop onto a rock and off", 15, 0, 1, 7, 3,
     {{"sub", 1, T5(3)}, {"length", 1, T5(4)}, {"thorn", 0, T5(1)}}, m_block},
    {"steps", "climb rock steps, drop off", 15, 0, 1, 5, 3,
     {{"steps", 2, T5(3)}, {"step", 2, T5(3)}, {"thorn", 0, T5(1)}}, m_steps},
    {"gap", "a crevasse", 15, 0, 0, 7, 2,
     {{"sub", 1, T5(3)}, {"width", 2, T5(3)}}, m_gap},
    {"gapthorn", "a crevasse and a bush, either order", 15, 0, 0, 6, 3,
     {{"order", 0, T5(1)}, {"spacing", 0, T5(2)}, {"width", 2, T5(3)}}, m_gapthorn},
    {"pillar", "a pillar over thorns", 15, 0, 1, 5, 3,
     {{"height", 1, T5(2)}, {"before", 0, T5(1)}, {"after", 2, T5(3)}}, m_pillar},
    {"logpit", "two logs and a thorn pit", 14, 0, 0, 5, 3,
     {{"log1", 2, T5(4)}, {"pit", 1, T5(2)}, {"log2", 2, T5(3)}}, m_logpit},
    {"mushroom", "a pad over a wall or thorns", 15, 0, 1, 4, 2,
     {{"kind", 0, T5(1)}, {"size", 1, {1, 1, 2, 2, 3}}}, m_pad},
    {"dewdrop", "a dew drop over a crevasse", 15, 0, 1, 5, 2,
     {{"width", 4, {5, 6, 6, 6, 6}}, {"far", 0, T5(1)}}, m_orb},
    {"cone", "a pine cone rolls at you", 14, 0, 0, 5, 2,
     {{"sub", 0, T5(2)}, {"fast", 0, T5(1)}}, m_cone},
    {"overhang", "hanging thorns: do not jump", 15, 0, 0, 5, 3,
     {{"length", 2, T5(4)}, {"thorn", 0, T5(1)}, {"dist", 0, T5(1)}}, m_overhang},
    {"ledge", "a rock ledge with a bush on it", 15, 0, 0, 5, 3,
     {{"length", 4, T5(6)}, {"thorn", 0, T5(1)}, {"down", 0, T5(1)}}, m_ledge},
    {"conehop", "a thorn bush and a cone", 14, 0, 0, 4, 3,
     {{"order", 0, T5(1)}, {"spacing", 5, T5(8)}, {"width", 1, T5(2)}}, p_conehop},
    {"four", "a jump on every beat", 15, 0, 0, 5, 3,
     {{"sub", 1, T5(2)}, {"offbeat", 0, T5(1)}, {"double", 0, T5(4)}}, m_four},
    {"rockthorns", "over a bush onto a rock, over the next", 15, 0, 0, 5, 3,
     {{"before", 1, T5(2)}, {"length", 1, T5(2)}, {"after", 1, T5(2)}}, m_rockthorns},
    {"gapgap", "two crevasses close together", 15, 0, 0, 5, 3,
     {{"width1", 2, T5(3)}, {"spacing", 0, T5(2)}, {"width2", 2, T5(3)}}, m_gapgap},
    /* the set pieces */
    {"logs", "roll along fallen logs over thorn pits", 14, 0, 1, 2, 3,
     {{"logs", 2, T5(3)}, {"length", 3, T5(5)}, {"pit", 1, T5(2)}}, p_logs},
    {"pillars", "hop from pillar to pillar over thorns", 15, 0, 0, 2, 3,
     {{"count", 2, {4, 4, 4, 4, 3}}, {"shape", 0, T5(2)}, {"spacing", 3, T5(4)}}, p_pillars},
    {"islands", "stepping stones over a long crevasse", 15, 0, 1, 2, 2,
     {{"count", 2, T5(4)}, {"spacing", 3, T5(4)}}, p_islands},
    {"padsteps", "a pad throws the berry onto a ledge", 15, 0, 1, 2, 3,
     {{"height", 2, T5(3)}, {"length", 3, T5(6)}, {"thorns", 0, T5(1)}}, p_padsteps},
    {"dewchain", "a rhythm of dew drops across the void", 15, 0, 1, 2, 3,
     {{"count", 2, {3, 3, 3, 3, 4}}, {"shape", 0, {1, 2, 1, 1, 0}}, {"far", 0, T5(1)}}, p_orbchain},
    {"cones", "pine cones roll down at you", 14, 0, 0, 2, 3,
     {{"count", 2, T5(3)}, {"spacing", 6, T5(10)}, {"fast", 0, T5(1)}}, p_cones},
    {"ice", "an icy patch: no grip, slide, then jump", 1, 0, 0, 3, 3,
     {{"length", 3, T5(6)}, {"icicles", 0, T5(1)}, {"after", 1, T5(2)}}, p_ice},
    {"snowsmash", "grow into a snowberry and smash through", 1, 0, 0, 3, 3,
     {{"smash", 2, T5(5)}, {"rocks", 0, T5(2)}, {"cones", 0, T5(1)}}, p_snowsmash},
    {"snowjumps", "the heavy snowberry's low jumps", 1, 0, 0, 3, 3,
     {{"count", 1, T5(3)}, {"rock", 0, T5(1)}, {"spacing", 4, T5(6)}}, p_snowgaps},
    {"leaftunnel", "ride the leaf between floor and ceiling thorns", 14, 0, 1, 2, 3,
     {{"count", 2, T5(4)}, {"height", 5, T5(7)}, {"first", 0, T5(1)}}, p_glidetunnel},
    {"leafweave", "glide through low and high openings", 14, 0, 1, 2, 3,
     {{"count", 2, T5(4)}, {"open", 3, T5(4)}, {"spacing", 6, T5(8)}}, p_glideweave},
    {"breather", "open ground to breathe", 15, 1, 1, 0, 2,
     {{"bars", 1, T5(2)}, {"pebble", 0, T5(1)}}, p_breather},
};
const int bt_npatterns = (int)(sizeof bt_patterns / sizeof bt_patterns[0]);

const char *pat_name(int pat)
{
    if (pat >= 0 && pat < bt_npatterns) return bt_patterns[pat].name;
    return pat == PAT_GATE ? "gate" : pat == PAT_START ? "start" : "bridge";
}

int pat_ncombo(int pat, int tier)
{
    const pattern_def *d = &bt_patterns[pat];
    int n = 1;
    for (int i = 0; i < d->nparams; i++) {
        int r = d->p[i].hi[tier] - d->p[i].lo + 1;
        if (r <= 0) return 0;
        n *= r;
    }
    return n;
}

void pat_combo_values(int pat, int tier, int combo, int *v)
{
    const pattern_def *d = &bt_patterns[pat];
    for (int i = 0; i < d->nparams; i++) {
        int r = d->p[i].hi[tier] - d->p[i].lo + 1;
        v[i] = d->p[i].lo + combo % r;
        combo /= r;
    }
}

static void pb_clear(pbuild *b, int tier, int coin_on)
{
    memset(b, 0, sizeof *b);
    b->tier = tier;
    b->coin = coin_on;
}

static void round_beats(pbuild *b)
{
    if (b->len < BEAT_CELLS) b->len = BEAT_CELLS;
    b->len = (b->len + BEAT_CELLS - 1) / BEAT_CELLS * BEAT_CELLS;
    if (b->len > PAT_MAX_LEN) b->len = PAT_MAX_LEN;
}

void pat_build(pbuild *b, int pat, int tier, int combo, int coin_on)
{
    int v[PAT_MAX_PARAMS] = {0};
    pb_clear(b, tier, coin_on && bt_patterns[pat].coin_spot);
    pat_combo_values(pat, tier, combo, v);
    bt_patterns[pat].build(b, v);
    round_beats(b);
}

void pat_build_gate(pbuild *b, int biome)
{
    (void)biome;
    pb_clear(b, 0, 0);
    flag(b, 6, F_GATE);
    for (int i = 0; i < BAR_CELLS; i++) b->col[i].flags |= F_BREATH;
    b->len = BAR_CELLS;
}
