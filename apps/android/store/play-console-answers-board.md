# Play Console: draft answers for RetroStone Board and RetroStone Board Free

The Arcade's answers (`play-console-answers.md`) hold for the Board apps too, except where this page says otherwise.
Checked against the code as built today: `apps/android/app-board.conf` and `app-boardfree.conf` (`PLAY_REVIEW=0`:
no Play library at all), the shared `apps/android/app/src/main/AndroidManifest.xml`, `BoardActivity.java` (SDL's
activity, nothing more) and `apps/boardgames/src/save.c`. Check each answer against the form when you fill it in.
If the apps ever change (online play, a crash reporter, an SDK...), these answers change.

## What differs from the Arcade

| | Arcade | Board |
|---|---|---|
| Permissions | `VIBRATE` (gamepad rumble) | the same manifest: **`VIBRATE` only, no `INTERNET`** (the Board app never vibrates, but the shared manifest asks for it; harmless, no Data safety impact) |
| Third-party libraries | SDL2, libxmp-lite, Play In-App Review | **SDL2 only** (and stb, fonts Noto Sans CJK and DejaVu Sans, cut down into the app). No Google library: no review prompt |
| Rating prompt | Google's in-app review | **none** (`PLAY_REVIEW=0`) |
| Orientation | landscape | portrait and landscape (phones and tablets) |
| What is kept | best scores, saved games | one file `board.sav`: settings, each game's last choices, the levels' stars (`save.c`); no names, nothing about the player |
| Link out | free app: "Get it on Google Play" | free app: the "Full version" panel (from a locked game or the last free level) opens the paid app's Play page (`market://details?id=com.eightbcraft.retrostoneboard`, else its web page) |
| Content rating | mild cartoon violence | no violence; **the dice and card games: see "Simulated gambling" below** |
| Category | Game > Arcade | **Game > Board** (or Card; see below) |

## Privacy policy
URL of the hosted `privacy/privacy-board.html` (English, then French; contact hello@8bcraft.com), e.g.
https://www.8bcraft.com/privacy-policy-retrostone-board/ . **The Board apps do not show the policy inside the app**
(the Arcade's Credits & licences do; the Board app has no such page yet): see CHECKLIST-board.md, to do.

## Ads
**No.** The free app's locked games and levels open a "Full version" panel with a button to the paid app's Play page,
inside the app's own menu and level screen only, never during a game, never as a pop-up on its own: the promotion of
your own app in your own UI, as in the Arcade's free app. Keep it that way.

## App access
**All functionality is available without special access** (no login, no code, no subscription).

## Data safety
| Question | Answer |
|---|---|
| Does your app collect or share any of the required user data types? | **No** |
| (the form ends: "No data collected", "No data shared") | |

Why: no `INTERNET` permission and no network code; the settings and the stars are written in the app's private storage
(`board.sav`) and never leave the device through the app. Android's Auto Backup (`allowBackup="true"`, the user's
choice in Android) is a system feature; data kept on the device only is not "collected".

## Content rating (IARC questionnaire)
Category: **Game** (the form may offer "Board game, card game, puzzle"-type wording: take it). Draft answers:

| Topic | Answer | Notes |
|---|---|---|
| Violence | **No** | abstract board-game captures (chess, draughts, go), a sea-battle grid; no characters hurt, no blood |
| Fear / horror | No | |
| Sexuality, nudity | No | the French Tarot's trumps are 1898 genre scenes (dances, families, soldiers on parade): look at them once (`apps/boardgames/art/tarot/`), nothing seen at a glance |
| Language (profanity, crude humour) | No | |
| Controlled substances | No | none shown or named (Liar's Dice, 421, Ship, Captain and Crew are café dice games in real life; the app shows dice only) |
| **Gambling: real money** | **No** | nothing can be bet, bought or won |
| **Simulated gambling** | **No (suggested; the owner decides, see below)** | |
| Loot boxes, digital purchases | No | the free app links to the paid app's Play page; nothing is sold in the app |
| Users interact or communicate (chat, online play) | No | several people share one device, face to face; no network |
| Shares the user's location | No | |
| Unrestricted internet / web browser | No | |

### Simulated gambling: the facts, and why "No" is suggested (owner: please decide)
IARC's "simulated gambling" means games that imitate betting: casino games (poker, blackjack, roulette, slots),
wagering a currency on chance. The app has **no casino game and no currency**: no chips to buy or win, no balance kept
between games, nothing to exchange. What comes closest, so you can judge:
- **Yellow Dwarf** (the French *nain jaune*): each player starts every game with 100 counters, puts a few on the board's
  five cards each deal, and takes them back by playing those cards; the counters are the game's score and vanish when
  it ends.
- **421**, **Ship, Captain and Crew**: tokens (11, or 5 to win) count who is losing or winning; no stake.
- **Liar's Dice**, **Mexico**: bluffing over hidden dice; lives or points, no stake.
- **Mus**, **Truco** (and belote's *coinche*, bridge's double): "bets" are bids worth game points, the traditional
  scoring of these family card games.
- Points only everywhere; no payout, no shop, no in-app purchase.

Suggested answer: **No simulated gambling** (expected rating PEGI 3, ESRB Everyone, USK 0). If you prefer the
cautious answer **Yes** (because of Yellow Dwarf's counters), expect PEGI 12 and ESRB Teen, and the children's age
groups below become a poor fit. The free app plays none of the games above (its 13: four in a row, reversi,
draughts, chess, backgammon, ludo, yacht, solitaire, free cells, hearts, crazy eights, secret code, sea battle, plus
the puzzles; the others are locked and cannot be played): for it, **No** is plainly right.

## Target audience and content
An owner's decision. Two honest options:
- **13 and up** (13-15, 16-17, 18+): the apps' heart is classic adult games (bridge, skat, tarot, mus); the Families
  policy does not apply. Mark "the app does not appeal primarily to children".
- **Include 9-12** (or more), as the Arcade's draft suggests: the puzzles, ludo, snakes and ladders, pairs, old maid
  suit children. The Families policy then applies; the apps meet it (no ads, no data, no SDK, a policy), but the free
  app's "Full version" button opens the Play Store: check the Console's current Families rules on links out of the
  app (a parental gate may be asked for apps aimed at children).
Suggested: **13+ for both apps** (simplest, true to the paid app's games), unless you want the families' audience.

## Other declarations
| Form | Answer |
|---|---|
| News app | No |
| Government app | No |
| Financial features | None |
| Health | None |
| Data deletion URL | not needed (no account, no data collected) |
| Advertising ID | No |
| App category | Game > **Board** (Game > Card is the other fit; one only) |
| Tags | Board, Card, Puzzle, Chess, Offline (pick those the Console offers) |
| Families: teacher approved | no |
