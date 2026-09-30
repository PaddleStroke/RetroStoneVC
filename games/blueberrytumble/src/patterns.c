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

/* ---- the patterns --------------------------------------------------------------------------------------------------- */
/* 1. hop: thorn bushes on the beat; the count, the spacing (1, 1.5 or 2 beats) and single or double bushes */
static void p_hop(pbuild *b, const int *v)
{
    static const int sp[3] = {4, 6, 8};
    int n = v[0], s = sp[v[1]], w = v[2];
    for (int i = 0; i < n; i++) thorns(b, 1 + i * s, w);
    end(b, 1 + (n - 1) * s + w + 3);
}

/* 2. rows: long thorn rows (two or three bushes) to clear in one jump */
static void p_rows(pbuild *b, const int *v)
{
    int n = v[0], w = v[1], s = v[2] ? 12 : 8;
    for (int i = 0; i < n; i++) thorns(b, 1 + i * s, w);
    end(b, 1 + (n - 1) * s + w + 3);
}

/* 3. stairs: climb rock steps one block at a time, then drop off the top (over a thorn at the bottom) */
static void p_stairs(pbuild *b, const int *v)
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

/* 4. logs: roll along fallen logs; thorns in the pits between them */
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

/* 5. pillars: hop from rock pillar to rock pillar over thorns (flat, up-down or rising) */
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

/* 6. crevasses: jump the gaps in the mountainside */
static void p_gaps(pbuild *b, const int *v)
{
    int n = v[0], w = v[1], s = v[2], c = 1;
    for (int i = 0; i < n; i++) { ground(b, c, w, G_GAP); c += w + s; }
    end(b, c + 1);
}

/* 7. islands: stepping stones across a long crevasse */
static void p_islands(pbuild *b, const int *v)
{
    int n = v[0], s = v[1] + (b->tier >= 3), c = 1;                  /* faster tiers: one more cell */
    ground(b, c, n * s + s - 1, G_GAP);
    for (int i = 0; i < n; i++) ground(b, c + s - 1 + i * s, 1, G_GROUND);
    coin(b, c + s - 1 + (n / 2) * s - s / 2, 2);                     /* over a gap, at the apex of a hop */
    end(b, c + n * s + s + 2);
}

/* 8. mushroom: a pad launches the berry over a tall wall or a long thorn field */
static void p_mushroom(pbuild *b, const int *v)
{
    int kind = v[0], size = v[1];
    put(b, 1, 0, K_PAD);
    if (kind == 0) pillar(b, 4, 1 + size, K_BLOCK);
    else thorns(b, 2, 1 + size);                /* right after the pad: the launch clears it */
    if (kind == 0) coin(b, 3, 4);
    end(b, 12);
}

/* 9. pad steps: a pad throws the berry onto a high ledge; roll along it and drop off over thorns */
static void p_padsteps(pbuild *b, const int *v)
{
    int h = v[0], L = v[1], th = v[2];
    put(b, 1, 0, K_PAD);
    for (int i = 0; i < L; i++) pillar(b, 4 + i, h, K_BLOCK);
    if (th) thorns(b, 4 + L, 2);
    coin(b, 4 + L - 1, h + 2);
    end(b, 4 + L + 4);
}

/* 10. dew drop: jump into a crevasse's air and press on the drop to jump again */
static void p_orb(pbuild *b, const int *v)
{
    int w = v[0], far = v[1];
    ground(b, 2, w, G_GAP);
    put(b, 2 + w / 2, 2, K_ORB);
    if (far) thorns(b, 2 + w + 3, 1);           /* a bush on the far side */
    coin(b, 2 + w / 2 + 2, 4);                  /* near the top of the drop's jump */
    end(b, 2 + w + 5);
}

/* 11. dew chain: a rhythm of drops across a long crevasse (level, zigzag or rising) */
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

