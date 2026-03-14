#!/bin/bash
set -uo pipefail
shopt -s nullglob

PIDFILE_DIR="/run"
MATCH_NAME="${1:-}"
CHILD_PID=""

if [[ -z "$MATCH_NAME" ]]; then
    echo "ERROR: No keyboard name provided." >&2
    echo "Usage: $0 \"Keyboard Name\"" >&2
    exit 1
fi

forward_signal() {
    if [[ -n "$CHILD_PID" ]] && kill -0 "$CHILD_PID" 2>/dev/null; then
        kill -TERM "$CHILD_PID" 2>/dev/null || true
        wait "$CHILD_PID" 2>/dev/null || true
    fi
    exit 0
}

trap forward_signal SIGTERM SIGINT

cleanup_stale() {
    local dev="$1"
    local pidfile="$2"

    if [[ -f "$pidfile" ]]; then
        local old_pid
        old_pid=$(cat "$pidfile" 2>/dev/null) || true
        if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
            local old_proc_name
            old_proc_name=$(cat "/proc/$old_pid/comm" 2>/dev/null) || true
            if [[ "$old_proc_name" == "dvorak" ]]; then
                echo "Killing stale dvorak process $old_pid"
                kill "$old_pid" 2>/dev/null || true
                sleep 0.5
                kill -9 "$old_pid" 2>/dev/null || true
                sleep 0.5
            fi
        fi
        rm -f "$pidfile"
    fi

    local pids
    pids=$(fuser "$dev" 2>/dev/null | tr -dc '0-9 ') || true
    for pid in $pids; do
        local proc_name
        proc_name=$(cat "/proc/$pid/comm" 2>/dev/null) || true
        if [[ "$proc_name" == "dvorak" ]]; then
            echo "Killing stale dvorak process $pid on $dev"
            kill "$pid" 2>/dev/null || true
            sleep 0.5
            kill -9 "$pid" 2>/dev/null || true
            sleep 0.5
        fi
    done
}

find_all_devices() {
    for name_file in /sys/class/input/event*/device/name; do
        if [[ -f "$name_file" ]] && grep -qF "$MATCH_NAME" "$name_file" 2>/dev/null; then
            local event_name
            event_name=$(echo "$name_file" | grep -o 'event[0-9]*')
            local dev="/dev/input/${event_name}"
            if [[ -e "$dev" ]]; then
                echo "$dev"
            fi
        fi
    done
}

echo "Dvorak daemon wrapper starting for: $MATCH_NAME"

# Wait for at least one matching device — up to 180 seconds on initial boot
DEVICES=()
for i in $(seq 1 90); do
    mapfile -t DEVICES < <(find_all_devices)
    if [[ ${#DEVICES[@]} -gt 0 ]]; then
        break
    fi
    sleep 2
done

if [[ ${#DEVICES[@]} -eq 0 ]]; then
    echo "ERROR: $MATCH_NAME not found after 180 seconds" >&2
    exit 1
fi

# Try each candidate device. dvorak exits 0 if the device is not a real
# keyboard (e.g. consumer control or LED sub-device). We skip those and
# try the next. Any other exit means we found the keyboard (and it either
# ran successfully or the device vanished at runtime).
for DEV in "${DEVICES[@]}"; do
    EVENT_NAME=$(basename "$DEV")
    PIDFILE="${PIDFILE_DIR}/dvorak-${EVENT_NAME}.pid"

    cleanup_stale "$DEV" "$PIDFILE"

    echo "Trying dvorak on $DEV (pidfile: $PIDFILE)"

    /usr/local/bin/dvorak -d "$DEV" -p "$PIDFILE" &
    CHILD_PID=$!
    wait "$CHILD_PID" 2>/dev/null
    EXIT_CODE=$?
    CHILD_PID=""
    rm -f "$PIDFILE"

    if [[ $EXIT_CODE -eq 0 ]]; then
        # Exit 0 = "not a keyboard" or clean stop from signal.
        echo "$DEV exited 0 (not a keyboard or clean stop), trying next..."
        continue
    fi

    # Non-zero: this was the real keyboard. Exit so systemd can restart us.
    echo "dvorak on $DEV exited with code $EXIT_CODE"
    exit "$EXIT_CODE"
done

# All candidates returned 0 — none were keyboards, or device vanished.
# Exit non-zero so systemd restarts and we re-scan.
echo "No viable keyboard device found among candidates, will retry."
exit 1
