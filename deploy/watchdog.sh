#!/bin/sh
APP_HOME=${CAR_APP_HOME:-/opt/car}
USB_ROOT=${CAR_USB_ROOT:-/mnt/usb}
START_SECONDS=${CAR_START_SECONDS:-5}

# Optional board library paths; LVGL does not need qt_env.sh.
[ ! -r "$APP_HOME/runtime_env.sh" ] || . "$APP_HOME/runtime_env.sh"

run_app() {
    # Resolve the selected version so ./assets belongs to that same version.
    BOOT_APP=$(readlink -f "$1")
    [ -x "$BOOT_APP" ] || return 1
    started=$(date +%s)
    (cd "${BOOT_APP%/*}" && exec "$BOOT_APP") >/dev/null 2>&1
    result=$?
    if [ "$result" = 75 ]; then
        sync
        reboot
        return 0
    fi
    # A missing loader, bad executable or immediate crash counts as start failure.
    [ "$(( $(date +%s) - started ))" -ge "$START_SECONDS" ]
}

recover_usb() {
    for volume in "$USB_ROOT"/*; do
        source="$volume/car_dashboard_hml"
        [ -f "$source" ] || continue
        # Binary-only recovery is supported; verify MD5 when supplied.
        hash=
        if [ -f "$source.md5" ]; then
            hash=$(awk 'NR == 1 { print tolower($1); exit }' "$source.md5")
            actual=$(md5sum "$source" | awk '{ print $1 }')
            [ -n "$hash" ] && [ "$hash" = "$actual" ] || continue
        fi
        # Never write through car_A: it can be a symlink to a version in releases/.
        rm -f "$APP_HOME/car_A.recovery-new" || continue
        if ! cp "$source" "$APP_HOME/car_A.recovery-new"; then
            rm -f "$APP_HOME/car_A.recovery-new"
            continue
        fi
        if ! chmod 755 "$APP_HOME/car_A.recovery-new"; then
            rm -f "$APP_HOME/car_A.recovery-new"
            continue
        fi
        if [ -n "$hash" ]; then
            actual=$(md5sum "$APP_HOME/car_A.recovery-new" | awk '{ print $1 }')
            if [ "$hash" != "$actual" ]; then
                rm -f "$APP_HOME/car_A.recovery-new"
                continue
            fi
        fi
        if [ -d "$volume/assets" ]; then
            if ! mkdir -p "$APP_HOME/assets" || ! cp -R "$volume/assets/." "$APP_HOME/assets/"; then
                rm -f "$APP_HOME/car_A.recovery-new"
                continue
            fi
        fi
        sync
        mv -f "$APP_HOME/car_A.recovery-new" "$APP_HOME/car_A" || return 1
        echo 0 > "$APP_HOME/boot_flag" || return 1
        if [ -n "$hash" ]; then
            echo "$hash" > "$APP_HOME/car_dashboard_hml.md5" || return 1
        else
            rm -f "$APP_HOME/car_dashboard_hml.md5"
        fi
        sync
        return 0
    done
    return 1
}

while true; do
    flag=0
    if [ -r "$APP_HOME/boot_flag" ]; then
        flag=$(tr -d ' \r\n' < "$APP_HOME/boot_flag")
    fi
    if [ "$flag" = 1 ]; then first=car_B; second=car_A; other_flag=0
    else first=car_A; second=car_B; other_flag=1
    fi

    if ! run_app "$APP_HOME/$first"; then
        # Record the active fallback so subsequent upgrades back up the right app.
        if echo "$other_flag" > "$APP_HOME/boot_flag"; then
            if ! run_app "$APP_HOME/$second"; then
                if recover_usb; then run_app "$APP_HOME/car_A"; fi
            fi
        fi
    fi
    sleep 1
done
