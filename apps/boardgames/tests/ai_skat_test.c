/*
 * Skat's bot (src/games/skat_bot.c, trickbot.h): the core's rules against the game's (the legal cards, the tricks'
 * winners and points, the null lost at the declarer's first trick, the deal won or lost as the rules score it, card by
 * card over random deals of every game: suits, grand, null, hand, ouvert), the determinised deals, the view keeps to
 * what the seat may know (the others' hidden cards and the skat swapped: the same move and the same call), sensible
 * calls (the bids, the games named, hand games), the cancel, then self-play at every level (tests/ai_selfplay.h;
 * three seats: seats 0 and 2 one level, seat 1 the other, swapped every other game; the short list of 12 deals, then
 * the long list of 36 briefly). Headless: this file plays the screen's part (skat_live(), skat_show()). --full for
 * the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/skat.h"

static skgame T, U;
static int dealt, VAR = SKAT_SHORT, full;
const skgame *skat_live(void) { return &T; }

/* one action of the notation, as skat_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        skat_new(&T, VAR, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!skat_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int skat_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

/* the winner's half of the table (seats 0 and 2: the "a" level's in even games), 2 a tie across the halves */
static int result(void)
{
    if (!dealt || T.pos.phase != SK_OVER) return -1;
    int m = skat_winners(&T.pos), half = 0;
    for (int q = 0; q < SKAT_P; q++)
        if (m >> q & 1) half |= 1 << (q & 1);
    return half == 3 ? 2 : half == 2;
}

/* an action's notation */
static void name(uint32_t a, char *m, int cap)
{
    char c[3];
    int k = SKA_ARG(a), f = k >> 4;
    switch (SKA_KIND(a)) {
    case SA_SAY: snprintf(m, (size_t)cap, "b%d", k); break;
    case SA_HOLD: snprintf(m, (size_t)cap, "h"); break;
    case SA_PASS: snprintf(m, (size_t)cap, "p"); break;
    case SA_PICKUP: snprintf(m, (size_t)cap, "s"); break;
    case SA_PUT: trick_name(k, c); snprintf(m, (size_t)cap, "d%s", c); break;
    case SA_TAKE: trick_name(k, c); snprintf(m, (size_t)cap, "u%s", c); break;
    case SA_GAME:
        snprintf(m, (size_t)cap, "g%c%s%s%s", "shdcgn"[k & 15], f & SF_SCHNEIDER ? "S" : "", f & SF_SCHWARZ ? "Z" : "",
                 f & SF_OUVERT ? "O" : "");
        break;
    case SA_PLAY: trick_name(k, c); snprintf(m, (size_t)cap, "%s", c); break;
    case SA_NEXT: snprintf(m, (size_t)cap, "N"); break;
    default: m[0] = 0;
    }
}

/* a random game the declarer may name now (any announcements) */
static uint32_t random_game(ai_rng *r, const skpos *p)
{
    static const int fl[4] = {0, SF_SCHNEIDER, SF_SCHNEIDER | SF_SCHWARZ, SF_SCHNEIDER | SF_SCHWARZ | SF_OUVERT};
    for (int k = 0; k < 40; k++) {
        int t = ai_rand_n(r, SG_TYPES), f = t == SG_NULL ? (ai_rand_n(r, 2) ? SF_OUVERT : 0) : fl[ai_rand_n(r, 4)];
        if (ai_rand_n(r, 2) && t != SG_NULL) f = 0;
        if (skat_game_ok(p, t, f)) return SKA_GAME(t, f);
    }
    return SKA_GAME(SUIT_CLUBS, 0);
}

