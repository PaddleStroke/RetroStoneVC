#!/bin/sh
# The iOS apps, step 2 of 2 (macOS with Xcode and XcodeGen): the Xcode project, then the app. Run from anywhere,
# after apps/ios/prepare.sh <app> (build/ios/<app>/: the generated sources, the icon, app.env):
#
#   apps/ios/build.sh board simulator     build/ios/board/RetroStoneBoard.app (the simulator's, unsigned) and
#                                         RetroStoneBoard-simulator.zip
#   apps/ios/build.sh board ipa           a signed archive and build/ios/board/ipa/RetroStoneBoard.ipa (App Store)
#   apps/ios/build.sh board upload        the same, then sent to App Store Connect (TestFlight)
#
# ipa and upload sign by hand ("manual" signing: nothing is created in the developer account), with:
#   IOS_TEAM_ID        the Apple Developer team id (10 characters)
#   IOS_PROFILE        the App Store provisioning profile's UUID or name for this app's bundle id, installed in
#                      ~/Library/MobileDevice/Provisioning Profiles; its "Apple Distribution" certificate and key in
#                      the keychain (.github/workflows/ios.yml does both from the secrets)
# upload also needs App Store Connect's API key: ASC_KEY_PATH (the .p8 file), ASC_KEY_ID, ASC_ISSUER_ID.
# RS_BUILD_NUMBER: the build number (CFBundleVersion; the CI's run number), else the .conf's APP_VERSION_CODE.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
APP=${1:-}
MODE=${2:-simulator}
case "$APP" in board|arcade) ;; *) echo "usage: $0 board|arcade [simulator|ipa|upload]" >&2; exit 2 ;; esac
case "$MODE" in simulator|ipa|upload) ;; *) echo "usage: $0 board|arcade [simulator|ipa|upload]" >&2; exit 2 ;; esac
cd "$REPO"
OUT=build/ios/$APP
[ -f "$OUT/app.env" ] || { echo "$OUT/app.env is missing: run apps/ios/prepare.sh $APP first" >&2; exit 1; }
env_of() { sed -n "s/^$1=//p" "$OUT/app.env"; }
TARGET=$(env_of TARGET)
BUNDLE_ID=$(env_of BUNDLE_ID)
VERSION=$(env_of VERSION)
BUILD=${RS_BUILD_NUMBER:-$(env_of BUILD)}
say() { printf '\n== %s\n' "$*"; }

sh apps/ios/build-sdl.sh

say "the Xcode project (XcodeGen: apps/ios/$APP/project.yml)"
xcodegen generate --spec "apps/ios/$APP/project.yml" --project "$OUT"
PROJ=$OUT/$TARGET.xcodeproj
VERS="MARKETING_VERSION=$VERSION CURRENT_PROJECT_VERSION=$BUILD"

if [ "$MODE" = simulator ]; then
    say "$TARGET $VERSION ($BUILD) for the simulator"
    # shellcheck disable=SC2086  # VERS: two settings
    xcodebuild -project "$PROJ" -scheme "$TARGET" -configuration Release -sdk iphonesimulator \
        -destination "generic/platform=iOS Simulator" -derivedDataPath "$OUT/dd" $VERS \
        CODE_SIGNING_ALLOWED=NO build > "$OUT/xcodebuild-simulator.log" 2>&1 \
        || { grep -E "error:|warning: .*(SDL|iap|ios_app)" "$OUT/xcodebuild-simulator.log" | head -40;
             tail -40 "$OUT/xcodebuild-simulator.log"; exit 1; }
    grep -c "warning:" "$OUT/xcodebuild-simulator.log" | sed 's/^/warnings: /' || true
    rm -rf "$OUT/$TARGET.app"
    cp -R "$OUT/dd/Build/Products/Release-iphonesimulator/$TARGET.app" "$OUT/$TARGET.app"
    (cd "$OUT" && rm -f "$TARGET-simulator.zip" && zip -qr "$TARGET-simulator.zip" "$TARGET.app")
    echo "$OUT/$TARGET.app ($BUNDLE_ID)"
    exit 0
fi

: "${IOS_TEAM_ID:?IOS_TEAM_ID: the Apple Developer team id}"
: "${IOS_PROFILE:?IOS_PROFILE: the App Store provisioning profile (UUID or name) for $BUNDLE_ID}"
say "$TARGET $VERSION ($BUILD): the signed archive"
# shellcheck disable=SC2086
xcodebuild -project "$PROJ" -scheme "$TARGET" -configuration Release -sdk iphoneos \
    -destination "generic/platform=iOS" -derivedDataPath "$OUT/dd-device" -archivePath "$OUT/$TARGET.xcarchive" $VERS \
    DEVELOPMENT_TEAM="$IOS_TEAM_ID" CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY="Apple Distribution" \
    PROVISIONING_PROFILE_SPECIFIER="$IOS_PROFILE" archive > "$OUT/xcodebuild-archive.log" 2>&1 \
    || { grep -E "error:" "$OUT/xcodebuild-archive.log" | head -40; tail -40 "$OUT/xcodebuild-archive.log"; exit 1; }

# the export options: App Store Connect, the same manual signing; "upload" sends it straight to App Store Connect
export_options() {
    cat <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>method</key><string>app-store-connect</string>
    <key>destination</key><string>$1</string>
    <key>teamID</key><string>$IOS_TEAM_ID</string>
    <key>signingStyle</key><string>manual</string>
    <key>signingCertificate</key><string>Apple Distribution</string>
    <key>provisioningProfiles</key>
    <dict>
        <key>$BUNDLE_ID</key><string>$IOS_PROFILE</string>
    </dict>
    <key>uploadSymbols</key><true/>
    <key>manageAppVersionAndBuildNumber</key><false/>
</dict>
</plist>
EOF
}

say "the .ipa"
export_options export > "$OUT/ExportOptions.plist"
rm -rf "$OUT/ipa"
xcodebuild -exportArchive -archivePath "$OUT/$TARGET.xcarchive" -exportOptionsPlist "$OUT/ExportOptions.plist" \
    -exportPath "$OUT/ipa" > "$OUT/xcodebuild-export.log" 2>&1 || { tail -40 "$OUT/xcodebuild-export.log"; exit 1; }
ls -l "$OUT/ipa"

if [ "$MODE" = upload ]; then
    : "${ASC_KEY_PATH:?ASC_KEY_PATH: the App Store Connect API key (.p8)}"
    : "${ASC_KEY_ID:?ASC_KEY_ID: the id of the API key}"
    : "${ASC_ISSUER_ID:?ASC_ISSUER_ID: the issuer id of the API key}"
    say "$TARGET $VERSION ($BUILD) to App Store Connect (TestFlight)"
    export_options upload > "$OUT/ExportOptions-upload.plist"
    xcodebuild -exportArchive -archivePath "$OUT/$TARGET.xcarchive" \
        -exportOptionsPlist "$OUT/ExportOptions-upload.plist" -exportPath "$OUT/upload" \
        -authenticationKeyPath "$ASC_KEY_PATH" -authenticationKeyID "$ASC_KEY_ID" \
        -authenticationKeyIssuerID "$ASC_ISSUER_ID" > "$OUT/xcodebuild-upload.log" 2>&1 \
        || { tail -60 "$OUT/xcodebuild-upload.log"; exit 1; }
    tail -5 "$OUT/xcodebuild-upload.log"
fi
