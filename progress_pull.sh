#!/bin/bash
# Pull the study progress written by the Android apps into the repo copy.
# The repo copy is the master: build a new APK from it (quiz app ships
# status1.txt in its assets) and the phone keeps continuing where you left
# off. The writing app keeps wstatus.txt on the phone only; push it back with
# adb if you reinstall and want to keep it.
#
# usage: ./progress_pull.sh [app] [target-file]
#   app: quiz (default) | write | all
set -e
ADB=${ADB:-$HOME/android-sdk/platform-tools/adb}
case "${1:-quiz}" in
	quiz)  PKG=com.geleji.chinesememo;  REMOTE=status1.txt; OUT=${2:-status1.txt} ;;
	write) PKG=com.geleji.chinesewrite; REMOTE=wstatus.txt; OUT=${2:-wstatus.txt} ;;
	all)   "$0" quiz; "$0" write; exit $? ;;
	*)     echo "unknown app: $1 (quiz | write | all)" >&2; exit 2 ;;
esac
TMP=$(mktemp)
$ADB exec-out "run-as $PKG cat files/$REMOTE" > "$TMP"
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
