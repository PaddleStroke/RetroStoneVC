# RetroStone Arcade and RetroStone Arcade Free (Android)

Two Android apps built from the same code, the RetroStone VC games in one program each:

| | **RetroStone Arcade Free** | **RetroStone Arcade** (paid) |
|---|---|---|
| Config | `apps/android/app.conf` | `apps/android/app-full.conf` |
| Package id | `com.eightbcraft.retrostonearcade.free` | `com.eightbcraft.retrostonearcade` |
| Games | Leady Squid, Pancake Tower, Beaver Rush; every other game shown with a padlock | every game of `games/` but Bomber Mole (20 today), all playable |
| The full version | locked cards, the "Get the full version" panel, a small banner in the menu's footer: its button opens the paid app's Play page | none |
| Build | `apps/android/build.sh free` | `apps/android/build.sh full` |
| Debug APK | `dist/android/RetroStoneArcadeFree-debug.apk` | `dist/android/RetroStoneArcade-debug.apk` |
| Desktop | `dist/windows/RetroStoneArcadeFree.exe` | `dist/windows/RetroStoneArcade.exe` |

The same launcher otherwise (described in [apps/launcher/README.md](../launcher/README.md)): the "i" in the menu's
top-left corner opens **Credits & licences**, the "?" in its top-right corner a short **help** (touch is player 1,
gamepads players 2 to 4, the pause and back buttons). The pad's A / B hints appear only when a gamepad is connected.
The promotion of the full version stays **in the free app's menu only**: no pop-up, no interstitial, nothing during a
game, so neither app needs Google Play's "Contains ads" label.

![the menu](../launcher/docs/launcher-520.png)

## 1. Install an app on your phone

The build makes `dist/android/RetroStoneArcadeFree-debug.apk` and `dist/android/RetroStoneArcade-debug.apk`: test
versions, signed with a "debug" key (fine for your own phones, not for Google Play). The two apps install side by
side, each with its own saves.

**Without a cable** (simplest):
1. Copy the APK to the phone (USB file transfer, Google Drive, an e-mail to yourself...).
2. On the phone, open it with the **Files** app. Android asks to **allow installing unknown apps** for that app
   (Settings > Apps > Special app access > Install unknown apps): allow it, go back, tap **Install**.
3. Play Protect may warn that the app is unknown: choose "Install anyway" (it is your own app).

**With a cable** (handy when you rebuild often):
1. On the phone: Settings > About phone > tap **Build number** 7 times (developer mode), then Settings > System >
   Developer options > **USB debugging** on.
2. Plug the phone in, accept the "Allow USB debugging?" prompt, then in WSL:
   ```
   ~/android-app-sdk/sdk/platform-tools/adb install -r dist/android/RetroStoneArcade-debug.apk
   ```
   (WSL does not see USB devices by itself: either use the Windows `adb.exe` from the Android platform-tools for
   Windows, or `usbipd` to pass the phone to WSL.)

A new build installs over the old one and keeps the saves, as long as it is signed with the same key. The debug key
is `~/android-app-sdk/android-user/debug.keystore`: if it is deleted, a new one is made and the phone refuses the
update ("App not installed"): uninstall the app first (the saves go with it). A debug build and a release build
(your upload key) of the same app cannot replace each other either: uninstall first.

**Playing.** Landscape, full screen. The app speaks the phone's language (22 languages: the games' texts and the
menu's; the game names stay as they are). Tap a card to play (in the free app a padlock opens the full version's
page, whose big "Get it on Google Play" button opens the store; a tap outside it, its back arrow or Back closes it;
drag to scroll). The touch controls of each game: section 4. In a game, the small **pause** button is in the top-right
corner and the **back** button in the top-left corner: back, like the phone's **Back**, returns to the menu (the game
is saved where it was and waits for a tap when you come back to it); Back in the menu leaves the app. Bluetooth and USB gamepads work as the SNES pad (Guide button = Back); on a
phone, touch is player 1 and gamepads are players 2 to 4. Leaving the app (home button, a call) saves the game too.

## 2. Build

In WSL (Ubuntu 24.04), from the repository's folder:
```
apps/android/setup-toolchain.sh        # once: Java 17, the Android SDK and NDK, Gradle (in ~/android-app-sdk, 3.8 GB)
apps/android/build.sh free             # dist/android/RetroStoneArcadeFree-debug.apk
apps/android/build.sh full             # dist/android/RetroStoneArcade-debug.apk
apps/android/build.sh full --release   # also the signed .aab (Google Play) and .apk (section 5)
```
`build.sh` builds SDL2 and the app's games for the three kinds of phone processors (arm64-v8a: almost every phone;
armeabi-v7a: old 32-bit phones; x86_64: the emulator and Chromebooks), then the APK, then prints its contents. Each
app builds in `build/android/<free|full>/` (SDL2 is shared: `build/android/sdl2`). The toolchain lives only in
`~/android-app-sdk` (nothing in the system or in other projects; the LineageOS tree is not touched). The same apps on
the PC: `make -f apps/launcher/bundle.mk PLATFORM=win64` (the free one) and
`make -f apps/launcher/bundle.mk PLATFORM=win64 CONF=apps/android/app-full.conf` (the full one).

