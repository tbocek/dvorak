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
 * With X.org I was reling on the wonderful tool from Kenton Varda,
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
 * Intallation
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

static const char *const evval[3] = {
        "RELEASED",
        "PRESSED",
        "REPEATED"
};

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
        case 29:
            return 1;     // l-ctrl
        case 97:
            return 2;     // r-ctrl
        case 56:
            return 4;     // l-alt
        case 125:
            return 8;   // win
    }
    return 0;
}

//from: https://github.com/kentonv/dvorak-qwerty/tree/master/unix
static int qwerty2dvorak(int key) {
    switch (key) {
        case 12:
            return 40;
        case 13:
            return 27;
        case 16:
            return 45;
        case 17:
            return 51;
        case 18:
            return 32;
        case 19:
            return 24;
        case 20:
            return 37;
        case 21:
            return 20;
        case 22:
            return 33;
        case 23:
            return 34;
        case 24:
            return 31;
        case 25:
            return 19;
        case 26:
            return 12;
        case 27:
            return 13;
        case 30:
            return 30;
        case 31:
            return 39;
        case 32:
            return 35;
        case 33:
            return 21;
        case 34:
            return 22;
        case 35:
            return 36;
        case 36:
            return 46;
        case 37:
            return 47;
        case 38:
            return 25;
        case 39:
            return 44;
        case 40:
            return 16;
        case 44:
            return 53;
        case 45:
            return 48;
        case 46:
            return 23;
        case 47:
            return 52;
        case 48:
            return 49;
        case 49:
            return 38;
        case 50:
            return 50;
        case 51:
            return 17;
        case 52:
            return 18;
        case 53:
            return 26;
    }
    return key;
}

int main(int argc, char *argv[]) {

    setuid(0);

    if (argc < 2) {
        fprintf(stderr, "error: specify input device, e.g., found in "
                        "/dev/input/by-id/.\n");
        return EXIT_FAILURE;
    }

    struct input_event ev;
    ssize_t n;
    int fdi, fdo, i, mod_state, mod_current, array_counter, code, name_ret;
    struct uinput_setup usetup;
    const char MAX_LENGTH = 32;
    int array[MAX_LENGTH];
    char keyboard_name[UINPUT_MAX_NAME_SIZE] = "Unknown";
    int isDvorak=false;
    int lAlt=0;

    //the name and ids of the virtual keyboard, we need to define this now, as we need to ignore this to prevent
    //mapping the virtual keyboard
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.vendor = 0x1;
    usetup.id.product = 0x1;
    strcpy(usetup.name, "Virtual Dvorak Keyboard");

    //init states
    mod_state = 0;
    array_counter = 0;
    for (i = 0; i < MAX_LENGTH; i++) {
        array[i] = 0;
    }

    //get first input
    fdi = open(argv[1], O_RDONLY);
    if (fdi < 0) {
        fprintf(stderr, "Cannot open any of the devices [%s]: %s.\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }
    //
    name_ret = ioctl(fdi, EVIOCGNAME(sizeof(keyboard_name) - 1), keyboard_name);
    if (name_ret < 0) {
        fprintf(stderr, "Cannot get device name [%s]: %s.\n", keyboard_name, strerror(errno));
        return EXIT_FAILURE;
    }

    if (strcasestr(keyboard_name, usetup.name) != NULL) {
        fprintf(stderr, "Cannot get map the virtual device: %s.\n", keyboard_name);
        return EXIT_FAILURE;
    }

    // match names, reuse name_ret
    name_ret = -1;
    for (i = 2; i < argc; i++) {
        if (strcasestr(keyboard_name, argv[i]) != NULL) {
            printf("found input: [%s]\n", keyboard_name);
            name_ret = 0;
            break;
        }
    }
    if (name_ret < 0) {
        fprintf(stderr, "Not a matching device: [%s]\n", keyboard_name);
        return EXIT_FAILURE;
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
        if (n == (ssize_t) - 1) {
            if (errno == EINTR) {
                //printf( "error\n");
                continue;
            } else {
                //printf( "error2\n");
                break;
            }
        } else if (n != sizeof ev) {
            //printf( "error3\n");
            errno = EIO;
            break;
        }

        //if l-alt is pressed 3 times, the dvorak mapping is disabled, if it is
        //again pressed 3 times, it will be enabled again
        if (ev.code == 56) {
            if(ev.value == 1 && ++lAlt >= 3) {
                isDvorak = !isDvorak;
                lAlt = 0;
                printf("mapping is set to [%s]\n", !isDvorak ? "true" : "false");
            }
        } else if (ev.type == EV_KEY) {
            lAlt = 0;
        }

        if(isDvorak) {
            if(emit(fdo, ev.type, ev.code, ev.value) < 0) {
                fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
            }
        } else if (ev.type == EV_KEY && ev.value >= 0 && ev.value <= 2) {
            //printf("%s 0x%04x (%d), arr:%d\n", evval[ev.value], (int)ev.code, (int)ev.code, array_counter);

            mod_current = modifier_bit(ev.code);
            if (mod_current > 0) {
                if (ev.value == 1) { //pressed
                    mod_state |= mod_current;
                } else if (ev.value == 0) {//released
                    mod_state &= ~mod_current;
                }
            }

            if (ev.code != qwerty2dvorak(ev.code) && (mod_state > 0 || array_counter > 0)) {
                code = ev.code;
                //printf("dvorak %d, %d\n", array_counter, mod_state);
                if (ev.value == 1) { //pressed
                    if (array_counter == MAX_LENGTH) {
                        printf("warning, too many keys pressed: %d. %s 0x%04x (%d), arr:%d\n",
                               MAX_LENGTH, evval[ev.value], (int) ev.code, (int) ev.code, array_counter);
                        //skip dvorak mapping
                    } else {
                        array[array_counter] = ev.code + 1; //0 means not mapped
                        array_counter++;
                        code = qwerty2dvorak(ev.code); // dvorak mapping
                    }
                } else if (ev.value == 0) { //released
                    //now we need to check if the code is in the array
                    //if it is, then the pressed key was in dvorak mode and
                    //we need to remove it from the array. The ctrl or alt
                    //key does not need to be pressed, when a key is released.
                    //A previous implementation only had a counter, which resulted
                    //occasionally in stuck keys.
                    for (i = 0; i < array_counter; i++) {
                        if (array[i] == ev.code + 1) {
                            //found it, map it!
                            array[i] = 0;
                            code = qwerty2dvorak(ev.code); // dvorak mapping
                        }
                    }
                    //cleanup array counter
                    for (i = array_counter - 1; i >= 0; i--) {
                        if (array[i] == 0) {
                            array_counter--;
                        } else {
                            break;
                        }
                    }
                }
                if(emit(fdo, ev.type, code, ev.value) < 0) {
                    fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
                }
            } else {
                //printf("non dvorak %d\n", array_counter);
                if(emit(fdo, ev.type, ev.code, ev.value) < 0) {
                    fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
                }
            }
        } else {
            //printf("Not key: %d 0x%04x (%d)\n", ev.value, (int)ev.code, (int)ev.code);
            if(emit(fdo, ev.type, ev.code, ev.value) < 0) {
                fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
            }
        }
    }
    fflush(stdout);
    fprintf(stderr, "%s.\n", strerror(errno));
    return EXIT_FAILURE;
}
