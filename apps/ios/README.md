# The iPhone and iPad apps

**RetroStone Board** (`apps/boardgames`) for iPhone and iPad: ONE free app per product, a one-time in-app purchase
("unlock everything", non-consumable) opens everything at once. Built on GitHub's macOS runners
(`.github/workflows/ios.yml`): nobody needs a Mac. Same C code as the desktop and Android builds; what is iOS's
lives here and behind `#ifdef __IPHONEOS__`.

| | RetroStone Board |
|---|---|
| bundle id | `com.eightbcraft.retrostoneboard` |
| in-app purchase (non-consumable) | `com.eightbcraft.retrostoneboard.unlock` |
| identity | `apps/ios/board/app.conf` |
| version | `APP_VERSION_NAME` of `apps/android/app-board.conf`; build number: the CI run's number |
| free before the purchase | `FREE_GAMES`, `FREE_LEVELS` of `apps/android/app-boardfree.conf` |
| orientation | portrait and landscape, iPhone and iPad (full screen) |

## How it is built

1. **`apps/ios/prepare.sh board`** (Linux; the CI's `prepare` job): the generated C, with the same tools as every
   other build: the texts and their fonts (`gen_text.py`: needs the Linux font packages, so it runs there), the
   pictures, the edition (`apps/ios/board/app.conf` + the free set: `build/ios/board/edition.conf`); the icon
   (`apps/ios/common/make_icon.py`: the Android icon's artwork, one 1024 px opaque PNG, Xcode makes every other size
   from it); `app.env` (name, bundle id, version). `--preview`: pictures of the unlock panel from the desktop
   build with a stand-in for StoreKit (`apps/ios/tools/iap_preview.c`), the CI's `ios-panel-preview` artifact.
2. **`apps/ios/build-sdl.sh`** (macOS): SDL2's source release (`tools/fetch_sdl2_src.sh`, the version Android uses)
   built by its own Xcode project, for the devices and the simulator: `build/ios/sdl` (cached by the CI).
3. **`apps/ios/build.sh board simulator|ipa|upload`** (macOS): the Xcode project from `apps/ios/board/project.yml`
   (XcodeGen: nothing of Xcode's is kept in git), then `xcodebuild`: the simulator's app (unsigned), or a signed
   archive, its `.ipa`, and the upload to App Store Connect.
4. **`apps/ios/sim-shots.sh board`** (macOS): the app in an iPhone and an iPad simulator, screenshots (the CI's
   `ios-simulator-screenshots` artifact); fails if the app is not running after 20 s.

The pieces: `common/ios_app.m` (the safe area from UIKit, the entry point), `common/iap_storekit.swift` (the
in-app purchase, StoreKit 2: `apps/common/iap.h`), `common/LaunchScreen.storyboard` (the table's green),
`board/project.yml` (sources, frameworks, Info.plist: full screen, no status bar, every orientation, no
non-exempt encryption).

**Safe area**: the shell lays out and draws inside it (the notch, the Dynamic Island, the rounded corners, the home
indicator stay clear), the table's green fills the rest. **Saves**: `SDL_GetPrefPath` = the app's
`Library/Application Support/8BCraft/RetroStoneBoard` (backed up by iOS). **Sound**: the ambient category (the
silent switch mutes it).

## The in-app purchase

`apps/common/iap.h`, StoreKit 2. The free set until it is bought; then everything, at once. The unlock is kept in
the app's save (`shell/unlocked`) and checked again with StoreKit at every launch (a refund locks again; a purchase
on another device of the same Apple Account unlocks). The panel (a locked game or level tapped): "Unlock
everything" with the price from the App Store, and "Restore purchases". Without App Store Connect (the simulator in
the CI) the price is missing and the button answers with the deny sound: expected.

## GitHub secrets (Settings > Secrets and variables > Actions)

Without them the CI builds and runs the simulator app only. With all of them, the `testflight` job signs and uploads
each push to `main` (and each manual run) to TestFlight.

| secret | what |
|---|---|
| `APPLE_TEAM_ID` | the team id (developer.apple.com > Account > Membership: 10 characters) |
| `IOS_DIST_CERT_P12_BASE64` | an **Apple Distribution** certificate with its private key, exported as .p12, then `base64 -w0 cert.p12` |
| `IOS_DIST_CERT_PASSWORD` | the .p12's password |
| `IOS_BOARD_PROFILE_BASE64` | an **App Store** provisioning profile for `com.eightbcraft.retrostoneboard` with that certificate, `base64 -w0 x.mobileprovision` |
| `ASC_KEY_ID` | App Store Connect > Users and Access > Integrations > App Store Connect API: a team key (role App Manager), its Key ID |
| `ASC_ISSUER_ID` | the Issuer ID shown above the keys |
| `ASC_KEY_P8` | the text of the downloaded `AuthKey_<id>.p8` (it downloads once only) |

Making the certificate without a Mac (OpenSSL, e.g. in WSL):
`openssl req -new -newkey rsa:2048 -nodes -keyout dist.key -out dist.csr -subj "/CN=8BCraft/emailAddress=<you>"`;
upload `dist.csr` at developer.apple.com > Certificates > + > Apple Distribution; download `distribution.cer`; then
`openssl x509 -inform DER -in distribution.cer -out dist.pem` and
`openssl pkcs12 -export -legacy -inkey dist.key -in dist.pem -out dist.p12` (the password asked: the secret).

## What App Store Connect needs (once)

1. Identifiers (developer.apple.com > Certificates, IDs & Profiles > Identifiers): the App ID
   `com.eightbcraft.retrostoneboard` (In-App Purchase is on by default).
2. The provisioning profile: Profiles > + > App Store Connect, that App ID, the Apple Distribution certificate.
3. App Store Connect > Apps > +: a new iOS app, bundle id above, SKU e.g. `retrostoneboard`.
4. The app > Monetization > In-App Purchases > +: **Non-Consumable**, product id
   `com.eightbcraft.retrostoneboard.unlock`, a price, a display name and description per language, a review
   screenshot (the unlock panel: the `ios-panel-preview` artifact). It is submitted with the app's first version.
5. Business: the Paid Apps agreement signed, bank and tax forms (in-app purchases need them).
