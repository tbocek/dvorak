#!/bin/bash
# layout-watcher.sh — Companion to swaykbdd.
# Listens for Sway input events and signals dvorak daemons
# whenever the keyboard layout changes.
#
# Layout indexes (must match your sway input config):
#   0 = dvorak  → dvorak-signal.sh on
#   1/2/3       → dvorak-signal.sh off

# ── Kill the previous instance (if any) ─────────────────────────────
SCRIPT_NAME="$(basename "$0")"
PIDFILE="/tmp/${SCRIPT_NAME}.pid"
MY_PID=$$

get_start_ticks() {
    awk '{
        sub(/^.*\) /, "")
        print $20
    }' "/proc/$1/stat" 2>/dev/null
}

old_pid=""
if [[ -f "$PIDFILE" ]]; then
    old_pid=$(<"$PIDFILE")
fi

_tmpfile=$(mktemp "${PIDFILE}.XXXXXX")
echo "$MY_PID" > "$_tmpfile"
mv -f "$_tmpfile" "$PIDFILE"

if [[ -n "$old_pid" && "$old_pid" != "$MY_PID" ]] &&
   kill -0 "$old_pid" 2>/dev/null &&
   grep -Fqa "$SCRIPT_NAME" "/proc/$old_pid/cmdline" 2>/dev/null; then

    old_children=()
    old_child_ticks=()
    while IFS= read -r cpid; do
        [[ -z "$cpid" ]] && continue
        old_children+=("$cpid")
        old_child_ticks+=("$(get_start_ticks "$cpid")")
    done < <(pgrep -P "$old_pid" 2>/dev/null)

    kill "$old_pid" 2>/dev/null
    sleep 0.3
    kill -9 "$old_pid" 2>/dev/null

    for i in "${!old_children[@]}"; do
        cpid="${old_children[$i]}"
        expected="${old_child_ticks[$i]}"
        actual=$(get_start_ticks "$cpid")
        if [[ -n "$actual" && "$actual" == "$expected" ]]; then
            kill -9 "$cpid" 2>/dev/null
        fi
    done
fi

SWAYMSG_PID=""

cleanup() {
    if [[ -n "$SWAYMSG_PID" ]] && kill -0 "$SWAYMSG_PID" 2>/dev/null; then
        kill "$SWAYMSG_PID" 2>/dev/null
        kill -9 "$SWAYMSG_PID" 2>/dev/null
    fi
    pkill -P $$ 2>/dev/null
    sleep 0.1
    pkill -9 -P $$ 2>/dev/null
    [[ -f "$PIDFILE" && "$(<"$PIDFILE")" == "$$" ]] && rm -f "$PIDFILE"
    rm -f "$SUBSCRIBE_FIFO"
}
trap cleanup EXIT
# ─────────────────────────────────────────────────────────────────────

SIGNAL_SCRIPT="/usr/local/bin/dvorak-signal.sh"
LAST_INDEX=""
SUBSCRIBE_FIFO="/tmp/${SCRIPT_NAME}.$$.fifo"

get_layout_index() {
    local result
    result=$(swaymsg -t get_inputs 2>/dev/null | jq -r '
        [.[] | select(.type=="keyboard")] | .[0].xkb_active_layout_index // 0
    ' 2>/dev/null)
    if [[ "$result" =~ ^[0-9]+$ ]]; then
        echo "$result"
    else
        echo ""
    fi
}

signal_for_index() {
    local index="$1"
    [[ -z "$index" ]] && return
    [[ "$index" == "$LAST_INDEX" ]] && return
    LAST_INDEX="$index"

    if [[ "$index" -eq 0 ]]; then
        "$SIGNAL_SCRIPT" on  >>/tmp/dvorak-layout.log 2>&1
    else
        "$SIGNAL_SCRIPT" off >>/tmp/dvorak-layout.log 2>&1
    fi
}

wait_for_sway() {
    while ! swaymsg -t get_version &>/dev/null; do
        sleep 1
    done
}

is_alive() {
    local state
    state=$(awk '/^State:/ {print $2; exit}' "/proc/$1/status" 2>/dev/null)
    [[ -n "$state" && "$state" != "Z" ]]
}

while true; do
    wait_for_sway

    LAST_INDEX=""
    signal_for_index "$(get_layout_index)"

    rm -f "$SUBSCRIBE_FIFO"
    mkfifo "$SUBSCRIBE_FIFO"

    exec 3<> "$SUBSCRIBE_FIFO"

    swaymsg -t subscribe -m '["input"]' > "$SUBSCRIBE_FIFO" 2>/dev/null 3<&- &
    SWAYMSG_PID=$!

    sleep 0.2
    if ! is_alive "$SWAYMSG_PID"; then
        exec 3<&-
        wait "$SWAYMSG_PID" 2>/dev/null
        SWAYMSG_PID=""
        rm -f "$SUBSCRIBE_FIFO"
        sleep 1
        continue
    fi

    while is_alive "$SWAYMSG_PID"; do
        if read -r -t 5 _event <&3 2>/dev/null; then
            index=$(get_layout_index)
            signal_for_index "$index"
        fi
    done

    exec 3<&-
    wait "$SWAYMSG_PID" 2>/dev/null
    SWAYMSG_PID=""
    rm -f "$SUBSCRIBE_FIFO"

    sleep 1
done
