#!/bin/bash
# Stop the sim and GDS that sim/run-sil.sh started in <run-dir>.
# Exits non-zero if either is still running afterwards.
RUN="$1"
# A pid file outlives a sim that stopped at -stop_at, so check that the pid is still ours.
ours() { [ -n "$1" ] && grep -qa "$2" "/proc/$1/cmdline" 2> /dev/null; }
GDS=$(cat "$RUN/gds.pid" 2> /dev/null)
SIL=$(cat "$RUN/sil.pid" 2> /dev/null)
ours "$GDS" fprime-gds || GDS=
ours "$SIL" pcr-native_sim || SIL=
# The GDS leads its own process group (setsid), and its children hold the ports.
# A zombie has an empty cmdline, so it does not count as alive.
group_alive() { for p in $(pgrep -g "$1"); do [ -s "/proc/$p/cmdline" ] && return 0; done; return 1; }
alive() { { [ -n "$GDS" ] && group_alive "$GDS"; } || ours "$SIL" pcr-native_sim; }
[ -n "$GDS" ] && kill -- "-$GDS" 2> /dev/null
[ -n "$SIL" ] && kill -9 "$SIL" 2> /dev/null
# The next run binds the same GDS ports.
for _ in $(seq 50); do
    alive || break
    sleep 0.1
done
if [ -n "$GDS" ] && group_alive "$GDS"; then
    kill -9 -- "-$GDS" 2> /dev/null
    sleep 0.5
fi
rm -f "$RUN/gds.pid" "$RUN/sil.pid"
if alive; then
    echo "sim or GDS from $RUN still running" >&2
    exit 1
fi
