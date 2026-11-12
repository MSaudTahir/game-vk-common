#!/usr/bin/env bash
# Sourced (not executed) by the game projects' tools/build_android.sh scripts.
# Each game script keeps only what is
# really app specific (cmake flags, FFmpeg, icons, manifest, APK / data names).
#
# Sets after gvc_android_env:
#   GVC_SDK GVC_NDK GVC_BT GVC_PLATFORM GVC_ANDROID_JAR GVC_STRIP   (env: ANDROID_HOME / ANDROID_SDK_ROOT,
#    ANDROID_NDK_HOME, ANDROID_PLATFORM — same defaults as the original scripts)

# Locate the Android SDK, newest NDK, newest build-tools, android.jar and llvm-strip.
gvc_android_env() {
    set -euo pipefail
    GVC_SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/sdk}}"
    GVC_NDK="${ANDROID_NDK_HOME:-$(ls -d "$GVC_SDK"/ndk/* 2>/dev/null | sort -V | tail -1)}"
    [ -d "$GVC_NDK" ] || { echo "Android NDK not found (set ANDROID_NDK_HOME)"; return 1; }
    GVC_BT="$(ls -d "$GVC_SDK"/build-tools/* | sort -V | tail -1)"
    GVC_PLATFORM="${ANDROID_PLATFORM:-34}"
    GVC_ANDROID_JAR="$GVC_SDK/platforms/android-$GVC_PLATFORM/android.jar"
    [ -f "$GVC_ANDROID_JAR" ] || GVC_ANDROID_JAR="$(ls "$GVC_SDK"/platforms/android-*/android.jar | sort -V | tail -1)"
    GVC_STRIP="$(ls "$GVC_NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strip | head -1)"
    export ANDROID_HOME="$GVC_SDK" ANDROID_NDK_HOME="$GVC_NDK"
}

# One game's java/ sources (an Activity subclass) -> classes.dex, staged.
gvc_android_build_classes() { # <java_dir> <stage_dir> [<classes_out_dir>]
    local java_dir="$1" stage="$2" classes="${3:-$stage/../classes}"
    rm -rf "$classes" && mkdir -p "$classes"
    javac -source 8 -target 8 -nowarn -Xlint:-options -classpath "$GVC_ANDROID_JAR" -d "$classes" \
        "$java_dir"/*.java
    "$GVC_BT/d8" --min-api 28 --lib "$GVC_ANDROID_JAR" --output "$stage" $(find "$classes" -name '*.class')
}

# The debug keystore (created on first use), honouring the KEYSTORE / KEY_ALIAS / KEY_PASS env vars.
gvc_android_keystore() {
    KEYSTORE="${KEYSTORE:-$HOME/.android/debug.keystore}"
    KEY_ALIAS="${KEY_ALIAS:-androiddebugkey}"
    KEY_PASS="${KEY_PASS:-android}"
    if [ ! -f "$KEYSTORE" ]; then
        mkdir -p "$(dirname "$KEYSTORE")"
        keytool -genkeypair -keystore "$KEYSTORE" -storepass "$KEY_PASS" -keypass "$KEY_PASS" -alias "$KEY_ALIAS" \
            -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null
    fi
}

# zipalign -p 4 + apksigner (the last packaging step; native libs must already be stored -0 in the unsigned zip).
gvc_android_align_sign() { # <unsigned.apk> <out.apk>
    local unsigned="$1" out="$2" aligned="${unsigned%.apk}_aligned.apk"
    "$GVC_BT/zipalign" -f -p 4 "$unsigned" "$aligned"
    gvc_android_keystore
    "$GVC_BT/apksigner" sign --ks "$KEYSTORE" --ks-pass "pass:$KEY_PASS" --key-pass "pass:$KEY_PASS" \
        --ks-key-alias "$KEY_ALIAS" --out "$out" "$aligned"
    rm -f "$unsigned" "$aligned"
}
