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

| Signal | Effect |
|--------|--------|
| `SIGUSR1` | **On** — enable Dvorak-to-Qwerty remapping (original behavior) |
| `SIGUSR2` | **Off** — passthrough mode (no remapping; all keys forwarded as-is) |

This allows an external script, desktop shortcut, or layout-switching hook to tell all running `dvorak` daemons to disable remapping when the OS layout is not Dvorak, and re-enable it when switching back.

**Thread safety:** if a signal arrives while a modifier-based shortcut is in progress (e.g., the user is holding Ctrl+C), the mode change is deferred until all modifier and remapped keys are released. This prevents key events from being split across two different mapping states, which would result in stuck or phantom keys.

**Signal authority:** once a signal has set the mode, the Left-Alt triple-press toggle is suppressed. Only another signal can change the mode. This prevents accidental toggling via the keyboard when the mode is being managed externally.

## Installation

### Basic installation (udev-triggered)

* Create binary with `make`
* Install with `sudo make install`

This will copy 3 files: `dvorak`, `80-dvorak.rules`, and `dvorak@.service`.

The udev rule triggers the `dvorak` systemd service whenever an input device is attached. The rule contains a search term (e.g., `"keyb k360 k750"`) that matches the device name case-insensitively. Only devices whose name contains a matching substring will be considered. The newly created virtual device is excluded from mapping itself to prevent an endless loop.

If your keyboard name does not match the default keywords, edit the udev rule and service file to add a keyword matching your keyboard.

### Alternative installation (with signal support and PID files)

If you need signal-based mode switching (e.g., you switch between Dvorak and Qwerty layouts at the OS level), use the files in the `examples/` directory instead of or in addition to the basic installation.

**1. Copy the binaries:**

```bash
sudo cp dvorak /usr/local/bin/dvorak
sudo cp examples/dvorak-start.sh /usr/local/bin/dvorak-start.sh
sudo chmod +x /usr/local/bin/dvorak-start.sh
2. Install the systemd service:
Copy examples/dvorak-usb.service to /etc/systemd/system/ and create one instance per keyboard:
bash


sudo cp examples/dvorak-usb.service /etc/systemd/system/dvorak-usb@.service
sudo systemctl daemon-reload
Enable and start a service for each keyboard, using the keyboard name as the instance identifier:
bash


sudo systemctl enable --now "dvorak-usb@Logitech K750.service"
sudo systemctl enable --now "dvorak-usb@Das Keyboard.service"
The service calls dvorak-start.sh which:
Searches /sys/class/input/ for a device matching the given name
Retries for up to 120 seconds (useful for USB devices that appear after boot)
Kills any stale dvorak process already holding the device
Starts dvorak with a PID file at /run/dvorak-<eventN>.pid
3. Install the signal script:
bash


sudo cp examples/dvorak-signal.sh /usr/local/bin/dvorak-signal.sh
sudo chmod +x /usr/local/bin/dvorak-signal.sh
4. Use it:
bash


# Disable remapping on all keyboards (passthrough):
dvorak-signal.sh off
Re-enable Dvorak remapping on all keyboards:
dvorak-signal.sh on



Or signal directly without the script:

```bash
pkill -SIGUSR2 -x dvorak   # all daemons → passthrough
pkill -SIGUSR1 -x dvorak   # all daemons → Dvorak on
Hook these into your desktop's layout-switching mechanism — for example, a GNOME extension, a Sway keybinding, or an inotifywait script watching the keyboard layout.
Project structure


.
├── 80-dvorak.rules         # udev rule — triggers dvorak on device attach
├── dvorak                  # compiled binary
├── dvorak.c                # source code
├── dvorak@.service         # systemd template service (basic udev-triggered mode)
├── examples/
│   ├── dvorak-signal.sh    # send on/off signals to all dvorak daemons
│   ├── dvorak-start.sh     # daemon launcher with retry, cleanup, and PID files
│   └── dvorak-usb.service  # systemd template service for dvorak-start.sh
├── LICENSE
├── Makefile
└── README.md
Run
Most likely, you will need to use sudo as it needs access to input devices. The following parameters can be used:


usage: dvorak [OPTION]
  -d /dev/input/by-id/...   Specifies which device should be captured.
  -m STRING                 Match only the STRING with the USB device name.
                            STRING can contain multiple words, separated by space.
  -t                        Disable layout toggle feature (press Left-Alt 3 times to switch layout).
  -c                        Disable caps lock as a modifier.
  -p FILE                   Write PID to FILE (useful for daemon mode).
Signals: SIGUSR1 Enable Dvorak mapping (on). SIGUSR2 Disable mapping / passthrough (off).
example: dvorak -d /dev/input/by-id/usb-Logitech_USB_Receiver-if02-event-kbd -m "k750 k350"



Once installed via `make install`, the mapping will be applied whenever a keyboard is attached, as it listens to the udev event when a device is attached.

If you have more mappings, e.g., a Dvorak mapping and a non-Dvorak mapping, you can disable the mapping so the shortcuts are not remapped. There are two ways:

1. **Signal-based (recommended):** use `dvorak-signal.sh off` to switch all daemons to passthrough, and `dvorak-signal.sh on` to re-enable. Once controlled by signals, the keyboard toggle is suppressed.
2. **Keyboard toggle:** press **3 times L-ALT** to toggle the Dvorak-to-Qwerty remapping on or off. This only works when the mode has not been set by a signal.

## Not a matching device: [xyz]

If you see the above message in syslog or journalctl, it means that your keyboard device name does not have a matching string in it. For example, `Not a matching device: [Logitech K360]`. In order to make it work with your device, add the relevant keyword to the `-m` parameter in your service file:

ExecStart=/usr/local/bin/dvorak -d /dev/input/%i -m "keyb k360"



Or if using the `dvorak-start.sh` approach, pass the full device name:

ExecStart=/usr/local/bin/dvorak-start.sh "Logitech K360"



## Uninstallation

To uninstall with make use:

sudo make uninstall



To uninstall manually, you can type (if you are not root, use sudo):

```bash
# Stop all running services
systemctl stop 'dvorak@*.service'
systemctl stop 'dvorak-usb@*.service'
Remove binaries and scripts
rm -f /usr/local/bin/dvorak rm -f /usr/local/bin/dvorak-start.sh rm -f /usr/local/bin/dvorak-signal.sh
Remove udev rules and service files
rm -f /etc/udev/rules.d/80-dvorak.rules rm -f /etc/systemd/system/dvorak@.service rm -f /etc/systemd/system/dvorak-usb@*.service
Clean up PID files
rm -f /run/dvorak-*.pid
Reload
udevadm control --reload systemctl restart systemd-udevd.service systemctl daemon-reload



---

## Related Links
I used the following sites for inspiration:

 * https://www.kernel.org/doc/html/v4.12/input/uinput.html
 * https://www.linuxquestions.org/questions/programming-9/uinput-any-complete-example-4175524044/
 * https://stackoverflow.com/questions/20943322/accessing-keys-from-linux-input-device
 * https://gist.github.com/toinsson/7e9fdd3c908b3c3d3cd635321d19d44d