Versions: OpenJDK 17, Android command-line tools 23.0, platform android-36 (Android 16; minSdk 21 = Android 5.0),
build-tools 36.1.0, NDK r27d (27.3.13750724), Gradle 9.8.0 (wrapper), Android Gradle Plugin 9.4.1, SDL 2.32.10.
They are pinned in `setup-toolchain.sh`, `build.gradle` and `gradle/wrapper/gradle-wrapper.properties`.

**Widescreen.** A game that opted in (`rs_game.max_width`, docs/spec.md "Widescreen") fills the phone's width; the
others run at 320 columns and the app draws side borders (their own picture, blurred). Nothing to configure: a game
that opts in later fills the screen at the next build.

## 3. The two configs

`app.conf` (free) and `app-full.conf` (full) have the same keys:

| Key | What |
|---|---|
| `PROFILE` | `free` or `full`: which app (`build.sh free|full` picks the file) |
| `APP_NAME` | the name under the icon and in the menu's header |
| `APP_ID` | the package id. **Permanent once on Google Play** (it identifies the app forever): choose it before the first upload. Changing it makes another app (the saves of the old one are not carried over). |
| `APP_VERSION_NAME`, `APP_VERSION_CODE` | the version shown, and a number that must grow with every upload to Google Play |
| `PAID_APP_NAME`, `PAID_APP_ID`, `PAID_STORE_URL` | free app: the paid app (its `APP_NAME` and `APP_ID` in app-full.conf): the "Get the full version" button opens `market://details?id=<PAID_APP_ID>` (the Play Store app), or `PAID_STORE_URL` in the browser when there is no Play Store. Empty in the full app |
| `FREE_GAMES` | the playable games in menu order; every other game of `games/` (with a `docs/screenshots/title.png`) is shown locked, so a new game appears locked in the free app at the next build. `all` (the full app): every game, playable, by name |
| `HIDDEN_GAMES` | never shown (Bomber Mole) |
| `EXE_NAME` | the file names: `dist/android/<EXE_NAME>-debug.apk`, `dist/windows/<EXE_NAME>.exe` |
| `ICON_GAMES` | (full app) the first title screens of the icon |

The icons are drawn by `tools/make_icon.py`: the free app's, four title screens on navy in a gold frame; the full
app's, nine on ruby. Google Play's 512 px icon goes to `dist/android/<EXE_NAME>-icon-512.png` (and the store kit).

## 4. Touch controls

On a phone, the touches are **player 1** (players 2 to 4 use gamepads). The pause button is always in the top-right
corner, the back button in the top-left one: a finger that goes down on them never reaches the game. On the desktop
(`dist/windows/*.exe`) the **mouse's left button is the finger**, to try the touch controls without a phone.

Two kinds of games:
- **App-level** (13 games): `apps/launcher/games.cfg` turns the touches into the virtual pad: screen zones that hold
  pad buttons (the game's code knows only the pad).
- **Game-side** (7 games): the game reads the touches itself, through the SDK's touch API (`rs_touch*()`,
  docs/spec.md "Touch"; the game declares `rs_game.touch_native`): the app passes it the fingers, in its screen
  pixels, with no zone and no drawn button. Its menus take taps too (a tap starts from the title, a tap on the
  game-over panel goes back to it).