/* 12. pine cones: cones roll down at the berry; jump each on its beat */
static void p_cones(pbuild *b, const int *v)
{
    int n = v[0], s = v[1], fast = v[2];
    for (int i = 0; i < n; i++) cone(b, 2 + i * s, fast ? CONE_FAST : CONE_SLOW);
    end(b, 2 + (n - 1) * s + 4);
}

/* 13. cone hop: a thorn bush and a cone, in either order */
static void p_conehop(pbuild *b, const int *v)
{
    int order = v[0], s = v[1], w = v[2];
    if (order == 0) { thorns(b, 1, w); cone(b, 1 + s, CONE_SLOW); }
    else { cone(b, 2, CONE_SLOW); thorns(b, 2 + s, w); }
    end(b, 2 + s + w + 3);
}

/* 14. ice: jump onto an icy patch, slide (no grip, no jump) under icicles, jump as soon as it ends */
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

/* 15. snow smash: roll through snow, grow into a snowberry, smash a row of small things, wash off in the stream */
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

/* 16. snow jumps: the heavy snowberry's low jumps over crevasses and rocks */
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

/* 17. leaf tunnel: ride the maple leaf between floor thorns and hanging thorns */
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

/* 18. leaf weave: glide through openings in log fences, low and high in turn */
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

/* 19. ledge: jump onto a rock ledge, hop the thorns along it, jump off the end */
static void p_ledge(pbuild *b, const int *v)
{
    int L = v[0], th = v[1], down = v[2];
    if (L < 6 && th == 2) th = 1;               /* two thorns need a long ledge */
    if (down && L < 6) th = 0;                  /* a short ledge that drops over thorns: nothing on top */
    if (down && th == 2) th = 1;
    for (int i = 0; i < L; i++) pillar(b, 2 + i, 1, K_BLOCK);
    if (th >= 1) put(b, 2 + L / 2, 1, K_THORN);
    if (th >= 2) put(b, 2 + L - 1, 1, K_THORN);
    if (down) thorns(b, 2 + L, 2);
    end(b, 2 + L + 5);
}

/* 20. phrase: a two-bar musical phrase of bushes, doubles and steps (8 phrases, forwards or mirrored) */
static void p_phrase(pbuild *b, const int *v)
{
    /* one symbol per beat: . nothing, t thorn, d double thorn, b a rock to step on, p pebble */
    static const char *ph[8] = {"t.t.d...", "t.tt.d..", "b.t.b.t.", "tt.d.t..", "p.t.p.d.", "d..t.t..",
                                "t.b..t.d", "tp.t.b.."};
    const char *s = ph[v[0]];
    int n = (int)strlen(s);
    for (int i = 0; i < n; i++) {
        int k = v[1] ? n - 1 - i : i, c = 1 + i * 4;
        switch (s[k]) {
        case 't': thorns(b, c, 1); break;
        case 'd': thorns(b, c, 2); break;
        case 'p': put(b, c, 0, K_PEBBLE); break;
        case 'b': pillar(b, c, 1, K_BLOCK); break;
        }
    }
    end(b, 1 + n * 4 + 1);
}

/* 21. breather: open ground and the scenery, sometimes one pebble; a golden blueberry up in the air */
static void p_breather(pbuild *b, const int *v)
{
    int bars = v[0], pebble = v[1];
    if (pebble) put(b, 9, 0, K_PEBBLE);
    coin(b, 9, 2);
    end(b, bars * BAR_CELLS);
}

