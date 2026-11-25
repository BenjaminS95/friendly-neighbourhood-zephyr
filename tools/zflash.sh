#!/usr/bin/env bash
# usage: tools/zflash.sh [app path] [extra west build args]
# Default app: the Zephyr blinky sample. Workspace: $ZEPHYR_WS or ~/zephyrproject.
set -eo pipefail

WS=${ZEPHYR_WS:-$HOME/zephyrproject}

if [ $# -ge 1 ]; then
  APP=$(realpath "$1"); shift
else
  APP="$WS/zephyr/samples/basic/blinky"
fi

source "$WS/.venv/bin/activate"
source "$WS/zephyr/zephyr-env.sh"
cd "$WS"

pkill -f JLinkGDBServer 2>/dev/null || true
sleep 1

west build -b mr_canhubk3 "$APP" "$@"
west flash --runner jlink

# bring RTT / remote debug back up
nohup JLinkGDBServer -device S32K344 -if SWD -speed 4000 -port 2331 \
  -RTTTelnetPort 19021 -nogui </dev/null >/tmp/gdbserver.log 2>&1 &
echo "Flashed. GDB server restarted (RTT on port 19021). Run ~/attach.sh to attach and enable RTT."
