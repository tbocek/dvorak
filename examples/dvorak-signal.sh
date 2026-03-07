#!/usr/bin/env bash
#
# dvorak-signal.sh — Send on/off signals to all running dvorak daemons.
#
# Usage:
#   dvorak-signal.sh on    # SIGUSR1 — enable Dvorak mapping
#   dvorak-signal.sh off   # SIGUSR2 — passthrough mode
#
# Works with:
#   1. PID files (reads from /run/dvorak-*.pid by default)
#   2. Falls back to pkill if no PID files found/valid
#

set -euo pipefail

PIDFILE_DIR="/run"
PIDFILE_GLOB="dvorak-*.pid"
PROCESS_NAME="dvorak"

usage() {
    echo "Usage: $0 {on|off}" >&2
    echo "  on   — enable Dvorak mapping (SIGUSR1)" >&2
    echo "  off  — passthrough / no mapping (SIGUSR2)" >&2
    exit 1
}

[ $# -eq 1 ] || usage

case "$1" in
    on)  SIG="USR1" ;;
    off) SIG="USR2" ;;
    *)   usage ;;
esac

sent=0

# Try PID files first
for pidfile in "${PIDFILE_DIR}"/${PIDFILE_GLOB}; do
    [ -f "$pidfile" ] || continue
    pid=$(cat "$pidfile" 2>/dev/null) || true
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        proc_name=$(cat "/proc/$pid/comm" 2>/dev/null) || true
        if [ "$proc_name" = "dvorak" ]; then
            if kill -s "$SIG" "$pid" 2>/dev/null; then
                echo "Sent SIG${SIG} to PID ${pid} (from ${pidfile})"
                sent=$((sent + 1))
            else
                echo "Warning: failed to signal PID ${pid} (from ${pidfile})" >&2
            fi
        else
            echo "Warning: PID ${pid} from ${pidfile} is not dvorak (is: ${proc_name:-unknown})" >&2
        fi
    else
        echo "Warning: stale PID file ${pidfile} (pid=${pid:-empty})" >&2
    fi
done

# Fall back to pkill if no valid PID files were found
if [ "$sent" -eq 0 ]; then
    if pgrep -x "$PROCESS_NAME" >/dev/null 2>&1; then
        pkill -"$SIG" -x "$PROCESS_NAME" || true
        count=$(pgrep -cx "$PROCESS_NAME" 2>/dev/null || echo "?")
        echo "Sent SIG${SIG} to ${count} '${PROCESS_NAME}' process(es) via pkill"
    else
        echo "Error: no running '${PROCESS_NAME}' processes found." >&2
        exit 1
    fi
fi
