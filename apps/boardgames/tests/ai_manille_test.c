/*
 * Manille's bot (src/games/manille_bot.c, trickbot.h): the core's rules against the game's (the legal cards with the
 * duty to beat and to trump, the tricks' winners and points, card by card over random deals), the determinised deals
 * (what each play showed: voids and caps; the turned card in the dealer's hand), the view keeps to what the seat may
 * know (the others' hidden cards swapped: the same move), the doubles, the cancel, then self-play at every level by
 * sides (tests/ai_selfplay.h: seats 0 and 2 one level, 1 and 3 the other). Headless: this file plays the screen's part
 * (manille_live(), manille_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/manille.h"

static mgame T, U, S;
static int dealt;
const mgame *manille_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        man_new(&T, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!man_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int manille_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == MP_OVER ? man_winner_team(&T.pos) : -1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const mpos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    switch (p->phase) {
    case MP_SPEAK: {
        int k = ai_rand_n(r, 6);
        snprintf(m, (size_t)cap, "%s", k == 0 && man_legal(p, MA_DOUBLE) ? "d" : k == 1 && man_legal(p, MA_REDOUBLE)
                                                                                     ? "r" : "p");
        return 1;
    }
    case MP_PLAY: {
        uint8_t L[MAN_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (man_can_play(p, h[i])) L[k++] = h[i];
        if (!k) return 0;
        char c[3];
        trick_name(L[ai_rand_n(r, k)], c);
        snprintf(m, (size_t)cap, "%s", c);
        return 1;
    }
    case MP_DEAL: snprintf(m, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase != MP_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 16);
    tb_state v, w;
    {   /* the core's rules are the game's */
        int same = 1, plays = 0, tricks_ok = 1, fresh = 1;
        for (int gn = 0; gn < 150 && same && tricks_ok; gn++) {
            man_new(&T, 500 + gn);
            dealt = 1;
            fresh = 1;
            while (T.pos.phase != MP_OVER && T.pos.deal < 6 && same && tricks_ok) {
                const mpos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == MP_PLAY) {
                    manb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += man_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = man_can_play(p, L[i]);
                    if (fresh) {
                        w = v, tt_reveal(&w, &p->hand[0][0], MAN_HAND, p->n), fresh = 0;
                        memset(w.pts, 0, sizeof w.pts);
                        memset(w.won, 0, sizeof w.won);
                    }
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int before = p->tricks, was_play = p->phase == MP_PLAY;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->tricks != before && p->phase == MP_PLAY) {
                    int pts[2] = {0, 0}, won[2] = {0, 0};
                    for (int s = 0; s < MAN_P; s++) pts[s & 1] += w.pts[s], won[s & 1] += w.won[s];
                    tricks_ok &= pts[0] == p->pts[0] && pts[1] == p->pts[1] && won[0] == p->won[0] &&
                                 won[1] == p->won[1] && w.turn == p->turn;
                }
                if (p->phase == MP_DEAL || p->phase == MP_OVER) tricks_ok &= tb_over(&w), fresh = 1;
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns)", plays);
        tt_check(name, same);
        tt_check("a revealed deal played by the core: the same tricks, points, turns and end", tricks_ok);
    }
    {   /* the determinisation (and what the plays showed holds in the true hands: the caps are sound) */
        int ok = 1, tried = 0, sound = 1, capped = 0;
        for (int gn = 0; gn < 150 && ok; gn++) {
            man_new(&T, 900 + gn);
            dealt = 1;
            walk(&r, 4 + gn % 36);
            const mpos *p = &T.pos;
            if (p->phase != MP_PLAY) continue;
            manb_view(&T, p->turn, &v, &S);
            for (int s = 0; s < MAN_P; s++)
                for (int u = 0; u < 4; u++) {
                    if (v.cap[s][u] == TB_NOCAP) continue;
                    capped++;
                    for (int i = 0; i < p->n[s]; i++)
                        sound &= CARD_SUIT(p->hand[s][i]) != u || man_rank(p->hand[s][i]) <= v.cap[s][u];
                }
            for (int d = 0; d < 10 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                tried++;
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, caps, own hand (%d); %d caps, all true", tried,
                 capped);
        tt_check(name, ok && sound && tried > 300 && capped > 50);
    }
    const bot *b = bot_find("manille");
    tt_check("registered: bot_find(\"manille\")", b && b->think && b->play == manille_show && b->actor);
    {   /* honest: the others' hidden cards swapped (same suit, keeping every cap): the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 80; gn++) {
            man_new(&T, 3000 + gn);
            dealt = 1;
            walk(&r, 4 + gn % 30);
            mpos *p = &T.pos;
            if (p->phase != MP_PLAY) continue;
            int me = p->turn, a = (me + 1) % 4, c = (me + 3) % 4, swapped = 0;
            manb_view(&T, me, &v, &S);
            for (int i = 0; i < p->n[a] && !swapped; i++)
                for (int j = 0; j < p->n[c] && !swapped; j++) {
                    int x = p->hand[a][i], y = p->hand[c][j], u = CARD_SUIT(x);
                    if (u != CARD_SUIT(y) || x == p->turned || y == p->turned) continue;
                    if (!tb_fits(&v, a, y) || !tb_fits(&v, c, x)) continue;
                    p->hand[a][i] = (uint8_t)y;
                    p->hand[c][j] = (uint8_t)x;
                    swapped = 1;
                }
            if (!swapped) continue;
            int m1 = manb_choose(&T, me, AI_MEDIUM, 77, NULL);
            mgame keep = T;
            man_new(&T, 3000 + gn);
            for (int i = 0; i < keep.n; i++) man_play(&T, keep.act[i]);
            same &= m1 == manb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    {   /* doubles: a doubling side makes its deal more often than not (medium against medium) */
        int doubled = 0, made = 0, deals = 0, ok = 1;
        for (int gn = 0; gn < 150; gn++) {
            man_new(&T, 6000 + gn);
            dealt = 1;
            while (T.pos.phase != MP_DEAL && T.pos.phase != MP_OVER) {
                int a = manb_choose(&T, T.pos.turn, AI_MEDIUM, 3, NULL);
                ok &= man_play(&T, a);
            }
            deals++;
            if (T.pos.doubler != MAN_NONE) doubled++, made += T.pos.made[T.pos.doubler] > MAN_HALF;
        }
        snprintf(name, sizeof name, "doubles: %d in %d deals, the doublers made %d", doubled, deals, made);
        tt_check(name, ok && doubled > 3 && doubled * 10 < deals * 8 && made * 10 >= doubled * 6);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        man_new(&T, 5);
        dealt = 1;
        while (T.pos.phase != MP_PLAY) walk(&r, 1);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 4, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                man_new(&T, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk(&r, 4 + k % 20 + retry);
                if (T.pos.phase != MP_PLAY || T.pos.n[T.pos.turn] < 3) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                manb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "manille", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 80},
        {AI_MEDIUM, AI_EASY, 100, 52},
        {AI_HARD, AI_MEDIUM, 100, 50},
        {AI_HARD, AI_EASY, 60, 55},
    };
    tt_targets(P, (int)(sizeof P / sizeof P[0]), argc, argv);
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
