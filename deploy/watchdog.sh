#!/bin/sh
# rcS starts this supervisor once. Each launch resolves boot_flag again.
APP_HOME=${CAR_APP_HOME:-/opt/car}
LOCK_DIR=${CAR_WATCHDOG_LOCK:-/var/run/car-dashboard-watchdog.lock}
STOP=0
CHILD=

if ! mkdir "$LOCK_DIR" 2>/dev/null; then
    # A persistent /var/run may contain a lock from a previous boot.
    old_pid=$(cat "$LOCK_DIR/pid" 2>/dev/null)
    case "$old_pid" in ''|*[!0-9]*) exit 1 ;; esac
    kill -0 "$old_pid" 2>/dev/null && exit 0
    rm -f "$LOCK_DIR/pid" || exit 1
    rmdir "$LOCK_DIR" || exit 1
    mkdir "$LOCK_DIR" || exit 1
fi
echo "$$" > "$LOCK_DIR/pid" || exit 1

cleanup() {
    rm -f "$LOCK_DIR/pid"
    rmdir "$LOCK_DIR" 2>/dev/null
}
stop() {
    STOP=1
    [ -z "$CHILD" ] || kill -TERM "$CHILD" 2>/dev/null
}
trap stop INT TERM
trap cleanup EXIT

# LVGL has no Qt dependency. Put any board-specific library paths in this file.
if [ -r "$APP_HOME/runtime_env.sh" ]; then
    . "$APP_HOME/runtime_env.sh"
fi

while [ "$STOP" = 0 ]; do
    flag=$(tr -d ' \r\n' < "$APP_HOME/boot_flag" 2>/dev/null)
    selected="$APP_HOME/car_A"
    [ "$flag" != 1 ] || selected="$APP_HOME/car_B"
    # Versioned executable and images must use the same working directory.
    executable=$(readlink -f "$selected" 2>/dev/null)
    if [ ! -x "$executable" ]; then
        # Missing A can fall back to a valid B; never rewrite boot_flag here.
        executable=$(readlink -f "$APP_HOME/car_B" 2>/dev/null)
    fi
    if [ ! -x "$executable" ]; then
        sleep 1
        continue
    fi
    directory=${executable%/*}
    (cd "$directory" && exec "$executable") >/dev/null 2>&1 &
    CHILD=$!
    wait "$CHILD"
    result=$?
    # wait can be interrupted by TERM; reap the application after forwarding it.
    if [ "$STOP" != 0 ]; then
        wait "$CHILD" 2>/dev/null
        CHILD=
        break
    fi
    CHILD=
    if [ "$result" = 75 ]; then
        sync
        reboot
        # If reboot is unavailable, retry supervision instead of spinning.
    fi
    sleep 1
done
