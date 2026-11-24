#!/usr/bin/env bash
# usage: ~/zflash.sh <app path> [extra west build args]
set -e
source ~/zephyrproject/.venv/bin/activate
source ~/zephyrproject/zephyr/zephyr-env.sh
cd ~/zephyrproject

APP=${1:-zephyr/samples/basic/blinky}
shift || true

pkill -f JLinkGDBServer 2>/dev/null || true
sleep 1

west build -b mr_canhubk3 "$APP" "$@"
west flash --runner jlink

# bring RTT / remote debug back up
nohup JLinkGDBServer -device S32K344 -if SWD -speed 4000 -port 2331 \
  -RTTTelnetPort 19021 -nogui >/tmp/gdbserver.log 2>&1 &
echo "Flashed. GDB server/RTT restarted (RTT on port 19021)."
