#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
runtime_dir="$(mktemp -d)"
xvfb_pid=""
test_window_pid=""
toggle_pid=""

cleanup() {
  set +e
  [[ -n "$toggle_pid" ]] && kill "$toggle_pid" 2>/dev/null
  [[ -n "$test_window_pid" ]] && kill "$test_window_pid" 2>/dev/null
  [[ -n "$xvfb_pid" ]] && kill "$xvfb_pid" 2>/dev/null
  rm -rf "$runtime_dir"
}
trap cleanup EXIT

export DISPLAY=:98
export XDG_RUNTIME_DIR="$runtime_dir"

Xvfb "$DISPLAY" -screen 0 1024x768x24 -nolisten tcp >"$runtime_dir/xvfb.log" 2>&1 &
xvfb_pid=$!

for _ in {1..50}; do
  if xdpyinfo >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done
xdpyinfo >/dev/null

"$project_root/build/test-window" >"$runtime_dir/window-id" &
test_window_pid=$!

for _ in {1..50}; do
  [[ -s "$runtime_dir/window-id" ]] && break
  sleep 0.1
done
window_id="$(cat "$runtime_dir/window-id")"
[[ "$window_id" == 0x* ]]

if "$project_root/build/window-clickthrough" --status >"$runtime_dir/status-inactive"; then
  echo "status unexpectedly reported active" >&2
  exit 1
fi
grep -qx 'inactive' "$runtime_dir/status-inactive"

"$project_root/build/window-clickthrough" >"$runtime_dir/enable.log" 2>&1 &
toggle_pid=$!
sleep 0.2
xdotool mousemove --sync 100 100 click 1
wait "$toggle_pid"
toggle_pid=""

grep -q 'Mouse click-through enabled' "$runtime_dir/enable.log"
"$project_root/build/window-clickthrough" --status | grep -Eq '^active 0x[0-9a-f]+$'
[[ "$("$project_root/build/query-shape" "$window_id")" == "0" ]]

"$project_root/build/window-clickthrough" >"$runtime_dir/restore.log"
grep -q 'Mouse click-through disabled' "$runtime_dir/restore.log"
if "$project_root/build/window-clickthrough" --status >"$runtime_dir/status-restored"; then
  echo "status unexpectedly reported active after restore" >&2
  exit 1
fi
grep -qx 'inactive' "$runtime_dir/status-restored"
[[ "$("$project_root/build/query-shape" "$window_id")" != "0" ]]

"$project_root/build/window-clickthrough" >"$runtime_dir/cancel.log" 2>&1 &
toggle_pid=$!
sleep 0.2
xdotool click 3
wait "$toggle_pid"
toggle_pid=""
if "$project_root/build/window-clickthrough" --status >/dev/null; then
  echo "right-click cancellation left an active window" >&2
  exit 1
fi

"$project_root/build/window-clickthrough" --version | grep -qx 'window-clickthrough 1.0.0'
"$project_root/build/window-clickthrough" --help | grep -q '^Usage:'

echo "integration tests passed"
