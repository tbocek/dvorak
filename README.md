# Dvorak <> Qwerty - Keyboard remapping for Linux when pressing L-CTRL, L-ALT, or L-WIN

Since I type with the "Dvorak" keyboard layout, the shortcuts such as ctrl-c, ctrl-x, or ctrl-v are not comfortable anymore for using the left hand only.

Furthermore, many applications have their default shortcuts, which I'm used to. So for these shortcuts I prefer "Querty". Since there is no way to configure this, this program intercept these keys and remap them from "Dvorak" to "Querty" when pressing L-CTRL, L-ALT, L-WIN, or any of those combinations.
   
With X11 I was relying on the [xdq](https://github.com/kentonv/dvorak-qwerty) from Kenton Varda. However, this does not work reliably with Wayland.

## Keyboard remapping with dvorak that works reliably with Wayland - make ctrl-c ctrl-c again (and not ctrl-i)

X11's XGrabKey() works partially with some application but not with others (e.g., gedit is not working). Since XGrabKey() is an X11 function with some support in Wayland, I was looking for a more stable solution. After a quick look to this [repo](https://github.com/kentonv/dvorak-qwerty), I saw that Kenton added a systemtap script to implement the mapping. It scared me a bit to follow the systemtap path, so I implemented an other solution based on /dev/uinput. The idea is to read /dev/input, grab keys with EVIOCGRAB, create a virtual device that can emit the keys and pass the keys from /dev/input to /dev/uinput. If L-CTRL, L-ALT, L-WIN is pressed it will map the keys back to "Qwerty".

This program is tested with Arch and Ubuntu, and Kenton Varda reported that it also works with Chrome OS.

## Installation

 * create binary with ```make```
 * install it with ```sudo make install```

This will copy 3 files: dvorak, 80-dvorak.rules, and dvorak@.service

The file is triggered on the udev rule and call dvorak systemd service with the device that was attached. The rule contains
the search term "keyb k360 k750", that will match case insensitive the device name. Only a device with name that contains the substring
"keyb k360 k750" will be considered. To prevent an endless loop, the newly created virtual device is excluded from mapping itself. If your keyboard name does not match these keywords, then you have to add a keyword matching your keyboard.

That way, the program ```dvorak``` will be called whenever an input device is attached.

If you have more mappings, e.g., a Dvorak mapping a non-Dvorak mapping, you can disable the mapping, as the shortcuts would be mappend as well, and for ctrl-c you need to press ctrl-i. To disable this mapping hit **3 times L-ALT** to disable the Dvorak <> Qwerty keyboard remapping.

## Run

Most likely, you will need to use sudo as it needs access to those input device. The following parameters can be used:

```
usage: dvorak [OPTION]
  -u                    Enable Umlaut mapping.
  -d /dev/input/by-id/… Specifies which device should be captured.
  -m STRING             Match only the STRING with the USB device name. 
                        STRING can contain multiple words, separated by space.

example: dvorak -u -d /dev/input/by-id/usb-Logitech_USB_Receiver-if02-event-kbd -m "k750 k350"
```
Once installed via ```make install```, the mapping will be applied whenever a keyboard is attached, as it listends to the udev event when a device is attached.

The flag ```-u``` was added to remap some keys when using the ```Dvorak intl., with dead keys``` keyboard layout. Since this layout is handy for special characters it deviates too much from the original US-based Dvorak layout. So this -u flag maps some characters back. Only use this if you are using ```Dvorak intl., with dead keys```.

## Not a matching device: [xyz]

If you see the above message in syslog or journalctl, it means that your keyboard device name does not have the string "keyb" (case insensitive) in it. For example, ```Not a matching device: [Logitech K360]```. In order to make it work with your device, in dvorak@.service, you can call the executable with

```
ExecStart=/usr/bin/dvorak /dev/input/%i keyb k360
```

## Unistallation

To uninstall with make use:

```

```

To uninstall manually, you can type (if you are not root, use sudo):

```
systemctl stop 'dvorak@*.service'
rm /usr/local/bin/dvorak
rm /etc/udev/rules.d/80-dvorak.rules
rm /etc/systemd/system/dvorak@.service
udevadm control --reload
systemctl restart systemd-udevd.service
systemctl daemon-reload
```

Certainly! Here's the revised version:

---

### Resolving a Boot Delay

If you experience an x-minute boot delay after installing the script, it's likely due to the `systemd-udev-settle.service`. The code seems to call this service, causing the computer to wait for device initialization, which significantly slows down the boot process.

This service is deprecated, and you can restore normal boot times by masking it with the following command:
```bash
systemctl mask systemd-udev-settle.service
```

---

## Related Links
I used the following sites for inspiration:

 * https://www.kernel.org/doc/html/v4.12/input/uinput.html
 * https://www.linuxquestions.org/questions/programming-9/uinput-any-complete-example-4175524044/
 * https://stackoverflow.com/questions/20943322/accessing-keys-from-linux-input-device
 * https://gist.github.com/toinsson/7e9fdd3c908b3c3d3cd635321d19d44d
