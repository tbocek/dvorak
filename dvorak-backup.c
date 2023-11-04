/*
 * Copyright 2018 Thomas Bocek
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 */

/*
 * Why is this tool useful?
 * ========================
 *
 * Since I type with the "Dvorak" keyboard layout, the shortcuts such
 * as ctrl-c, ctrl-x, or ctrl-v are not comfortable anymore and one of them
 * require two hands to press.
 *
 * Furthermore, applications such as Intellij and Eclipse have their
 * shortcuts, which I'm used to. So for these shortcuts I prefer "Querty".
 * Since there is no way to configure this, I had to intercept the
 * keys and remap the keys from "Dvorak" to "Querty" once CTRL, ALT,
 * WIN or any of those combinations are pressed.
 *
 * With X.org I was relying on the wonderful tool from Kenton Varda,
 * which I modified a bit, to make it work when Numlock is active. Other
 * than that, it worked as expected.
 *
 * And then came Wayland. XGrabKey() works partially with some application
 * but not with others (e.g., gedit is not working). Since XGrabKey() is
 * an X.org function with some support in Wayland, I was looking for a more
 * stable solution. After a quick look to the repo https://github.com/kentonv/dvorak-qwerty
 * I saw that Kenton added a systemtap script to implement the mapping. This
 * scared me a bit to follow that path, so I implemented an other solution
 * based on /dev/uinput. The idea is to read /dev/input, grab keys with
 * EVIOCGRAB, create a virtual device that can emit the keys and pass
 * the keys from /dev/input to /dev/uinput. If CTRL/ALT/WIN is
 * pressed it will map the keys back to "Qwerty".
 *
 * Installation
 * ===========
 *
 * make dvorak
 * //make sure your user belongs to the group "input" -> ls -la /dev/input
 * //this also applies for /dev/uinput -> https://github.com/tuomasjjrasanen/python-uinput/blob/master/udev-rules/40-uinput.rules
 * //start it in startup applications
 *
 * Related Links
 * =============
 * I used the following sites for inspiration:
 * https://www.kernel.org/doc/html/v4.12/input/uinput.html
 * https://www.linuxquestions.org/questions/programming-9/uinput-any-complete-example-4175524044/
 * https://stackoverflow.com/questions/20943322/accessing-keys-from-linux-input-device
 * https://gist.github.com/toinsson/7e9fdd3c908b3c3d3cd635321d19d44d
 *
 */
#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/uinput.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX_LENGTH 16

static int emit(int fd, int type, int code, int val) {
    struct input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    /* timestamp values below are ignored */
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;

    return write(fd, &ie, sizeof(ie));
}

//from: https://github.com/kentonv/dvorak-qwerty/tree/master/unix
static int modifier_bit(int key) {
    switch (key) {
        case KEY_LEFTCTRL:
            return 1;
        case KEY_RIGHTCTRL:
            return 2;
        case KEY_LEFTALT:
            return 4;
        case KEY_LEFTMETA:
            return 8;
        case KEY_CAPSLOCK:
            return 16;
    }
    return 0;
}

static int umlaut2dvorak(int key) {
    switch (key) {
        case KEY_A:
            return KEY_X;
        case KEY_X:
            return KEY_A;
        case KEY_S:
            return KEY_R;
        case KEY_R:
            return KEY_S;
        case KEY_F:
            return KEY_T;
        case KEY_T:
            return KEY_F;
        default:
            return key;
    }
}

