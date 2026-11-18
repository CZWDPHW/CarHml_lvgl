#!/bin/sh
# Add this block once near the end of the board's existing /etc/init.d/rcS.
# The existing USB automounter must already mount removable drives under /mnt/usb.
if [ -x /opt/car/watchdog.sh ]; then
    /opt/car/watchdog.sh >/dev/null 2>&1 &
fi
