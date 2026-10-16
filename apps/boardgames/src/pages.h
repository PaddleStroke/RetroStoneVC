/*
 * The app's pages around the games' menu (pages.c): a ribbon of three entries at the bottom (a rail at the left on a
 * wide screen): Stats, Games (the shell's menu: its tabs and cards), Settings; each page its big title at the top.
 * The Games page's header: the day streak (a flame and the number) and a search over the games' names. The Settings
 * page: support, appearance (light, dark, as the device), the language, the gameplay's switches, the legal pages, the
 * version. The Stats page: the week, today, the level, the XP, the sections' progress, the achievements (stats.h).
 * The pages, their menus and the rules page follow the theme (the chrome); the games' boards never do.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef PAGES_H
#define PAGES_H

#include "shell.h"

enum { PAGE_STATS, PAGE_GAMES, PAGE_SETTINGS, PAGES };
extern int page;                                /* the page shown while the shell is on its menu */

/* the theme's colours (the house palette: a deep charcoal green, or a warm ivory; gold) */
typedef struct {
    int dark;
    uint8_t bg[3];                              /* the page */
    uint8_t card[3];                            /* a row, a card */
    uint8_t line[3];                            /* a track, a rule, an empty chip */
    uint8_t text[3], dim[3];                    /* words, quieter words */
    uint8_t accent[3];                          /* gold words and icons: the headers, the active entry */
    uint8_t gold[3];                            /* a gold fill (a choice made, a day played, a bar) */
    uint8_t on_gold[3];                         /* words on it */
    uint8_t nav[3];                             /* the ribbon */
} theme_t;
extern theme_t TH;

/* ---- the pages' side, called by the shell ---------------------------------------------------------------------- */
const char *pages_lang_kept(void);              /* the language chosen in Settings (NULL: the device's); after
                                                 * save_open, before text_init */
void pages_init(void);                          /* the theme, the screen kept awake, the stats (after save_open) */
void pages_layout(void);                        /* the menu's screen: the ribbon, the room left for a page */
box  pages_room(void);                          /* that room */
float pages_games_top(void);                    /* the Games page: where its tabs go, under its header */
void pages_draw_bg(void);                       /* the page's ground (before the Games page's cards) */
void pages_draw(void);                          /* the page (the Games page: its header), the ribbon, a panel */
int  pages_press(float x, float y);             /* 1: the pages' (the ribbon, a page, the Games page's header) */
void pages_motion(float x, float y);
int  pages_release(float x, float y);           /* 1: the pages' */
void pages_wheel(float dy);                     /* 1: scrolled */
int  pages_key(SDL_Keycode k);                  /* 1: the pages' (the search field's keys, Back to the games) */
void pages_text(const char *utf8);              /* typed (SDL_TEXTINPUT): the search field */
const char *pages_search(void);                 /* the search's words, as text_fold makes them; NULL: no search */
void pages_foreground(void);                    /* the app is back in front: the device's theme looked at again */
void pages_set_theme(int mode);                 /* 0 as the device, 1 light, 2 dark (the picture mode's --theme) */
void pages_show(int p);                         /* a page shown (the picture mode's --page) */
void pages_scroll(float px);                    /* the page scrolled (the picture mode's --scroll) */
void pages_search_set(const char *s);           /* the search open with these words (the picture mode's --search) */
void pages_tips_demo(void);                     /* the tip panel as if a store sold tips (the picture mode's
                                                 * --tips-demo: made-up prices) */

/* the support links (a solved puzzle's card), each shown when its Settings row is (the .conf's URLs, the tips):
 * 1 when shown, with its icon, the icon's colour and its word (help/<lang>/_ui.txt, text_word("_ui", word)) */
enum { PAGES_LINK_REVIEW, PAGES_LINK_REPORT, PAGES_LINK_SUPPORT };
int  pages_link(int i, int *icon, const uint8_t **colour, int *word);
void pages_link_open(int i);                    /* the store's page, the mail, the tip panel (or the support page) */
int  pages_panel_draw(void);                    /* off the menu: the tip panel when open (1: drawn) */
int  pages_panel_press(float x, float y);       /* 1: the panel's */

/* ---- the shell's side, called by the pages (shell.c) ------------------------------------------------------------ */
void shell_page_changed(void);                  /* lay the menu out again (another page, the search's words) */
void shell_set_mute(int muted);
void shell_text_changed(void);                  /* the language switched: every picture of words made again */
void shell_theme_changed(void);                 /* the theme switched: the menu's pictures of words made again */
void shell_open_text(const char *key);          /* a page of text (the rules page's look): help/<lang>/<key>.txt */
void shell_progress(int section, int *done, int *total);   /* a menu section: levels solved / games won, of all */
int  shell_sections(int *sections);             /* the menu's sections that have games, in order (SECTIONS at most) */
void shell_tab_icon(int section, int d, uint8_t *rgba);    /* a section's tab picture */
int  shell_three_stars(void);                   /* the levels solved with three stars, every game */
void shell_open_url(const char *url);           /* (the picture mode: on stderr) */
int  shell_search_hits(void);                   /* the games the search finds */

#endif
