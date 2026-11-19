#!/bin/sh
# Run from the source root. All files and command substitutes stay in a fixture.
set -eu
WATCHDOG=$(pwd)/deploy/watchdog.sh
FIXTURES=${1:-./build/watchdog-test}
mkdir -p "$FIXTURES"
TEST_ROOT=$(mktemp -d "$FIXTURES/case.XXXXXXXX")
TEST_ROOT=$(cd "$TEST_ROOT" && pwd)
REAL_CP=$(command -v cp)
export REAL_CP
mkdir "$TEST_ROOT/bin"
cat > "$TEST_ROOT/bin/date" <<'STUB'
#!/bin/sh
cat "$WD_CASE/time"
STUB
cat > "$TEST_ROOT/bin/sleep" <<'STUB'
#!/bin/sh
# Stop after one complete supervisor iteration, without any real waiting.
kill -TERM "$PPID"
STUB
cat > "$TEST_ROOT/bin/sync" <<'STUB'
#!/bin/sh
exit 0
STUB
cat > "$TEST_ROOT/bin/reboot" <<'STUB'
#!/bin/sh
echo reboot >> "$WD_CASE/trace"
STUB
cat > "$TEST_ROOT/bin/cp" <<'STUB'
#!/bin/sh
if [ -f "$WD_CASE/fail-copy" ]; then exit 1; fi
exec "$REAL_CP" "$@"
STUB
chmod 755 "$TEST_ROOT/bin/"*
PATH="$TEST_ROOT/bin:$PATH"
export PATH
case_number=0
new_case() {
    case_number=$((case_number + 1))
    WD_CASE="$TEST_ROOT/$case_number"
    CAR_APP_HOME="$WD_CASE/app home"
    CAR_USB_ROOT="$WD_CASE/usb"
    export WD_CASE CAR_APP_HOME CAR_USB_ROOT
    mkdir -p "$CAR_APP_HOME" "$CAR_USB_ROOT/volume one"
    echo 100 > "$WD_CASE/time"
    echo 0 > "$CAR_APP_HOME/boot_flag"
    : > "$WD_CASE/trace"
}
app() {
    destination=$1
    label=$2
    seconds=$3
    cat > "$destination" <<APP
#!/bin/sh
printf '%s:%s\\n' '$label' "\$PWD" >> "\$WD_CASE/trace"
echo \$(( \$(cat "\$WD_CASE/time") + $seconds )) > "\$WD_CASE/time"
exit 1
APP
    chmod 755 "$destination"
}
run() {
    result=0
    sh "$WATCHDOG" || result=$?
    [ "$result" = 143 ] || { echo "Unexpected supervisor exit: $result"; exit 1; }
}
assert_trace() {
    actual=$(cut -d: -f1 "$WD_CASE/trace" | tr '\n' ' ')
    [ "$actual" = "$1" ] || { echo "Case $case_number: '$actual' != '$1'"; exit 1; }
}

# A runs normally; a later runtime exit must not cause USB recovery.
new_case
app "$CAR_APP_HOME/car_A" A 10
app "$CAR_APP_HOME/car_B" B 10
app "$CAR_USB_ROOT/volume one/car_dashboard_hml" USB 10
run
assert_trace 'A '

# A crashes immediately; use B and remember it as the active application.
new_case
app "$CAR_APP_HOME/car_A" A 0
app "$CAR_APP_HOME/car_B" B 10
run
assert_trace 'A B '
[ "$(cat "$CAR_APP_HOME/boot_flag")" = 1 ]

# Selected B fails; A is the fallback.
new_case
echo 1 > "$CAR_APP_HOME/boot_flag"
app "$CAR_APP_HOME/car_A" A 10
app "$CAR_APP_HOME/car_B" B 0
run
assert_trace 'B A '
[ "$(cat "$CAR_APP_HOME/boot_flag")" = 0 ]

# Both crash: restore the binary and resources from USB, with MD5 validation.
new_case
app "$CAR_APP_HOME/car_A" A 0
app "$CAR_APP_HOME/car_B" B 0
app "$CAR_USB_ROOT/volume one/car_dashboard_hml" USB 10
md5sum "$CAR_USB_ROOT/volume one/car_dashboard_hml" > "$CAR_USB_ROOT/volume one/car_dashboard_hml.md5"
mkdir "$CAR_USB_ROOT/volume one/assets"
echo resource > "$CAR_USB_ROOT/volume one/assets/image.txt"
# A stale interrupted copy must not permanently prevent recovery.
echo incomplete > "$CAR_APP_HOME/car_A.recovery-new"
run
assert_trace 'A B USB '
[ "$(cat "$CAR_APP_HOME/boot_flag")" = 0 ]
[ "$(cat "$CAR_APP_HOME/assets/image.txt")" = resource ]
cmp "$CAR_APP_HOME/car_A" "$CAR_USB_ROOT/volume one/car_dashboard_hml"
! [ -e "$CAR_APP_HOME/car_A.recovery-new" ]

