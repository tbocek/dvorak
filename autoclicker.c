#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

// Thread control variables
pthread_t thread_id = 0;
bool thread_active = false;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t emit_mutex = PTHREAD_MUTEX_INITIALIZER;

static int emit(int fd, __u16 type, __u16 code, __s32 value) {
    struct input_event ev;
    
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    gettimeofday(&ev.time, NULL);
    
    // Lock the mutex
    pthread_mutex_lock(&emit_mutex);
    int result = write(fd, &ev, sizeof(struct input_event));
    // Unlock the mutex
    pthread_mutex_unlock(&emit_mutex);
    return result;
}

static void usage(const char *path) {
    /* take only the last portion of the path */
    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    fprintf(stderr, "usage: %s [OPTION]\n", basename);
    fprintf(stderr, "  -d /dev/input/by-id/…\t"
                    "Specifies which device should be captured.\n");
    fprintf(stderr, "example: %s -d /dev/input/by-id/usb-Logitech_USB_Receiver-if02-event-mouse\n", basename);
}

void* click(void *arg) {
    int fd = *(int *)arg;

    while (1) {
        pthread_mutex_lock(&lock);
        if (!thread_active) {
            pthread_mutex_unlock(&lock);
            break; // Exit the loop if thread_active is false
        }
        pthread_mutex_unlock(&lock);

        // Emit the mouse press event (left button)
        emit(fd, EV_KEY, BTN_LEFT, 1); // Button press
        emit(fd, EV_SYN, SYN_REPORT, 0); // Sync report

        usleep(100 * 1000); // Wait 100 milliseconds

        // Emit the mouse release event (left button)
        emit(fd, EV_KEY, BTN_LEFT, 0); // Button release
        emit(fd, EV_SYN, SYN_REPORT, 0); // Sync report

        usleep(100 * 1000);
    }
    return NULL;
}

void startClickThread(int *fd) {
    pthread_mutex_lock(&lock);
    if (thread_active) {
        pthread_mutex_unlock(&lock);
        return;
    }
    thread_active = true;
    pthread_mutex_unlock(&lock);

    pthread_create(&thread_id, NULL, click, fd);
}

void stopClickThread() {
    pthread_mutex_lock(&lock);
    if (!thread_active) {
        pthread_mutex_unlock(&lock);
        return;
    }
    thread_active = false;
    pthread_mutex_unlock(&lock);

    pthread_join(thread_id, NULL);
}

int main(int argc, char *argv[]) {
    struct uinput_setup usetup;
    int fdi, fdo, n, opt;
    struct input_event ev;
    struct timespec currentTime;
    long long milliseconds_start = 0;

    currentTime.tv_sec = 0;
    currentTime.tv_nsec = 0;

    char *device = NULL;
    while ((opt = getopt(argc, argv, "d:")) != -1) {
        switch (opt) {
            case 'd':
                device = optarg;
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
    
    memset(&usetup, 0, sizeof(usetup));
    strcpy(usetup.name, "Virtual Mouse");
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.version = 1;
    usetup.id.vendor = 0x1;
    usetup.id.product = 0x1;
    
    fdi = open(device, O_RDONLY);
    if (fdi < 0) {
        fprintf(stderr, "Cannot open any of the devices [%s]: %s.\n", device, strerror(errno));
        return EXIT_FAILURE;
    }
    
    fdo = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fdo < 0) {
        perror("Cannot open /dev/uinput");
        return EXIT_FAILURE;
    }


    //setup this type of mouse:
    /*
Input device name: "Logitech M705"
Supported events:
  Event type 0 (EV_SYN)
  Event type 1 (EV_KEY)
    Event code 272 (BTN_LEFT)
    Event code 273 (BTN_RIGHT)
    Event code 274 (BTN_MIDDLE)
    Event code 275 (BTN_SIDE)
    Event code 276 (BTN_EXTRA)
    Event code 277 (BTN_FORWARD)
    Event code 278 (BTN_BACK)
    Event code 279 (BTN_TASK)
    Event code 280 (?)
    Event code 281 (?)
    Event code 282 (?)
    Event code 283 (?)
    Event code 284 (?)
    Event code 285 (?)
    Event code 286 (?)
    Event code 287 (?)
  Event type 2 (EV_REL)
    Event code 0 (REL_X)
    Event code 1 (REL_Y)
    Event code 6 (REL_HWHEEL)
    Event code 8 (REL_WHEEL)
    Event code 11 (REL_WHEEL_HI_RES)
    Event code 12 (REL_HWHEEL_HI_RES)
  Event type 4 (EV_MSC)
    Event code 4 (MSC_SCAN)
    */

    if (ioctl(fdo, UI_SET_EVBIT, EV_SYN) < 0) {
        fprintf(stderr, "Cannot set EV_SYN: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(fdo, UI_SET_EVBIT, EV_KEY) < 0) {
        fprintf(stderr, "Cannot set EV_KEY: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int specific_keys[] = { BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA, BTN_FORWARD, BTN_BACK, BTN_TASK };
    int num_keys = sizeof(specific_keys) / sizeof(specific_keys[0]);

    for (int i = 0; i < num_keys; i++) {
        if (ioctl(fdo, UI_SET_KEYBIT, specific_keys[i]) < 0) {
            fprintf(stderr, "Cannot set ev key bits %d: %s.\n", i, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    if (ioctl(fdo, UI_SET_EVBIT, EV_REL) < 0) {
        fprintf(stderr, "Cannot set EV_REL: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    int specific_rels[] = { REL_X, REL_Y, REL_HWHEEL, REL_WHEEL, REL_WHEEL_HI_RES, REL_HWHEEL_HI_RES };
    int num_rels = sizeof(specific_rels) / sizeof(specific_rels[0]);

    for (int i = 0; i < num_rels; i++) {
        if (ioctl(fdo, UI_SET_RELBIT, specific_rels[i]) < 0) {
            fprintf(stderr, "Cannot set ev rel bits %d: %s.\n", i, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    if (ioctl(fdo, UI_SET_EVBIT, EV_MSC) < 0) {
        fprintf(stderr, "Cannot set EV_MSC: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(fdo, UI_SET_MSCBIT, MSC_SCAN) < 0) {
        fprintf(stderr, "Cannot set MSC_SCAN: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    
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
                perror("Error reading");
                break;
            }
        } else if (n != sizeof ev) {
            fprintf(stderr, "Incomplete read.");
            break;
        }

        if (ev.type == EV_KEY && ev.code == BTN_LEFT) {
            clock_gettime(CLOCK_REALTIME, &currentTime);
            if (ev.value == 1) {         
                milliseconds_start = (long long)(currentTime.tv_sec) * 1000 + (long long)(currentTime.tv_nsec) / 1000000;
                stopClickThread();
            } else if (ev.value == 0) {
                long long milliseconds_stop = (long long)(currentTime.tv_sec) * 1000 + (long long)(currentTime.tv_nsec) / 1000000;
                if(milliseconds_stop - milliseconds_start > 3000) {
                    startClickThread(&fdo);
                }
            }
        }
        
        //printf("Regular key: %d 0x%04x (%d)\n", ev.value, (int)ev.code, (int)ev.type);
        emit(fdo, ev.type, ev.code, ev.value);
    }
    
    perror("exit");
    return EXIT_FAILURE;
}
