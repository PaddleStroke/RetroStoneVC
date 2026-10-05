#!/bin/bash
# RetroStone Arcade: creates YOUR upload key for Google Play, once (WSL / Linux). Both apps use it.
#
#   apps/android/make-keystore.sh
#
# Writes, OUTSIDE the repository (never commit them, never send them by e-mail):
#   ~/.retrostone-keys/upload-keystore.jks    the key (RSA 4096, valid 30 years), alias "upload"
#   ~/.retrostone-keys/arcade.properties      where it is and its password, read by apps/android/build.sh --release
# KEYS_DIR=<dir> puts them elsewhere (then build with RSA_SIGNING=<dir>/arcade.properties).
#
# Google Play App Signing (the default for new apps, recommended): Google keeps the key that signs what the
# phones install; this one only proves to Google that an upload comes from you. If it is lost or stolen, Google
# support can reset it (it takes days). Without Play App Signing, a lost key means the app can never be updated.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -euo pipefail

DIR=${KEYS_DIR:-$HOME/.retrostone-keys}
STORE=$DIR/upload-keystore.jks
PROPS=$DIR/arcade.properties
ALIAS=upload
SDK_ROOT=${ANDROID_APP_SDK:-$HOME/android-app-sdk}
[ -f "$SDK_ROOT/env.sh" ] && . "$SDK_ROOT/env.sh"
KEYTOOL=$(command -v keytool || true)
[ -n "$KEYTOOL" ] || KEYTOOL=/usr/lib/jvm/java-17-openjdk-amd64/bin/keytool
[ -x "$KEYTOOL" ] || { echo "keytool (Java) is missing: run apps/android/setup-toolchain.sh first." >&2; exit 1; }

if [ -e "$STORE" ] || [ -e "$PROPS" ]; then
    echo "A key already exists: $STORE / $PROPS"
    echo "Keep it: Google Play knows your apps by it. To make a new one anyway, move these files away first."
    exit 1
fi

cat <<EOF
RetroStone Arcade: your Google Play upload key
-----------------------------------------------
It goes to $DIR (outside the repository). You will choose ONE password (6 characters
at least, a long one is better): write it in your password manager now.

The certificate shows a name and a place (Google Play does not display them). Press Enter to accept a default.
EOF
read -r -p "Your name or your company's [Pierre-Louis Boyer]: " CN
read -r -p "Organisation [8BCraft]: " O
read -r -p "City []: " L
read -r -p "Two-letter country code [FR]: " C
CN=${CN:-Pierre-Louis Boyer}; O=${O:-8BCraft}; C=${C:-FR}
esc() { printf '%s' "$1" | sed 's/[,+"\\<>;=]/\\&/g'; }
DNAME="CN=$(esc "$CN"), O=$(esc "$O")${L:+, L=$(esc "$L")}, C=$(esc "$C")"

while :; do
    read -r -s -p "Password (not shown): " P1; echo
    read -r -s -p "The same password again: " P2; echo
    [ "$P1" = "$P2" ] || { echo "They differ, again."; continue; }
    [ ${#P1} -ge 6 ] || { echo "6 characters at least, again."; continue; }
    break
done

umask 077
mkdir -p "$DIR"
chmod 700 "$DIR"
export RSA_KS_PASS="$P1"                    # given to keytool through the environment, never on its command line
"$KEYTOOL" -genkeypair -keystore "$STORE" -storetype PKCS12 -alias "$ALIAS" -keyalg RSA -keysize 4096 \
    -validity 10950 -dname "$DNAME" -storepass:env RSA_KS_PASS -keypass:env RSA_KS_PASS > /dev/null
PQ=${P1//\\/\\\\}                            # a Java properties file: a backslash is written twice
{
    echo "# RetroStone Arcade: the upload key (apps/android/make-keystore.sh). NEVER commit this file."
    echo "storeFile=$STORE"
    echo "storePassword=$PQ"
    echo "keyAlias=$ALIAS"
    echo "keyPassword=$PQ"
} > "$PROPS"
chmod 600 "$STORE" "$PROPS"

echo
"$KEYTOOL" -list -v -keystore "$STORE" -storetype PKCS12 -storepass:env RSA_KS_PASS 2>/dev/null \
    | grep -E "^(Alias name|Owner|Valid until|.*SHA256:)" | sed 's/^/  /' || true
unset RSA_KS_PASS
P1=; P2=; PQ=
WIN='\\wsl$\'"${WSL_DISTRO_NAME:-Ubuntu-24.04}${DIR//\//\\}"     # the same folder, as Windows' Explorer names it
cat <<EOF

Done: $STORE
      $PROPS (read by apps/android/build.sh free --release and full --release)

NOW, before anything else:
  1. Back it up, in two places at least: copy the folder $DIR to a USB key kept at home AND to
     your password manager or an encrypted cloud folder (from Windows: $WIN).
     The password is in arcade.properties: keep that file as private as the key.
  2. Keep "Play App Signing" on when you create each app in the Play Console (it is the default): Google then
     holds the real signing key, and this upload key can be reset by Google support if it is ever lost.
  3. Never commit these files, never send them by e-mail, never paste the password in a chat.
EOF
