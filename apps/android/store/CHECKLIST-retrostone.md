# Publishing RetroStone on Google Play: the brief and the checklist

**RetroStone** (`apps/android/app-retrostone.conf`, `com.eightbcraft.retrostone`): the one free app. Every board,
card and puzzle game of RetroStone Board and every RetroStone VC arcade game (its Arcade tab), nothing locked, no ads,
paid for by **tips** (and later, maybe, a supporter subscription). The steps are the Arcade's (`CHECKLIST.md`:
developer account, closed test, production); this page lists the RetroStone kit and what differs.

## The kit (made by the tools; run them in WSL from the repository's root)
| | File | Tool |
|---|---|---|
| Icon 512 x 512 | `retrostone/graphics/icon-512.png` | `tools/make_store_art_board.py retrostone` |
| Feature graphic 1024 x 500 | `retrostone/graphics/feature-1024x500.png` | the same |
| Phone screenshots, 8 per language (en, fr, de): 7 upright 1080 x 1920, the Arcade game 1920 x 1080 | `retrostone/screenshots/<lang>/` | `tools/store_shots_retrostone.py` |
| Listing, 22 languages (title, short and full description) | `retrostone/listings/<locale>/`, `retrostone/listings-all-languages.txt` | `tools/make_listings_retrostone.py` |
| Release notes, 22 languages (+ en-GB) | `retrostone/release-notes-<version>.txt` | the same |
| The tips' product names and descriptions, 22 languages | `retrostone/in-app-products.txt` | the same |
| Privacy policy (web page) | `privacy/privacy-retrostone.html` | `tools/make_privacy_retrostone.py` (from the app's own `help/<lang>/_privacy.txt`) |
| Play Console answers | `play-console-answers-retrostone.md` | |

- The listing counts the games from the code (253 today: 216 board, card and puzzle games in 7 tabs, 37 arcade games;
  over 14,000 levels): **rerun `make_listings_retrostone.py` when a game is added** and check its PROBLEM lines
  (lengths, trademarks). The title's count is rounded down to 50 ("250 in 1"), so it only changes at 300; the
  descriptions give the exact count. The listing says **open source** (the short description in English and French,
  a line in every full description). 253 names do not fit in 4,000 characters: each board tab names its
  best-known games (`FEATURED` in the tool) and "and N more"; the Arcade tab names all its games.
- English is the source, French uses "tu"; **the 20 other languages were written by an AI: have a native speaker
  check them** (at least the titles and short descriptions), or publish English and French first.
- The titles say "250 in 1" in each language's way ("en 1", "i 1", "w 1", "в 1", "合1"...), then the kinds of games
  that fit in 30 characters; Polish, Czech, Russian and Ukrainian never put a noun after a number in the descriptions.
- **The icon is new** (`ICON_STYLE=retrostone`, `apps/android/tools/make_icon.py` `retrostone_icon`): RetroStone
  Board's gold frame, green table, knight and card, with an Arcade hero (the crab) on a little screen in the die's
  place, so the app is not mistaken for RetroStone Board. The launcher icon (`build.sh retrostone`) is the same picture.
- The screenshots have no caption band (as the Board's and the Arcade's). One is landscape (an Arcade game: the app
  turns for it); Google Play accepts mixed orientations.

## The tips (in-app donations): what Google Play allows
- **No free amount.** Google Play Billing sells products at prices you set in the Play Console (one price per country,
  converted automatically, VAT included in what the player sees); a player cannot type an amount. Two ways to come
  close: **several tiers**, and **multi-quantity** (a product option in the Console: the purchase sheet then lets the
  player buy 1 to N of the same tip in one go, e.g. N x 1 €).
- **The tips must go through Google Play Billing.** A Ko-fi / PayPal / Patreon link inside the Play app for tips to
  the developer breaks the Payments policy (only donations to registered charities may go elsewhere). `SUPPORT_URL`
  stays for the desktop and F-Droid builds only.
- **Consumable one-time products**, each "bought" again and again; Google takes **15 %** (the first $1M of a year).
  Product ids are **permanent** (a deleted id can never be used again).
- **Subscriptions** are possible (Monetize > Products > Subscriptions): a "Supporter" subscription with monthly base
  plans (e.g. 1 €, 2 €, 5 € a month: one subscription, three base plans) or a yearly one; Google takes 15 % of
  subscriptions from the first day. Rules: say clearly what it gives and what it costs, how to cancel (a link to Play's
  subscriptions page in the app), and handle the subscription's state. A subscription that unlocks nothing is a
  recurring gift: say so plainly ("unlocks nothing: everything is free"); a small visible thank-you (a supporter badge
  on the Stats page, a colour theme) makes it easier to describe and less likely to puzzle a reviewer.

### The products (proposed: owner decides)
| Product id (permanent) | Type | Price (EUR, the other currencies converted by Google) | Name in the app (`_ui.txt`) |
|---|---|---|---|
| `com.eightbcraft.retrostone.tip1` | consumable | 1 € (multi-quantity on) | A small tip |
| `com.eightbcraft.retrostone.tip2` | consumable | 2 € | A nice tip |
| `com.eightbcraft.retrostone.tip3` | consumable | 5 € | A big tip |
| `com.eightbcraft.retrostone.tip4` | consumable | 10 € | *(needs a 4th row in the tip panel)* |
| `com.eightbcraft.retrostone.tip5` | consumable | 20 € | *(needs a 5th row)* |
| `com.eightbcraft.retrostone.supporter`, base plans `monthly-1`, `monthly-2`, `monthly-5` | subscription | 1 €, 2 €, 5 € a month | *(needs its own rows, cancellation link, state)* |

The app as built today (`TIP_PRODUCTS` in the .conf, `help/<lang>/_ui.txt`, `pages.c`'s tip panel) has **three tiers**
(tip1..tip3) and no subscription: `in-app-products.txt` holds their names and descriptions in 22 languages, ready to
paste. Five tiers or a subscription are app changes (the panel's rows, their names in 22 languages, the subscription's
state and cancel link): add the ids to `TIP_PRODUCTS`, the names to `_ui.txt`, then rerun `make_listings_retrostone.py`.

## Before the first upload: blockers
- [x] **Google Play Billing** is in the Android build (`IAP=1`: `apps/common/iap_play.c` and
      `apps/common/android/java/com/eightbcraft/iap/Billing.java`, Billing Library 9.1.0; `apps/common/README.md`).
- [ ] **Data safety and the privacy policy with billing**: the Billing Library brings `INTERNET` and Google's
      diagnostics of the purchase flow. Check its Google Play SDK Index entry, answer Data safety as it says, and if
      something must be declared, add a sentence on Google's library to `help/<lang>/_privacy.txt` (22 languages)
      before release (`play-console-answers-retrostone.md`, Data safety).
- [ ] **Test a tip end to end** before production: upload the release `.aab` to internal testing, create the three
      products (below) and activate them, add your Google account as a license tester (Setup > License testing),
      install the app from the internal-testing link, then Settings > Leave a tip: the prices show, the sheet opens,
      a test purchase goes through, the panel thanks you, and the same tier can be bought again (consumed).
- [ ] **The Console needs an uploaded build with the billing library** (the `com.android.vending.BILLING` permission)
      before it lets you create the products: upload to internal testing first, then create the products.
- [ ] **A payments profile** (Setup > Payments profile: 8BCraft SASU's bank account and tax details) to sell tips.
- [ ] `APP_VERSION_NAME` is `0.1.0`: make it `1.0.0` for the first release if you like (then rerun the listing tool:
      the release notes' file is named after it).
- [ ] Package id final (permanent): `com.eightbcraft.retrostone`.
- [ ] Release build: `apps/android/build.sh retrostone --release` (the upload key in `~/.retrostone-keys/`).
- [ ] Host `privacy/privacy-retrostone.html` and put its address in `PRIVACY_URL` (app-retrostone.conf).
- [ ] **RetroStone Arcade and RetroStone Board on Google Play**: if they are (or will be) published, decide their
      future before this one goes live. The same games under the same developer in several apps can be refused as
      "repetitive content" (Spam policy). Suggested: publish RetroStone, then unpublish the others (their pages
      stay for people who bought them; they keep working), or never publish them.

## Create the app (Play Console > Create app)
| | RetroStone |
|---|---|
| App name (store title, 30 max) | `250 in 1 Puzzle, Board, Arcade` (each language's in `listings/<locale>/title.txt`): what the app is, the owner's choice; the name under the icon stays **RetroStone** (`APP_NAME`), and the developer line shows 8BCraft |
| Default language | English (United States) |
| App or game | Game |
| Free or paid | **Free** (a free app can never become paid) |
| Category | Game > **Board** (the most games and the best search terms; Game > Casual is the other fit) |
| Tags | Board, Card, Puzzle, Arcade, Offline, Single player, Multiplayer (pick those the Console offers) |
| Contact e-mail | hello@8bcraft.com |
| Website | the 8bcraft.com page of the app (optional) |

## App content, store listing, testing, production
As `CHECKLIST.md` sections 3 to 8, with `play-console-answers-retrostone.md`. Store listing: upload the icon, the
feature graphic and the screenshots of each language you publish (the listing's other languages show the English
ones); paste each language's three fields (or import `listings-all-languages.txt` with the Console's translation
import). Owner decisions:
- [ ] **The tips**: three tiers (as the app is) or five, the prices, multi-quantity; a subscription now or later.
- [ ] **Content rating**: the simulated gambling question (suggested "No": `play-console-answers-retrostone.md`).
- [ ] **Target audience**: 13+ suggested (see the answers page).
- [ ] **Privacy URL** (where `privacy-retrostone.html` is hosted).
- [ ] **Countries**: all.
