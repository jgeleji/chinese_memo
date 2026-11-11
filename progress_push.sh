#!/bin/bash
# Push the repo copy of the study progress onto the phone, i.e. move your
# Termux / previous-phone progress into the Android app.
# Refuses to shrink the phone's file unless --force is given.
#
# usage: ./progress_push.sh [--force] [source-file]
set -e
ADB=${ADB:-$HOME/android-sdk/platform-tools/adb}
PKG=com.geleji.chinesememo
FORCE=0
SRC=status1.txt
for a in "$@"; do
	if [ "$a" = "--force" ]; then FORCE=1; else SRC="$a"; fi
done
[ -f "$SRC" ] || { echo "no such file: $SRC" >&2; exit 1; }

phone=$(mktemp)
$ADB exec-out "run-as $PKG cat files/status1.txt" > "$phone" || true
if [ -s "$phone" ]; then
	echo "phone copy: $(wc -l < "$phone") lines, $(wc -c < "$phone") bytes"
	if [ "$FORCE" != 1 ] && [ "$(wc -c < "$phone")" -gt "$(wc -c < "$SRC")" ]; then
		echo "refusing: the phone has MORE progress than $SRC (use --force to overwrite anyway)" >&2
		rm -f "$phone"
		exit 1
	fi
	cp "$phone" "status1.phone-backup"
	echo "phone copy kept as status1.phone-backup"
fi
rm -f "$phone"

$ADB push "$SRC" /data/local/tmp/status1.txt >/dev/null
$ADB shell "run-as $PKG cp /data/local/tmp/status1.txt files/status1.txt"
$ADB shell "run-as $PKG ls -l files/status1.txt"
$ADB shell rm /data/local/tmp/status1.txt
echo "pushed $SRC ($(wc -l < "$SRC") lines) -> $PKG; restart the app to pick it up"
