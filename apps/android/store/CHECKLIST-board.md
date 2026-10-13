# Publishing RetroStone Board on Google Play: the checklist

**RetroStone Board** (paid, every game: `apps/android/app-board.conf`, `com.eightbcraft.retrostoneboard`) and
**RetroStone Board Free** (`apps/android/app-boardfree.conf`, `com.eightbcraft.retrostoneboard.free`: 13 games and
every puzzle's first 40 levels, the rest locked with a "Full version" panel to the paid app).
The steps are the Arcade's (`CHECKLIST.md`: developer account, closed test of 12 testers for 14 days per app,
pricing, production); this page lists only the Board kit and what differs.

## The kit (made by the tools; run them in WSL from the repository's root)
| | Paid | Free | Tool |
|---|---|---|---|
| Icon 512 x 512 | `board/graphics/icon-512.png` | `boardfree/graphics/icon-512.png` | `tools/make_store_art_board.py` |
| Feature graphic 1024 x 500 | `board/graphics/feature-1024x500.png` | `boardfree/graphics/feature-1024x500.png` | the same |
| Phone screenshots 1080 x 1920 (9:16), 8 per language: en, fr, de | `board/screenshots/<lang>/` | `boardfree/screenshots/<lang>/` | `tools/store_shots_board.py` |
| Listings, 22 languages | `board/listings/<locale>/`, `board/listings-all-languages.txt` | `boardfree/...` | `tools/make_listings_board.py` |
| Release notes 1.0.0, 22 languages (+ en-GB) | `board/release-notes-1.0.0.txt` | `boardfree/release-notes-1.0.0.txt` | the same |
| Play Console answers | `play-console-answers-board.md` | the same | |
| Privacy policy | `privacy/privacy-board.html` | the same | |

- The listings count the games from the code (141 today, 6 tabs, 90 with a bot, over 6,000 levels): **rerun
  `make_listings_board.py` when a game is added**, and check its PROBLEM lines (length, trademarks).
- English is the source, French uses "tu"; **the 20 other languages were written by an AI: have a native speaker
  check them** (at least titles and short descriptions), or publish English and French first.
- The listings name the games as the app's menu does, except the dice game Yacht in sv, da, nb, fi, where the menu
  says "Yatzy" (close to a brand): the listing says "Yacht" (`NAMES` in the tool). **To do (app):** consider renaming
  it in `help/{sv,da,nb,fi}/yacht.txt`.
- No tablet screenshots: the Arcade's kit has none (its phone shots are reused); the 9:16 phone shots can be used.
- No caption band on the screenshots, as the Arcade's.

## Before the first upload
- [ ] Package ids final (permanent): `com.eightbcraft.retrostoneboard`, `com.eightbcraft.retrostoneboard.free`.
- [ ] Release builds: `apps/android/build.sh board --release` and `build.sh boardfree --release`.
- [ ] **The icon (decision):** both apps have the same launcher icon (the store icon must match it). The Arcade tells
      its free app apart by the tile's colour (navy free, ruby paid). If you want the same for the Board, give
      `ICON_STYLE=board` a variant for `PROFILE=boardfree` in `apps/android/tools/make_icon.py` (e.g. another felt or
      frame colour), then rerun `make_store_art_board.py`. Not done: it changes the app.
- [ ] **The privacy policy in the app (to do):** the Arcade shows it in Credits & licences; **the Board app has no
      such page** (its menu's settings: the sound and the rules only). Google wants the policy reachable in the app
      too, and the fonts' OFL licence and SDL's notice belong in an about page: add a "Credits & licences / Privacy"
      row to the settings panel (shell.c) before release. The store side is ready: host `privacy/privacy-board.html`
      and note the URL.

## Create the two apps (Play Console > Create app)
| | RetroStone Board | RetroStone Board Free |
|---|---|---|
| App name (store title, 30 max) | RetroStone: 141 Board Games | RetroStone: Board Games Lite |
| Default language | English (United States) | English (United States) |
| App or game | Game | Game |
| Free or paid | **Paid** (can never become paid later) | Free |
| Category | Game > Board | Game > Board |

The titles are keyword-led like the Arcade's ("RetroStone: 21 Offline Games" / "... Lite"): the brand, then "board
games", the main search term, and the count for the paid app (a number sells breadth). Languages where the title would
pass 30 characters drop the count (fr, it, pt, pl, ru, uk) or, for the free app, keep the English one (as the Arcade's
French free title does).

## App content, store listing, testing, pricing, production
As `CHECKLIST.md` sections 3 to 8, with `play-console-answers-board.md`. Owner decisions:
- [ ] **Price** of the paid app (and countries: the same list for both; the paid app wherever the free one is).
- [ ] **Content rating**: the simulated gambling question (suggested "No"; read the facts in the answers page).
- [ ] **Target audience**: 13+ suggested, or include children (Families policy).
- [ ] **Privacy URL** (where `privacy-board.html` is hosted).
- [ ] Publish the paid app first (the free app's "Full version" button opens its page).
