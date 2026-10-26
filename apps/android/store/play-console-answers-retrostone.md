# Play Console: draft answers for RetroStone

RetroStone is RetroStone Board's code (`apps/boardgames`, `EDITION=retrostone`) with the Arcade's games in a tab, so
the Board's answers (`play-console-answers-board.md`) and the Arcade's (`play-console-answers.md`) hold, except where
this page says otherwise. Checked against the code as built today: `apps/android/app-retrostone.conf`
(`PLAY_REVIEW=0`, `IAP=1`), the merged manifest of `dist/android/RetroStone-debug.apk` (26 October 2026),
`apps/common/iap_play.c` and its Java class, `apps/boardgames/src/save.c` and the Settings page (`pages.c`). Check
each answer against the form when you fill it in. The answers marked "billing" are about the tips.

## What differs from the Board and the Arcade
| | RetroStone |
|---|---|
| Price | free; everything playable; **tips** as in-app purchases (Google Play Billing) |
| Permissions | `VIBRATE` (gamepad rumble in the Arcade games); from the Billing Library and its dependencies: `com.android.vending.BILLING`, **`INTERNET`** and `ACCESS_NETWORK_STATE`. The app's own code opens no connection; Google's libraries do (the store, and Google's own diagnostics of the purchase flow) |
| Third-party libraries | SDL2, libxmp-lite (the Arcade's music); Google Play Billing Library 9.1.0, with Google Play services, Google's data transport and AndroidX libraries |
| Rating prompt | none (`PLAY_REVIEW=0`); the Settings page's "Leave a review" opens the store page |
| Links out | Settings: Leave a review (the Play page), Star on GitHub, Report a problem (an e-mail); nothing during a game |
| In the app | its privacy policy, terms and licences (Settings > System and legal) |
| Orientation | portrait and landscape; landscape while an Arcade game plays |

## Privacy policy
The hosted `privacy/privacy-retrostone.html` (the app's own text, English then French; contact hello@8bcraft.com),
e.g. https://www.8bcraft.com/privacy-policy-retrostone/ . The app shows the same policy in Settings. It already
covers the tips ("paid through your store... we receive no card details and nothing about you").

## Ads
**No.**

## App access
**All functionality is available without special access** (no login, no code; the tips unlock nothing).

## Data safety
| Question | Answer |
|---|---|
| Does your app collect or share any of the required user data types? | **No** |
| (the form ends: "No data collected", "No data shared") | |

Why: the app's own code has no network code; the settings, progress and stats are written in the app's private
storage and never leave the device through it. **Billing (check before you answer):** the app now has `INTERNET`,
added by the Google Play Billing Library, whose dependencies (Google's data transport) send Google diagnostics of the
purchase flow. Google Play's own handling of a purchase is not data you collect; whether the library's diagnostics
must be declared is Google's call: read the **Google Play SDK Index** entry for "Google Play Billing Library" (its
data collection and its Data safety guidance) and answer as it says. If it lists data types to declare (likely
"App activity / App interactions" or "App info and performance / Diagnostics", collected, not shared, not optional,
for analytics or app functionality), declare them; then the in-app policy's "nothing leaves your device through the
app" needs a sentence on Google's library (`help/<lang>/_privacy.txt`, 22 languages, and rerun
`make_privacy_retrostone.py`).

## Content rating (IARC questionnaire)
Category: **Game**. The questionnaire covers the whole app, the Arcade games included:

| Topic | Answer | Notes |
|---|---|---|
| Violence | **Yes, mild cartoon / fantasy violence** | the Arcade games' (as `play-console-answers.md` describes them, and the newer ones in the same spirit: cute animals, bumps, pops, snowballs): no blood, no human victims; **play the newer Arcade games once with this question in mind**. The board games: abstract captures only. Werewolves and The Spy are party games of words and votes: nothing shown |
| Fear / horror | No | |
| Sexuality, nudity | No | the French Tarot's trumps: 1898 genre scenes (see the Board's page) |
| Language | No | |
| Controlled substances | No | |
| Gambling: real money | **No** | nothing can be bet or won |
| Simulated gambling | **No (suggested; the owner decides)** | the Board's facts hold (no casino game, no currency, Yellow Dwarf's counters are the score): see `play-console-answers-board.md` |
| Loot boxes | No | |
| Digital purchases | **Yes** (the tips: voluntary, unlock nothing) | |
| Users interact or communicate | No | one device shared face to face; gamepads; no network |
| Shares location | No | |
| Unrestricted internet | No | |

Expected: PEGI 3 or PEGI 7 (mild fantasy violence), ESRB Everyone, USK 0, with "In-app purchases" shown (the
digital purchases question).

## Target audience and content
Suggested: **13 and up** (13-15, 16-17, 18+), "the app does not appeal primarily to children": the app's heart is
classic adult games (bridge, skat, tarot, mus), and with tips in the app the Families policy's
rules on purchases would apply. The Arcade's cute animals could make a reviewer ask: the answer is the catalogue (216
board, card and puzzle games, most for adults). Including children (9-12) is possible but then the Families policy
applies to the tips and the links out of Settings (a parental gate may be required).

## Monetization declarations
| Form | Answer |
|---|---|
| Contains ads | No |
| In-app purchases | **Yes** (Play adds the label when products exist) |
| Payments profile | 8BCraft SASU (Setup > Payments profile) |

## Other declarations
As the Board's: news No, government No, financial features None, health None, data deletion URL not needed (no
account, no data), advertising ID No. Category **Game > Board**. Tags: Board, Card, Puzzle, Arcade, Offline.
