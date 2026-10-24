#!/bin/bash
# Stop the sim and GDS that sim/run-sil.sh started in <run-dir>.
RUN="$1"
# A pid file outlives a sim that stopped at -stop_at, so check that the pid is still ours.
ours() { [ -n "$1" ] && grep -qa "$2" "/proc/$1/cmdline" 2> /dev/null; }
GDS=$(cat "$RUN/gds.pid" 2> /dev/null)
SIL=$(cat "$RUN/sil.pid" 2> /dev/null)
ours "$GDS" fprime-gds && kill -- "-$GDS"
ours "$SIL" pcr-native_sim && kill -9 "$SIL"
rm -f "$RUN/gds.pid" "$RUN/sil.pid"
# The next run binds the same GDS ports
for _ in $(seq 50); do
    ours "$GDS" fprime-gds || ours "$SIL" pcr-native_sim || break
    sleep 0.1
done
exit 0
