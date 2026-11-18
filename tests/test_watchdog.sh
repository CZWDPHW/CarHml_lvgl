#!/bin/sh
# Run from the source root; optional argument selects a build-directory fixture root.
set -eu
WATCHDOG=$(pwd)/deploy/watchdog.sh
FIXTURES=${1:-./build/watchdog-test}
mkdir -p "$FIXTURES"
TEST_ROOT=$(mktemp -d "$FIXTURES/case.XXXXXXXX")
TEST_ROOT=$(cd "$TEST_ROOT" && pwd)
SUPERVISOR=
cleanup() {
    [ -z "$SUPERVISOR" ] || kill -TERM "$SUPERVISOR" 2>/dev/null || true
    [ -z "$SUPERVISOR" ] || wait "$SUPERVISOR" 2>/dev/null || true
}
trap cleanup EXIT INT TERM
mkdir -p "$TEST_ROOT/app home/releases/old" "$TEST_ROOT/app home/releases/new"
export CAR_APP_HOME="$TEST_ROOT/app home"
export CAR_WATCHDOG_LOCK="$TEST_ROOT/lock"
cat > "$CAR_APP_HOME/releases/new/program" <<'APP_A'
#!/bin/sh
printf 'A:%s\n' "$PWD" >> "$CAR_APP_HOME/trace"
echo 1 > "$CAR_APP_HOME/boot_flag"
APP_A
cat > "$CAR_APP_HOME/releases/old/program" <<'APP_B'
#!/bin/sh
printf 'B:%s\n' "$PWD" >> "$CAR_APP_HOME/trace"
sleep 1
APP_B
chmod 755 "$CAR_APP_HOME/releases/new/program" "$CAR_APP_HOME/releases/old/program"
ln -s "$CAR_APP_HOME/releases/new/program" "$CAR_APP_HOME/car_A"
ln -s "$CAR_APP_HOME/releases/old/program" "$CAR_APP_HOME/car_B"
echo 0 > "$CAR_APP_HOME/boot_flag"
sh "$WATCHDOG" &
SUPERVISOR=$!
attempt=0
while [ "$attempt" -lt 10 ]; do
    if [ -r "$CAR_APP_HOME/trace" ] && grep -q '^B:' "$CAR_APP_HOME/trace"; then break; fi
    sleep 1
    attempt=$((attempt + 1))
done
grep -Fx "A:$CAR_APP_HOME/releases/new" "$CAR_APP_HOME/trace"
grep -Fx "B:$CAR_APP_HOME/releases/old" "$CAR_APP_HOME/trace"
# A second supervisor must exit without starting another application.
before=$(wc -l < "$CAR_APP_HOME/trace")
sh "$WATCHDOG"
after=$(wc -l < "$CAR_APP_HOME/trace")
[ "$before" = "$after" ]
kill -TERM "$SUPERVISOR"
wait "$SUPERVISOR"
SUPERVISOR=
[ ! -d "$CAR_WATCHDOG_LOCK" ]
echo "Watchdog flag reload, version cwd, single-instance and shutdown tests passed"