/* the random player: a legal action (bids now and then, the skat mostly taken up) */
static uint32_t random_act(ai_rng *r)
{
    const skpos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    switch (p->phase) {
    case SK_BID:
        if (skat_saying(p)) {
            int b = skat_next_bid(p->bid);
            return b && ai_rand_n(r, 3) == 0 ? SA_SAY | (uint32_t)b : SA_PASS;
        }
        return ai_rand_n(r, 2) ? SA_HOLD : SA_PASS;
    case SK_SKAT: return ai_rand_n(r, 4) ? SA_PICKUP : random_game(r, p);
    case SK_DISCARD: return p->nskat < 2 ? SA_PUT | h[ai_rand_n(r, n)] : random_game(r, p);
    case SK_PLAY: {
        uint8_t L[SKAT_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (skat_can_play(p, h[i])) L[k++] = h[i];
        return k ? SA_PLAY | L[ai_rand_n(r, k)] : 0;
    }
    case SK_DEAL: return SA_NEXT;
    default: return 0;
    }
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    uint32_t a = random_act(r);
    if (!a) return 0;
    name(a, m, cap);
    return 1;
}

static void walk(ai_rng *r, int plies)          /* random actions (not past the game's end) */
{
    for (int k = 0; k < plies && T.pos.phase != SK_OVER; k++) skat_play(&T, random_act(r));
}

static void walk_to_play(ai_rng *r, int extra)  /* to the card play, then extra plays (in the same deal) */
{
    for (int k = 0; k < 200 && T.pos.phase != SK_PLAY && T.pos.phase != SK_OVER; k++) skat_play(&T, random_act(r));
    for (int k = 0; k < extra && T.pos.phase == SK_PLAY; k++) {
        const skpos *p = &T.pos;
        if (p->tricks == 9 && p->ntrick == 2) break;
        skat_play(&T, random_act(r));
    }
}

/* every hand known (a test may look) */
static void reveal(tb_state *v, const skpos *p)
{
    tt_reveal(v, &p->hand[0][0], SKAT_HAND, p->n);
}

int main(int argc, char **argv)
{
    char name_[160];
    ai_rng r;
    ai_seed(&r, 13);
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    static tb_state v, w;
    dealt = 1;
    /* the core's rules are the game's: the legal cards at every turn; a revealed deal played card by card keeps the
     * game's points, tricks, turns and end, and the reward says won as the rules score it */
    {
        int same = 1, plays = 0, tricks_ok = 1, score_ok = 1, deals = 0, types[SG_TYPES] = {0}, hands = 0, ouv = 0;
        for (int gn = 0; gn < 120 && same && tricks_ok && score_ok; gn++) {
            skat_new(&T, gn & 1 ? SKAT_LONG : SKAT_SHORT, 500 + gn);
            int fresh = 1;
            while (T.pos.phase != SK_OVER && T.pos.ndeals < 6 && same && tricks_ok && score_ok) {
                const skpos *p = &T.pos;
                if (p->phase == SK_PLAY) {
                    sktb_view(&T, p->turn, &v);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += skat_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = skat_can_play(p, L[i]);
                    if (fresh) {
                        sktb_view(&T, p->declarer, &w);
                        reveal(&w, p);
                        w.rest[0] = p->skat[0], w.rest[1] = p->skat[1], w.nrest = 2;
                        w.R->dealt(&w);
                        fresh = 0;
                        deals++;
                        types[p->type]++;
                        hands += p->hand_game;
                        ouv += (p->flags & SF_OUVERT) != 0;
                    }
                    plays++;
                }
                uint32_t a = random_act(&r);
                int before = p->tricks, was_play = p->phase == SK_PLAY;
                if (was_play) tb_play(&w, SKA_ARG(a));
                skat_play(&T, a);
                if (was_play && (p->tricks != before || p->phase != SK_PLAY)) {
                    for (int s = 0; s < SKAT_P; s++)
                        tricks_ok &= (p->type == SG_NULL || w.pts[s] == p->points[s]) && w.won[s] == p->won[s];
                    tricks_ok &= p->phase != SK_PLAY || (w.turn == p->turn && !tb_over(&w));
                    if (!tricks_ok)
                        printf("     game %d: tricks %d phase %d type %d turn %d/%d pts %d/%d %d/%d %d/%d won %d/%d "
                               "%d/%d %d/%d\n", gn, p->tricks, p->phase, p->type, w.turn, p->turn, w.pts[0],
                               p->points[0], w.pts[1], p->points[1], w.pts[2], p->points[2], w.won[0], p->won[0],
                               w.won[1], p->won[1], w.won[2], p->won[2]);
                }
                if (was_play && p->phase != SK_PLAY) {   /* the deal's end */
                    float rw[TB_MAXP];
                    tricks_ok &= tb_over(&w);
                    w.R->reward(&w, rw);
                    const skdeal *d = &p->list[p->ndeals - 1];
                    score_ok &= (rw[d->declarer] > 0.5f) == (d->won != 0);
                    if (!score_ok)
                        printf("     game %d deal %d: type %d flags %d hand %d bid %d points %d tricks %d won %d "
                               "reward %.3f\n", gn, p->ndeals, d->type, d->flags, d->hand, d->bid, d->points, d->tricks,
                               d->won, rw[d->declarer]);
                }
                if (p->phase != SK_PLAY) fresh = 1;
            }
        }
        snprintf(name_, sizeof name_, "the core's legal cards = the game's (%d turns, %d deals: %d %d %d %d suits, %d "
                 "grand, %d null; %d hand, %d ouvert)", plays, deals, types[3], types[0], types[1], types[2],
                 types[SG_GRAND], types[SG_NULL], hands, ouv);
        tt_check(name_, same);
        tt_check("a revealed deal played by the core: the same points, tricks, turns and end", tricks_ok);
        tt_check("the reward's winner is the rules' (overbid, schneider, schwarz, announcements, null)", score_ok);
    }
    /* the determinisation keeps the counts, the voids, the own hand, the open hand; the declarer's skat out of it */
    {
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 80 && ok; gn++) {
            skat_new(&T, SKAT_SHORT, 900 + gn);
            walk_to_play(&r, gn % 25);
            if (T.pos.phase != SK_PLAY) continue;
            const skpos *p = &T.pos;
            for (int me = 0; me < SKAT_P; me++) {
                sktb_view(&T, me, &v);
                for (int d = 0; d < 10 && ok; d++) {
                    w = v;
                    tb_determinise(&w, &r);
                    tried++;
                    ok &= tt_dealt_ok(&v, &w);
                    if ((p->flags & SF_OUVERT))  /* the open hand where it is */
                        for (int i = 0; i < p->n[p->declarer]; i++)
                            ok &= trick_find(w.hand[p->declarer], w.n[p->declarer], p->hand[p->declarer][i]) >= 0;
                    if (me == p->declarer && !p->hand_game)   /* the cards laid away: in no hand */
                        for (int s = 0; s < SKAT_P; s++)
                            for (int k = 0; k < 2; k++) ok &= trick_find(w.hand[s], w.n[s], p->skat[k]) < 0;
                    ok &= w.nrest == (me == p->declarer && !p->hand_game ? 0 : 2);
                }
            }
        }
        snprintf(name_, sizeof name_, "determinised deals keep counts, voids, own hand, the open hand, the skat (%d)",
                 tried);
        tt_check(name_, ok && tried > 300);
    }

    const bot *b = bot_find("skat");
    tt_check("registered: bot_find(\"skat\")", b && b->think && b->play == skat_show && b->actor);

    /* honest: the others' hidden cards swapped (in the same suit of the game, so every void holds) or with the
     * skat: the same card; in the bidding, a card of another hand swapped with the skat: the same call */
    {
        int same = 1, tried = 0, calls = 0;
        for (int gn = 0; gn < 70; gn++) {
            skat_new(&T, SKAT_SHORT, 3000 + gn);
            int bidding = gn % 3 == 0;
            if (bidding) walk(&r, gn % 4);
            else walk_to_play(&r, 1 + gn % 20);
            skpos *p = &T.pos;
            if (bidding ? p->phase != SK_BID : p->phase != SK_PLAY) continue;
            int me = p->turn, a = (me + 1) % SKAT_P, c = (me + 2) % SKAT_P, swapped = 0;
            if (bidding) {
                int i = ai_rand_n(&r, p->n[a]);
                uint8_t t = p->hand[a][i];
                p->hand[a][i] = p->skat[0];
                p->skat[0] = t;
                swapped = 1;
            } else {
                int open = (p->flags & SF_OUVERT) != 0, t = p->type;
                if (open && (a == p->declarer || c == p->declarer)) continue;
                for (int i = 0; i < p->n[a] && !swapped; i++)
                    for (int j = 0; j < p->n[c] && !swapped; j++)
                        if (skat_suit(p->hand[a][i], t) == skat_suit(p->hand[c][j], t) && p->hand[a][i] != p->hand[c][j]) {
                            uint8_t x = p->hand[a][i];
                            p->hand[a][i] = p->hand[c][j];
                            p->hand[c][j] = x;
                            swapped = 1;
                        }
                if (me != p->declarer && gn % 2) {   /* a defender: the skat swapped with a card of the declarer's */
                    int d = p->declarer;
                    for (int i = 0; i < p->n[d] && !open; i++)
                        if (skat_suit(p->hand[d][i], t) == skat_suit(p->skat[0], t)) {
                            uint8_t x = p->hand[d][i];
                            p->hand[d][i] = p->skat[0];
                            p->skat[0] = x;
                            break;
                        }
                }
            }
            if (!swapped) continue;
            skgame keep = T;
            uint32_t m1 = sktb_choose(&T, me, AI_MEDIUM, 77, NULL);
            skat_new(&T, SKAT_SHORT, 3000 + gn);          /* back to the true deal */
            for (int i = 0; i < keep.n; i++) skat_play(&T, keep.act[i]);
            uint32_t m2 = sktb_choose(&T, me, AI_MEDIUM, 77, NULL);
            same &= m1 == m2;
            tried++;
            calls += bidding;
        }
        snprintf(name_, sizeof name_, "the others' hidden cards and the skat swapped: the same move (%d positions, %d "
                 "in the bidding)", tried, calls);
        tt_check(name_, same && tried >= 30 && calls >= 10);
    }
    /* the calls: every seat a hard bot from the deal to the game named; the bids, the games, hand games */
    /* (the bots name a grand more often than people do, under half the games: measured on 585 deals of their own
     * calls played out at medium, the declarer won 85% of the suit games and 84% of the grands) */
    {
        int ok = 1, deals = full ? 600 : 150, thrown = 0, types[SG_TYPES] = {0}, hands = 0, ann = 0, bids = 0;
        for (int gn = 0; gn < deals && ok; gn++) {
            skat_new(&T, SKAT_SHORT, 6000 + gn);
            for (int k = 0; k < 60 && T.pos.phase < SK_PLAY; k++) {
                uint32_t a = sktb_choose(&T, T.pos.turn, AI_HARD, 3, NULL);
                ok &= skat_legal(&T.pos, a);
                if (!skat_legal(&T.pos, a))
                    printf("     deal %d: phase %d turn %d action %x illegal\n", gn, T.pos.phase, T.pos.turn, a);
                if (getenv("DEBUG")) {
                    char m[16];
                    name(a, m, sizeof m);
                    printf(" %s", m);
                }
                if (!skat_play(&T, a)) break;
            }
            const skpos *p = &T.pos;
            ok &= p->phase == SK_PLAY || p->phase == SK_DEAL;
            if (p->phase != SK_PLAY) { thrown++; continue; }
            types[p->type]++;
            hands += p->hand_game;
            ann += p->flags != 0;
            bids += p->bid;
        }
        int played = deals - thrown;
        snprintf(name_, sizeof name_, "calls: %d of %d deals played, bid %.1f on average; %d suit, %d grand, %d null; "
                 "%d hand, %d announced", played, deals, played ? (double)bids / played : 0,
                 types[0] + types[1] + types[2] + types[3], types[SG_GRAND], types[SG_NULL], hands, ann);
        tt_check(name_, ok && played * 10 >= deals * 6 && played * 100 <= deals * 99 && bids >= played * 18 &&
                 bids <= played * 45 && types[SG_GRAND] > 0 && types[SG_GRAND] * 2 < played);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        skat_new(&T, SKAT_SHORT, 5);
        walk_to_play(&r, 0);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % SKAT_P, AI_HARD, m, sizeof m));
        tt_check("actor: the seat to play", b->actor() == T.pos.turn);
    }
    {   /* the levels' work: iterations and time on positions of random deals */
        int npos = full ? 30 : 6;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                skat_new(&T, SKAT_SHORT, 40 + k + 100 * (uint64_t)retry);
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk_to_play(&r, k % 12);
                if (T.pos.phase != SK_PLAY) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                sktb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--checks")) return 0;   /* the checks alone */

    static const sp_game G = {
        .id = "skat", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 75},
        {AI_MEDIUM, AI_EASY, 200, 52},
        {AI_HARD, AI_MEDIUM, 200, 45},
        {AI_HARD, AI_EASY, 200, 52},
    };
    VAR = SKAT_SHORT;
    int bad = tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 40, 0}};
    printf("-- the long list (36 deals)\n");
    VAR = SKAT_LONG;
    bad |= tt_run(&G, Q, 1, argc, argv);
    return bad;
}
