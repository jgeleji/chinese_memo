#!/bin/bash
# Pull the study progress written by the Android app into the repo copy.
# The repo copy of status1.txt is the master: build a new APK from it and the
# phone keeps continuing where you left off.
#
# usage: ./progress_pull.sh [target-file]
set -e
ADB=${ADB:-$HOME/android-sdk/platform-tools/adb}
PKG=com.geleji.chinesememo
OUT=${1:-status1.txt}
TMP=$(mktemp)
$ADB exec-out "run-as $PKG cat files/status1.txt" > "$TMP"
if [ ! -s "$TMP" ]; then
	echo "nothing pulled (app data empty, or app not installed / not debuggable)" >&2
	rm -f "$TMP"
	exit 1
fi
if [ -f "$OUT" ]; then
	cp "$OUT" "$OUT.pre-pull"
	echo "previous repo copy kept as $OUT.pre-pull"
fi
mv "$TMP" "$OUT"
echo "pulled $(wc -l < "$OUT") lines, $(wc -c < "$OUT") bytes -> $OUT"
