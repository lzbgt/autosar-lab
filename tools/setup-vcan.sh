#!/usr/bin/env bash
set -euo pipefail
dev="${1:-vcan0}"
if [[ ! "$dev" =~ ^vcan[0-9]+$ ]]; then
  echo "Use device name like vcan0" >&2
  exit 2
fi
if ip link show "$dev" >/dev/null 2>&1; then
  echo "$dev already exists"
  exit 0
fi
sudo modprobe vcan || true
sudo ip link add dev "$dev" type vcan || {
  echo "No usable vcan kernel module. Use python3 tools/smoke.py --backend udp" >&2
  exit 1
}
sudo ip link set "$dev" up
ip -details link show "$dev"
