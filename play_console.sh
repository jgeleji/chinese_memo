#!/bin/bash
# Run the console (ncurses/Termux) version of the game.
#
# The program opens "status1.txt", "union_q.txt" and "data/..." with relative
# paths, so it must be started from this directory: that is the only copy of the
# progress file that is kept in git and synced with the phone. Running it from
# elsewhere silently starts a second, separate progress file.
#
#   ./play_console.sh          -> status1.txt
#   ./play_console.sh 2        -> status2.txt (a different, second list)
#
# After playing on the PC:  ./progress_push.sh      (phone takes over the new lines)
# After playing on the phone: ./progress_pull.sh && git commit -am status
set -e
cd "$(dirname "$0")"
if [ ! -x ./questions10 ]; then
	echo "no ./questions10 here - build it first (build-pc/ or: make questions10)" >&2
	exit 1
fi
exec ./questions10 "$@"
