/*
 * Koi-Koi's bot (src/games/koikoi_bot.c): the view keeps to what the seat may know (the other hand and the stock
 * shuffled together: the same move), a bright on the field taken when a card can, the cancel, the levels' work, whole
 * games (6 and 12 rounds), then self-play by duplicate deals (tests/ai_tricks.h): a "game" there is one round, won by
 * the player who stops with a yaku (nobody: a draw). Headless: this file plays the screen's part (koikoi_live(),
 * koikoi_show()). --full for the long run and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"

#include "games/cardbot.h"
#include "games/koikoi.h"

static kgame T, U;
static int VAR = KK_HALF, dealt, ROUND = 1;
const kgame *koikoi_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        kk_new(&T, VAR, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!kk_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int koikoi_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

static int result(void)
{
    if (!dealt) return -1;
    const kpos *p = &T.pos;
    if (ROUND && (p->phase == KP_ROUND || p->phase == KP_OVER)) return p->res.winner < 2 ? p->res.winner : 2;
    if (p->phase != KP_OVER) return -1;
    int w = kk_winner(p);
    return w < 0 ? 2 : w;
}

static void card(int c, char *out) { snprintf(out, 4, "%d%c", HANA_MONTH(c) + 1, 'a' + HANA_INDEX(c)); }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const kpos *p = &T.pos;
    char c[4];
    int sl[4], n;
    switch (p->phase) {
    case KP_PLAY: card(p->hand[p->turn][ai_rand_n(r, p->nh[p->turn])], c), snprintf(m, (size_t)cap, "p%s", c); return 1;
    case KP_CHOOSE:
        n = kk_matches(p, p->held, sl);
        card(p->field[sl[ai_rand_n(r, n)]], c), snprintf(m, (size_t)cap, "t%s", c);
        return 1;
    case KP_DRAW: snprintf(m, (size_t)cap, "d"); return 1;
    case KP_DECIDE: snprintf(m, (size_t)cap, ai_rand_n(r, 2) ? "k" : "s"); return 1;
    case KP_ROUND: snprintf(m, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static int choose(int seat, int level, uint64_t seed, ai_stats *st) { return kkb_choose(&T.pos, seat, level, seed, st) != 0; }

static int setup(int k, ai_rng *r)
{
    VAR = k % 2;
    kk_new(&T, VAR, 40 + (uint32_t)k);
    dealt = 1;
    for (int i = 0, n = ai_rand_n(r, 30); i < n && T.pos.phase < KP_ROUND; i++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
    return T.pos.phase < KP_ROUND ? T.pos.turn : -1;
}

int main(int argc, char **argv)
{
    char line[200];
    int full = cg_full(argc, argv);
    ai_rng r;
    ai_seed(&r, 11);
    const bot *b = bot_find("koikoi");
    tt_check("registered: bot_find(\"koikoi\")", b && b->think && b->play == koikoi_show && b->actor);
    if (!b) return 1;

    {   /* honest: the other hand and the stock shuffled together: the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 60; gn++) {
            VAR = gn % 2;
            kk_new(&T, VAR, 3000 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < gn % 30 && T.pos.phase < KP_ROUND; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            kpos *p = &T.pos;
            if (p->phase >= KP_ROUND) continue;
            int me = p->turn, o = 1 - me, a1 = kkb_choose(p, me, AI_MEDIUM, 77, NULL);
            uint8_t pool[KK_CARDS];
            int n = 0;
            memcpy(pool + n, p->hand[o], p->nh[o]), n += p->nh[o];
            memcpy(pool + n, p->deck, p->ndeck), n += p->ndeck;
            cdb_shuffle(pool, n, &r);
            memcpy(p->hand[o], pool, p->nh[o]);
            memcpy(p->deck, pool + p->nh[o], p->ndeck);
            p->seed ^= 0x5a5a5a5a;
            int a2 = kkb_choose(p, me, AI_MEDIUM, 77, NULL);
            if (a2 != a1) printf("     game %d: %x then %x\n", gn, a1, a2);
            same &= a2 == a1;
            tried++;
        }
        snprintf(line, sizeof line, "the hidden cards shuffled: the same move (%d positions)", tried);
        tt_check(line, same && tried >= 40);
    }
    {   /* a bright on the field, a card of its month in hand: taken (medium and hard) */
        int found = 0, took = 0;
        for (int gn = 0; gn < 2000 && found < 30; gn++) {
            kk_new(&T, KK_HALF, 9000 + (uint32_t)gn);
            dealt = 1;
            const kpos *p = &T.pos;
            if (p->phase != KP_PLAY) continue;
            int want = -1;
            for (int i = 0; i < p->nh[p->turn]; i++) {
                int sl[4], c = p->hand[p->turn][i], n = kk_matches(p, c, sl);
                for (int j = 0; j < n; j++)
                    if (kk_type(p->field[sl[j]]) == HT_BRIGHT && n == 1 && kk_type(c) == HT_PLAIN) want = c;
            }
            if (want < 0) continue;
            found++;
            int a = kkb_choose(p, p->turn, AI_MEDIUM + (found & 1), 3, NULL), sl[4];
            int n = kk_matches(p, KA_CARD(a), sl);
            took += n == 1 && kk_type(p->field[sl[0]]) == HT_BRIGHT;
        }
        snprintf(line, sizeof line, "a bright on the field taken: %d of %d", took, found);
        tt_check(line, found >= 20 && took * 10 >= found * 8);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        kk_new(&T, KK_FULL, 5);
        dealt = 1;
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think(1 - T.pos.turn, AI_HARD, m, sizeof m));
        tt_check("actor: the seat to move", b->actor() == T.pos.turn);
    }
    cg_timing("koikoi", full ? 40 : 10, setup, choose);

    static const sp_game G = {
        .id = "koikoi", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    {
        int ok = 1;
        ROUND = 0;
        for (VAR = 0; VAR < KK_VARIANTS; VAR++) ok &= cg_whole(&G, b, 70 + (uint32_t)VAR, AI_EASY, SP_RANDOM, 3000);
        ROUND = 1;
        tt_check("whole games end (12 and 6 rounds, easy and random)", ok);
    }
    if (tt_fails) return 1;

    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 70},
        {AI_MEDIUM, AI_EASY, 60, 48},
        {AI_HARD, AI_MEDIUM, 60, 45},
        {AI_HARD, AI_EASY, 40, 55},
    };
    cg_scale(P, 4);
    VAR = KK_HALF;
    return tt_run(&G, P, 4, argc, argv);
}