//from: https://github.com/kentonv/dvorak-qwerty/tree/master/unix
static int qwerty2dvorak(int key) {
    switch (key) {
        case KEY_MINUS:
            return KEY_APOSTROPHE;
        case KEY_EQUAL:
            return KEY_RIGHTBRACE;
        case KEY_Q:
            return KEY_X;
        case KEY_W:
            return KEY_COMMA;
        case KEY_E:
            return KEY_D;
        case KEY_R:
            return KEY_O;
        case KEY_T:
            return KEY_K;
        case KEY_Y:
            return KEY_T;
        case KEY_U:
            return KEY_F;
        case KEY_I:
            return KEY_G;
        case KEY_O:
            return KEY_S;
        case KEY_P:
            return KEY_R;
        case KEY_LEFTBRACE:
            return KEY_MINUS;
        case KEY_RIGHTBRACE:
            return KEY_EQUAL;
        case KEY_A:
            return KEY_A;
        case KEY_S:
            return KEY_SEMICOLON;
        case KEY_D:
            return KEY_H;
        case KEY_F:
            return KEY_Y;
        case KEY_G:
            return KEY_U;
        case KEY_H:
            return KEY_J;
        case KEY_J:
            return KEY_C;
        case KEY_K:
            return KEY_V;
        case KEY_L:
            return KEY_P;
        case KEY_SEMICOLON:
            return KEY_Z;
        case KEY_APOSTROPHE:
            return KEY_Q;
        case KEY_Z:
            return KEY_SLASH;
        case KEY_X:
            return KEY_B;
        case KEY_C:
            return KEY_I;
        case KEY_V:
            return KEY_DOT;
        case KEY_B:
            return KEY_N;
        case KEY_N:
            return KEY_L;
        case KEY_M:
            return KEY_M;
        case KEY_COMMA:
            return KEY_W;
        case KEY_DOT:
            return KEY_E;
        case KEY_SLASH:
            return KEY_LEFTBRACE;
        default:
            return key;
    }
}

void usage(const char *path) {
    /* take only the last portion of the path */
    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    fprintf(stderr, "usage: %s [OPTION]\n", basename);
    fprintf(stderr, "  -u\t\t\t"
                    "Enable Umlaut mapping.\n");
    fprintf(stderr, "  -d /dev/input/by-id/…\t"
                    "Specifies which device should be captured.\n");
    fprintf(stderr, "  -m STRING\t\t"
                    "Match only the STRING with the USB device name. \n"
                    "\t\t\tSTRING can contain multiple words, separated by space.\n");
    fprintf(stderr, "  -t\t\t\t"
                    "Disable layout toggle feature (press Left-Alt 3 times to switch layout).\n");
    fprintf(stderr, "  -c\t\t\t"
                    "Disable caps lock as a modifier.\n\n");
    fprintf(stderr, "example: %s -u -d /dev/input/by-id/usb-Logitech_USB_Receiver-if02-event-kbd -m \"k750 k350\"\n", basename);
}

