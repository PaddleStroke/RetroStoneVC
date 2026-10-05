# Play Console: draft answers (both apps)

Drafts for the forms of **Policy and programs > App content** in the Play Console, true for RetroStone Arcade Free and
RetroStone Arcade as built today (no `INTERNET` permission, no ads, no analytics, no account, saves only on the
device). Check each answer against the form when you fill it in: the questions change from time to time.
If the apps ever change (an online leaderboard, a crash reporter, an ads or analytics SDK...), these answers change.

## Privacy policy
URL of the hosted page made from `privacy/privacy-policy-en.html` (and `-fr.html`). Fill in the contact e-mail in
both files first (`[CONTACT E-MAIL]`), rebuild them (`python3 apps/android/store/tools/privacy_html.py`), host them
(GitHub Pages, your website...), and paste the English URL.

## Ads
**Does your app contain ads?** No.
(The free app's menu shows the full version's games and a "Get it on Google Play" button: Google Play treats the
promotion of your own apps inside your app's own UI, never in a pop-up, an interstitial or during play, as part of the
app, not as ads. Keep it that way: no promotion in the games, no pop-up.)

## App access
**All functionality is available without special access** (no login, no code, no subscription).

## Data safety
| Question | Answer |
|---|---|
| Does your app collect or share any of the required user data types? | **No** |
| (then the form ends: "No data collected", "No data shared") | |

Why "No": the apps cannot send anything (no internet permission); the best scores and save states are written in the
app's private storage and never leave the device through the app. Android's own backup (Auto Backup to the user's
Google account) is a system feature the user controls; data the app keeps on the device only is not "collected".
Optional badges you may NOT claim: none needed (no data in transit, nothing to delete on a server).

## Content rating (IARC questionnaire)
Category: **Game**. Contact e-mail: yours. Draft answers:

| Topic | Answer | Notes |
|---|---|---|
| Violence | **Yes, mild cartoon / fantasy violence** | cute animal characters bump into obstacles, a mole's harpoon pops gemstones, a farmer shoos rabbits with a pitchfork, crabs break shells: no blood, no gore, no realistic violence, no human victims |
| ... violence against humans, realistic, blood, gore, killing, torture | No | |
| Fear / horror | No | |
| Sexuality, nudity | No | |
| Language (profanity, crude humour) | No | |
| Controlled substances (drugs, alcohol, tobacco) | No | (Mouse Espresso: a coffee machine, not a controlled substance) |
| Gambling, simulated gambling, loot boxes | No | |
| Users can interact or communicate (chat, online play) | No | local multiplayer with gamepads only |
| Shares the user's location | No | |
| Digital purchases inside the app | No | the free app links to the paid app's Play page; nothing is sold in the app |
| Unrestricted internet access / web browser | No | |
| Miscellaneous (e.g. promotes other apps?) | answer as the form asks; the free app promotes the full app in its menu | |

Expected ratings: PEGI 3 (or PEGI 7 for "mild violence towards fantasy characters"), ESRB Everyone, USK 0,
ClassInd L. The same questionnaire for both apps (save the answers once, reuse them).

## Target audience and content
An owner's decision. The games suit children, so declaring only 13+ risks Google's "unintended appeal to children"
review. Suggested: **include the children's age groups (6-8, 9-12) and the older ones**: the Families policy then
applies, and the apps already meet it (no ads, no data collected, no third-party SDK, a privacy policy, content
suitable for children). Under 6 is possible too (the controls are one-tap for many games).
**Does the app appeal to children?** Yes (if you include their age groups, the question is answered by that).

## Other declarations
| Form | Answer |
|---|---|
| News app | No |
| COVID-19 contact tracing / status | No |
| Government app | No |
| Financial features | None |
| Health | None |
| Data deletion URL | not needed (no account, no data collected) |
| Advertising ID | No (the apps do not use it) |
| App category | Game > **Arcade** |
| Tags | Arcade, Casual, Pixel art / Retro (pick those the Console offers) |
