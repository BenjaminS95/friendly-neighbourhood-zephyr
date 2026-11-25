#!/usr/bin/env bash
# Attach GDB to the J-Link GDB server, point it at the RTT control block, reset and run.
WS=${ZEPHYR_WS:-$HOME/zephyrproject}
ELF="$WS/build/zephyr/zephyr.elf"
ADDR=0x$(nm "$ELF" | awk '$3=="_SEGGER_RTT"{print $1}')
echo "RTT control block at $ADDR"
exec "$HOME/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb" "$ELF" \
  -ex "target remote localhost:2331" \
  -ex "monitor exec SetRTTAddr $ADDR" \
  -ex "monitor reset" \
  -ex "monitor go"