# Missing A/B: binary-only emergency recovery needs no sidecar.
new_case
app "$CAR_USB_ROOT/volume one/car_dashboard_hml" USB 10
chmod 644 "$CAR_USB_ROOT/volume one/car_dashboard_hml"
echo stale > "$CAR_APP_HOME/car_dashboard_hml.md5"
run
assert_trace 'USB '
! [ -e "$CAR_APP_HOME/car_dashboard_hml.md5" ]
grep -E '^USB:.*app home$' "$WD_CASE/trace"

# A corrupt USB candidate must not overwrite A; a second valid volume is tried.
new_case
app "$CAR_APP_HOME/car_A" A 0
app "$CAR_APP_HOME/car_B" B 0
app "$CAR_USB_ROOT/volume one/car_dashboard_hml" BAD 10
echo 00000000000000000000000000000000 > "$CAR_USB_ROOT/volume one/car_dashboard_hml.md5"
mkdir "$CAR_USB_ROOT/volume two"
app "$CAR_USB_ROOT/volume two/car_dashboard_hml" USB 10
run
assert_trace 'A B USB '

# A bad checksum with no alternate candidate leaves the old A intact.
new_case
app "$CAR_APP_HOME/car_A" A 0
app "$CAR_APP_HOME/car_B" B 0
app "$CAR_USB_ROOT/volume one/car_dashboard_hml" BAD 10
echo 00000000000000000000000000000000 > "$CAR_USB_ROOT/volume one/car_dashboard_hml.md5"
"$REAL_CP" "$CAR_APP_HOME/car_A" "$WD_CASE/original"
run
assert_trace 'A B '
cmp "$CAR_APP_HOME/car_A" "$WD_CASE/original"

# Failed copy leaves the old A and its bytes intact.
new_case
app "$CAR_APP_HOME/car_A" A 0
app "$CAR_APP_HOME/car_B" B 0
app "$CAR_USB_ROOT/volume one/car_dashboard_hml" USB 10
"$REAL_CP" "$CAR_APP_HOME/car_A" "$WD_CASE/original"
touch "$WD_CASE/fail-copy"
run
assert_trace 'A B '
cmp "$CAR_APP_HOME/car_A" "$WD_CASE/original"
! [ -e "$CAR_APP_HOME/car_A.recovery-new" ]

# No USB payload: keep waiting rather than manufacturing an application.
new_case
run
assert_trace ''
! [ -e "$CAR_APP_HOME/car_A" ]

# Preserve the upgrade reboot request; do not treat exit 75 as a broken image.
new_case
printf '#!/bin/sh\nexit 75\n' > "$CAR_APP_HOME/car_A"
chmod 755 "$CAR_APP_HOME/car_A"
app "$CAR_APP_HOME/car_B" B 10
run
assert_trace 'reboot '

# Native symlink semantics are exercised on Linux; Git Bash may copy ln -s.
new_case
mkdir -p "$CAR_APP_HOME/releases/old" "$CAR_APP_HOME/releases/broken"
app "$CAR_APP_HOME/releases/old/program" A 0
app "$CAR_APP_HOME/releases/broken/program" B 0
ln -s "$CAR_APP_HOME/releases/old/program" "$CAR_APP_HOME/car_A"
ln -s "$CAR_APP_HOME/releases/broken/program" "$CAR_APP_HOME/car_B"
if [ -L "$CAR_APP_HOME/car_A" ]; then
    app "$CAR_USB_ROOT/volume one/car_dashboard_hml" USB 10
    "$REAL_CP" "$CAR_APP_HOME/releases/old/program" "$WD_CASE/original"
    run
    assert_trace 'A B USB '
    ! [ -L "$CAR_APP_HOME/car_A" ]
    cmp "$CAR_APP_HOME/releases/old/program" "$WD_CASE/original"
    grep -E '^A:.*releases/old$' "$WD_CASE/trace"
else
    case_number=$((case_number - 1))
    echo 'Native symlink recovery scenario skipped (host has no symlink support)'
fi

echo "Watchdog: $case_number fallback and USB recovery scenarios passed"
