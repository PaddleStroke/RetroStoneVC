# The iPhone and iPad apps

**RetroStone Board** (`apps/boardgames`) and **RetroStone Arcade** (`apps/launcher` and `games/`) for iPhone and
iPad: ONE free app per product, a one-time in-app purchase ("unlock everything", non-consumable) opens everything at
once. Built on GitHub's macOS runners (`.github/workflows/ios.yml`): nobody needs a Mac. Same C code as the desktop
and Android builds; what is iOS's lives here and behind `#ifdef __IPHONEOS__`.

| | RetroStone Board | RetroStone Arcade |
|---|---|---|
| bundle id | `com.eightbcraft.retrostoneboard` | `com.eightbcraft.retrostonearcade` |
| in-app purchase (non-consumable) | `com.eightbcraft.retrostoneboard.unlock` | `com.eightbcraft.retrostonearcade.unlock` |
| identity | `board/app.conf` | `arcade/app.conf` |
| version | `APP_VERSION_NAME` of `apps/android/app-board.conf` | `APP_VERSION_NAME` of `apps/android/app-full.conf` |
| free before the purchase | `FREE_GAMES`, `FREE_LEVELS` of `apps/android/app-boardfree.conf` | `FREE_GAMES` (and `HIDDEN_GAMES`) of `apps/android/app.conf` |
| orientation | portrait and landscape | landscape |

The build number (`CFBundleVersion`): the CI run's number. Both apps: iPhone and iPad, full screen, no status bar.

## How it is built

