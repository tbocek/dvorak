#!/bin/bash
set -uo pipefail

shopt -s nullglob

PIDFILE_DIR="/run"
MATCH_NAME="${1:-}"

if [[ -z "$MATCH_NAME" ]]; then
    echo "ERROR: No keyboard name provided."
    echo "Usage: $0 \"Keyboard Name\""
    exit 1
fi

echo "Looking for keyboard matching: $MATCH_NAME"

for i in $(seq 1 60); do
    for name_file in /sys/class/input/event*/device/name; do
        if [[ -f "$name_file" ]] && grep -qF "$MATCH_NAME" "$name_file" 2>/dev/null; then
            EVENT_NAME=$(echo "$name_file" | grep -o 'event[0-9]*')
            DEV="/dev/input/${EVENT_NAME}"

            if [[ ! -e "$DEV" ]]; then
                echo "Device node $DEV not ready yet, retrying..."
                break
            fi

            echo "Found $MATCH_NAME at $DEV"

            PIDFILE="${PIDFILE_DIR}/dvorak-${EVENT_NAME}.pid"

            # Clean up stale pidfile from a previous run
            if [[ -f "$PIDFILE" ]]; then
                old_pid=$(cat "$PIDFILE" 2>/dev/null) || true
                if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
                    old_proc_name=$(cat "/proc/$old_pid/comm" 2>/dev/null) || true
                    if [[ "$old_proc_name" == "dvorak" ]]; then
                        echo "Killing stale dvorak process $old_pid (from pidfile $PIDFILE)"
                        kill "$old_pid" 2>/dev/null || true
                        sleep 1
                    fi
                fi
                rm -f "$PIDFILE"
            fi

            # Also check for any dvorak process holding the device directly
            pids=$(fuser "$DEV" 2>/dev/null | tr -dc '0-9 ') || true
            for pid in $pids; do
                proc_name=$(cat "/proc/$pid/comm" 2>/dev/null) || true
                if [[ "$proc_name" == "dvorak" ]]; then
                    echo "Killing stale dvorak process $pid on $DEV"
                    kill "$pid" 2>/dev/null || true
                    sleep 1
                fi
            done

            echo "Starting dvorak on $DEV (pidfile: $PIDFILE)"
            # on my system, dvorak is located in /usr/local/bin/dvorak
            exec ../dvorak -d "$DEV" -p "$PIDFILE"
        fi
    done
    sleep 2
done

echo "ERROR: $MATCH_NAME not found after 120 seconds"
exit 1
