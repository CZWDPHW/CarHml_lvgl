#!/bin/sh
APP_HOME=${CAR_APP_HOME:-/opt/car}

# Optional board library paths; LVGL does not need qt_env.sh.
[ ! -r "$APP_HOME/runtime_env.sh" ] || . "$APP_HOME/runtime_env.sh"

while true; do
    BOOT_APP="$APP_HOME/car_A"
    if [ -r "$APP_HOME/boot_flag" ]; then
        flag=$(tr -d ' \r\n' < "$APP_HOME/boot_flag")
        [ "$flag" != 1 ] || BOOT_APP="$APP_HOME/car_B"
    fi

    # Resolve the selected version so ./assets belongs to that same version.
    BOOT_APP=$(readlink -f "$BOOT_APP")
    if [ -x "$BOOT_APP" ]; then
        (cd "${BOOT_APP%/*}" && exec "$BOOT_APP") >/dev/null 2>&1
        if [ "$?" = 75 ]; then
            sync
            reboot
        fi
    fi
    sleep 1
done
