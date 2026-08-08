# Dvorak <> Qwerty - Keyboard remapping for Linux when pressing L-CTRL, R-CTRL, L-ALT, L-WIN, CAPSLOCK

Since I type with the "Dvorak" keyboard layout, the shortcuts such as ctrl-c, ctrl-x, or ctrl-v are not comfortable anymore for using the left hand only.

Furthermore, many applications have their default shortcuts, which I'm used to. So for these shortcuts I prefer "Qwerty". Since there is no way to configure this, this program intercepts these keys and remaps them from "Dvorak" to "Qwerty" when pressing L-CTRL, R-CTRL, L-ALT, L-WIN, CAPSLOCK, or any of those combinations. CAPSLOCK is also used as a modifier, but can be disabled with the "-c" flag.

With X11 I was relying on the [xdq](https://github.com/kentonv/dvorak-qwerty) from Kenton Varda. However, this does not work reliably with Wayland.

## Keyboard remapping with dvorak that works reliably with Wayland - make ctrl-c ctrl-c again (and not ctrl-i)

X11's XGrabKey() works partially with some application but not with others (e.g., gedit is not working). Since XGrabKey() is an X11 function with some support in Wayland, I was looking for a more stable solution. After a quick look to this [repo](https://github.com/kentonv/dvorak-qwerty), I saw that Kenton added a systemtap script to implement the mapping. It scared me a bit to follow the systemtap path, so I implemented an other solution based on /dev/uinput. The idea is to read /dev/input, grab keys with EVIOCGRAB, create a virtual device that can emit the keys and pass the keys from /dev/input to /dev/uinput. If L-CTRL, R-CTRL, L-ALT, L-WIN, CAPSLOCK is pressed it will map the keys back to "Qwerty".

This program is tested with Arch and Ubuntu, and Kenton Varda reported that it also works with Chrome OS.

## The input scrambling problem

Because `dvorak` works at the `/dev/input` level — grabbing raw events with `EVIOCGRAB` and re-emitting them through a virtual `/dev/uinput` device — it operates completely independently of the desktop environment's keyboard layout settings. This means:

* **Switching layouts scrambles shortcuts.** If you switch the OS keyboard layout from Dvorak to Qwerty (e.g., for a different language, a different user, or a non-Dvorak keyboard), `dvorak` keeps remapping modifier combinations. The result is double-remapped shortcuts: pressing Ctrl+C no longer produces Ctrl+C in either layout — it produces something unexpected like Ctrl+I.

* **Multiple keyboards, one mapping.** If you have multiple keyboards attached (e.g., a laptop built-in keyboard and an external USB keyboard), each running its own `dvorak` daemon, switching the OS layout for one keyboard affects the other, but the daemons have no way to know.

* **No feedback from the desktop.** Since `dvorak` sits below the display server, it cannot observe layout changes made via GNOME Settings, `setxkbmap`, `swaymsg input`, or any other desktop-level tool.

### Solution: signal-based mode switching

The `dvorak` program now supports two Unix signals to externally control whether remapping is active:

| Signal    | Effect                                                          |
|-----------|-----------------------------------------------------------------|
| `SIGUSR1` | **On** — enable Dvorak-to-Qwerty remapping (original behavior) |
| `SIGUSR2` | **Off** — passthrough mode (no remapping; all keys forwarded as-is) |

This allows an external script, desktop shortcut, or layout-switching hook to tell all running `dvorak` daemons to disable remapping when the OS layout is not Dvorak, and re-enable it when switching back.

**Thread safety:** if a signal arrives while a modifier-based shortcut is in progress (e.g., the user is holding Ctrl+C), the mode change is deferred until all modifier and remapped keys are released. This prevents key events from being split across two different mapping states, which would result in stuck or phantom keys.

**Signal authority:** once a signal has set the mode, the Left-Alt triple-press toggle is suppressed. Only another signal can change the mode. This prevents accidental toggling via the keyboard when the mode is being managed externally.

## Project structure

```
.
├── 80-dvorak.rules              # udev rule — triggers dvorak on device attach
├── dvorak                       # compiled binary
├── dvorak.c                     # source code
├── dvorak@.service              # systemd template service (basic udev-triggered mode)
├── examples/
│   ├── dvorak-signal-sudoers    # sudoers rule for passwordless signaling
│   ├── dvorak-signal.sh         # send on/off signals to all dvorak daemons
│   ├── dvorak-start.sh          # daemon launcher with retry, cleanup, and PID files
│   ├── dvorak-usb.service       # systemd template service for dvorak-start.sh
│   ├── sway_layout_switch_example.sh  # example Sway layout switcher with signal integration
│   └── sway_layout-watcher.sh   # watches Sway input events and signals on layout change
├── LICENSE
├── Makefile
└── README.md
```

## Installation

### Option A: Basic installation (udev-triggered)

This is the simplest setup. It automatically starts `dvorak` whenever a keyboard is attached.

```bash
make
sudo make install
```

This will copy 3 files: `dvorak`, `80-dvorak.rules`, and `dvorak@.service`.

The udev rule matches **every** input device (`SUBSYSTEM=="input", KERNEL=="event[0-9]*"`) and starts one `dvorak@<eventN>.service` instance per device — it contains no device filter of its own. Filtering happens inside `dvorak`, which exits 0 without grabbing anything when the device:

* has no `X`/`C`/`V` keys, i.e. is not a keyboard (mice, power buttons, audio jacks — the common case),
* is dvorak's own `Virtual Dvorak Keyboard` output device, which is always refused so the mapping cannot be applied twice,
* matches a `-i` keyword, or fails to match a `-m` keyword, if either option is given.

So the default installation captures *all* keyboards. To restrict it to specific ones, or to skip virtual devices created by other remappers, add `-m` / `-i` to `ExecStart` in `/etc/systemd/system/dvorak@.service` — not to the udev rule:

```
ExecStart=/usr/local/bin/dvorak -d /dev/input/%i -m "keyb k360 k750"
```

The signal interface itself still works here — `sudo pkill -SIGUSR2 -x dvorak` switches every daemon to passthrough. What this setup does *not* install is `dvorak-signal.sh`, the sudoers rule that lets a non-root user send those signals, or PID files. Add `-p /run/dvorak-%i.pid` to `ExecStart` for PID files, or use Option B for the complete arrangement.

### Option B: Installation with signal support (recommended for multi-layout setups)

If you switch between keyboard layouts at the OS level (e.g., Dvorak, Qwerty, Ukrainian, Russian), use this setup. It adds PID file tracking, signal-based on/off control, and a sudoers rule so your user can signal the root-owned daemons without a password.

#### Prerequisites

Your user must be in the `input` group (for the sudoers rule and `/dev/input` access):

```bash
sudo usermod -aG input $USER
```

**Log out and back in** for the group change to take effect. Verify with:

```bash
groups | grep input
```

#### Step 1: Build and install the binary

```bash
make
sudo cp dvorak /usr/local/bin/dvorak
sudo chmod 755 /usr/local/bin/dvorak
```

#### Step 2: Install the daemon start script

```bash
sudo cp examples/dvorak-start.sh /usr/local/bin/dvorak-start.sh
sudo chmod 755 /usr/local/bin/dvorak-start.sh
```

`dvorak-start.sh` takes a keyboard name as an argument and:
* Searches `/sys/class/input/*/device/name` for devices whose name contains the given string. Unlike dvorak's `-m`, this match is **case-sensitive** (`grep -F`), so the name must be spelled as it appears in `/proc/bus/input/devices`
* Retries for up to 180 seconds (90 attempts, 2 seconds apart) — useful for USB devices that appear after boot
* Kills any stale `dvorak` process already holding the device
* Starts `dvorak` with a PID file at `/run/dvorak-<eventN>.pid`
* Tries each matching event node in turn: exit code 0 means "not a keyboard" (a keyboard usually exposes several event nodes, only one of which carries the keys), so the script moves on to the next candidate. Any other exit code means the real keyboard was found, and the script exits so systemd can restart it

#### Step 3: Install the signal script

```bash
sudo cp examples/dvorak-signal.sh /usr/local/bin/dvorak-signal.sh
sudo chmod 755 /usr/local/bin/dvorak-signal.sh
```

`dvorak-signal.sh` reads PID files from `/run/dvorak-*.pid`, verifies each process is actually a `dvorak` daemon, and sends the appropriate signal. It auto-elevates to root via `sudo -n` (non-interactive) when run as a regular user. Falls back to `pkill` if no valid PID files are found.

#### Step 4: Install the sudoers rule

The `dvorak` daemons run as root (started via systemd), so signaling them requires root. The sudoers rule allows members of the `input` group to run `dvorak-signal.sh on` and `dvorak-signal.sh off` as root without a password:

```bash
sudo cp examples/dvorak-signal-sudoers /etc/sudoers.d/dvorak-signal
sudo chmod 440 /etc/sudoers.d/dvorak-signal
sudo visudo -cf /etc/sudoers.d/dvorak-signal   # verify syntax — must print "parsed OK"
```

> **Security note:** the sudoers rule points to `/usr/local/bin/dvorak-signal.sh`, which is root-owned. Never point a sudoers rule to a user-writable path (e.g., `~/.config/...`) as that would allow arbitrary code execution as root.

#### Step 5: Install the systemd service

Copy the template service and reload systemd:

```bash
sudo cp examples/dvorak-usb.service /etc/systemd/system/dvorak-usb@.service
sudo systemctl daemon-reload
```

Enable and start one instance per keyboard, using the keyboard name as the instance identifier:

```bash
sudo systemctl enable --now "dvorak-usb@Logitech K750.service"
sudo systemctl enable --now "dvorak-usb@Das Keyboard.service"
```

Check the status:

```bash
systemctl status "dvorak-usb@Logitech K750.service"
journalctl -u "dvorak-usb@Logitech K750.service" -f
```

#### Step 6: Test signaling

```bash
# From a terminal — should auto-elevate via sudo and succeed:
dvorak-signal.sh off    # all daemons -> passthrough
dvorak-signal.sh on     # all daemons -> Dvorak remapping enabled
```

#### Step 7 (optional): Integrate with your desktop layout switcher

An example Sway layout switching script is provided at `examples/sway_layout_switch_example.sh`. It switches the Sway keyboard layout and automatically signals all `dvorak` daemons:

* Switching to Dvorak (index 0) sends `dvorak-signal.sh on`
* Switching to any other layout sends `dvorak-signal.sh off`

To use it with Sway, copy it to your scripts directory and bind it to a key:

```bash
cp examples/sway_layout_switch_example.sh ~/.config/sway/scripts/layout_switch.sh
chmod +x ~/.config/sway/scripts/layout_switch.sh
```

Add to your Sway config (`~/.config/sway/config`):

```
# Cycle layouts with Super+Space
bindsym $mod+space exec ~/.config/sway/scripts/layout_switch.sh

# Or switch to a specific layout
bindsym $mod+F1 exec ~/.config/sway/scripts/layout_switch.sh dvorak
bindsym $mod+F2 exec ~/.config/sway/scripts/layout_switch.sh us
bindsym $mod+F3 exec ~/.config/sway/scripts/layout_switch.sh ua
bindsym $mod+F4 exec ~/.config/sway/scripts/layout_switch.sh ru
```

Adapt the layout names and indices in the script to match your Sway input configuration.

## Run

Most likely, you will need to use sudo as it needs access to input devices. The following parameters can be used:

```
usage: dvorak [OPTION]
  -d /dev/input/by-id/...   Specifies which device should be captured.
  -m STRING                 Match only the STRING with the USB device name.
                            STRING can contain multiple words, separated by space.
  -i STRING                 Ignore devices whose name matches STRING.
                            STRING can contain multiple words, separated by space.
                            Takes precedence over -m.
  -t                        Disable layout toggle feature (press Left-Alt 3 times to switch layout).
  -c                        Disable caps lock as a modifier.
  -p FILE                   Write PID to FILE (useful for daemon mode).

Signals:
  SIGUSR1                   Enable Dvorak mapping (on).
  SIGUSR2                   Disable mapping / passthrough (off).

example: dvorak -d /dev/input/by-id/usb-Logitech_USB_Receiver-if02-event-kbd -m "k750 k350"
example: dvorak -d /dev/input/event1 -i "virtual clickmate"
```

Once installed via `make install` or systemd services, the mapping will be applied whenever a keyboard is attached.

If you have more mappings, e.g., a Dvorak mapping and a non-Dvorak mapping, you can disable the mapping so the shortcuts are not remapped. There are two ways:

1. **Signal-based (recommended):** use `dvorak-signal.sh off` to switch all daemons to passthrough, and `dvorak-signal.sh on` to re-enable. Once controlled by signals, the keyboard toggle is suppressed — only another signal can change the mode.
2. **Keyboard toggle:** press **3 times L-ALT** to toggle the Dvorak-to-Qwerty remapping on or off. This only works when the mode has not been set by a signal. Can be disabled with the `-t` flag.

## Selecting and excluding devices

If you see this in `journalctl`:

```
Error: Device [Logitech K360] does not match any specified keywords.
```

it means a `-m` was given but the device name does not contain any of its keywords, so the device was left alone (exit code 1). Add the relevant keyword to the `-m` parameter in your service file:

```
ExecStart=/usr/local/bin/dvorak -d /dev/input/%i -m "keyb k360"
```

The complement of `-m` is `-i`: instead of listing every keyboard to capture, list
the device names to skip. This is useful with the udev rule, which starts an
instance for *every* input device that appears — including the virtual devices
created by other remappers. Grabbing such a device applies the Dvorak mapping a
second time, so a key that should arrive as `d` arrives as `e`:

```
ExecStart=/usr/local/bin/dvorak -d /dev/input/%i -i virtual
```

A device matching both `-i` and `-m` is ignored. Ignoring is not an error: dvorak
logs one line (`dvorak: ignoring [Clickmate Virtual Device 0] (matched "virtual")`)
and exits with status 0, without ever grabbing the device.

Or if using the `dvorak-start.sh` approach, pass the full device name:

```
ExecStart=/usr/local/bin/dvorak-start.sh "Logitech K360"
```

## Exit codes

Because udev starts an instance for every input device, most instances are expected to exit immediately. The exit code says why, and wrapper scripts rely on it:

| Code | Meaning |
|------|---------|
| `0` | Nothing to do, or done. Either the device was skipped without ever being grabbed — not a keyboard, excluded by `-i`, or dvorak's own virtual device — in which case a wrapper should try the next candidate; or a running daemon shut down cleanly on `SIGTERM`/`SIGINT` |
| `1` | Configuration or setup error — no `-d` given, `-m` matched nothing, `/dev/uinput` unavailable, or the device could not be grabbed because another process holds it. Retrying will not help |
| `2` | The device disappeared at runtime, or the virtual device stopped accepting writes. Restarting is appropriate |

All diagnostics are written to **stderr**, so they appear in `journalctl -u 'dvorak@*'` even though `dvorak@.service` sets `StandardOutput=null`.

## Troubleshooting

### Signals not delivered (Operation not permitted)

The `dvorak` daemons run as root (started via systemd). If you see `Operation not permitted` when running `dvorak-signal.sh`, check:

1. **Is the sudoers rule installed?**
   ```bash
   sudo cat /etc/sudoers.d/dvorak-signal
   ```
2. **Is your user in the `input` group?**
   ```bash
   groups | grep input
   ```
   If not: `sudo usermod -aG input $USER` then **log out and back in**.
3. **Is the sudoers file valid?**
   ```bash
   sudo visudo -cf /etc/sudoers.d/dvorak-signal
   ```

### Signal script works from terminal but not from keybinding

Most likely your user was just added to the `input` group but hasn't logged out and back in yet. Group changes only take effect on new login sessions.

### Stale PID files

If `dvorak-signal.sh` reports stale PID files, it means a previous `dvorak` process was killed with SIGKILL (which cannot be caught, so the PID file wasn't cleaned up). The PID files live in `/run` (tmpfs), so they are automatically cleared on reboot. To clean them manually:

```bash
sudo rm -f /run/dvorak-*.pid
```

## Uninstallation

To uninstall the basic installation:

```
sudo make uninstall
```

To fully uninstall everything (basic + signal support), run:

```bash
# Stop all running services
sudo systemctl stop 'dvorak@*.service'
sudo systemctl stop 'dvorak-usb@*.service'
sudo systemctl disable 'dvorak@*.service'
sudo systemctl disable 'dvorak-usb@*.service'

# Remove binaries and scripts
sudo rm -f /usr/local/bin/dvorak
sudo rm -f /usr/local/bin/dvorak-start.sh
sudo rm -f /usr/local/bin/dvorak-signal.sh

# Remove udev rules and service files
sudo rm -f /etc/udev/rules.d/80-dvorak.rules
sudo rm -f /etc/systemd/system/dvorak@.service
sudo rm -f /etc/systemd/system/dvorak-usb@*.service

# Remove sudoers rule
sudo rm -f /etc/sudoers.d/dvorak-signal

# Clean up PID files
sudo rm -f /run/dvorak-*.pid

# Reload
sudo udevadm control --reload
sudo systemctl restart systemd-udevd.service
sudo systemctl daemon-reload
```

---

## Related Links
I used the following sites for inspiration:

 * https://www.kernel.org/doc/html/v4.12/input/uinput.html
 * https://www.linuxquestions.org/questions/programming-9/uinput-any-complete-example-4175524044/
 * https://stackoverflow.com/questions/20943322/accessing-keys-from-linux-input-device
 * https://gist.github.com/toinsson/7e9fdd3c908b3c3d3cd635321d19d44d
