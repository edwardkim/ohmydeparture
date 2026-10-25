#!/usr/bin/env bash
# Run with: xvfb-run -a -s '-screen 0 1920x1080x24' bash tests/omarchy-widget.sh
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$project_dir"
test_dir=$(mktemp -d /tmp/rdepartures-widget-XXXXXXXX)
shell_pid=
renderer_pid=
cleanup() {
  for task_pid in "$renderer_pid" "$shell_pid"; do
    if [[ -n $task_pid ]] && kill -0 "$task_pid" 2>/dev/null; then
      kill "$task_pid" 2>/dev/null || true
    fi
  done
  [[ -z $shell_pid ]] || wait "$shell_pid" 2>/dev/null || true
  rm -rf -- "$test_dir"
}
trap cleanup EXIT
omarchy_dir=${OMARCHY_PATH:-/usr/share/omarchy}
ln -s "$omarchy_dir/shell/Ui" "$test_dir/Ui"
ln -s "$omarchy_dir/shell/Commons" "$test_dir/Commons"
cp tests/omarchy-preview.qml "$test_dir/shell.qml"
mkdir -m700 "$test_dir/runtime"
export XDG_RUNTIME_DIR="$test_dir/runtime"
export XDG_CONFIG_HOME="$test_dir/config"
export XDG_DATA_HOME="$test_dir/data"
export XDG_STATE_HOME="$test_dir/state"
export XDG_CACHE_HOME="$test_dir/cache"
export QT_QPA_PLATFORM=xcb
export QT_QPA_PLATFORMTHEME=
export QT_QUICK_BACKEND=software
export QT_IM_MODULE=
unset HYPRLAND_INSTANCE_SIGNATURE WAYLAND_DISPLAY
plugin_version=$(/usr/bin/python3 -c 'import json; print(json.load(open("packaging/omarchy/plugin/manifest.json"))["version"])')
export RDEPARTURES_PLUGIN_TEST_PATH="$project_dir/dist/ohmydeparture-omarchy-$plugin_version-$(uname -m)/edwardkim.ohmydeparture"
# The native plugin must use the bundled helper, without reading personal logs.
export CODEX_HOME="$test_dir/codex"
mkdir -p "$CODEX_HOME/sessions/2026"
printf '%s\n' '{"type":"session_meta","payload":{"id":"widget-demo","cwd":"/demo/omarchy","source":"cli"}}' > "$CODEX_HOME/sessions/2026/rollout-demo.jsonl"
printf '%s\n' '{"type":"turn_context","payload":{"turn_id":"widget-turn"}}' \
  '{"type":"event_msg","payload":{"type":"item_completed","item":{"id":"widget-tool","type":"CommandExecution","command":"make test","status":"completed","exit_code":0,"aggregated_output":"Native plugin fixture"}}}' \
  '{"type":"event_msg","payload":{"type":"task_complete"}}' >> "$CODEX_HOME/sessions/2026/rollout-demo.jsonl"
mkdir -p test-output
quickshell --no-color -p "$test_dir/shell.qml" > test-output/omarchy-widget.log 2>&1 &
shell_pid=$!
find_window() {
  local title=$1
  local window_id
  for attempt in {1..100}; do
    window_id=$(xdotool search --onlyvisible --name "$title" 2>/dev/null | head -n 1 || true)
    if [[ -n $window_id ]]; then printf '%s\n' "$window_id"; return 0; fi
    if ! kill -0 "$shell_pid" 2>/dev/null; then cat test-output/omarchy-widget.log >&2; return 1; fi
    sleep 0.1
  done
  cat test-output/omarchy-widget.log >&2
  return 1
}
preview=$(find_window '^Oh My Departure plugin preview$')
sleep 0.5
magick import -window "$preview" test-output/omarchy-bar-plugin.png
xdotool windowfocus --sync "$preview"
xdotool mousemove --window "$preview" 36 24 click 1
board=$(find_window '^Oh My Departure — Codex$')
renderer_pid=$(xdotool getwindowpid "$board")
sleep 4.2
magick import -window "$board" test-output/omarchy-plugin-board.png
xdotool windowfocus --sync "$board"
xdotool key Escape
for attempt in {1..30}; do
  if ! xdotool getwindowname "$board" >/dev/null 2>&1; then break; fi
  sleep 0.1
done
if xdotool getwindowname "$board" >/dev/null 2>&1; then exit 1; fi
renderer_pid=
kill -0 "$shell_pid"
grep -q 'PASS: native Omarchy widget loaded' test-output/omarchy-widget.log
echo 'PASS: native Omarchy QML widget, settings, real bar-button click, bundled renderer launch, Escape, independent shell lifetime'
