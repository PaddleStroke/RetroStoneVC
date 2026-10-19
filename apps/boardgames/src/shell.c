/*
 * RetroStone Board: the shell (shell.h). One app for all the board games: the menu (a card per game), the variant
 * picker, the players' bars, the buttons, the end-of-game card, the sounds, and a picture mode for tests and
 * screenshots. The games bring their rules and their boards (games/<id>.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   retrostoneboard [--save DIR]      the app (a window; full screen on Android); its settings and progress kept in
 *                                     the device's folder for apps (SDL_GetPrefPath), or in DIR
 *   retrostoneboard --shot out.bmp [--size 720x1280] [--game chess] [--variant 0] [--moves "e2e4 e7e5"]
 *                   [--taps "0.5,0.9 0.5,0.7"] [--drag "0.1,0.9 0.5,0.5"] [--hold "0.5,0.5 800"]
 *                   [--slide "0.1,0.9 0.1,0.5 0.5,0.5"] [--flip] [--f2f]
 *                   [--keep] [--pick] [--players n] [--reveal] [--at ms] [--level n] [--solved "1-12:3 13:2"]
 *                   [--keys "Up Left"] [--random] [--icons] [--save DIR] [--bots "1:h,2:m"] [--steps n]
 *                                     one picture, no window: the menu (no --game), the picker (--pick) or a game
 *                                     after its moves (its own notation) and gestures (fractions of the board; of the
 *                                     screen on the menu and the pickers); the hand-off cover lifted (--reveal: the
 *                                     start button tapped); a clock of its own (the same picture every time); nothing
 *                                     kept from a past run (--save DIR: that folder's); a game with levels:
 *                                     --pick its level screen, --level n (1..) that level, --solved the progress;
 *                                     --random a random puzzle of a game of levels (--variant n); --pick --tab n,
 *                                     n its chapters' count: its random tab; --bots the seats the device plays
 *                                     (players from 0: e easy, m medium, h hard; "all:m"), --steps n: the bots play
 *                                     n moves after --moves (each move logged on stderr)
 *   retrostoneboard --perf menu|menu-warm|starts|cards|dump DIR|<game> [--size WxH] [--worker] [--pace] [--only "ids"]
 *                                     the frames timed, no window (a developer's measure: a phone is some 6 times
 *                                     slower): the menu's tabs and scrolls (menu-warm: its pictures painted ahead
 *                                     first; --worker: by the app's helper thread), every game's start, the decks'
 *                                     faces, a game's first frames; dump: every game's pictures as raw RGBA
 *                                     (RSB_ART_REF=1: the reference painter's, to compare)
 * Keys: Esc / Back home (in the menu: quit), U undo, F flip, T face to face, N new game, M sound.
 */
#include "shell.h"
#include "ai.h"
#include "arcade.h"
#include "bots.h"
#include "cards.h"
#include "edition.h"
#include "games/chess.h"
#include "iap.h"
#include "pages.h"
#include "stats.h"
#include "text.h"
#ifdef __IPHONEOS__
#include "ios_app.h"                            /* the iPhone and iPad app (apps/ios): the safe area */
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GAMES 256
#define MAX_VARIANTS 15                         /* a picker card each (the thumbnails: one more, the menu's) */

struct ui ui = {.players = 2, .level = -1};
static const game *games[MAX_GAMES];
static int ngames;
static SDL_Window *win;
static int quit, need_frame;

enum { SCR_MENU, SCR_PICK, SCR_GAME, SCR_LEVELS };   /* SCR_LEVELS: the picker of a game with levels */
static int screen, cur = -1;                    /* the game shown (games[cur]) */
static int variant[MAX_GAMES], started[MAX_GAMES];   /* variant: a game with levels, the level */
static int chosen[MAX_GAMES];                   /* a variant was chosen (this run, or kept from the last): the
                                                 * picker shows it in gold light */
static int nplayers[MAX_GAMES];                 /* how many play each game (the picker's choice; 0: not chosen) */
/* a game of levels that also plays at random (start_random): in random play (the level screen's last tab), and the
 * variant picked there */
static int rnd_mode[MAX_GAMES], rvariant[MAX_GAMES];
/* the free set in play: the free edition (edition.h) until its in-app purchase unlocks everything (iap.h; never in
 * the builds without one, where the edition alone decides) */
static int free_now(void) { return ed_free && !iap_unlocked(); }
/* (the free edition: no random play, its levels only) */
static int has_random(int g) { return games[g]->levels > 0 && games[g]->start_random && !free_now(); }
static int in_random(int g) { return has_random(g) && rnd_mode[g]; }

/* the hand-off (VIEW_* games): whose view each game shows (kept while away in the menu); the cover (up: hiding the
 * screen until `player` taps start); a hand-off waiting for the last move to land (pending: the player, or -1) */
static int view_of[MAX_GAMES];
static struct { int up, player, pending; } hand = {0, 0, -1};
static int is_view(void) { return cur >= 0 && games[cur]->view != VIEW_BOARD; }
static unsigned winners_of(int st);

/* the bots (bots.h): each game's seats (0 a person, else 1 + the bot's level AI_EASY..AI_HARD: the picker's choice,
 * kept), the seats given back to the people in the game in play (a move refused), and what that makes of the game on
 * the screen: its bot (NULL: none, or a game of levels), the seats the device plays (botmask) */
static uint8_t seat_lv[MAX_GAMES][MAX_PLAYERS];
static unsigned bot_off[MAX_GAMES];
static const bot *cbot;
static unsigned botmask;
static int is_bot(int p) { return p >= 0 && p < MAX_PLAYERS && (botmask >> p & 1); }

/* who plays a seat: a person at this device, the device's bot, or (later, play between devices:
 * docs/multiplayer-ready.md) a player elsewhere whose moves come in the game's notation. Nothing makes a remote seat
 * yet (remotemask stays 0). What only a person here gets (the input, the view and the cover, face to face, the pass
 * button, undo back to their move) asks seat_is_local_person(); a seat whose move comes from elsewhere than this
 * device's input (a bot's worker; the network) asks seat_waits_for_move(): the input is held meanwhile. is_bot()
 * stays for what is a bot's own (its thinking, its level). A seat out of range is nobody's: taken as a person's. */
enum { SEAT_PERSON, SEAT_BOT, SEAT_REMOTE };
static unsigned remotemask;                     /* the remote seats in the game on the screen (none yet) */
static int seat_kind(int p)
{
    if (p < 0 || p >= MAX_PLAYERS) return SEAT_PERSON;
    return botmask >> p & 1 ? SEAT_BOT : remotemask >> p & 1 ? SEAT_REMOTE : SEAT_PERSON;
}
static int seat_waits_for_move(int p) { return seat_kind(p) != SEAT_PERSON; }
static int seat_is_local_person(int p) { return seat_kind(p) == SEAT_PERSON; }
static unsigned elsewhere(void) { return botmask | remotemask; }   /* the seats no person here plays */
static int humans(void) { return __builtin_popcount(((1u << ui.players) - 1) & ~elsewhere()); }

/* the menu's sections: a tab each (a heading in a search: its line of help/<lang>/_menu.txt), then their games. The
 * numbers are kept (the tab remembered, the Stats page's sections, _menu.txt's lines): a new section takes the next
 * one, and SEC_SHOWN puts it in its place in the row. SEC_ARCADE: the RetroStone app's Arcade games (arcade.h; none
 * in the other editions: no tab), STATS_ARCADE. SEC_LOGIC: the logic grids and the number puzzles, the puzzles'
 * harder half (is_puzzle_sec: the shell treats both alike). */
enum { SEC_PUZZLE, SEC_STRATEGY, SEC_CHESS, SEC_RACE, SEC_CARDS, SEC_GUESS, SEC_ARCADE, SEC_LOGIC, SECTIONS };
static const int SEC_SHOWN[SECTIONS] = {SEC_PUZZLE, SEC_LOGIC, SEC_STRATEGY, SEC_CHESS, SEC_RACE, SEC_CARDS, SEC_GUESS,
                                        SEC_ARCADE};
static int is_puzzle_sec(int s) { return s == SEC_PUZZLE || s == SEC_LOGIC; }
static int sec_rank(int s)                      /* its place in the row */
{
    for (int k = 0; k < SECTIONS; k++)
        if (SEC_SHOWN[k] == s) return k;
    return SECTIONS;
}
/* each game's section; the games not listed are strategy games (an id not built is no harm). The puzzles and the
 * logic grids come in the order of their lines here (the easy, quick ones first), before game.order; the other
 * sections by game.order. */
static const struct { const char *id; int section; } SECTION_OF[] = {
    {"chess", SEC_CHESS}, {"makruk", SEC_CHESS}, {"shogi", SEC_CHESS}, {"xiangqi", SEC_CHESS},
    {"janggi", SEC_CHESS}, {"jungle", SEC_CHESS}, {"backgammon", SEC_RACE}, {"ur", SEC_RACE}, {"ludo", SEC_RACE},
    {"pachisi", SEC_RACE}, {"snakes", SEC_RACE}, {"shutthebox", SEC_RACE}, {"yacht", SEC_RACE},
    {"farkle", SEC_RACE}, {"fourtwoone", SEC_RACE}, {"liarsdice", SEC_RACE}, {"golf", SEC_CARDS},
    {"tilerummy", SEC_CARDS}, {"tarot", SEC_CARDS}, {"belote", SEC_CARDS}, {"crazyeights", SEC_CARDS},
    {"hearts", SEC_CARDS}, {"dominoes", SEC_CARDS}, {"cribbage", SEC_CARDS}, {"ginrummy", SEC_CARDS},
    {"spades", SEC_CARDS}, {"president", SEC_CARDS}, {"war", SEC_CARDS}, {"oldmaid", SEC_CARDS},
    {"families", SEC_CARDS}, {"secretcode", SEC_GUESS}, {"mysteryface", SEC_GUESS}, {"seabattle", SEC_GUESS},
    {"wordtiles", SEC_CARDS}, {"klondike", SEC_CARDS}, {"freecells", SEC_CARDS}, {"spider", SEC_CARDS},
    {"mahjong", SEC_CARDS}, {"goose", SEC_RACE}, {"nainjaune", SEC_CARDS}, {"crapette", SEC_CARDS},
    {"manille", SEC_CARDS}, {"slap", SEC_CARDS}, {"mahjong4", SEC_CARDS}, {"koikoi", SEC_CARDS},
    {"scopa", SEC_CARDS}, {"briscola", SEC_CARDS}, {"skat", SEC_CARDS}, {"euchre", SEC_CARDS},
    {"canasta", SEC_CARDS}, {"pairs", SEC_GUESS}, {"pyramid", SEC_CARDS}, {"threepeaks", SEC_CARDS},
    {"durak", SEC_CARDS}, {"barbu", SEC_CARDS}, {"rikiki", SEC_CARDS}, {"piquet", SEC_CARDS},
    {"ecarte", SEC_CARDS}, {"bigtwo", SEC_CARDS}, {"tienlen", SEC_CARDS}, {"doppelkopf", SEC_CARDS},
    {"schafkopf", SEC_CARDS}, {"jass", SEC_CARDS}, {"klaverjas", SEC_CARDS}, {"mus", SEC_CARDS},
    {"truco", SEC_CARDS}, {"bridge", SEC_CARDS}, {"bellelucie", SEC_CARDS}, {"yukon", SEC_CARDS},
    {"fortythieves", SEC_CARDS}, {"golfpatience", SEC_CARDS}, {"canfield", SEC_CARDS}, {"montana", SEC_CARDS},
    {"clock", SEC_CARDS}, {"banqi", SEC_CHESS}, {"mexico", SEC_RACE}, {"pig", SEC_RACE}, {"shipcaptain", SEC_RACE},
    {"senet", SEC_RACE}, {"scorpion", SEC_CARDS}, {"bakersdozen", SEC_CARDS}, {"accordion", SEC_CARDS},
    {"whist", SEC_CARDS}, {"pinochle", SEC_CARDS}, {"spy", SEC_GUESS}, {"werewolves", SEC_GUESS},
    {"drawguess", SEC_GUESS}, {"carrom", SEC_STRATEGY}, {"pool", SEC_STRATEGY}, {"mapquiz", SEC_GUESS},
    /* the puzzles: the casual ones first (a move or two to learn, satisfying at once), the problems of chess, go and
     * shogi last */
    {"rollblock", SEC_PUZZLE}, {"diceroll", SEC_PUZZLE}, {"paintroller", SEC_PUZZLE},
    {"arrowescape", SEC_PUZZLE}, {"loopclose", SEC_PUZZLE}, {"hexstack", SEC_PUZZLE}, {"pins", SEC_PUZZLE},
    {"gears", SEC_PUZZLE}, {"colourmix", SEC_PUZZLE}, {"rings", SEC_PUZZLE}, {"untangle", SEC_PUZZLE},
    {"pipes", SEC_PUZZLE}, {"watersort", SEC_PUZZLE}, {"ballsort", SEC_PUZZLE}, {"iceslide", SEC_PUZZLE},
    {"twinwalk", SEC_PUZZLE}, {"tiltmaze", SEC_PUZZLE}, {"paperfold", SEC_PUZZLE}, {"dominochain", SEC_PUZZLE},
    {"trains", SEC_PUZZLE}, {"parking", SEC_PUZZLE}, {"klotski", SEC_PUZZLE}, {"taquin", SEC_PUZZLE},
    {"merge", SEC_PUZZLE}, {"flood", SEC_PUZZLE}, {"samegame", SEC_PUZZLE}, {"blocks", SEC_PUZZLE},
    {"tileconnect", SEC_PUZZLE}, {"jigsaw", SEC_PUZZLE}, {"tangram", SEC_PUZZLE}, {"spotdiff", SEC_PUZZLE},
    {"twistycube", SEC_PUZZLE}, {"loopshift", SEC_PUZZLE}, {"logicgates", SEC_PUZZLE}, {"defuse", SEC_PUZZLE},
    {"mirrors", SEC_PUZZLE},
    {"lights", SEC_PUZZLE}, {"paths", SEC_PUZZLE}, {"boxpusher", SEC_PUZZLE}, {"hanoi", SEC_PUZZLE},
    {"pegsolitaire", SEC_PUZZLE}, {"mines", SEC_PUZZLE}, {"melody", SEC_PUZZLE}, {"pianofall", SEC_PUZZLE},
    {"chesspuzzles", SEC_PUZZLE}, {"goproblems", SEC_PUZZLE}, {"shogimate", SEC_PUZZLE},
    /* the logic grids, easy to hard: the sudokus, the number puzzles, then the pencil-and-paper grids */
    {"sudoku", SEC_LOGIC}, {"diagonalsudoku", SEC_LOGIC}, {"killersudoku", SEC_LOGIC}, {"jigsawsudoku", SEC_LOGIC},
    {"mentalmath", SEC_LOGIC}, {"mathpz", SEC_LOGIC}, {"balance", SEC_LOGIC}, {"sumerase", SEC_LOGIC},
    {"mathcross", SEC_LOGIC}, {"magicsquare", SEC_LOGIC},
    {"calcudoku", SEC_LOGIC}, {"kakuro", SEC_LOGIC}, {"futoshiki", SEC_LOGIC},
    {"skyscrapers", SEC_LOGIC}, {"binairo", SEC_LOGIC}, {"nonogram", SEC_LOGIC}, {"tents", SEC_LOGIC},
    {"queens", SEC_LOGIC}, {"starbattle", SEC_LOGIC}, {"fleet", SEC_LOGIC}, {"lightup", SEC_LOGIC},
    {"knightstour", SEC_LOGIC}, {"numberpath", SEC_LOGIC}, {"hashi", SEC_LOGIC}, {"dominosa", SEC_LOGIC},
    {"rectangles", SEC_LOGIC}, {"thermometers", SEC_LOGIC}, {"tracks", SEC_LOGIC}, {"norinori", SEC_LOGIC},
    {"lits", SEC_LOGIC}, {"hitori", SEC_LOGIC}, {"loop", SEC_LOGIC}, {"masyu", SEC_LOGIC}, {"nurikabe", SEC_LOGIC},
    {"fillomino", SEC_LOGIC}, {"heyawake", SEC_LOGIC}, {"tapa", SEC_LOGIC}, {"yinyang", SEC_LOGIC},
};

static int section_line(const game *g)          /* its line of SECTION_OF (-1: none) */
{
    for (int i = 0; i < (int)(sizeof SECTION_OF / sizeof SECTION_OF[0]); i++)
        if (!strcmp(SECTION_OF[i].id, g->id)) return i;
    return -1;
}

static int section_of(const game *g)
{
    if (arcade_of(g) >= 0) return SEC_ARCADE;
    int i = section_line(g);
    return i >= 0 ? SECTION_OF[i].section : SEC_STRATEGY;
}

/* the free edition (edition.h): every puzzle (its first ed_free_levels levels) and the games of ed_free_games are
 * played; the others show locked (after the playable ones in their tab), and a tap opens the full version's panel.
 * The paid app, or the free one once its in-app purchase is made (free_now): nothing locked. */
static int locked_game(const game *g)
{
    if (!free_now() ||!ed_free_games[0] || is_puzzle_sec(section_of(g))) return 0;
    const char *id = g->id;
    size_t n = strlen(id);
    for (const char *s = ed_free_games; (s = strstr(s, id)) != NULL; s += n)
        if ((s == ed_free_games || s[-1] == ' ') && (s[n] == 0 || s[n] == ' ')) return 0;
    return 1;
}
static int locked(int g) { return locked_game(games[g]); }

/* the frame round a game's board (art_frame_light, this thread's): the light theme's puzzles in light maple, every
 * other one (and the dark theme) in walnut. Set for the game about to paint: cur's, or a card's (then cur's again) */
static int frame_light_of(int g) { return g >= 0 && !TH.dark && is_puzzle_sec(section_of(games[g])); }
static void frame_for(int g) { art_frame_light(frame_light_of(g)); }
static int level_locked(int l) { return free_now() && ed_free_levels > 0 && l >= ed_free_levels; }
static void full_open(void);

/* the counts a game can be played with: bit n, n players */
static unsigned player_counts(int g) { return games[g]->players ? games[g]->players : 1u << 2; }
static int many_counts(int g) { unsigned c = player_counts(g); return (c & (c - 1)) != 0; }

static int players_of(int g)
{
    unsigned c = player_counts(g);
    if (!nplayers[g] || !(c >> nplayers[g] & 1)) {     /* the usual count, else two if it can, else the fewest */
        int u = games[g]->players_usual;
        nplayers[g] = u > 0 && c >> u & 1 ? u : c >> 2 & 1 ? 2 : 1;
        while (!(c >> nplayers[g] & 1)) nplayers[g]++;
    }
    return nplayers[g];
}
static box grid_ext;                            /* the board as the game fitted it (ui_grid_fit; w 0: not known) */
static int last_status, over_hidden, instant;   /* instant: the picture mode (no pause before the end card) */
static Uint32 end_seen;                         /* when the end of the game was first shown (0: not yet) */
#define END_PAUSE 900                           /* ms of the final position before the end card */
#define SOLVED_W 4.6f                           /* a solved puzzle's card: its width in its own unit (3.8 high; 3 with
                                                 * no moves shown), 0.7 of the screen's short side */
static Uint32 armed;                            /* "new game" tapped once: a second tap within 2.5 s confirms */

enum { B_HOME, B_UNDO, B_FLIP, B_F2F, B_NEW, B_HELP, NBTN };
static const int BTN_ICON[NBTN] = {ICON_HOME, ICON_UNDO, ICON_FLIP, ICON_F2F, ICON_NEW, ICON_HELP};

static struct {
    box board, bar[MAX_PLAYERS];                /* bar[player] */
    box room;                                   /* all the room between the bars (the board: a square in it) */
    float bx[NBTN], by[NBTN], br;               /* the buttons: centres, radius */
    float tk;                                   /* a token's size */
    /* the menu and the picker */
    int cols, card, label;                      /* label: the height of a card's name under its picture */
    box cards[MAX_GAMES];
    float snd_x, snd_y, snd_r;
    int row;                                    /* a picker's or the level screen's slim row under its header: */
    float row_y, row_r;                         /* the stars won at the left, the resume button at the right */
    box pills[MAX_PLAYERS + 1];                 /* the picker: how many players (pill_n[i] each) */
    int npills, pill_n[MAX_PLAYERS + 1], pill_rows;
    box seats[MAX_PLAYERS];                     /* the picker of a game with a bot: who plays each seat */
    int nseats;
    box online;                                 /* after them, the online chip (greyed: coming later; w 0: none) */
    float sec_y[SECTIONS], sec_x, sec_w, sec_h;   /* the menu's headings (sec_y -1: an empty section) */
    box tabs[SECTIONS];                         /* the menu's tabs, one a section with games (in the row: menu.tx
                                                 * scrolls them; tab_row: the row's window) */
    box tab_row;
    int ntabs, tab_sec[SECTIONS];
} L;

static SDL_Texture *t_bg, *t_btn, *t_btn_armed, *t_btn_on, *t_card, *t_gold, *t_shadow, *t_ring,
    *t_glow, *t_tok[MAX_PLAYERS], *t_big[MAX_PLAYERS], *t_bartex, *t_menu_card, *t_menu_card_on, *t_thumb[MAX_GAMES][MAX_VARIANTS + 1],
    *t_name[MAX_GAMES], *t_pill, *t_pill_on, *t_menu_shadow, *t_ptok[MAX_PLAYERS], *t_sec[SECTIONS],
    *t_tab, *t_tab_on, *t_tabicon[SECTIONS];
static float made_sec_w;
static int made_unit = -1, made_game = -1, made_card = -1, made_pill = -1, made_tab = -1, bar_w, bar_h, bar_m;
/* a card picture's size, and when it began to fade in (1: waited for, a placeholder shown; 0: not fading) */
static int t_thumb_sz[MAX_GAMES][MAX_VARIANTS + 1];
static Uint32 t_thumb_at[MAX_GAMES][MAX_VARIANTS + 1];
static int tok_gen;                            /* the tokens made again (ui_tokens_changed): the cover's too */
static int text_gen;                           /* the language switched (Settings): words made again */

void ui_tokens_changed(void)
{
    made_game = made_pill = -1;
    tok_gen++;
    need_frame = 1;
}

/* the rules page (a game's, or the app's from the menu): the text drawn in tiles, scrolled */
#define TILE 512
#define MAX_TILES 64
static struct {
    int open, game;                             /* game: games[game], -1 the app's */
    box card, view;                             /* the page, the text's window in it */
    float close_x, close_y, close_r, px;
    int h, tiles, made_w;
    SDL_Texture *tile[MAX_TILES];
    float scroll, grab_y, grab_scroll;
    int grabbing;
} help;

/* the menu: more games than a screen holds, so the cards scroll under the buttons (dragged, or the mouse wheel) */
static struct {
    float scroll, max, top;                     /* top: where the cards' window starts, below the buttons */
    float grab_x, grab_y, grab_scroll, vel;     /* vel: the fling after a drag, pixels a ms */
    int grabbing, moved, axis;                  /* axis: the drag's, 1 sideways (the tabs), 2 up and down */
    int tab;                                    /* the section shown (its tab) */
    Uint32 t;                                   /* the last drag or fling step */
    /* the tabs' row, when they do not all fit: scrolled by tx (0..tmax), dragged (tgrab: the press was on the row;
     * tmoved: it went sideways, no tab chosen), flung (tvel), or sliding to tgoal (-1: none) to show the tab chosen */
    float tx, tmax, tgoal, tvel, tgrab_x, tgrab_tx;
    int tgrab, tmoved, tkey;                    /* tkey: the row as laid out (another: the tab chosen shown at once) */
    Uint32 tt;
} menu = {.tgoal = -1};

/* the level screen (a game with levels): the chapters' tabs, the chapter's levels as tiles */
#define MAX_LEVELS 2048
#define MAX_CHAPTERS 12
#define CHAPTER_MAX 100                         /* the most levels a chapter shows */
static struct {
    int g;                                      /* whose progress best[] holds (-1: none read yet) */
    uint8_t best[MAX_LEVELS];                   /* a level each: 0 not solved, else 1 + its best stars */
    int chapter, nch, first[MAX_CHAPTERS + 1];  /* the tab shown; the chapters (their first levels) */
    box tabs[MAX_CHAPTERS + 1], tiles[CHAPTER_MAX];   /* tabs: the chapters', then random play's */
    float grab_x, grab_y;
    int grabbing, moved;
} LV = {.g = -1};
/* the solved puzzle's card: its next and replay buttons as last drawn (d 0: none); when it was first shown (its
 * stars pop in) */
static struct { float x, y, d; } next_btn, replay_btn;
static Uint32 card_t0;

/* the pointer (the mouse, or the finger SDL makes it from) as the game knows it. game: its press went to the game (its
 * moves and its lifting go there too); stray: it was down when the game ended (a puzzle solved, an end card shown), or
 * it fell on the card: its moves and its lifting are nobody's. x, y: where the game last had it. Several fingers (a
 * game's finger()): the ones the game holds, the stray ones. */
#define MAX_FINGERS 10
static struct {
    int game, stray, live;                      /* live: pressed while the game was on (not a tool once solved) */
    float x, y;
    long long held[MAX_FINGERS], strays[MAX_FINGERS];
    int nheld, nstrays;
} PT;
static void fingers_stray(void);

/* a puzzle solved (a game of levels, or one player with stars): its stars card, which only its replay and next
 * buttons take away (or undo); the game hears nothing meanwhile but its tools */
static int solved_card(void)
{
    if (screen != SCR_GAME || cur < 0) return 0;
    const game *g = games[cur];
    return g->status() == GS_WIN0 && (g->levels || (ui.players == 1 && g->level_stars));
}

/* the game's gesture still going on: its pointer (or its fingers) down. A game that ends under it (a puzzle solved by
 * a drag, a move that wins) keeps hearing that gesture to its lifting, and its end waits: the tune, the pause, then
 * the card come after all its fingers are up; a gesture that undoes the solution (the pin moved back) leaves no end. */
static int gesture_held(void) { return screen == SCR_GAME && ((PT.game && PT.live) || PT.nheld > 0); }

/* ---- the registry ------------------------------------------------------------------------------------------------ */
void game_register(const game *g)
{
    if (ngames < MAX_GAMES) games[ngames++] = g;
}

static int by_order(const void *a, const void *b)
{
    const game *ga = *(const game *const *)a, *gb = *(const game *const *)b;
    int sa = section_of(ga), sb = section_of(gb);
    if (sa != sb) return sec_rank(sa) - sec_rank(sb);   /* the sections in the tabs' order */
    int la = locked_game(ga), lb = locked_game(gb);
    if (la != lb) return la - lb;               /* the free edition: the playable games first */
    if (is_puzzle_sec(sa) && section_line(ga) != section_line(gb))   /* the puzzles: SECTION_OF's order */
        return section_line(ga) - section_line(gb);
    if (ga->order != gb->order) return ga->order - gb->order;
    return strcmp(ga->id, gb->id);              /* the same order: by id */
}

/* ---- helpers for the games --------------------------------------------------------------------------------------- */
int ui_bottom(void)
{
    if (is_view()) return ui.viewer >= 0 ? ui.viewer : hand.player;   /* the viewer, at the bottom */
    return ui.flipped ? 1 : 0;
}

/* a player's bar above the board (then turned round face to face): the game's home(), else the player not at the
 * bottom (VIEW_* games: everyone but the viewer) */
static int seat_top(int p)
{
    if (is_view() || cur < 0 || !games[cur]->home) return p != ui_bottom();
    float fx, fy;
    games[cur]->home(p, &fx, &fy);
    return ui.flipped ? fy > 0.5f : fy < 0.5f;
}

/* face to face turns the people's pieces only (a bot's seat faces nobody) */
int ui_upside(int player) { return ui.f2f && !is_view() && seat_top(player) && seat_is_local_person(player); }
/* the picture mode's clock is its own (shot_clock): it stands still, and moves on only past a gesture (its
 * animations have ended) or to --at ms after the last one, so the same picture comes out every time */
static Uint32 ms_ahead;
static int shot_clock;
static int rp_on;                               /* a kept game played again (the shell's resume): its own clock */
static Uint32 rp_now;
static SDL_Texture *rp_sink;                    /* meanwhile the frames go to this small target: nothing is shown,
                                                 * the blits are clipped away (draw() runs for what it starts) */
static int rp_over;                             /* the game played again began on the end card (its next button) */
Uint32 ui_ms(void) { return rp_on ? rp_now : (shot_clock ? 1000 : SDL_GetTicks()) + ms_ahead; }
int ui_replaying(void) { return rp_on; }
void ui_animate(void) { need_frame = 1; }

static int perf_tex;                            /* --perf: the textures made (counted per frame) */
static double perf_now(void) { return SDL_GetPerformanceCounter() * 1000.0 / SDL_GetPerformanceFrequency(); }

/* ---- pictures painted a few at a time ----------------------------------------------------------------------------
 * The cards' pictures (the menu's, a picker's, the level screen's tiles) are painted by the games' code, some slowly
 * on a phone: a frame paints them for PAINT_BUDGET ms at most (one at least), the others wait for the next frames
 * (their cards show without them meanwhile, then they fade in), and the idle main loop paints the ones not seen yet
 * ahead (paint_ahead): a tab, a scroll, a picker find them made. The picture mode paints them all at once. */
#define PAINT_BUDGET 4.0
#define FADE_MS 160
static double paint_used;                       /* this frame's painting, ms */
static int paint_n;                             /* this frame's pictures */

static int menu_missing;                        /* the cards drawn without their picture (still painted) */
static void worker_park(void);
static void worker_unpark(void);

static int paint_may(void)
{
    if (instant || paint_n == 0 || paint_used < PAINT_BUDGET) return 1;
    need_frame = 1;                             /* the next frame paints it */
    return 0;
}

static void paint_done(double t0)
{
    paint_used += perf_now() - t0;
    paint_n++;
}

SDL_Texture *ui_tex(int w, int h, const uint8_t *rgba)
{
    perf_tex++;
    SDL_Texture *t = SDL_CreateTexture(ui.r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) return NULL;
    SDL_UpdateTexture(t, NULL, rgba, w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

void ui_free(SDL_Texture **t)
{
    if (*t) SDL_DestroyTexture(*t);
    *t = NULL;
}

uint8_t *ui_buf(int w, int h)
{
    static _Thread_local uint8_t *b;            /* each thread its own (the menu's pictures: a thread of their own) */
    static _Thread_local size_t cap;
    size_t need = (size_t)(w > 0 ? w : 1) * (h > 0 ? h : 1) * 4;
    if (need > cap) { free(b); b = malloc(need); cap = need; }
    return b;
}

void ui_put(SDL_Texture *t, float x, float y, float w, float h)
{
    SDL_FRect d = {x, y, w, h};
    if (t) SDL_RenderCopyF(ui.r, t, NULL, &d);
}

void ui_put_c(SDL_Texture *t, float cx, float cy, float w, float h) { ui_put(t, cx - w / 2, cy - h / 2, w, h); }

void ui_put_rot(SDL_Texture *t, float cx, float cy, float w, float h, double deg)
{
    SDL_FRect d = {cx - w / 2, cy - h / 2, w, h};
    if (t) SDL_RenderCopyExF(ui.r, t, NULL, &d, deg, NULL, SDL_FLIP_NONE);
}

void ui_fill(float x, float y, float w, float h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_FRect q = {x, y, w, h};
    SDL_SetRenderDrawColor(ui.r, r, g, b, a);
    SDL_RenderFillRectF(ui.r, &q);
}

void ui_shadow(float x, float y, float size)
{
    ui_put(t_shadow, x + size * 0.17f, y + size * 0.82f, size * 0.66f, size * 0.15f);
}

void ui_shadow_flat(float cx, float cy, float w, float h, float alpha)
{
    static SDL_Texture *t;
    if (!t) {                                   /* one soft disc, scaled smoothly to any piece */
        art_flat_shadow(128, ui_buf(128, 128));
        t = ui_tex(128, 128, ui_buf(128, 128));
    }
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * (alpha < 0 ? 0 : alpha > 1 ? 1 : alpha)));
    ui_put_c(t, cx + 0.05f * h, cy + 0.08f * h, w, h);
}

void ui_flat_at(SDL_Texture *t, float cx, float cy, float d, float alpha, int upside)
{
    float a = alpha < 0 ? 0 : alpha > 1 ? 1 : alpha;
    ui_shadow_flat(cx, cy, d, d, a);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * a + 0.5f));
    if (upside) ui_put_rot(t, cx, cy, d, d, 180);
    else ui_put_c(t, cx, cy, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

void ui_stand_at(SDL_Texture *t, float x, float y, float size, int upside)
{
    ui_shadow(x, upside ? y - 0.79f * size : y, size);
    if (upside) ui_put_rot(t, x + size / 2, y + size / 2, size, size, 180);
    else ui_put(t, x, y, size, size);
}

/* marks with thin edges, painted at the size they are drawn (a few sizes kept): scaled down, an edge would blur */
#define SIZES 4
typedef struct { int size[SIZES], next; SDL_Texture *t[SIZES]; } sized;

static SDL_Texture *sized_tex(sized *c, int size, void (*paint)(int size, uint8_t *rgba))
{
    if (size < 4) return NULL;
    for (int i = 0; i < SIZES; i++)
        if (c->t[i] && c->size[i] == size) return c->t[i];
    int i = c->next++ % SIZES;
    ui_free(&c->t[i]);
    paint(size, ui_buf(size, size));
    c->t[i] = ui_tex(size, size, ui_buf(size, size));
    c->size[i] = size;
    return c->t[i];
}

/* the move dot: dark, in a thin cream rim, so it reads on light wood, dark wood and a dark point or hole alike */
static void paint_dot(int size, uint8_t *rgba)
{
    static const uint8_t cream[3] = {246, 238, 220}, shade[3] = {24, 34, 26};
    float rim = fmaxf(1.1f, 0.07f * size), r_in = 1 - 2 * rim / size;
    art_ring(size, r_in, cream, 0.55f, rgba);
    int in = (int)(size * (r_in + 0.04f) + 0.5f), at = (size - in) / 2;
    uint8_t *disc = malloc((size_t)in * in * 4);
    if (!disc) return;
    art_disc(in, shade, 0.42f, 0, disc);
    art_blend(rgba, size, size, disc, in, in, at, at);
    free(disc);
}

static void paint_round(int size, uint8_t *rgba)
{
    static const uint8_t warm[3] = {250, 214, 90};
    art_disc(size, warm, 1, 0, rgba);
}

static void round_tint(float cx, float cy, float d, Uint8 alpha)
{
    static sized cache;
    SDL_Texture *t = sized_tex(&cache, (int)(d + 0.5f), paint_round);
    if (!t) return;
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, cx, cy, d, d);
}

void ui_mark_gold(float cx, float cy, float d) { ui_put_c(t_gold, cx, cy, d, d); }
void ui_mark_last(float x, float y, float w, float h) { ui_fill(x, y, w, h, 250, 214, 90, 100); }
void ui_mark_select(float x, float y, float w, float h) { ui_fill(x, y, w, h, 250, 214, 90, 150); }
void ui_mark_last_round(float cx, float cy, float d) { round_tint(cx, cy, d, 100); }
void ui_mark_select_round(float cx, float cy, float d) { round_tint(cx, cy, d, 150); }

void ui_mark_dot(float cx, float cy, float d)
{
    static sized cache;
    ui_put_c(sized_tex(&cache, (int)(d + 0.5f), paint_dot), cx, cy, d, d);
}
void ui_mark_ring(float cx, float cy, float d) { ui_put_c(t_ring, cx, cy, d, d); }
void ui_mark_glow(float cx, float cy, float d) { ui_put_c(t_glow, cx, cy, d, d); }

void ui_mark_gold_a(float cx, float cy, float d, float a)
{
    SDL_SetTextureAlphaMod(t_gold, (Uint8)(255 * clampf(a, 0, 1) + 0.5f));
    ui_put_c(t_gold, cx, cy, d, d);
    SDL_SetTextureAlphaMod(t_gold, 255);
}

/* ---- the games' round buttons ------------------------------------------------------------------------------------ */
#define BTN_CREAM 252, 244, 226                 /* the icons' tint, as the shell's buttons' */

/* ---- the light theme (pages.h: TH) ---------------------------------------------------------------------------------
 * The chrome follows the theme: the table round the board, the header, the bars, the round buttons, the menu's cards
 * and tabs, the pickers, the level screen. Dark: the green table, cream ink (as it always was, to the pixel). Light: a
 * warm ivory, white discs and cards in a hairline rim, dark ink, the gold accent. The boards, cards and pieces stay as
 * they are; the hand-off cover stays a dark overlay (over_dark while it is drawn); the end card is white, its light
 * digits and words dark (light_panel, card_ink). */
static int over_dark;                           /* drawing on a dark overlay (the cover): the dark theme's look */
#define LIGHT_UI (!TH.dark && !over_dark)
static const uint8_t INK[3] = {46, 40, 30};     /* the light theme's ink (TH.text) */
static const uint8_t LT_DISC[3] = {255, 253, 248}, LT_RIM[3] = {220, 208, 184}, LT_GOLD[3] = {212, 166, 68};

static int table_ink(void);                     /* a bar's light digits and words: dark on the light table */

static void ink_mod(SDL_Texture *t)             /* an icon tinted: cream, or the light theme's ink */
{
    if (!t) return;
    if (LIGHT_UI) SDL_SetTextureColorMod(t, INK[0], INK[1], INK[2]);
    else SDL_SetTextureColorMod(t, BTN_CREAM);
}

/* a light disc: white in a hairline rim (the light theme's buttons) */
static void light_disc(int size, const uint8_t fill[3], uint8_t *o)
{
    art_disc(size, LT_RIM, 1, 0, o);
    int m = size - 2 * (size >= 40 ? 2 : 1), off = (size - m) / 2;
    uint8_t *in = m > 1 ? malloc((size_t)m * m * 4) : NULL;
    if (!in) return;
    art_disc(m, fill, 1, 0, in);
    art_blend(o, size, size, in, m, m, off, off);
    free(in);
}

/* a light card w x h: fill in a hairline rim (the light theme's cards, tabs, chips) */
static void light_card(int w, int h, float rad, const uint8_t fill[3], uint8_t *o)
{
    const uint8_t rim[4] = {LT_RIM[0], LT_RIM[1], LT_RIM[2], 255};
    int e = w >= 160 ? 2 : 1;
    art_roundrect(w, h, rad, rim, o);
    art_roundrect_put(o, w, h, e, e, w - 2 * e, h - 2 * e, fmaxf(0, rad - e), fill, 1);
}

/* the light theme's end card (the stars card, the end card, the summary panels): white in a hairline rim, lifted by a
 * soft warm shadow (three faint layers, each wider and lower); kept by its size (a few) */
static void light_panel(float x, float y, float w, float h, float rad)
{
    enum { KEEP = 4 };
    static struct { SDL_Texture *t, *s; int w, h, r; } k[KEEP];
    static int next;
    int iw = (int)w, ih = (int)h, ir = (int)(rad * 4), i;
    if (iw < 2 || ih < 2) return;
    for (i = 0; i < KEEP && !(k[i].t && k[i].w == iw && k[i].h == ih && k[i].r == ir); i++) {}
    if (i == KEEP) {
        static const uint8_t shade[4] = {70, 50, 20, 255};
        i = next++ % KEEP;
        ui_free(&k[i].t);
        ui_free(&k[i].s);
        art_roundrect(iw, ih, rad, shade, ui_buf(iw, ih));
        k[i].s = ui_tex(iw, ih, ui_buf(iw, ih));
        light_card(iw, ih, rad, LT_DISC, ui_buf(iw, ih));
        k[i].t = ui_tex(iw, ih, ui_buf(iw, ih));
        k[i].w = iw, k[i].h = ih, k[i].r = ir;
    }
    static const float grow[3] = {2.4f, 1.4f, 0.6f}, drop[3] = {3, 2, 1};
    static const Uint8 alpha[3] = {8, 12, 18};
    float e = fmaxf(1, 0.05f * ui.unit);
    for (int j = 0; j < 3 && k[i].s; j++) {
        float g = grow[j] * e;
        SDL_SetTextureAlphaMod(k[i].s, alpha[j]);
        ui_put(k[i].s, x - g, y - g + drop[j] * e, w + 2 * g, h + 2 * g);
    }
    if (k[i].s) SDL_SetTextureAlphaMod(k[i].s, 255);
    ui_put(k[i].t, x, y, w, h);
}

/* a button's disc: the shell's dark green, or its gold (kind bit 0); an ivory medallion inside the rim (bit 1); bit 2:
 * the light theme's (white in a rim, its gold) */
static void paint_btn_disc(int kind, int size, uint8_t *o)
{
    static const uint8_t dark[3] = {54, 72, 64}, gold[3] = {150, 118, 46}, ivory[3] = {242, 234, 216};
    if (kind & 4) light_disc(size, kind & 1 ? LT_GOLD : LT_DISC, o);
    else art_disc(size, kind & 1 ? gold : dark, 1, 0, o);
    if (!(kind & 2)) return;
    int m = (int)(0.8f * size), off = (size - m) / 2;
    uint8_t *in = malloc((size_t)m * m * 4);
    if (!in) return;
    art_disc(m, ivory, 1, 0, in);
    art_blend(o, size, size, in, m, m, off, off);
    free(in);
}

static void paint_btn_icon(int icon, int size, uint8_t *o) { art_icon(icon, size, o); }

/* the buttons' pictures, made at the size shown (painter, kind, size), the least used one remade */
static SDL_Texture *btn_tex(ui_painter paint, int kind, int size)
{
    enum { KEEP = 64 };
    static struct { ui_painter paint; int kind, size; Uint32 used; SDL_Texture *t; } c[KEEP];
    static Uint32 clock;
    if (size < 2 || !paint) return NULL;
    int lru = 0;
    clock++;
    for (int i = 0; i < KEEP; i++) {
        if (c[i].t && c[i].paint == paint && c[i].kind == kind && c[i].size == size) {
            c[i].used = clock;
            return c[i].t;
        }
        if (!c[i].t) { lru = i; break; }
        if (c[i].used < c[lru].used) lru = i;
    }
    uint8_t *b = calloc((size_t)size * size, 4);   /* its own: a painter may use ui_buf() */
    if (!b) return NULL;
    paint(kind, size, b);
    ui_free(&c[lru].t);
    c[lru].t = ui_tex(size, size, b);
    free(b);
    c[lru].paint = paint;
    c[lru].kind = kind;
    c[lru].size = size;
    c[lru].used = clock;
    return c[lru].t;
}

void ui_button_paint(float cx, float cy, float d, ui_painter paint, int kind, float k, int how)
{
    if (how & UI_LIT) ui_mark_gold(cx, cy, 2.0f * d);
    Uint8 a = how & UI_DIM ? 100 : 255;
    SDL_Texture *disc = btn_tex(paint_btn_disc, (how & UI_ON ? 1 : 0) | (how & UI_IVORY ? 2 : 0) | (LIGHT_UI ? 4 : 0),
                                (int)(d + 0.5f));
    if (LIGHT_UI) ui_shadow_flat(cx, cy, 1.04f * d, 1.04f * d, a / 255.0f * 0.32f);   /* white on ivory: lifted */
    if (disc) {
        SDL_SetTextureAlphaMod(disc, a);
        ui_put_c(disc, cx, cy, d, d);
        SDL_SetTextureAlphaMod(disc, 255);
    }
    SDL_Texture *t = btn_tex(paint, kind, (int)(k * d));
    if (!t) return;
    if (how & UI_TINT) ink_mod(t);
    SDL_SetTextureAlphaMod(t, a);
    if (how & UI_UPSIDE) ui_put_rot(t, cx, cy, k * d, k * d, 180);
    else ui_put_c(t, cx, cy, k * d, k * d);
    SDL_SetTextureAlphaMod(t, 255);
    SDL_SetTextureColorMod(t, 255, 255, 255);
}

void ui_button(float cx, float cy, float d, int icon, int how)
{
    ui_button_paint(cx, cy, d, paint_btn_icon, icon, 0.62f, how | UI_TINT);
}

int ui_button_hit(float cx, float cy, float d, float x, float y) { return hypotf(x - cx, y - cy) <= 0.56f * d; }

SDL_Texture *ui_word(const char *key, int i, float px, float maxw, const uint8_t rgb[3], int *w, int *h)
{
    enum { KEEP = 32 };
    static struct { char key[32]; int i, px4, maxw; uint8_t rgb[3]; Uint32 used; int w, h; SDL_Texture *t; } c[KEEP];
    static Uint32 clock;
    int px4 = (int)(px * 4), mw = (int)maxw, k = 0;
    if (table_ink() && rgb[0] + rgb[1] + rgb[2] > 600) rgb = INK;   /* light words in a bar: dark on the ivory */
    static int gen;
    if (gen != text_gen) {                      /* another language: every word again */
        gen = text_gen;
        for (int j = 0; j < KEEP; j++) ui_free(&c[j].t);
    }
    clock++;
    for (int j = 0; j < KEEP; j++) {
        if (c[j].t && c[j].i == i && c[j].px4 == px4 && c[j].maxw == mw && !memcmp(c[j].rgb, rgb, 3) &&
            !strcmp(c[j].key, key)) {
            c[j].used = clock;
            *w = c[j].w;
            *h = c[j].h;
            return c[j].t;
        }
        if (!c[j].t) { k = j; break; }
        if (c[j].used < c[k].used) k = j;
    }
    char s[96];
    if (!text_word(key, i, s, sizeof s)) return NULL;
    int tw = text_line(s, px, 1, rgb, NULL, 0, 0, 0, 0);
    if (tw > maxw && tw > 0) {
        px = fmaxf(6, px * maxw / tw);
        tw = text_line(s, px, 1, rgb, NULL, 0, 0, 0, 0);
    }
    int W = tw + 4, H = (int)ceilf(px * 1.55f);
    uint8_t *b = calloc((size_t)W * H, 4);
    if (!b) return NULL;
    text_line(s, px, 1, rgb, b, W, H, 2, (int)((H - 1.25f * px) / 2));
    ui_free(&c[k].t);
    c[k].t = ui_tex(W, H, b);
    free(b);
    snprintf(c[k].key, sizeof c[k].key, "%s", key);
    c[k].i = i;
    c[k].px4 = px4;
    c[k].maxw = mw;
    memcpy(c[k].rgb, rgb, 3);
    c[k].used = clock;
    *w = c[k].w = W;
    *h = c[k].h = H;
    return c[k].t;
}

void ui_button_pic(uint8_t *o, int w, int h, float cx, float cy, int d, ui_painter paint, int kind, float k, int how)
{
    int s = (int)(k * d), x0 = (int)lroundf(cx - d / 2.0f), y0 = (int)lroundf(cy - d / 2.0f);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    paint_btn_disc((how & UI_ON ? 1 : 0) | (how & UI_IVORY ? 2 : 0), d, b);
    if (s >= 2 && paint) {
        uint8_t *p = calloc((size_t)s * s, 4);
        if (p) {
            paint(kind, s, p);
            static const uint8_t cream[3] = {BTN_CREAM};
            for (int i = 0; i < s * s && (how & UI_TINT); i++)
                for (int c = 0; c < 3; c++) p[4 * i + c] = (uint8_t)(p[4 * i + c] * cream[c] / 255);
            art_blend(b, d, d, p, s, s, (d - s) / 2, (d - s) / 2);
            free(p);
        }
    }
    art_blend(o, w, h, b, d, d, x0, y0);
    free(b);
}

/* ---- stars and the level screen's tiles -------------------------------------------------------------------------- */
/* a star: 0 gold, 1 a faint dark hollow (on wood, on gold), 2 a faint light one (on the dark end card), 3 a soft beige
 * one (on the light theme's white end card) */
static void paint_star(int kind, int size, uint8_t *o)
{
    static const uint8_t gold[3] = {255, 200, 80}, ink[3] = {150, 102, 28}, dark[3] = {0, 0, 0},
                         light[3] = {246, 238, 220}, beige[3] = {150, 128, 96};
    art_fig *f = art_fig_new();
    if (!f) return;
    art_star(f, ART_PART, 0.5f, 0.53f, 0.46f);
    if (kind == 0) {
        art_fig_paint(o, size, f, gold, ink, 0.045f, 0.5f);
        return;
    }
    art_fig_paint(o, size, f, kind == 1 ? dark : kind == 2 ? light : beige, NULL, 0, 0);
    float k = kind == 1 ? 0.3f : kind == 2 ? 0.2f : 0.35f;
    for (int i = 0; i < size * size; i++) o[4 * i + 3] = (uint8_t)(o[4 * i + 3] * k);
}

static void star_at(float cx, float cy, float d, int kind)
{
    ui_put_c(btn_tex(paint_star, kind, (int)(d + 0.5f)), cx, cy, d, d);
}

void ui_star(float cx, float cy, float d, int lit) { star_at(cx, cy, d, lit ? 0 : 1); }

int ui_par_stars(int moves, int par)
{
    if (par <= 0 || moves <= par) return 3;
    return 2 * moves <= 3 * par ? 2 : 1;
}

int ui_logic_stars(int hints, int mistakes)
{
    int k = (hints > 0 ? hints : 0) + (mistakes > 0 ? mistakes : 0);
    return k == 0 ? 3 : k <= 2 ? 2 : 1;
}

/* a level's tile: 0 gold (solved), 1 wood (open), 2 dark walnut (locked), 3 soft beige (locked, the light theme);
 * rounded, a bevel lit from the upper left */
static void paint_tile(int kind, int size, uint8_t *o)
{
    static const float col[4][3] = {{214, 172, 76}, {222, 184, 128}, {92, 60, 40}, {226, 216, 198}};
    float rad = 0.16f * size, bev = fmaxf(1.5f, 0.055f * size), h = size * 0.5f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float qx = fabsf(x + 0.5f - h) - (h - rad), qy = fabsf(y + 0.5f - h) - (h - rad);
            float d = hypotf(fmaxf(qx, 0), fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - rad;   /* < 0: inside */
            float u = (float)x / size, v = (float)y / size, k;
            if (kind == 0) k = (1.12f - 0.12f * (u + v)) * (1 + 0.03f * art_noise(x, y));
            else if (kind == 3) k = 1.04f - 0.06f * (u + v);   /* plain, a little lighter at the top left */
            else {                              /* the house's plank: its grain across */
                float g = sinf(v * 23 + 2.4f * sinf(u * 0.9f + 1.3f) + 1.7f * sinf(u * 0.21f)) * 0.5f + 0.5f;
                k = (1 + 0.06f * (g - 0.5f)) * (kind == 2 ? 0.72f : 1);
            }
            if (-d < bev) k *= x + y < size ? 1.16f : 0.8f;
            uint8_t *p = o + 4 * (y * size + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)clampf(col[kind][c] * k, 0, 255);
            p[3] = (uint8_t)(255 * clampf(0.5f - d, 0, 1));
        }
}

/* a small dark disc under a number on a level's picture */
static void paint_badge(int kind, int size, uint8_t *o)
{
    static const uint8_t c[3] = {28, 34, 32};
    (void)kind;
    art_disc(size, c, 0.86f, 0, o);
}

void ui_sound(int s) { snd_play(s); }

/* ---- chance: the shell's dice ------------------------------------------------------------------------------------ */
static uint32_t rng = 0x9e3779b9u;              /* xorshift; seeded at start (--seed in the picture mode) */
static uint32_t start_rng[MAX_GAMES];            /* the state each game's last start() was given: a replay gives it
                                                 * again (the same deal, the same puzzle) */

static void seed_random(uint32_t s) { rng = s ? s : 0x9e3779b9u; }

int ui_random(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return n > 0 ? (int)(rng % (uint32_t)n) : 0;
}

#define DICE_MS UI_DICE_MS
static struct {
    int n, v[UI_MAX_DICE], player[UI_MAX_DICE];
    float tilt[UI_MAX_DICE];
    Uint32 t0;
} dice;

/* the dice's faces at a few sizes, each face made when first shown */
#define FACE_SIZES 4
static SDL_Texture *die_face(int size, int player, int value)
{
    static struct { int size; SDL_Texture *t[MAX_PLAYERS][7]; } c[FACE_SIZES];
    static int next;
    int k;
    for (k = 0; k < FACE_SIZES && c[k].size != size; k++) {}
    if (k == FACE_SIZES) {
        k = next++ % FACE_SIZES;
        for (int p = 0; p < MAX_PLAYERS; p++)
            for (int v = 0; v < 7; v++) ui_free(&c[k].t[p][v]);
        c[k].size = size;
    }
    player = player < 0 ? 0 : player >= MAX_PLAYERS ? MAX_PLAYERS - 1 : player;
    value = value < 1 ? 1 : value > 6 ? 6 : value;
    if (!c[k].t[player][value]) {
        art_die(value, player, size, ui_buf(size, size));
        c[k].t[player][value] = ui_tex(size, size, ui_buf(size, size));
    }
    return c[k].t[player][value];
}

/* the dice set to their values and colours (players: one per die, or NULL: all `player`'s), tumbling or at rest */
static void dice_set(int n, const int *values, const int *players, int player, int tumble)
{
    int was = dice.n;
    dice.n = n < 0 ? 0 : n > UI_MAX_DICE ? UI_MAX_DICE : n;
    for (int i = 0; i < dice.n; i++) {
        int p = players ? players[i] : player;
        p = p < 0 ? 0 : p >= MAX_PLAYERS ? MAX_PLAYERS - 1 : p;
        int same = i < was && dice.v[i] == values[i] && dice.player[i] == p;   /* shown again (a layout, a resume) */
        dice.v[i] = values[i];
        dice.player[i] = p;
        dice.tilt[i] = tumble ? (float)(ui_random(31) - 15) : same ? dice.tilt[i] : 0;   /* they land a little askew */
    }
    if (!tumble) {
        dice.t0 = ui_ms() - DICE_MS;            /* already at rest */
        return;
    }
    dice.t0 = ui_ms();
    snd_play(SND_DICE);
    need_frame = 1;
}

void ui_dice_throw(int n, const int *values, int player) { dice_set(n, values, NULL, player, 1); }
void ui_dice_show(int n, const int *values, int player) { dice_set(n, values, NULL, player, 0); }
void ui_dice_throw_each(int n, const int *values, const int *players) { dice_set(n, values, players, 0, 1); }
void ui_dice_show_each(int n, const int *values, const int *players) { dice_set(n, values, players, 0, 0); }

int ui_dice_busy(void) { return dice.n && ui_ms() - dice.t0 < DICE_MS; }

void ui_die_tumble(int t, int i, float *turn, float *lift)
{
    float f = clampf(t / (float)DICE_MS, 0, 1);
    *turn = (1 - f) * (1 - f) * 540 * (i & 1 ? -1 : 1);
    *lift = (1 - f) * 0.6f * fabsf(sinf(f * 9.4f));
}

/* a tumbling die's face: they flicker */
static int tumble_face(int i, int value) { return 1 + (int)((unsigned)(ui_ms() / 70 + i * 3 + value) % 6); }

void ui_die_at(float cx, float cy, float size, int player, int value, float alpha)
{
    int s = (int)size;
    if (s < 4 || value < 1 || value > 6) return;
    SDL_Texture *f = die_face(s, player, value);
    Uint8 a = (Uint8)(255 * (alpha < 0 ? 0 : alpha > 1 ? 1 : alpha) + 0.5f);
    ui_shadow_flat(cx, cy, size * 1.1f, size * 1.1f, alpha);
    SDL_SetTextureAlphaMod(f, a);
    ui_put_c(f, cx, cy, size, size);
    SDL_SetTextureAlphaMod(f, 255);
}

void ui_die(float cx, float cy, float size, int player, int value, float deg, float lift, float alpha, float shade)
{
    int s = (int)(size + 0.5f);
    if (s < 4) return;
    SDL_Texture *f = die_face(s, player, value);
    ui_shadow_flat(cx, cy, size * 1.1f, size * 1.1f, alpha * (lift > 0 ? 0.7f : 1));
    Uint8 k = (Uint8)(255 * clampf(shade, 0, 1) + 0.5f);
    SDL_SetTextureAlphaMod(f, (Uint8)(255 * clampf(alpha, 0, 1) + 0.5f));
    SDL_SetTextureColorMod(f, k, k, k);
    ui_put_rot(f, cx, cy - lift, size, size, deg);
    SDL_SetTextureAlphaMod(f, 255);
    SDL_SetTextureColorMod(f, 255, 255, 255);
}

int ui_die_thrown(float cx, float cy, float size, int player, int value, float deg, int i, int t, float shade)
{
    if (t >= DICE_MS) {
        ui_die(cx, cy, size, player, value, deg, 0, 1, shade);
        return 0;
    }
    float turn, lift;
    ui_die_tumble(t < 0 ? 0 : t, i, &turn, &lift);
    ui_die(cx, cy, size, player, tumble_face(i, value), deg + turn, lift * size, 1, 1);
    need_frame = 1;
    return 1;
}

void ui_dice_draw(float cx, float cy, float size, unsigned used)
{
    int s = (int)size;
    if (s < 4 || !dice.n) return;
    int rolling = ui_dice_busy();
    float gap = 0.3f * size, x = cx - (dice.n * size + (dice.n - 1) * gap) / 2 + size / 2;
    for (int i = 0; i < dice.n; i++, x += size + gap) {
        int v = dice.v[i];
        float a = dice.tilt[i], y = cy;
        if (rolling) {                          /* tumbling: the faces flicker, the die turns and bounces */
            float turn, lift;
            ui_die_tumble((int)(ui_ms() - dice.t0), i, &turn, &lift);
            v = tumble_face(i, dice.v[i]);
            a += turn;
            y -= lift * size;
        }
        Uint8 alpha = used >> i & 1 ? 110 : 255;
        SDL_Texture *f = die_face(s, dice.player[i], v);
        ui_shadow_flat(x, cy, size * 1.1f, size * 1.1f, alpha / 255.0f);
        SDL_SetTextureAlphaMod(f, alpha);
        ui_put_rot(f, x, y, size, size, a);
        SDL_SetTextureAlphaMod(f, 255);
    }
    if (rolling) need_frame = 1;
}

void ui_sound_soft(int s, float volume) { snd_play_vol(s, volume); }
void ui_sound_later(int s, int ms) { snd_play_at(s, 1, ms); }

/* digits: white textures per height and weight (a few kept), tinted when drawn, turned round their centre */
static const uint8_t NUM_LIGHT[3] = {246, 238, 220}, NUM_DARK[3] = {40, 34, 30};

static void digits_at(float cx, float cy, float h, int value, const uint8_t rgb[3], Uint8 alpha, int how, double deg)
{
    enum { KEEP = 6 };
    static struct { int h, regular; SDL_Texture *d[10]; } cache[KEEP];
    static int next;
    int ih = (int)(h + 0.5f), k, regular = how & UI_NUM_REGULAR;
    if (ih < 4) return;
    for (k = 0; k < KEEP && (cache[k].h != ih || cache[k].regular != regular); k++) {}
    if (k == KEEP) {
        k = next++ % KEEP;
        for (int i = 0; i < 10; i++) ui_free(&cache[k].d[i]);
        int w = (int)(0.62f * ih + 1);
        static const uint8_t white[3] = {255, 255, 255};
        for (int i = 0; i < 10; i++) {          /* the font's digits (help/glyphs.txt keeps them), tinted */
            uint8_t *b = ui_buf(w, ih);
            memset(b, 0, (size_t)w * ih * 4);
            text_glyph('0' + i, ih * 1.2f, !regular, white, b, w, ih);
            cache[k].d[i] = ui_tex(w, ih, b);
        }
        cache[k].h = ih;
        cache[k].regular = regular;
    }
    char s[12];
    int n = snprintf(s, sizeof s, "%d", value < 0 ? 0 : value);
    float dw = 0.62f * ih, step = (how & UI_NUM_TIGHT ? 0.66f : 0.74f) * ih, x = cx - (step * (n - 1) + dw) / 2;
    float ca = cosf((float)(deg * 3.14159265 / 180)), sa = sinf((float)(deg * 3.14159265 / 180));
    for (int i = 0; i < n; i++, x += step) {
        SDL_Texture *t = cache[k].d[s[i] - '0'];
        SDL_SetTextureColorMod(t, rgb[0], rgb[1], rgb[2]);
        SDL_SetTextureAlphaMod(t, alpha);
        if (deg == 0) ui_put(t, x, cy - h / 2, dw, (float)ih);
        else {                                  /* the digit's centre turned round the number's */
            float ox = x + dw / 2 - cx;
            ui_put_rot(t, cx + ox * ca, cy + ox * sa, dw, (float)ih, deg);
        }
        SDL_SetTextureAlphaMod(t, 255);
    }
}

/* a game's light digits in its bars are drawn on the table: the light theme's table is ivory, so they are dark there
 * (elsewhere they may be on a dark panel or badge: as they are) */
static int in_bar;                              /* drawing a player's bar (bar_content) */
static int card_ink;                            /* drawing on the light theme's white end card or summary panel */
static int table_ink(void) { return LIGHT_UI && ((screen == SCR_GAME && in_bar) || card_ink); }

int ui_ink_dark(void) { return table_ink(); }

void ui_light_panel(float x, float y, float w, float h)
{
    if (TH.dark) return;
    light_panel(x, y, w, h, 0.4f * ui.unit);
    card_ink = 1;                               /* (to the end of the game's draw()) */
}

static void number(float cx, float cy, float h, int value, int light, double deg)
{
    digits_at(cx, cy, h, value, light && !table_ink() ? NUM_LIGHT : NUM_DARK, 255, 0, deg);
}

void ui_number(float cx, float cy, float h, int value, int light) { number(cx, cy, h, value, light, 0); }

/* on the table: the light theme's ivory takes dark ink (the cover, a dark overlay: cream, as the dark theme's) */
const uint8_t *ui_table_ink(void) { return LIGHT_UI ? INK : NUM_LIGHT; }
void ui_number_table(float cx, float cy, float h, int value)
{
    digits_at(cx, cy, h, value, ui_table_ink(), 255, 0, 0);
}

void ui_number_rot(float cx, float cy, float h, int value, int light, double deg)
{
    number(cx, cy, h, value, light, deg);
}

void ui_number_ex(float cx, float cy, float h, int value, const uint8_t rgb[3], Uint8 alpha, int how)
{
    if (card_ink && LIGHT_UI && rgb[0] + rgb[1] + rgb[2] > 600) rgb = INK;   /* light digits: dark on the white card */
    digits_at(cx, cy, h, value, rgb, alpha, how, 0);
}

float ui_number_w(int value, float h)
{
    int n = 1;
    for (int v = value; v >= 10; v /= 10) n++;
    return (0.74f * (n - 1) + 0.62f) * h;
}

void ui_number_signed(float cx, float cy, float h, int value, int light, int plus)
{
    int sign = value < 0 ? -1 : plus && value > 0 ? 1 : 0, a = abs(value), n = 1;
    if (!sign) { number(cx, cy, h, a, light, 0); return; }
    for (int v = a; v >= 10; v /= 10) n++;
    float ih = floorf(h + 0.5f), dw = 0.62f * ih, step = 0.74f * ih, sw = 0.46f * ih, gap = 0.1f * ih;
    float digits = step * (n - 1) + dw, x = cx - (sw + gap + digits) / 2, t = fmaxf(1, 0.13f * ih);
    if (table_ink()) light = 0;
    Uint8 r = light ? 246 : 40, g = light ? 238 : 34, b = light ? 220 : 30;
    t = floorf(t + 0.5f);                       /* the sign on whole pixels: a bar, crossed for a plus (one bar
                                                 * each way, so no seam makes it look like a division sign) */
    float sx = floorf(x + 0.5f), sy = floorf(cy - t / 2 + 0.5f), s = floorf(sw + 0.5f);
    ui_fill(sx, sy, s, t, r, g, b, 255);
    if (sign > 0) ui_fill(sx + floorf((s - t) / 2), sy - floorf((s - t) / 2), t, s, r, g, b, 255);
    number(x + sw + gap + digits / 2, cy, h, a, light, 0);
}

float ui_bar_number(box b, SDL_Texture *piece, int n)
{
    float sz = 0.6f * ui.unit, dh = 0.46f * ui.unit, cy = b.y + b.h / 2, x = b.x;
    if (n < 0) n = 0;
    if (piece) {
        ui_put(piece, x, cy - sz / 2, sz, sz);
        x = x + sz + 0.12f * ui.unit;
    }
    int digits = 1;
    for (int v = n; v >= 10; v /= 10) digits++;
    float w = (0.74f * (digits - 1) + 0.62f) * dh;
    ui_number(x + w / 2, cy, dh, n, 1);
    return x + w;
}

void ui_bar_count(box b, SDL_Texture *piece, int n)
{
    if (n > 0) ui_bar_number(b, piece, n);
}

box ui_bar_countdown(box b, int left)
{
    float u = ui.unit, dh = 0.42f * u, id = 0.5f * u, cy = b.y + b.h / 2;
    left = left < 0 ? 0 : left;
    float nw = ui_number_w(left, dh), x = b.x + b.w - nw;
    const uint8_t *ink = table_ink() ? INK : NUM_LIGHT;   /* (dark on the light table) */
    ui_number_ex(x + nw / 2, cy, dh, left, ink, 220, UI_NUM_REGULAR);
    SDL_Texture *t = btn_tex(paint_btn_icon, ICON_HOURGLASS, (int)(id + 0.5f));
    x -= 0.06f * u + id;
    if (t) {
        SDL_SetTextureColorMod(t, ink[0], ink[1], ink[2]);
        SDL_SetTextureAlphaMod(t, 200);
        ui_put(t, x, cy - id / 2, id, id);
        SDL_SetTextureAlphaMod(t, 255);
        SDL_SetTextureColorMod(t, 255, 255, 255);
    }
    b.w = fmaxf(0, x - 0.3f * u - b.x);
    return b;
}

/* the width of the counts at a scale, the first one's picture dropped or not */
static float counts_w(const ui_count *c, int nc, float k, int drop)
{
    float u = ui.unit, w = 0;
    for (int i = 0; i < nc; i++) {
        if (c[i].pic && !(drop && i == 0)) w += 0.72f * u * k;
        w += ui_number_w(c[i].value < 0 ? 0 : c[i].value, 0.46f * u * k) + (i ? 0.35f * u * k : 0);
    }
    return w;
}

float ui_bar_fit(box b, const ui_count *c, int nc, int nbtn)
{
    static const float scale[5] = {1, 1, 0.9f, 0.8f, 0.7f};
    float u = ui.unit, k = 1, d = 0.84f * u;
    int drop = 0;
    for (int pass = 0; pass < 5; pass++) {      /* the first count's picture goes first, then all gets smaller */
        drop = pass >= 1 && nc > 0 && c[0].pic;
        k = scale[pass];
        float room = b.w - counts_w(c, nc, k, drop) - (nc && nbtn ? 0.3f * u : 0) - 0.05f * u;
        d = nbtn ? (room - (nbtn - 1) * 0.3f * u) / nbtn : 0.84f * u;
        if (nbtn ? d >= 0.8f * u * k : room >= 0) break;   /* no buttons: the counts alone must fit */
        if (pass == 0 && !(nc > 0 && c[0].pic)) pass = 1;   /* no picture to drop */
    }
    d = floorf(clampf(d, 0.5f * u, 0.84f * u));
    float x = b.x, cy = b.y + b.h / 2, dh = 0.46f * u * k, sz = 0.6f * u * k;
    for (int i = 0; i < nc; i++) {
        if (i) x += 0.35f * u * k;
        if (c[i].pic && !(drop && i == 0)) {
            ui_put(c[i].pic, x, cy - sz / 2, sz, sz);
            x += sz + 0.12f * u * k;
        }
        int v = c[i].value < 0 ? 0 : c[i].value;
        float w = ui_number_w(v, dh);
        ui_number(x + w / 2, cy, dh, v, 1);
        x += w;
    }
    return d;
}

void ui_bar_spot(box b, int i, float d, float *cx, float *cy)
{
    *cx = b.x + b.w - d / 2 - 0.05f * ui.unit - i * (d + 0.3f * ui.unit);
    *cy = b.y + b.h / 2;
}

/* ---- what is kept between two runs (save.c): the sound, the menu's tab, each game's choice ---------------------- */
static void game_key(int g, char *k, size_t n) { snprintf(k, n, "shell/game/%s", games[g]->id); }

static int nlevels(int g) { return games[g]->levels < MAX_LEVELS ? games[g]->levels : MAX_LEVELS; }

static int has_bot(int g);

/* a game's choice: its variant (a game with levels: the last level played) and how many play; a game of levels that
 * also plays at random: the variant picked for it, and which way it was last played; a game with a bot: who plays
 * each seat (bytes 5..10: 0 a person, else 1 + the bot's level) */
#define SAVE_GAME (5 + MAX_PLAYERS)
static void remember_game(int g)
{
    char k[96];
    game_key(g, k, sizeof k);
    int v = chosen[g] ? variant[g] + 1 : 0;
    uint8_t b[SAVE_GAME] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)nplayers[g], (uint8_t)(rvariant[g] + 1),
                            (uint8_t)rnd_mode[g]};
    memcpy(b + 5, seat_lv[g], MAX_PLAYERS);
    ui_save_set(k, b, has_bot(g) ? SAVE_GAME : has_random(g) ? 5 : 3);
}

static void remember_mute(void)
{
    uint8_t b = (uint8_t)(snd_muted != 0);
    ui_save_set("shell/mute", &b, 1);
}

/* each game's variant before one is chosen (game.variant_first) */
static void variants_first(void)
{
    for (int g = 0; g < ngames; g++) {
        int v = games[g]->variant_first;
        if (v <= 0 || v >= games[g]->variants) continue;
        if (games[g]->levels) rvariant[g] = v;
        else variant[g] = v;
    }
}

/* the kept settings, each checked (a game gone, fewer variants: left as new) */
static void settings_load(void)
{
    uint8_t b[SAVE_GAME];
    if (ui_save_get("shell/mute", b, 1) == 1) snd_muted = b[0] == 1;
    if (ui_save_get("shell/tab", b, 1) == 1 && b[0] < SECTIONS) menu.tab = b[0];
    for (int g = 0; g < ngames; g++) {
        char k[96];
        game_key(g, k, sizeof k);
        int len = ui_save_get(k, b, SAVE_GAME);
        if (len < 3) continue;
        int v = (b[0] | b[1] << 8) - 1, most = games[g]->levels ? nlevels(g) : games[g]->variants;
        if (v >= 0 && v < most) {
            variant[g] = v;
            chosen[g] = 1;
        }
        if (b[2] >= 1 && b[2] <= MAX_PLAYERS && player_counts(g) >> b[2] & 1) nplayers[g] = b[2];
        if (len >= 5 && has_random(g) && b[3] >= 1 && b[3] <= games[g]->variants) {
            rvariant[g] = b[3] - 1;
            rnd_mode[g] = b[4] == 1;
        }
        for (int p = 0; len >= SAVE_GAME && has_bot(g) && p < MAX_PLAYERS; p++)
            seat_lv[g][p] = b[5 + p] <= 1 + AI_HARD ? b[5 + p] : 0;
    }
}

/* ---- the levels (game.levels): the progress, the chapters ------------------------------------------------------- */
static void progress_load(int g)
{
    if (LV.g == g) return;
    LV.g = g;
    memset(LV.best, 0, sizeof LV.best);
    char k[96];
    snprintf(k, sizeof k, "shell/levels/%s", games[g]->id);
    ui_save_get(k, LV.best, nlevels(g));        /* fewer kept (levels added since): the rest not solved */
    for (int l = 0; l < MAX_LEVELS; l++)
        if (LV.best[l] > 4) LV.best[l] = 4;
}

static void progress_save(void)
{
    char k[96];
    snprintf(k, sizeof k, "shell/levels/%s", games[LV.g]->id);
    ui_save_set(k, LV.best, nlevels(LV.g));
}

static int level_chapter(int g, int l)
{
    const game *G = games[g];
    int c;
    if (G->chapter_of) c = G->chapter_of(l);
    else {
        int n = G->chapters > 1 ? G->chapters : 1, per = (nlevels(g) + n - 1) / n;
        c = per ? l / per : 0;
    }
    return c < 0 ? 0 : c >= MAX_CHAPTERS ? MAX_CHAPTERS - 1 : c;
}

static void chapters_make(int g)
{
    int n = nlevels(g);
    LV.nch = 0;
    for (int l = 0; l < n; l++)
        for (int c = level_chapter(g, l); LV.nch <= c;) LV.first[LV.nch++] = l;
    if (!LV.nch) LV.first[LV.nch++] = 0;
    LV.first[LV.nch] = n;
}

/* open: solved, or among the first unsolved ones (levels_open, else 3); never one the free edition locks */
static int level_open(int g, int l)
{
    progress_load(g);
    if (l < 0 || l >= nlevels(g) || level_locked(l)) return 0;
    if (LV.best[l]) return 1;
    int ahead = games[g]->levels_open > 0 ? games[g]->levels_open : 3, before = 0;
    for (int k = 0; k < l && before < ahead; k++) before += !LV.best[k];
    return before < ahead;
}

static int first_unsolved(int g)
{
    progress_load(g);
    for (int l = 0; l < nlevels(g); l++)
        if (!LV.best[l]) return l;
    return -1;
}

/* the level screen opens at the chapter of the level last played, else of the first one not solved */
static void levels_enter(int g)
{
    int l = chosen[g] ? variant[g] : first_unsolved(g);
    chapters_make(g);
    LV.chapter = l < 0 ? 0 : level_chapter(g, l);
    LV.grabbing = 0;
}

/* the level in play solved: its stars kept when better than before */
static void level_check(void)
{
    if (screen != SCR_GAME || cur < 0 || !games[cur]->levels || !started[cur] || in_random(cur)) return;
    if (gesture_held()) return;                 /* solved under a finger still down: kept once it lifts */
    int l = variant[cur];
    if (l < 0 || l >= nlevels(cur) || games[cur]->status() != GS_WIN0) return;
    int s = games[cur]->level_stars ? games[cur]->level_stars() : 0;
    s = s < 0 ? 0 : s > 3 ? 3 : s;
    progress_load(cur);
    if (LV.best[l] < 1 + s) {                   /* the XP: a first solve, else the stars gained */
        int was = LV.best[l];
        LV.best[l] = (uint8_t)(1 + s);
        progress_save();
        stats_award(section_of(games[cur]),
                    (uint32_t)(was ? STATS_XP_STAR * (s - (was - 1)) : STATS_XP_LEVEL + STATS_XP_STAR * s));
    }
}

int ui_level_best(int level)
{
    if (cur < 0 || !games[cur]->levels || level < 0 || level >= nlevels(cur)) return -1;
    progress_load(cur);
    return LV.best[level] - 1;
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* the textures shared_textures() makes for a unit and a button size: a few sets kept (the menu's, a picker's and a
 * game's are not the same size: going from one to another makes none again). The buttons' icons are made when first
 * drawn (icon_tex): a screen shows a few of them. */
#define SETS 4
typedef struct {
    int u, d, ic, dark;                         /* dark: the theme it was made for (TH.dark) */
    Uint32 used;
    SDL_Texture *btn, *btn_armed, *btn_on, *icon[ICONS], *gold, *ring, *glow, *card;
} shared_set;
static shared_set sets[SETS];
static int set_cur;
static Uint32 set_clock;

static void set_use(int k)                      /* set k's textures, the ones the shell draws */
{
    shared_set *s = &sets[k];
    set_cur = k;
    s->used = ++set_clock;
    t_btn = s->btn;
    t_btn_armed = s->btn_armed;
    t_btn_on = s->btn_on;
    t_gold = s->gold;
    t_ring = s->ring;
    t_glow = s->glow;
    t_card = s->card;
}

static void set_keep(void)                      /* the textures just made, kept in the set in use */
{
    shared_set *s = &sets[set_cur];
    s->btn = t_btn;
    s->btn_armed = t_btn_armed;
    s->btn_on = t_btn_on;
    s->gold = t_gold;
    s->ring = t_ring;
    s->glow = t_glow;
    s->card = t_card;
}

/* a round button's icon, cream (the set in use's size) */
static SDL_Texture *icon_tex(int i)
{
    shared_set *s = &sets[set_cur];
    if (i < 0 || i >= ICONS || s->ic < 2) return NULL;
    if (!s->icon[i]) {
        uint8_t *b = calloc((size_t)s->ic * s->ic, 4);
        if (!b) return NULL;
        art_icon(i, s->ic, b);
        s->icon[i] = ui_tex(s->ic, s->ic, b);
        free(b);
        if (s->icon[i]) SDL_SetTextureColorMod(s->icon[i], 252, 244, 226);
    }
    return s->icon[i];
}

static void bg_texture(void);

static void shared_textures(void)
{
    bg_texture();
    int u = (int)ui.unit, d = (int)(2 * L.br + 0.5f);
    if (made_unit == u && sets[set_cur].btn && sets[set_cur].u == u && sets[set_cur].d == d &&
        sets[set_cur].dark == TH.dark)
        return;
    int was = made_unit;
    made_unit = u;
    if (was != u) made_game = -1;               /* the tokens follow the unit */
    int old = 0;
    for (int k = 0; k < SETS; k++) {
        if (sets[k].btn && sets[k].u == u && sets[k].d == d && sets[k].dark == TH.dark) {   /* made before: kept */
            set_use(k);
            return;
        }
        if (sets[k].used < sets[old].used) old = k;
    }
    set_use(old);                               /* made in the set used least lately */
    shared_set *s = &sets[set_cur];
    s->u = u;
    s->d = d;
    s->dark = TH.dark;
    s->ic = (int)(0.62f * d);
    for (int i = 0; i < ICONS; i++) ui_free(&s->icon[i]);
    static const uint8_t btn_c[3] = {54, 72, 64}, armed_c[3] = {176, 64, 52}, on_c[3] = {150, 118, 46},
                         gold[3] = {255, 196, 64}, shade[3] = {24, 34, 26}, red[3] = {235, 40, 30},
                         black[3] = {0, 0, 0}, lt_armed[3] = {236, 150, 132};
    ui_free(&t_btn);
    if (TH.dark) art_disc(d, btn_c, 1, 0, ui_buf(d, d));
    else light_disc(d, LT_DISC, ui_buf(d, d));
    t_btn = ui_tex(d, d, ui_buf(d, d));
    ui_free(&t_btn_armed);
    if (TH.dark) art_disc(d, armed_c, 1, 0, ui_buf(d, d));
    else light_disc(d, lt_armed, ui_buf(d, d));
    t_btn_armed = ui_tex(d, d, ui_buf(d, d));
    ui_free(&t_btn_on);
    if (TH.dark) art_disc(d, on_c, 1, 0, ui_buf(d, d));
    else light_disc(d, LT_GOLD, ui_buf(d, d));
    t_btn_on = ui_tex(d, d, ui_buf(d, d));
    int big = (int)(2.6f * u), m = u;
    ui_free(&t_gold);
    art_disc(big, gold, 0.75f, 1.3f, ui_buf(big, big));
    t_gold = ui_tex(big, big, ui_buf(big, big));
    ui_free(&t_ring);
    art_ring(m, 0.82f, shade, 0.30f, ui_buf(m, m));
    t_ring = ui_tex(m, m, ui_buf(m, m));
    ui_free(&t_glow);
    art_disc(m, red, 0.9f, 1.5f, ui_buf(m, m));
    t_glow = ui_tex(m, m, ui_buf(m, m));
    if (!t_shadow) {
        art_disc(64, black, 0.42f, 1.2f, ui_buf(64, 64));
        t_shadow = ui_tex(64, 64, ui_buf(64, 64));
    }
    const uint8_t card_c[4] = {28, 34, 32, 236};
    int cw = (int)(6.8f * u), ch = (int)(3.8f * u);
    ui_free(&t_card);
    art_roundrect(cw, ch, 0.4f * u, card_c, ui_buf(cw, ch));
    t_card = ui_tex(cw, ch, ui_buf(cw, ch));
    set_keep();
}

static void bg_texture(void)
{
    static int bg_dark = -1;
    if (t_bg && bg_dark != TH.dark) ui_free(&t_bg);
    bg_dark = TH.dark;
    if (!t_bg) {                                /* the table: a dark green, lighter in the middle (the light theme: a
                                                 * warm ivory, the same way) */
        uint8_t *b = ui_buf(2, 256);
        for (int y = 0; y < 256; y++)
            for (int x = 0; x < 2; x++) {
                float k = 1 - fabsf((float)(y - 110)) / 256.0f * 0.55f;
                uint8_t *p = b + 4 * (y * 2 + x);
                if (!TH.dark) {
                    float l = 0.9f + 0.1f * k;
                    p[0] = (uint8_t)(250 * l);
                    p[1] = (uint8_t)(245 * l);
                    p[2] = (uint8_t)(233 * l);
                } else {
                    p[0] = (uint8_t)(34 * k);
                    p[1] = (uint8_t)(52 * k);
                    p[2] = (uint8_t)(46 * k);
                }
                p[3] = 255;
            }
        t_bg = ui_tex(2, 256, b);
        SDL_SetTextureBlendMode(t_bg, SDL_BLENDMODE_NONE);
    }
}

static void game_textures(void)
{
    if (made_game == cur * 8 + ui.players) return;   /* (ui_tokens_changed() sets it to -1) */
    made_game = cur * 8 + ui.players;
    const game *g = games[cur];
    int tk = (int)L.tk;
    uint8_t *b = calloc((size_t)(tk > 1 ? tk : 1) * (tk > 1 ? tk : 1), 4);   /* its own buffer: the game may use
                                                                              * ui_buf() while drawing */
    for (int p = 0; p < ui.players && b; p++) {
        ui_free(&t_tok[p]);
        memset(b, 0, (size_t)tk * tk * 4);
        g->token(p, tk, b);
        t_tok[p] = ui_tex(tk, tk, b);
        ui_free(&t_big[p]);                     /* the end card's: made when it shows (big_tok) */
    }
    free(b);
}

/* a player's token as the end card shows it, big */
static SDL_Texture *big_tok(int p)
{
    if (p < 0 || p >= MAX_PLAYERS) return NULL;
    if (!t_big[p]) {
        int big = (int)(2.4f * ui.unit);
        uint8_t *b = big > 1 ? calloc((size_t)big * big, 4) : NULL;
        if (!b) return NULL;
        games[cur]->token(p, big, b);
        t_big[p] = ui_tex(big, big, b);
        free(b);
    }
    return t_big[p];
}

/* ---- layout ------------------------------------------------------------------------------------------------------ */
/* the buttons shown at the bottom: undo (a game with an undo()), and flip and face to face for two people or more on a
 * board (home, new game and the rules: the header's back and i) */
static int shown(int i)
{
    if (i == B_HOME || i == B_NEW || i == B_HELP) return 0;
    if (i == B_UNDO && (cur < 0 || !games[cur]->undo)) return 0;
    return (humans() > 1 && !is_view()) || (i != B_FLIP && i != B_F2F);
}

/* the game's header, at the top of its screen (portrait), of its column (landscape): back at the left (to the game's
 * new game screen: its picker, its levels; a game in progress waits), the game's name, i (its rules) at the right */
static struct { float cy, r, bx, ix; } HD;
static int header_hit(float x, float y, float cx) { return hypotf(x - cx, y - HD.cy) < HD.r * 1.35f; }

/* the game's own round buttons (game.tools): the room the shell gave them (right: packed against its right end, the
 * bottom bar's), their size; where each is, worked out again when the count shown changes */
static struct {
    int n, shown, right, dirty;
    int row;                                    /* portrait: in the buttons' row, after them (centred together) */
    float step, by;                             /* that row's pitch and height */
    box room;
    float d0, d, x[UI_MAX_TOOLS], y[UI_MAX_TOOLS];
} TL;

static int tools_of(int g)
{
    int n = g >= 0 ? games[g]->tools : 0;
    return n < 0 ? 0 : n > UI_MAX_TOOLS ? UI_MAX_TOOLS : n;
}

/* the tools shown in their room: rows as few as fit, as big as fit (down to 0.5 unit), each row centred (or packed to
 * the right) */
static void tools_place(void)
{
    TL.dirty = 0;
    int n = TL.shown;
    if (TL.row) {                               /* the buttons and the tools shown, one row centred */
        int nb = 0;
        for (int i = 0; i < NBTN; i++) nb += shown(i);
        float x0 = ui.w / 2.0f - (nb + n - 1) / 2.0f * TL.step;
        for (int i = 0, k = 0; i < NBTN; i++)
            if (shown(i)) L.bx[i] = floorf(x0 + k++ * TL.step);
        TL.d = floorf(TL.d0);
        for (int i = 0; i < n; i++) {
            TL.x[i] = floorf(x0 + (nb + i) * TL.step);
            TL.y[i] = TL.by;
        }
        return;
    }
    if (n <= 0 || TL.room.w <= 0) return;
    float u = ui.unit, d = TL.d0, gap = 0.3f * u;
    int per = 1, rows = 1;
    for (;;) {
        per = (int)((TL.room.w + gap) / (d + gap));
        per = per < 1 ? 1 : per > n ? n : per;
        rows = (n + per - 1) / per;
        if (rows * (d + gap) - gap <= TL.room.h + 0.5f || d <= 0.5f * u) break;
        d *= 0.92f;
        gap = fmaxf(0.15f * u, gap * 0.92f);
    }
    per = (n + rows - 1) / rows;                /* the rows even */
    TL.d = floorf(d);
    float y0 = TL.room.y + (TL.room.h - (rows * (d + gap) - gap)) / 2 + d / 2;
    for (int i = 0; i < n; i++) {
        int r = i / per, k = r == rows - 1 ? n - r * per : per;
        float w = k * d + (k - 1) * gap, x0 = TL.right ? TL.room.x + TL.room.w - w : TL.room.x + (TL.room.w - w) / 2;
        TL.x[i] = floorf(x0 + (i % per) * (d + gap) + d / 2);
        TL.y[i] = floorf(y0 + r * (d + gap));
    }
}

void ui_tools_shown(int n)
{
    n = n < 0 ? 0 : n > TL.n ? TL.n : n;
    if (n != TL.shown) TL.dirty = 1;
    TL.shown = n;
}

int ui_tool_at(int i, float *cx, float *cy, float *d)
{
    if (TL.dirty) tools_place();
    if (screen != SCR_GAME || i < 0 || i >= TL.shown) return 0;
    if (cx) *cx = TL.x[i];
    if (cy) *cy = TL.y[i];
    if (d) *d = TL.d;
    return 1;
}

void ui_tool(int i, int icon, int how)
{
    float x, y, d;
    if (ui_tool_at(i, &x, &y, &d)) ui_button(x, y, d, icon, how);
}

void ui_tool_paint(int i, ui_painter paint, int kind, float k, int how)
{
    float x, y, d;
    if (ui_tool_at(i, &x, &y, &d)) ui_button_paint(x, y, d, paint, kind, k, how);
}

int ui_tool_hit(float x, float y)
{
    float cx, cy, d;
    for (int i = 0; ui_tool_at(i, &cx, &cy, &d); i++)
        if (ui_button_hit(cx, cy, d, x, y)) return i;
    return -1;
}

/* the screen of a game: its bars, its board, the shell's buttons, the game's own tools.
 * Portrait: the top bar (when a player sits there) at the top of the screen, the bottom bar and the buttons at its
 * bottom, the board in all the room between (a square in its middle; the whole rectangle for VIEW_* games and
 * game.fill). Tools: up to three at the right end of the bottom bar (one bar there), else a row (two) just above it.
 * Landscape: the board on the left (a square, the whole room left for VIEW_* and fill), a column on the right: the
 * bars, the buttons (two rows) and the tools between the top bar and the buttons. */
static void layout_game(void)
{
    float m = fminf(ui.w, ui.h) * 0.025f, u;
    int view = is_view(), fill = view || (cur >= 0 && games[cur]->fill), nt = tools_of(cur);
    /* each player's side (home(): top or bottom), in their order from the left (VIEW_*: in turn order) */
    int side[2][MAX_PLAYERS], n[2] = {0, 0};
    float key[MAX_PLAYERS];
    for (int p = 0; p < ui.players; p++) {
        float fx = 0.5f, fy = 0;
        if (cur >= 0 && games[cur]->home && !view) games[cur]->home(p, &fx, &fy);
        key[p] = ui.flipped ? 1 - fx : fx;
        if (view) key[p] = (float)((p - ui_bottom() + ui.players) % ui.players);
        int s = seat_top(p), k = n[s]++;
        while (k > 0 && key[side[s][k - 1]] > key[p]) { side[s][k] = side[s][k - 1]; k--; }
        side[s][k] = p;
    }
    int nb = 0;
    for (int i = 0; i < NBTN; i++) nb += shown(i);
    box top, bottom;
    TL.n = TL.shown = nt;
    TL.room = (box){0, 0, 0, 0};
    TL.right = TL.row = 0;
    TL.dirty = 1;
    if (ui.w <= ui.h) {
        /* the header's row at the top; at the bottom one row: the buttons, then the game's tools (six at most), big;
         * more tools: a row of their own above the bottom bar */
        int in_row = nt > 0 && nb + nt <= 6, row = nt > 0 && !in_row, rows = nt > 7 ? 2 : 1;
        float extra = row ? 1.1f * rows : 0;   /* units */
        u = floorf(fminf((ui.w - 2 * m) / 9, (ui.h - 2 * m) / (14.0f + extra + (row ? 0.1f : 0))));
        float x = floorf((ui.w - 9 * u) / 2);
        L.br = 0.6f * u;
        HD.cy = floorf(m + 0.5f * u);
        HD.r = 0.42f * u;
        HD.bx = x + 0.45f * u;
        HD.ix = x + 8.55f * u;
        top = (box){x, floorf(m + 1.05f * u), 9 * u, u};
        bottom = (box){x, floorf(ui.h - m - 1.35f * u - 2 * L.br), 9 * u, u};
        float step = 1.5f * u, by = bottom.y + bottom.h + 0.35f * u + L.br;
        float bx0 = ui.w / 2.0f - ((in_row ? nb + nt : nb) - 1) / 2.0f * step;
        for (int i = 0, k = 0; i < NBTN; i++) {
            if (!shown(i)) continue;
            L.bx[i] = floorf(bx0 + k++ * step);
            L.by[i] = by;
        }
        float r0 = n[1] ? top.y + 1.2f * u : top.y, r1 = bottom.y - 0.2f * u;
        TL.d0 = 0.84f * u;
        if (in_row) {                           /* the tools after the buttons, as big, the same pitch */
            TL.d0 = 2 * L.br;
            TL.room = (box){floorf(bx0 + nb * step - L.br), by - L.br, nt * step, TL.d0};
            TL.row = 1;                         /* (tools_place: the row centred on the tools shown) */
            TL.step = step;
            TL.by = by;
        } else if (row) {
            TL.d0 = 0.9f * u;
            TL.room = (box){x, r1 - extra * u, 9 * u, extra * u};
            r1 -= extra * u + 0.1f * u;
        }
        L.room = (box){x, r0, 9 * u, r1 - r0};
        if (fill) L.board = L.room;
        else L.board = (box){x, floorf(r0 + fmaxf(0, r1 - r0 - 9 * u) / 2), 9 * u, 9 * u};
    } else if (view) {                          /* the table on the left; others' bars, buttons, the viewer's bar */
        u = floorf(fminf((ui.h - 2 * m) / 9, (ui.w - 2 * m) / 13.45f));
        float y = floorf((ui.h - 9 * u) / 2), cx = floorf(ui.w - m - 4 * u);
        L.board = (box){floorf(m), y, cx - 0.45f * u - floorf(m), 9 * u};
        HD.cy = y + 0.45f * u;                  /* the header: the column's top */
        HD.r = 0.38f * u;
        HD.bx = cx + 0.4f * u;
        HD.ix = cx + 3.6f * u;
        top = (box){cx, y + u, 4 * u, u};
        bottom = (box){cx, y + 8 * u, 4 * u, u};
        L.br = 0.55f * u;
        for (int i = 0, k = 0; i < NBTN; i++) { /* one row, over the viewer's bar */
            if (!shown(i)) continue;
            L.bx[i] = cx + 2 * u + (k++ - (nb - 1) / 2.0f) * 1.35f * u;
            L.by[i] = y + 7.25f * u;
        }
        float t0 = n[1] ? y + u + fminf(n[1] * 1.1f * u, 4.4f * u) : y + u;   /* below the others' bars */
        TL.room = (box){cx, t0, 4 * u, y + 6.5f * u - t0};
        TL.d0 = 0.84f * u;
    } else {                                    /* the board, then a column: bar, tools, buttons, bar */
        u = floorf(fminf((ui.h - 2 * m) / 9, (ui.w - 2 * m) / 13.45f));
        float x = floorf((ui.w - 13.45f * u) / 2), y = floorf((ui.h - 9 * u) / 2), cx = x + 9.45f * u;
        L.board = (box){x, y, 9 * u, 9 * u};
        if (fill) {                             /* all the width left of the column, the column at the right */
            cx = floorf(ui.w - m - 4 * u);
            L.board = (box){floorf(m), y, cx - 0.45f * u - floorf(m), 9 * u};
        }
        HD.cy = y + 0.45f * u;                  /* the header: the column's top */
        HD.r = 0.38f * u;
        HD.bx = cx + 0.4f * u;
        HD.ix = cx + 3.6f * u;
        top = (box){cx, y + u, 4 * u, u};
        bottom = (box){cx, y + 8 * u, 4 * u, u};
        L.br = 0.55f * u;
        /* one row of buttons over the bottom bars (stacked up from the column's foot), the tools above it, under the
         * top bars (stacked down from under the header) */
        float top_end = top.y + n[1] * 1.1f * u, bot_start = y + 8 * u - (n[0] > 1 ? n[0] - 1 : 0) * 1.1f * u;
        float by = bot_start - 0.3f * u - L.br;
        for (int i = 0, k = 0; i < NBTN; i++) {
            if (!shown(i)) continue;
            L.bx[i] = cx + 2 * u + (k++ - (nb - 1) / 2.0f) * 1.35f * u;
            L.by[i] = by;
        }
        float t0 = top_end + 0.1f * u;
        TL.room = (box){cx, t0, 4 * u, fmaxf(0.5f * u, by - L.br - 0.2f * u - t0)};
        TL.d0 = 0.84f * u;
    }
    ui.unit = u;
    if (ui.w > ui.h) L.room = L.board;
    grid_ext = (box){0, 0, 0, 0};               /* the board's grid, once the game fits it (ui_grid_fit) */
    /* each player's bar on their side: side by side sharing the bar's width (portrait), or stacked (landscape: the
     * column is narrow) */
    for (int s = 0; s < 2; s++) {
        box b = s ? top : bottom;
        for (int k = 0; k < n[s]; k++) {
            box c = b;
            if (ui.w <= ui.h && n[s] >= 5) {    /* five bars on a side: the screen's whole width, closer */
                float gap = 0.08f * u, x0 = floorf(fminf(b.x, 0.012f * ui.w));
                c.w = floorf((ui.w - 2 * x0 - (n[s] - 1) * gap) / n[s]);
                c.x = x0 + k * (c.w + gap);
            } else if (ui.w <= ui.h) {
                float gap = 0.25f * u;
                c.w = floorf((b.w - (n[s] - 1) * gap) / n[s]);
                c.x = b.x + k * (c.w + gap);
            } else if (view) {                  /* the others' bars from the top, above the buttons */
                c.y = b.y + k * fminf(1.1f * u, 3.4f * u / fmaxf(1, n[s] - 1));
            } else {
                c.y = b.y + (s ? 1 : -1) * k * 1.1f * u;
            }
            L.bar[side[s][k]] = c;
        }
    }
    L.tk = 0.72f * u;
    shared_textures();
    made_game = -1;
    if (bar_w != (int)top.w || bar_h != (int)top.h) {
        ui_free(&t_bartex);
        bar_w = (int)top.w;
        bar_h = (int)top.h;
        bar_m = (int)(0.7f * u);                /* room around the bar: the gold light spills over its edges */
        t_bartex = SDL_CreateTexture(ui.r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, bar_w + 2 * bar_m,
                                     bar_h + 2 * bar_m);
        if (t_bartex) SDL_SetTextureBlendMode(t_bartex, SDL_BLENDMODE_BLEND);
    }
    if (cur >= 0) {
        /* its frame changed since it was laid out (the theme): laid out once at another size first, so the pictures
         * it keeps by their size are made again in the new wood */
        static signed char laid_light[MAX_GAMES];   /* 0: never laid out; 1 walnut, 2 light */
        int light = frame_light_of(cur);
        frame_for(cur);
        if (laid_light[cur] && laid_light[cur] != 1 + light) {
            box half = L.board;
            half.w = floorf(half.w / 2);
            half.h = floorf(half.h / 2);
            games[cur]->layout(half);
        }
        laid_light[cur] = (signed char)(1 + light);
        games[cur]->layout(L.board);
    }
}

/* ---- a grid of square cells fitted into a box (game.fill's room): turned a quarter when that makes them bigger --- */
ui_grid ui_grid_fit(box b, int cols, int rows, float frame)
{
    ui_grid g = {0, 0, 0, cols, rows, 0};
    if (cols < 1 || rows < 1) return g;
    float a = fminf(b.w / (cols + 2 * frame), b.h / (rows + 2 * frame)),
          t = fminf(b.w / (rows + 2 * frame), b.h / (cols + 2 * frame));
    g.turned = t > 1.04f * a;                   /* turned only when the cells grow by more than a little */
    g.cell = floorf(g.turned ? t : a);
    g.cols = g.turned ? rows : cols;
    g.rows = g.turned ? cols : rows;
    g.x = floorf(b.x + (b.w - g.cols * g.cell) / 2);
    g.y = floorf(b.y + (b.h - g.rows * g.cell) / 2);
    if (screen == SCR_GAME) {                   /* the biggest grid fitted: the board (a solved puzzle's card goes
                                                 * beside it) */
        float fw = (g.cols + 2 * frame) * g.cell, fh = (g.rows + 2 * frame) * g.cell;
        if (fw * fh > grid_ext.w * grid_ext.h) grid_ext = (box){g.x - frame * g.cell, g.y - frame * g.cell, fw, fh};
    }
    return g;
}

void ui_grid_cell(const ui_grid *g, int col, int row, float *x, float *y)
{
    *x = g->x + (g->turned ? row : col) * g->cell;
    *y = g->y + (g->turned ? col : row) * g->cell;
}

int ui_grid_hit(const ui_grid *g, float x, float y, int *col, int *row)
{
    if (g->cell <= 0 || x < g->x || y < g->y) return 0;
    int sc = (int)((x - g->x) / g->cell), sr = (int)((y - g->y) / g->cell);
    if (sc >= g->cols || sr >= g->rows) return 0;
    *col = g->turned ? sr : sc;
    *row = g->turned ? sc : sr;
    return 1;
}

/* ---- the loupe: small cells magnified under the finger ---------------------------------------------------------- */
int ui_loupe_small(float cell) { return cell < UI_LOUPE_SMALL * fminf((float)ui.w, (float)ui.h); }

ui_loupe ui_loupe_at(float fx, float fy, float cell, float cells, int below, box room)
{
    ui_loupe l = {fx, fy, 0, 0, floorf(cells * cell * UI_LOUPE_Z), UI_LOUPE_Z, cell};
    float full = l.side, gap = 0.75f * ui.unit, k = 1.05f;   /* k: the frame round it */
    float up = fy - gap - room.y, dn = room.y + room.h - fy - gap;   /* the room above and below the finger */
    float pref = below ? dn : up, other = below ? up : dn;
    int down = pref < 0.6f * full * k && other > pref ? !below : below;
    l.side = floorf(fmaxf(0.6f * full, fminf(full, (down ? dn : up) / k)));
    float half = l.side * k / 2;
    l.cx = clampf(fx, room.x + half, room.x + room.w - half);
    l.cy = clampf(down ? fy + gap + half : fy - gap - half, room.y + half, room.y + room.h - half);
    return l;
}

void ui_loupe_map(const ui_loupe *l, float x, float y, float *lx, float *ly)
{
    *lx = l->cx + (x - l->fx) * l->z;
    *ly = l->cy + (y - l->fy) * l->z;
}

void ui_loupe_begin(const ui_loupe *l)
{
    float s = l->side, x0 = floorf(l->cx - s / 2), y0 = floorf(l->cy - s / 2), e = fmaxf(4, 0.022f * s);
    ui_shadow_flat(l->cx + 0.03f * s, l->cy + 0.05f * s, s * 1.08f, s * 1.08f, 0.8f);
    ui_fill(x0 - e, y0 - e, s + 2 * e, s + 2 * e, 92, 60, 40, 255);   /* a walnut frame */
    ui_fill(x0 - e * 0.4f, y0 - e * 0.4f, s + 0.8f * e, s + 0.8f * e, 52, 34, 22, 255);
    SDL_Rect clip = {(int)x0, (int)y0, (int)s, (int)s};
    SDL_RenderSetClipRect(ui.r, &clip);
    ui_fill(x0, y0, s, s, 30, 60, 44, 255);
}

void ui_loupe_end(void) { SDL_RenderSetClipRect(ui.r, NULL); }

void ui_loupe_snap(float x, float y, float gx, float gy, float slack, float *dx, float *dy)
{
    int near = hypotf(x - gx, y - gy) < slack;
    *dx = near ? gx - x : 0;
    *dy = near ? gy - y : 0;
}

/* ---- trays by the players' sides -------------------------------------------------------------------------------- */
/* the biggest slots (side up to `most`) for n things in w x h: cols x rows of side *pitch; of those as big, the
 * fewest rows (wide: a tray along the board) or the fewest columns (one beside it) */
static void trays_grid(float w, float h, int n, float most, int wide, int *cols, int *rows, float *pitch)
{
    *cols = n, *rows = 1, *pitch = 0;
    for (int k = 1; k <= n; k++) {
        int c = wide ? n + 1 - k : k, r = (n + c - 1) / c;
        float p = floorf(fminf(fminf(w / c, h / r), most));
        if (p > *pitch) *cols = c, *rows = r, *pitch = p;
    }
}

void ui_trays_place(ui_trays *t, box r, float bw, float bh, int n, float most, float least, float *x, float *y)
{
    memset(t, 0, sizeof *t);
    t->n = n;
    *x = floorf(r.x + (r.w - bw) / 2);
    *y = floorf(r.y + (r.h - bh) / 2);
    int bottom = ui_bottom(), c, rows;
    float p, gap = floorf(0.25f * least);
    if (r.h - bh >= r.w - bw) {                 /* above and below the board, as wide as it */
        float d = (r.h - bh) / 2 - gap;
        if (d < least) return;
        trays_grid(bw, d, n, most, 1, &c, &rows, &p);
        if (p < least) return;
        float th = rows * p, y0 = floorf(r.y + (r.h - bh) / 2 - gap - th);
        t->b[!bottom] = (box){*x, y0, bw, th};
        t->b[bottom] = (box){*x, *y + bh + gap, bw, th};
    } else {                                    /* beside it, on the right: the top player's half, the bottom one's */
        float d = r.w - bw - gap, hh = floorf((bh - gap) / 2);
        if (d < least) return;
        trays_grid(d, hh, n, most, 0, &c, &rows, &p);
        if (p < least) return;
        float tw = c * p;
        *x = floorf(r.x + (r.w - bw - gap - tw) / 2);
        t->b[!bottom] = (box){*x + bw + gap, *y, tw, hh};
        t->b[bottom] = (box){*x + bw + gap, *y + bh - hh, tw, hh};
    }
    t->cols = c, t->rows = rows, t->pitch = p;
}

int ui_trays_on(const ui_trays *t) { return t->b[0].w > 0; }

void ui_trays_slot(const ui_trays *t, int p, int i, float *cx, float *cy)
{
    box b = t->b[p];
    int top = p != ui_bottom(), col = i % t->cols, row = i / t->cols;
    float gw = t->cols * t->pitch, gh = t->rows * t->pitch;
    float x0 = b.x + floorf((b.w - gw) / 2), y0 = top ? b.y : b.y + b.h - gh;
    if (ui_upside(p)) col = t->cols - 1 - col;
    if (!top) row = t->rows - 1 - row;
    *cx = x0 + (col + 0.5f) * t->pitch;
    *cy = y0 + (row + 0.5f) * t->pitch;
}

int ui_trays_hit(const ui_trays *t, int p, float x, float y)
{
    box b = t->b[p];
    return b.w > 0 && x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

/* the Logic tab's picture: a three by three grid of a pencil-and-paper puzzle, a pencil writing in it (cut out of the
 * grid round its edge), flat white as the jigsaw piece */
static void logic_icon(int d, uint8_t *o)
{
    static const uint8_t white[3] = {255, 255, 255};
    const float x0 = 0.10f, y0 = 0.24f, s = 0.64f, w = 0.065f, wi = 0.035f;   /* the grid: its corner, side, lines */
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, x0, y0, x0 + s, y0 + w);
    art_rect(f, ART_PART, x0, y0 + s - w, x0 + s, y0 + s);
    art_rect(f, ART_PART, x0, y0, x0 + w, y0 + s);
    art_rect(f, ART_PART, x0 + s - w, y0, x0 + s, y0 + s);
    for (int k = 1; k < 3; k++) {
        float c = x0 + w / 2 + k * (s - w) / 3;
        art_rect(f, ART_PART, c - wi / 2, y0, c + wi / 2, y0 + s);
        c = y0 + w / 2 + k * (s - w) / 3;
        art_rect(f, ART_PART, x0, c - wi / 2, x0 + s, c + wi / 2);
    }
    art_fig_paint(o, d, f, white, NULL, 0, 0);
    /* the pencil (as ICON_PENCIL, smaller), its point in the grid's middle right cell, going up to the right */
    const float k = 0.56f, px = 0.60f, py = 0.62f;   /* its scale; where its point goes */
#define PX(x) (px + k * ((x) - 0.17f))
#define PY(y) (py + k * ((y) - 0.83f))
    for (int pass = 0; pass < 2; pass++) {      /* its margin cut out of the grid, then the pencil itself */
        art_fig *p = art_fig_new();
        const float tip[] = {PX(0.335f), PY(0.745f), PX(0.255f), PY(0.665f), PX(0.17f), PY(0.83f)};
        art_band(p, ART_PART, PX(0.32f), PY(0.68f), PX(0.69f), PY(0.31f), 0.17f * k);
        art_band(p, ART_PART, PX(0.725f), PY(0.275f), PX(0.81f), PY(0.19f), 0.17f * k);
        art_poly(p, ART_PART, tip, 3);
        if (pass == 0) {
            uint8_t *m = calloc((size_t)d * d, 4);
            art_fig_paint(m, d, p, white, white, 0.09f, 0);
            for (int i = 0; i < d * d; i++) o[4 * i + 3] = (uint8_t)(o[4 * i + 3] * (255 - m[4 * i + 3]) / 255);
            free(m);
        } else {
            art_fig_paint(o, d, p, white, NULL, 0, 0);
        }
    }
#undef PX
#undef PY
}

/* a tab's picture: the pieces of its section (a draughts man, a knight, a die, a card, a question, a marble, a jigsaw
 * piece, a grid and its pencil) */
static void tab_icon(int s, int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    if (s == SEC_STRATEGY) art_counter(COUNTER_MAN, 0, d, o);
    else if (s == SEC_CHESS) art_piece(KNIGHT, WHITE, d, o);
    else if (s == SEC_GUESS) art_icon(ICON_HELP, d, o);
    else if (s == SEC_PUZZLE) art_icon(ICON_PUZZLE, d, o);
    else if (s == SEC_LOGIC) logic_icon(d, o);
    else if (s == SEC_ARCADE) arcade_icon(d, o);
    else {                                      /* a die, a card: smaller, in the middle */
        int h = (int)(d * (s == SEC_RACE ? 0.78f : 0.92f));
        int w = s == SEC_RACE ? h : (int)(h / cards_aspect(DECK_FRENCH));
        uint8_t *b = calloc((size_t)w * h, 4);
        if (s == SEC_RACE) art_die(5, 0, h, b);
        else cards_paint(DECK_FRENCH, CARD(SUIT_SPADES, 1), w, h, b);
        art_blend(o, d, d, b, w, h, (d - w) / 2, (d - h) / 2);
        free(b);
    }
}

static void tab_textures(int tw, int th)
{
    if (made_tab == (tw * 1000 + th) * 2 + TH.dark) return;
    made_tab = (tw * 1000 + th) * 2 + TH.dark;
    const uint8_t off[4] = {40, 56, 50, 255}, on[4] = {150, 118, 46, 255};
    ui_free(&t_tab);
    if (TH.dark) art_roundrect(tw, th, th * 0.3f, off, ui_buf(tw, th));
    else light_card(tw, th, th * 0.3f, LT_DISC, ui_buf(tw, th));
    t_tab = ui_tex(tw, th, ui_buf(tw, th));
    ui_free(&t_tab_on);
    if (TH.dark) art_roundrect(tw, th, th * 0.3f, on, ui_buf(tw, th));
    else light_card(tw, th, th * 0.3f, LT_GOLD, ui_buf(tw, th));
    t_tab_on = ui_tex(tw, th, ui_buf(tw, th));
    int d = (int)(th * 0.66f);
    uint8_t *b = calloc((size_t)d * d, 4);       /* its own buffer: cards_paint may use ui_buf() */
    for (int s = 0; s < SECTIONS; s++) {
        ui_free(&t_tabicon[s]);
        tab_icon(s, d, b);
        t_tabicon[s] = ui_tex(d, d, b);
    }
    free(b);
    if (TH.dark) {
        SDL_SetTextureColorMod(t_tabicon[SEC_GUESS], 246, 238, 220);
        SDL_SetTextureColorMod(t_tabicon[SEC_PUZZLE], 246, 238, 220);
        SDL_SetTextureColorMod(t_tabicon[SEC_LOGIC], 246, 238, 220);
    } else {
        SDL_SetTextureColorMod(t_tabicon[SEC_GUESS], INK[0], INK[1], INK[2]);
        SDL_SetTextureColorMod(t_tabicon[SEC_PUZZLE], INK[0], INK[1], INK[2]);
        SDL_SetTextureColorMod(t_tabicon[SEC_LOGIC], INK[0], INK[1], INK[2]);
    }
}

static float level_tabs(float head);
static void draw_level_tabs(int on);

/* ---- the picker on two axes (game.axes: a size, then a level) --------------------------------------------------- */
static int pick_size[MAX_GAMES];                /* the size chosen on the picker */

static int axes_on(int g)
{
    if (g < 0) return 0;
    const game *G = games[g];
    return G->axes[0] > 0 && G->axes[1] > 0 && G->axes[0] * G->axes[1] <= G->variants &&
           G->axes[0] + G->axes[1] <= MAX_VARIANTS;
}

/* the variant the picker shows as chosen last (a game of levels: its random play's); -1 none */
static int picked(int g)
{
    if (games[g]->levels)                       /* (its picker is random play's: its last one, or its first choice) */
        return in_random(g) || (g == cur && screen == SCR_PICK) ? rvariant[g] : -1;
    return chosen[g] || games[g]->variant_first ? variant[g] : -1;   /* (a game's first choice: as if chosen) */
}

/* the picker opens on the size chosen last (else the first) */
static void axes_enter(int g)
{
    int v = picked(g);
    if (axes_on(g)) pick_size[g] = v >= 0 ? v / games[g]->axes[1] : 0;
}

/* the cards: the sizes in rows (as many a row as makes them biggest), the levels in one row under them, smaller.
 * L.cards[0..sizes-1], then the levels'; their tops from 0; returns the block's height, *card the sizes' side */
static float axes_layout(float H, float g, float lab, float *card)
{
    int ns = games[cur]->axes[0], nl = games[cur]->axes[1], bc = 1;
    float W = ui.w - 2 * fminf(ui.w, ui.h) * 0.05f, best = 0, k1 = 1 + lab;   /* k1: a card's height (a name under) */
    for (int cols = 1; cols <= ns; cols++) {
        int rows = (ns + cols - 1) / cols;
        float c = fminf((W - (cols - 1) * g) / cols, (H - (rows - 1) * g - 1.4f * g) / ((rows + 0.55f) * k1));
        c = fminf(c, fminf(ui.w, ui.h) * 0.62f);
        if (c > best + 0.5f) { best = c; bc = cols; }
    }
    float c = floorf(best), cl = floorf(fminf(0.55f * c, (W - (nl - 1) * g) / nl)), ch = floorf(c * k1);
    int rows = (ns + bc - 1) / bc;
    for (int i = 0; i < ns; i++) {
        int r = i / bc, k = r == rows - 1 ? ns - r * bc : bc;   /* the last row centred */
        float x0 = (ui.w - (k * c + (k - 1) * g)) / 2;
        L.cards[i] = (box){floorf(x0 + (i % bc) * (c + g)), r * (ch + g), c, ch};
    }
    float ly = rows * ch + (rows - 1) * g + 1.4f * g, x0 = (ui.w - (nl * cl + (nl - 1) * g)) / 2;
    for (int j = 0; j < nl; j++) L.cards[ns + j] = (box){floorf(x0 + j * (cl + g)), floorf(ly), cl, floorf(cl * k1)};
    *card = c;
    return ly + floorf(cl * k1);
}

/* a level's picture by default: as many ebony stones as its number (one easy .. three hard), in a row */
static void paint_stones(int k, int size, uint8_t *o)
{
    int n = k + 1, d = (int)(size * fminf(0.24f, 0.9f / (1.2f * n)));
    uint8_t *st = d > 2 ? calloc((size_t)d * d, 4) : NULL;
    if (!st) return;
    art_counter(COUNTER_STONE, 1, d, st);
    for (int i = 0; i < n; i++)
        art_blend(o, size, size, st, d, d, (int)(size / 2.0f + (i - (n - 1) / 2.0f) * 1.2f * d - d / 2.0f),
                  (size - d) / 2);
    free(st);
}

static void draw_button(float x, float y, float r, int icon, SDL_Texture *disc);
static void open_help(int g);
static int resumable(void);
static int in_circle(float x, float y, float cx, float cy, float r);

/* the slim row under a picker's or the level screen's header (the stars won, the resume button), when it has either:
 * at head, its height added to it */
static float head_row(float head, int stars)
{
    float s = fminf(ui.w, ui.h), h = floorf(s * 0.1f);
    L.row = stars || resumable();
    if (!L.row) return head;
    L.row_r = floorf(s * 0.036f);
    L.row_y = floorf(head - s * 0.015f + h / 2);
    return head + h;
}

/* a picture of one colour, rounded (a panel, a switch's track), kept by its size and colour (a few) */
static SDL_Texture *rounded(int w, int h, float rad, const uint8_t c[4])
{
    static struct { SDL_Texture *t; int w, h; uint8_t c[4]; } k[12];
    static int next;
    for (int i = 0; i < 12; i++)
        if (k[i].t && k[i].w == w && k[i].h == h && !memcmp(k[i].c, c, 4)) return k[i].t;
    int i = next++ % 12;
    ui_free(&k[i].t);
    if (w < 1 || h < 1) return NULL;
    art_roundrect(w, h, rad, c, ui_buf(w, h));
    k[i].t = ui_tex(w, h, ui_buf(w, h));
    k[i].w = w;
    k[i].h = h;
    memcpy(k[i].c, c, 4);
    return k[i].t;
}

/* ---- the full version's panel (the free edition: a locked game or level tapped) ----------------------------------
 * The settings' panel's look: the paid app's icon (its gold frame round the green table, the knight, the die, the
 * card: make_icon.py's, drawn by the app's own code) and its name, two lines (help/<lang>/_full.txt: a title line for
 * the translators, the two lines, the button's word), and a gold button in gold light that opens the paid app's
 * page: market://details?id=<PAID_APP_ID> on Android (PAID_STORE_URL when there is no Play Store), PAID_STORE_URL on
 * a computer. Its close button, a tap outside it or Back closes it.
 * A build that sells the unlock inside the app (iap_available(): iOS) shows this app's own icon and name, its two
 * lines and words from help/<lang>/_unlock.txt, the gold button "Unlock everything" with the store's price (the
 * purchase: iap_buy) and under it a quieter one, "Restore purchases" (iap_restore); both wait while the store works
 * (iap_busy). Once it is bought the panel closes and everything opens (unlock_poll). */
static struct {
    int open, made;                             /* made: the screen's short side its pictures were made for (0: make
                                                 * them again) */
    box card, btn, rbtn;                        /* rbtn: Restore purchases (an in-app purchase) */
    float xx, xy, xr;                           /* its close button */
    float icon_y, name_y, line_y[2];
    int icon_d;
    SDL_Texture *icon, *name, *line[2], *get, *restore;
    int tw[5], th[5];                           /* name, line 0, line 1, the button's word, restore: their sizes */
} FV;

static const uint8_t FV_CREAM[3] = {246, 238, 220};

/* one line of text in cream (bold or not), px pixels, no wider than maxw (made smaller to fit) */
static SDL_Texture *fv_text(const char *s, float px, int bold, float maxw, int *w, int *h)
{
    int tw = text_line(s, px, bold, FV_CREAM, NULL, 0, 0, 0, 0);
    if (tw > maxw && tw > 0) {
        px = fmaxf(6, px * maxw / tw);
        tw = text_line(s, px, bold, FV_CREAM, NULL, 0, 0, 0, 0);
    }
    int W = tw + 4, H = (int)ceilf(px * 1.55f);
    uint8_t *b = calloc((size_t)W * H, 4);
    if (!b) return NULL;
    text_line(s, px, bold, FV_CREAM, b, W, H, 2, (int)((H - 1.25f * px) / 2));
    SDL_Texture *t = ui_tex(W, H, b);
    free(b);
    *w = W;
    *h = H;
    return t;
}

/* the alpha of a picture blurred (a box blur, r pixels, twice): a soft shadow */
static void fv_blur(uint8_t *a, int w, int h, int r)
{
    uint8_t *t = malloc((size_t)w * h);
    if (!t || r < 1) { free(t); return; }
    for (int pass = 0; pass < 4; pass++) {      /* across, down, across, down */
        int across = !(pass & 1), n = across ? w : h, m = across ? h : w;
        for (int j = 0; j < m; j++) {
            int sum = 0, cnt = 0;
            for (int i = -r; i < n + r; i++) {
                if (i + r < n) { sum += a[across ? j * w + i + r : (i + r) * w + j]; cnt++; }
                if (i - r - 1 >= 0) { sum -= a[across ? j * w + i - r - 1 : (i - r - 1) * w + j]; cnt--; }
                if (i >= 0 && i < n) t[across ? j * w + i : i * w + j] = (uint8_t)(cnt ? sum / cnt : 0);
            }
        }
        memcpy(a, t, (size_t)w * h);
    }
    free(t);
}

/* a picture laid on the icon centred at (cx, cy) (fractions of it), turned deg, over its soft shadow */
static void fv_lay(uint8_t *o, int d, const uint8_t *p, int pw, int ph, float cx, float cy, float deg)
{
    uint8_t *sh = calloc((size_t)d * d, 4), *a = malloc((size_t)d * d);
    if (sh && a) {
        art_blend_rot(sh, d, d, p, pw, ph, cx * d + 7.0f * d / 480, cy * d + 9.0f * d / 480, deg);
        for (int i = 0; i < d * d; i++) a[i] = sh[4 * i + 3];
        fv_blur(a, d, d, (int)fmaxf(1, 4.0f * d / 480));
        for (int i = 0; i < d * d; i++) {
            sh[4 * i] = sh[4 * i + 1] = sh[4 * i + 2] = 0;
            sh[4 * i + 3] = (uint8_t)(a[i] * 0.55f);
        }
        art_blend(o, d, d, sh, d, d, 0, 0);
    }
    free(sh);
    free(a);
    art_blend_rot(o, d, d, p, pw, ph, cx * d, cy * d, deg);
}

/* the paid app's icon, d x d: apps/android/tools/make_icon.py's board_icon, smaller */
static void fv_icon(int d, uint8_t *o)
{
    static const uint8_t out[4] = {22, 18, 40, 255}, gold_d[4] = {186, 128, 22, 255}, gold[4] = {252, 216, 72, 255},
                         gold_l[4] = {255, 240, 150, 255};
    static const struct { float x0, y0, x1, y1, r; const uint8_t *c; } ring[] = {
        {0, 0, 480, 480, 60, out}, {5, 5, 475, 475, 56, gold_d}, {5, 5, 469, 469, 56, gold},
        {10, 10, 464, 464, 52, gold_l}, {15, 15, 464, 464, 48, gold}, {22, 22, 458, 458, 42, out}};
    memset(o, 0, (size_t)d * d * 4);
    float k = d / 480.0f;
    for (size_t i = 0; i < sizeof ring / sizeof ring[0]; i++) {
        int x = (int)lroundf(ring[i].x0 * k), y = (int)lroundf(ring[i].y0 * k);
        int w = (int)lroundf((ring[i].x1 - ring[i].x0) * k), h = (int)lroundf((ring[i].y1 - ring[i].y0) * k);
        art_roundrect_put(o, d, d, x, y, w, h, ring[i].r * k, ring[i].c, 1);
    }
    int x0 = (int)lroundf(26 * k), w = (int)lroundf(428 * k);
    uint8_t *t = calloc((size_t)w * w, 4);      /* the table, lighter in the middle */
    if (t) {
        const uint8_t fill[4] = {255, 255, 255, 255};
        art_roundrect(w, w, 38 * k, fill, t);
        for (int y = 0; y < w; y++)
            for (int x = 0; x < w; x++) {
                float dd = fminf(1, hypotf((x + x0) / k - 240, (y + x0) / k - 200) / 300), f = 1.25f - 0.45f * dd;
                uint8_t *p = t + 4 * (y * w + x);
                p[0] = (uint8_t)clampf(34 * f, 0, 255);
                p[1] = (uint8_t)clampf(52 * f, 0, 255);
                p[2] = (uint8_t)clampf(46 * f, 0, 255);
            }
        art_blend(o, d, d, t, w, w, x0, x0);
        free(t);
    }
    int ps = (int)(320 * k), ds = (int)(150 * k), cw = (int)(200 * k), ch = (int)(cw * cards_aspect(DECK_FRENCH) + 0.5f);
    uint8_t *b = calloc((size_t)ps * ps + (size_t)cw * ch, 4);
    if (!b) return;
    cards_paint(DECK_FRENCH, CARD(SUIT_HEARTS, RANK_ACE), cw, ch, b);
    fv_lay(o, d, b, cw, ch, 322 / 480.0f, 190 / 480.0f, 14);
    memset(b, 0, (size_t)ds * ds * 4);
    art_die(5, 1, ds, b);
    fv_lay(o, d, b, ds, ds, 356 / 480.0f, 366 / 480.0f, -16);
    memset(b, 0, (size_t)ps * ps * 4);
    art_piece(KNIGHT, WHITE, ps, b);
    fv_lay(o, d, b, ps, ps, 182 / 480.0f, 258 / 480.0f, 0);
    free(b);
}

static void fv_layout(void)
{
    float s = fminf(ui.w, ui.h), m = floorf(s * 0.05f), pad = floorf(s * 0.05f);
    float pw = floorf(fminf(s * 0.84f, ui.w - 2 * m)), maxw = pw - 2 * pad;
    int sell = iap_available();                 /* the unlock sold in the app: its own words, the price, Restore */
    const char *words = sell ? "_unlock" : "_full";
    if (FV.made != (int)s) {                    /* the pictures, at this size */
        FV.made = (int)s;
        ui_free(&FV.icon);
        ui_free(&FV.name);
        ui_free(&FV.get);
        ui_free(&FV.restore);
        for (int i = 0; i < 2; i++) ui_free(&FV.line[i]);
        FV.icon_d = (int)(s * 0.27f);
        uint8_t *b = calloc((size_t)FV.icon_d * FV.icon_d, 4);   /* its own: cards_paint may use ui_buf() */
        if (b) {
            fv_icon(FV.icon_d, b);
            FV.icon = ui_tex(FV.icon_d, FV.icon_d, b);
            free(b);
        }
        FV.name = fv_text(sell ? ed_app_name : ed_paid_name[0] ? ed_paid_name : "RetroStone Board", s * 0.054f, 1, maxw,
                          &FV.tw[0], &FV.th[0]);
        char t[128];
        for (int i = 0; i < 2; i++)
            FV.line[i] = text_word(words, i, t, sizeof t) ? fv_text(t, s * 0.04f, 0, maxw, &FV.tw[1 + i], &FV.th[1 + i])
                                                          : NULL;
        int got = text_word(words, 2, t, sizeof t);
        if (sell && got && iap_price_text()[0]) {   /* the word, an en dash, the price */
            size_t n = strlen(t);
            snprintf(t + n, sizeof t - n, " \xe2\x80\x93 %s", iap_price_text());
        }
        FV.get = got ? fv_text(t, s * 0.05f, 1, maxw * 0.8f, &FV.tw[3], &FV.th[3]) : NULL;
        if (sell && text_word(words, 3, t, sizeof t))
            FV.restore = fv_text(t, s * 0.038f, 0, maxw * 0.8f, &FV.tw[4], &FV.th[4]);
    }
    float bh = floorf(s * 0.12f), lh = FV.line[0] ? (float)FV.th[1] : s * 0.062f, rh = floorf(s * 0.085f);
    float h = pad + FV.icon_d + s * 0.03f + FV.th[0] + s * 0.02f + 2 * lh + s * 0.06f + bh + pad;
    if (sell) h += s * 0.04f + rh;              /* Restore purchases, under the gold button */
    FV.card = (box){floorf((ui.w - pw) / 2), floorf((ui.h - h) / 2), pw, floorf(h)};
    float y = FV.card.y + pad;
    FV.icon_y = y + FV.icon_d / 2.0f;
    y += FV.icon_d + s * 0.03f;
    FV.name_y = y + FV.th[0] / 2.0f;
    y += FV.th[0] + s * 0.02f;
    for (int i = 0; i < 2; i++, y += lh) FV.line_y[i] = y + lh / 2;
    y += s * 0.06f;
    float bw = floorf(fmaxf(pw * 0.5f, (FV.get ? FV.tw[3] : 0) + s * 0.12f));
    FV.btn = (box){floorf((ui.w - bw) / 2), floorf(y), bw, bh};
    float rw = floorf(fmaxf(pw * 0.4f, (FV.restore ? FV.tw[4] : 0) + s * 0.1f));
    FV.rbtn = sell ? (box){floorf((ui.w - rw) / 2), floorf(y + bh + s * 0.04f), rw, rh} : (box){0, 0, 0, 0};
    FV.xr = floorf(s * 0.045f);
    FV.xx = FV.card.x + FV.card.w - pad * 0.3f - FV.xr;
    FV.xy = FV.card.y + pad * 0.3f + FV.xr;
}

static void full_open(void)
{
    if (!free_now()) return;
    FV.open = 1;
}

/* the paid app's page: the Play Store's (market://) on Android, its web page when that cannot open, or on a computer;
 * the picture mode opens nothing (it says so on stderr) */
static void open_store(void)
{
    char url[256];
#ifdef __ANDROID__
    snprintf(url, sizeof url, "market://details?id=%s", ed_paid_id);
#else
    snprintf(url, sizeof url, "%s", ed_paid_url[0] ? ed_paid_url : "");
#endif
    if (shot_clock) {
        fprintf(stderr, "store: %s\n", url);
        return;
    }
    if (!url[0] || SDL_OpenURL(url) != 0) {
        SDL_Log("cannot open %s (%s): %s", url, SDL_GetError(), ed_paid_url);
        if (ed_paid_url[0]) SDL_OpenURL(ed_paid_url);
    }
}

static void draw_full(void)
{
    static const uint8_t panel[4] = {28, 34, 32, 250}, gold[4] = {150, 118, 46, 255}, quiet[4] = {58, 68, 64, 255};
    fv_layout();
    float s = fminf(ui.w, ui.h), cx = ui.w / 2.0f;
    ui_fill(0, 0, (float)ui.w, (float)ui.h, 0, 0, 0, 120);
    box c = FV.card;
    ui_put(rounded((int)c.w, (int)c.h, floorf(s * 0.028f), panel), c.x, c.y, c.w, c.h);
    if (FV.icon) ui_put_c(FV.icon, cx, FV.icon_y, (float)FV.icon_d, (float)FV.icon_d);
    if (FV.name) ui_put_c(FV.name, cx, FV.name_y, (float)FV.tw[0], (float)FV.th[0]);
    for (int i = 0; i < 2; i++)
        if (FV.line[i]) ui_put_c(FV.line[i], cx, FV.line_y[i], (float)FV.tw[1 + i], (float)FV.th[1 + i]);
    box b = FV.btn;                             /* the button: gold, in gold light */
    ui_put_c(t_gold, b.x + b.w / 2, b.y + b.h / 2, b.w * 1.3f, b.h * 2.4f);
    ui_put(rounded((int)b.w, (int)b.h, b.h / 2, gold), b.x, b.y, b.w, b.h);
    if (FV.get) ui_put_c(FV.get, b.x + b.w / 2, b.y + b.h / 2, (float)FV.tw[3], (float)FV.th[3]);
    else ui_put_c(icon_tex(ICON_ARROW), b.x + b.w / 2, b.y + b.h / 2, b.h * 0.5f, b.h * 0.5f);
    box r = FV.rbtn;                            /* Restore purchases: a quiet button */
    if (r.w > 0) {
        ui_put(rounded((int)r.w, (int)r.h, r.h / 2, quiet), r.x, r.y, r.w, r.h);
        if (FV.restore) ui_put_c(FV.restore, r.x + r.w / 2, r.y + r.h / 2, (float)FV.tw[4], (float)FV.th[4]);
    }
    if (iap_busy()) {                           /* the store at work: both buttons wait, dimmed */
        float y0 = b.y - b.h * 0.7f, y1 = (r.w > 0 ? r.y + r.h : b.y + b.h) + b.h * 0.2f;
        ui_fill(c.x, y0, c.w, y1 - y0, panel[0], panel[1], panel[2], 170);
        ui_animate();
    }
    draw_button(FV.xx, FV.xy, FV.xr, ICON_CLOSE, NULL);
}

/* a press while the panel is open: its button (and Restore), else it closes (its close button, outside it) */
static void full_press(float x, float y)
{
    fv_layout();                                /* (the screen may have turned since it was drawn) */
    box b = FV.btn, c = FV.card, r = FV.rbtn;
    if (x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h) {
        if (!iap_available()) {
            snd_play(SND_TAP);
            open_store();
        } else if (iap_busy() || !iap_buy()) {  /* (the product not known yet: the store is asked again) */
            snd_play(SND_DENY);
        } else {
            snd_play(SND_TAP);
        }
        return;
    }
    if (r.w > 0 && x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h) {
        snd_play(iap_busy() ? SND_DENY : SND_TAP);
        if (!iap_busy()) iap_restore();
        return;
    }
    if (hypotf(x - FV.xx, y - FV.xy) < FV.xr * 1.2f || x < c.x || y < c.y || x >= c.x + c.w || y >= c.y + c.h) {
        FV.open = 0;
        snd_play(SND_TAP);
    }
}

/* the in-app purchase (iap.h): started with what the last run kept ("shell/unlocked"), before the settings are read;
 * an app already unlocked sorts its menu again (nothing locked to put last). Never unlocked without one. */
static unsigned iap_seen;
static int by_order(const void *a, const void *b);
static void layout(void);
static void help_layout(void);

static void unlock_start(void)
{
    uint8_t b = 0;
    ui_save_get("shell/unlocked", &b, 1);
    iap_init(ed_iap_product, b == 1);
    iap_seen = iap_changes();
    if (ed_free && iap_unlocked()) qsort(games, (size_t)ngames, sizeof games[0], by_order);
}

/* once a frame: what the store says, kept; bought (or restored): the panel closes and everything opens at once (the
 * menu keeps its order until the next launch); the panel's pictures follow the price */
static void unlock_poll(void)
{
    unsigned n = iap_changes();
    if (n == iap_seen) return;
    iap_seen = n;
    uint8_t now = (uint8_t)(iap_unlocked() != 0), was = 0;
    ui_save_get("shell/unlocked", &was, 1);
    if (now != was) ui_save_set("shell/unlocked", &now, 1);
    if (now) FV.open = 0;
    FV.made = 0;
    layout();
    need_frame = 1;
}

/* a lock on a dark disc, d across: a locked game's card, a locked level's tile */
static void lock_badge(float cx, float cy, float d)
{
    ui_put_c(btn_tex(paint_badge, 0, (int)d), cx, cy, d, d);
    SDL_Texture *t = btn_tex(paint_btn_icon, ICON_LOCK, (int)(0.72f * d));
    if (!t) return;
    SDL_SetTextureColorMod(t, BTN_CREAM);
    ui_put_c(t, cx, cy, 0.72f * d, 0.72f * d);
    SDL_SetTextureColorMod(t, 255, 255, 255);
}

/* the picker's names: help/<lang>/<id>_variants.txt, a line a card in its order (two axes: the sizes, then the levels;
 * a game of levels: its random play's variants). Line `line` in the language, else in English; 0: none (no file, an
 * empty line). */
#define PICK_LAB 0.3f                           /* the name's room under a picker card's picture (of its width) */
static int vname(int g, int line, char *buf, int size)
{
    char key[64];
    snprintf(key, sizeof key, "%s_variants", games[g]->id);
    const char *lang[2] = {text_lang(), "en"};
    for (int l = 0; l < 2; l++)
        for (int i = 0; i < tx_ntexts; i++) {
            if (strcmp(tx_texts[i].game, key) || strcmp(tx_texts[i].lang, lang[l])) continue;
            const char *t = tx_texts[i].text;
            for (int k = 0; t && k < line; k++) {
                t = strchr(t, '\n');
                if (t) t++;
            }
            int n = t ? (int)strcspn(t, "\r\n") : 0;
            while (n > 0 && t[n - 1] == ' ') n--;
            if (n <= 0) break;                  /* none in this language: English's */
            if (n >= size) n = size - 1;
            memcpy(buf, t, (size_t)n);
            buf[n] = 0;
            return 1;
        }
    return 0;
}

static int vnames(int g)                        /* the picker shows names under its cards */
{
    char b[128];
    int n = axes_on(g) ? games[g]->axes[0] + games[g]->axes[1] : games[g]->variants;
    for (int i = 0; i < n && i < 2 * MAX_VARIANTS; i++)
        if (vname(g, i, b, sizeof b)) return 1;
    return 0;
}

/* the Games page's search: game i's name (in the language) holds the words (both folded: no accents, small) */
static int search_hits;
static int found(int i)
{
    char name[96], f[192];
    const char *q = pages_search();
    if (!q) return 1;
    if (!text_name(games[i]->id, name, sizeof name)) return 0;
    text_fold(name, f, sizeof f);
    return strstr(f, q) != NULL;
}

int shell_search_hits(void) { return search_hits; }

/* the menu's tabs: one row. All of them in it when they fit (centred); else at least TAB_MIN times as wide as high,
 * from the room's margin, the row scrolled sideways (dragged, flung, the mouse wheel; the arrows' tab brought into
 * view), faded at an edge with more tabs past it */
#define TAB_MIN 1.15f

/* the tab chosen brought into the row's view (with a neighbour's edge beside it): at once, or sliding there */
static void tabs_show(int now)
{
    menu.tgoal = -1;
    menu.tvel = 0;
    if (menu.tmax <= 0) {
        menu.tx = 0;
        return;
    }
    for (int k = 0; k < L.ntabs; k++) {
        if (L.tab_sec[k] != menu.tab) continue;
        box b = L.tabs[k], r = L.tab_row;
        float pad = 0.75f * r.h, lo = b.x - r.x - pad, hi = b.x + b.w + pad - (r.x + r.w);
        float to = clampf(fminf(fmaxf(menu.tx, hi), lo), 0, menu.tmax);
        if (now || instant) menu.tx = to;
        else if (fabsf(to - menu.tx) >= 0.5f) {
            menu.tgoal = to;
            menu.tt = SDL_GetTicks();
            need_frame = 1;
        }
        return;
    }
}

static void tabs_scroll(float d)                /* the row moved by d pixels (the wheel) */
{
    menu.tgoal = -1;
    menu.tvel = 0;
    menu.tx = clampf(menu.tx + d, 0, menu.tmax);
    need_frame = 1;
}

/* the menu (all the games) or the picker (one game's variants): cards in a grid */
static void layout_cards(int n)
{
    float m = fminf(ui.w, ui.h) * 0.05f, g = fminf(ui.w, ui.h) * 0.04f, head = fminf(ui.w, ui.h) * 0.16f;
    if (screen == SCR_PICK)                     /* (a game in progress: its resume button; random play: the stars) */
        head = head_row(head, games[cur]->levels > 0 && games[cur]->level_stars != NULL);
    float hd = fminf(ui.w, ui.h) * 0.16f;         /* the header alone (its buttons), the row apart */
    float top = head;                           /* the buttons' row (the tabs, the pills go below it) */
    L.ntabs = 0;
    box room = {0, 0, (float)ui.w, (float)ui.h};   /* the menu: the Games page's room (the ribbon apart) */
    if (screen == SCR_MENU) {                   /* the sections as tabs under the page's header, a picture each */
        pages_layout();
        room = pages_room();
        for (int r = 0; r < SECTIONS; r++)
            for (int i = 0, s = SEC_SHOWN[r]; i < n; i++)
                if (section_of(games[i]) == s) { L.tab_sec[L.ntabs++] = s; break; }
        int shown_tab = 0;
        for (int k = 0; k < L.ntabs; k++) shown_tab |= L.tab_sec[k] == menu.tab;
        if (!shown_tab && L.ntabs) menu.tab = L.tab_sec[0];
        float th = floorf(fminf(ui.w, ui.h) * 0.12f), tg = floorf(g * 0.4f), ty = floorf(pages_games_top());
        float tw = floorf(fminf((room.w - 2 * m - (L.ntabs - 1) * tg) / (L.ntabs ? L.ntabs : 1), 2.2f * th));
        float tx = room.x + (room.w - (L.ntabs * tw + (L.ntabs - 1) * tg)) / 2;
        menu.tmax = 0;
        if (tw < floorf(TAB_MIN * th)) {        /* too many to fit: the row scrolls, k whole tabs in view and half
                                                 * of the next one at the edge (more that way) */
            int k = (int)((room.w - m) / (TAB_MIN * th + tg) - 0.5f);
            k = k < 1 ? 1 : k;
            tw = floorf(fminf((room.w - m - k * tg) / (k + 0.5f), 2.2f * th));
            tx = room.x + m;
            menu.tmax = fmaxf(0, 2 * m + L.ntabs * tw + (L.ntabs - 1) * tg - room.w);
        }
        for (int k = 0; k < L.ntabs; k++) L.tabs[k] = (box){floorf(tx + k * (tw + tg)), ty, tw, th};
        L.tab_row = (box){room.x, ty, room.w, th};
        L.snd_r = floorf(th * 0.45f);           /* (the buttons' size: the shared pictures') */
        head = ty + th + floorf(g * 0.5f);
        if (pages_search()) {                   /* searching: every tab's games, no tabs */
            L.ntabs = 0;
            menu.tmax = 0;
            head = ty;
        }
        int key = (int)room.w * 100000 + (int)tw * 10 + L.ntabs;   /* another row: the tab chosen in view at once */
        if (key != menu.tkey) {
            menu.tkey = key;
            tabs_show(1);
        }
        menu.tx = clampf(menu.tx, 0, menu.tmax);
        if (menu.tgoal >= 0) menu.tgoal = clampf(menu.tgoal, 0, menu.tmax);
        tab_textures((int)tw, (int)th);
    }
    float below = top;                          /* where the pills and the cards may start */
    if (screen == SCR_PICK && games[cur]->levels) {   /* random play's picker: the level screen's tabs over it */
        chapters_make(cur);
        head = below = level_tabs(head);
    }
    L.npills = 0;
    if (screen == SCR_PICK && many_counts(cur)) {   /* how many play: a row of pills under the buttons, each with
                                                     * that many of the game's tokens */
        unsigned cnt = player_counts(cur);
        int most = 0;
        for (int k = 1; k <= MAX_PLAYERS; k++)
            if (cnt >> k & 1) { L.pill_n[L.npills++] = k; most = k; }
        float ph = fminf(ui.w, ui.h) * 0.12f, pw = ph * (0.5f + most * 0.44f), pg = ph * 0.3f;
        float fit = (ui.w - 2 * m) / (L.npills * pw + (L.npills - 1) * pg);
        int per = L.npills;
        if (fit < (ui.w > ui.h ? 0.5f : 0.75f) && L.npills > 2) {   /* too small in one row (a phone upright, five
                                                                 * counts): two rows; landscape: rather one */
            per = (L.npills + 1) / 2;
            fit = (ui.w - 2 * m) / (per * pw + (per - 1) * pg);
        }
        if (fit < 1) { ph *= fit; pw *= fit; pg *= fit; }
        ph = floorf(ph);
        pw = floorf(pw);
        for (int k = 0; k < L.npills; k++) {     /* each row centred (the second one shorter) */
            int r = k / per, inrow = r ? L.npills - per : per;
            float x = (ui.w - (inrow * pw + (inrow - 1) * pg)) / 2;
            L.pills[k] = (box){floorf(x + (k - r * per) * (pw + pg)), floorf(head + r * (ph + pg)), pw, ph};
        }
        L.pill_rows = (L.npills + per - 1) / per;
        head += L.pill_rows * ph + (L.pill_rows - 1) * pg + g;
        int ts = (int)(ph * 0.56f), key = (cur * 100000 + (int)pw * 100 + (int)ph) * 2 + TH.dark;
        if (made_pill != key) {
            made_pill = key;
            const uint8_t off[4] = {54, 72, 64, 255}, on[4] = {150, 118, 46, 255};
            ui_free(&t_pill);
            if (TH.dark) art_roundrect((int)pw, (int)ph, ph / 2, off, ui_buf((int)pw, (int)ph));
            else light_card((int)pw, (int)ph, ph / 2, LT_DISC, ui_buf((int)pw, (int)ph));
            t_pill = ui_tex((int)pw, (int)ph, ui_buf((int)pw, (int)ph));
            ui_free(&t_pill_on);
            if (TH.dark) art_roundrect((int)pw, (int)ph, ph / 2, on, ui_buf((int)pw, (int)ph));
            else light_card((int)pw, (int)ph, ph / 2, LT_GOLD, ui_buf((int)pw, (int)ph));
            t_pill_on = ui_tex((int)pw, (int)ph, ui_buf((int)pw, (int)ph));
            uint8_t *b = calloc((size_t)ts * ts, 4);   /* its own buffer: the game may use ui_buf() */
            for (int p = 0; p < MAX_PLAYERS; p++) {
                ui_free(&t_ptok[p]);
                if (p >= most) continue;
                memset(b, 0, (size_t)ts * ts * 4);
                games[cur]->token(p, ts, b);
                t_ptok[p] = ui_tex(ts, ts, b);
            }
            free(b);
        }
    }
    L.nseats = 0;
    L.online = (box){0, 0, 0, 0};
    if (screen == SCR_PICK && has_bot(cur)) {   /* who plays each seat: a chip each (its token, then a person or the
                                                 * device's robot), under the counts: one row, or two on a narrow
                                                 * screen */
        int ns = ui.players, per = ns, on = ns > 1;   /* on: a round online chip ends the last row (online play: not
                                                    * there yet, a tap says so; it never changes a seat) */
        float sh = fminf(ui.w, ui.h) * 0.12f, sw = sh * 1.95f, sg = sh * 0.3f, ow = on ? sh : -sg;
        float fit = (ui.w - 2 * m) / (ns * sw + ns * sg + ow);
        if (fit < 0.8f && ns > 2) {
            per = (ns + 1) / 2;
            fit = (ui.w - 2 * m) / fmaxf(per * sw + (per - 1) * sg, (ns - per) * (sw + sg) + ow);
        }
        if (fit < 1) { sh *= fit; sw *= fit; sg *= fit; ow *= fit; }
        sh = floorf(sh);
        sw = floorf(sw);
        for (int k = 0; k < ns; k++) {
            int r = k / per, inrow = r ? ns - per : per, last = r == (ns - 1) / per;
            float x = (ui.w - (inrow * sw + (inrow - 1) * sg + (last && on ? sg + sh : 0))) / 2;
            L.seats[k] = (box){floorf(x + (k - r * per) * (sw + sg)), floorf(head + r * (sh + sg)), sw, sh};
        }
        L.online = on ? (box){L.seats[ns - 1].x + sw + floorf(sg), L.seats[ns - 1].y, sh, sh} : (box){0, 0, 0, 0};
        L.nseats = ns;
        int rows = (ns + per - 1) / per;
        head += rows * sh + (rows - 1) * sg + g;
    }
    float lab = screen == SCR_MENU ? 0.22f : screen == SCR_PICK && vnames(cur) ? PICK_LAB : 0;   /* a name under each */
    int cols = ui.w > ui.h ? (n > 3 ? 3 : n) : (n > 1 ? 2 : 1);
    if (screen == SCR_PICK) {                   /* the picker: the grid that makes the cards biggest */
        float W = ui.w - 2 * m, H = ui.h - head - 2 * m, best = 0;
        for (int k = 1; k <= n; k++) {
            int r = (n + k - 1) / k;
            float cc = fminf(fminf((W - (k - 1) * g) / k, (H - (r - 1) * g) / r / (1 + lab)), fminf(ui.w, ui.h) * 0.62f);
            if (cc > best + 0.5f) { best = cc; cols = k; }
        }
    }
    if (screen == SCR_MENU) {                   /* cards about a quarter of the screen's width: as many as fit */
        cols = (int)((room.w - 2 * m + g) / (fminf(ui.w, ui.h) * 0.26f + g));
        cols = cols < 2 ? 2 : cols > n ? n : cols;
        if (n == 1) cols = 1;
    }
    int rows = (n + cols - 1) / cols;
    float c = (room.w - 2 * m - (cols - 1) * g) / cols;
    if (screen != SCR_MENU) c = fminf(c, (ui.h - head - 2 * m - (rows - 1) * g) / rows / (1 + lab));
    c = floorf(fminf(c, fminf(ui.w, ui.h) * 0.62f));
    float ch = floorf(c * (1 + lab)), x0 = room.x + (room.w - (cols * c + (cols - 1) * g)) / 2, tall = 0;
    L.sec_h = floorf(c * 0.34f);
    L.sec_x = floorf(x0);
    L.sec_w = cols * c + (cols - 1) * g;
    for (int s = 0; s < SECTIONS; s++) L.sec_y[s] = -1;
    search_hits = 0;
    if (screen == SCR_MENU) {                   /* the sections one after the other: a heading, then its games */
        for (int r = 0, i = 0; r < SECTIONS; r++) {   /* (the games sorted in the tabs' order: by_order) */
            int s = SEC_SHOWN[r], first = i;
            while (i < n && section_of(games[i]) == s) i++;
            if (i == first) continue;
            int k = 0;                          /* the games shown: the tab's, or every tab's the search finds */
            for (int j = first; j < i; j++) {
                L.cards[j] = (box){0, 0, 0, 0};
                if (pages_search() ? !found(j) : s != menu.tab) continue;
                L.cards[j] = (box){floorf(x0 + k % cols * (c + g)), tall + L.sec_h + k / cols * (ch + g), c, ch};
                k++;
            }
            if (!k) continue;
            L.sec_y[s] = tall;
            tall += L.sec_h + (k + cols - 1) / cols * (ch + g);
            search_hits += k;
        }
        tall = fmaxf(0, tall - g);
    } else if (screen == SCR_PICK && axes_on(cur)) {   /* two axes: the sizes, then the levels */
        tall = axes_layout(ui.h - head - 2 * m, g, lab, &c);
        ch = floorf(c * (1 + lab));
        n = games[cur]->axes[0] + games[cur]->axes[1];
    } else {
        for (int i = 0; i < n; i++) {           /* the last row centred */
            int r = i / cols, k = r == rows - 1 ? n - r * cols : cols;
            float xr = (ui.w - (k * c + (k - 1) * g)) / 2;
            L.cards[i] = (box){floorf(xr + (i % cols) * (c + g)), r * (ch + g), c, ch};
        }
        tall = rows * ch + (rows - 1) * g;
    }
    float y0 = head + (ui.h - head - tall) / 2;
    if (screen == SCR_MENU) {                   /* from the top, scrolling when taller than the screen (the picker
                                                 * keeps the menu's place, for the way back) */
        y0 = head;
        menu.top = head;
        menu.max = fmaxf(0, y0 + tall + m - (room.y + room.h));
        menu.scroll = fminf(menu.max, fmaxf(0, menu.scroll));
    }
    if (L.npills || L.nseats) {                 /* the picker: the pills, the seats and the cards, one block in the
                                                 * middle */
        float t0 = L.npills ? L.pills[0].y : L.seats[0].y;
        float t1 = L.nseats ? L.seats[L.nseats - 1].y + L.seats[0].h : L.pills[L.npills - 1].y + L.pills[0].h;
        float ph = t1 - t0;                     /* their rows */
        float start = below + fmaxf(0, (ui.h - below - m - (ph + g + tall)) / 2), dy = floorf(start) - t0;
        for (int k = 0; k < L.npills; k++) L.pills[k].y += dy;
        for (int k = 0; k < L.nseats; k++) L.seats[k].y += dy;
        L.online.y += dy;
        y0 = start + ph + g;
    }
    for (int i = 0; i < n; i++) L.cards[i].y = floorf(L.cards[i].y + y0);
    for (int s = 0; s < SECTIONS; s++)
        if (L.sec_y[s] >= 0) L.sec_y[s] = floorf(L.sec_y[s] + y0);
    L.cols = cols;
    L.label = (int)(ch - c);
    if (made_card != ((int)c * 1000 + L.label) * 2 + TH.dark || L.sec_w != made_sec_w) {   /* (the theme: the names'
                                                                                         * ink) */
        for (int i = 0; i < MAX_GAMES; i++) ui_free(&t_name[i]);
        for (int s = 0; s < SECTIONS; s++) ui_free(&t_sec[s]);
        made_sec_w = L.sec_w;
    }
    L.card = (int)c;
    if (screen != SCR_MENU) {                   /* the picker's row: home at the left */
        L.snd_r = hd * 0.28f;
        L.snd_x = ui.w - m - L.snd_r;
        L.snd_y = hd * 0.55f;
    }
    ui.unit = fminf(ui.w, ui.h) / 12.0f;
    L.br = L.snd_r;
    shared_textures();
    if (made_card != (L.card * 1000 + L.label) * 2 + TH.dark) {
        made_card = (L.card * 1000 + L.label) * 2 + TH.dark;
        const uint8_t back[4] = {52, 72, 64, 255};
        int h = L.card + L.label;
        ui_free(&t_menu_card);
        if (TH.dark) art_roundrect(L.card, h, 0.1f * L.card, back, ui_buf(L.card, h));
        else light_card(L.card, h, 0.1f * L.card, LT_DISC, ui_buf(L.card, h));
        t_menu_card = ui_tex(L.card, h, ui_buf(L.card, h));
        const uint8_t gold[4] = {150, 118, 46, 255};   /* the picker's size chosen (two axes) */
        ui_free(&t_menu_card_on);
        if (TH.dark) art_roundrect(L.card, h, 0.1f * L.card, gold, ui_buf(L.card, h));
        else light_card(L.card, h, 0.1f * L.card, LT_GOLD, ui_buf(L.card, h));
        t_menu_card_on = ui_tex(L.card, h, ui_buf(L.card, h));
        const uint8_t shade[4] = {70, 50, 20, 255};   /* the light theme's: a soft shadow under each card */
        ui_free(&t_menu_shadow);
        if (!TH.dark) {
            art_roundrect(L.card, h, 0.1f * L.card, shade, ui_buf(L.card, h));
            t_menu_shadow = ui_tex(L.card, h, ui_buf(L.card, h));
        }
        /* the pictures are kept at their size (thumb_ex): back from a picker, the menu's are still there */
    }
}

/* the level screen's tabs (the chapters, then a die for random play when the game has it) under the top row's
 * buttons, at head; where what is under them starts (head: no tabs, a single chapter) */
static int level_ntabs(void) { return LV.nch + has_random(cur); }

static float level_tabs(float head)
{
    float s = fminf(ui.w, ui.h), m = s * 0.05f, g = s * 0.04f;
    int n = level_ntabs();
    if (n <= 1) return head;
    float th = floorf(s * 0.12f), tg = floorf(g * 0.4f);
    float tw = floorf(fminf((ui.w - 2 * m - (n - 1) * tg) / n, 2.4f * th));
    float tx = (ui.w - (n * tw + (n - 1) * tg)) / 2;
    for (int k = 0; k < n; k++) LV.tabs[k] = (box){floorf(tx + k * (tw + tg)), floorf(head), tw, th};
    tab_textures((int)tw, (int)th);
    return head + th + g;
}

/* the level screen: home at the top left (the stars won at the right), the chapters' tabs under it (as the menu's),
 * then the chapter's tiles in a grid right under them, as big as the room allows (at most 0.31 of the screen's short
 * side): a chapter of few levels gets bigger tiles */
static void layout_levels(void)
{
    float s = fminf(ui.w, ui.h), m = s * 0.05f, head = s * 0.16f;
    chapters_make(cur);
    LV.chapter = LV.chapter >= LV.nch ? LV.nch - 1 : LV.chapter < 0 ? 0 : LV.chapter;
    float top = level_tabs(head_row(head, games[cur]->level_stars != NULL));
    int most = LV.first[LV.chapter + 1] - LV.first[LV.chapter];   /* the chapter shown: few levels, bigger tiles */
    most = most < 1 ? 1 : most > CHAPTER_MAX ? CHAPTER_MAX : most;
    float W = ui.w - 2 * m, H = ui.h - m - top, cap = s * 0.31f, bp = 0, bs = 0, ba = 0;
    int cols = 1;
    for (int c = 1; c <= most; c++) {           /* big tiles, few gaps in the last row (25: five by five, not four
                                                 * by seven); even: the grid's shape nearest the room's */
        int r = (most + c - 1) / c;
        float p = floorf(fminf(fminf(W / c, H / r), cap)), sc = p * (1 - 0.04f * (c * r - most));
        float a = fabsf(logf((float)c / r) - logf(W / H));
        if (c == 1 || sc > bs + 0.5f || (fabsf(sc - bs) <= 0.5f && a < ba)) {
            cols = c;
            bp = p;
            bs = sc;
            ba = a;
        }
    }
    float tile = floorf(bp * 0.84f), x0 = (ui.w - cols * bp) / 2, y0 = top;   /* right under the tabs */
    for (int i = 0; i < most; i++)
        LV.tiles[i] = (box){floorf(x0 + (i % cols) * bp + (bp - tile) / 2),
                            floorf(y0 + (i / cols) * bp + (bp - tile) / 2), tile, tile};
    L.snd_r = head * 0.28f;                     /* the top row's buttons, as the menu's and the picker's */
    L.snd_x = ui.w - m - L.snd_r;
    L.snd_y = head * 0.55f;
    ui.unit = s / 12.0f;
    L.br = L.snd_r;
    shared_textures();
}

static void layout(void)
{
    if (help.open) help_layout();
    if (screen == SCR_GAME) layout_game();
    else if (screen == SCR_LEVELS) layout_levels();
    else layout_cards(screen == SCR_MENU ? ngames : games[cur]->variants);
}

/* ---- the bots' seats (bots.h) ------------------------------------------------------------------------------------ */
/* a game's bot (one that thinks; never for a game of levels: a puzzle is the player's own) */
static const bot *bot_of(int g)
{
    const bot *b = g >= 0 && !games[g]->levels ? bot_find(games[g]->id) : NULL;
    return b && b->think ? b : NULL;
}
static int has_bot(int g) { return bot_of(g) != NULL; }
static int bot_plays(int g, int v) { const bot *b = bot_of(g); return b && (!b->plays || b->plays(v)); }
static int any_bot_seat(int g)                  /* the picker gives the device a seat (of those that play) */
{
    for (int p = 0; p < players_of(g); p++)
        if (seat_lv[g][p]) return 1;
    return 0;
}

/* the seats the device plays in the game on the screen: those the picker gave it, when its bot plays the variant,
 * less those given back to the people (a move refused) */
static void bots_update(void)
{
    cbot = bot_of(cur);
    botmask = 0;
    if (cbot && bot_plays(cur, variant[cur]))
        for (int p = 0; p < ui.players; p++)
            if (seat_lv[cur][p] && !(bot_off[cur] >> p & 1)) botmask |= 1u << p;
    bot_seats_set(botmask);
}

unsigned ui_bot_seats(void) { return botmask; }
static int bot_moving = -1;
int ui_bot_moving(void) { return bot_moving; }

/* the first person's seat from p on, in turn order (p itself when no person plays: the bots alone) */
static int human_from(int p)
{
    for (int k = 0; k < ui.players && humans(); k++)
        if (seat_is_local_person((p + k) % ui.players)) return (p + k) % ui.players;
    return p;
}

/* one person, or nobody (a game for one, one person against the bots, the bots alone): no flip nor face to face
 * (their buttons are hidden), the person at the bottom; the settings come back for the next game of two people or
 * more */
static void seats_view(void)
{
    static int saved = -1, saved_f2f;
    int h = humans();
    if (h <= 1) {
        if (saved < 0) {
            saved = ui.flipped;
            saved_f2f = ui.f2f;
        }
        ui.flipped = ui.f2f = 0;
        if (h == 1 && ui.players > 1 && cur >= 0 && !is_view()) ui.flipped = seat_top(human_from(0));
    } else if (saved >= 0) {
        ui.flipped = saved;
        ui.f2f = saved_f2f;
        saved = -1;
    }
}

/* ---- the screens ------------------------------------------------------------------------------------------------- */
/* the picker comes before a new game when there is something to choose: a variant, how many play, a level, who plays
 * (a game with a bot) */
static int needs_picker(int g)
{
    return games[g]->variants > 1 || many_counts(g) || games[g]->levels > 0 || has_bot(g);
}

/* the picker, or a game with levels its level screen (random play: its picker, under the level screen's tabs) */
static void show_picker(int g)
{
    worker_park();                              /* game code from here on: the menu's helper waits */
    SDL_StopTextInput();                        /* a search's keyboard put away (its letters: shortcuts here) */
    frame_for(g);                               /* (the level screen's pictures) */
    if (in_random(g) && (games[g]->variants > 1 || many_counts(g))) {   /* random play's picker, under the tabs */
        chapters_make(g);
        screen = SCR_PICK;
    } else if (games[g]->levels > 0) {
        levels_enter(g);
        screen = SCR_LEVELS;
    } else {
        screen = SCR_PICK;
    }
    axes_enter(g);
    layout();
}

/* ---- the hand-off ------------------------------------------------------------------------------------------------ */
/* the view goes to `player`: covered first (VIEW_HIDDEN, while the game is on, between two people: one person
 * against the bots, or the bots alone, never see a cover), or at once */
static void view_to(int player)
{
    hand.pending = -1;
    view_of[cur] = player;
    const game *g = games[cur];
    if ((g->hidden ? g->hidden() : g->view == VIEW_HIDDEN) && g->status() == GS_PLAY && humans() > 1) {
        hand.up = 1;
        hand.player = player;
        ui.viewer = -1;
    } else {
        hand.up = 0;
        hand.player = player;
        ui.viewer = player;
    }
    if (screen == SCR_GAME) layout();
}

/* the start button: the cover lifts, the view is the player's */
static void jr_mark_uncover(void);
static void uncover(void)
{
    jr_mark_uncover();                          /* (kept in the game's record) */
    hand.up = 0;
    ui.viewer = view_of[cur] = hand.player;
    if (screen == SCR_GAME) layout();
}

void ui_handoff(int player)
{
    if (!is_view() || player < 0 || player >= ui.players) return;
    if (seat_waits_for_move(player) && humans()) return;   /* to a bot: the view stays the last person's */
    if (player == ui.viewer && !hand.up) { hand.pending = -1; return; }
    hand.pending = player;
    need_frame = 1;
}

int ui_handing(void) { return is_view() && (hand.up || hand.pending >= 0); }

/* a waiting hand-off happens once the last move has landed (the game over: none, the view stays) */
static void handoff_settle(void)
{
    if (!is_view() || hand.pending < 0) return;
    const game *g = games[cur];
    if (g->status() != GS_PLAY) { hand.pending = -1; return; }
    if (g->busy && g->busy()) { need_frame = 1; return; }
    view_to(hand.pending);
}

/* a new game: the player to move gets the view (covered: VIEW_HIDDEN), the first person after them when a bot moves
 * first; a game opened again: the cover first */
static void view_start(int fresh)
{
    hand.pending = -1;
    hand.up = 0;
    if (!is_view()) { ui.viewer = -1; return; }
    int p = fresh ? games[cur]->turn() : view_of[cur];
    view_to(human_from(p < 0 || p >= ui.players ? 0 : p));
}

/* ---- the game in progress kept (closed or killed in the background: opened again where it was) -----------------
 * Each game's record (J[g]): how it began (its variant, players, seats, the random state start() was given, the
 * screen) and then everything the game was given, in order, with the time between: the presses, motions (a finger
 * down) and releases it got, keys, fingers, passes, undos, the cover lifted, the moves played by the bots or by
 * script(), the screen turned or flipped, and the random state whenever something else drew from it between two.
 * Kept in the save store (shell/run/<id>/<k>: 4 KB pieces) at the end of the frame of an event that ends something (a
 * release, a key, a bot's move, a flip), else at most once a second (a finger sliding), and as the app leaves the
 * front, while the game is on; taken out once it is over; a new game begins a new one. No room in the store (save.c
 * gives the games in progress half of it): that game is not kept, nothing else is lost. Opened from the menu with no
 * game in progress (the app started again), a game with a record is played again from it at once, silently, on a
 * clock of its own (its animations land), its frames drawn to a small target where nothing shows (a thin bar on the
 * screen says how far it has gone), on the screen it was played on, undo history and all; then laid out for the
 * screen as it is now. Meanwhile ui_replaying() is 1 and the game's writes to the store are ignored. A hidden-hand
 * game comes back on its cover. JR_ON 0: nothing kept, nothing resumed.
 * A person's moves come in as taps, so the record replays the input. A game with history() also gets its moves'
 * notation logged beside it (EV_NOTE, see "the moves' notation" below; nothing when played again but the list rebuilt):
 * the record a game between devices will need. Version 3 (the frames, the time away); 1 and 2 are still played
 * again (drawn at each event, as they were). */
#define JR_ON 1
#define JR_PIECE 4096
#define JR_PIECES 48
#define JR_MAX (JR_PIECE * JR_PIECES)
enum { EV_PRESS = 1, EV_MOTION, EV_RELEASE, EV_FINGER, EV_KEY, EV_PASS, EV_UNDO, EV_UNCOVER, EV_SCRIPT, EV_BOT, EV_VIEW,
       EV_RNG, EV_NOTE, EV_FRAME, EV_AWAY, EV_BACK };
/* Version 3: also the frames. EV_FRAME: the last frame drawn before the next event (none: no frame between two
 * events), so the record plays again with its last frame where it was (a flag cleared in draw() the same at the next
 * tap). EV_AWAY: the game left the screen (the menu, the app in the background), EV_BACK: shown again; meanwhile
 * nothing drawn, and ui_real_ms() jumps (the game's clock waits, as it did). */
/* Version 4: EV_BACK is written at the first frame drawn after the return (or with the first event, if one comes
 * first), not at the return itself: the record plays again with nothing drawn until the game was drawn again, so a
 * clock that skips the time away skips the same time in both (played the same way as version 3). */
#define JR_VERSION 4
#define JR_EVERY 1000                           /* the record kept at most this often (ms) while a finger moves */
static struct jrec {
    uint8_t *b;
    int n, cap, ok;                             /* ok 0: too long, or not begun (nothing kept) */
    Uint32 last;                                /* ui_ms() at the last event */
    uint32_t rng;                               /* the random state after the last event */
    int w, h, flipped, f2f;                     /* the screen as last recorded */
    int kept;                                   /* the pieces in the store */
    int swept;                                  /* the store swept of a past run's pieces past the record's end */
    unsigned frames;                            /* frames_drawn at the last event */
    int away;                                   /* EV_AWAY the last of EV_AWAY / EV_BACK */
    int back;                                   /* back on the screen, its EV_BACK not yet written (at the next
                                                 * frame drawn, or the next event) */
} J[MAX_GAMES];
static int jr_quiet;                            /* 1 while a record plays again: nothing recorded */
static int jr_dirty = -1;                       /* the game whose record changed since it was last kept (-1: none) */
static int jr_soon;                             /* kept at the end of this frame (not JR_EVERY later): a release... */
static Uint32 jr_kept_at;                       /* SDL_GetTicks() when it was last kept */
static unsigned frames_drawn;                   /* the game's frames drawn (draw_game) */
static Uint32 frame_at;                         /* ui_ms() at the last one */
static Uint32 rp_real;                          /* added to the real time while a record plays again (EV_BACK) */
Uint32 ui_real_ms(void) { return SDL_GetTicks() + (rp_on ? rp_real : 0); }

static int jr_room(struct jrec *r, int n)
{
    if (!r->ok) return 0;
    if (r->n + n > JR_MAX) { r->ok = 0; return 0; }   /* too long: not kept (the store is small) */
    if (r->n + n > r->cap) {
        int c = r->cap ? r->cap * 2 : 1024;
        while (c < r->n + n) c *= 2;
        uint8_t *m = realloc(r->b, (size_t)c);
        if (!m) { r->ok = 0; return 0; }
        r->b = m;
        r->cap = c;
    }
    return 1;
}

static void jr_u8(struct jrec *r, unsigned v) { if (jr_room(r, 1)) r->b[r->n++] = (uint8_t)v; }
static void jr_u16(struct jrec *r, unsigned v) { jr_u8(r, v & 255); jr_u8(r, v >> 8 & 255); }
static void jr_u32(struct jrec *r, uint32_t v) { jr_u16(r, v & 0xffff); jr_u16(r, v >> 16); }
static void jr_var(struct jrec *r, uint32_t v)  /* 7 bits a byte, the last without its top bit */
{
    while (v >= 128) { jr_u8(r, (v & 127) | 128); v >>= 7; }
    jr_u8(r, v);
}
/* a point as the record keeps it (quarter pixels): what the game is given, live and played again alike (a tap on
 * a cell's edge lands in the same cell) */
static float jr_q(float v) { return floorf(clampf(v * 4 + 0.5f, 0, 65535)) / 4; }
static void jr_xy(struct jrec *r, float x, float y)   /* quarter pixels */
{
    jr_u16(r, (unsigned)clampf(x * 4 + 0.5f, 0, 65535));
    jr_u16(r, (unsigned)clampf(y * 4 + 0.5f, 0, 65535));
}

static void jr_key(int g, int k, char *s, size_t n) { snprintf(s, n, "shell/run/%s/%d", games[g]->id, k); }

/* the record kept in the store (the game on), or taken out of it (over, or none) */
static void jr_keep(int g)
{
    struct jrec *r = &J[g];
    char k[96];
    int on = JR_ON && r->ok && started[g] && games[g]->status() == GS_PLAY;
    int pieces = on ? (r->n + JR_PIECE - 1) / JR_PIECE : 0;
    for (int i = 0; i < pieces; i++) {
        jr_key(g, i, k, sizeof k);
        int len = r->n - i * JR_PIECE < JR_PIECE ? r->n - i * JR_PIECE : JR_PIECE;
        if (!save_try(k, r->b + i * JR_PIECE, len)) {   /* no room in the store (save.c's share for these): this
                                                         * game is not kept at all, the rest of the store is */
            fprintf(stderr, "save: no room for %s's game in progress: not kept\n", games[g]->id);
            r->ok = 0;
            pieces = 0;
            r->swept = 0;                       /* every piece taken out */
            break;
        }
    }
    /* the pieces no longer needed (one more: a past run's); once a run, every one past the end (a past run's longer
     * record, left by a new game begun instead of it) */
    int all = !r->swept;
    r->swept = 1;
    for (int i = pieces; i < JR_PIECES && (all || i < r->kept || i == pieces); i++) {
        jr_key(g, i, k, sizeof k);
        if (ui_save_get(k, NULL, 0)) ui_save_set(k, NULL, 0);
        else if (!all) break;
    }
    r->kept = pieces;
}

static void jr_flush(void)
{
    if (jr_dirty >= 0) jr_keep(jr_dirty);
    jr_dirty = -1;
    jr_soon = 0;
    jr_kept_at = SDL_GetTicks();
}

/* the end of a frame: the record kept if something ended (a release, a key, a bot's move...), else at most every
 * JR_EVERY ms (a finger sliding: not the whole store written each frame) */
static void jr_flush_due(void)
{
    if (jr_dirty >= 0 && (jr_soon || SDL_GetTicks() - jr_kept_at >= JR_EVERY)) jr_flush();
}

/* the start of an event of the game on the screen: the screen and the random state first, if they changed; NULL:
 * nothing kept */
static void jr_view_mark(struct jrec *r)       /* the screen, if it changed (turned, flipped, face to face) */
{
    if (r->w == ui.w && r->h == ui.h && r->flipped == ui.flipped && r->f2f == ui.f2f) return;
    jr_u8(r, EV_VIEW);
    jr_var(r, 0);
    jr_u16(r, (unsigned)ui.w);
    jr_u16(r, (unsigned)ui.h);
    jr_u8(r, (unsigned)(ui.flipped | ui.f2f << 1));
    r->w = ui.w;
    r->h = ui.h;
    r->flipped = ui.flipped;
    r->f2f = ui.f2f;
}

static void jr_done(struct jrec *r);
static struct jrec *jr_event(int type)
{
    if (!JR_ON || jr_quiet || cur < 0) return NULL;
    struct jrec *r = &J[cur];
    if (!r->ok) return NULL;
    if (jr_dirty >= 0 && jr_dirty != cur) jr_flush();
    if (r->back && type != EV_BACK) {          /* back, and an event before any frame: EV_BACK first */
        r->back = 0;
        jr_done(jr_event(EV_BACK));
    }
    Uint32 now = ui_ms();
    if (r->frames != frames_drawn) {            /* the last frame drawn since the last event */
        Uint32 f = frame_at - r->last <= now - r->last ? frame_at : r->last, df = f - r->last;
        jr_u8(r, EV_FRAME);
        jr_var(r, df > 3600000 ? 3600000 : df);
        r->last = f;
        r->frames = frames_drawn;
    }
    Uint32 dt = now - r->last;
    if (dt > 3600000) dt = 3600000;
    r->last = now;
    jr_view_mark(r);
    if (type != EV_PRESS && type != EV_MOTION && type != EV_FINGER) jr_soon = 1;   /* kept at the frame's end */
    if (r->rng != rng) {
        jr_u8(r, EV_RNG);
        jr_var(r, 0);
        jr_u32(r, rng);
    }
    jr_u8(r, (unsigned)type);
    jr_var(r, dt);
    jr_dirty = cur;
    return r;
}

/* an event's end: the random state it left (what draws from it next, unrecorded, is recorded as a new state); the
 * moves it made, in their notation (notes_check) */
static void notes_check(void);
static void jr_done(struct jrec *r)
{
    if (!r) return;
    r->rng = rng;
    notes_check();
}

static struct jrec *jr_point(int type, float x, float y)
{
    struct jrec *r = jr_event(type);
    if (r) jr_xy(r, x, y);
    return r;
}

static struct jrec *jr_move(int type, int seat, const char *move)
{
    struct jrec *r = jr_event(type);
    if (!r) return NULL;
    size_t n = strlen(move);
    if (n > 65535) { r->ok = 0; return NULL; }
    jr_u8(r, (unsigned)(seat & 255));
    jr_u16(r, (unsigned)n);
    for (size_t i = 0; i < n; i++) jr_u8(r, (uint8_t)move[i]);
    return r;
}

/* a new game of the open one begins its record (its start: start_rng[cur], variant v) */
static void jr_begin(int v, int card)
{
    if (!JR_ON || jr_quiet) return;
    struct jrec *r = &J[cur];
    r->n = 0;
    r->ok = 1;
    jr_u8(r, 'R');
    jr_u8(r, 'J');
    jr_u8(r, JR_VERSION);                       /* the record's version (2: with the moves' notation; 3: frames) */
    jr_u16(r, (unsigned)v);
    jr_u8(r, (unsigned)in_random(cur));
    jr_u8(r, (unsigned)ui.players);
    for (int p = 0; p < MAX_PLAYERS; p++) jr_u8(r, seat_lv[cur][p]);
    jr_u32(r, start_rng[cur]);
    jr_u16(r, (unsigned)ui.w);
    jr_u16(r, (unsigned)ui.h);
    jr_u8(r, (unsigned)(ui.flipped | ui.f2f << 1 | (card ? 4 : 0)));   /* 4: begun from the end card (ui_over) */
    r->w = ui.w;
    r->h = ui.h;
    r->flipped = ui.flipped;
    r->f2f = ui.f2f;
    r->last = ui_ms();
    r->rng = rng;
    r->frames = frames_drawn;
    r->away = 0;
    r->back = 0;
    jr_dirty = cur;
    jr_soon = 1;
}
#define JR_HEAD (7 + MAX_PLAYERS + 9)            /* the record's head, bytes */
static void jr_mark_uncover(void) { jr_done(jr_event(EV_UNCOVER)); }

/* the board flipped, face to face turned: kept at once (not with the next event: a game closed right after it) */
static void jr_view(void)
{
    if (!JR_ON || jr_quiet || cur < 0 || !J[cur].ok || !started[cur]) return;
    if (jr_dirty >= 0 && jr_dirty != cur) jr_flush();
    jr_view_mark(&J[cur]);
    jr_dirty = cur;
    jr_soon = 1;
}

/* the game off the screen (away 1: the menu, the picker, the app in the background), or on it again (0) */
static void jr_away(int away)
{
    if (!JR_ON || jr_quiet || cur < 0 || !J[cur].ok || !started[cur] || J[cur].away == away) return;
    J[cur].away = away;
    if (!away) { J[cur].back = 1; return; }     /* (written as the game is drawn again: draw_game) */
    if (J[cur].back) { J[cur].back = 0; return; }   /* (away again before a frame: as if it never came back) */
    jr_done(jr_event(EV_AWAY));
}

/* ---- the moves' notation (game.history) ---------------------------------------------------------------------------
 * The one path a move will take between devices: the game's notation. A person's move reaches the shell as taps, a
 * bot's and script()'s as notation; a game with history() gives every move's notation, whoever made it. After each
 * event of the record and each frame of the game, the shell compares the game's moves with the list it keeps for it
 * (NT[g]): the moves undone are taken off, the new ones added, and the change is logged in the record (EV_NOTE: how
 * many of the list stay, how many come, each one's notation; no time of its own, nothing played when the record is
 * played again but the list rebuilt). Only the count is asked while nothing moves; the last move kept is compared
 * too (an undo and a new move together). Nothing else reads the list yet (--notation prints it): the moves a game
 * between devices will send, the record it will keep. */
#define NOTE_CAP BOT_MOVE_CAP
static struct notes {
    char *s;                                    /* the moves' notation, each NUL-terminated, one after the other */
    int *at;                                    /* move i's start in s */
    int n, len, cap, cap_at;
} NT[MAX_GAMES];

static void nt_cut(struct notes *t, int keep)   /* the first `keep` moves stay */
{
    if (keep >= t->n) return;
    t->n = keep < 0 ? 0 : keep;
    t->len = t->n ? t->at[t->n - 1] + (int)strlen(t->s + t->at[t->n - 1]) + 1 : 0;
}

static void nt_add(struct notes *t, const char *m)
{
    int k = (int)strlen(m) + 1;
    if (t->len + k > t->cap) {
        int c = t->cap ? t->cap * 2 : 1024;
        while (c < t->len + k) c *= 2;
        char *s = realloc(t->s, (size_t)c);
        if (!s) return;
        t->s = s;
        t->cap = c;
    }
    if (t->n == t->cap_at) {
        int c = t->cap_at ? t->cap_at * 2 : 256;
        int *a = realloc(t->at, (size_t)c * sizeof *a);
        if (!a) return;
        t->at = a;
        t->cap_at = c;
    }
    memcpy(t->s + t->len, m, (size_t)k);
    t->at[t->n++] = t->len;
    t->len += k;
}

static void nt_move(const game *g, int i, char *m)   /* the game's move i ("": forgotten) */
{
    m[0] = 0;
    g->history(i, m, NOTE_CAP);
    m[NOTE_CAP - 1] = 0;
}

static void notes_check(void)
{
    if (cur < 0 || jr_quiet || !started[cur] || !games[cur]->history) return;
    const game *g = games[cur];
    struct notes *t = &NT[cur];
    int n = g->history(-1, NULL, 0);
    if (n < 0) n = 0;
    int keep = n < t->n ? n : t->n;
    char m[NOTE_CAP];
    if (keep > 0 && keep == t->n) {             /* the last move kept: still the same? */
        nt_move(g, keep - 1, m);
        if (m[0] && strcmp(m, t->s + t->at[keep - 1])) keep--;
    }
    if (keep == t->n && n == t->n) return;
    nt_cut(t, keep);
    struct jrec *r = JR_ON && J[cur].ok ? &J[cur] : NULL;
    if (r) {
        if (jr_dirty >= 0 && jr_dirty != cur) jr_flush();
        jr_u8(r, EV_NOTE);
        jr_var(r, 0);                           /* (no time: the event before it made the moves) */
        jr_var(r, (uint32_t)keep);
        jr_var(r, (uint32_t)(n - keep));
        jr_dirty = cur;
    }
    for (int i = keep; i < n; i++) {
        nt_move(g, i, m);
        nt_add(t, m);
        if (!r) continue;
        int k = (int)strlen(m);
        jr_u8(r, (unsigned)k);
        for (int j = 0; j < k; j++) jr_u8(r, (uint8_t)m[j]);
    }
}

/* ---- the bots at play (bots.h's integration plan) -----------------------------------------------------------------
 * Each frame (bot_step): a bot starts when none thinks, the game is on (GS_PLAY), not busy(), no hand-off waits, the
 * rules page is shut and the seat that must act (its bot's actor(), else turn()) is the device's; the worker thinks
 * while the screen keeps drawing; its move is played (the bot's play(), else script()) once it has thought at least
 * BOT_PAUSE ms (BOT_FOLLOW for its next action in the same turn; BOT_WATCH with no person playing: the bots alone
 * are paced for watching; a real-time bot's at once).
 * An empty or refused move: logged, and the seat goes to the people for the rest of the game (never a loop). Undo, a
 * new game, leaving and quitting stop the worker first (bot_stop); the rules page only holds the move until it
 * closes. With no person playing (the bots alone), a game that waits for a tap with nobody to act (a round's score
 * sheet: actor() -1) gets one after WATCH_TAP ms, in the middle of its table, as a person's tap would come. */
#define BOT_PAUSE 600
#define BOT_WATCH 900
#define WATCH_TAP 2500
#define BOT_FOLLOW 350                          /* the same bot again, nobody between: one turn's next action */
static struct {
    int seat, held, played, taps;               /* the seat thinking; a move ready, held for its pause; moves played;
                                                 * taps given in watch mode */
    unsigned ms;                                /* how long the move took (the picture mode's log) */
    Uint32 idle;                                /* watch mode: since when nobody has had anything to do (0: not) */
    char move[BOT_MOVE_CAP];
} BT = {-1, 0, 0, 0, 0, 0, ""};
static int bot_last = -1;                       /* the seat whose bot played last, no person since (-1: none) */
static int held_press;                          /* a finger put down while a bot's turn held the input: its motion
                                                 * and its release are not the game's either */
static void after_input(void);
static void press(float x, float y);
static void release(float x, float y);

/* a real-time game (a race: slaps): its bot's flag, or a game played with several fingers at once */
static int realtime(void) { return cbot && (cbot->realtime || games[cur]->finger); }

/* the seat that must act now (-1: nobody) */
static int bot_actor(void) { return cbot && cbot->actor ? cbot->actor() : games[cur]->turn(); }

/* the worker stopped (cancelled, joined) and its move forgotten */
static void bot_stop(void)
{
    bot_halt();
    BT.held = 0;
    BT.seat = -1;
    bot_last = -1;
}

/* the game's own input held: a bot's turn (not in a race), the game on */
static int bot_holds(void)
{
    if (!elsewhere() || cur < 0 || screen != SCR_GAME || realtime() || games[cur]->status() != GS_PLAY) return 0;
    return seat_waits_for_move(bot_actor()) || bot_thinking() || BT.held;
}

/* the seat's bot could not move: the people play that seat for the rest of the game */
static void bot_gives_up(int seat, const char *why)
{
    fprintf(stderr, "bots: %s, seat %d: %s \"%s\"; the seat goes to the people\n", games[cur]->id, seat, why, BT.move);
    bot_off[cur] |= 1u << seat;
    bots_update();
    seats_view();
    layout();
}

static void bot_step(void)
{
    if (screen != SCR_GAME || cur < 0 || !botmask) {
        if (bot_thinking() || BT.held) bot_stop();
        return;
    }
    const game *g = games[cur];
    if (bot_thinking()) {
        need_frame = 1;                         /* the thinking sign moves */
        if (!bot_done(BT.move, sizeof BT.move)) return;
        BT.held = 1;
        BT.ms = bot_ms();
    }
    if (BT.held) {
        need_frame = 1;
        unsigned pause = realtime() || instant ? 0 : BT.seat == bot_last ? (humans() ? BOT_FOLLOW : BOT_PAUSE)
                         : humans() ? BOT_PAUSE : BOT_WATCH;    /* a follow-up in the same turn: quicker */
        if (help.open || bot_ms() < pause) return;
        int seat = BT.seat;
        BT.held = 0;
        BT.seat = -1;
        if (g->status() != GS_PLAY || bot_actor() != seat) return;   /* stale (a person acted first: a race) */
        if (!BT.move[0]) {
            bot_gives_up(seat, "no move");
            return;
        }
        bot_moving = seat;
        struct jrec *jr = jr_move(EV_BOT, seat, BT.move);
        int ok = cbot->play ? cbot->play(BT.move) : g->script ? g->script(BT.move) : 0;
        jr_done(jr);
        bot_moving = -1;
        if (!ok) {
            bot_gives_up(seat, "move refused");
            return;
        }
        BT.played++;
        bot_last = seat;
        if (instant) fprintf(stderr, "bot %d: %s (%u ms)\n", seat, BT.move, BT.ms);
        after_input();
        return;
    }
    int a = help.open || g->status() != GS_PLAY || (g->busy && g->busy()) || ui_handing() ? -2 : bot_actor();
    if (a == -1 && !humans()) {                 /* the bots alone, nobody to act: the tap a person would give */
        Uint32 now = SDL_GetTicks();
        if (!BT.idle) BT.idle = now ? now : 1;
        need_frame = 1;
        if (!instant && now - BT.idle < WATCH_TAP) return;
        BT.idle = 0;
        BT.taps++;
        if (instant) fprintf(stderr, "watch: a tap (nobody to act)\n");
        press(L.board.x + L.board.w / 2, L.board.y + L.board.h / 2);
        release(L.board.x + L.board.w / 2, L.board.y + L.board.h / 2);
        after_input();
        return;
    }
    BT.idle = 0;
    if (!is_bot(a)) return;
    uint64_t seed = (uint64_t)ui_random(1 << 30) << 34 ^ (uint64_t)ui_random(1 << 30) << 4 ^ (uint64_t)ui_random(16);
    ai_set_seed(seed);
    BT.move[0] = 0;
    if (!bot_begin(cbot, a, seat_lv[cur][a] - 1)) {
        bot_gives_up(a, "no worker");
        return;
    }
    BT.seat = a;
    need_frame = 1;
}

/* undo against the bots: back to the last person's move (the bots' replies taken back with it); the bots alone: one
 * move */
static int undo_human(void)
{
    const game *g = games[cur];
    bot_stop();
    if (!g->undo) return 0;                     /* (a game without undo) */
    struct jrec *jr = jr_event(EV_UNDO);
    int done = g->undo();
    for (int k = 0; done && k < 256 && elsewhere() && humans() && g->status() == GS_PLAY &&
                    seat_waits_for_move(bot_actor()); k++)
        if (!g->undo()) break;
    jr_done(jr);
    return done;
}

/* the end of a game shown (once a game: an undo and the same end again gives nothing more): the stats (stats.h).
 * One player: a puzzle solved (a level's XP: level_check, as its stars improve; at random: its stars), or failed; more:
 * a person won (against the device: the strongest bot's level more XP). The device alone: nobody's game. */
static uint8_t awarded[MAX_GAMES];
static void game_finished(void)
{
    const game *g = games[cur];
    if (awarded[cur] || !started[cur] || humans() == 0) return;
    awarded[cur] = 1;
    int st = g->status(), sec = section_of(g);
    if (ui.players == 1) {
        uint32_t xp = 0;
        if (st == GS_WIN0 && !(g->levels && !in_random(cur))) {
            int k = g->level_stars ? g->level_stars() : 0;
            xp = (uint32_t)(STATS_XP_LEVEL + STATS_XP_STAR * (k < 0 ? 0 : k > 3 ? 3 : k));
        }
        stats_finish(sec, g->id, xp, st == GS_WIN0, 0);
        return;
    }
    unsigned people = ((1u << ui.players) - 1) & ~elsewhere();
    int won = (winners_of(st) & people) != 0, lv = 0;
    for (int p = 0; p < ui.players; p++)
        if (is_bot(p) && seat_lv[cur][p] > lv) lv = seat_lv[cur][p];   /* 1 + the bot's level */
    uint32_t xp = (uint32_t)(STATS_XP_GAME + (won && lv ? STATS_XP_BOT * lv : 0));
    stats_finish(sec, g->id, xp, won, won && lv == 1 + AI_HARD ? STATS_BEAT_HARD : 0);
}

/* left (back, home) or opened again with its end not shown yet (it came during the last move's animation): counted all
 * the same */
static void finished_unseen(void)
{
    if (cur >= 0 && started[cur] && games[cur]->status() != GS_PLAY) game_finished();
}

/* a new game of the open one: start(v), given a fresh random state (kept), or the one it was given last (same: a
 * replay, the same deal or puzzle) */
static void start_game(int v, int same)
{
    bot_stop();
    bot_off[cur] = 0;                           /* every seat as the picker gave it */
    bots_update();
    seats_view();
    if (same) rng = start_rng[cur];
    else start_rng[cur] = rng;
    int card = ui_over();                       /* (the end card's next or replay: what start() sees, kept) */
    ui.level = games[cur]->levels && !in_random(cur) ? v : -1;
    dice.n = 0;                                 /* no dice of the last game (still rolling: ui_dice_busy) */
    if (in_random(cur)) games[cur]->start_random(v);
    else games[cur]->start(v);
    started[cur] = 1;
    awarded[cur] = 0;
    card_t0 = 0;
    nt_cut(&NT[cur], 0);                        /* (its moves' notation: none yet) */
    jr_begin(v, card);
}

/* the game's kept record looked at again soon (a game left, a new one) */
static void jr_touch(int g)
{
    if (jr_dirty >= 0 && jr_dirty != g) jr_flush();
    jr_dirty = g;
    jr_soon = 1;
}
static int jr_resume(int g);

static void open_game(int i, int fresh_variant)
{
    worker_park();
    SDL_StopTextInput();                        /* (a search's keyboard) */
    if (cur != i) bot_stop();
    cur = i;
    frame_for(cur);
    ui.players = players_of(i);
    bots_update();
    seats_view();
    int fresh = 0;
    if (fresh_variant >= 0) {
        if (in_random(i)) rvariant[i] = fresh_variant;
        else variant[i] = fresh_variant;
        chosen[i] = 1;
        remember_game(i);
        start_game(fresh_variant, 0);
        fresh = 1;
    } else if (!started[i] && jr_resume(i)) {   /* kept from the last run: played again to where it was */
        fresh = 0;
    } else if (!started[i]) {
        if (needs_picker(i)) {
            show_picker(i);
            return;
        }
        start_game(0, 0);
        fresh = 1;
    }
    view_start(fresh);
    if (!fresh) jr_away(0);                     /* (on the screen again: kept) */
    screen = SCR_GAME;
    over_hidden = 0;
    armed = 0;
    layout();
    last_status = games[cur]->status();
    end_seen = last_status != GS_PLAY;          /* a game that had ended: its card at once, no tune again */
    finished_unseen();
}

/* back from a game: its new game screen (the picker, the level screen), the game in progress kept (the picker's gold
 * play button, or the menu's card, picks it up) */
static void game_back(void)
{
    bot_stop();
    finished_unseen();
    jr_away(1);                                 /* (its record: the game off the screen) */
    if (is_view() && hand.pending >= 0) view_of[cur] = hand.pending;   /* the next player's, when it comes back */
    hand.pending = -1;
    show_picker(cur);
}

/* a game in progress to go back to (the picker's and the level screen's gold play button) */
static int resumable(void) { return cur >= 0 && started[cur] && games[cur]->status() == GS_PLAY; }

static void go_home(void)
{
    bot_stop();                                 /* a bot thinking: it starts again when the game is opened again */
    finished_unseen();
    jr_away(1);
    if (is_view() && hand.pending >= 0) view_of[cur] = hand.pending;   /* the next player's, when it comes back */
    hand.pending = -1;
    screen = SCR_MENU;
    worker_unpark();                            /* the menu: its pictures painted by the helper again */
    layout();
}

/* the game started again at once (its variant, its level): a new deal, or the same one (same: replay) */
static void restart(int same)
{
    armed = 0;
    start_game(in_random(cur) ? rvariant[cur] : variant[cur], same);
    view_start(1);
    over_hidden = 0;
    last_status = games[cur]->status();
    end_seen = 0;
}

static void new_game(void)
{
    armed = 0;
    bot_stop();
    if (needs_picker(cur)) {
        started[cur] = 0;
        jr_touch(cur);                          /* (the game left: its record too) */
        show_picker(cur);
        return;
    }
    restart(0);
}

/* a solved level's next button: the next level, or the level screen after the last one */
static void level_next(void)
{
    int l = variant[cur] + 1;
    if (l < nlevels(cur) && level_open(cur, l)) open_game(cur, l);
    else {
        new_game();
        if (l < nlevels(cur) && level_locked(l)) full_open();   /* the free edition's last level: the full version */
    }
}

/* the solved puzzle's card: replay (the same puzzle, the same level), next (the next level; a new puzzle of the same
 * variant) */
static void card_replay(void) { restart(1); }
static void card_next(void)
{
    if (games[cur]->levels && !in_random(cur)) level_next();
    else restart(0);
}

/* after anything a player did: a new end of the game (or none any more) is noticed; draw_game() shows it. A race (a
 * real-time game): a bot's move made stale by a person's (the seat to act has changed) is dropped, and it thinks
 * again. */
static void after_input(void)
{
    if (screen != SCR_GAME) return;
    level_check();
    int s = games[cur]->status();
    if (s != last_status) { over_hidden = 0; end_seen = 0; card_t0 = 0; }
    last_status = s;
    if (BT.seat >= 0 && realtime() && (s != GS_PLAY || bot_actor() != BT.seat)) bot_stop();
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void draw_button(float x, float y, float r, int icon, SDL_Texture *disc)
{
    if (LIGHT_UI) ui_shadow_flat(x, y, 2.08f * r, 2.08f * r, 0.32f);   /* white on ivory: lifted */
    ui_put_c(disc ? disc : t_btn, x, y, 2 * r, 2 * r);
    SDL_Texture *t = icon_tex(icon);
    ink_mod(t);
    ui_put_c(t, x, y, 1.24f * r, 1.24f * r);
}

/* the pass button sits at the right end of the bar (in the bar's own frame: it turns with it) */
static int shows_pass(int p)
{
    const game *g = games[cur];
    return g->can_pass && g->status() == GS_PLAY && g->turn() == p && seat_is_local_person(p) && g->can_pass();
}

/* a bot's seat in its bar: a small robot on a dark disc at the token's lower right; while it thinks, three dots on
 * the disc instead, rising one after the other */
static void bot_badge(float tx, float cy, float tk, int thinking)
{
    static const uint8_t disc_c[4] = {28, 34, 32, 235}, dot_c[4] = {246, 238, 220, 255};
    float d = floorf(0.62f * tk), bx = tx + 0.4f * tk, by = cy + 0.3f * tk;
    SDL_Texture *disc = rounded((int)d, (int)d, d / 2, disc_c);
    ui_put_c(disc, bx, by, d, d);
    if (!thinking) {
        SDL_Texture *t = btn_tex(paint_btn_icon, ICON_BOT, (int)(0.78f * d));
        if (!t) return;
        SDL_SetTextureColorMod(t, BTN_CREAM);
        ui_put_c(t, bx, by - 0.02f * d, 0.78f * d, 0.78f * d);
        SDL_SetTextureColorMod(t, 255, 255, 255);
        return;
    }
    float dd = fmaxf(2, floorf(0.17f * d)), t = (float)(ui_ms() % 1200) / 1200;
    SDL_Texture *dot = rounded((int)dd, (int)dd, dd / 2, dot_c);
    for (int k = 0; k < 3; k++) {
        float ph = t * 3 - k * 0.5f, lift = ph > 0 && ph < 1 ? sinf(ph * ART_PI) : 0;
        ui_put_c(dot, bx + (k - 1) * 0.27f * d, by - lift * 0.16f * d, dd, dd);
    }
    ui_animate();
}

static void bar_content(int p, box b)
{
    const game *g = games[cur];
    int st = g->status(), turn = st == GS_PLAY && g->turn() == p;
    int won = (ui.players > 1 && (winners_of(st) >> p & 1)) || (ui.players == 1 && st == GS_WIN0);
    int narrow = b.w < 1.9f * ui.unit;          /* five bars side by side: closer, the token smaller */
    float tk = narrow ? 0.6f * ui.unit : L.tk;
    float cy = b.y + b.h / 2, tx = b.x + (narrow ? 0.04f : 0.15f) * ui.unit + tk / 2;
    int thinking = is_bot(p) && BT.seat == p && (bot_thinking() || BT.held);
    if (turn || won || thinking) {              /* the gold light: whose turn it is, or who won; a bot thinking:
                                                 * breathing */
        float k = turn || thinking ? 1.8f : 2.3f, a = 1;
        if (thinking) a = 0.55f + 0.45f * cosf((float)(ui_ms() % 1400) / 1400 * 2 * ART_PI);
        SDL_SetTextureAlphaMod(t_gold, (Uint8)(255 * a));
        ui_put_c(t_gold, tx, cy, tk * k, tk * k);
        SDL_SetTextureAlphaMod(t_gold, 255);
    }
    ui_put_c(t_tok[p], tx, cy, tk, tk);
    if (is_bot(p)) bot_badge(tx, cy, tk, thinking);
    float right = b.x + b.w;
    if (shows_pass(p)) {
        float r = 0.42f * ui.unit;
        draw_button(right - r - 0.05f * ui.unit, cy, r, ICON_PASS, NULL);
        right -= 2 * r + 0.25f * ui.unit;
    }
    if (g->bar) {
        float x = tx + tk / 2 + (narrow ? 0.08f : 0.3f) * ui.unit;
        in_bar = 1;                             /* (its light digits: dark on the light theme's table) */
        g->bar(p, (box){x, b.y, right - x, b.h});
        in_bar = 0;
    }
}

static void draw_bar(int p)
{
    box b = L.bar[p];
    if (!ui_upside(p) || !t_bartex) {
        bar_content(p, b);
        return;
    }
    SDL_SetRenderTarget(ui.r, t_bartex);         /* face to face: drawn in its own frame, then turned round */
    SDL_SetRenderDrawColor(ui.r, 0, 0, 0, 0);
    SDL_RenderClear(ui.r);
    bar_content(p, (box){(float)bar_m, (float)bar_m, b.w, b.h});
    SDL_SetRenderTarget(ui.r, rp_sink);         /* (the screen; a record played again: its sink) */
    SDL_Rect src = {0, 0, (int)b.w + 2 * bar_m, (int)b.h + 2 * bar_m};   /* this bar's part (bars may share a side) */
    SDL_FRect d = {b.x + b.w / 2 - src.w / 2.0f, b.y + b.h / 2 - src.h / 2.0f, (float)src.w, (float)src.h};
    SDL_RenderCopyExF(ui.r, t_bartex, &src, &d, 180, NULL, SDL_FLIP_NONE);
}

box ui_bar_box(int player, int *upside)
{
    int ok = cur >= 0 && player >= 0 && player < ui.players;
    if (upside) *upside = ok && ui_upside(player) && t_bartex;
    return ok ? L.bar[player] : (box){0, 0, 0, 0};
}

/* the players who share the win (the end card, the bars): winners() when the game has it, else gs_winner(); 0: none
 * (playing, or a draw without winners()) */
static unsigned winners_of(int st)
{
    if (st == GS_PLAY) return 0;
    const game *g = games[cur];
    unsigned m = g->winners ? g->winners() & ((1u << ui.players) - 1) : 0;
    if (m) return m;
    int w = gs_winner(st);
    return w >= 0 ? 1u << w : 0;
}

/* a player who lost, on the end card: their token upright (a glyph still reads), set a little lower, dimmed */
static void put_lost(SDL_Texture *t, float cx, float cy, float s)
{
    SDL_SetTextureColorMod(t, 150, 150, 150);
    SDL_SetTextureAlphaMod(t, 210);
    ui_put_c(t, cx, cy + 0.12f * s, s, s);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

/* the end card with winners(): the winners standing side by side (in gold light, unless it is a draw: then the tied
 * players only), the others lower, dimmed */
static void draw_winners(unsigned won, int draw, float cx, float cy, float u)
{
    int nw = 0, nl = 0;
    for (int p = 0; p < ui.players; p++) nw += won >> p & 1;
    nl = ui.players - nw;
    float wx = nl ? cx - 1.25f * u : cx, ww = nl ? 3.9f * u : 6.2f * u, s = fminf(2.1f * u, ww / nw / 1.05f);
    for (int p = 0, k = 0; p < ui.players; p++) {
        if (!(won >> p & 1)) continue;
        float x = wx + (k++ - (nw - 1) / 2.0f) * s * 1.05f;
        if (!draw) ui_put_c(t_gold, x, cy, s * 1.3f, s * 1.3f);
        ui_put_c(big_tok(p), x, cy, s, s);
    }
    if (!nl) return;
    int rows = nl > 2 ? 2 : 1, per = (nl + rows - 1) / rows;
    float ls = fminf(1.3f * u, 2.3f * u / per), lx = cx + 2.1f * u;
    for (int p = 0, k = 0; p < ui.players; p++) {
        if (won >> p & 1) continue;
        int r = k / per, c = k % per;
        put_lost(big_tok(p), lx + (c - (per - 1) / 2.0f) * ls * 1.1f, cy + (r - (rows - 1) / 2.0f) * ls * 1.15f, ls);
        k++;
    }
}

/* a solved puzzle's card (a game of levels, or one player with stars): its stars popping in one by one (a game of
 * levels without stars: the player's token in gold light), the moves made beside the fewest possible (score(),
 * par(): digits, the par after a flag), then two buttons: replay (the same puzzle) and next in gold light (the next
 * level, after the last one the level screen; a new puzzle of the same variant), as big as the shell's buttons. u: the
 * card's own unit (SOLVED_W of them wide). */
static void draw_solved(float cx, float cy, float u)
{
    const game *g = games[cur];
    int row = g->score != NULL;
    float top = cy - (row ? 0.95f : 0.5f) * u, by = cy + (row ? 1.2f : 0.8f) * u;
    if (g->level_stars) {
        int s = g->level_stars();
        s = s < 0 ? 0 : s > 3 ? 3 : s;
        if (!card_t0) {                         /* first shown: the stars to come, each with a soft knock */
            card_t0 = instant ? ui_ms() - 100000 : ui_ms();
            for (int k = 0; k < s && !instant; k++) snd_play_at(SND_PLACE, 0.6f, 150 + 170 * k);
        }
        float t = (float)(ui_ms() - card_t0);
        for (int k = 0; k < 3; k++) {
            float x = cx + (k - 1) * 1.2f * u, y = top - (k == 1 ? 0.14f * u : 0), d = (k == 1 ? 1.1f : 0.92f) * u;
            star_at(x, y, d, TH.dark ? 2 : 3);  /* (the light theme's white card: a soft beige hollow) */
            if (k >= s) continue;
            float e = (t - 150 - 170 * k) / 240;   /* 0..1: it swells past its size, then settles */
            if (e <= 0) {
                ui_animate();
                continue;
            }
            float grow = e >= 1 ? 1 : e < 0.7f ? 1.2f * e / 0.7f : 1.2f - 0.2f * (e - 0.7f) / 0.3f;
            if (e < 1) ui_animate();
            star_at(x, y, d * grow, 0);
        }
    } else {
        ui_put_c(t_gold, cx, top + 0.1f * u, 2.1f * u, 2.1f * u);
        ui_put_c(big_tok(0), cx, top + 0.1f * u, 1.35f * u, 1.35f * u);
    }
    if (row) {                                  /* the moves; the par after its flag */
        int m = g->score(), p = g->par ? g->par() : 0;
        float h = 0.72f * u, y = cy + 0.17f * u, fd = 0.86f * u, gap = 0.45f * u;
        m = m < 0 ? 0 : m;
        float mw = ui_number_w(m, h), pw = p > 0 ? ui_number_w(p, h) : 0;
        float w = mw + (p > 0 ? gap + fd + 0.06f * u + pw : 0), x = cx - w / 2;
        ui_number(x + mw / 2, y, h, m, 1);
        if (p > 0) {
            SDL_Texture *flag = btn_tex(paint_btn_icon, ICON_PAR, (int)(fd + 0.5f));
            x += mw + gap;
            if (flag) {
                if (TH.dark) SDL_SetTextureColorMod(flag, 255, 200, 80);
                else SDL_SetTextureColorMod(flag, 196, 140, 40);   /* (a deeper gold on the white) */
                ui_put_c(flag, x + fd / 2, y, fd, fd);
                SDL_SetTextureColorMod(flag, 255, 255, 255);
            }
            ui_number_ex(x + fd + 0.06f * u + pw / 2, y, h, p, NUM_LIGHT, 170, 0);
        }
    }
    replay_btn.x = cx - 1.0f * u;
    next_btn.x = cx + 1.0f * u;
    replay_btn.y = next_btn.y = by;
    replay_btn.d = next_btn.d = fminf(1.1f * u, 2.1f * L.br);   /* the shell's buttons' size (a small card: less) */
    int last = g->levels && !in_random(cur) && variant[cur] + 1 >= nlevels(cur);
    ui_button(replay_btn.x, replay_btn.y, replay_btn.d, ICON_REPLAY, 0);
    ui_button(next_btn.x, next_btn.y, next_btn.d, last ? ICON_GRID : ICON_ARROW, UI_LIT);
}

/* the end card's result (its winners, a solved puzzle's stars): centred on (cx, cy), in a card 6.8 x 3.8 u wide */
static void draw_result(float cx, float cy, float u)
{
    const game *g = games[cur];
    int st = g->status();
    float big = 2.4f * u;
    if (solved_card()) {                        /* a puzzle solved */
        draw_solved(cx, cy, u);
        return;
    }
    int np = ui.players;
    unsigned won = g->winners ? winners_of(st) : 0;
    if (np > 1 && won) {                       /* winners() (partners, a shared win, a tie): they stand together */
        draw_winners(won, st == GS_DRAW, cx, cy, u);
    } else if (np == 1 && g->score) {           /* alone, a score: the token on the left, the score in big digits */
        float tx = cx - 1.55f * u;
        if (st == GS_WIN0) {
            ui_put_c(t_gold, tx, cy, big * 1.15f, big * 1.15f);
            ui_put_c(big_tok(0), tx, cy, 2.1f * u, 2.1f * u);
        } else {
            put_lost(big_tok(0), tx, cy + 0.1f * u, 1.8f * u);
        }
        int sc = g->score();
        float h = 1.25f * u, w = ui_number_w(sc < 0 ? -sc : sc, h) + (sc < 0 ? 0.56f * h : 0);
        if (w > 3.2f * u) h *= 3.2f * u / w;    /* a long number: smaller, within the card */
        ui_number_signed(cx + 1.3f * u, cy, h, sc, 1, 0);
    } else if (np == 1) {                              /* alone: in gold light when it won, else lower, dimmed */
        if (st == GS_WIN0) ui_put_c(t_gold, cx, cy, big * 1.25f, big * 1.25f);
        if (st == GS_WIN0) ui_put_c(big_tok(0), cx, cy, big, big);
        else put_lost(big_tok(0), cx, cy + 0.1f * u, 2.0f * u);
    } else if (st == GS_DRAW && np > 2) {      /* everyone standing in a row */
        float k = fminf(2.0f * u, 6.0f * u / np);
        for (int p = 0; p < np; p++) ui_put_c(big_tok(p), cx + (p - (np - 1) / 2.0f) * k * 1.05f, cy, k, k);
    } else if (np > 2) {                        /* the winner in gold light, the others lower, smaller, dimmed */
        int w = gs_winner(st), k = 0;
        ui_put_c(t_gold, cx - 1.6f * u, cy, big * 1.25f, big * 1.25f);
        ui_put_c(big_tok(w), cx - 1.6f * u, cy, big, big);
        int rows = np > 3 ? 2 : 1, per = (np - 1 + rows - 1) / rows;
        float s = fminf(1.3f * u, 2.9f * u / (per * 1.1f)), x0 = cx + 0.15f * u + (3.0f * u - per * s * 1.1f) / 2;
        for (int p = 0; p < np; p++) {         /* in the card's right part */
            if (p == w) continue;
            int r = k / per, c = k % per;
            put_lost(big_tok(p), x0 + (c + 0.5f) * s * 1.1f, cy + (r - (rows - 1) / 2.0f) * s * 1.15f, s);
            k++;
        }
    } else if (st == GS_DRAW) {                 /* both standing, an equals sign between them */
        float k = 2.0f * u;
        ui_put_c(big_tok(0), cx - 1.7f * u, cy, k, k);
        ui_put_c(big_tok(1), cx + 1.7f * u, cy, k, k);
        const uint8_t *c = TH.dark ? NUM_LIGHT : INK;
        ui_fill(cx - 0.32f * u, cy - 0.22f * u, 0.64f * u, 0.13f * u, c[0], c[1], c[2], 255);
        ui_fill(cx - 0.32f * u, cy + 0.09f * u, 0.64f * u, 0.13f * u, c[0], c[1], c[2], 255);
    } else {                                    /* the winner in gold light, the loser lower, dimmed */
        int w = gs_winner(st) == 1;
        ui_put_c(t_gold, cx - 1.0f * u, cy, big * 1.25f, big * 1.25f);
        ui_put_c(big_tok(w), cx - 1.0f * u, cy, big, big);
        put_lost(big_tok(!w), cx + 1.6f * u, cy + 0.12f * u, 1.6f * u);
    }
}

/* the end card grown round a game's final table (ui_end_room): this frame's (room 1: drawn, the result goes at hx, hy
 * in a card k times the usual size) */
static struct { int due, room; float hx, hy, k; box r; } E;
static SDL_Texture *t_endcard;
static int endcard_w, endcard_h;

int ui_over(void) { return rp_over || (screen == SCR_GAME && cur >= 0 && E.due); }

/* the table veiled under the end card: the board's box in black (the dark theme, as it always was); on the light
 * theme's ivory a grey box looked like a hole: the whole screen in a soft warm shade */
static void end_veil(box b, Uint8 a)
{
    if (TH.dark) ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, a);
    else ui_fill(0, 0, (float)ui.w, (float)ui.h, 120, 92, 56, (Uint8)(a / 2));
}

/* the game's table in the card: below the result (portrait, a tall table), or beside it on its right (a wide table
 * with room to spare: landscape) */
box ui_end_room(float w, float h)
{
    if (!ui_over()) return (box){0, 0, 0, 0};
    if (E.room) return E.r;                     /* asked again this frame: the same room */
    box b = L.board;
    float u = ui.unit, pad = floorf(0.3f * u), cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    float ks = 0.72f, hs = 6.8f * u * ks;       /* beside: the result's part, this wide */
    int side = b.w > 1.3f * b.h && w + hs + 3 * pad <= b.w * 0.98f;
    float k = side ? ks : 0.62f, hw = 6.8f * u * k, hh = 3.8f * u * k;
    w = floorf(fminf(w, b.w * 0.98f - 2 * pad - (side ? hw + pad : 0)));
    h = floorf(fminf(h, b.h * 0.98f - 2 * pad - (side ? 0 : hh)));
    float cw, ch;
    if (side) {
        cw = hw + w + 3 * pad;
        ch = fmaxf(h, hh) + 2 * pad;
    } else {
        cw = fmaxf(w, hw) + 2 * pad;
        ch = hh + h + pad;
    }
    float x = floorf(cx - cw / 2), y = floorf(cy - ch / 2);
    if (side) {
        E.hx = x + pad + hw / 2;
        E.hy = y + ch / 2;
        E.r = (box){x + cw - pad - w, floorf(y + (ch - h) / 2), w, h};
    } else {
        E.hx = x + cw / 2;
        E.hy = y + hh / 2 + 0.25f * pad;
        E.r = (box){floorf(x + (cw - w) / 2), y + hh, w, h};
    }
    E.k = k;
    E.room = 1;
    end_veil(b, 120);                           /* the table veiled, the card opaque */
    if (!TH.dark) {                             /* the light theme: a white card, the game's light ink dark on it */
        light_panel(x, y, cw, ch, 0.4f * u);
        card_ink = 1;
        float t = fmaxf(1, floorf(0.03f * u));
        if (side) ui_fill(x + pad + hw + pad / 2 - t / 2, y + pad, t, ch - 2 * pad, INK[0], INK[1], INK[2], 36);
        else ui_fill(x + pad, y + hh - t, cw - 2 * pad, t, INK[0], INK[1], INK[2], 36);
        return E.r;
    }
    if (endcard_w != (int)cw || endcard_h != (int)ch || !t_endcard) {
        static const uint8_t c[4] = {28, 34, 32, 255};
        endcard_w = (int)cw;
        endcard_h = (int)ch;
        ui_free(&t_endcard);
        art_roundrect(endcard_w, endcard_h, 0.4f * u, c, ui_buf(endcard_w, endcard_h));
        t_endcard = ui_tex(endcard_w, endcard_h, ui_buf(endcard_w, endcard_h));
    }
    ui_put(t_endcard, x, y, cw, ch);
    float t = fmaxf(1, floorf(0.03f * u));      /* a faint line between the result and the table */
    if (side) ui_fill(x + pad + hw + pad / 2 - t / 2, y + pad, t, ch - 2 * pad, 246, 238, 220, 40);
    else ui_fill(x + pad, y + hh - t, cw - 2 * pad, t, 246, 238, 220, 40);
    return E.r;
}

/* a game whose hands are secret (its final table drawn under the card, not in it): the table veiled whole */
static int end_veil_whole(void)
{
    const game *g = games[cur];
    return ui.players > 1 && (g->view == VIEW_HIDDEN || (g->hidden && g->hidden()));
}

/* a solved puzzle's card: big, 0.7 of the screen's short side wide (the same look on a phone, either way round, and on
 * a desktop's window), centred on the board (it covers it: the player's eyes go to the stars), within the room between
 * the bars (smaller if the room is). The board: the square (a board game's), else the biggest grid the game fitted
 * (ui_grid_fit), else the whole room. q: the card's own unit (SOLVED_W of them wide). */
#define LINK_ROW 0.8f                           /* the stars card's rows of links: their height, in its unit */
static box link_box[3];                         /* where its links are (pages.h PAGES_LINK_*; w 0: not shown) */

/* the stars card's links shown: the top row's (a review, a problem), the bottom one's (support) */
static int link_shown(int i)
{
    int icon, word;
    const uint8_t *col;
    return pages_link(i, &icon, &col, &word);
}
static int links_top(void) { return link_shown(PAGES_LINK_REVIEW) || link_shown(PAGES_LINK_REPORT); }
static int links_bottom(void) { return link_shown(PAGES_LINK_SUPPORT); }

static void solved_place(float *cx, float *cy, float *w, float *h, float *q)
{
    box r = L.room, e = games[cur]->fill || is_view() ? grid_ext : L.board;
    if (e.w <= 0) e = r;
    float hq = (games[cur]->score ? 3.8f : 3.0f)   /* no moves to show: lower */
               + LINK_ROW * (links_top() + links_bottom());   /* the links' rows, above and below */
    float cw = 0.7f * fminf(ui.w, ui.h), ch = cw * hq / SOLVED_W;
    float k = fminf(1, fminf(0.98f * r.w / cw, 0.98f * r.h / ch));
    *w = floorf(cw * k);
    *h = floorf(ch * k);
    *q = *w / SOLVED_W;
    *cx = floorf(clampf(e.x + e.w / 2, r.x + *w / 2, r.x + r.w - *w / 2));
    *cy = floorf(clampf(e.y + e.h / 2, r.y + *h / 2, r.y + r.h - *h / 2));
}

/* a link: its icon in its colour, its word, small and quiet; *b its tap's box (taller than it is: a good target) */
static float link_draw(int i, float x, float cy, float px, float maxw, int measure, box *b)
{
    int icon, word, w = 0, h = 0;
    const uint8_t *col;
    static const uint8_t quiet[3] = {200, 196, 182}, quiet_lt[3] = {104, 94, 78};   /* (on the light theme's white) */
    if (!pages_link(i, &icon, &col, &word)) return 0;
    SDL_Texture *t = ui_word("_ui", word, px, maxw - 1.6f * px, TH.dark ? quiet : quiet_lt, &w, &h);
    float d = floorf(1.25f * px), all = d + 0.35f * px + w;
    if (measure) return all;
    SDL_Texture *ic = btn_tex(paint_btn_icon, icon, (int)d);
    if (ic) {
        SDL_SetTextureColorMod(ic, col[0], col[1], col[2]);
        ui_put_c(ic, x + d / 2, cy, d, d);
        SDL_SetTextureColorMod(ic, 255, 255, 255);
    }
    if (t) ui_put(t, x + d + 0.35f * px, floorf(cy - h / 2.0f), (float)w, (float)h);
    *b = (box){x - 0.4f * px, cy - 1.3f * px, all + 0.8f * px, 2.6f * px};
    return all;
}

/* the stars card's links: a review | a problem along its top (ty), support along its bottom (by); w: the card's width,
 * q its unit */
static void draw_links(float cx, float ty, float by, float w, float q)
{
    memset(link_box, 0, sizeof link_box);
    float px = floorf(0.3f * q), maxw = 2 * w, gap = 0.9f * px;   /* (the words at one size: the row shrinks as one) */
    box b;
    float a = link_draw(PAGES_LINK_REVIEW, 0, 0, px, maxw, 1, &b), c = link_draw(PAGES_LINK_REPORT, 0, 0, px, maxw, 1, &b);
    float all = a + c + (a > 0 && c > 0 ? 2 * gap : 0);
    if (all > 0.86f * w) {                      /* too wide for the card (long words): smaller */
        px = floorf(px * 0.86f * w / all);
        gap = 0.9f * px;
        a = link_draw(PAGES_LINK_REVIEW, 0, 0, px, maxw, 1, &b);
        c = link_draw(PAGES_LINK_REPORT, 0, 0, px, maxw, 1, &b);
        all = a + c + (a > 0 && c > 0 ? 2 * gap : 0);
    }
    float x = cx - all / 2;
    if (a > 0) link_draw(PAGES_LINK_REVIEW, x, ty, px, maxw, 0, &link_box[PAGES_LINK_REVIEW]);
    if (a > 0 && c > 0) {                       /* the bar between them (on the light theme's white: of its ink) */
        float bx = floorf(x + a + gap), by = floorf(ty - 0.55f * px), bh = floorf(1.1f * px);
        if (TH.dark) ui_fill(bx, by, 1, bh, 200, 196, 182, 90);
        else ui_fill(bx, by, 1, bh, INK[0], INK[1], INK[2], 60);
    }
    if (c > 0) link_draw(PAGES_LINK_REPORT, x + (a > 0 ? a + 2 * gap : 0), ty, px, maxw, 0, &link_box[PAGES_LINK_REPORT]);
    float s = link_draw(PAGES_LINK_SUPPORT, 0, 0, px, 0.8f * w, 1, &b);
    if (s > 0) link_draw(PAGES_LINK_SUPPORT, cx - s / 2, by, px, 0.8f * w, 0, &link_box[PAGES_LINK_SUPPORT]);
}

static void draw_over(void)
{
    memset(link_box, 0, sizeof link_box);
    if (E.room) {                               /* the game's table is in the card: the result above it */
        draw_result(E.hx, E.hy, ui.unit * E.k);
        return;
    }
    box b = L.board;
    float u = ui.unit, cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    if (solved_card()) {                        /* a puzzle solved: its stars card */
        static const uint8_t card_c[4] = {28, 34, 32, 255};   /* over the board: opaque, nothing shows through */
        float w, h, q;
        solved_place(&cx, &cy, &w, &h, &q);
        if (TH.dark) ui_put(rounded((int)w, (int)h, 0.36f * q, card_c), cx - w / 2, cy - h / 2, w, h);
        else light_panel(cx - w / 2, cy - h / 2, w, h, 0.36f * q);   /* the light theme: white */
        int top = links_top(), bot = links_bottom();
        draw_result(cx, cy + (top - bot) * LINK_ROW / 2 * q, q);   /* the result in the middle of the rest */
        draw_links(cx, cy - h / 2 + 0.5f * LINK_ROW * q, cy + h / 2 - 0.5f * LINK_ROW * q, w, q);
        return;
    }
    if (end_veil_whole() && TH.dark) ui_fill(b.x, b.y, b.w, b.h, 14, 20, 18, 250);
    else if (end_veil_whole()) {                /* the light theme: the hands hidden under the ivory table itself */
        SDL_Rect src = {0, (int)(b.y * 256 / ui.h), 2, (int)fmaxf(1, b.h * 256 / ui.h)};
        SDL_FRect d = {b.x, b.y, b.w, b.h};
        SDL_RenderCopyF(ui.r, t_bg, &src, &d);
    } else end_veil(b, 90);
    if (TH.dark) ui_put_c(t_card, cx, cy, 6.8f * u, 3.8f * u);
    else light_panel(cx - 3.4f * u, cy - 1.9f * u, 6.8f * u, 3.8f * u, 0.4f * u);
    draw_result(cx, cy, u);
}

/* the hand-off cover, over the table greyed (drawn with ui.viewer -1: every hand hidden, only what all players see)
 * and its bars and buttons. The next player's token big in gold light, the picture "pass the device",
 * a big round start button; the header over it (back, the rules). Portrait: a column; landscape: a row. */
static struct { float tx, ty, td, ix, iy, id, sx, sy, sr; } C;
static void draw_header(void);

static void cover_layout(void)
{
    float s = fminf(ui.w, ui.h);
    C.td = floorf(s * 0.34f);
    C.id = floorf(s * 0.26f);
    C.sr = floorf(s * 0.12f);
    if (ui.w <= ui.h) {
        C.tx = C.ix = C.sx = ui.w / 2.0f;
        C.ty = ui.h * 0.31f;
        C.iy = ui.h * 0.54f;
        C.sy = ui.h * 0.75f;
    } else {
        C.ty = C.iy = C.sy = ui.h * 0.54f;
        C.tx = ui.w * 0.22f;
        C.ix = ui.w * 0.5f;
        C.sx = ui.w * 0.78f;
    }
}

static void draw_cover(void)
{
    static SDL_Texture *tok, *icon, *disc, *play;
    static int tok_key = -1, icon_d, disc_d;
    cover_layout();
    int key = ((cur * 8 + hand.player) * 4096 + (int)C.td) * 8 + tok_gen % 8;
    if (tok_key != key) {                       /* its own buffer: the game may use ui_buf() */
        tok_key = key;
        int d = (int)C.td;
        uint8_t *b = calloc((size_t)d * d, 4);
        games[cur]->token(hand.player, d, b);
        ui_free(&tok);
        tok = ui_tex(d, d, b);
        free(b);
    }
    if (icon_d != (int)C.id || disc_d != (int)(2 * C.sr)) {
        static const uint8_t gold[3] = {150, 118, 46};
        icon_d = (int)C.id;
        disc_d = (int)(2 * C.sr);
        ui_free(&icon);
        art_icon(ICON_DEVICE, icon_d, ui_buf(icon_d, icon_d));
        icon = ui_tex(icon_d, icon_d, ui_buf(icon_d, icon_d));
        SDL_SetTextureColorMod(icon, 246, 238, 220);
        SDL_SetTextureAlphaMod(icon, 200);
        ui_free(&disc);
        art_disc(disc_d, gold, 1, 0, ui_buf(disc_d, disc_d));
        disc = ui_tex(disc_d, disc_d, ui_buf(disc_d, disc_d));
        int pd = (int)(1.2f * C.sr);
        ui_free(&play);
        art_icon(ICON_PLAY, pd, ui_buf(pd, pd));
        play = ui_tex(pd, pd, ui_buf(pd, pd));
        SDL_SetTextureColorMod(play, 252, 244, 226);
    }
    ui_put_c(t_gold, C.tx, C.ty, C.td * 1.7f, C.td * 1.7f);
    ui_put_c(tok, C.tx, C.ty, C.td, C.td);
    ui_put_c(icon, C.ix, C.iy, C.id, C.id);
    ui_put_c(t_gold, C.sx, C.sy, C.sr * 3.2f, C.sr * 3.2f);
    ui_put_c(disc, C.sx, C.sy, 2 * C.sr, 2 * C.sr);
    ui_put_c(play, C.sx + 0.06f * C.sr, C.sy, 1.2f * C.sr, 1.2f * C.sr);
    draw_header();                              /* back and the rules, over the cover too */
}

/* the game's name as the header shows it (the title's capitals), kept for the game, its size, the language */
static SDL_Texture *header_name(float px, float maxw, int *w, int *h)
{
    static SDL_Texture *t;
    static int key[5] = {-1}, tw, th;
    int k[5] = {cur, (int)px, (int)maxw, text_gen, !LIGHT_UI};
    const uint8_t *ink = LIGHT_UI ? INK : FV_CREAM;
    if (memcmp(k, key, sizeof k)) {
        memcpy(key, k, sizeof k);
        ui_free(&t);
        char name[96];
        if (text_name(games[cur]->id, name, sizeof name)) {
            int lw = text_title(name, px, ink, NULL, 0, 0, 0, 0);
            if (lw > maxw && lw > 0) {
                px = fmaxf(6, px * maxw / lw);
                lw = text_title(name, px, ink, NULL, 0, 0, 0, 0);
            }
            tw = lw + 4;
            th = (int)ceilf(px * 1.55f);
            uint8_t *b = calloc((size_t)tw * th, 4);
            if (b) {
                text_title(name, px, ink, b, tw, th, 2, (int)((th - 1.25f * px) / 2));
                t = ui_tex(tw, th, b);
                free(b);
            }
        }
    }
    *w = tw;
    *h = th;
    return t;
}

static void draw_header(void)
{
    float d = 2.1f * HD.r;
    ink_mod(icon_tex(ICON_BACK));
    ink_mod(icon_tex(ICON_INFO));
    ui_put_c(icon_tex(ICON_BACK), HD.bx, HD.cy, d, d);
    ui_put_c(icon_tex(ICON_INFO), HD.ix, HD.cy, d, d);
    int w, h;
    SDL_Texture *t = header_name(floorf(1.15f * HD.r), HD.ix - HD.bx - 2.8f * HD.r, &w, &h);
    if (t) ui_put(t, floorf((HD.bx + HD.ix - w) / 2), floorf(HD.cy - h / 2.0f), (float)w, (float)h);
}

static void draw_game(void)
{
    const game *g = games[cur];
    if (J[cur].back && !jr_quiet) {           /* the first frame back: the record's EV_BACK now */
        J[cur].back = 0;
        jr_done(jr_event(EV_BACK));
    }
    frames_drawn++;                             /* (the record: the last frame before an event) */
    frame_at = ui_ms();
    handoff_settle();
    level_check();
    notes_check();                              /* (a move a frame made: one that waited for its animation) */
    next_btn.d = replay_btn.d = 0;
    memset(link_box, 0, sizeof link_box);
    /* the end: its tune, the final position a moment, then the card (due: it shows this frame, ui_over(); the game
     * may draw its final table in it, ui_end_room) */
    E.due = E.room = 0;
    if (!hand.up && g->status() != GS_PLAY) {
        /* the last move still moving, the gesture that ended it still down: the end waits */
        if ((g->busy && g->busy()) || gesture_held()) end_seen = 0;
        else {
            Uint32 now = SDL_GetTicks();
            if (!end_seen) {
                end_seen = now ? now : 1;
                snd_play(SND_END);
                game_finished();
            }
            if (!instant && now - end_seen < END_PAUSE) ui_animate();
            else E.due = !over_hidden;
        }
    }
    static int card_was;                        /* the end card shown last frame */
    if (E.due && !card_was && !solved_card()) { /* it appears: a finger still down is nobody's to its lifting (a */
        if (PT.game) {                          /* puzzle solved: since then already) */
            PT.game = 0;
            PT.stray = 1;
        }
        fingers_stray();
    }
    card_was = E.due;
    game_textures();
    card_ink = 0;
    g->draw();                                  /* under the cover too: ui.viewer -1, no hand shown */
    card_ink = 0;                               /* (a white panel's ink: to the end of draw()) */
    game_textures();                           /* again: the game may have changed its tokens while drawing */
    for (int p = 0; p < ui.players; p++) draw_bar(p);
    if (armed && SDL_GetTicks() - armed > 2500) armed = 0;
    if (TL.dirty) tools_place();                /* (the buttons' row follows the tools shown) */
    for (int i = 0; i < NBTN; i++) {
        if (!shown(i)) continue;
        SDL_Texture *disc = i == B_NEW && armed ? t_btn_armed : i == B_F2F && ui.f2f ? t_btn_on : NULL;
        int icon = i == B_NEW && g->levels ? ICON_GRID : BTN_ICON[i];   /* a level: back to the level screen */
        draw_button(L.bx[i], L.by[i], L.br, icon, disc);
    }
    draw_header();
    if (hand.up) {
        ui_fill(0, 0, (float)ui.w, (float)ui.h, 14, 22, 18, 175);
        over_dark = 1;                          /* dark overlays: the dark theme's look in both */
        draw_cover();
        over_dark = 0;
        return;
    }
    card_ink = !TH.dark;                        /* the end card: dark (the light theme: white, its ink dark) */
    if (E.due) draw_over();
    card_ink = 0;
}

/* ---- the menu's pictures, painted on a thread of their own ---------------------------------------------------------
 * On the menu the main thread runs no game code: a helper thread paints the cards' pictures (each game's thumb(),
 * into its own buffer), the main thread makes them textures. The cards seen first, then the tab's others, then the
 * other tabs'. Off the menu (a game, a picker, the level screen) the helper is parked: the main thread holds `paint`
 * (it waits for the picture being painted, the only time it waits), and paints what it needs itself. */
static struct {
    SDL_mutex *m, *paint;                       /* m: the lists; paint: held while game code paints */
    SDL_cond *wake;
    int on, parked;                             /* on: the helper runs; parked: the main thread holds paint */
    int want[MAX_GAMES], nwant, size;           /* the cards to paint (the main thread's order), at that size */
    uint8_t *ready[MAX_GAMES];                  /* painted, waiting to become textures (main thread) */
    int ready_sz[MAX_GAMES], doing;             /* doing: the game being painted (-1: none) */
    Uint32 event;                               /* a picture is ready: the main loop wakes */
} WK = {.doing = -1};

/* the cards' pictures follow the theme (a game paints its light or dark look): all made again when it changes */
static void thumbs_theme(void)
{
    static int dark = -1;
    if (dark == TH.dark) return;
    if (dark >= 0) {
        for (int i = 0; i < MAX_GAMES; i++)
            for (int k = 0; k <= MAX_VARIANTS; k++) {
                ui_free(&t_thumb[i][k]);
                t_thumb_sz[i][k] = 0;
            }
        if (WK.on) {                            /* the helper's, painted in the other theme */
            SDL_LockMutex(WK.m);
            for (int i = 0; i < MAX_GAMES; i++) {
                free(WK.ready[i]);
                WK.ready[i] = NULL;
            }
            SDL_UnlockMutex(WK.m);
        }
    }
    dark = TH.dark;
}

static int worker_main(void *arg)
{
    (void)arg;
    SDL_LockMutex(WK.m);
    for (;;) {
        while (!WK.nwant) SDL_CondWait(WK.wake, WK.m);
        int i = WK.want[0], s = WK.size;
        memmove(WK.want, WK.want + 1, sizeof WK.want[0] * (size_t)--WK.nwant);
        if (WK.ready[i] && WK.ready_sz[i] == s) continue;
        WK.doing = i;
        SDL_UnlockMutex(WK.m);
        uint8_t *b = calloc((size_t)s * s, 4);
        if (b) {
            SDL_LockMutex(WK.paint);            /* not while the main thread runs game code */
            int dark = TH.dark;
            frame_for(i);                       /* (this thread's) */
            games[i]->thumb(-1, s, b);
            SDL_UnlockMutex(WK.paint);
            if (dark != TH.dark) {              /* the theme changed meanwhile: painted again when wanted */
                free(b);
                b = NULL;
            }
        }
        SDL_LockMutex(WK.m);
        WK.doing = -1;
        if (b) {
            free(WK.ready[i]);
            WK.ready[i] = b;
            WK.ready_sz[i] = s;
            SDL_Event e = {.type = WK.event};
            SDL_PushEvent(&e);                  /* the main loop draws it */
        }
    }
    return 0;
}

/* the helper started (the app, not the picture mode); 0: none (the main thread paints) */
static int worker_start(void)
{
    static int tried;
    if (tried) return WK.on;
    tried = 1;
    if (instant || getenv("RSB_NO_WORKER")) return 0;
    WK.m = SDL_CreateMutex();
    WK.paint = SDL_CreateMutex();
    WK.wake = SDL_CreateCond();
    WK.event = SDL_RegisterEvents(1);
    if (!WK.m || !WK.paint || !WK.wake || WK.event == (Uint32)-1) return 0;
    SDL_Thread *t = SDL_CreateThread(worker_main, "pictures", NULL);
    if (!t) return 0;
    SDL_DetachThread(t);
    WK.on = 1;
    return 1;
}

/* game code about to run on the main thread (a game, a picker): the helper waits */
static void worker_park(void)
{
    if (!WK.on || WK.parked) return;
    SDL_LockMutex(WK.m);
    WK.nwant = 0;                               /* nothing more to take */
    SDL_UnlockMutex(WK.m);
    SDL_LockMutex(WK.paint);                    /* the picture being painted ends first */
    WK.parked = 1;
}

static void worker_unpark(void)                 /* back on the menu */
{
    if (!WK.on || !WK.parked) return;
    WK.parked = 0;
    SDL_UnlockMutex(WK.paint);
}

/* the menu's pictures missing at size s, in order: the cards on the screen (vis), then the tab's, then the others' */
static void worker_want(const int *vis, int nvis, int s)
{
    thumbs_theme();
    if (!WK.on || WK.parked) return;
    int list[MAX_GAMES], n = 0;
    static unsigned char in[MAX_GAMES];
    memset(in, 0, sizeof in);
    for (int k = 0; k < nvis; k++) in[vis[k]] = 1, list[n++] = vis[k];
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < ngames; i++)
            if (!in[i] && (section_of(games[i]) == menu.tab) == !pass) in[i] = 1, list[n++] = i;
    SDL_LockMutex(WK.m);
    int m = 0;
    for (int k = 0; k < n; k++) {
        int i = list[k];
        if ((t_thumb[i][0] && t_thumb_sz[i][0] == s) || (WK.ready[i] && WK.ready_sz[i] == s) || WK.doing == i)
            continue;
        WK.want[m++] = i;
    }
    WK.nwant = m;
    WK.size = s;
    if (m) SDL_CondSignal(WK.wake);
    SDL_UnlockMutex(WK.m);
}

/* a picture the helper painted for game i at size s, made a texture (t_thumb); 1: one was */
static int worker_take(int i, int s)
{
    if (!WK.on) return 0;
    SDL_LockMutex(WK.m);
    uint8_t *b = WK.ready[i];
    int ok = b && WK.ready_sz[i] == s;
    if (b) WK.ready[i] = NULL;
    SDL_UnlockMutex(WK.m);
    if (ok) {
        ui_free(&t_thumb[i][0]);
        t_thumb[i][0] = ui_tex(s, s, b);
        t_thumb_sz[i][0] = s;
        t_thumb_at[i][0] = t_thumb_at[i][0] == 1 ? (ui_ms() > 1 ? ui_ms() : 2) : 0;   /* waited for: it fades in */
        if (t_thumb_at[i][0]) need_frame = 1;
    }
    free(b);
    return ok;
}

/* a card's picture, game i's (v -1: the menu's; else the picker's variant v), at the cards' size (kept at its size:
 * the menu's and a picker's cards are not the same size). Painted a few at a time (paint_may), the menu's by the
 * helper: NULL meanwhile (the card shows without it, the picture fades in when it comes). ahead: painted now. */
static SDL_Texture *thumb_ex(int i, int v, int ahead)
{
    int k = v + 1, s = (int)(L.card * 0.86f);   /* -1: the game's card */
    if (k < 0 || k > MAX_VARIANTS || s < 4) return NULL;
    thumbs_theme();
    if (t_thumb[i][k] && t_thumb_sz[i][k] == s) return t_thumb[i][k];
    if (!k && screen == SCR_MENU && WK.on && !WK.parked) {   /* the helper's */
        if (worker_take(i, s)) return t_thumb[i][k];
        t_thumb_at[i][k] = 1;
        return NULL;
    }
    if (!ahead && !paint_may()) {
        t_thumb_at[i][k] = 1;                   /* waited for: it fades in */
        return NULL;
    }
    double t0 = perf_now();
    uint8_t *b = calloc((size_t)s * s, 4);      /* its own buffer: the game may use ui_buf() while drawing */
    if (!b) return NULL;
    frame_for(i);
    games[i]->thumb(v, s, b);
    frame_for(cur);
    ui_free(&t_thumb[i][k]);
    t_thumb[i][k] = ui_tex(s, s, b);
    t_thumb_sz[i][k] = s;
    free(b);
    t_thumb_at[i][k] = t_thumb_at[i][k] == 1 ? (ui_ms() > 1 ? ui_ms() : 2) : 0;   /* waited for: it fades in */
    if (t_thumb_at[i][k]) need_frame = 1;
    paint_done(t0);
    return t_thumb[i][k];
}

static SDL_Texture *thumb(int i, int v) { return thumb_ex(i, v, 0); }

/* a card's picture as it fades in (its alpha 0..255 times a), 255 when it is not fading; asks for frames meanwhile */
static Uint8 thumb_fade(int i, int v, Uint8 a)
{
    int k = v + 1;
    if (k < 0 || k > MAX_VARIANTS || t_thumb_at[i][k] <= 1) return a;
    Uint32 t = ui_ms() - t_thumb_at[i][k];
    if (t >= FADE_MS) {
        t_thumb_at[i][k] = 0;
        return a;
    }
    ui_animate();
    return (Uint8)(a * t / FADE_MS);
}

static const uint8_t CREAM[3] = {246, 238, 220};

/* a name in cream under a card's picture, w x h: one line (k1: its size, a part of h), shrunk to fit; too long for one
 * line ("Snakes and Ladders"): two (k2), split at the space nearest the middle, if that is bigger */
static SDL_Texture *name_pic(char *name, int w, int h, float k1, float k2)
{
    const uint8_t *col = TH.dark ? CREAM : INK;   /* (the light theme: dark on its white cards) */
    float px = h * k1;
    int tw = text_line(name, px, 1, col, NULL, 0, 0, 0, 0);
    if (tw > 0.9f * w) {
        px *= 0.9f * w / tw;
        tw = text_line(name, px, 1, col, NULL, 0, 0, 0, 0);
    }
    char *cut = NULL;
    for (char *s = strchr(name, ' '); s; s = strchr(s + 1, ' '))
        if (!cut || abs((int)(s - name) * 2 - (int)strlen(name)) < abs((int)(cut - name) * 2 - (int)strlen(name)))
            cut = s;
    float px2 = h * k2;
    int w1 = 0, w2 = 0;
    if (cut && px < 0.8f * h * k1) {
        *cut = 0;
        w1 = text_line(name, px2, 1, col, NULL, 0, 0, 0, 0);
        w2 = text_line(cut + 1, px2, 1, col, NULL, 0, 0, 0, 0);
        int wide = w1 > w2 ? w1 : w2;
        if (wide > 0.9f * w) px2 *= 0.9f * w / wide;
        if (px2 > px) {
            w1 = text_line(name, px2, 1, col, NULL, 0, 0, 0, 0);
            w2 = text_line(cut + 1, px2, 1, col, NULL, 0, 0, 0, 0);
        } else {
            *cut = ' ';
            cut = NULL;
        }
    } else {
        cut = NULL;
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    if (cut) {
        float lh = 1.18f * px2, y = (h - 2 * lh) / 2;
        text_line(name, px2, 1, col, b, w, h, (w - w1) / 2, (int)y);
        text_line(cut + 1, px2, 1, col, b, w, h, (w - w2) / 2, (int)(y + lh));
    } else {
        text_line(name, px, 1, col, b, w, h, (w - tw) / 2, (int)((h - 1.25f * px) / 2));
    }
    SDL_Texture *t = ui_tex(w, h, b);
    free(b);
    return t;
}

/* a menu card's name: the first line of the game's text, in the language */
static SDL_Texture *label(int i)
{
    if (t_name[i] || L.label < 4) return t_name[i];
    char name[96];
    if (!text_name(games[i]->id, name, sizeof name)) return NULL;
    return t_name[i] = name_pic(name, L.card, L.label, 0.42f, 0.36f);
}

/* a picker card's name (card i: its line of the game's variant names), w x h; NULL: none */
static SDL_Texture *vlabel(int i, int w, int h)
{
    static SDL_Texture *t[2 * MAX_VARIANTS];
    static int key[2 * MAX_VARIANTS][3], none[2 * MAX_VARIANTS];
    static int gen;
    if (gen != text_gen) {                      /* another language: every name again */
        gen = text_gen;
        memset(key, 0, sizeof key);
    }
    if (i < 0 || i >= 2 * MAX_VARIANTS || w < 4 || h < 4) return NULL;
    if (key[i][0] == (cur + 1) * 2 + TH.dark && key[i][1] == w && key[i][2] == h) return none[i] ? NULL : t[i];
    key[i][0] = (cur + 1) * 2 + TH.dark;   /* (the theme: the names' ink) */
    key[i][1] = w;
    key[i][2] = h;
    ui_free(&t[i]);
    char name[128];
    none[i] = !vname(cur, i, name, sizeof name);
    int small = w < L.card - 2;                 /* a level's card (two axes): smaller, its name as big as the sizes' */
    if (!none[i]) t[i] = name_pic(name, w, h, small ? 0.5f : 0.30f, small ? 0.36f : 0.29f);
    return t[i];
}

/* a section's heading: its line of help/<lang>/_menu.txt, bold, cream, then a thin rule to the right edge */
static SDL_Texture *heading(int s)
{
    if (t_sec[s] || L.sec_h < 4) return t_sec[s];
    const char *t = text_get("_menu");
    for (int k = 0; t && k < s; k++) {
        t = strchr(t, '\n');
        if (t) t++;
    }
    if (!t || !*t) return NULL;
    char name[96];
    size_t n = strcspn(t, "\n");
    if (n >= sizeof name) n = sizeof name - 1;
    memcpy(name, t, n);
    name[n] = 0;
    int w = (int)L.sec_w, h = (int)L.sec_h;
    float px = fminf(h * 0.42f, fminf(ui.w, ui.h) * 0.046f);
    int tw = text_title(name, px, TH.accent, NULL, 0, 0, 0, 0);
    if (tw > 0.8f * w) {
        px *= 0.8f * w / tw;
        tw = text_title(name, px, TH.accent, NULL, 0, 0, 0, 0);
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    int ty = (int)(h * 0.62f - px * 0.75f);
    text_title(name, px, TH.accent, b, w, h, 0, ty);
    for (int y = (int)(h * 0.62f - px * 0.32f), x = tw + (int)(px * 0.6f); x < w; x++) {   /* the rule */
        uint8_t *p = b + 4 * (y * w + x);
        p[0] = TH.line[0], p[1] = TH.line[1], p[2] = TH.line[2], p[3] = 255;
    }
    t_sec[s] = ui_tex(w, h, b);
    free(b);
    return t_sec[s];
}

/* the light theme: a soft shadow under a card (two faint layers, the lower one wider), a: the card's alpha */
static void card_shadow(box c, Uint8 a)
{
    if (!LIGHT_UI || !t_menu_shadow) return;
    float e = fmaxf(1, 0.012f * c.w);
    SDL_SetTextureAlphaMod(t_menu_shadow, (Uint8)(10 * a / 255));
    ui_put(t_menu_shadow, c.x - e, c.y + 2 * e, c.w + 2 * e, c.h + e);
    SDL_SetTextureAlphaMod(t_menu_shadow, (Uint8)(16 * a / 255));
    ui_put(t_menu_shadow, c.x, c.y + e, c.w, c.h);
    SDL_SetTextureAlphaMod(t_menu_shadow, 255);
}

/* a picker card's name under its picture (a card taller than wide: the room for it) */
static void card_name(int i, box c)
{
    if (c.h > c.w + 4) ui_put(vlabel(i, (int)c.w, (int)(c.h - c.w)), c.x, c.y + c.w - 0.04f * c.w, c.w, c.h - c.w);
}

/* the picker on two axes: the sizes (the one chosen on a gold card, in gold light), the levels (the one played last at
 * that size in gold light) */
static void draw_axes(void)
{
    const game *G = games[cur];
    int ns = G->axes[0], nl = G->axes[1], v = picked(cur), lev = v >= 0 ? v % nl : 0;
    for (int i = 0; i < ns + nl; i++) {
        box c = L.cards[i];
        int level = i >= ns, k = level ? i - ns : i;
        int on = level ? v >= 0 && v / nl == pick_size[cur] && lev == k : k == pick_size[cur];
        if (on) ui_put_c(t_gold, c.x + c.w / 2, c.y + c.h / 2, c.w * 1.4f, c.h * 1.4f);
        card_shadow(c, 255);
        ui_put(on && !level ? t_menu_card_on : t_menu_card, c.x, c.y, c.w, c.h);
        float s = c.w * 0.86f;
        SDL_Texture *t;
        if (G->axis_pic[level]) t = btn_tex(G->axis_pic[level], k, (int)s);
        else t = level ? btn_tex(paint_stones, k, (int)s) : thumb(cur, k * nl + lev);
        Uint8 fa = !G->axis_pic[level] && !level ? thumb_fade(cur, k * nl + lev, 255) : 255;   /* fading in */
        if (t && fa < 255) SDL_SetTextureAlphaMod(t, fa);
        ui_put_c(t, c.x + c.w / 2, c.y + c.w / 2, s, s);
        if (t && fa < 255) SDL_SetTextureAlphaMod(t, 255);
        card_name(i, c);
    }
}

/* the picker's seats (a game with a bot): a chip each, the seat's token on its left; on its right a person, or the
 * device's robot with its level under it (one to three gold lights). A tap: a person, then the robot easy, medium,
 * hard, then a person again. */
static void draw_seats(void)
{
    static SDL_Texture *tok[MAX_PLAYERS];
    static int key = -1;
    static const uint8_t person_c[4] = {54, 72, 64, 255}, bot_c[4] = {30, 40, 46, 255}, gold[4] = {255, 200, 80, 255};
    if (!L.nseats) return;
    float h = L.seats[0].h, w = L.seats[0].w;
    int ts = (int)(0.62f * h), k0 = ((cur * 64 + ts) * 8 + tok_gen % 8);
    if (key != k0) {                            /* the seats' tokens, at their size (their own buffer) */
        key = k0;
        uint8_t *b = calloc((size_t)ts * ts, 4);
        int most = 31 - __builtin_clz(player_counts(cur));   /* (a game's tokens: its players' only) */
        for (int p = 0; p < MAX_PLAYERS && b; p++) {
            ui_free(&tok[p]);
            if (p >= most) continue;
            memset(b, 0, (size_t)ts * ts * 4);
            games[cur]->token(p, ts, b);
            tok[p] = ui_tex(ts, ts, b);
        }
        free(b);
    }
    for (int k = 0; k < L.nseats; k++) {
        box b = L.seats[k];
        int lv = seat_lv[cur][k];
        float cy = b.y + h / 2, fx = b.x + w - 0.64f * h;
        if (TH.dark) ui_put(rounded((int)w, (int)h, h / 2, lv ? bot_c : person_c), b.x, b.y, w, h);
        else {                                  /* the light theme: white chips, the robot's beige */
            static const uint8_t lt_person[4] = {255, 253, 248, 255}, lt_bot[4] = {236, 226, 206, 255},
                                 rim[4] = {220, 208, 184, 255};
            ui_put(rounded((int)w, (int)h, h / 2, rim), b.x, b.y, w, h);
            ui_put(rounded((int)w - 2, (int)h - 2, h / 2 - 1, lv ? lt_bot : lt_person), b.x + 1, b.y + 1, w - 2, h - 2);
        }
        ui_put_c(tok[k], b.x + 0.52f * h, cy, (float)ts, (float)ts);
        float id = floorf((lv ? 0.58f : 0.66f) * h);
        SDL_Texture *t = btn_tex(paint_btn_icon, lv ? ICON_BOT : ICON_PERSON, (int)id);
        if (t) {
            ink_mod(t);
            ui_put_c(t, fx, lv ? cy - 0.1f * h : cy, id, id);
            SDL_SetTextureColorMod(t, 255, 255, 255);
        }
        float pd = floorf(fmaxf(3, 0.12f * h));
        for (int i = 0; i < lv; i++)            /* the level: one, two or three lights */
            ui_put_c(rounded((int)pd, (int)pd, pd / 2, gold), fx + (i - (lv - 1) / 2.0f) * 0.19f * h, cy + 0.3f * h,
                     pd, pd);
    }
    if (L.online.w > 0) {                       /* the online chip: a globe, faded (online play is coming later) */
        box b = L.online;
        static const uint8_t lt_rim[4] = {220, 208, 184, 255};   /* (the light seats' rim) */
        SDL_Texture *c = rounded((int)b.w, (int)b.h, b.h / 2, TH.dark ? person_c : lt_rim);
        if (c) {
            SDL_SetTextureAlphaMod(c, TH.dark ? 110 : 150);
            ui_put(c, b.x, b.y, b.w, b.h);
            SDL_SetTextureAlphaMod(c, 255);
        }
        float id = floorf(0.56f * b.h);
        SDL_Texture *t = btn_tex(paint_btn_icon, ICON_GLOBE, (int)id);
        if (t) {
            ink_mod(t);
            SDL_SetTextureAlphaMod(t, 120);
            ui_put_c(t, b.x + b.w / 2, b.y + b.h / 2, id, id);
            SDL_SetTextureAlphaMod(t, 255);
            SDL_SetTextureColorMod(t, 255, 255, 255);
        }
    }
}

/* the header of a game's picker or level screen, as a game's: back (to the games) at the left, the game's name in the
 * middle, i (its rules) at the right; under it the slim row (L.row): the stars won at the left (the level screen), and
 * when a game of it is in progress the gold play button at the right (it picks it up) */
static float head_x(void) { return fminf(ui.w, ui.h) * 0.05f + L.snd_r; }

static void draw_pick_head(void)
{
    float hx = head_x(), d = floorf(1.6f * L.snd_r);
    SDL_Texture *bk = icon_tex(ICON_BACK), *in = icon_tex(ICON_INFO);
    ink_mod(bk);
    ink_mod(in);
    ui_put_c(bk, hx, L.snd_y, d, d);
    ui_put_c(in, ui.w - hx, L.snd_y, d, d);
    int w, h;
    SDL_Texture *t = header_name(floorf(1.0f * L.snd_r), ui.w - 2 * hx - 2.4f * L.snd_r, &w, &h);
    if (t) ui_put(t, floorf((ui.w - w) / 2), floorf(L.snd_y - h / 2.0f), (float)w, (float)h);
    if (!L.row) return;
    float m = fminf(ui.w, ui.h) * 0.05f, r = L.row_r;
    if (resumable()) ui_button(ui.w - m - r, L.row_y, 2 * r, ICON_PLAY, UI_ON | UI_LIT);
    if (games[cur]->levels > 0 && games[cur]->level_stars) {   /* the stars won (the level screen, random play's) */
        progress_load(cur);
        int total = 0;
        for (int l = 0; l < nlevels(cur); l++) total += LV.best[l] ? LV.best[l] - 1 : 0;
        float nh = floorf(r * 1.05f), sd = 1.6f * nh, nw = ui_number_w(total, nh);
        star_at(m + sd / 2, L.row_y, sd, 0);
        ui_number_ex(m + sd + 0.3f * nh + nw / 2, L.row_y, nh, total, TH.dark ? CREAM : INK, 255, 0);
    }
}

/* a tap on that header and row: 1 when it was theirs */
static int pick_head_press(float x, float y)
{
    float hx = head_x();
    if (in_circle(x, y, hx, L.snd_y, L.snd_r)) {
        snd_play(SND_TAP);
        go_home();
        return 1;
    }
    if (in_circle(x, y, ui.w - hx, L.snd_y, L.snd_r)) {
        snd_play(SND_TAP);
        open_help(cur);
        return 1;
    }
    if (L.row && resumable() && in_circle(x, y, ui.w - fminf(ui.w, ui.h) * 0.05f - L.row_r, L.row_y, 1.3f * L.row_r)) {
        snd_play(SND_TAP);                      /* the game in progress waits */
        open_game(cur, -1);
        return 1;
    }
    return 0;
}

/* a picture in the tabs' row, its columns faded towards an edge of the window lo..hi that has more past it: over fl
 * pixels at the left, fr at the right (0: no fade), in strips of a few columns; nothing outside the window */
static void put_faded(SDL_Texture *t, float x, float y, float w, float h, float lo, float hi, float fl, float fr)
{
    if (!t || w <= 0 || x >= hi || x + w <= lo) return;
    int tw, th;
    Uint8 a0;
    SDL_QueryTexture(t, NULL, NULL, &tw, &th);
    SDL_GetTextureAlphaMod(t, &a0);
    const float cut[4] = {lo, lo + fl, hi - fr, hi};
    for (int seg = 0; seg < 3; seg++) {         /* the left fade, the middle, the right fade */
        float s0 = fmaxf(x, cut[seg]), s1 = fminf(x + w, cut[seg + 1]);
        if (s1 <= s0) continue;
        float step = seg == 1 ? s1 - s0 : fmaxf(2, (seg ? fr : fl) / 12);
        for (float a = s0; a < s1 - 0.01f; a += step) {
            int c0 = (int)lroundf((a - x) * tw / w), c1 = (int)lroundf((fminf(s1, a + step) - x) * tw / w);
            if (c1 <= c0) continue;
            float mid = x + (c0 + c1) * 0.5f * w / tw;
            float f = seg == 0 ? (mid - lo) / fl : seg == 2 ? (hi - mid) / fr : 1;
            SDL_SetTextureAlphaMod(t, (Uint8)(a0 * clampf(f, 0, 1)));
            SDL_Rect src = {c0, 0, c1 - c0, th};
            SDL_FRect dst = {x + c0 * w / tw, y, (c1 - c0) * w / tw, h};
            SDL_RenderCopyF(ui.r, t, &src, &dst);
        }
    }
    SDL_SetTextureAlphaMod(t, a0);
}

/* the menu's tabs: the row scrolled (its fling, its slide to the tab chosen), each tab and its picture */
static void draw_tabs(void)
{
    Uint32 now = SDL_GetTicks();
    float dt = fminf(50, (float)(now - menu.tt));
    if (menu.tmax > 0 && !menu.tgrab && (menu.tvel != 0 || menu.tgoal >= 0)) {
        menu.tt = now;
        if (menu.tgoal >= 0) {                  /* sliding to the tab chosen: eased, about a fifth of a second */
            menu.tx += (menu.tgoal - menu.tx) * fminf(1, dt / 70);
            if (fabsf(menu.tgoal - menu.tx) < 0.5f) menu.tx = menu.tgoal, menu.tgoal = -1;
        } else {                                /* the fling: slows down, stops at the ends */
            menu.tx = clampf(menu.tx + menu.tvel * dt, 0, menu.tmax);
            menu.tvel *= powf(0.995f, dt);
            if (fabsf(menu.tvel) < 0.02f || menu.tx <= 0 || menu.tx >= menu.tmax) menu.tvel = 0;
        }
        need_frame = 1;
    }
    box r = L.tab_row;
    float fade = menu.tmax > 0 ? floorf(0.4f * r.h) : 0;
    float fl = menu.tx > 0.5f ? fade : 0, fr = menu.tx < menu.tmax - 0.5f ? fade : 0;
    for (int k = 0; k < L.ntabs; k++) {
        box b = L.tabs[k];
        b.x -= menu.tx;
        int s = L.tab_sec[k];
        float d = floorf(b.h * 0.66f);
        if (menu.tmax <= 0) {                   /* all of them in the row */
            ui_put(s == menu.tab ? t_tab_on : t_tab, b.x, b.y, b.w, b.h);
            ui_put_c(t_tabicon[s], b.x + b.w / 2, b.y + b.h / 2, d, d);
            continue;
        }
        put_faded(s == menu.tab ? t_tab_on : t_tab, b.x, b.y, b.w, b.h, r.x, r.x + r.w, fl, fr);
        put_faded(t_tabicon[s], b.x + (b.w - d) / 2, b.y + (b.h - d) / 2, d, d, r.x, r.x + r.w, fl, fr);
    }
}

static void draw_cards(void)
{
    int n = screen == SCR_MENU ? ngames : games[cur]->variants;
    if (screen == SCR_PICK && axes_on(cur)) {   /* two axes: their own cards */
        draw_axes();
        n = 0;
    }
    if (screen == SCR_MENU && L.ntabs) draw_tabs();
    if (screen == SCR_PICK && games[cur]->levels) draw_level_tabs(LV.nch);   /* random play's picker */
    for (int k = 0; k < L.npills; k++) {       /* the picker: how many play */
        box b = L.pills[k];
        int np = L.pill_n[k];
        float ts = floorf(b.h * 0.56f), step = b.h * 0.44f, x0 = b.x + b.w / 2 - (np - 1) * step / 2;
        ui_put(np == ui.players ? t_pill_on : t_pill, b.x, b.y, b.w, b.h);
        for (int p = 0; p < np; p++) ui_put_c(t_ptok[p], x0 + p * step, b.y + b.h / 2, ts, ts);
    }
    if (screen == SCR_PICK) draw_seats();
    float dy = 0;
    box room = screen == SCR_MENU ? pages_room() : (box){0, 0, (float)ui.w, (float)ui.h};
    if (screen == SCR_MENU && menu.max > 0) {   /* the cards' window, below the buttons */
        if (menu.vel != 0 && !menu.grabbing) {  /* the fling: slows down, stops at the ends */
            Uint32 t = SDL_GetTicks();
            float dt = fminf(50, (float)(t - menu.t));
            menu.t = t;
            menu.scroll = fminf(menu.max, fmaxf(0, menu.scroll + menu.vel * dt));
            menu.vel *= powf(0.996f, dt);
            if (fabsf(menu.vel) < 0.02f || menu.scroll <= 0 || menu.scroll >= menu.max) menu.vel = 0;
            else need_frame = 1;
        }
        dy = -menu.scroll;
        SDL_Rect clip = {(int)room.x, (int)menu.top, (int)room.w, (int)(room.y + room.h - menu.top)};
        SDL_RenderSetClipRect(ui.r, &clip);
    }
    int vis[MAX_GAMES], nvis = 0;               /* the menu's cards on the screen: the helper's first pictures */
    menu_missing = 0;
    for (int i = 0; i < n; i++) {
        box c = L.cards[i];
        c.y += dy;
        if (c.w <= 0) continue;                 /* another tab's game */
        if (c.y > room.y + room.h || c.y + c.h * 1.2f < menu.top) continue;
        int gi = screen == SCR_MENU ? i : cur, v = screen == SCR_MENU ? -1 : i;
        if (screen == SCR_MENU) vis[nvis++] = i;
        if ((screen == SCR_MENU && started[i] && games[i]->status() == GS_PLAY) ||   /* a game in progress */
            (screen == SCR_PICK && picked(cur) == i))   /* the variant chosen last */
            ui_put_c(t_gold, c.x + c.w / 2, c.y + c.h / 2, c.w * 1.25f, c.h * 1.25f);
        /* a variant the bot cannot play, the device given a seat: dimmed (it starts with people in every seat) */
        Uint8 a = screen == SCR_PICK && L.nseats && any_bot_seat(cur) && !bot_plays(cur, i) ? 110 : 255;
        int lk = screen == SCR_MENU && locked(i);   /* the free edition: dimmed, a lock (its picture still seen) */
        if (lk) a = 130;
        SDL_Texture *th = thumb(gi, v);
        menu_missing += !th;
        SDL_SetTextureAlphaMod(t_menu_card, a);
        if (th) SDL_SetTextureAlphaMod(th, thumb_fade(gi, v, a));
        card_shadow(c, a);
        ui_put(t_menu_card, c.x, c.y, c.w, c.h);
        float s = c.w * 0.86f;
        ui_put_c(th, c.x + c.w / 2, c.y + c.w / 2, s, s);
        SDL_SetTextureAlphaMod(t_menu_card, 255);
        if (th) SDL_SetTextureAlphaMod(th, 255);
        if (screen == SCR_MENU) {
            SDL_Texture *lb = label(i);
            if (lk && lb) SDL_SetTextureAlphaMod(lb, 170);
            ui_put(lb, c.x, c.y + c.w - 0.04f * c.w, c.w, (float)L.label);
            if (lk && lb) SDL_SetTextureAlphaMod(lb, 255);
            if (lk) lock_badge(c.x + 0.83f * c.w, c.y + 0.17f * c.w, 0.25f * c.w);
        } else {
            card_name(i, c);
        }
    }
    if (screen == SCR_MENU) worker_want(vis, nvis, (int)(L.card * 0.86f));
    for (int s = 0; s < SECTIONS && screen == SCR_MENU; s++) {
        float y = L.sec_y[s] + dy;
        if (L.sec_y[s] < 0 || y > room.y + room.h || y + L.sec_h < menu.top) continue;
        ui_put(heading(s), L.sec_x, y, L.sec_w, L.sec_h);
    }
    if (screen == SCR_MENU && menu.max > 0) {   /* where the list is: a thin bar on the right */
        SDL_RenderSetClipRect(ui.r, NULL);
        float vh = room.y + room.h - menu.top, th = vh * vh / (vh + menu.max);
        float ty = menu.top + (vh - th) * menu.scroll / menu.max;
        ui_fill(room.x + room.w - fminf(ui.w, ui.h) * 0.016f, ty, fminf(ui.w, ui.h) * 0.008f, th, TH.text[0],
                TH.text[1], TH.text[2], 70);
    }
    if (screen != SCR_MENU) draw_pick_head();
}

/* ---- the level screen -------------------------------------------------------------------------------------------- */
/* a level's picture (game.level_thumb) at the size shown, kept for the chapter shown (a texture a tile); made again
 * when the level's best changes (a puzzle solved may show its picture) */
static SDL_Texture *lv_pic[CHAPTER_MAX];
static int lv_key[CHAPTER_MAX][4];

static int level_pic_made(int i, int level, int size)
{
    const int *key = lv_key[i];
    return lv_pic[i] && key[0] == cur && key[1] == level && key[2] == size && key[3] == LV.best[level];
}

/* (painted a few at a time, as the cards' pictures: NULL meanwhile, the tile shows its number; ahead: now) */
static SDL_Texture *level_pic_ex(int i, int level, int size, int ahead)
{
    SDL_Texture **t = lv_pic;
    int (*key)[4] = lv_key;
    if (size < 4 || i < 0 || i >= CHAPTER_MAX) return NULL;
    if (level_pic_made(i, level, size)) return t[i];
    if (!ahead && !paint_may()) return NULL;
    double t0 = perf_now();
    uint8_t *b = calloc((size_t)size * size, 4);   /* its own buffer: the game may use ui_buf() */
    if (!b) return NULL;
    games[cur]->level_thumb(level, size, b);
    paint_done(t0);
    ui_free(&t[i]);
    t[i] = ui_tex(size, size, b);
    free(b);
    key[i][0] = cur;
    key[i][1] = level;
    key[i][2] = size;
    key[i][3] = LV.best[level];
    return t[i];
}

static SDL_Texture *level_pic(int i, int level, int size) { return level_pic_ex(i, level, size, 0); }

/* a level's tile: gold when solved (its stars under its number), wood when open, dim walnut when locked; the next
 * level to play in gold light. With the game's picture: the picture on the tile, its number on a dark disc in the
 * corner, the stars along the bottom. */
static void draw_tile(int l, int i, box t, int next)
{
    static const uint8_t dim[3] = {150, 120, 90};
    const game *G = games[cur];
    int lk = level_locked(l), best = lk ? 0 : LV.best[l], open = level_open(cur, l), n = l + 1,
        stars = best && G->level_stars;
    float T = t.w, cx = t.x + T / 2, cy = t.y + T / 2;
    if (l == next && open) ui_mark_gold(cx, cy, 1.5f * T);
    ui_shadow_flat(cx + 0.04f * T, cy + 0.06f * T, T, T, 0.55f);
    ui_put(btn_tex(paint_tile, best ? 0 : open ? 1 : TH.dark ? 2 : 3, (int)T), t.x, t.y, T, T);
    if (G->level_thumb) {
        float ps = floorf(0.8f * T), bd = floorf(0.44f * T), bx = t.x + 0.17f * T, by = t.y + 0.17f * T;
        SDL_Texture *p = level_pic(i, l, (int)ps);
        if (p) {
            SDL_SetTextureAlphaMod(p, open ? 255 : 90);
            ui_put_c(p, cx, cy, ps, ps);
            SDL_SetTextureAlphaMod(p, 255);
        }
        ui_put_c(btn_tex(paint_badge, 0, (int)bd), bx, by, bd, bd);
        float nh = fminf(0.58f * bd, 0.9f * bd / (0.66f * (n >= 100 ? 2 : n >= 10 ? 1 : 0) + 0.62f));   /* fits the disc */
        ui_number_ex(bx, by, nh, n, CREAM, open ? 255 : 140, UI_NUM_TIGHT);
        for (int k = 0; stars && k < 3; k++)
            star_at(cx + (k - 1) * 0.25f * T, t.y + 0.85f * T, 0.25f * T, k < best - 1 ? 0 : 1);
        return;
    }
    ui_number_ex(cx, cy - (stars ? 0.12f * T : 0), 0.4f * T, n, open ? NUM_DARK : dim, open ? 255 : 160, 0);
    for (int k = 0; stars && k < 3; k++)
        star_at(cx + (k - 1) * 0.25f * T, cy + 0.28f * T, 0.24f * T, k < best - 1 ? 0 : 1);
}

/* the level screen's tabs (on: the one shown; LV.nch: random play) */
static void draw_level_tabs(int on)
{
    const game *G = games[cur];
    if (level_ntabs() <= 1) return;
    progress_load(cur);
    if (has_random(cur)) {                      /* random play: a die */
        box b = LV.tabs[LV.nch];
        float d = floorf(b.h * 0.66f);
        ui_put(on == LV.nch ? t_tab_on : t_tab, b.x, b.y, b.w, b.h);
        ui_put_c(t_tabicon[SEC_RACE], b.x + b.w / 2, b.y + b.h / 2, d, d);
    }
    for (int k = 0; k < LV.nch; k++) {          /* the chapters: the game's picture, else the number */
        box b = LV.tabs[k];
        int any = 0;
        for (int l = LV.first[k]; l < LV.first[k + 1] && !any; l++) any = level_open(cur, l);
        Uint8 a = any ? 255 : 90;
        float d = floorf(b.h * 0.66f), cy = b.y + b.h / 2, nh = floorf(b.h * 0.4f);
        ui_put(k == on ? t_tab_on : t_tab, b.x, b.y, b.w, b.h);
        SDL_Texture *p = G->chapter_icon ? btn_tex(G->chapter_icon, k, (int)d) : NULL;
        if (!p) {
            ui_number_ex(b.x + b.w / 2, cy, nh, k + 1, TH.dark ? CREAM : INK, a, 0);
            continue;
        }
        float nw = b.w >= 1.9f * b.h ? ui_number_w(k + 1, nh) : 0, gap = nw ? 0.12f * b.h : 0;
        float x0 = b.x + (b.w - d - gap - nw) / 2;   /* wide enough: the picture, then the number */
        SDL_SetTextureAlphaMod(p, a);
        ui_put(p, x0, cy - d / 2, d, d);
        SDL_SetTextureAlphaMod(p, 255);
        if (nw) ui_number_ex(x0 + d + gap + nw / 2, cy, nh, k + 1, TH.dark ? CREAM : INK, a, 0);
    }
}

static void draw_levels(void)
{
    progress_load(cur);
    draw_level_tabs(LV.chapter);
    int first = LV.first[LV.chapter], n = LV.first[LV.chapter + 1] - first, next = first_unsolved(cur);
    if (n > CHAPTER_MAX) n = CHAPTER_MAX;
    for (int i = 0; i < n; i++) {
        draw_tile(first + i, i, LV.tiles[i], next);
        box t = LV.tiles[i];                    /* the free edition: a lock on the full version's */
        if (level_locked(first + i)) lock_badge(t.x + 0.84f * t.w, t.y + 0.16f * t.w, 0.3f * t.w);
    }
    draw_pick_head();                           /* (the stars won: in its row) */
}

/* ---- the rules page ---------------------------------------------------------------------------------------------- */
static char *help_text;                        /* the game's rules (-1: the app's page, "_app") */

static void help_free(void)
{
    for (int i = 0; i < MAX_TILES; i++) ui_free(&help.tile[i]);
    help.made_w = 0;
}

static const char *help_key;                    /* help.game -2: this text (shell_open_text) */
static void help_compose(void)
{
    /* a game's rules alone (how the app plays: Settings, How to play, "_app"); -1: that page */
    const char *t = text_get(help.game == -2 ? help_key : help.game >= 0 ? games[help.game]->id : "_app");
    free(help_text);
    help_text = malloc(strlen(t ? t : "") + 1);
    strcpy(help_text, t ? t : "");
}

static void help_layout(void)
{
    float mn = fminf(ui.w, ui.h), m = mn * 0.035f, pad = mn * 0.05f;
    help.card = (box){m, m, ui.w - 2 * m, ui.h - 2 * m};
    help.px = fmaxf(13, fminf(44, mn * 0.036f));
    help.close_r = fmaxf(help.px * 1.05f, mn * 0.05f);
    help.close_x = help.card.x + help.card.w - pad * 0.5f - help.close_r;
    help.close_y = help.card.y + pad * 0.5f + help.close_r;
    float top = help.card.y + pad * 0.5f + 2 * help.close_r + pad * 0.2f;
    help.view = (box){help.card.x + pad, top, help.card.w - 2 * pad, help.card.y + help.card.h - pad * 0.6f - top};
    int w = (int)help.view.w;
    if (help.made_w == w) return;
    help_free();
    help.made_w = w;
    help.h = text_page(help_text, w, help.px, TH.text, NULL, 0, 0);
    help.tiles = (help.h + TILE - 1) / TILE;
    if (help.tiles > MAX_TILES) help.tiles = MAX_TILES;
    uint8_t *b = malloc((size_t)w * TILE * 4);
    for (int k = 0; k < help.tiles; k++) {
        memset(b, 0, (size_t)w * TILE * 4);
        text_page(help_text, w, help.px, TH.text, b, k * TILE, TILE);
        help.tile[k] = ui_tex(w, TILE, b);
    }
    free(b);
}

static float help_max(void) { return fmaxf(0, help.h - help.view.h); }

static void open_help(int g)
{
    help.open = 1;
    help.game = g;
    help.scroll = 0;
    help.grabbing = 0;
    help_compose();
    help_free();
    help_layout();
}

static void draw_help(void)
{
    static SDL_Texture *page;
    static int page_w, page_h, page_dark = -1;
    ui_fill(0, 0, (float)ui.w, (float)ui.h, 0, 0, 0, 150);
    if (page_w != (int)help.card.w || page_h != (int)help.card.h || page_dark != TH.dark) {
        page_w = (int)help.card.w;
        page_h = (int)help.card.h;
        page_dark = TH.dark;
        const uint8_t c[4] = {TH.bg[0], TH.bg[1], TH.bg[2], 250};
        ui_free(&page);
        art_roundrect(page_w, page_h, fminf(ui.w, ui.h) * 0.04f, c, ui_buf(page_w, page_h));
        page = ui_tex(page_w, page_h, ui_buf(page_w, page_h));
    }
    ui_put(page, help.card.x, help.card.y, help.card.w, help.card.h);
    SDL_Rect clip = {(int)help.view.x, (int)help.view.y, (int)help.view.w, (int)help.view.h};
    SDL_RenderSetClipRect(ui.r, &clip);
    for (int k = 0; k < help.tiles; k++) {
        float y = help.view.y + k * TILE - help.scroll;
        if (y + TILE < help.view.y || y > help.view.y + help.view.h) continue;
        ui_put(help.tile[k], help.view.x, y, help.view.w, TILE);
    }
    SDL_RenderSetClipRect(ui.r, NULL);
    if (help_max() > 0) {                       /* where the page is: a thin bar on the right */
        float th = help.view.h * help.view.h / help.h, ty = help.view.y + (help.view.h - th) * help.scroll / help_max();
        ui_fill(help.card.x + help.card.w - fminf(ui.w, ui.h) * 0.022f, ty, fminf(ui.w, ui.h) * 0.008f, th,
                TH.text[0], TH.text[1], TH.text[2], 90);
    }
    draw_button(help.close_x, help.close_y, help.close_r, ICON_CLOSE, NULL);
}

static void help_scroll(float dy)
{
    help.scroll = fminf(help_max(), fmaxf(0, help.scroll + dy));
}

/* the idle main loop: one picture not seen yet painted ahead (the menu: the tab shown from its top, then the other
 * tabs; a picker: its cards; the level screen: the chapter's tiles); 1: one was painted */
static int paint_ahead(void)
{
    if (help.open || FV.open) return 0;
    if (screen == SCR_MENU && L.card > 0) {
        int s = (int)(L.card * 0.86f);
        if (WK.on && !WK.parked) {              /* the helper paints them: the ones it has painted become textures */
            for (int i = 0; i < ngames; i++)
                if (worker_take(i, s)) return 1;
            return 0;
        }
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < ngames; i++) {
                if ((section_of(games[i]) == menu.tab) != !pass) continue;
                if (t_thumb[i][0] && t_thumb_sz[i][0] == s) continue;
                thumb_ex(i, -1, 1);
                return 1;
            }
        return 0;
    }
    if (cur < 0) return 0;
    if (screen == SCR_PICK && !axes_on(cur)) {
        for (int v = 0; v < games[cur]->variants && v < MAX_VARIANTS; v++) {
            int s = (int)(L.card * 0.86f);
            if (t_thumb[cur][v + 1] && t_thumb_sz[cur][v + 1] == s) continue;
            thumb_ex(cur, v, 1);
            return 1;
        }
        return 0;
    }
    if (screen == SCR_PICK) {                   /* two axes: the sizes' cards (a size: thumb() at the level chosen) */
        const game *G = games[cur];
        int nl = G->axes[1], v = picked(cur), lev = v >= 0 ? v % nl : 0;
        if (G->axis_pic[0]) return 0;
        for (int k = 0; k < G->axes[0]; k++) {
            int s = (int)(L.card * 0.86f), vk = k * nl + lev;
            if (vk + 1 > MAX_VARIANTS || (t_thumb[cur][vk + 1] && t_thumb_sz[cur][vk + 1] == s)) continue;
            thumb_ex(cur, vk, 1);
            return 1;
        }
        return 0;
    }
    if (screen == SCR_LEVELS && games[cur]->level_thumb) {
        int first = LV.first[LV.chapter], n = LV.first[LV.chapter + 1] - first;
        if (n > CHAPTER_MAX) n = CHAPTER_MAX;
        for (int i = 0; i < n; i++) {
            int ps = (int)floorf(0.8f * LV.tiles[i].w);
            if (ps < 4 || level_pic_made(i, first + i, ps)) continue;
            level_pic_ex(i, first + i, ps, 1);
            need_frame = 1;                     /* a tile shown: drawn with it */
            return 1;
        }
    }
    return 0;
}

static void draw(void)
{
    paint_used = 0;
    paint_n = 0;
    if (screen == SCR_MENU) pages_draw_bg();
    else ui_put(t_bg, 0, 0, (float)ui.w, (float)ui.h);
    if (screen == SCR_GAME) draw_game();
    else if (screen == SCR_LEVELS) draw_levels();
    else if (screen == SCR_PICK) draw_cards();
    else {
        if (page == PAGE_GAMES) draw_cards();
        else menu_missing = 0;
        pages_draw();
    }
    if (help.open) draw_help();
    if (FV.open) draw_full();
    if (screen != SCR_MENU) pages_panel_draw();   /* the tip panel, opened off the menu (the stars card) */
    pages_soon_draw(screen == SCR_MENU);        /* "online play is coming later", after a tap on it */
}

/* ---- input ------------------------------------------------------------------------------------------------------- */
static int in_circle(float x, float y, float cx, float cy, float r) { return hypotf(x - cx, y - cy) < r * 1.1f; }

static int press_bar(float x, float y)
{
    for (int p = 0; p < ui.players; p++) {
        if (!shows_pass(p)) continue;
        box b = L.bar[p];
        float lx = x - b.x, ly = y - b.y;
        if (ui_upside(p)) { lx = b.w - lx; ly = b.h - ly; }
        float r = 0.42f * ui.unit;
        if (in_circle(lx, ly, b.w - r - 0.05f * ui.unit, b.h / 2, r)) {
            struct jrec *jr = jr_event(EV_PASS);
            games[cur]->pass();
            jr_done(jr);
            snd_play(SND_TAP);
            return 1;
        }
    }
    return 0;
}

/* the menu's tab: another section's games, from the top */
static void menu_tab(int s)
{
    if (menu.tab == s) return;
    menu.tab = s;
    menu.scroll = menu.vel = 0;
    uint8_t b = (uint8_t)s;                     /* the menu opens there next time */
    ui_save_set("shell/tab", &b, 1);
    layout();
    tabs_show(0);                               /* its tab in the row's view */
}

static void menu_step(int d)                    /* the tab on the left (-1) or the right (1) */
{
    for (int k = 0; k < L.ntabs; k++)
        if (L.tab_sec[k] == menu.tab && k + d >= 0 && k + d < L.ntabs) {
            snd_play(SND_TAP);
            menu_tab(L.tab_sec[k + d]);
            return;
        }
}

/* the level screen's last tab: random play. Its variant picker (the level screen's tabs over it), or a random
 * puzzle at once when there is nothing to pick */
static void random_tab(void)
{
    if (games[cur]->variants > 1 || many_counts(cur)) {
        screen = SCR_PICK;
        axes_enter(cur);
        layout();
        return;
    }
    rnd_mode[cur] = 1;
    open_game(cur, 0);
}

static void levels_step(int d)                  /* the level screen's chapter on the left (-1) or the right (1) */
{
    if (LV.chapter + d < 0 || LV.chapter + d >= level_ntabs()) return;
    snd_play(SND_TAP);
    if (LV.chapter + d == LV.nch) {             /* past the last chapter: random play */
        random_tab();
        return;
    }
    LV.chapter += d;
    layout();
}

static int in_box(box b, float x, float y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

/* the level screen: home and the tabs at once; a tile on release, unless the finger swiped (another chapter) */
static void levels_press(float x, float y)
{
    if (pick_head_press(x, y)) return;          /* back, the rules, the level in play */
    for (int k = 0; k < level_ntabs() && level_ntabs() > 1; k++)
        if (in_box(LV.tabs[k], x, y)) {
            snd_play(SND_TAP);
            if (k == LV.nch) {
                random_tab();
                return;
            }
            LV.chapter = k;
            layout();
            return;
        }
    LV.grabbing = 1;
    LV.moved = 0;
    LV.grab_x = x;
    LV.grab_y = y;
}

static void levels_release(float x, float y)
{
    float s = fminf(ui.w, ui.h);
    LV.grabbing = 0;
    if (LV.moved) {
        if (fabsf(x - LV.grab_x) > 0.12f * s && fabsf(x - LV.grab_x) > fabsf(y - LV.grab_y))
            levels_step(x < LV.grab_x ? 1 : -1);
        return;
    }
    int first = LV.first[LV.chapter], n = LV.first[LV.chapter + 1] - first;
    for (int i = 0; i < n && i < CHAPTER_MAX; i++)
        if (in_box(LV.tiles[i], x, y)) {
            if (level_locked(first + i)) { snd_play(SND_TAP); full_open(); return; }   /* the full version's */
            if (!level_open(cur, first + i)) { snd_play(SND_DENY); return; }
            snd_play(SND_TAP);
            rnd_mode[cur] = 0;
            open_game(cur, first + i);
            return;
        }
}

static void game_press(float x, float y);
static void press(float x, float y)
{
    PT.game = PT.stray = 0;                     /* a new press: the last one has lifted */
    if (screen != SCR_MENU && pages_panel_press(x, y)) {   /* the tip panel, opened off the menu (the stars card) */
        PT.stray = 1;
        return;
    }
    if (FV.open) {                              /* the full version's panel: its button, or it closes */
        full_press(x, y);
        return;
    }
    if (help.open) {                            /* the rules page: scroll it, or close it */
        box c = help.card;
        if (in_circle(x, y, help.close_x, help.close_y, help.close_r) || x < c.x || y < c.y || x > c.x + c.w ||
            y > c.y + c.h) {
            help.open = 0;
            snd_play(SND_TAP);
            return;
        }
        help.grabbing = 1;
        help.grab_y = y;
        help.grab_scroll = help.scroll;
        return;
    }
    if (screen == SCR_LEVELS) {
        levels_press(x, y);
        return;
    }
    if (screen == SCR_MENU && pages_press(x, y)) return;   /* the ribbon, the pages, the Games page's header */
    if (screen != SCR_GAME) {
        if (screen == SCR_PICK && pick_head_press(x, y)) return;   /* back, the rules, the game in progress */
        for (int k = 0; k < L.npills; k++) {
            box b = L.pills[k];
            if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
            snd_play(SND_TAP);
            if (ui.players != L.pill_n[k]) { started[cur] = 0; jr_touch(cur); }   /* (a game in progress: left) */
            nplayers[cur] = ui.players = L.pill_n[k];
            remember_game(cur);
            bots_update();
            seats_view();
            if (L.nseats) layout();             /* as many seats as play */
            return;
        }
        for (int k = 0; k < L.nseats; k++) {    /* a seat: a person, the bot easy, medium, hard, a person again */
            if (!in_box(L.seats[k], x, y)) continue;
            snd_play(SND_TAP);
            started[cur] = 0;                   /* (a game in progress: left) */
            jr_touch(cur);
            seat_lv[cur][k] = (uint8_t)((seat_lv[cur][k] + 1) % (2 + AI_HARD));
            remember_game(cur);
            bots_update();
            seats_view();
            return;
        }
        if (L.online.w > 0 && in_box(L.online, x, y)) {   /* online play: only the note (no seat changes) */
            snd_play(SND_TAP);
            pages_soon();
            return;
        }
        if (screen == SCR_MENU && L.ntabs && menu.tmax > 0 && in_box(L.tab_row, x, y)) {   /* the row that scrolls:
                                                                                         * a tab on release */
            menu.tgrab = 1;
            menu.tmoved = menu.tvel != 0;       /* a tap that stops a fling chooses nothing */
            menu.tvel = 0;
            menu.tgoal = -1;
            menu.tgrab_x = x;
            menu.tgrab_tx = menu.tx;
            menu.tt = SDL_GetTicks();
            return;
        }
        for (int k = 0; k < L.ntabs && screen == SCR_MENU; k++) {
            box b = L.tabs[k];
            if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
            snd_play(SND_TAP);
            menu_tab(L.tab_sec[k]);
            return;
        }
        if (screen == SCR_MENU) {               /* a card opens on release, unless the finger scrolled the list */
            menu.grabbing = y >= menu.top && in_box(pages_room(), x, y);
            menu.moved = menu.vel != 0;         /* a tap that stops a fling opens nothing */
            menu.axis = 0;
            menu.vel = 0;
            menu.grab_x = x;
            menu.grab_y = y;
            menu.grab_scroll = menu.scroll;
            menu.t = SDL_GetTicks();
            return;
        }
        for (int k = 0; k < LV.nch && games[cur]->levels; k++)   /* random play's picker: back to a chapter */
            if (in_box(LV.tabs[k], x, y)) {
                snd_play(SND_TAP);
                LV.chapter = k;
                screen = SCR_LEVELS;
                layout();
                return;
            }
        if (axes_on(cur)) {                     /* two axes: a size chosen, or a level started at that size */
            int ns = games[cur]->axes[0], nl = games[cur]->axes[1];
            for (int i = 0; i < ns + nl; i++) {
                if (!in_box(L.cards[i], x, y)) continue;
                snd_play(SND_TAP);
                if (i < ns) {
                    pick_size[cur] = i;
                    return;
                }
                if (games[cur]->levels) rnd_mode[cur] = 1;   /* random play */
                open_game(cur, pick_size[cur] * nl + i - ns);
                return;
            }
            return;
        }
        int n = games[cur]->variants;
        for (int i = 0; i < n; i++) {
            box c = L.cards[i];
            if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) {
                snd_play(SND_TAP);
                if (games[cur]->levels) rnd_mode[cur] = 1;   /* random play */
                open_game(cur, i);
                return;
            }
        }
        return;
    }
    if (header_hit(x, y, HD.bx)) { snd_play(SND_TAP); game_back(); return; }   /* the header: back, the rules */
    if (header_hit(x, y, HD.ix)) { snd_play(SND_TAP); open_help(cur); return; }
    if (is_view() && hand.up) {                 /* the cover: start (the view is the player's) */
        cover_layout();
        if (in_circle(x, y, C.sx, C.sy, C.sr)) { snd_play(SND_TAP); uncover(); }
        return;
    }
    for (int i = 0; i < NBTN; i++) {
        if (!shown(i) || !in_circle(x, y, L.bx[i], L.by[i], L.br)) continue;
        if (i == B_HOME) { snd_play(SND_TAP); go_home(); }
        else if (i == B_UNDO && ui_handing()) snd_play(SND_DENY);   /* the turn has passed: no going back */
        else if (i == B_UNDO) { snd_play(undo_human() ? SND_TAP : SND_DENY); over_hidden = 0; }
        else if (i == B_FLIP) { snd_play(SND_TAP); ui.flipped = !ui.flipped; layout(); jr_view(); }
        else if (i == B_F2F) { snd_play(SND_TAP); ui.f2f = !ui.f2f; jr_view(); }
        else if (i == B_HELP) { snd_play(SND_TAP); open_help(cur); }
        else if (games[cur]->status() == GS_PLAY && !armed && games[cur]->turn() >= 0 && started[cur]) {
            armed = SDL_GetTicks();
            snd_play(SND_TAP);
        } else { snd_play(SND_TAP); new_game(); }
        last_status = screen == SCR_GAME ? games[cur]->status() : GS_PLAY;
        return;
    }
    if (ui_handing()) return;                   /* the device is changing hands */
    if (press_bar(x, y)) return;
    int solved = solved_card();
    if (games[cur]->status() != GS_PLAY && (!over_hidden || solved)) {
        if (next_btn.d > 0 && ui_button_hit(next_btn.x, next_btn.y, next_btn.d, x, y)) {   /* a solved puzzle's next */
            snd_play(SND_TAP);
            card_next();
            return;
        }
        if (replay_btn.d > 0 && ui_button_hit(replay_btn.x, replay_btn.y, replay_btn.d, x, y)) {   /* the same again */
            snd_play(SND_TAP);
            card_replay();
            return;
        }
        for (int k = 0; k < 3 && solved; k++)   /* the stars card's links: a review, a problem, support */
            if (link_box[k].w > 0 && in_box(link_box[k], x, y)) {
                snd_play(SND_TAP);
                pages_link_open(k);
                PT.stray = 1;
                return;
            }
        if (!solved || ui_tool_hit(x, y) < 0) { /* the end card: a tap hides it; the stars card stays (only its */
            if (!solved) over_hidden = 1;       /* buttons, undo or the game's tools) */
            PT.stray = 1;                       /* the rest of this tap: nobody's */
            return;
        }
    }
    if (bot_holds()) {                          /* a bot's turn: the game's input waits (the finger's whole way) */
        held_press = 1;
        return;
    }
    PT.game = 1;                                /* the game's, to its lifting */
    PT.live = games[cur]->status() == GS_PLAY;
    PT.x = x;
    PT.y = y;
    game_press(x, y);
}

/* a press that is the game's: a bar's (its own use of it), else the board's (kept in the game's record) */
static void game_press(float x, float y)
{
    x = jr_q(x), y = jr_q(y);
    struct jrec *jr = jr_point(EV_PRESS, x, y);
    for (int p = 0; p < ui.players && games[cur]->bar_press; p++) {   /* the game's own use of a bar */
        box b = L.bar[p];
        if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
        float fx = x, fy = y;                   /* in the frame bar() drew in */
        if (ui_upside(p) && t_bartex) { fx = bar_m + b.w - (x - b.x); fy = bar_m + b.h - (y - b.y); }
        if (games[cur]->bar_press(p, fx, fy)) { jr_done(jr); return; }
    }
    bot_last = -1;                              /* a person acted: the next bot's move is a new turn */
    games[cur]->press(x, y);                    /* (solved by it, the finger still down: its gesture goes on, the
                                                 * end waits for it to lift: gesture_held) */
    jr_done(jr);
}

static void motion(float x, float y)
{
    if (FV.open) return;
    if (help.open) {
        if (help.grabbing) help.scroll = fminf(help_max(), fmaxf(0, help.grab_scroll - (y - help.grab_y)));
        return;
    }
    if (screen == SCR_MENU && page != PAGE_GAMES) {
        pages_motion(x, y);
        return;
    }
    if (screen == SCR_MENU && menu.tgrab) {     /* the tabs' row dragged sideways */
        if (!menu.tmoved && fabsf(x - menu.tgrab_x) > fminf(ui.w, ui.h) * 0.02f) menu.tmoved = 1;
        if (!menu.tmoved) return;
        float s = clampf(menu.tgrab_tx - (x - menu.tgrab_x), 0, menu.tmax);
        Uint32 t = SDL_GetTicks();
        if (t > menu.tt) menu.tvel = 0.5f * menu.tvel + 0.5f * (s - menu.tx) / (float)(t - menu.tt);
        menu.tt = t;
        menu.tx = s;
        return;
    }
    if (screen == SCR_MENU && menu.grabbing) {
        if (!menu.moved && hypotf(x - menu.grab_x, y - menu.grab_y) > fminf(ui.w, ui.h) * 0.02f) {
            menu.moved = 1;
            menu.axis = fabsf(x - menu.grab_x) > fabsf(y - menu.grab_y) ? 1 : 2;
        }
        if (!menu.moved || menu.axis == 1) return;   /* sideways: another tab, on release */
        float s = fminf(menu.max, fmaxf(0, menu.grab_scroll - (y - menu.grab_y)));
        Uint32 t = SDL_GetTicks();
        if (t > menu.t) menu.vel = 0.5f * menu.vel + 0.5f * (s - menu.scroll) / (float)(t - menu.t);
        menu.t = t;
        menu.scroll = s;
        return;
    }
    if (screen == SCR_LEVELS && LV.grabbing && hypotf(x - LV.grab_x, y - LV.grab_y) > fminf(ui.w, ui.h) * 0.02f)
        LV.moved = 1;
    if (screen != SCR_GAME || (is_view() && hand.up) || held_press) return;
    /* a stray finger: nobody's; a puzzle solved: only the gesture the game had then (it goes on to its lifting: the pin
     * placed where the player wants it), or a tool's press */
    if (PT.stray || (solved_card() && !PT.game)) return;
    x = jr_q(x), y = jr_q(y);
    struct jrec *jr = PT.game ? jr_point(EV_MOTION, x, y) : NULL;   /* (a mouse hovering: not kept) */
    games[cur]->motion(x, y);
    jr_done(jr);
    if (!PT.game) return;
    PT.x = x;
    PT.y = y;
}

static void release(float x, float y)
{
    int mine = PT.game, stray = PT.stray;      /* the pointer's: the game's, or nobody's */
    PT.game = PT.stray = 0;
    if (FV.open) {
        menu.grabbing = menu.tgrab = LV.grabbing = held_press = 0;
        return;
    }
    if (help.open) {
        help.grabbing = 0;
        return;
    }
    if (screen == SCR_LEVELS) {
        if (LV.grabbing) levels_release(x, y);
        return;
    }
    if (screen == SCR_MENU && menu.tgrab) {     /* the tabs' row: a tap chooses a tab, a drag flies on if quick */
        menu.tgrab = 0;
        if (menu.tmoved) {
            if (SDL_GetTicks() - menu.tt > 80) menu.tvel = 0;
            menu.tt = SDL_GetTicks();
            need_frame = 1;
            return;
        }
        for (int k = 0; k < L.ntabs && page == PAGE_GAMES; k++) {
            box b = L.tabs[k];
            b.x -= menu.tx;
            if (!in_box(b, x, y) || !in_box(L.tab_row, x, y)) continue;
            snd_play(SND_TAP);
            menu_tab(L.tab_sec[k]);
            tabs_show(0);                       /* a tab half out of view: all of it */
            return;
        }
        return;
    }
    if (screen == SCR_MENU && pages_release(x, y)) return;
    if (screen == SCR_MENU && menu.grabbing) {
        menu.grabbing = 0;
        if (menu.moved) {                       /* the end of a drag: the list flies on if the finger was quick */
            if (menu.axis == 1) {               /* a swipe: the next tab that way */
                if (fabsf(x - menu.grab_x) > fminf(ui.w, ui.h) * 0.12f) menu_step(x < menu.grab_x ? 1 : -1);
                return;
            }
            if (SDL_GetTicks() - menu.t > 80) menu.vel = 0;
            menu.t = SDL_GetTicks();
            need_frame = 1;
            return;
        }
        for (int i = 0; i < ngames; i++) {
            box c = L.cards[i];
            c.y -= menu.scroll;
            if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) {
                snd_play(SND_TAP);
                if (locked(i)) full_open();     /* the free edition: the full version's */
                else if (arcade_of(games[i]) >= 0) {   /* (not in the picture mode: its own loop would never end) */
                    SDL_StopTextInput();        /* (a search's keyboard: put away, as for a board game) */
                    if (!instant) arcade_play(arcade_cards[arcade_of(games[i])].id, ui.r);
                }
                else open_game(i, -1);
                return;
            }
        }
        return;
    }
    if (held_press) held_press = 0;           /* a finger put down on a bot's turn: none of it the game's */
    else if (stray || (!mine && solved_card())) return;   /* down when the game ended, or on its card: nobody's */
    else if (screen == SCR_GAME && !(is_view() && hand.up)) {
        x = jr_q(x), y = jr_q(y);
        struct jrec *jr = jr_point(EV_RELEASE, x, y);
        games[cur]->release(x, y);
        jr_done(jr);
    }
}

static void menu_wheel(float dy)
{
    if (page != PAGE_GAMES) {
        pages_wheel(dy);
        return;
    }
    menu.vel = 0;
    menu.scroll = fminf(menu.max, fmaxf(0, menu.scroll + dy));
}

/* ---- several fingers at once (a game's finger(): slaps) -------------------------------------------------------- */
static int fingers_on(void)
{
    return screen == SCR_GAME && games[cur]->finger && !help.open && !(is_view() && hand.up);
}

/* a finger's id taken out of a list of n; 1: it was in it */
static int id_drop(long long *ids, int *n, long long id)
{
    for (int k = 0; k < *n; k++)
        if (ids[k] == id) {
            ids[k] = ids[--*n];
            return 1;
        }
    return 0;
}

static int id_in(const long long *ids, int n, long long id)
{
    for (int k = 0; k < n; k++)
        if (ids[k] == id) return 1;
    return 0;
}

/* the game ended under the fingers it holds (an end card shown, a puzzle solved): stray from now on, to their lifting */
static void fingers_stray(void)
{
    while (PT.nheld > 0) {
        long long id = PT.held[--PT.nheld];
        if (PT.nstrays < MAX_FINGERS) PT.strays[PT.nstrays++] = id;
    }
}

/* a finger: on the shell's buttons or the bars it is an ordinary tap (press), and on the end card (its buttons; a tap
 * hides an end card, never a puzzle's stars card); elsewhere the game's, with its id. A finger down when the game
 * ended is nobody's to its lifting. */
static void touch(Uint32 type, long long id, float x, float y)
{
    static long long shell_id[MAX_FINGERS];
    static int nshell;
    if (type == SDL_FINGERDOWN) {
        id_drop(shell_id, &nshell, id);         /* a finger of that id before has lifted (its lifting unseen) */
        id_drop(PT.held, &PT.nheld, id);
        id_drop(PT.strays, &PT.nstrays, id);
        int hit = header_hit(x, y, HD.bx) || header_hit(x, y, HD.ix);
        for (int i = 0; i < NBTN && !hit; i++) hit = shown(i) && in_circle(x, y, L.bx[i], L.by[i], L.br);
        for (int p = 0; p < ui.players && !hit; p++)
            hit = x >= L.bar[p].x && y >= L.bar[p].y && x < L.bar[p].x + L.bar[p].w && y < L.bar[p].y + L.bar[p].h;
        int ended = !hit && screen == SCR_GAME && games[cur]->status() != GS_PLAY;
        if (ended && PT.nheld > 0) {            /* ended under the game's fingers still down: theirs goes on, a new */
            if (PT.nstrays < MAX_FINGERS) PT.strays[PT.nstrays++] = id;   /* finger is nobody's */
            return;
        }
        if (ended && (!over_hidden || solved_card())) hit = 1;   /* the end card: as press() takes it */
        if (hit) {
            if (nshell < MAX_FINGERS) shell_id[nshell++] = id;
            press(x, y);
            return;
        }
    }
    for (int k = 0; k < nshell; k++)            /* a finger that began on a button: the shell's to its end */
        if (shell_id[k] == id) {
            if (type == SDL_FINGERUP) shell_id[k] = shell_id[--nshell];
            return;
        }
    if (type == SDL_FINGERUP ? id_drop(PT.strays, &PT.nstrays, id) : id_in(PT.strays, PT.nstrays, id)) return;
    if (!fingers_on()) return;                  /* the button left the game */
    int phase = type == SDL_FINGERDOWN ? FINGER_DOWN : type == SDL_FINGERUP ? FINGER_UP : FINGER_MOVE;
    if (phase == FINGER_DOWN && PT.nheld < MAX_FINGERS) PT.held[PT.nheld++] = id;
    if (phase == FINGER_UP) id_drop(PT.held, &PT.nheld, id);
    x = jr_q(x), y = jr_q(y);
    struct jrec *jr = jr_point(EV_FINGER, x, y);
    if (jr) { jr_u32(jr, (uint32_t)(id & 0x7fffffff)); jr_u8(jr, (unsigned)phase); jr_soon |= phase == FINGER_UP; }
    games[cur]->finger((int)(id & 0x7fffffff), phase, x, y);   /* (ended under them: they go on, gesture_held) */
    jr_done(jr);
}

/* ---- the pages' hooks (pages.h) ------------------------------------------------------------------------------------ */
void shell_page_changed(void)
{
    if (pages_search()) menu.scroll = menu.vel = 0;   /* the search's words: its finds from the top */
    if (screen == SCR_MENU) layout();
    need_frame = 1;
}

void shell_set_mute(int muted)
{
    snd_muted = muted;
    remember_mute();
}

void shell_text_set(const char *code)
{
    int was = WK.parked;
    worker_park();                              /* no picture painted while the fonts change */
    text_init(code);
    if (!was) worker_unpark();
}

void shell_text_changed(void)
{
    text_gen++;
    for (int i = 0; i < MAX_GAMES; i++) ui_free(&t_name[i]);
    for (int s = 0; s < SECTIONS; s++) ui_free(&t_sec[s]);
    FV.made = 0;
    if (help.open) {
        help_compose();
        help_free();
        help_layout();
    }
    if (ui.r) layout();
    need_frame = 1;
}

void shell_theme_changed(void)
{
    for (int s = 0; s < SECTIONS; s++) ui_free(&t_sec[s]);
    for (int i = 0; i < MAX_GAMES; i++) ui_free(&t_name[i]);   /* the cards' names: the theme's ink */
    help_free();
    if (help.open) help_layout();
    frame_for(cur);                             /* (a puzzle's frame: light maple in the light theme) */
    if (ui.r && made_unit > 0) layout();       /* (laid out before) the chrome's pictures: they know their theme */
    need_frame = 1;
}

void shell_open_text(const char *key)
{
    help_key = key;
    open_help(-2);
}

static int levels_kept(int g, uint8_t *b)       /* a game's levels' record (shell/levels/<id>): how many kept */
{
    char k[96];
    snprintf(k, sizeof k, "shell/levels/%s", games[g]->id);
    return ui_save_get(k, b, nlevels(g));
}

void shell_progress(int sec, int *done, int *total)
{
    static uint8_t b[MAX_LEVELS];
    *done = *total = 0;
    for (int g = 0; g < ngames; g++) {
        if (section_of(games[g]) != sec) continue;
        if (is_puzzle_sec(sec) && games[g]->levels) {   /* a puzzle's levels: the ones solved */
            int n = levels_kept(g, b);
            for (int l = 0; l < n; l++) *done += b[l] != 0;
            *total += nlevels(g);
        } else {                                /* a game: won (solved) once */
            *done += stats_won(games[g]->id);
            *total += 1;
        }
    }
}

int shell_three_stars(void)
{
    static uint8_t b[MAX_LEVELS];
    int n3 = 0;
    for (int g = 0; g < ngames; g++) {
        if (!games[g]->levels) continue;
        int n = levels_kept(g, b);
        for (int l = 0; l < n; l++) n3 += b[l] == 4;
    }
    return n3;
}

int shell_sections(int *out)
{
    int n = 0;
    for (int r = 0; r < SECTIONS; r++)
        for (int g = 0, s = SEC_SHOWN[r]; g < ngames; g++)
            if (section_of(games[g]) == s) { out[n++] = s; break; }
    return n;
}

void shell_tab_icon(int s, int d, uint8_t *o)
{
    tab_icon(s, d, o);
    if (s == SEC_GUESS || is_puzzle_sec(s))     /* the white ones: in the words' colour */
        for (int i = 0; i < d * d; i++) memcpy(o + 4 * i, TH.text, 3);
}

void shell_open_url(const char *url)
{
    if (!url || !*url) return;
    if (shot_clock) {
        fprintf(stderr, "url: %s\n", url);
        return;
    }
    if (SDL_OpenURL(url) != 0) SDL_Log("cannot open %s: %s", url, SDL_GetError());
}

/* --stats-demo: progress made up for the pictures: a level, a streak, games won, levels solved */
static void stats_seed(void)
{
    const char *ids[MAX_GAMES];
    int n = 0;
    for (int g = 0; g < ngames; g++)
        if (g % 3 == 0 && !games[g]->levels) ids[n++] = games[g]->id;
    stats_demo(ids, n);
    static uint8_t b[MAX_LEVELS];
    for (int g = 0; g < ngames; g++) {
        if (!games[g]->levels) continue;
        int k = nlevels(g) < 14 ? nlevels(g) : 14;
        for (int l = 0; l < k; l++) b[l] = (uint8_t)(l < 9 ? 4 : 2);
        char key[96];
        snprintf(key, sizeof key, "shell/levels/%s", games[g]->id);
        ui_save_set(key, b, k);
    }
}

/* ---- the picture mode -------------------------------------------------------------------------------------------- */
static int find_game(const char *id)
{
    for (int i = 0; i < ngames; i++)
        if (!strcmp(games[i]->id, id)) return i;
    return -1;
}

/* VIEW_* games: the clock on past the last tap (it has landed), the hand-off done; reveal: the cover lifted too (the
 * start button tapped) */
static void shot_settle(int reveal, int advance)
{
    if (advance) ms_ahead += 600000;
    handoff_settle();
    if (reveal && is_view() && hand.up) uncover();
}

/* before a gesture: the game's animations over (a deal, a slide, the dice), so the finger is not ignored */
static void shot_wait(void)
{
    const game *g = games[cur];
    for (int k = 0; k < 64 && ((g->busy && g->busy()) || ui_dice_busy()); k++) {   /* (a long chain: many moves) */
        ms_ahead += 600000;
        draw();
    }
}

/* the picture mode's gestures, in the order given: --taps "fx,fy ...", --drag "fx0,fy0 fx1,fy1" (the finger down,
 * moved, lifted), --hold "fx,fy ms" (the finger down that long; the last gesture: the picture with it still down),
 * --slide "fx0,fy0 fx1,fy1 fx2,fy2 ..." (the finger down, through each point in turn; lifted at the last, unless it is
 * the last gesture: the picture with it still down, the clock on to rest). Fractions of the board's box (the menu, a
 * picker, the level screen: of the screen); the game's tools lie outside it (portrait: fy past 1; landscape: fx past
 * 1): a tap "t0" taps tool 0 wherever it is. --keys "Up Left": keys pressed (SDL's names), as from the keyboard. */
enum { GE_TAP, GE_DRAG, GE_HOLD, GE_KEY, GE_SLIDE };
static void key(SDL_Keycode k, int repeat);
#define MAX_GESTURES 16
static struct { int kind; const char *s; } gesture[MAX_GESTURES];
static int ngestures;

/* base: the clock before the last gesture; held: the last gesture's hold (ms), the finger still down (else -1) */
static void shot_gestures(int reveal, Uint32 *base, int *held)
{
    int first = 1;
    for (int k = 0; k < ngestures; k++) {
        for (const char *s = gesture[k].s; s && *s; first = 0) {
            float fx, fy, gx = 0, gy = 0;
            int n = 0, ms = 0, kind = gesture[k].kind;
            if (kind == GE_KEY) {               /* a key: its name up to the next space */
                char name[32];
                if (sscanf(s, "%31s%n", name, &n) < 1) break;
                s += n;
                SDL_Keycode kc = SDL_GetKeyFromName(name);
                if (kc == SDLK_UNKNOWN) { fprintf(stderr, "no key %s\n", name); break; }
                if (screen == SCR_GAME) shot_wait();
                *base = ms_ahead;
                key(kc, 0);
                after_input();
                while (*s == ' ') s++;
                continue;
            }
            int tool = -1;                      /* "t2": the game's tool 2 (ui_tool), wherever the shell put it */
            if (kind == GE_TAP && *s == 't' && sscanf(s, "t%d%n", &tool, &n) == 1) fx = fy = 0;
            else if (sscanf(s, "%f,%f%n", &fx, &fy, &n) < 2) break;
            s += n;
            if (kind == GE_DRAG && sscanf(s, " %f,%f%n", &gx, &gy, &n) < 2) break;
            if (kind == GE_HOLD && sscanf(s, " %d%n", &ms, &n) < 1) break;
            if (kind == GE_DRAG || kind == GE_HOLD) s += n;
            if (!first && is_view()) shot_settle(reveal, 1);   /* the last tap has landed, the device passed */
            if (screen == SCR_GAME) shot_wait();
            *base = ms_ahead;
            box f = screen == SCR_GAME ? L.board : (box){0, 0, (float)ui.w, (float)ui.h};
            float x = f.x + fx * f.w, y = f.y + fy * f.h;
            if (tool >= 0 && !ui_tool_at(tool, &x, &y, NULL)) { fprintf(stderr, "no tool %d\n", tool); break; }
            press(x, y);
            if (kind == GE_DRAG) {              /* the finger slides there in steps, then lifts */
                float x1 = f.x + gx * f.w, y1 = f.y + gy * f.h;
                for (int i = 1; i <= 8; i++) motion(x + (x1 - x) * i / 8, y + (y1 - y) * i / 8);
                x = x1;
                y = y1;
            } else if (kind == GE_HOLD) {       /* held down: the clock moves on meanwhile */
                ms_ahead += (Uint32)(ms > 0 ? ms : 0);
                draw();
                while (*s == ' ') s++;
                *held = k == ngestures - 1 && !*s ? ms : -1;
                if (*held >= 0) return;         /* the last gesture: the picture with the finger down */
            } else if (kind == GE_SLIDE) {      /* through each point in turn, a frame at each */
                while (sscanf(s, " %f,%f%n", &gx, &gy, &n) == 2) {
                    s += n;
                    float x1 = f.x + gx * f.w, y1 = f.y + gy * f.h;
                    for (int i = 1; i <= 8; i++) motion(x + (x1 - x) * i / 8, y + (y1 - y) * i / 8);
                    x = x1;
                    y = y1;
                    draw();
                }
                if (k == ngestures - 1) return; /* the last gesture: the picture with the finger still down */
            }
            release(x, y);
            after_input();
            while (*s == ' ') s++;
        }
    }
}

/* --solved "1-12 13-14:2 15:0": levels 1 to 12 solved with 3 stars, 13 and 14 with 2, 15 with none; 0: wrong */
static int shot_solved(const char *s)
{
    if (!games[cur]->levels) return 0;
    progress_load(cur);
    for (;;) {
        int a, b, k = 3, n = 0;
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (sscanf(s, "%d%n", &a, &n) < 1) return 0;
        s += n;
        b = a;
        if (*s == '-') {
            if (sscanf(s + 1, "%d%n", &b, &n) < 1) return 0;
            s += 1 + n;
        }
        if (*s == ':') {
            if (sscanf(s + 1, "%d%n", &k, &n) < 1) return 0;
            s += 1 + n;
        }
        if (a < 1 || b > nlevels(cur) || a > b || k < 0 || k > 3) return 0;
        for (int l = a - 1; l < b; l++) LV.best[l] = (uint8_t)(1 + k);
    }
    progress_save();
    return 1;
}

/* --bots "1:h,2:m" (or "all:e"): the device plays those seats (players from 0), easy, medium or hard; the others are
 * people. 0: wrong (or the game has no bot). */
static int shot_seats(const char *s)
{
    if (!has_bot(cur)) return 0;
    memset(seat_lv[cur], 0, MAX_PLAYERS);
    for (;;) {
        int p = -1, n = 0;
        while (*s == ' ' || *s == ',') s++;
        if (!*s) return 1;
        if (!strncmp(s, "all:", 4)) {
            p = MAX_PLAYERS;
            s += 4;
        } else if (sscanf(s, "%d:%n", &p, &n) == 1 && n > 0) {
            s += n;
        } else {
            return 0;
        }
        int lv = *s == 'e' ? 1 + AI_EASY : *s == 'm' ? 1 + AI_MEDIUM : *s == 'h' ? 1 + AI_HARD : 0;
        if (!lv || p < 0 || p > MAX_PLAYERS) return 0;
        s++;
        for (int q = 0; q < MAX_PLAYERS; q++)
            if (q == p || p == MAX_PLAYERS) seat_lv[cur][q] = (uint8_t)lv;
    }
}

/* --steps n: the bots play n moves, each as in the app (the worker, the bot's play(); the pause left out), the board
 * at rest between them, hand-offs done (--reveal: the cover lifted). It stops sooner when a person must play, the
 * game is over or nothing moves; it says so on stderr, with each move and its time. base: the clock when the last
 * move was played (--at ms catches its animation). */
static void shot_bots(int steps, int reveal, Uint32 *base)
{
    int start = BT.played, idle = 0, taps = BT.taps;
    Uint32 t0 = SDL_GetTicks();
    while (BT.played - start < steps) {
        if (is_view()) shot_settle(reveal, 0);
        shot_wait();
        int before = BT.played, was = bot_thinking(), tapped = BT.taps;
        Uint32 now = ms_ahead;
        bot_step();
        if (!was && bot_thinking()) t0 = SDL_GetTicks();
        if (BT.taps > tapped && BT.taps - taps <= steps) {   /* watch mode's tap (a score sheet): on to the next */
            *base = now;
            idle = 0;
            draw();
            continue;
        }
        if (BT.played > before) {
            *base = now;
            idle = 0;
            draw();                             /* what draw() starts (a deal, a slide) starts now */
            continue;
        }
        if (bot_thinking() || BT.held) {
            idle = 0;
            if (SDL_GetTicks() - t0 > 60000) {
                fprintf(stderr, "bots: seat %d has thought for a minute: stopped\n", BT.seat);
                bot_stop();
                break;
            }
            SDL_Delay(1);
            continue;
        }
        if (++idle > 2) break;                  /* nothing to do: the board's animations run on, then look again */
        ms_ahead += 600000;
        draw();
    }
    const game *g = games[cur];
    int a = g->status() == GS_PLAY ? bot_actor() : -1;
    fprintf(stderr, "bots: %d moves played, %d taps; status %d, to act: %d (%s)%s%s\n", BT.played - start,
            BT.taps - taps, g->status(), a,
            a < 0 ? "nobody" : is_bot(a) ? "a bot" : "a person", g->busy && g->busy() ? ", busy" : "",
            ui_handing() ? ", handing" : "");
}

/* --icons: every icon of the shell on a button, in rows of six, as games show them (plain, then in gold light, then
 * on the gold disc) */
static void draw_icons(void)
{
    ui_put(t_bg, 0, 0, (float)ui.w, (float)ui.h);
    float d = fminf(ui.w / 8.0f, ui.h / 18.0f), step = 1.3f * d;
    static const int how[3] = {0, UI_LIT, UI_ON};
    int rows = (ICONS + 5) / 6;
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < ICONS; i++) {
            float x = ui.w / 2.0f + (i % 6 - 2.5f) * step, y = d + (k * (rows + 0.5f) + i / 6) * step;
            ui_button(x, y, d, i, how[k]);
        }
}

static int shot(int argc, char **argv)
{
    const char *out = NULL, *id = NULL, *moves = NULL, *lang = NULL, *save_dir = NULL, *solved = NULL, *seats = NULL;
    int v = 0, keep = 0, pick = 0, show_help = 0, np = 0, tab = -1, reveal = 0, level = 0, rnd = 0, icons = 0;
    float scroll = 0;
    uint32_t seed = 1;
    int at = -1;                                /* --at ms: the picture that long after the last tap (else at rest) */
    int steps = 0, think = 0;                   /* --steps n: the bots play n moves; --think: one starts */
    const char *pg = NULL, *theme = NULL, *search = NULL;   /* --page, --theme, --search: the app's pages */
    int demo = 0, tips = 0;                     /* --stats-demo, --tips-demo */
    int resume = 0;                             /* --resume: the game kept in --save DIR, played again */
    int notation = 0;                           /* --notation: the moves' notation the shell kept, printed */
    int held = -1;                              /* held: the last gesture's hold (ms), the finger still down */
    Uint32 base = 0;                            /* the clock before the last tap */
    ui.w = 720;
    ui.h = 1280;
    shot_clock = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &ui.w, &ui.h);
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) id = argv[++i];
        else if (!strcmp(argv[i], "--variant") && i + 1 < argc) v = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--moves") && i + 1 < argc) moves = argv[++i];
        else if ((!strcmp(argv[i], "--taps") || !strcmp(argv[i], "--drag") || !strcmp(argv[i], "--hold") ||
                  !strcmp(argv[i], "--keys") || !strcmp(argv[i], "--slide")) &&
                 i + 1 < argc && ngestures < MAX_GESTURES) {
            char c = argv[i][2];
            gesture[ngestures].kind = c == 't' ? GE_TAP : c == 'd' ? GE_DRAG : c == 'h' ? GE_HOLD : c == 's' ? GE_SLIDE
                                                                                                         : GE_KEY;
            gesture[ngestures++].s = argv[++i];
        }
        else if (!strcmp(argv[i], "--flip")) ui.flipped = 1;
        else if (!strcmp(argv[i], "--f2f")) ui.f2f = 1;
        else if (!strcmp(argv[i], "--keep")) keep = 1;
        else if (!strcmp(argv[i], "--pick")) pick = 1;
        else if (!strcmp(argv[i], "--lang") && i + 1 < argc) lang = argv[++i];
        else if (!strcmp(argv[i], "--help")) show_help = 1;
        else if (!strcmp(argv[i], "--scroll") && i + 1 < argc) scroll = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--players") && i + 1 < argc) np = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--tab") && i + 1 < argc) tab = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--at") && i + 1 < argc) at = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--reveal")) reveal = 1;
        else if (!strcmp(argv[i], "--resume")) resume = 1;
        else if (!strcmp(argv[i], "--notation")) notation = 1;
        else if (!strcmp(argv[i], "--random")) rnd = 1;
        else if (!strcmp(argv[i], "--icons")) icons = 1;
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save_dir = argv[++i];
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--solved") && i + 1 < argc) solved = argv[++i];
        else if (!strcmp(argv[i], "--bots") && i + 1 < argc) seats = argv[++i];
        else if (!strcmp(argv[i], "--steps") && i + 1 < argc) steps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--think")) think = 1;
        else if (!strcmp(argv[i], "--page") && i + 1 < argc) pg = argv[++i];
        else if (!strcmp(argv[i], "--theme") && i + 1 < argc) theme = argv[++i];
        else if (!strcmp(argv[i], "--search") && i + 1 < argc) search = argv[++i];
        else if (!strcmp(argv[i], "--stats-demo")) demo = 1;
        else if (!strcmp(argv[i], "--tips-demo")) tips = 1;
    }
    seed_random(seed);                          /* the same dice every time: tests and screenshots repeat */
    text_init(lang ? lang : "en");
    instant = 1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");     /* smooth scaling, as in the app */
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, ui.w, ui.h, 32, SDL_PIXELFORMAT_RGBA32);
    ui.r = SDL_CreateSoftwareRenderer(surf);
    if (!ui.r) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    screen = SCR_MENU;
    variants_first();
    if (save_dir) {                             /* else nothing kept from a past run: the same picture every time */
        save_open(save_dir);
        settings_load();
    }
    stats_set_today(stats_day(2026, 10, 16));   /* a Friday: the same week in every picture */
    pages_init();
    if (theme) pages_set_theme(!strcmp(theme, "light") ? 1 : !strcmp(theme, "dark") ? 2 : 0);
    if (demo) stats_seed();
    if (tips) pages_tips_demo();
    if (id) {
        cur = find_game(id);
        if (cur < 0) { fprintf(stderr, "no game %s\n", id); return 1; }
        if (locked(cur)) { fprintf(stderr, "%s is locked in %s\n", id, ed_app_name); return 1; }
        if (arcade_of(games[cur]) >= 0)         /* an Arcade game (--at ms, --pad "A@60"): its own picture */
            return arcade_shot(arcade_cards[arcade_of(games[cur])].id, ui.r, surf, out, argc, argv);
        if (np) nplayers[cur] = np;             /* --players n: how many play */
        ui.players = players_of(cur);
        if (solved && !shot_solved(solved)) { fprintf(stderr, "bad --solved\n"); return 1; }
        if (seats && !shot_seats(seats)) { fprintf(stderr, "bad --bots (or no bot for %s)\n", id); return 1; }
        bots_update();                          /* (the picker shows the seats; a game starts with them) */
        if (pick) {                             /* the picker, or the level screen (--tab n: its chapter n, 0..) */
            show_picker(cur);
            if (screen == SCR_LEVELS && tab >= 0 && tab == LV.nch && has_random(cur)) random_tab();
            else if (screen == SCR_LEVELS && tab >= 0) {
                LV.chapter = tab;
                layout();
            }
        } else {
            if (level > nlevels(cur)) { fprintf(stderr, "no level %d\n", level); return 1; }
            if (level > 0 && level_locked(level - 1)) {
                fprintf(stderr, "level %d is locked in %s\n", level, ed_app_name);
                return 1;
            }
            if (rnd && !has_random(cur)) { fprintf(stderr, "no random play\n"); return 1; }
            rnd_mode[cur] = rnd;                /* --random: a game of levels at random (--variant n) */
            open_game(cur, resume ? -1 : level > 0 ? level - 1 : v);   /* --level n: level n (1..); --resume: the game
                                                                        * kept in --save DIR */
            if (resume) base = ms_ahead;
            struct jrec *jr = moves ? jr_move(EV_SCRIPT, 255, moves) : NULL;   /* (kept: --save DIR) */
            int bad = moves && (!games[cur]->script || !games[cur]->script(moves));
            jr_done(jr);
            if (bad) {
                fprintf(stderr, "bad moves\n");
                return 1;
            }
            draw();                             /* once before the taps, as the app always has: the bars' layout */
            if (reveal) shot_settle(1, 0);
            if (steps > 0) shot_bots(steps, reveal, &base);   /* --steps n: the bots' moves */
        }
    } else {
        layout();
        if (tab >= 0) menu_tab(tab);           /* --tab n: that section's tab (SEC_*: 0 puzzles .. 7 logic) */
        if (pg) pages_show(!strcmp(pg, "stats") ? PAGE_STATS : !strcmp(pg, "settings") ? PAGE_SETTINGS : PAGE_GAMES);
        if (search) pages_search_set(search);   /* --search "words": the Games page's search */
        if (!show_help && page != PAGE_GAMES) pages_scroll(scroll);   /* --scroll px: further down */
        else if (!show_help) menu_wheel(scroll);
    }
    shot_gestures(reveal, &base, &held);
    if (id) over_hidden = keep;
    /* the gestures' animations: over, or caught --at ms after the last one began (a hold: as long as it was held) */
    ms_ahead = base + (at >= 0 ? (Uint32)at : held >= 0 ? (Uint32)held : 600000);
    if (id && !pick && reveal) shot_settle(1, 0);
    if (id && !pick && screen == SCR_GAME && !icons) {   /* a frame first: what draw() starts (a deal as the cover
                                                          * lifts) starts now; at rest: it lands too */
        draw();
        if (at < 0 && held < 0) {
            ms_ahead += 600000;
            if (reveal) shot_settle(1, 0);
        }
    }
    if (think && id && !pick) bot_step();       /* --think: the next bot starts thinking (its sign in its bar) */
    if (show_help) {                            /* the rules page (the game's, or the app's), scrolled by --scroll px */
        open_help(id ? cur : -1);
        help_scroll(scroll);
    }
    if (icons) draw_icons();                    /* --icons: every icon of the shell, on its buttons */
    else draw();
    SDL_RenderPresent(ui.r);
    if (!out || SDL_SaveBMP(surf, out)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    int st = id && started[cur] ? games[cur]->status() : -1;   /* -1: the menu, or a game not started (--pick) */
    printf("%s: %dx%d %s status %d\n", out, ui.w, ui.h, id ? id : "menu", st);
    if (notation && id && started[cur]) {       /* "notation: e2e4 e7e5": --moves of it plays the same game again */
        notes_check();
        printf("notation:");
        for (int i = 0; i < NT[cur].n; i++) printf(" %s", NT[cur].s + NT[cur].at[i]);
        printf("\n");
    }
    bot_stop();
    jr_flush();
    save_flush();                               /* --save DIR: written back (else nothing to write) */
    return 0;
}

/* ---- --perf: the frames timed (a developer's measure: what a phone, ~6x slower, would stall on) ------------------ */

/* one frame drawn (its clock 16 ms on): its time, ms; *tex: the textures it made */
static int perf_pace;                           /* --perf: frames 16.7 ms apart, as the screen's */
static double perf_frame(int *tex)
{
    perf_tex = 0;
    double t0 = perf_now();
    bot_step();
    draw();
    SDL_RenderPresent(ui.r);
    double t = perf_now() - t0;
    if (tex) *tex = perf_tex;
    ms_ahead += 16;
    if (perf_pace)                              /* a screen's frame: the helper threads paint meanwhile */
        while (perf_now() - t0 < 16.7) SDL_Delay(1);
    return t;
}

typedef struct { double first, max, sum; int n, slow, tex; } perf_run_t;

static void perf_add(perf_run_t *r, double t, int tex)
{
    if (!r->n) r->first = t;
    if (r->n && t > r->max) r->max = t;          /* the worst after the first */
    r->sum += t;
    r->n++;
    if (r->n > 1 && t > 16.0 / 6) r->slow++;     /* over a phone's frame */
    if (r->n > 1) r->tex += tex;
}

static void perf_print(const char *what, const perf_run_t *r)
{
    printf("%-34s first %8.2f ms  then avg %6.2f  max %7.2f  slow %3d/%-3d  textures/frame %.2f\n", what, r->first,
           r->n > 1 ? (r->sum - r->first) / (r->n - 1) : 0, r->max, r->slow, r->n - 1,
           r->n > 1 ? (double)r->tex / (r->n - 1) : 0);
}

/* --perf menu: the first frame, each tab's first frames, a fling down each tab; the cards' pictures, each game's.
 * --perf GAME [--variant n] [--players n] [--pick]: the game's first frame and 3 s of frames (its picker's). */
static int perf(int argc, char **argv)
{
    const char *what = "menu", *lang = "en";
    int v = 0, np = 0, pick = 0, frames = 180;
    ui.w = 720;
    ui.h = 1280;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--perf") && i + 1 < argc) what = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &ui.w, &ui.h);
        else if (!strcmp(argv[i], "--variant") && i + 1 < argc) v = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--players") && i + 1 < argc) np = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--lang") && i + 1 < argc) lang = argv[++i];
        else if (!strcmp(argv[i], "--pick")) pick = 1;
    }
    seed_random(1);
    text_init(lang);
    shot_clock = 1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    /* a small surface: the blits are clipped away (a phone's GPU does them), the painting and the uploads stay */
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, 16, 16, 32, SDL_PIXELFORMAT_RGBA32);
    if (getenv("RSB_PERF_GL")) {                /* or a hidden window's GPU renderer, no vsync (as a phone) */
        if (SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
        win = SDL_CreateWindow("perf", 0, 0, ui.w, ui.h, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
        ui.r = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED) : NULL;
        SDL_RendererInfo ri;
        if (ui.r && !SDL_GetRendererInfo(ui.r, &ri)) printf("renderer %s\n", ri.name);
    } else
        ui.r = SDL_CreateSoftwareRenderer(surf);
    if (!ui.r) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    screen = SCR_MENU;
    char name[96];
    if (!strcmp(what, "cards")) {               /* --perf cards: every deck's faces painted, at a hand's width */
        for (int d = 0; d < DECKS; d++) {
            static const int PACK_OF[DECKS] = {PACK_54, PACK_TAROT, PACK_NUMBER, PACK_TILES, PACK_ACTION, PACK_40,
                                               PACK_ES48};
            uint8_t pack[CARDS_MAX];
            int n = cards_pack(PACK_OF[d], pack);
            int w = 110, h = (int)(w * cards_aspect(d) + 0.5f);
            uint8_t *b = malloc((size_t)w * h * 4);
            double all = 0, most = 0;
            int most_c = 0;
            for (int i = 0; i < n + 1; i++) {
                int c = i < n ? pack[i] : CARD_NONE;
                double t0 = perf_now();
                cards_paint(d, c, w, h, b);
                double t = perf_now() - t0;
                all += t;
                if (t > most) most = t, most_c = c;
            }
            printf("deck %d: %d bytes at %dx%d: %.1f ms, %.2f a card, the most %.2f (card %d)\n", d, n + 1, w, h, all,
                   all / (n + 1), most, most_c);
            free(b);
        }
        return 0;
    }
    if (!strcmp(what, "dump")) {                /* --perf dump DIR: every game's pictures as raw RGBA (comparisons) */
        const char *dir = NULL;
        for (int i = 1; i + 2 < argc; i++)
            if (!strcmp(argv[i], "--perf")) dir = argv[i + 2];
        if (!dir) { fprintf(stderr, "--perf dump DIR\n"); return 1; }
        static const int sizes[] = {48, 168, 251};
        double t0 = perf_now();
        for (int i = 0; i < ngames; i++)
            for (int z = 0; z < 3; z++) {
                int sz = sizes[z];
                frame_for(i);
                uint8_t *b = calloc((size_t)sz * sz, 4);
                char fn[512];
                for (int k = -1 - ART_PLAYERS; k < MAX_VARIANTS + 8; k++) {
                    int lv = k >= MAX_VARIANTS;         /* past the variants: a few levels' pictures */
                    if (lv && (!games[i]->level_thumb || (k - MAX_VARIANTS) * 7 >= games[i]->levels)) continue;
                    if (!lv && k >= games[i]->variants) continue;
                    memset(b, 0, (size_t)sz * sz * 4);
                    if (k < -1) games[i]->token(-2 - k, sz, b);
                    else if (lv) games[i]->level_thumb((k - MAX_VARIANTS) * 7, sz, b);
                    else games[i]->thumb(k, sz, b);
                    snprintf(fn, sizeof fn, "%s/%s_%d_%d.rgba", dir, games[i]->id, k, sz);
                    FILE *fp = fopen(fn, "wb");
                    if (fp) {
                        fwrite(b, 4, (size_t)sz * sz, fp);
                        fclose(fp);
                    }
                }
                free(b);
            }
        printf("dumped in %.0f ms\n", perf_now() - t0);
        return 0;
    }
    if (!strcmp(what, "menu") || !strcmp(what, "menu-warm")) {
        /* menu: no time between (every picture waited for); menu-warm: the idle loop's pictures painted ahead first.
         * --worker: the app's helper thread paints the menu's pictures, the frames 16.7 ms apart */
        for (int i = 1; i < argc; i++)
            if (!strcmp(argv[i], "--worker")) perf_pace = worker_start();
        for (int i = 1; i < argc; i++)          /* --started: every game begun first (the menu reads their state) */
            if (!strcmp(argv[i], "--started"))
                for (int g = 0; g < ngames; g++)
                    if (!locked(g)) {
                        open_game(g, 0);
                        go_home();
                    }
        perf_run_t r = {0};
        double t0 = perf_now();
        layout();
        double tl = perf_now() - t0;
        int tex = 0;
        perf_add(&r, tl + perf_frame(&tex), tex);
        snprintf(name, sizeof name, "menu: first frame (tab %d)", menu.tab);
        perf_print(name, &r);
        if (!strcmp(what, "menu-warm")) {
            int n = 0;
            double most = 0, all = 0, start = perf_now();
            for (;;) {
                t0 = perf_now();
                if (!paint_ahead()) {
                    if (!WK.on) break;
                    SDL_LockMutex(WK.m);
                    int left = WK.nwant || WK.doing >= 0;
                    SDL_UnlockMutex(WK.m);
                    if (!left) break;
                    SDL_Delay(1);               /* the helper still painting */
                    continue;
                }
                double t = perf_now() - t0;
                all += t;
                most = t > most ? t : most;
                n++;
            }
            printf("idle: %d pictures painted ahead, %.1f ms of the main thread's in all, the longest %.2f; %.0f ms "
                   "until all were\n", n, all, most, perf_now() - start);
        }
        int ntab = L.ntabs, secs[SECTIONS];
        memcpy(secs, L.tab_sec, sizeof secs);
        double tabs = 0;
        for (int k = 0; k < ntab; k++) {        /* each tab: its first frames, then a fling to its bottom */
            perf_run_t t = {0};
            t0 = perf_now();
            menu_tab(secs[k]);
            tl = perf_now() - t0;
            perf_add(&t, tl + perf_frame(&tex), tex);
            tabs += t.first;
            int f = 0;
            for (; f < 240 && (paint_n || menu_missing || f < 3); f++) perf_add(&t, perf_frame(&tex), tex);
            snprintf(name, sizeof name, "tab %d: switch (%d frames)", secs[k], f);
            perf_print(name, &t);
            perf_run_t s = {0};
            perf_add(&s, 0, 0);
            float step = (L.card + L.label) / 20.0f;   /* a row of cards in 20 frames (a third of a second) */
            for (int f = 0; f < 4000 && menu.scroll < menu.max; f++) {
                menu_wheel(step);
                perf_add(&s, perf_frame(&tex), tex);
            }
            snprintf(name, sizeof name, "tab %d: scroll down", secs[k]);
            perf_print(name, &s);
        }
        printf("all tabs' first frames: %.1f ms\n", tabs);
        /* the cards' pictures, each game's, at the menu's size (painted afresh) */
        int sz = (int)(L.card * 0.86f);
        uint8_t *b = calloc((size_t)sz * sz, 4);
        double all = 0, most = 0;
        for (int i = 0; i < ngames; i++) {
            memset(b, 0, (size_t)sz * sz * 4);
            t0 = perf_now();
            frame_for(i);
            games[i]->thumb(-1, sz, b);
            double t = perf_now() - t0;
            all += t;
            if (t > most) most = t;
            printf("thumb %-16s %7.2f ms\n", games[i]->id, t);
        }
        printf("thumbs: %d games at %d px: %.1f ms in all, %.2f a card on average, %.2f the most\n", ngames, sz, all,
               all / ngames, most);
        free(b);
        return 0;
    }
    if (!strcmp(what, "starts")) {              /* --perf starts [--only "id id"]: every game (those) opened from the
                                                 * menu, its first frames */
        const char *only = NULL;
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "--only") && i + 1 < argc) only = argv[i + 1];
            if (!strcmp(argv[i], "--pace")) perf_pace = 1;   /* frames 16.7 ms apart: the helper threads work */
        }
        layout();
        draw();
        for (int g = 0; g < ngames; g++) {
            if (locked(g)) continue;
            if (only) {
                const char *p = strstr(only, games[g]->id);
                size_t l = strlen(games[g]->id);
                if (!p || (p > only && p[-1] != ' ') || (p[l] && p[l] != ' ')) continue;
            }
            int tex = 0;
            double t0 = perf_now();
            double tpl = 0;
            if (needs_picker(g)) {              /* its picker (or level screen) first, as a tap on its card */
                open_game(g, -1);
                tpl = perf_now() - t0;
                perf_frame(&tex);
            }
            double tp = perf_now() - t0;
            if (getenv("RSB_PERF_PICKER")) printf("picker %s: layout %.2f, first frame %.2f\n", games[g]->id, tpl,
                                                  tp - tpl);
            t0 = perf_now();
            open_game(g, 0);
            double tl = perf_now() - t0, f1 = perf_frame(&tex), f2 = 0, worst = 0;
            if (is_view() && hand.up) {
                uncover();
                f2 = perf_frame(&tex);
            }
            int slow = 0;
            for (int f = 0; f < 120; f++) {     /* two seconds: a deal, the cards' slides */
                double t = perf_frame(&tex);
                worst = t > worst ? t : worst;
                slow += t > 16.0 / 6;
            }
            printf("start %-16s picker %6.2f  start %6.2f  first frame %6.2f  cover lifted %6.2f  then worst %6.2f  "
                   "slow %3d  total %6.2f\n", games[g]->id, tp, tl, f1, f2, worst, slow, tl + f1 + f2);
            go_home();
            draw();
        }
        return 0;
    }
    cur = find_game(what);
    if (cur < 0) { fprintf(stderr, "no game %s\n", what); return 1; }
    if (np) nplayers[cur] = np;
    ui.players = players_of(cur);
    layout();
    draw();
    perf_run_t r = {0};
    int tex = 0;
    double t0 = perf_now();
    if (pick) show_picker(cur);
    else open_game(cur, v);
    double tl = perf_now() - t0;
    perf_add(&r, tl + perf_frame(&tex), tex);
    printf("(start and layout %.2f ms)\n", tl);
    if (is_view() && hand.up) {                 /* the cover: lifted, its frame timed apart */
        perf_run_t u = {0};
        uncover();
        perf_add(&u, perf_frame(&tex), tex);
        perf_print("the cover lifted", &u);
    }
    for (int f = 0; f < frames; f++) perf_add(&r, perf_frame(&tex), tex);
    snprintf(name, sizeof name, "%s%s v%d", what, pick ? " picker" : "", v);
    perf_print(name, &r);
    bot_stop();
    return 0;
}

/* RSB_FRAMES=1: each second, on stderr, the frames drawn and the longest draw() (ms, the screen's wait apart), and
 * when the menu's helper has painted every picture (a developer's measure of the app itself) */
static void frames_log(double ms)
{
    static int on = -1, n, idle;
    static double most;
    static Uint32 t0;
    if (on < 0) on = getenv("RSB_FRAMES") != NULL;
    if (!on) return;
    n++;
    most = ms > most ? ms : most;
    Uint32 now = SDL_GetTicks();
    if (WK.on) {
        SDL_LockMutex(WK.m);
        int left = WK.nwant || WK.doing >= 0;
        SDL_UnlockMutex(WK.m);
        if (!left && !idle) fprintf(stderr, "frames: the helper has painted all it was asked at %u ms\n", now);
        idle = !left;
    }
    if (now - t0 < 1000) return;
    fprintf(stderr, "frames: %d in %u ms, the longest draw %.2f ms (screen %d)\n", n, now - t0, most, screen);
    t0 = now;
    n = 0;
    most = 0;
}

/* ---- the program ------------------------------------------------------------------------------------------------- */
#ifdef __IPHONEOS__
/* iPhone and iPad: the screen's safe area (apps/ios/common/ios_app.h: clear of the notch, the Dynamic Island, the
 * rounded corners and the home indicator), in pixels. The shell lays out and draws in it (ui.w x ui.h, the renderer's
 * viewport), the table's green reaching every edge round it; touches and the mouse are moved into it. */
static SDL_Rect safe;
static int scr_w, scr_h, away;                  /* the whole screen, pixels; away: the app in the background */

static void safe_update(void)
{
    SDL_GetRendererOutputSize(ui.r, &scr_w, &scr_h);
    ios_safe_area(win, scr_w, scr_h, &safe);
}

/* before a frame: the table round the safe area (its gradient's first and last rows above and below, the same rows
 * beside it), then the viewport */
static void safe_frame(void)
{
    SDL_RenderSetViewport(ui.r, NULL);
    if (screen == SCR_MENU) {                   /* the pages: their ground, the ribbon's colour below (or left) */
        SDL_SetRenderDrawColor(ui.r, TH.bg[0], TH.bg[1], TH.bg[2], 255);
        SDL_RenderClear(ui.r);
        SDL_SetRenderDrawColor(ui.r, TH.nav[0], TH.nav[1], TH.nav[2], 255);
        SDL_Rect r = safe.w > safe.h ? (SDL_Rect){0, 0, safe.x, scr_h}
                                     : (SDL_Rect){0, safe.y + safe.h, scr_w, scr_h - safe.y - safe.h};
        SDL_RenderFillRect(ui.r, &r);
    } else if (t_bg) {
        SDL_Rect top = {0, 0, 2, 1}, bottom = {0, 255, 2, 1};
        SDL_Rect r = {0, 0, scr_w, safe.y};
        SDL_RenderCopy(ui.r, t_bg, &top, &r);
        r = (SDL_Rect){0, safe.y, scr_w, safe.h};
        SDL_RenderCopy(ui.r, t_bg, NULL, &r);
        r = (SDL_Rect){0, safe.y + safe.h, scr_w, scr_h - safe.y - safe.h};
        SDL_RenderCopy(ui.r, t_bg, &bottom, &r);
    }
    SDL_RenderSetViewport(ui.r, &safe);
}
#endif

static void to_pixels(int x, int y, float *fx, float *fy)
{
    int ww, wh;
    SDL_GetWindowSize(win, &ww, &wh);
#ifdef __IPHONEOS__
    *fx = x * (float)scr_w / (ww ? ww : 1) - safe.x;
    *fy = y * (float)scr_h / (wh ? wh : 1) - safe.y;
#else
    *fx = x * (float)ui.w / (ww ? ww : 1);
    *fy = y * (float)ui.h / (wh ? wh : 1);
#endif
}

/* repeat: the key held down (only the shell's scrolling and undo repeat; a game gets a key once) */
static void key(SDL_Keycode k, int repeat)
{
    if (screen == SCR_MENU && !FV.open && !help.open && pages_key(k)) return;   /* the pages' (the search's keys) */
    if (screen != SCR_MENU && pages_panel_key(k)) return;   /* the tip panel over a solved puzzle's card */
    if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) {
        if (FV.open) FV.open = 0;
        else if (help.open) help.open = 0;
        else if (screen == SCR_MENU) quit = 1;
        else if (screen == SCR_GAME) game_back();   /* a game: its new game screen; from there the games */
        else go_home();
        return;
    }
    if (FV.open) return;                        /* the full version's panel: only Back */
    if (k == SDLK_m) {
        snd_muted = !snd_muted;
        remember_mute();
        return;
    }
    if (k == SDLK_h || k == SDLK_F1) { if (!help.open) open_help(screen == SCR_GAME ? cur : -1); return; }
    if (screen == SCR_MENU && !help.open && page == PAGE_GAMES && (k == SDLK_LEFT || k == SDLK_RIGHT)) {   /* tabs */
        menu_step(k == SDLK_RIGHT ? 1 : -1);
        return;
    }
    if (screen == SCR_PICK && !help.open && k == SDLK_LEFT && games[cur]->levels) {   /* random play: the chapters */
        LV.chapter = LV.nch - 1;
        screen = SCR_LEVELS;
        layout();
        return;
    }
    if (screen == SCR_LEVELS && !help.open && (k == SDLK_LEFT || k == SDLK_RIGHT)) {   /* the chapters */
        levels_step(k == SDLK_RIGHT ? 1 : -1);
        return;
    }
    if (screen == SCR_MENU && !help.open &&
        (k == SDLK_DOWN || k == SDLK_UP || k == SDLK_PAGEDOWN || k == SDLK_PAGEUP)) {
        float step = (k == SDLK_PAGEDOWN || k == SDLK_PAGEUP ? 3 : 1) * (L.card + L.label) * 0.5f;
        menu_wheel(k == SDLK_DOWN || k == SDLK_PAGEDOWN ? step : -step);
        return;
    }
    if (screen != SCR_GAME || help.open) return;
    if (k == SDLK_u || k == SDLK_BACKSPACE) { if (!ui_handing()) undo_human(); }
    else if (k == SDLK_f && shown(B_FLIP)) { ui.flipped = !ui.flipped; layout(); jr_view(); }
    else if (k == SDLK_t && shown(B_F2F)) { ui.f2f = !ui.f2f; jr_view(); }
    else if (k == SDLK_n) new_game();
    else if (k != SDLK_f && k != SDLK_t && games[cur]->key && !repeat && !(is_view() && hand.up) && !ui_handing() &&
             !bot_holds() && !solved_card()) {
        struct jrec *jr = jr_event(EV_KEY);
        if (jr) jr_u32(jr, (uint32_t)k);
        games[cur]->key(k);                     /* the game's own keys (the arrows: a push; none once solved) */
        jr_done(jr);
    }
}

/* ---- a kept game played again (the record: see "the game in progress kept") ------------------------------------- */
static uint32_t jr_rd(const uint8_t *b, int n, int *p, int bytes)   /* little-endian; past the end: 0 */
{
    uint32_t v = 0;
    for (int i = 0; i < bytes; i++, ++*p) v |= (uint32_t)(*p < n ? b[*p] : 0) << 8 * i;
    return v;
}

static uint32_t jr_rdvar(const uint8_t *b, int n, int *p)
{
    uint32_t v = 0;
    for (int s = 0; *p < n && s < 35; s += 7) {
        uint8_t c = b[(*p)++];
        v |= (uint32_t)(c & 127) << s;
        if (!(c & 128)) break;
    }
    return v;
}

static int rp_hidden;                           /* the record played again is at an EV_AWAY: nothing drawn */
static int rp_draws, rp_pos, rp_len, rp_w, rp_h; /* the frames drawn; how far in the record; the screen */
static Uint32 rp_shown;                         /* SDL_GetTicks() at the last frame shown meanwhile */

/* a long record played again (not the picture mode): every 100 ms a frame on the screen, the table and a thin bar
 * of how far it has gone (the app is not frozen) */
static void rp_progress(void)
{
    if (instant || !rp_sink || SDL_GetTicks() - rp_shown < 100) return;
    rp_shown = SDL_GetTicks();
    SDL_SetRenderTarget(ui.r, NULL);
    ui_put(t_bg, 0, 0, (float)rp_w, (float)rp_h);
    float s = (float)(rp_w < rp_h ? rp_w : rp_h), bw = floorf(0.4f * s), bh = fmaxf(3, roundf(0.012f * s));
    float x = floorf((rp_w - bw) / 2), y = floorf((rp_h - bh) / 2);
    const Uint8 *c = TH.dark ? (const Uint8[3]){246, 238, 220} : (const Uint8[3]){84, 56, 36};
    ui_fill(x, y, bw, bh, c[0], c[1], c[2], 50);
    ui_fill(x, y, roundf(bw * (float)rp_pos / (float)(rp_len > 0 ? rp_len : 1)), bh, c[0], c[1], c[2], 200);
    SDL_RenderPresent(ui.r);
    SDL_PumpEvents();
    SDL_SetRenderTarget(ui.r, rp_sink);
}

static void rp_draw(void)                       /* a frame of the record played again: drawn to the sink */
{
    if (rp_sink) SDL_SetRenderTarget(ui.r, rp_sink);
    draw();
    rp_draws++;
    rp_progress();
}

/* the clock on to `to`, as the frames went meanwhile: drawn every 50 ms while the game is busy (its animations land,
 * what draw() starts starts, the hand-off comes), then at `to` itself if `last` (a frame there); the game off the
 * screen (rp_hidden): nothing drawn */
static void jr_advance(Uint32 to, int last)
{
    const game *g = games[cur];
    if (rp_hidden) { rp_now = to; return; }
    for (int k = 0; rp_now != to; k++) {
        rp_draw();
        int busy = (g->busy && g->busy()) || ui_dice_busy();
        Uint32 left = to - rp_now;
        rp_now += !busy || k >= 400 || left < 50 ? left : 50;
    }
    if (last) rp_draw();
}

/* game g (open, cur) played again from its record in the store; 0: none (or not this game's) */
static int jr_resume(int g)
{
    if (!JR_ON || !games[g]->start || ui.w <= 0) return 0;
    uint8_t *b = NULL;
    int n = 0;
    for (int k = 0; k < JR_PIECES; k++) {       /* its pieces, in order */
        char kn[96];
        jr_key(g, k, kn, sizeof kn);
        int len = ui_save_get(kn, NULL, 0);
        if (len <= 0) break;
        uint8_t *m = realloc(b, (size_t)(n + len));
        if (!m) break;
        b = m;
        n += ui_save_get(kn, b + n, len);
        if (len < JR_PIECE) break;
    }
    int p = 3;
    if (n < JR_HEAD || b[0] != 'R' || b[1] != 'J' || (b[2] < 1 || b[2] > JR_VERSION)) { free(b); return 0; }
    int frames = b[2] >= 3;                     /* the frames recorded: drawn only there (else at each event) */
    int v = (int)jr_rd(b, n, &p, 2), rnd = b[p++], np = b[p++];
    uint8_t seats[MAX_PLAYERS];
    memcpy(seats, b + p, MAX_PLAYERS);
    p += MAX_PLAYERS;
    uint32_t r0 = jr_rd(b, n, &p, 4);
    int w = (int)jr_rd(b, n, &p, 2), h = (int)jr_rd(b, n, &p, 2), fl = b[p++];
    int nv = rnd || !games[g]->levels ? (games[g]->axes[0] > 0 ? games[g]->axes[0] * games[g]->axes[1]
                                                               : games[g]->variants) : nlevels(g);
    if (np < 1 || np > MAX_PLAYERS || !(player_counts(g) >> np & 1) || (rnd && !has_random(g)) || v < 0 ||
        v >= (nv > 0 ? nv : 1) || w <= 0 || h <= 0) {
        free(b);
        return 0;
    }
    /* the game set up as it began: its players, seats, variant (or level) */
    nplayers[g] = ui.players = np;
    memcpy(seat_lv[g], seats, MAX_PLAYERS);
    rnd_mode[g] = rnd;
    if (rnd) rvariant[g] = v;
    else variant[g] = v;
    chosen[g] = 1;
    remember_game(g);
    bots_update();
    seats_view();
    int sw = ui.w, sh = ui.h, muted = snd_muted, was = screen;
    jr_quiet = 1;
    snd_muted = 1;
    rp_now = ui_ms() - (instant ? 0 : 0x40000000u);   /* long ago: back on the real clock, every animation has landed
                                                 * (the picture mode: its clock goes on as the first run's went) */
    rp_on = 1;
    start_rng[g] = r0;
    rp_hidden = rp_draws = 0;
    rp_pos = p;
    rp_len = n;
    rp_w = sw;
    rp_h = sh;
    rp_real = 0;
    rp_sink = SDL_CreateTexture(ui.r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 16, 16);
    if (rp_sink && SDL_SetRenderTarget(ui.r, rp_sink)) { SDL_DestroyTexture(rp_sink); rp_sink = NULL; }
    Uint32 t_real = rp_shown = SDL_GetTicks();
    rp_over = fl >> 2 & 1;                      /* begun on the end card: start() sees it (logicgates' next) */
    start_game(v, 1);
    rp_over = 0;
    awarded[g] = 1;                             /* (an end passed on the way, then taken back: not counted again) */
    view_start(1);
    screen = SCR_GAME;
    over_hidden = 0;
    ui.w = w;
    ui.h = h;
    ui.flipped = fl & 1;
    ui.f2f = fl >> 1 & 1;
    layout();
    last_status = games[g]->status();
    const game *gm = games[g];
    Uint32 t = rp_now;
    int events = 0;
    while (p < n) {
        int type = b[p++];
        t += jr_rdvar(b, n, &p);
        rp_pos = p;
        if (type == EV_FRAME) {                 /* the last frame before the next event: there */
            jr_advance(t, 1);
            continue;
        }
        if (type != EV_VIEW && type != EV_RNG && type != EV_NOTE) {
            if (frames) {                       /* no frame since the last: none drawn */
                rp_now = t;
                if (!rp_hidden) handoff_settle();   /* (input came: no hand-off was waiting; the picture mode
                                                     * settles one without a frame) */
            } else jr_advance(t, 1);
        }
        float x = 0, y = 0;
        if (type == EV_PRESS || type == EV_MOTION || type == EV_RELEASE || type == EV_FINGER) {
            x = (float)jr_rd(b, n, &p, 2) / 4;
            y = (float)jr_rd(b, n, &p, 2) / 4;
        }
        if (type == EV_PRESS) game_press(x, y);
        else if (type == EV_MOTION) gm->motion(x, y);
        else if (type == EV_RELEASE) gm->release(x, y);
        else if (type == EV_FINGER) {
            int id = (int)jr_rd(b, n, &p, 4), phase = b[p < n ? p : n - 1];
            p++;
            if (gm->finger) gm->finger(id, phase, x, y);
        } else if (type == EV_KEY) {
            SDL_Keycode k = (SDL_Keycode)jr_rd(b, n, &p, 4);
            if (gm->key) gm->key(k);
        } else if (type == EV_PASS) {
            if (gm->pass) gm->pass();
        } else if (type == EV_UNDO) undo_human();
        else if (type == EV_UNCOVER) uncover();
        else if (type == EV_AWAY) rp_hidden = 1;   /* off the screen: nothing drawn until it comes back */
        else if (type == EV_BACK) {
            rp_hidden = 0;
            rp_real += 3600000;                 /* ui_real_ms(): a gap (the game was away) */
        }
        else if (type == EV_SCRIPT || type == EV_BOT) {
            int seat = (int)jr_rd(b, n, &p, 1), len = (int)jr_rd(b, n, &p, 2);
            char *mv = p + len <= n ? malloc((size_t)len + 1) : NULL;
            if (!mv) break;
            memcpy(mv, b + p, (size_t)len);
            mv[len] = 0;
            p += len;
            if (type == EV_SCRIPT) {
                if (gm->script) gm->script(mv);
            } else {
                bot_moving = seat;
                if (cbot && cbot->play) cbot->play(mv);
                else if (gm->script) gm->script(mv);
                bot_moving = -1;
                bot_last = seat;
            }
            free(mv);
        } else if (type == EV_VIEW) {
            ui.w = (int)jr_rd(b, n, &p, 2);
            ui.h = (int)jr_rd(b, n, &p, 2);
            int f = p < n ? b[p] : 0;
            p++;
            ui.flipped = f & 1;
            ui.f2f = f >> 1 & 1;
            layout();
        } else if (type == EV_RNG) rng = jr_rd(b, n, &p, 4);
        else if (type == EV_NOTE) {             /* the moves' notation: the list rebuilt, nothing played */
            int keep = (int)jr_rdvar(b, n, &p), k = (int)jr_rdvar(b, n, &p);
            nt_cut(&NT[g], keep);
            for (int i = 0; i < k && p < n; i++) {
                char m[256];
                int len = b[p++];
                if (len > n - p) len = n - p;
                memcpy(m, b + p, (size_t)len);
                m[len] = 0;
                p += len;
                nt_add(&NT[g], m);
            }
            continue;
        } else break;                           /* (not a record of ours: played as far as it went) */
        after_input();
        events++;
    }
    int away = rp_hidden;                       /* (left off the screen: on it again now, EV_BACK) */
    rp_hidden = 0;
    if (!instant) jr_advance(t + 600000, 1);    /* the last move landed */
    rp_on = 0;
    if (rp_sink) {
        SDL_SetRenderTarget(ui.r, NULL);
        SDL_DestroyTexture(rp_sink);
        rp_sink = NULL;
    }
    if (rp_shown != t_real) {                   /* shown meanwhile: the taps then are nobody's */
        SDL_FlushEvents(SDL_MOUSEMOTION, SDL_MOUSEWHEEL);
        SDL_FlushEvents(SDL_FINGERDOWN, SDL_FINGERMOTION);
    }
    need_frame = 1;
    if (instant) ms_ahead = t - 1000;           /* (shot_clock's: ui_ms() goes on from the last event) */
    jr_quiet = 0;
    snd_muted = muted;
    awarded[g] = 0;
    struct jrec *r = &J[g];                     /* the record goes on from here */
    free(r->b);
    r->b = b;
    r->n = r->cap = n;
    r->ok = 1;
    r->kept = (n + JR_PIECE - 1) / JR_PIECE;
    r->last = ui_ms();
    r->rng = rng;
    r->w = ui.w;
    r->h = ui.h;
    r->flipped = ui.flipped;
    r->f2f = ui.f2f;
    r->frames = frames_drawn;
    r->away = away;
    r->back = 0;
    ui.w = sw;
    ui.h = sh;
    if (is_view()) view_of[g] = hand.pending >= 0 ? hand.pending : hand.up ? hand.player : ui.viewer;
    hand.pending = -1;
    screen = was;
    if (instant)
        fprintf(stderr, "resume: %s, %d events played again, status %d (%d frames, %u ms)\n", gm->id, events,
                gm->status(), rp_draws, SDL_GetTicks() - t_real);
    return 1;
}

int main(int argc, char **argv)
{
    qsort(games, (size_t)ngames, sizeof games[0], by_order);
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--shot")) return shot(argc, argv);
        else if (!strcmp(argv[i], "--perf")) return perf(argc, argv);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    /* every way round: with a resizable window SDL would otherwise lock Android to portrait (the manifest ignored) */
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    const char *save_dir = NULL;                /* --save DIR: the settings and progress kept there */
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--save")) save_dir = argv[i + 1];
    save_open(save_dir);
    const char *lang = pages_lang_kept();       /* the language chosen in Settings, else the device's */
    for (int i = 1; i + 1 < argc; i++)          /* --lang fr: another language */
        if (!strcmp(argv[i], "--lang")) lang = argv[i + 1];
    text_init(lang);
    unlock_start();
    variants_first();
    settings_load();
    seed_random((uint32_t)SDL_GetPerformanceCounter());
#ifdef __IPHONEOS__
    SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "ambient");   /* the sounds follow the silent switch */
#endif
    snd_init();
#ifdef __IPHONEOS__                             /* borderless: no status bar */
    win = SDL_CreateWindow(ed_app_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 540, 900,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_BORDERLESS);
#else
    win = SDL_CreateWindow(ed_app_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 540, 900,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
#endif
    ui.r = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : NULL;
    if (win && !ui.r) ui.r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!win || !ui.r) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    SDL_StopTextInput();                        /* (on until the search wants it: no keyboard) */
    pages_init();                               /* the theme, the screen kept on, the stats */
    screen = SCR_MENU;
    worker_start();                             /* the menu's pictures: a thread of their own */
    while (!quit) {
        SDL_Event e;
        int busy = need_frame || armed;
        need_frame = 0;
        if (!busy) {                            /* idle: a picture painted ahead, while no event waits */
            SDL_PumpEvents();
#if SDL_VERSION_ATLEAST(2, 0, 18)                  /* not the poll cycle's own sentinel */
            int waiting = SDL_HasEvents(SDL_FIRSTEVENT, SDL_POLLSENTINEL - 1) ||
                          SDL_HasEvents(SDL_POLLSENTINEL + 1, SDL_LASTEVENT);
#else
            int waiting = SDL_HasEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
#endif
            if (!waiting && paint_ahead()) {
                if (!need_frame) continue;      /* not on the screen: nothing to draw again */
                busy = 1;
                need_frame = 0;
            }
        }
        if (busy ? SDL_PollEvent(&e) : SDL_WaitEventTimeout(&e, 250)) {
            do {
                float fx, fy;
                switch (e.type) {
                case SDL_QUIT: quit = 1; break;
                case SDL_APP_WILLENTERBACKGROUND:     /* the bot thinking stops (it starts again on return) */
                case SDL_APP_TERMINATING:
                    bot_stop();
                    if (screen == SCR_GAME) jr_away(1);   /* (its record: the time away not the game's) */
                    jr_flush();
                    save_flush();
                    PT.game = 0;                /* a finger down then is lost: an end waiting for it shows */
                    fingers_stray();
                    break;
#ifdef __IPHONEOS__                             /* nothing drawn in the background */
                case SDL_APP_DIDENTERBACKGROUND: away = 1; break;
                case SDL_APP_WILLENTERFOREGROUND: away = 0; break;
#endif
                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION:
                case SDL_FINGERUP:
                    if (fingers_on()) {
#ifdef __IPHONEOS__
                        touch(e.type, (long long)e.tfinger.fingerId, e.tfinger.x * scr_w - safe.x,
                              e.tfinger.y * scr_h - safe.y);
#else
                        touch(e.type, (long long)e.tfinger.fingerId, e.tfinger.x * ui.w, e.tfinger.y * ui.h);
#endif
                        after_input();
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button != SDL_BUTTON_LEFT) break;
                    if (e.button.which == SDL_TOUCH_MOUSEID && fingers_on()) break;   /* the fingers say it */
                    to_pixels(e.button.x, e.button.y, &fx, &fy);
                    press(fx, fy);
                    after_input();
                    break;
                case SDL_MOUSEMOTION:
                    if (e.motion.which == SDL_TOUCH_MOUSEID && fingers_on()) break;
                    to_pixels(e.motion.x, e.motion.y, &fx, &fy);
                    motion(fx, fy);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.button != SDL_BUTTON_LEFT) break;
                    if (e.button.which == SDL_TOUCH_MOUSEID && fingers_on()) break;
                    to_pixels(e.button.x, e.button.y, &fx, &fy);
                    release(fx, fy);
                    after_input();
                    break;
                case SDL_KEYDOWN:
                    key(e.key.keysym.sym, e.key.repeat);
                    after_input();
                    break;
                case SDL_TEXTINPUT:             /* typed: the Games page's search */
                    if (screen == SCR_MENU) pages_text(e.text.text);
                    break;
                case SDL_APP_DIDENTERFOREGROUND:   /* the device's theme may have changed meanwhile */
                    if (screen == SCR_GAME) jr_away(0);
                    pages_foreground();
                    break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) pages_foreground();
                    if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {   /* (the record kept now: closed next?) */
                        jr_flush();
                        save_flush();
                    }
                    break;
                case SDL_MOUSEWHEEL: {
                    int wx, wy;
                    float mx, my;
                    SDL_GetMouseState(&wx, &wy);
                    to_pixels(wx, wy, &mx, &my);
                    if (help.open) help_scroll(-e.wheel.y * help.px * 3);
                    else if (screen == SCR_MENU && page == PAGE_GAMES && L.ntabs && menu.tmax > 0 &&
                             (e.wheel.x || in_box(L.tab_row, mx, my)))   /* over the tabs (or sideways): the row */
                        tabs_scroll((e.wheel.x - e.wheel.y) * L.tab_row.h);
                    else if (screen == SCR_MENU) menu_wheel(-e.wheel.y * (L.card + L.label) * 0.4f);
                    break;
                }
                }
            } while (SDL_PollEvent(&e));
        }
        bot_step();                             /* a bot's turn: it starts, or its move is played */
        int w, h;
        SDL_GetRendererOutputSize(ui.r, &w, &h);
#ifdef __IPHONEOS__
        if (away) continue;
        safe_update();                          /* the shell's screen: the safe area */
        w = safe.w;
        h = safe.h;
#endif
        if (w != ui.w || h != ui.h) {
            ui.w = w;
            ui.h = h;
            layout();
            if (help.open) help_layout();
        }
        unlock_poll();                          /* the in-app purchase: bought, restored, its price */
#ifdef __IPHONEOS__
        safe_frame();
#endif
        double t0 = perf_now();
        draw();
        frames_log(perf_now() - t0);
        SDL_RenderPresent(ui.r);
        jr_flush_due();                         /* the record: at a release, else once a second */
        save_flush();                           /* what changed, written at once (nothing: no write) */
    }
    jr_flush();
    save_flush();
    bot_stop();
    worker_park();                              /* no picture half painted */
    SDL_Quit();
    return 0;
}
