/*
 * Hearts' bot (src/games/hearts_bot.c, trickbot.h): the core's rules against the game's (the legal cards, the tricks'
 * winners and points, card by card over random deals), the view keeps to what the seat may know (the others' hidden
 * cards swapped: the same move), the queen of spades shed on a void, the cancel, then self-play at every level
 * (tests/ai_selfplay.h; 4 players, then 3, 5 and 6 briefly). Headless: this file plays the screen's part
 * (hearts_live(), hearts_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/hearts.h"
#include "games/trickbot.h"

static hgame T, U, S;                           /* the "screen's" game; a scratch copy; the view's replay */
static int NP = 4, dealt;
const hgame *hearts_live(void) { return &T; }

/* one action of the notation, as hearts_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        hearts_new(&T, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!hearts_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int hearts_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

/* the winner's half of the table (even seats: the "a" level's in even games), 2 a tie across the halves */
static int result(void)
{
    if (!dealt || T.pos.phase != HP_OVER) return -1;
    int low = 1 << 20, half = 0;
    for (int q = 0; q < NP; q++) low = T.pos.total[q] < low ? T.pos.total[q] : low;
    for (int q = 0; q < NP; q++)
        if (T.pos.total[q] == low) half |= 1 << (q & 1);
    return half == 3 ? 2 : half == 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const hpos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    char a[3], b[3], c[3];
    switch (p->phase) {
    case HP_PASS: {
        uint8_t ix[HEARTS_HAND];
        for (int i = 0; i < n; i++) ix[i] = (uint8_t)i;
        for (int i = 0; i < 3; i++) {           /* three different cards */
            int j = i + ai_rand_n(r, n - i);
            uint8_t t = ix[i]; ix[i] = ix[j]; ix[j] = t;
        }
        trick_name(h[ix[0]], a), trick_name(h[ix[1]], b), trick_name(h[ix[2]], c);
        snprintf(m, (size_t)cap, "p%s%s%s", a, b, c);
        return 1;
    }
    case HP_PLAY: {
        uint8_t L[HEARTS_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (hearts_can_play(p, h[i])) L[k++] = h[i];
        if (!k) return 0;
        trick_name(L[ai_rand_n(r, k)], a);
        snprintf(m, (size_t)cap, "%s", a);
        return 1;
    }
    case HP_ROUND: snprintf(m, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* the view of the seat to move, its hidden cards then revealed (a test may look): the deal as the core plays it */
static void reveal(tb_state *v, const hpos *p)
{
    for (int s = 0; s < p->players; s++) {
        memcpy(v->hand[s], p->hand[s], p->n[s]);
        v->n[s] = v->nknown[s] = p->n[s];
    }
    v->full = 1;
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 11);
    tb_state v, w;
    /* the core's rules are the game's: legal cards at every turn, and a revealed deal played card by card keeps
     * the game's points, turns and tricks */
    {
        int same = 1, plays = 0, tricks_ok = 1;
        for (int gn = 0; gn < 300 && same; gn++) {
            NP = 3 + gn % 4;
            hearts_new(&T, NP, 500 + gn);
            dealt = 1;
            int fresh = 1;
            while (T.pos.phase != HP_OVER && same && tricks_ok) {
                const hpos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == HP_PLAY) {
                    hb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += hearts_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = hearts_can_play(p, L[i]);
                    if (fresh) w = v, reveal(&w, p), fresh = 0;
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int before = p->tricks, was_play = p->phase == HP_PLAY;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->tricks != before)
                    for (int s = 0; s < NP; s++)
                        tricks_ok &= w.pts[s] == p->taken[s] && (p->phase != HP_PLAY || w.turn == p->turn);
                if (p->phase == HP_ROUND || p->phase == HP_PASS) fresh = 1;
                if (p->phase == HP_ROUND) tricks_ok &= tb_over(&w);
                if (!tricks_ok) {
                    printf("     game %d: after %s: tricks %d phase %d turn %d/%d pts", gn, m, p->tricks, p->phase,
                           w.turn, p->turn);
                    for (int s = 0; s < NP; s++) printf(" %d/%d", w.pts[s], p->taken[s]);
                    printf("\n");
                }
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns, 3 to 6 players)", plays);
        check(name, same);
        check("a revealed deal played by the core: the same points, turns and end", tricks_ok);
    }
    /* the determinisation keeps the counts, the voids and the passed cards where they went */
    {
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 60 && ok; gn++) {
            NP = 4;
            hearts_new(&T, NP, 900 + gn);
            dealt = 1;
            for (int k = 0; k < 4 + gn % 30 && T.pos.phase != HP_OVER; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            if (T.pos.phase != HP_PLAY) continue;
            const hpos *p = &T.pos;
            hb_view(&T, p->turn, &v, &S);
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                tried++;
                int cnt[256] = {0};
                for (int s = 0; s < NP; s++) {
                    ok &= w.n[s] == p->n[s];
                    for (int i = 0; i < w.n[s]; i++) {
                        cnt[w.hand[s][i]]++;
                        ok &= w.cap[s][CARD_SUIT(w.hand[s][i])] != 0;   /* nobody holds a suit shown void */
                    }
                }
                for (int i = 0; i < p->n[p->turn]; i++) ok &= w.hand[p->turn][i] == p->hand[p->turn][i];
                for (int i = 0; i < 256; i++) ok &= cnt[i] <= 1;
                if (p->dir != PASS_HOLD)        /* the passed cards still out are where they went */
                    for (int k = 0; k < 3; k++) {
                        int c = p->pass[p->turn][k], to = hearts_target(p, p->turn), in_to = 0, out = 1;
                        for (int i = 0; i < p->n[to]; i++) out &= p->hand[to][i] != c;
                        for (int i = 0; i < w.n[to]; i++) in_to |= w.hand[to][i] == c;
                        ok &= out || in_to;
                    }
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, voids, own hand, passed cards (%d)", tried);
        check(name, ok && tried > 100);
    }

    const bot *b = bot_find("hearts");
    check("registered: bot_find(\"hearts\")", b && b->think && b->play == hearts_show && b->actor);

    /* honest: the others' hidden cards swapped (same suits, so every void still holds): the same move */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 40; gn++) {
            NP = 4;
            hearts_new(&T, NP, 3000 + gn);
            dealt = 1;
            for (int k = 0; k < 8 + gn && T.pos.phase != HP_OVER; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            hpos *p = &T.pos;
            if (p->phase != HP_PLAY) continue;
            int me = p->turn, a = (me + 1) % NP, c = (me + 2) % NP, swapped = 0;
            for (int i = 0; i < p->n[a] && !swapped; i++)
                for (int j = 0; j < p->n[c] && !swapped; j++)
                    if (CARD_SUIT(p->hand[a][i]) == CARD_SUIT(p->hand[c][j])) {
                        int known = 0;          /* a card it passed there is known: leave it */
                        for (int k = 0; k < 3; k++)
                            known |= p->pass[me][k] == p->hand[a][i] || p->pass[me][k] == p->hand[c][j];
                        if (known) continue;
                        uint8_t t = p->hand[a][i];
                        p->hand[a][i] = p->hand[c][j];
                        p->hand[c][j] = t;
                        swapped = 1;
                    }
            if (!swapped) continue;
            hgame keep = T;
            uint32_t m1 = hb_choose(&T, me, AI_MEDIUM, 77, NULL);
            /* undo the swap: back to the true deal */
            hearts_new(&T, NP, 3000 + gn);
            for (int i = 0; i < keep.n; i++) hearts_play(&T, keep.act[i]);
            uint32_t m2 = hb_choose(&T, me, AI_MEDIUM, 77, NULL);
            same &= m1 == m2;
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        check(name, same && tried >= 20);
    }
    /* void in the suit led, holding the queen of spades, another taking the trick: she goes (medium and hard) */
    {
        int found = 0, shed = 0;
        for (int gn = 0; gn < 3000 && found < 30; gn++) {
            NP = 4;
            hearts_new(&T, NP, 7000 + gn);
            dealt = 1;
            for (int k = 0; k < 400 && T.pos.phase != HP_OVER; k++) {
                const hpos *p = &T.pos;
                if (p->phase == HP_PLAY && p->tricks > 0 && p->ntrick > 0 && CARD_SUIT(p->trick[0]) != SUIT_SPADES &&
                    !trick_has_suit(p->hand[p->turn], p->n[p->turn], CARD_SUIT(p->trick[0])) &&
                    trick_find(p->hand[p->turn], p->n[p->turn], HEARTS_QUEEN) >= 0) {
                    found++;
                    uint32_t a = hb_choose(&T, p->turn, AI_MEDIUM + (found & 1), 5, NULL);
                    shed += HA_KIND(a) == HA_PLAY && hearts_points(HA_CARD(a, 2)) > 0;
                    if (a != (HA_PLAY | HEARTS_QUEEN) && getenv("DEBUG")) {
                        char c[3];
                        printf("     kept the queen: played %s, hand", (trick_name(HA_CARD(a, 2), c), c));
                        for (int i = 0; i < p->n[p->turn]; i++) printf(" %s", (trick_name(p->hand[p->turn][i], c), c));
                        printf("; trick");
                        for (int i = 0; i < p->ntrick; i++) printf(" %s", (trick_name(p->trick[i], c), c));
                        printf("; taken");
                        for (int i = 0; i < 4; i++) printf(" %d", p->taken[i]);
                        printf(" seat %d leader %d\n", p->turn, p->leader);
                    }
                    break;
                }
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
        }
        snprintf(name, sizeof name, "void in the suit led, holding the queen of spades: it sheds points: %d of %d",
                 shed, found);
        check(name, found >= 20 && shed * 10 >= found * 9);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        NP = 4;
        hearts_new(&T, NP, 5);
        dealt = 1;
        while (T.pos.phase == HP_PASS) { char x[BOT_MOVE_CAP]; random_move(&r, x, sizeof x); one(x); }
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
        check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 4, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work: iterations and time on positions of random deals */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            for (int k = 0; k < npos; k++) {
                NP = 4;
                hearts_new(&T, NP, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k);
                for (int i = 0; i < 12 + k % 20; i++) { char m[BOT_MOVE_CAP]; random_move(&r, m, sizeof m); one(m); }
                if (T.pos.phase != HP_PLAY) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                hb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "hearts", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 75},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 50},
        {AI_HARD, AI_EASY, 40, 55},
    };
    NP = 4;
    int bad = sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static const sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 20, 0}};
    for (NP = 3; NP <= 6; NP++) {
        if (NP == 4) continue;
        printf("-- %d players\n", NP);
        bad |= sp_run(&G, Q, 1, argc, argv);
    }
    return bad;
}
