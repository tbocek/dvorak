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

# Read the start time (clock ticks since boot) from /proc/<pid>/stat.
# Strips through the last ")" to handle spaces in the comm field.
get_start_ticks() {
    awk '{
        sub(/^.*\) /, "")
        print $20
    }' "/proc/$1/stat" 2>/dev/null
}

# Read old PID before we overwrite the file
old_pid=""
if [[ -f "$PIDFILE" ]]; then
    old_pid=$(<"$PIDFILE")
fi

# Write our PID file atomically *before* sending SIGTERM, so the old
# instance's EXIT trap sees our PID in the file and won't delete it.
_tmpfile=$(mktemp "${PIDFILE}.XXXXXX")
echo "$MY_PID" > "$_tmpfile"
mv -f "$_tmpfile" "$PIDFILE"

# Kill the old instance if it's genuinely ours
if [[ -n "$old_pid" && "$old_pid" != "$MY_PID" ]] &&
   kill -0 "$old_pid" 2>/dev/null &&
   grep -Fqa "$SCRIPT_NAME" "/proc/$old_pid/cmdline" 2>/dev/null; then

    # Snapshot child PIDs with their start times so we can safely
    # force-kill them later even after reparenting to PID 1.
    old_children=()
    old_child_ticks=()
    while IFS= read -r cpid; do
        [[ -z "$cpid" ]] && continue
        old_children+=("$cpid")
        old_child_ticks+=("$(get_start_ticks "$cpid")")
    done < <(pgrep -P "$old_pid" 2>/dev/null)

    # SIGTERM the parent — triggers its EXIT trap, which cooperatively
    # kills its own children
    kill "$old_pid" 2>/dev/null
    sleep 0.3

    # Force-kill the parent if it's somehow still alive
    kill -9 "$old_pid" 2>/dev/null

    # Force-kill any children that survived, verifying start time
    # to guard against PID reuse
    for i in "${!old_children[@]}"; do
        cpid="${old_children[$i]}"
        expected="${old_child_ticks[$i]}"
        actual=$(get_start_ticks "$cpid")
        if [[ -n "$actual" && "$actual" == "$expected" ]]; then
            kill -9 "$cpid" 2>/dev/null
        fi
    done
fi

# Cleanup: kill our own children and remove the PID file on exit.
# Covers SIGTERM, SIGINT, SIGHUP, and normal exit.
cleanup() {
    pkill -P $$ 2>/dev/null
    sleep 0.1
    pkill -9 -P $$ 2>/dev/null
    # Only remove the PID file if it still belongs to us
    # (a newer instance may have already overwritten it)
    [[ -f "$PIDFILE" && "$(<"$PIDFILE")" == "$$" ]] && rm -f "$PIDFILE"
}
trap cleanup EXIT
# ─────────────────────────────────────────────────────────────────────

SIGNAL_SCRIPT="/usr/local/bin/dvorak-signal.sh"
LAST_INDEX=""

get_layout_index() {
    swaymsg -t get_inputs | jq -r '
        [.[] | select(.type=="keyboard")] | .[0].xkb_active_layout_index // 0
    '
}

signal_for_index() {
    local index="$1"

    # Deduplicate: don't signal if the layout hasn't actually changed
    [[ "$index" == "$LAST_INDEX" ]] && return
    LAST_INDEX="$index"

    if [[ "$index" -eq 0 ]]; then
        "$SIGNAL_SCRIPT" on  >>/tmp/dvorak-layout.log 2>&1
    else
        "$SIGNAL_SCRIPT" off >>/tmp/dvorak-layout.log 2>&1
    fi
}

# Main loop: subscribe to input events, reconnect if sway restarts.
# Process substitution keeps the while-loop in the *main* shell,
# so $LAST_INDEX updates persist and cleanup() can find swaymsg
# as a direct child of $$.
while true; do
    # Signal once at (re)connect to match whatever layout is active
    signal_for_index "$(get_layout_index)"

    while read -r _event; do
        index=$(get_layout_index)
        signal_for_index "$index"
    done < <(swaymsg -t subscribe -m '["input"]')

    # swaymsg disconnected — wait for sway to come back
    sleep 1
done
