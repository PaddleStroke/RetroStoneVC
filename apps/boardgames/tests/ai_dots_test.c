/*
 * Dots and boxes' bot (src/games/dots_bot.c): its registry entry, its endgame play against a brute-force solver of
 * the 3 x 3 board (every line, every box: the exact value of a position), a double-deal, the cancel, then self-play
 * at every level on the three boards (tests/ai_selfplay.h). Headless: this file plays the screen's part (dots_live(),
 * dots_show()). --full for the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/dots.h"

static dgame T;                                 /* the "screen's" game: the bot reads it */
static int size = 3;
const dgame *dots_live(void) { return &T; }
int dots_show(const char *move)
{
    int ply = T.ply;
    if (!dots_script(&T, move)) { T.ply = ply; return 0; }
    return T.ply == ply + 1;
}

static void reset(void) { dots_new(&T, size); }
static int actor(void) { return dots_pos(&T)->side; }
static int result(void)
{
    dstatus s = dots_status(&T);
    return s == DS_PLAY ? -1 : s == DS_WIN0 ? 0 : s == DS_WIN1 ? 1 : 2;
}

static void name(int n, int l, char *m, int cap)
{
    int across = l < n * (n + 1), k = across ? l : l - n * (n + 1), w = across ? n : n + 1;
    snprintf(m, (size_t)cap, "%c%d,%d", across ? 'h' : 'v', k % w + 1, k / w + 1);
}

static int random_move(ai_rng *r, char *m, int cap)
{
    const dpos *p = dots_pos(&T);
    int nl = dots_lines(p->n), left = nl - p->drawn;
    if (!left) return 0;
    int k = ai_rand_n(r, left), l;
    for (l = 0; l < nl; l++)
        if (p->line[l] < 0 && k-- == 0) break;
    name(p->n, l, m, cap);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* the 3 x 3 board by brute force: the value (boxes left: the mover's - the other's) of each set of lines drawn */
static int8_t *V;
static int exact(const dpos *p, uint32_t mask)
{
    if (V[mask] != -128) return V[mask];
    int best = -100;
    for (int l = 0; l < 24; l++) {
        if (mask >> l & 1) continue;
        dpos q = *p;
        int c = dots_make(&q, l), v = c ? c + exact(&q, mask | 1u << l) : -exact(&q, mask | 1u << l);
        if (v > best) best = v;
    }
    if (best == -100) best = 0;
    V[mask] = (int8_t)best;
    return best;
}

static uint32_t mask_of(const dpos *p)
{
    uint32_t m = 0;
    for (int l = 0; l < 24; l++)
        if (p->line[l] >= 0) m |= 1u << l;
    return m;
}

int main(int argc, char **argv)
{
    const bot *b = bot_find("dots");
    check("registered: bot_find(\"dots\")", b && b->think && b->play == dots_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP], nm[160];
    V = malloc((size_t)1 << 24);
    if (!V) return 1;
    memset(V, -128, (size_t)1 << 24);
    {   /* 3 x 3: positions from random games, near the end (no safe line, or a few): hard's line keeps the value */
        ai_rng r;
        ai_seed(&r, 3);
        int tried[2] = {0, 0}, good[2] = {0, 0}, endv = 0, endn = 0;
        for (int k = 0; k < 4000 && (tried[0] < 300 || tried[1] < 300); k++) {
            dots_new(&T, 3);
            int stop = 10 + ai_rand_n(&r, 10);
            while (dots_pos(&T)->drawn < stop && result() < 0) {
                random_move(&r, m, sizeof m);
                dots_show(m);
            }
            if (result() >= 0) continue;
            const dpos *p = dots_pos(&T);
            int left = 24 - p->drawn;
            if (left > 14) continue;
            uint32_t mask = mask_of(p);
            int opt = exact(p, mask);
            /* the endgame value (no safe line left, nothing to take): the bot's own count against the truth */
            int any_safe = 0, take = 0;
            for (int l = 0; l < 24; l++) {
                if (p->line[l] >= 0) continue;
                dpos q = *p;
                int c = dots_make(&q, l);
                take |= c > 0;
                int s[4], three = 0;
                for (int bb = 0; bb < 9; bb++) {
                    dots_sides(3, bb, s);
                    int d = 0;
                    for (int i = 0; i < 4; i++) d += q.line[s[i]] >= 0;
                    three |= d == 3 && q.box[bb] < 0;
                }
                any_safe |= !three && !c;
            }
            int kind = any_safe ? 1 : 0;        /* 0: the chains' endgame, 1: safe lines left */
            if (tried[kind] >= 300) continue;
            if (!any_safe && !take) { endn++; endv += dtb_endgame(p) == opt; }
            b->think(p->side, AI_HARD, m, sizeof m);
            dpos q = *p;
            int len, l = dots_parse(3, m, &len), c = dots_make(&q, l);
            int got = c ? c + exact(&q, mask | 1u << l) : -exact(&q, mask | 1u << l);
            tried[kind]++;
            good[kind] += got == opt;
        }
        snprintf(nm, sizeof nm, "3 x 3, no safe line left: hard plays perfectly in %d of %d", good[0], tried[0]);
        check(nm, good[0] >= tried[0] * 95 / 100);
        snprintf(nm, sizeof nm, "3 x 3, safe lines left (14 lines or fewer): hard plays perfectly in %d of %d", good[1],
                 tried[1]);
        check(nm, good[1] >= tried[1] * 80 / 100);
        snprintf(nm, sizeof nm, "the chains' value is exact in %d of %d endgames", endv, endn);
        if (endn) check(nm, endv >= endn * 95 / 100);
    }
    {   /* not its turn: no move; the end of a game: no move */
        dots_new(&T, 5);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    if (fails) return 1;
    free(V);

    static const sp_game G = {
        .id = "dots", .max_plies = DOTS_MAX_LINES, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = dots_show,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 85},
        {AI_MEDIUM, AI_EASY, 300, 58},
        {AI_HARD, AI_EASY, 200, 80},
        {AI_HARD, AI_MEDIUM, 200, 60},
        {AI_HARD, AI_HARD, 20, 0},
    };
    int bad = 0;
    for (int i = 0; i < 3; i++) {
        size = 3 + 2 * i;
        printf("-- %d x %d boxes\n", size, size);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
