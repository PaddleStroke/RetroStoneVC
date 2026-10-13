/*
 * Doppelkopf's bot (src/games/doppelkopf_bot.c, trickbot.h): the core's rules against the game's (the legal cards in
 * every game: normal, marriage, the solos; a revealed deal played card by card keeps the game's points, turns and the
 * deal's score), the determinised deals keep to the seat's knowledge, the view is honest (the others' hidden cards
 * swapped: the same move), the calls are sensible (reservations and announcements counted), the cancel, the levels'
 * time, then self-play (tests/ai_selfplay.h). Headless: this file plays the screen's part (doppelkopf_live(),
 * doppelkopf_show()). A test game is the first deals of a match (3 in the short run, 4 with --full; the long
 * match's run: all 24 with --full), the halves of the table (even seats, odd seats) scored by their best total.
 * --checks: the checks alone.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include <time.h>

#include "games/doppelkopf.h"

int dkb_choose(const dkgame *g, int seat, int level, uint64_t seed, ai_stats *stats);
void dkb_view(const dkgame *g, int me, tb_state *v);
int dkb_re_points(const tb_state *st, const uint8_t *ann);
int dkb_power(int card, int game);

#define QC CARD(SUIT_CLUBS, RANK_QUEEN)

static dkgame T, U;                             /* the "screen's" game; a scratch copy */
static int VAR = DKO_SHORT, DEALS = 3, dealt, SOLOS;   /* SOLOS: the random player's reservations, percent */
const dkgame *doppelkopf_live(void) { return &T; }

