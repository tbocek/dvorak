#!/usr/bin/env bash
#
# dvorak-signal.sh — Send on/off signals to all running dvorak daemons.
#
# Usage:
#   dvorak-signal.sh on    # SIGUSR1 — enable Dvorak mapping
#   dvorak-signal.sh off   # SIGUSR2 — passthrough mode
#
# Automatically elevates to root via sudo if needed (dvorak daemons run as root).
# Requires a sudoers rule for non-interactive use — see examples/dvorak-signal-sudoers.
#
# Works with:
#   1. PID files (reads from /run/dvorak-*.pid by default)
#   2. Falls back to pkill if no PID files found/valid
#

set -uo pipefail
shopt -s nullglob

PIDFILE_DIR="/run"
PIDFILE_GLOB="dvorak-*.pid"
PROCESS_NAME="dvorak"

usage() {
    echo "Usage: $0 {on|off}" >&2
    echo "  on   — enable Dvorak mapping (SIGUSR1)" >&2
    echo "  off  — passthrough / no mapping (SIGUSR2)" >&2
    exit 1
}

[[ $# -eq 1 ]] || usage

case "$1" in
    on)  SIG="USR1" ;;
    off) SIG="USR2" ;;
    *)   usage ;;
esac

# Auto-elevate: dvorak daemons run as root, signaling them requires root.
# Uses sudo -n (non-interactive) so it fails immediately with a clear error
# instead of hanging waiting for a password (e.g., when called from a keybinding).
if [[ $EUID -ne 0 ]]; then
    SELF="$(realpath "$0" 2>/dev/null || readlink -f "$0")"
    exec sudo -n -- "$SELF" "$@"
fi

sent=0
failed=0

# Try PID files first
for pidfile in "${PIDFILE_DIR}"/${PIDFILE_GLOB}; do
    pid=$(cat "$pidfile" 2>/dev/null) || continue
    [[ "$pid" =~ ^[0-9]+$ ]] || { echo "Warning: corrupt PID file ${pidfile}" >&2; failed=$((failed + 1)); continue; }

    if ! kill -0 "$pid" 2>/dev/null; then
        echo "Warning: stale PID file ${pidfile} (pid=${pid})" >&2
        failed=$((failed + 1))
        continue
    fi

    proc_name=$(cat "/proc/$pid/comm" 2>/dev/null) || true
    if [[ "$proc_name" != "$PROCESS_NAME" ]]; then
        echo "Warning: PID ${pid} from ${pidfile} is not dvorak (is: ${proc_name:-unknown})" >&2
        failed=$((failed + 1))
        continue
    fi

    if kill -s "$SIG" "$pid" 2>/dev/null; then
        echo "Sent SIG${SIG} to PID ${pid} (from ${pidfile})"
        sent=$((sent + 1))
    else
        echo "Warning: failed to signal PID ${pid} (from ${pidfile})" >&2
        failed=$((failed + 1))
    fi
done

# Fall back to pkill if no valid PID files were found
if [[ $sent -eq 0 ]]; then
    if pkill -"$SIG" -x "$PROCESS_NAME" 2>/dev/null; then
        echo "Sent SIG${SIG} to '${PROCESS_NAME}' process(es) via pkill"
    else
        echo "Error: no running '${PROCESS_NAME}' processes found." >&2
        exit 1
    fi
fi

# Partial success = non-zero exit so callers can detect
if [[ $failed -gt 0 ]]; then
    echo "Warning: ${failed} pidfile(s) had issues (${sent} signaled successfully)" >&2
    exit 2
fi

exit 0