#define T5(a) {a, a, a, a, a}
const pattern_def bt_patterns[] = {
    {"hop", "thorn bushes on the beat", 15, 0, 0, 3,
     {{"count", 1, T5(4)}, {"spacing", 0, T5(2)}, {"width", 1, T5(2)}}, p_hop},
    {"rows", "long thorn rows in one jump", 15, 0, 0, 3,
     {{"count", 1, T5(2)}, {"width", 2, {2, 2, 2, 3, 3}}, {"loose", 0, T5(1)}}, p_rows},
    {"stairs", "climb the rocks, drop off the top", 15, 0, 1, 3,
     {{"steps", 1, T5(3)}, {"step", 3, T5(5)}, {"thorn", 0, T5(1)}}, p_stairs},
    {"logs", "roll along fallen logs over thorn pits", 14, 0, 1, 3,
     {{"logs", 1, T5(3)}, {"length", 3, T5(5)}, {"pit", 1, T5(2)}}, p_logs},
    {"pillars", "hop from pillar to pillar over thorns", 15, 0, 0, 3,
     {{"count", 2, {4, 4, 4, 4, 3}}, {"shape", 0, T5(2)}, {"spacing", 3, T5(4)}}, p_pillars},
    {"gaps", "jump the crevasses", 15, 0, 0, 3,
     {{"count", 1, T5(3)}, {"width", 2, T5(3)}, {"spacing", 4, T5(7)}}, p_gaps},
    {"islands", "stepping stones over a long crevasse", 15, 0, 1, 2,
     {{"count", 2, T5(4)}, {"spacing", 3, T5(4)}}, p_islands},
    {"mushroom", "a pad launches over a wall or thorns", 15, 0, 1, 2,
     {{"kind", 0, T5(1)}, {"size", 1, {1, 1, 2, 2, 3}}}, p_mushroom},
    {"padsteps", "a pad throws the berry onto a ledge", 15, 0, 1, 3,
     {{"height", 2, T5(3)}, {"length", 3, T5(6)}, {"thorns", 0, T5(1)}}, p_padsteps},
    {"dewdrop", "a dew drop over a crevasse: jump again", 15, 0, 1, 2,
     {{"width", 4, {5, 6, 6, 6, 6}}, {"far", 0, T5(1)}}, p_orb},
    {"dewchain", "a rhythm of dew drops across the void", 15, 0, 1, 3,
     {{"count", 2, {3, 3, 3, 3, 4}}, {"shape", 0, {1, 2, 1, 1, 0}}, {"far", 0, T5(1)}}, p_orbchain},
    {"cones", "pine cones roll down at you", 14, 0, 0, 3,
     {{"count", 1, T5(3)}, {"spacing", 6, T5(10)}, {"fast", 0, T5(1)}}, p_cones},
    {"conehop", "a thorn bush and a cone", 14, 0, 0, 3,
     {{"order", 0, T5(1)}, {"spacing", 5, T5(8)}, {"width", 1, T5(2)}}, p_conehop},
    {"ice", "an icy patch: no grip, slide, then jump", 1, 0, 0, 3,
     {{"length", 3, T5(6)}, {"icicles", 0, T5(1)}, {"after", 1, T5(2)}}, p_ice},
    {"snowsmash", "grow into a snowberry and smash through", 1, 0, 0, 3,
     {{"smash", 2, T5(5)}, {"rocks", 0, T5(2)}, {"cones", 0, T5(1)}}, p_snowsmash},
    {"snowjumps", "the heavy snowberry's low jumps", 1, 0, 0, 3,
     {{"count", 1, T5(3)}, {"rock", 0, T5(1)}, {"spacing", 4, T5(6)}}, p_snowgaps},
    {"leaftunnel", "ride the leaf between floor and ceiling thorns", 14, 0, 1, 3,
     {{"count", 2, T5(4)}, {"height", 5, T5(7)}, {"first", 0, T5(1)}}, p_glidetunnel},
    {"leafweave", "glide through low and high openings", 14, 0, 1, 3,
     {{"count", 2, T5(4)}, {"open", 3, T5(4)}, {"spacing", 6, T5(8)}}, p_glideweave},
    {"ledge", "run a rock ledge, hop its thorns, jump off", 15, 0, 0, 3,
     {{"length", 4, T5(8)}, {"thorns", 0, T5(2)}, {"down", 0, T5(1)}}, p_ledge},
    {"phrase", "a two-bar phrase of bushes and steps", 15, 0, 0, 2,
     {{"phrase", 0, T5(7)}, {"mirror", 0, T5(1)}}, p_phrase},
    {"breather", "open ground to breathe", 15, 1, 1, 2,
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