int main(int argc, char *argv[]) {

    struct input_event ev;
    struct uinput_setup usetup;
    ssize_t n;
    int fdi, fdo, i, mod_current, code, name_ret, mod_state = 0, array_qwerty_counter = 0, array_umlaut_counter = 0, lAlt = 0, opt;
    bool alt_gr = false, isDvorak = false, isUmlaut = false, lshift = false, rshift = false, noToggle = false, noCapsLockAsModifier = false;
    int array_qwerty[MAX_LENGTH] = {0}, array_umlaut[MAX_LENGTH] = {0};
    char keyboard_name[UINPUT_MAX_NAME_SIZE] = "Unknown";

    char *device = NULL, *match = NULL;
    while ((opt = getopt(argc, argv, "ud:m:t")) != -1) {
        switch (opt) {
            case 'u':
                isUmlaut = true;
                break;
            case 'd':
                device = optarg;
                break;
            case 'm':
                match = optarg;
                break;
            case 't':
                noToggle = true;
                break;
            case 'c':
                noCapsLockAsModifier = true;
                break;
            case '?':
                usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (device == NULL) {
        usage(argv[0]);
        fprintf(stderr, "\nerror: specify input device, e.g., found in /dev/input/by-id/...\n");
        return EXIT_FAILURE;
    }

    //the name and ids of the virtual keyboard, we need to define this now, as we need to ignore this to prevent
    //mapping the virtual keyboard
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.vendor = 0x1;
    usetup.id.product = 0x1;
    strcpy(usetup.name, "Virtual Dvorak Keyboard");

    //get first input
    fdi = open(device, O_RDONLY);
    if (fdi < 0) {
        fprintf(stderr, "Cannot open any of the devices [%s]: %s.\n", device, strerror(errno));
        return EXIT_FAILURE;
    }
    //
    name_ret = ioctl(fdi, EVIOCGNAME(sizeof(keyboard_name) - 1), keyboard_name);
    if (name_ret < 0) {
        fprintf(stderr, "Cannot get device name [%s]: %s.\n", keyboard_name, strerror(errno));
        return EXIT_FAILURE;
    }

    if(strcmp(keyboard_name, usetup.name) == 0) {
        fprintf(stdout, "Do not map the device we just created here: %s.\n", keyboard_name);
        return EXIT_SUCCESS;
    }

    // match names, reuse name_ret
    name_ret = -1;

    if(match != NULL) {
        char *token = strtok(match, " ");
        while (token != NULL) {
            if (strcasestr(keyboard_name, token) != NULL) {
                printf("found input: [%s]\n", keyboard_name);
                name_ret = 0;
                break;
            }
            token = strtok(NULL, " ");
        }
        if (name_ret < 0) {
            fprintf(stderr, "Not a matching device: [%s] does not match these words: [%s]\n", keyboard_name, match);
            return EXIT_FAILURE;
        }
    }

    fdo = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fdo < 0) {
        fprintf(stderr, "Cannot open /dev/uinput: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    //grab the key, from the input
    //https://unix.stackexchange.com/questions/126974/where-do-i-find-ioctl-eviocgrab-documented/126996

    //https://bugs.freedesktop.org/show_bug.cgi?id=101796
    //the bug in the above tracker was fixed, but I still run into this issue, so sleep a bit to not have stuck
    //keys when EVIOCGRAB is called
    //quick workaround, sleep for 200ms...
    usleep(200 * 1000);

    if (ioctl(fdi, EVIOCGRAB, 1) < 0) {
        fprintf(stderr, "Cannot grab key: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Keyboard
    if (ioctl(fdo, UI_SET_EVBIT, EV_KEY) < 0) {
        fprintf(stderr, "Cannot set ev key bits, key: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    if (ioctl(fdo, UI_SET_EVBIT, EV_SYN) < 0) {
        fprintf(stderr, "Cannot set ev syn bits, syn: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Register all keys - not working at the moment. Since ~Nov. 2020, I cannot use KEY_MAX anymore, as the kernel throws:
    // Similar? https://github.com/systemd/systemd/issues/15784
    // This line here: https://elixir.bootlin.com/linux/latest/source/lib/kobject_uevent.c#L670
    //
    //
    // kernel: ------------[ cut here ]------------
    // add_uevent_var: buffer size too small
    // WARNING: CPU: 0 PID: 23180 at lib/kobject_uevent.c:670 add_uevent_var+0x114/0x130
    // Modules linked in: uinput amdgpu iwlmvm snd_hda_codec_realtek mac80211 snd_hda_codec_generic snd_hda_codec_hdmi ledtrig_audio gpu_sched nls_iso8859_1 ttm nls_cp437 snd_hda_intel libarc4 vfat snd_intel_dspcfg amd64_edac_mod fat edac_mce_amd iwlwifi drm_kms_helper snd_hda_codec kvm_amd snd_hda_core cec kvm snd_hwdep joydev cfg80211 snd_pcm igb rc_core syscopyarea snd_timer input_leds i2c_algo_bit sp5100_tco sysfillrect mousedev irqbypass snd wmi_bmof pcspkr mxm_wmi sysimgblt rapl soundcore rfkill fb_sys_fops dca k10temp i2c_piix4 gpio_amdpt pinctrl_amd evdev mac_hid acpi_cpufreq vboxnetflt(OE) vboxnetadp(OE) vboxdrv(OE) drm msr sg crypto_user fuse agpgart ip_tables x_tables f2fs hid_logitech_hidpp hid_logitech_dj hid_lenovo hid_generic usbhid hid dm_crypt cbc encrypted_keys dm_mod trusted tpm crct10dif_pclmul crc32_pclmul crc32c_intel ghash_clmulni_intel aesni_intel crypto_simd cryptd glue_helper xhci_pci ccp xhci_pci_renesas rng_core xhci_hcd wmi
    // CPU: 0 PID: 23180 Comm: dvorak Tainted: G        W  OE     5.9.14-arch1-1 #1
    // Hardware name: To Be Filled By O.E.M. To Be Filled By O.E.M./X399M Taichi, BIOS P3.80 12/04/2019
    // RIP: 0010:add_uevent_var+0x114/0x130
    // Code: 48 83 c4 50 5b 41 5c 5d c3 48 c7 c7 00 f2 98 b0 e8 e3 16 4d 00 0f 0b b8 f4 ff ff ff eb d2 48 c7 c7 28 f2 98 b0 e8 ce 16 4d 00 <0f> 0b b8 f4 ff ff ff eb bd e8 5e 00 52 00 66 66 2e 0f 1f 84 00 00
    // RSP: 0018:ffffbbdba9ce3b78 EFLAGS: 00010286
    // RAX: 0000000000000000 RBX: ffff9ce5d755b000 RCX: 0000000000000000
    // RDX: 0000000000000001 RSI: ffffffffb0959b0f RDI: 00000000ffffffff
    // RBP: ffffbbdba9ce3bd8 R08: 0000000000000de5 R09: ffffbbdba9ce3a30
    // R10: 0000000000000000 R11: ffffbbdba9ce3a35 R12: 0000000000000009
    // R13: 0000000000000000 R14: ffff9cedd9f458a0 R15: 0000000000000000
    // FS:  00007fb3da2a8740(0000) GS:ffff9ce5df200000(0000) knlGS:0000000000000000
    // CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
    // CR2: 00000a80db624000 CR3: 000000083710a000 CR4: 00000000003506f0
    // Call Trace:
    //  ? dev_uevent+0xe5/0x300
    //  kobject_uevent_env+0x38e/0x6a0
    //  ? acpi_platform_notify+0x2c/0x1e0
    //  ? software_node_notify+0x13/0xf0
    //  device_del+0x2de/0x410
    //  input_unregister_device+0x41/0x60
    //  uinput_destroy_device+0xb6/0xc0 [uinput]
    //  uinput_release+0x15/0x30 [uinput]
    //  __fput+0x8e/0x230
    //  task_work_run+0x5c/0x90
    //  do_exit+0x36f/0xaa0
    //  do_group_exit+0x33/0xa0
    //  get_signal+0x148/0x900
    //  ? preempt_count_add+0x68/0xa0
    //  ? _raw_spin_unlock_irqrestore+0x20/0x40
    //  ? prepare_to_wait_event+0x68/0xf0
    //  arch_do_signal+0x3d/0x730
    //  exit_to_user_mode_prepare+0xdf/0x160
    //  syscall_exit_to_user_mode+0x2c/0x180
    //  entry_SYSCALL_64_after_hwframe+0x44/0xa9
    // RIP: 0033:0x7fb3da3daec2
    // Code: Unable to access opcode bytes at RIP 0x7fb3da3dae98.
    // RSP: 002b:00007ffe40d14ac8 EFLAGS: 00000246 ORIG_RAX: 0000000000000000
    // RAX: fffffffffffffe00 RBX: 000000000000002e RCX: 00007fb3da3daec2
    // RDX: 0000000000000018 RSI: 00007ffe40d14bb0 RDI: 0000000000000003
    // RBP: 00007ffe40d14cb0 R08: 0000000000000013 R09: 00007ffe40d149a7
    // R10: 0000000000000001 R11: 0000000000000246 R12: 00007ffe40d14b50
    // R13: 0000000000000000 R14: 0000000000000020 R15: 0000000000000000
    // ---[ end trace 33378a9e15d96d7c ]---
    for (i = 0; i < 0x23e; i++) {
        if (ioctl(fdo, UI_SET_KEYBIT, i) < 0) {
            fprintf(stderr, "Cannot set ev bits: %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    //use REL, MSC? https://gist.github.com/CyberShadow/ae30a8d9f86c170c2451c3dd7edb649c

    //Setup: https://www.kernel.org/doc/html/v5.10/input/uinput.html#libevdev
    if (ioctl(fdo, UI_DEV_SETUP, &usetup) < 0) {
        fprintf(stderr, "Cannot setup device: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(fdo, UI_DEV_CREATE) < 0) {
        fprintf(stderr, "Cannot create device: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    while (1) {
        n = read(fdi, &ev, sizeof ev);

        //check if we read the proper size
        if (n == (ssize_t) -1) {
            if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        } else if (n != sizeof ev) {
            break;
        }

        //when not disabled by the -t option, if l-alt is pressed 3 times, the dvorak mapping is disabled,
        //if it is again pressed 3 times, it will be enabled again
        if (!noToggle && ev.code == 56) {
            if (ev.value == 1 && ++lAlt >= 3) {
                isDvorak = !isDvorak;
                lAlt = 0;
                printf("mapping is set to [%s]\n", !isDvorak ? "true" : "false");
            }
        } else if (ev.type == EV_KEY) {
            lAlt = 0;
        }

        if (isDvorak) {
            if (emit(fdo, ev.type, ev.code, ev.value) < 0) {
                fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
            }
        } else if (ev.type == EV_KEY && (ev.value == 0 || ev.value == 1)) {
            //printf("%s 0x%04x (%d)\n", ev.value == 1 ? "pressed" : "released", (int) ev.code, (int) ev.code);

            //Umlaute mapping - since I want r-alt to produce umlauts without modifying
            if (ev.code == KEY_RIGHTALT) {
                alt_gr = ev.value == 1; //true if pressed
            }

            if (ev.code == KEY_RIGHTSHIFT) {
                rshift = ev.value == 1; //true if pressed
            }

            if (ev.code == KEY_LEFTSHIFT) {
                lshift = ev.value == 1; //true if pressed
            }

            mod_current = modifier_bit(ev.code);

            //Caps lock is optional (TODO: don't use 16 here, but define as variables in modifier_bit)
            if(noCapsLockAsModifier && mod_current == 16) {
                mod_current = 0;
            }

            if (mod_current > 0) {
                if (ev.value == 1) { //pressed
                    mod_state |= mod_current;
                } else {//released
                    mod_state &= ~mod_current;
                }
            }

            if (isUmlaut && (ev.code == KEY_6)) {
                //remap the ^
                if (ev.value == 1) {
                    if((lshift || rshift) && !alt_gr) {
                        emit(fdo, EV_KEY, KEY_RIGHTALT, 1);
                        emit(fdo, EV_SYN, SYN_REPORT, 0);
                    } else if((lAlt || rshift) && alt_gr) {
                        emit(fdo, EV_KEY, KEY_RIGHTALT, 0);
                        emit(fdo, EV_SYN, SYN_REPORT, 0);
                    }
                    emit(fdo, EV_KEY, ev.code, ev.value);
                } else {
                    if((lshift || rshift) && !alt_gr) {
                        emit(fdo, EV_KEY, KEY_RIGHTALT, 0);
                        emit(fdo, EV_SYN, SYN_REPORT, 0);
                    }
                    emit(fdo, EV_KEY, ev.code, ev.value);
                }
            } else if (isUmlaut && (ev.code == KEY_Q || ev.code == KEY_GRAVE)) {
                //The " and ¨, as well as ' and ´ should be swapped when using international dvorak layout
                //The same goes for `(deadkey) and `(non-deadkey)
                if (ev.value == 1) { //pressed
                    if (!alt_gr) {
                        emit(fdo, EV_KEY, KEY_RIGHTALT, 1);
                        emit(fdo, EV_SYN, SYN_REPORT, 0);
                    } else {
                        //if it was pressed, release it immediately, we can see a double release, but this is ok
                        emit(fdo, EV_KEY, KEY_RIGHTALT, 0);
                        emit(fdo, EV_SYN, SYN_REPORT, 0);
                    }
                    emit(fdo, EV_KEY, ev.code, ev.value);
                } else {
                    if (!alt_gr) {
                        emit(fdo, EV_KEY, KEY_RIGHTALT, 0);
                        emit(fdo, EV_SYN, SYN_REPORT, 0);
                    }
                    emit(fdo, EV_KEY, ev.code, ev.value);
                }
            } else if (isUmlaut && ev.code != umlaut2dvorak(ev.code) && (alt_gr || array_umlaut_counter > 0)) {
                code = ev.code;
                if (ev.value == 1) { //pressed
                    if (array_umlaut_counter == MAX_LENGTH) {
                        printf("warning, too many keys pressed: %d. %s 0x%04x (%d), arr:%d\n",
                               MAX_LENGTH, ev.value == 1 ? "pressed" : "released", (int) ev.code, (int) ev.code,
                               array_umlaut_counter);
                        //skip dvorak mapping
                    } else {
                        array_umlaut[array_umlaut_counter++] = ev.code + 1; //0 means not mapped
                        code = umlaut2dvorak(ev.code); // dvorak mapping
                    }
                } else {
                    for (i = 0; i < array_umlaut_counter; i++) {
                        if (array_umlaut[i] == ev.code + 1) {
                            //found it, map it!
                            array_umlaut[i] = 0;
                            code = umlaut2dvorak(ev.code); // dvorak mapping
                        }
                    }
                    //cleanup array counter
                    for (i = array_umlaut_counter - 1; i >= 0; i--) {
                        if (array_umlaut[i] == 0) {
                            array_umlaut_counter--;
                        } else {
                            break;
                        }
                    }
                }
                if (emit(fdo, ev.type, code, ev.value) < 0) {
                    fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
                }
            } else if (ev.code != qwerty2dvorak(ev.code) && (mod_state > 0 || array_qwerty_counter > 0)) {
                code = ev.code;
                //printf("dvorak %d, %d\n", array_counter, mod_state);
                if (ev.value == 1) { //pressed
                    if (array_qwerty_counter == MAX_LENGTH) {
                        printf("warning, too many keys pressed: %d. %s 0x%04x (%d), arr:%d\n",
                               MAX_LENGTH, ev.value == 1 ? "pressed" : "released", (int) ev.code, (int) ev.code,
                               array_qwerty_counter);
                        //skip dvorak mapping
                    } else {
                        array_qwerty[array_qwerty_counter++] = ev.code + 1; //0 means not mapped
                        code = qwerty2dvorak(ev.code); // dvorak mapping
                    }
                } else { //released
                    //now we need to check if the code is in the array
                    //if it is, then the pressed key was in dvorak mode and
                    //we need to remove it from the array. The ctrl or alt
                    //key does not need to be pressed, when a key is released.
                    //A previous implementation only had a counter, which resulted
                    //occasionally in stuck keys.
                    for (i = 0; i < array_qwerty_counter; i++) {
                        if (array_qwerty[i] == ev.code + 1) {
                            //found it, map it!
                            array_qwerty[i] = 0;
                            code = qwerty2dvorak(ev.code); // dvorak mapping
                        }
                    }
                    //cleanup array counter
                    for (i = array_qwerty_counter - 1; i >= 0; i--) {
                        if (array_qwerty[i] == 0) {
                            array_qwerty_counter--;
                        } else {
                            break;
                        }
                    }
                }

                if (emit(fdo, ev.type, code, ev.value) < 0) {
                    fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
                }
            } else {
                //printf("non dvorak %d\n", array_counter);
                if (emit(fdo, ev.type, ev.code, ev.value) < 0) {
                    fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
                }
            }
        } else {
            //printf("Not key: %d 0x%04x (%d)\n", ev.value, (int)ev.code, (int)ev.code);
            if (emit(fdo, ev.type, ev.code, ev.value) < 0) {
                fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
            }
        }
    }
    fflush(stdout);
    fprintf(stderr, "%s.\n", strerror(errno));
    return EXIT_FAILURE;
}