/* one action of the notation, as doppelkopf_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        dko_new(&T, VAR, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!dko_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int doppelkopf_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

/* the winning half of the table (the seats' parity) by the best total after DEALS deals; 2 both halves */
static int result(void)
{
    const dkpos *p = &T.pos;
    if (!dealt || (p->phase != DKP_OVER && !(p->phase == DKP_DEAL && p->ndeals >= DEALS))) return -1;
    int best = -(1 << 20), half = 0;
    for (int q = 0; q < TK4_P; q++) best = p->total[q] > best ? p->total[q] : best;
    for (int q = 0; q < TK4_P; q++)
        if (p->total[q] == best) half |= 1 << (q & 1);
    return half == 3 ? 2 : half == 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const dkpos *p = &T.pos;
    const uint8_t *h = p->t.hand[p->turn];
    int n = p->t.n[p->turn];
    switch (p->phase) {
    case DKP_RESERVE: {
        static const char *SAY[] = {"q", "j", "f", "ss", "sh", "sd", "sc"};
        if (dko_say_ok(p, DKG_WEDDING) && ai_rand_n(r, 2)) snprintf(m, (size_t)cap, "w");
        else if (ai_rand_n(r, 100) < SOLOS) snprintf(m, (size_t)cap, "%s", SAY[ai_rand_n(r, 7)]);
        else snprintf(m, (size_t)cap, "g");
        return 1;
    }
    case DKP_PLAY: {
        if (dko_announce_ok(p) && ai_rand_n(r, 100) < 3) { snprintf(m, (size_t)cap, "a"); return 1; }
        uint8_t L[DKO_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (dko_can_play(p, h[i])) L[k++] = h[i];
        if (!k) return 0;
        char c[3];
        trick_name(L[ai_rand_n(r, k)], c);
        snprintf(m, (size_t)cap, "%s", c);
        return 1;
    }
    case DKP_DEAL: snprintf(m, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

/* the view of the seat to move with every hand revealed (a test may look), its parties the true ones */
static void reveal(tb_state *w, const dkpos *p)
{
    dkb_view(&T, p->turn, w);
    uint8_t n[TK4_P];
    for (int s = 0; s < TK4_P; s++) n[s] = p->t.n[s];
    tt_reveal(w, &p->t.hand[0][0], TK4_HAND, n);
    if (p->game == DKG_NORMAL || p->game == DKG_SILENT)
        for (int s = 0; s < TK4_P; s++) w->team[s] = (uint8_t)dko_party(p, s);
}

static int legal_count(const dkpos *p)          /* the rules' legal cards, each once */
{
    const uint8_t *h = p->t.hand[p->turn];
    int k = 0;
    for (int i = 0; i < p->t.n[p->turn]; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) dup |= h[j] == h[i];
        k += !dup && dko_can_play(p, h[i]);
    }
    return k;
}

int main(int argc, char **argv)
{
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    char name[200];
    ai_rng r;
    ai_seed(&r, 11);
    static tb_state v, w;
    {   /* the bot's card strengths are the rules' */
        static const int ranks[6] = {RANK_ACE, 9, 10, RANK_JACK, RANK_QUEEN, RANK_KING};
        int same = 1;
        for (int g = 0; g < DKG_TYPES; g++)
            for (int s = 0; s < 4; s++)
                for (int k = 0; k < 6; k++) same &= dkb_power(CARD(s, ranks[k]), g) == dko_power(CARD(s, ranks[k]), g);
        tt_check("the bot's card strengths = the rules' (every game)", same);
    }
    /* the core's rules are the game's: legal cards at every turn of every game, and a revealed deal played card by
     * card keeps the game's points, turns, and the deal's score */
    {
        int same = 1, plays = 0, tricks_ok = 1, deals = 0, games[DKG_TYPES] = {0};
        SOLOS = 40;
        for (int gn = 0; gn < 400 && same && tricks_ok; gn++) {
            dko_new(&T, gn & 1, 500 + gn);
            dealt = 1;
            int fresh = 1;
            while (T.pos.phase != DKP_OVER && T.pos.ndeals < 2 && same && tricks_ok) {
                const dkpos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == DKP_PLAY) {
                    if (fresh) reveal(&w, p), fresh = 0, games[p->game]++;
                    dkb_view(&T, p->turn, &v);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L);
                    same = k == legal_count(p);
                    for (int i = 0; i < k && same; i++) same = dko_can_play(p, L[i]);
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int was_play = p->phase == DKP_PLAY && m[0] != 'a', before = p->t.tricks, nd = p->ndeals;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && (p->t.tricks != before || p->ndeals != nd)) {
                    for (int s = 0; s < TK4_P && p->ndeals == nd; s++)
                        tricks_ok &= w.pts[s] == p->t.eyes[s] && w.turn == p->t.leader;
                    if (p->ndeals != nd) {
                        const dkdeal *d = &p->list[p->ndeals - 1];
                        tricks_ok &= tb_over(&w) && dkb_re_points(&w, d->ann) == d->re_points;
                        for (int s = 0; s < TK4_P; s++) tricks_ok &= (w.team[s] == 0) == (d->re >> s & 1);
                        if (!tricks_ok)
                            printf("     game %d deal %d (game %d): core %d, rules %d\n", gn, nd, d->game,
                                   dkb_re_points(&w, d->ann), d->re_points);
                        deals++;
                        fresh = 1;
                    }
                }
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns; games: normal %d, marriage %d, "
                 "silent %d, solos %d)", plays, games[0], games[1], games[2],
                 games[3] + games[4] + games[5] + games[6] + games[7] + games[8] + games[9]);
        tt_check(name, same && plays > 5000);
        snprintf(name, sizeof name, "a revealed deal played by the core: the same points, turns, parties and score "
                 "(%d deals)", deals);
        tt_check(name, tricks_ok && deals > 300);
    }
    /* the determinisation keeps the counts, the voids, the seat's own hand, the queens of clubs it knows of */
    {
        int ok = 1, tried = 0, silent = 0, worlds = 0;
        SOLOS = 15;
        for (int gn = 0; gn < 80 && ok; gn++) {
            dko_new(&T, DKO_SHORT, 900 + gn);
            dealt = 1;
            for (int k = 0; k < 4 + gn % 40 && T.pos.phase != DKP_DEAL; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            const dkpos *p = &T.pos;
            if (p->phase != DKP_PLAY) continue;
            dkb_view(&T, p->turn, &v);
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                tried++;
                ok &= tt_dealt_ok(&v, &w);
                int re = 0;
                for (int s = 0; s < TK4_P; s++) re += w.team[s] == 0;
                for (int s = 0; s < TK4_P; s++)   /* the parties the table knows hold */
                    if (p->game == DKG_NORMAL && dko_known(p, s) >= 0) ok &= (w.team[s] != 0) == dko_known(p, s);
                if (p->game == DKG_NORMAL) worlds++, silent += re == 1;
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, voids, own hand, the known parties (%d; a silent "
                 "solo in %d of %d normal worlds)", tried, silent, worlds);
        tt_check(name, ok && tried > 200 && silent * 5 < worlds);
    }

    const bot *b = bot_find("doppelkopf");
    tt_check("registered: bot_find(\"doppelkopf\")", b && b->think && b->play == doppelkopf_show && b->actor);

    /* honest: two other seats' hidden cards swapped (the same suit as followed, no queen of clubs): the same move */
    {
        int same = 1, tried = 0;
        SOLOS = 10;
        for (int gn = 0; gn < 40; gn++) {
            dko_new(&T, DKO_SHORT, 3000 + gn);
            dealt = 1;
            for (int k = 0; k < 8 + gn % 30 && T.pos.phase != DKP_DEAL; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            dkpos *p = &T.pos;
            if (p->phase != DKP_PLAY) continue;
            int me = p->turn, a = (me + 1) % 4, c = (me + 2) % 4, swapped = 0;
            dkgame keep = T;
            for (int i = 0; i < p->t.n[a] && !swapped; i++)
                for (int j = 0; j < p->t.n[c] && !swapped; j++) {
                    int x = p->t.hand[a][i], y = p->t.hand[c][j];
                    if (x == y || x == QC || y == QC || dko_trump(x, p->game) != dko_trump(y, p->game) ||
                        (!dko_trump(x, p->game) && CARD_SUIT(x) != CARD_SUIT(y)))
                        continue;
                    p->t.hand[a][i] = (uint8_t)y;
                    p->t.hand[c][j] = (uint8_t)x;
                    swapped = 1;
                }
            if (!swapped) continue;
            int m1 = dkb_choose(&T, me, AI_MEDIUM, 77, NULL);
            T = keep;
            int m2 = dkb_choose(&T, me, AI_MEDIUM, 77, NULL);
            same &= m1 == m2;
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    /* sensible calls: the reservations over many hands, the first announcements and the hands behind them */
    {
        int hands = 0, wed = 0, qc2 = 0, solos = 0, silent = 0, bad = 0;
        for (int gn = 0; gn < 1500; gn++) {
            dko_new(&T, DKO_SHORT, 20000 + gn);
            dealt = 1;
            const dkpos *p = &T.pos;
            int me = p->turn, a = dkb_choose(&T, me, AI_MEDIUM, gn, NULL), x = DKA_ARG(a);
            int q2 = 0;
            for (int i = 0; i < p->t.n[me]; i++) q2 += p->t.hand[me][i] == QC;
            hands++;
            qc2 += q2 == 2;
            wed += x == DKG_WEDDING;
            solos += dko_is_solo(x);
            silent += q2 == 2 && x == DKG_NORMAL;
            bad += DKA_KIND(a) != DKA_SAY || !dko_say_ok(p, x);
        }
        snprintf(name, sizeof name, "reservations: %d hands, %d solos, %d with both queens of clubs (%d marriages, %d "
                 "silent solos)", hands, solos, qc2, wed, silent);
        tt_check(name, !bad && solos * 100 <= hands * 6 && solos > 0 && wed >= qc2 * 2 / 3);
        int asked = 0, said = 0;
        double tr_said = 0, tr_not = 0;
        for (int gn = 0; gn < (full ? 400 : 120); gn++) {
            dko_new(&T, DKO_SHORT, 30000 + gn);
            dealt = 1;
            while (T.pos.phase == DKP_RESERVE) one("g");
            const dkpos *p = &T.pos;
            if (p->game != DKG_NORMAL) continue;
            int me = p->turn, a = dkb_choose(&T, me, AI_MEDIUM, gn, NULL), tr = 0;
            for (int i = 0; i < p->t.n[me]; i++) tr += dko_trump(p->t.hand[me][i], DKG_NORMAL);
            asked++;
            if (DKA_KIND(a) == DKA_ANNOUNCE) said++, tr_said += tr;
            else tr_not += tr;
        }
        snprintf(name, sizeof name, "first announcements: %d of %d leaders say Re or Kontra (trumps %.1f, the others "
                 "%.1f)", said, asked, said ? tr_said / said : 0, asked > said ? tr_not / (asked - said) : 0);
        tt_check(name, said * 100 >= asked * 3 && said * 100 <= asked * 50 && (!said || tr_said / said > tr_not /
                 (asked - said) + 1));
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        dko_new(&T, DKO_SHORT, 5);
        dealt = 1;
        while (T.pos.phase == DKP_RESERVE) one("g");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 4, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work: iterations and time on positions of random deals */
        int npos = full ? 30 : 6;
        SOLOS = 5;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0, cpu = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                dko_new(&T, DKO_SHORT, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                for (int i = 0; i < 4 + k % 30 + retry; i++) {
                    char m[BOT_MOVE_CAP];
                    random_move(&r, m, sizeof m);
                    if (m[0] != 'a') one(m);
                }
                if (T.pos.phase != DKP_PLAY || legal_count(&T.pos) < 2) { k--; continue; }
                ai_stats s;
                clock_t c0 = clock();
                double t0 = ai_now_ms();
                dkb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                cpu += (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f), processor time %6.1f ms (a choice of cards)\n",
                   sp_name(lv), nodes / npos, ms / npos, worst, cpu / npos);
        }
        double cpu = 0, worst = 0;            /* the deal's first card, hard: the most work */
        for (int k = 0; k < npos; k++) {
            dko_new(&T, DKO_SHORT, 70 + k);
            dealt = 1;
            while (T.pos.phase == DKP_RESERVE) one("g");
            clock_t c0 = clock();
            dkb_choose(&T, T.pos.turn, AI_HARD, 5, NULL);
            double c = (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
            cpu += c, worst = c > worst ? c : worst;
        }
        printf("     hard, the first card of a deal: processor time %6.1f ms (worst %6.1f)\n", cpu / npos, worst);
    }
    if (tt_fails) return 1;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--checks")) return 0;   /* the checks alone, no self-play */

    SOLOS = 0;
    static const sp_game G = {
        .id = "doppelkopf", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 75},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 45},
        {AI_HARD, AI_EASY, 40, 55},
    };
    VAR = DKO_SHORT;
    DEALS = full ? 4 : 3;
    int bad = tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 20, 0}};
    printf("-- the long match (24 deals)\n");
    VAR = DKO_LONG;
    DEALS = full ? dko_deals(VAR) : 3;
    bad |= tt_run(&G, Q, 1, argc, argv);
    return bad;
}