**All 20 schemes are final** (the owner's design), to be tuned after the owner's phone test:

| Game | Touch controls | |
|---|---|---|
| Leady Squid | tap anywhere = A (swim) | app |
| Pancake Tower | tap anywhere = A (drop) | app |
| Beaver Rush | the left / right half = Left / Right | app |
| Frog Reach | hold anywhere = A | app |
| Hedgehog Hills | hold anywhere = A | app |
| Mouse Espresso | hold anywhere = A | app |
| Blueberry Tumble | tap anywhere = A | app |
| Snail Switch | tap anywhere = A | app |
| Penguin Slalom | hold the left / right half = Left / Right | app |
| Otter Rapids | hold the left / right half = Left / Right | app |
| Crab Coconut | hold the left / right half = Left / Right | app |
| Biscuit Bandit | hold the left / right half = Left / Right (each half also presses A: the only input that starts it from its title; A does nothing in a run) | app |
| Duck Parade | tap a spot: the duck hops there one hop at a time along the direct path (each hop along the axis with the larger distance left), and falls into the water on the way; a new tap retargets, a tap on the duck stops it; a small marker shows the target | game |
| Radish Rascals | tap the bed to defend | game |
| Raccoon Recycler | tap the branch the toy should take | game |
| Moth Around | hold the left / right half = turn one way / the other around the lantern | app |
| Nutty Branch | hold the left / right half to steer; swipe up to jump (keep the finger down for the higher jump) | game |
| Pogo Mamie | hold the left / right half to steer; swipe up = the big bounce (it arms the next landing; a small cue shows it) | game |
| Mole Marbles | the mole walks at its normal speed to your finger's x; a quick tap shoots (and sends it there) | game |
| Spider Swing | tap to let go of the web; in flight, hold the left / right half to steer | game |

## 5. Publishing

### The upload key: create it once, keep it forever
```
apps/android/make-keystore.sh
```
asks for your name and a password and writes, **outside the repository**:
- `~/.retrostone-keys/upload-keystore.jks`: the key (RSA 4096, valid 30 years, alias `upload`), for both apps;
- `~/.retrostone-keys/arcade.properties`: where it is and its password (`storeFile`, `storePassword`, `keyAlias`,
  `keyPassword`), read by `build.sh --release` (`RSA_SIGNING=<file>` to use another file).

Then `apps/android/build.sh full --release` and `apps/android/build.sh free --release` give
`dist/android/<EXE_NAME>-release.aab` (upload this to Google Play) and `<EXE_NAME>-release.apk` (sideloading), and
check both signatures (`apksigner verify`, `jarsigner -verify`). Without the properties file, `--release` stops (no
unsigned release by mistake).

**Keep the .jks file and its password safe, in two places at least** (a password manager or an encrypted cloud
folder, and an offline copy such as a USB key at home; from Windows the folder is
`\\wsl$\Ubuntu-24.04\root\.retrostone-keys`). **Keep "Play App Signing" on** when you create the apps (the default
for new apps): Google keeps the real app signing key, and this upload key can be reset through Play support if it is
lost (it takes days); without Play App Signing, a lost key means the app can never be updated again. Never commit
it (`.gitignore` refuses `*.jks` and `*.keystore` anyway), never send it by e-mail.

### The store kit
Everything else Google Play asks for is in [`store/`](store/):

| | |
|---|---|
| [`store/CHECKLIST.md`](store/CHECKLIST.md) | the steps: the developer account ($25), the closed test (12 testers, 14 days), creating the apps (paid vs free), the listing, pricing, countries, review |
| `store/<free\|full>/listings/<language>/` | the title, short and full descriptions in 22 languages (English source, French with "tu", **the 20 others AI-made: to review**) |
| `store/<free\|full>/graphics/` | the 512 x 512 icon and the 1024 x 500 feature graphic, code-drawn |
| `store/<free\|full>/screenshots/<en\|fr\|de>/` | phone screenshots, 2160 x 1080, made headlessly from the apps |
| `store/privacy/` | the privacy policy (EN, FR; Markdown and HTML ready to host) |
| `store/play-console-answers.md` | draft answers: Data safety ("no data collected"), the IARC content rating, ads, target audience |
| `store/tools/` | the generators: `make_listings.py`, `make_store_art.py`, `store_shots.py`, `privacy_html.py` |

## 6. What was tested

- The bundle's tests, for both apps (`make -f apps/launcher/bundle.mk check` and the same with
  `CONF=apps/android/app-full.conf`, Linux): each game identical to its standalone build (fresh, at width 520 and
  after another game), the flow with touches, the store request (free) or none (full), the credits screens, every
  game launching in one process with its own save state, each game's touch scheme (the game-side ones: the touches
  reach the game, and never those that go down on the pause or back button), the launcher in 22 languages.
- The game-side touch controls: each of the 7 games' `make <game>-check` plays them with scripted touches (headless,
  at 320, 427 and 520 columns where it applies), and its pad play stays byte-identical without touches.
- The APKs: built, their ABIs and 16 KB page alignment checked by `build.sh`; the release path (`--release`) with a
  throwaway key: a signed AAB and APK. `apps/android/smoke-emulator.sh` ran the free app in an Android 11 x86_64
  emulator earlier (the emulator, ~4 GB, was removed afterwards: `setup-toolchain.sh --emulator` installs it again).
- **Not tested on a real phone yet**: the feel of the touch controls (the game-side ones were played only with
  scripted touches: real fingers, several at once, the thresholds of a tap, a swipe and a hold; the mouse as the
  finger in the desktop apps was not tried by hand either), the drawn buttons' size, the audio latency, the integer
  scaling on the phone's real resolution, the display cutout, Bluetooth / USB pads, the Back gesture of gesture
  navigation, the Play Store link (the paid app is not on Google Play yet: the store says "not found").

MIT licence (the apps' code), (c) 2026 Pierre-Louis Boyer (8BCraft). The games' assets: CC BY-NC-SA 4.0.
