# Publishing the two apps on Google Play: the checklist

**RetroStone Arcade** (the paid app, all the games: `apps/android/app-full.conf`) and **RetroStone Arcade Free**
(3 games, the others shown locked with a "Get it on Google Play" button to the paid app: `apps/android/app.conf`).
Everything the store needs is in this folder (`apps/android/store/`); the builds: `apps/android/README.md`.
The Play Console changes its menus and rules often: when this list and the Console disagree, the Console wins.

## 0. Before you start (the code side)
- [ ] The final **package ids** (`APP_ID`): `com.eightbcraft.retrostonearcade` (paid) and
      `com.eightbcraft.retrostonearcade.free`. **They can never change once uploaded.** If you change the paid one,
      change `PAID_APP_ID` and `PAID_STORE_URL` in `app.conf` too.
- [ ] The final names (`APP_NAME`, 30 characters at most on Google Play) and `APP_VERSION_CODE` 1.
- [ ] The touch controls tuned after your phone test (`apps/launcher/games.cfg`; the provisional ones are marked).
- [ ] Your **upload key**: `apps/android/make-keystore.sh` (once; back it up as it says).
- [ ] The release files: `apps/android/build.sh full --release` and `apps/android/build.sh free --release` give
      `dist/android/RetroStoneArcade-release.aab` and `dist/android/RetroStoneArcadeFree-release.aab` (for Google
      Play; the `.apk` next to them is for sideloading).
- [ ] The **privacy policy** online: put your contact e-mail in `privacy/privacy-policy-en.md` and `-fr.md`
      (`[CONTACT E-MAIL]`), run `python3 apps/android/store/tools/privacy_html.py`, host the two `.html` pages
      (GitHub Pages, your site), note the URL.

## 1. The developer account (once, both apps)
- [ ] <https://play.google.com/console/signup>: a Google account for the business (not a throwaway one).
- [ ] Account type: **personal** (you alone) or **organisation** (8BCraft as a company: needs a D-U-N-S number,
      free but takes days; organisations skip the 12-tester rule below).
- [ ] Pay the **one-time $25** registration fee.
- [ ] **Identity verification** (an ID document; for a personal account also your address), and the verification of
      your contact phone and e-mail. Allow a few days.
- [ ] Developer name shown on the store: "8BCraft". A public contact e-mail (shown on the listings).
- [ ] Google may also ask new accounts to prove they have a real Android device (through the Play Console app).
- [ ] **For the paid app: a payments profile** (Play Console > Setup > Payments profile): your bank account and tax
      details (French VAT / SIRET if you have one). Without it you cannot set a price.

## 2. Create the two apps
Play Console > **Create app**, twice:

| | RetroStone Arcade | RetroStone Arcade Free |
|---|---|---|
| App name | RetroStone Arcade | RetroStone Arcade Free |
| Default language | English (United States) - en-US | English (United States) - en-US |
| App or game | Game | Game |
| **Free or paid** | **Paid** | Free |

**Free or paid cannot be undone in one direction: a free app can never become paid.** Create the paid app as Paid
from the start. The package id is fixed by the first AAB you upload.

## 3. App content (Policy > App content), each app
Answers drafted in `play-console-answers.md`:
- [ ] Privacy policy URL
- [ ] Ads: **No**
- [ ] App access: all functionality available without special access
- [ ] Content rating: the IARC questionnaire (Game; mild cartoon violence; expected PEGI 3 / ESRB Everyone)
- [ ] Target audience and content (your decision: the children's groups included is suggested; the apps meet the
      Families policy)
- [ ] Data safety: **No data collected, no data shared**
- [ ] News app No, government app No, financial features None, health None, advertising ID No

## 4. The store listing (Grow > Store presence > Main store listing), each app
Everything is in `free/` and `full/`:
- [ ] **Texts**, per language (Add translations > choose the language > paste): `listings/<language>/title.txt`,
      `short_description.txt` (80 characters max), `full_description.txt` (4000 max). English is the source, French
      written with "tu"; **the 20 other languages were translated by an AI: have a native speaker check them**
      (at least the short descriptions) before you publish them, or publish English and French only at first.
- [ ] **App icon**: `graphics/icon-512.png` (512 x 512)
- [ ] **Feature graphic**: `graphics/feature-1024x500.png` (1024 x 500)
- [ ] **Phone screenshots**: `screenshots/<language>/*.png` (2160 x 1080, landscape; 2 to 8 per language; the
      English ones are the default, add the French and German ones in those languages)
- [ ] Tablet / Chromebook screenshots: optional (the phone ones can be reused for 10-inch tablets: same 2:1
      landscape shape is accepted).
- [ ] Category **Game > Arcade**; tags; contact e-mail (and website if you have one).
- [ ] The paid app's text says how many games it has (20 today): rebuild the texts if a game is added
      (`python3 apps/android/store/tools/make_listings.py`).

## 5. Testing: the closed test (required for a new personal account)
Personal developer accounts created after 13 November 2023 must run a **closed test with at least 12 testers who
stay opted in for at least 14 days in a row** before they can apply for production access, **for each app**.
- [ ] Testing > **Closed testing** > Create track (or use "Alpha"), add testers: a Google Group (easiest) or a list of
      e-mails; **12 at least, 15-20 to be safe** (some will drop out; one who leaves early can reset the count).
- [ ] Create a release, upload the `.aab`, release notes ("First test version"), save, review, roll out.
- [ ] Send the testers the **opt-in link** (Testing > Closed testing > Testers): each must accept it with the Google
      account of their phone, then install from Google Play.
- [ ] **The paid app's testers must own it**: give them promo codes (Monetize > Promo codes, free for you), or add
      them as license testers (Settings > License testing): check the Console's current rule.
- [ ] Run both tests at the same time with the same testers (one Google Group for both).
- [ ] Ask testers to play a little on several days: Google asks, when you apply, how testing went.
- [ ] After 14 days: Dashboard > **Apply for production** (questions on the testing, the feedback, your readiness).
      The answer takes up to about 7 days.

## 6. Pricing and countries (Monetize > Products > App pricing; Production > Countries)
- [ ] The paid app's **price**: set it in your currency; Google converts it per country (adjust the main ones by
      hand: Google shows local prices **with VAT included**, and takes 15 % (first $1M a year) of the price before
      tax).
- [ ] **Countries / regions**: all, or a chosen list (the same for both apps, so the free app's link always works).
      The paid app must be available wherever the free one is.
- [ ] The free app stays free (and has no in-app purchases).

## 7. Production
- [ ] Publish **the paid app first** (or on the same day): the free app's "Get it on Google Play" button opens its
      page, which says "not found" until it is published.
- [ ] Production > Create release > upload the same `.aab` as the test (or a newer one with a higher
      `APP_VERSION_CODE`), release notes, review, **roll out** (a staged rollout of 20 % first is fine).
- [ ] Google's review: from a few hours to a few days (longer for a new account). Fix what it reports, if anything.
- [ ] Check both listings on a phone, then the link from the free app to the paid one.

## 8. Every update afterwards
- [ ] `APP_VERSION_CODE` + 1 (and `APP_VERSION_NAME`) in the app's `.conf`, `build.sh <app> --release`, upload to a
      track, release notes.
- [ ] The same upload key, always (`~/.retrostone-keys`, and its backups).
- [ ] Each August: Google raises the minimum **target API level** for updates (`targetSdk` in
      `apps/android/app/build.gradle`, 36 today).
- [ ] Keep the promotion of the full version **inside the menu only** (no pop-up, no interstitial, nothing during a
      game): it is what keeps the "Contains ads" label away.
