#!/usr/bin/env bash
# usage: tools/zflash.sh [app path] [extra west build args]
set -eo pipefail

WS=${ZEPHYR_WS:-$HOME/zephyrproject}
BUILD_DIR=${BUILD_DIR:-$WS/build}

if [ $# -ge 1 ]; then APP=$(realpath "$1"); shift
else APP="$WS/zephyr/samples/basic/blinky"; fi

source "$WS/.venv/bin/activate"
source "$WS/zephyr/zephyr-env.sh"
cd "$WS"

start_gdbserver() {   # always restore the debug server, even if build/flash fails
  nohup JLinkGDBServer -device S32K344 -if SWD -speed 4000 -port 2331 \
    -RTTTelnetPort 19021 -nogui </dev/null >/tmp/gdbserver.log 2>&1 &
}

restore() {
  if [ "${RESTART_GDBSERVER:-1}" = "1" ]; then start_gdbserver; fi
}
trap restore EXIT

pkill -f JLinkGDBServer 2>/dev/null || true
sleep 1

west build -p auto -b mr_canhubk3 -d "$BUILD_DIR" "$APP" "$@"
west flash -d "$BUILD_DIR" --runner jlink
echo "Flashed. GDB server restarting; run tools/attach.sh to enable RTT."