1. **`apps/ios/prepare.sh board|arcade`** (Linux; the CI's `prepare` job): the generated sources, with the same
   tools as every other build (they need Linux's fonts and Python's Pillow, numpy, fontTools):
   - Board: the texts and their fonts, the pictures, the edition (`board/app.conf` + the free set:
     `build/ios/board/edition.conf`), into `build/ios/board/gen`;
   - Arcade: the games' assets and texts, the launcher's texts, the credits, the catalogue
     (`apps/launcher/bundle.mk` with `build/ios/arcade/app.conf`: `build/bundle/gen-ios`; the modules: `games.txt`);
   - the icon (`common/make_icon.py`: the Android icon's artwork, one 1024 px opaque PNG; Xcode makes the other
     sizes), `app.env` (name, bundle id, version);
   - `--preview`: pictures of the unlock panel from the desktop build with a stand-in for StoreKit
     (`tools/iap_preview.c`): the CI's `ios-panel-preview` artifact.
2. **`apps/ios/build-sdl.sh`** (macOS): SDL2's source release (`tools/fetch_sdl2_src.sh`, the version Android uses)
   built by its own Xcode project, for the devices and the simulator: `build/ios/sdl` (cached by the CI).
3. **`apps/ios/build.sh board|arcade simulator|ipa|upload`** (macOS): the Arcade first makes `libarcade.a`
   (`apps/launcher/bundle.mk PLATFORM=ios-<sdk>-arm64 ios-lib`, `arcade/ios.mk`); then the Xcode project from
   `<app>/project.yml` (XcodeGen: nothing of Xcode's is kept in git) and `xcodebuild`: the simulator's app
   (unsigned), or a signed archive, its `.ipa`, and the upload to App Store Connect.
4. **`apps/ios/sim-shots.sh board|arcade`** (macOS): the app in an iPhone and an iPad simulator, screenshots (the
   CI's `ios-<app>-simulator` artifact); fails if the app is not running after 20 s.

The pieces: `common/ios_app.m` (the safe area from UIKit, the entry point), `common/iap_storekit.swift` (the
in-app purchase, StoreKit 2: `apps/common/iap.h`), `common/LaunchScreen.storyboard` (the table's green),
`<app>/project.yml` (sources, frameworks, Info.plist).

**The Arcade's games on iOS**: every game is a module of one program (`apps/launcher/tools/modulize.py`). Apple's
tools cannot rename sections afterwards as on Linux and Windows, so each module's writable data is compiled straight
into sections of its own (`#pragma clang section`, `arcade/ios.mk`, `arcade/sections.sh`), the glue takes their
bounds from the linker, and `ld -r -exported_symbol` keeps one global symbol per game (`--flavor macho`).

**Safe area**: everything stays clear of the notch, the Dynamic Island, the rounded corners and the home indicator:
the Board's shell lays out inside it (the table's green fills the rest); the Arcade's picture, touch buttons, pause
and back buttons and the touches all live inside it (black around). **Saves**: `SDL_GetPrefPath`, the app's
`Library/Application Support/8BCraft/...` (backed up by iOS). **Sound**: the ambient category (the silent switch
mutes it). The Arcade defers the home indicator's swipe (a first swipe is the game's).

## The in-app purchase

`apps/common/iap.h`, StoreKit 2. The free set until it is bought; then everything, at once, without a restart. The
unlock is kept (Board: its save, `shell/unlocked`; Arcade: `unlock.cfg` beside the games' saves) and checked again
with StoreKit at every launch (a refund locks again; a purchase on another device of the same Apple Account unlocks).
The panel (a locked game or level tapped): "Unlock everything" with the price from the App Store, and "Restore
purchases". Without App Store Connect (the simulator in the CI) there is no price and the button answers no: expected.

## GitHub secrets (Settings > Secrets and variables > Actions)

Without them the CI builds and runs the simulator apps only. With them, the `testflight` job signs and uploads each
push to `main` (and each manual run) to TestFlight; an app without its profile secret is skipped.

| secret | what |
|---|---|
| `APPLE_TEAM_ID` | the team id (developer.apple.com > Account > Membership: 10 characters) |
| `IOS_DIST_CERT_P12_BASE64` | an **Apple Distribution** certificate with its private key, exported as .p12, then `base64 -w0 dist.p12` |
| `IOS_DIST_CERT_PASSWORD` | the .p12's password |
| `IOS_BOARD_PROFILE_BASE64` | an **App Store** provisioning profile for `com.eightbcraft.retrostoneboard` with that certificate, `base64 -w0 x.mobileprovision` |
| `IOS_ARCADE_PROFILE_BASE64` | the same for `com.eightbcraft.retrostonearcade` |
| `ASC_KEY_ID` | App Store Connect > Users and Access > Integrations > App Store Connect API: a team key (role App Manager), its Key ID |
| `ASC_ISSUER_ID` | the Issuer ID shown above the keys |
| `ASC_KEY_P8` | the text of the downloaded `AuthKey_<id>.p8` (it downloads once only) |

Making the certificate without a Mac (OpenSSL, e.g. in WSL):
`openssl req -new -newkey rsa:2048 -nodes -keyout dist.key -out dist.csr -subj "/CN=8BCraft/emailAddress=<you>"`;
upload `dist.csr` at developer.apple.com > Certificates > + > Apple Distribution; download `distribution.cer`; then
`openssl x509 -inform DER -in distribution.cer -out dist.pem` and
`openssl pkcs12 -export -legacy -inkey dist.key -in dist.pem -out dist.p12` (the password asked: the secret).

## What App Store Connect needs (once, for each app)

1. Identifiers (developer.apple.com > Certificates, IDs & Profiles > Identifiers): the App ID (In-App Purchase is
   on by default).
2. The provisioning profile: Profiles > + > App Store Connect, that App ID, the Apple Distribution certificate.
3. App Store Connect > Apps > +: a new iOS app, its bundle id, a SKU (`retrostoneboard`, `retrostonearcade`).
4. The app > Monetization > In-App Purchases > +: **Non-Consumable**, its product id, a price, a display name and
   description per language, a review screenshot (the unlock panel: the `ios-panel-preview` artifact). It is
   submitted with the app's first version.
5. Business: the Paid Apps agreement signed, bank and tax forms (in-app purchases need them).
