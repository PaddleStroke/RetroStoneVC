# RetroStone Arcade Free (Android)

The free Android app of the RetroStone VC games: **Leady Squid, Pancake Tower and Beaver Rush** to play, and every
other game shown with a padlock to advertise the full (paid) app. Its menu, the games and the touch controls are
described in [apps/launcher/README.md](../launcher/README.md).

![the menu](../launcher/docs/launcher-520.png)

## 1. Install the app on your phone

The build makes **`dist/android/RetroStoneArcadeFree-debug.apk`**: a test version, signed with a "debug" key (fine
for your own phones, not for Google Play).

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
   ~/android-app-sdk/sdk/platform-tools/adb install -r dist/android/RetroStoneArcadeFree-debug.apk
   ```
   (WSL does not see USB devices by itself: either use the Windows `adb.exe` from the Android platform-tools for
   Windows, or `usbipd` to pass the phone to WSL.)

A new build installs over the old one and keeps the saves, as long as it is signed with the same key. The debug key
is `~/android-app-sdk/android-user/debug.keystore`: if it is deleted, a new one is made and the phone refuses the
update ("App not installed"): uninstall the app first (the saves go with it).

**Playing.** Landscape, full screen. The app speaks the phone's language (22 languages: the games' texts and the menu's; the
game names stay as they are). Tap a card to play (a padlock opens the full version's page; drag to scroll).
In a game: Leady Squid and Pancake Tower: tap anywhere (A); Beaver Rush: tap the left or the right half. The small
pause button is in the top-right corner. The phone's **Back** returns to the menu (the game is saved where it was and
waits for a tap when you come back to it); Back in the menu leaves the app. Bluetooth and USB gamepads work as the
SNES pad (Guide button = Back). Leaving the app (home button, a call) saves the game too.

## 2. Rebuild

In WSL (Ubuntu 24.04), from the repository's folder:
```
apps/android/setup-toolchain.sh     # once: Java 17, the Android SDK and NDK, Gradle (in ~/android-app-sdk, 3.8 GB)
apps/android/build.sh               # the APK: dist/android/RetroStoneArcadeFree-debug.apk
```
`build.sh` builds SDL2 and the games for the three kinds of phone processors (arm64-v8a: almost every phone;
armeabi-v7a: old 32-bit phones; x86_64: the emulator and Chromebooks), then the APK, then prints its contents. The
toolchain lives only in `~/android-app-sdk` (nothing in the system or in other projects; the LineageOS tree is not
touched). The same games on the PC: `make -f apps/launcher/bundle.mk PLATFORM=win64` gives
`dist/windows/RetroStoneArcadeFree.exe`.

Versions: OpenJDK 17, Android command-line tools 23.0, platform android-36 (Android 16; minSdk 21 = Android 5.0),
build-tools 36.1.0, NDK r27d (27.3.13750724), Gradle 9.8.0 (wrapper), Android Gradle Plugin 9.4.1, SDL 2.32.10.
They are pinned in `setup-toolchain.sh`, `build.gradle` and `gradle/wrapper/gradle-wrapper.properties`.

## 3. The name, the package id, the Play link

All in **`apps/android/app.conf`**:

| Key | What |
|---|---|
| `APP_NAME` | the name under the icon and in the menu's header |
| `APP_ID` | the package id, e.g. `com.eightbcraft.retrostonearcade.free`. **Permanent once on Google Play** (it identifies the app forever): choose it before the first upload. Changing it makes another app (the saves of the old one are not carried over). |
| `APP_VERSION_NAME`, `APP_VERSION_CODE` | the version shown, and a number that must grow with every upload to Google Play |
| `PAID_APP_NAME`, `PAID_APP_ID`, `PAID_STORE_URL` | the paid app (placeholders): the "Get the full version" button opens `market://details?id=<PAID_APP_ID>` (the Play Store app), or `PAID_STORE_URL` in the browser when there is no Play Store |
| `FREE_GAMES` | the playable games; every other game of `games/` (with a `docs/screenshots/title.png`) is shown locked. A new game appears locked at the next build, with nothing to edit |
| `HIDDEN_GAMES` | never shown (Bomber Mole) |

