#!/bin/bash
# layout_switch.sh [dvorak|us|ua|ru]
# Uses xkb_switch_layout to switch pre-configured layouts.
# Indexes: 0=dvorak, 1=us qwerty, 2=ua, 3=ru
#
# Automatically signals all dvorak daemons:
#   dvorak layout  -> SIGUSR1 (enable remapping)
#   other layouts  -> SIGUSR2 (passthrough)

# Must point to the root-owned copy for sudoers security
SIGNAL_SCRIPT="/usr/local/bin/dvorak-signal.sh"

get_layout_index() {
    swaymsg -t get_inputs | jq -r '
        [.[] | select(.type=="keyboard")] | .[0].xkb_active_layout_index // 0
    '
}

set_layout() {
    swaymsg "input type:keyboard xkb_switch_layout $1"
}

#signal_dvorak() {
    #if [[ -x "$SIGNAL_SCRIPT" ]]; then
        #"$SIGNAL_SCRIPT" "$1"
    #else
        #echo "Warning: dvorak-signal.sh not found or not executable at $SIGNAL_SCRIPT" >&2
    #fi
#}

signal_dvorak() {
    if [[ -x "$SIGNAL_SCRIPT" ]]; then
        "$SIGNAL_SCRIPT" "$1" >>/tmp/dvorak-layout.log 2>&1
        echo "exit code: $?" >>/tmp/dvorak-layout.log
    else
        echo "NOT FOUND: $SIGNAL_SCRIPT" >>/tmp/dvorak-layout.log
    fi
}

switch_to() {
    local index="$1"
    set_layout "$index"
    if [[ "$index" -eq 0 ]]; then
        signal_dvorak on
    else
        signal_dvorak off
    fi
}

if [[ -n "$1" ]]; then
    case "$1" in
        dvorak) switch_to 0 ;;
        us)     switch_to 1 ;;
        ua)     switch_to 2 ;;
        ru)     switch_to 3 ;;
        *)      echo "Unknown layout: $1"; exit 1 ;;
    esac
else
    # Cycle: Dvorak (0) <-> Ukrainian (2)
    CURRENT=$(get_layout_index)
    if [[ "$CURRENT" == "0" ]]; then
        switch_to 2
    else
        switch_to 0
    fi
fi
