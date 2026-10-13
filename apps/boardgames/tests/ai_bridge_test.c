/*
 * Contract bridge's bot (src/games/bridge_bot.c, trickbot.h): the core's rules against the game's (the legal cards,
 * the tricks' winners in each strain, card by card over random deals), the determinised deals (the dummy where it
 * lies once shown), the view keeps to what the seat may know (the hidden hands' cards swapped: the same move), the
 * auction (sound contracts: the levels against the points, the contracts made), the dummy played by its declarer, the
 * cancel, then self-play at every level by sides (tests/ai_selfplay.h: seats 0 and 2 one level, 1 and 3 the other;
 * Chicago, and a rubber briefly). Headless: this file plays the screen's part (bridge_live(), bridge_show()). --full
 * for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/bridge.h"

static brg_game T, U, S;
static int VARIANT = BRG_CHICAGO, dealt;
const brg_game *bridge_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        brg_new(&T, VARIANT, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!brg_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int bridge_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? brg_controller(&T.pos) : 0; }
static int result(void) { return dealt && T.pos.phase == BRG_OVER ? brg_winner(&T.pos) : -1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    static const char strains[] = "cdhsn";
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const brg_pos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    switch (p->phase) {
    case BRG_AUCTION: {
        int k = ai_rand_n(r, 10), c = p->bid + 1 + ai_rand_n(r, 8);
        if (k == 0 && brg_can_call(p, BRG_DOUBLE)) { snprintf(m, (size_t)cap, "x"); return 1; }
        if (k <= 2 && c <= 35) {
            snprintf(m, (size_t)cap, "b%d%c", BRG_LEVEL(c), strains[BRG_STRAIN(c)]);
            return 1;
        }
        snprintf(m, (size_t)cap, "p");
        return 1;
    }
    case BRG_PLAY: {
        uint8_t L[BRG_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (brg_can_play(p, h[i])) L[k++] = h[i];
        if (!k) return 0;
        char c[3];
        trick_name(L[ai_rand_n(r, k)], c);
        snprintf(m, (size_t)cap, "%s", c);
        return 1;
    }
    case BRG_SCORE: snprintf(m, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase != BRG_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

int main(int argc, char **argv)
{
    char name[200];
    ai_rng r;
    ai_seed(&r, 18);
    tb_state v, w;
    {   /* the core's rules are the game's */
        int same = 1, plays = 0, tricks_ok = 1, fresh = 1, strains[5] = {0};
        for (int gn = 0; gn < 150 && same && tricks_ok; gn++) {
            brg_new(&T, gn & 1, 500 + gn);
            dealt = 1;
            fresh = 1;
            while (T.pos.phase != BRG_OVER && T.pos.dealt < 8 && same && tricks_ok) {
                const brg_pos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == BRG_PLAY) {
                    brb_view(&T, brg_controller(p), &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += brg_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = brg_can_play(p, L[i]);
                    if (fresh) w = v, tt_reveal(&w, &p->hand[0][0], BRG_HAND, p->n), fresh = 0, strains[p->strain]++;
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int before = p->tricks, was_play = p->phase == BRG_PLAY;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->tricks != before && p->phase == BRG_PLAY)
                    for (int s = 0; s < BRG_P; s++) tricks_ok &= w.won[s] == p->won[s] && w.turn == p->turn;
                if (was_play && p->phase != BRG_PLAY) tricks_ok &= tb_over(&w);
                if (p->phase != BRG_PLAY) fresh = 1;
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns; %d %d %d %d %d deals by strain)",
                 plays, strains[0], strains[1], strains[2], strains[3], strains[4]);
        tt_check(name, same);
        tt_check("a revealed deal played by the core: the same tricks, turns and end", tricks_ok);
    }
    {   /* the determinisation: the dummy where it lies */
        int ok = 1, tried = 0, dummy_seen = 0;
        for (int gn = 0; gn < 200 && ok; gn++) {
            brg_new(&T, BRG_CHICAGO, 900 + gn);
            dealt = 1;
            walk(&r, 4 + gn % 60);
            const brg_pos *p = &T.pos;
            if (p->phase != BRG_PLAY) continue;
            int me = brg_controller(p);
            brb_view(&T, me, &v, &S);
            int d = brg_dummy(p);
            if (brg_dummy_shown(p) && d != me) dummy_seen++, ok &= v.nknown[d] == p->n[d];
            for (int k = 0; k < 10 && ok; k++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                tried++;
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, voids, own hand, the dummy (%d, %d with it)",
                 tried, dummy_seen);
        tt_check(name, ok && tried > 300 && dummy_seen > 20);
    }
    const bot *b = bot_find("bridge");
    tt_check("registered: bot_find(\"bridge\")", b && b->think && b->play == bridge_show && b->actor);
    {   /* honest: the two hidden hands' cards swapped (same suit): the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 200 && tried < 50; gn++) {
            brg_new(&T, BRG_CHICAGO, 3000 + gn);
            dealt = 1;
            walk(&r, 4 + gn % 50);
            brg_pos *p = &T.pos;
            if (p->phase != BRG_PLAY || !brg_dummy_shown(p)) continue;
            int me = brg_controller(p), d = brg_dummy(p), hid[2], nh = 0;
            for (int s = 0; s < BRG_P; s++)
                if (s != me && s != d) hid[nh++] = s;
            if (nh != 2) continue;
            int swapped = 0;
            for (int i = 0; i < p->n[hid[0]] && !swapped; i++)
                for (int j = 0; j < p->n[hid[1]] && !swapped; j++)
                    if (CARD_SUIT(p->hand[hid[0]][i]) == CARD_SUIT(p->hand[hid[1]][j])) {
                        uint8_t t = p->hand[hid[0]][i];
                        p->hand[hid[0]][i] = p->hand[hid[1]][j];
                        p->hand[hid[1]][j] = t;
                        swapped = 1;
                    }
            if (!swapped) continue;
            int m1 = brb_choose(&T, me, AI_MEDIUM, 77, NULL);
            brg_game keep = T;
            brg_new(&T, BRG_CHICAGO, 3000 + gn);
            for (int i = 0; i < keep.n; i++) brg_play(&T, keep.act[i]);
            same &= m1 == brb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
        }
        snprintf(name, sizeof name, "the hidden hands' cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 30);
    }
    {   /* the auction: medium against medium, 200 deals: few passed out, the levels follow the points, the contracts
         * mostly made; the dummy's cards come from its declarer */
        int deals = 0, passed = 0, contracts = 0, made = 0, sound = 0, games = 0, ok = 1, dummy_by_declarer = 1;
        for (int gn = 0; gn < 200; gn++) {
            brg_new(&T, BRG_CHICAGO, 6000 + gn);
            dealt = 1;
            const brg_pos *p = &T.pos;
            int hcp[2] = {0, 0};
            for (int s = 0; s < BRG_P; s++) hcp[s & 1] += tb_hcp(p->hand[s], p->n[s]);
            while (p->phase == BRG_AUCTION || p->phase == BRG_PLAY) {
                int seat = brg_controller(p);
                if (p->phase == BRG_PLAY && p->turn == brg_dummy(p)) dummy_by_declarer &= seat == p->declarer;
                int a = brb_choose(&T, seat, p->phase == BRG_AUCTION ? AI_HARD : AI_EASY, 3 + gn, NULL);
                ok &= brg_play(&T, a);
                if (!ok) break;
            }
            deals++;
            if (p->passed_out) { passed++; continue; }
            const brg_deal *rec = &p->rec[p->nrec - 1];
            int side = rec->declarer & 1, lv = rec->level, pts = hcp[side];
            contracts++;
            made += rec->taken >= 6 + lv;
            games += lv >= 4 || (lv == 3 && rec->strain == BRG_NT);
            sound += (lv <= 2 && pts <= 26) || (lv >= 3 && lv <= 5 && pts >= 21) || (lv >= 6 && pts >= 29);
        }
        snprintf(name, sizeof name, "auctions: %d deals, %d passed out, %d contracts (%d games), %d made, %d sound by "
                 "the points", deals, passed, contracts, games, made, sound);
        tt_check(name, ok && passed * 5 < deals && made * 10 >= contracts * 5 && sound * 10 >= contracts * 8 &&
                       games * 5 >= contracts && dummy_by_declarer);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        brg_new(&T, BRG_CHICAGO, 5);
        dealt = 1;
        one("b1n");
        one("p");
        one("p");
        one("p");
        one(".");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(brg_controller(&T.pos), AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", T.pos.phase == BRG_PLAY && !ok && dt < 50);
        tt_check("not the controller: no move", !b->think(brg_dummy(&T.pos), AI_HARD, m, sizeof m));
    }
    {   /* the levels' work */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                brg_new(&T, BRG_CHICAGO, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk(&r, 6 + k % 30 + retry);
                if (T.pos.phase != BRG_PLAY || T.pos.n[T.pos.turn] < 4) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                brb_choose(&T, brg_controller(&T.pos), lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "bridge", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 45},
        {AI_HARD, AI_EASY, 40, 55},
    };
    printf("-- Chicago\n");
    VARIANT = BRG_CHICAGO;
    int bad = tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 20, 70}};
    printf("-- a rubber\n");
    VARIANT = BRG_RUBBER;
    bad |= tt_run(&G, Q, 1, argc, argv);
    return bad;
}
