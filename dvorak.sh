#!/bin/bash

#https://stackoverflow.com/questions/41242460/how-to-export-dbus-session-bus-address
PID=$(pidof -s gnome-shell)

if [[ -z "${PID}" ]]; then
  echo "Gnome Shell not running?"
  exit 1
fi

#https://askubuntu.com/questions/978711/how-do-i-split-a-proc-environ-file-in-separate-lines
#https://stackoverflow.com/questions/369758/how-to-trim-whitespace-from-a-bash-variable
export DBUS_SESSION_BUS_ADDRESS=`xargs -0 -L1 -a /proc/$PID/environ | grep DBUS_SESSION_BUS_ADDRESS |cut -d= -f2-`

if [[ -z "${SUDO_USER}" ]]; then
  #https://askubuntu.com/questions/978711/how-do-i-split-a-proc-environ-file-in-separate-lines
  export SUDO_USER=`xargs -0 -L1 -a /proc/$PID/environ | grep "USER=" |cut -d= -f2-`
fi
/usr/local/bin/dvorak "$@"