The icon is drawn by `tools/make_icon.py` (the four first games' title screens in a gold frame); Google Play's 512 px
icon is written to `dist/android/icon-512.png`. The touch controls of each game: `apps/launcher/games.cfg`.

## 4. Publishing (to do before the first upload)

### The release key: create it once, keep it forever
Google Play identifies the app's updates by their signature. Create your own **upload key** once:
```
keytool -genkeypair -v -keystore ~/retrostone-upload.jks -alias upload -keyalg RSA -keysize 4096 -validity 10000
```
(`keytool` comes with Java: `/usr/lib/jvm/java-17-openjdk-amd64/bin/keytool`.) Then write
`~/android-app-sdk/release-signing.properties` (never in the repository):
```
storeFile=/root/retrostone-upload.jks
storePassword=...
keyAlias=upload
keyPassword=...
```
and build with `apps/android/build.sh --release`: `dist/android/RetroStoneArcadeFree-release.aab` (for Google Play) and
`...-release.apk`.

**Keep the .jks file and its passwords safe, in two places at least** (a password manager and an offline copy). With
"Play App Signing" (the default for new apps) Google keeps the real app signing key and the upload key can be reset
through Play support if it is lost, but it takes days; without Play App Signing, a lost key means the app can never
be updated again. Never commit it, never send it by e-mail.

### Google Play checklist
- [ ] A **Google Play developer account**: $25, once, with identity verification (allow a few days). New personal
      accounts must run a **closed test with at least 12 testers for 14 days** before going public.
- [ ] The final `APP_ID` and name (see above), `APP_VERSION_CODE` 1 for the first upload.
- [ ] The **release key** (above) and an **AAB** (`build.sh --release`): Google Play takes .aab files, not .apk.
- [ ] **Target API level**: Google Play requires a recent `targetSdk` (36 here; check the yearly deadline each August).
- [ ] The store listing: the short and full description, the **512x512 icon** (`dist/android/icon-512.png`), a
      **1024x500 feature graphic**, at least **2 phone screenshots** (16:9 or 9:16, e.g. `apps/launcher/docs/*.png`
      scaled up, or screenshots from the phone), the category (Games > Arcade), a contact e-mail.
- [ ] The **content rating** questionnaire (IARC): no violence, no ads, no purchases -> rated for everyone.
- [ ] The **privacy policy** URL (required for every app): the app collects nothing, does not use the network, has no
      ads and no accounts; a one-page policy saying so, on a web page you control.
- [ ] The **Data safety** form: "no data collected, no data shared".
- [ ] "Contains ads": no. The full version is promoted only inside the menu (the locked cards, the "Get the full
      version" panel, the small "full version" banner of the footer): never a pop-up, an interstitial or a banner during
      a game. Keep it that way (Google Play counts in-game promotion of other apps as ads). Target audience: if children
      are included, the Families policy applies (fine here: no ads, no data).
- [ ] The paid app published first (or at the same time), and its real package id in `PAID_APP_ID`.

## 5. What was tested

- The bundle's tests (`make -f apps/launcher/bundle.mk check`, Linux and the Windows build): each game identical to
  its standalone build, the switching, the saves, the resume, the touch flow, the store request.
- The APK: built, its ABIs and 16 KB page alignment checked by `build.sh`; `apps/android/smoke-emulator.sh` ran it in an
  Android 11 x86_64 emulator: the menu, a tap on a card starts Leady Squid, a tap plays, Back returns to the menu,
  Home saves the game and it resumes held under the play sign. (The emulator, ~4 GB, was removed afterwards to
  save disk space: `setup-toolchain.sh --emulator` installs it again.)
- **Not tested on a real phone yet**: the feel of the touch controls, the audio latency, the integer scaling on the
  phone's real resolution, the display cutout (the camera hole may hide a few columns on the left in landscape),
  Bluetooth / USB pads, the Back gesture of gesture navigation, the Play Store link (the paid app does not exist yet:
  the store will say "not found").

MIT licence (the app's code), (c) 2026 Pierre-Louis Boyer (8BCraft). The games' assets: CC BY-NC-SA 4.0.
