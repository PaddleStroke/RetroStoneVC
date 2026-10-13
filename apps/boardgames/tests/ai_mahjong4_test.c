/*
 * Mahjong's bot (src/games/mahjong4_bot.c): its registry entry, a bot that reads no other hand nor the wall (both
 * shuffled together: the same action), games of four bots in both lengths of game with every win offered taken,
 * then self-play at every level (tests/ai_selfplay.h: seats 0 and 2 play one level, 1 and 3 the other; the half
 * holding the highest total wins; a game in which nobody wins a hand is a draw, often against the random player,
 * who never wins one). Headless: this file plays the screen's part (mahjong4_live(), mahjong4_show()). --full for
 * the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "cards.h"
#include "games/mahjong4.h"

int m4b_choose(const m4game *g, int seat, int level, uint64_t seed);
void m4b_name(const m4pos *p, int a, char *m, int cap);

static m4game T, U;
static int dealt, VAR = M4_EAST;
const m4game *mahjong4_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        m4_new(&T, VAR, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    if (!s[0] || strchr("a?!#*", s[0])) return 0;
    U = T;
    int next = !strcmp(s, "n");
    if (!m4_script(&U, s) || (!next && U.n != T.n + 1)) return 0;
    T = U;
    return 1;
}
int mahjong4_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void)
{
    if (!dealt) return 0;
    int a = m4_actor(&T.pos);
    return a < 0 ? 0 : a;
}
static int result(void)
{
    if (!dealt || T.pos.phase != MP_OVER) return -1;
    unsigned w = m4_winners(&T.pos), half = 0;
    for (int q = 0; q < M4_P; q++)
        if (w >> q & 1) half |= 1u << (q & 1);
    return half == 3 ? 2 : half == 2;
}

/* a random player: a win when it can, any discard, a claim or a pass at random */
static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const m4pos *p = &T.pos;
    int a = 0;
    if (p->phase == MP_SCORE) a = MA_NEXT;
    else if (m4_legal(p, MA_WIN)) a = MA_WIN;
    else if (p->phase == MP_DISCARD) {
        int q = p->turn;
        a = MA_DISCARD | p->hand[q][ai_rand_n(r, p->nhand[q])];
    } else {
        static const int k[] = {MA_PUNG, MA_KONG, MA_PASS, MA_PASS, MA_PASS};
        a = k[ai_rand_n(r, 5)];
        if (MA_KIND(a) == MA_KONG) a |= m4_kind(p->tile);
        if (!m4_legal(p, a)) a = MA_PASS;
    }
    m4b_name(p, a, m, cap);
    return 1;
}

static int fails;
static void check(const char *what, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

int main(int argc, char **argv)
{
    char what[160];
    ai_rng r;
    ai_seed(&r, 29);
    const bot *b = bot_find("mahjong4");
    check("registered: bot_find(\"mahjong4\")", b && b->think && b->actor && b->play == mahjong4_show);

    /* honest: the others' concealed tiles and the wall shuffled together: the same action at every level */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 40; gn++) {
            m4_new(&T, M4_EAST, 700 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 4 + gn * 3 && T.pos.phase < MP_SCORE; k++) {
                char m[BOT_MOVE_CAP];
                int a = m4b_choose(&T, m4_actor(&T.pos), AI_HARD, 1);
                m4b_name(&T.pos, a, m, sizeof m);
                if (!one(m)) break;
            }
            m4pos *p = &T.pos;
            if (p->phase >= MP_SCORE) continue;
            int me = m4_actor(p), acts[3];
            for (int lv = 0; lv < 3; lv++) acts[lv] = m4b_choose(&T, me, lv, 2);
            m4game *save = malloc(sizeof *save);
            *save = T;
            uint8_t pool[M4_TILES];
            int n = 0;
            for (int q = 0; q < M4_P; q++)
                if (q != me)
                    for (int i = 0; i < p->nhand[q]; i++) pool[n++] = p->hand[q][i];
            for (int i = p->head; i < p->tail; i++) pool[n++] = p->wall[i];
            cards_shuffle(pool, n, 3u + (uint32_t)gn);
            int k = 0;
            for (int q = 0; q < M4_P; q++)
                if (q != me)
                    for (int i = 0; i < p->nhand[q]; i++) p->hand[q][i] = pool[k++];
            for (int i = p->head; i < p->tail; i++) p->wall[i] = pool[k++];
            for (int lv = 0; lv < 3; lv++) {
                int a = m4b_choose(&T, me, lv, 2);
                if (a != acts[lv]) printf("     game %d level %d: %x, then %x\n", gn, lv, acts[lv], a), same = 0;
            }
            T = *save;
            free(save);
            tried++;
        }
        snprintf(what, sizeof what, "the hidden tiles shuffled: the same action at every level (%d positions)", tried);
        check(what, same && tried > 25);
    }
    /* two bots' hands to the end at every level, every action legal; a win offered is always taken */
    {
        int ok = 1, wins_offered = 0, wins_taken = 0;
        for (int gn = 0; gn < 6 && ok; gn++) {
            VAR = gn % M4_VARIANTS;
            m4_new(&T, VAR, 90 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 20000 && T.pos.phase != MP_OVER; k++) {
                const m4pos *p = &T.pos;
                int me = T.pos.phase == MP_SCORE ? 0 : m4_actor(p), lv = (me + gn) % 3;
                char m[BOT_MOVE_CAP] = "";
                int can_win = m4_legal(p, MA_WIN);
                ai_set_seed(ai_mix((uint64_t)gn, (uint64_t)k));
                if (!b->think(me, lv, m, sizeof m) || !one(m)) {
                    printf("     game %d: \"%s\" refused\n", gn, m);
                    ok = 0;
                    break;
                }
                wins_offered += can_win;
                wins_taken += can_win && !strcmp(m, "w");
            }
            ok &= T.pos.phase == MP_OVER;
        }
        snprintf(what, sizeof what, "6 games of four bots end; every win offered taken (%d)", wins_offered);
        check(what, ok && wins_offered == wins_taken && wins_offered > 5);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "mahjong4", .max_plies = 60000, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    /* (hard against medium: 56% over the 200 games, but luck rules the short run's ten) */
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 70}, {AI_MEDIUM, AI_EASY, 200, 55}, {AI_HARD, AI_MEDIUM, 200, 35},
        {AI_HARD, AI_EASY, 200, 58},
    };
    VAR = M4_EAST;
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
