#!/bin/bash
# layout-watcher.sh — Companion to swaykbdd.
# Listens for Sway input events and signals dvorak daemons
# whenever the keyboard layout changes.
#
# Layout indexes (must match your sway input config):
#   0 = dvorak  → dvorak-signal.sh on
#   1/2/3       → dvorak-signal.sh off

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

# Signal once at startup to match whatever layout is already active
signal_for_index "$(get_layout_index)"

# Subscribe to input events (fired on every layout change)
swaymsg -t subscribe -m '["input"]' | while read -r _event; do
    index=$(get_layout_index)
    signal_for_index "$index"
done
