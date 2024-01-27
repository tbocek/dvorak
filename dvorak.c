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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/uinput.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define MAX_LENGTH 16

static int emit(int fd, __u16 type, __u16 code, __s32 value) {
    struct input_event ev;

    memset(&ev, 0, sizeof(struct input_event));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    gettimeofday(&ev.time, NULL);

    return write(fd, &ev, sizeof(struct input_event));
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

static void usage(const char *path) {
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
    int fdi, fdo, i, mod_current, code, ret_val, mod_state = 0, array_qwerty_counter = 0, array_umlaut_counter = 0, lAlt = 0, opt;
    bool alt_gr = false, isDvorak = false, isUmlaut = false, lshift = false, rshift = false, noToggle = false, noCapsLockAsModifier = false;
    unsigned int array_qwerty[MAX_LENGTH] = {0}, array_umlaut[MAX_LENGTH] = {0}, array_bit[KEY_MAX/32 + 1]= {0};
    char keyboard_name[UINPUT_MAX_NAME_SIZE] = "Unknown";

    char *device = NULL, *match = NULL;
    while ((opt = getopt(argc, argv, "ud:m:tc")) != -1) {
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

    //get the physical available keys in a bitmap. The array_bit is 32 bit, so do bitbanging accordingly
    ret_val = ioctl(fdi, EVIOCGBIT(EV_KEY, sizeof(array_bit)), &array_bit);
    if (ret_val < 0) {
        fprintf(stderr, "Cannot EVIOCGBIT for device [%s]: %s.\n", device, strerror(errno));
        return EXIT_FAILURE;
    }

    //check if X, C, or V are present, if no, then its not a keyboard, exit
    if (!(array_bit[KEY_X / 32] & (1 << (KEY_X % 32))) ||
        !(array_bit[KEY_C / 32] & (1 << (KEY_C % 32))) ||
        !(array_bit[KEY_V / 32] & (1 << (KEY_V % 32)))) {
            fprintf(stdout, "Not a keyboard: [%s].\n", device);
            return EXIT_SUCCESS;
    }

    //
    ret_val = ioctl(fdi, EVIOCGNAME(sizeof(keyboard_name) - 1), keyboard_name);
    if (ret_val < 0) {
        fprintf(stderr, "Cannot get device name [%s]: %s.\n", keyboard_name, strerror(errno));
        return EXIT_FAILURE;
    }

    if(strcmp(keyboard_name, usetup.name) == 0) {
        fprintf(stdout, "Do not map the device we just created here: %s.\n", keyboard_name);
        return EXIT_SUCCESS;
    }

    // match names, reuse name_ret
    ret_val = -1;

    if(match != NULL) {
        char *token = strtok(match, " ");
        while (token != NULL) {
            if (strcasestr(keyboard_name, token) != NULL) {
                printf("found input: [%s] for device [%s]\n", keyboard_name, device);
                ret_val = 0;
                break;
            }
            token = strtok(NULL, " ");
        }
        if (ret_val < 0) {
            fprintf(stderr, "Not a matching device: [%s] does not match these words: [%s]\n", keyboard_name, match);
            return EXIT_FAILURE;
        }
    }

    fdo = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fdo < 0) {
        fprintf(stderr, "Cannot open /dev/uinput: %s.\n", strerror(errno));
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

    //add the capabilities for the virtual keyboard
    for (i = 0; i < KEY_MAX; i++) {
        //only register those that are on the keyboard available
        if(!(array_bit[i / 32] & (1 << (i % 32)))) {
            continue;
        }
        if (ioctl(fdo, UI_SET_KEYBIT, i) < 0) {
            fprintf(stderr, "Cannot set ev bits %d: %s.\n", i, strerror(errno));
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

    //grab the key, from the input
    //https://unix.stackexchange.com/questions/126974/where-do-i-find-ioctl-eviocgrab-documented/126996
    if (ioctl(fdi, EVIOCGRAB, 1) < 0) {
        fprintf(stderr, "Cannot grab key: %s.\n", strerror(errno));
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
        if (!noToggle && ev.code == KEY_LEFTALT) {
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

            if(noCapsLockAsModifier && mod_current == modifier_bit(KEY_CAPSLOCK)) {
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
                //printf("dvorak %d, %d\n", array_qwerty_counter, mod_state);
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

                //printf("Dvorak Key: %d 0x%04x => 0x%04x (%d, %d)\n", ev.value, (int)ev.code, code, (int)ev.code, code);
                if (emit(fdo, ev.type, code, ev.value) < 0) {
                    fprintf(stderr, "Cannot write to device: %s.\n", strerror(errno));
                }
            } else {
                //printf("Regular key: %d 0x%04x (%d)\n", ev.value, (int)ev.code, (int)ev.code);
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
