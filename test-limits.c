#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <string.h>
#include <stdio.h>

/*
 * This produces the following error:
 * kernel: ------------[ cut here ]------------
 * kernel: add_uevent_var: buffer size too small
 * kernel: WARNING: CPU: 38 PID: 29882 at lib/kobject_uevent.c:671 add_uevent_var+0x122/0x130
 * kernel: Modules linked in: uinput snd_hda_codec_realtek snd_hda_codec_generic intel_rapl_msr snd_hda_codec_hdmi ledtrig_audio iwlmvm intel_rapl_common snd_hda_intel snd_usb_audio uvcvideo amd64_edac btusb snd_intel_dspcfg edac_mce_amd snd_intel_sdw_acpi snd_usbmidi_lib videobuf2_vmalloc btrtl mac80211 snd_hda_codec snd_rawmidi btbcm videobuf2_memops libarc4 btintel snd_hda_core snd_seq_device kvm_amd iwlwifi videobuf2_v4l2 snd_hwdep amdgpu snd_pcm videobuf2_common nls_iso8859_1 bluetooth kvm cfg80211 gpu_sched snd_timer videodev vfat drm_ttm_helper sp5100_tco irqbypass snd ecdh_generic fat igb rapl mc joydev mousedev pcspkr wmi_bmof crc16 ttm mxm_wmi soundcore k10temp i2c_piix4 rfkill dca gpio_amdpt vboxnetflt(OE) vboxnetadp(OE) pinctrl_amd gpio_generic mac_hid acpi_cpufreq vboxdrv(OE) ipmi_devintf ipmi_msghandler sg crypto_user fuse bpf_preload ip_tables x_tables f2fs crc32_generic lz4hc_compress hid_logitech_hidpp hid_logitech_dj hid_lenovo usbhid dm_crypt cbc encrypted_keys
 * kernel:  dm_mod trusted asn1_encoder tee tpm crct10dif_pclmul crc32_pclmul crc32c_intel ghash_clmulni_intel aesni_intel crypto_simd cryptd ccp xhci_pci rng_core xhci_pci_renesas wmi
 * kernel: CPU: 38 PID: 29882 Comm: test Tainted: G        W  OE     5.15.2-arch1-1 #1 e3bfbeb633edc604ba956e06f24d5659e31c294f
 * kernel: Hardware name: To Be Filled By O.E.M. To Be Filled By O.E.M./X399M Taichi, BIOS P3.80 12/04/2019
 * kernel: RIP: 0010:add_uevent_var+0x122/0x130
 * kernel: Code: 89 d7 41 89 d0 41 89 d1 c3 48 c7 c7 88 6e 4b ad e8 84 ee 5c 00 0f 0b b8 f4 ff ff ff eb c4 48 c7 c7 b0 6e 4b ad e8 6f ee 5c 00 <0f> 0b b8 f4 ff ff ff eb af e8 f0 0e 62 00 55 48 8b af 20 01 00 00
 * kernel: RSP: 0018:ffffab858761fc28 EFLAGS: 00010246
 * kernel: RAX: 0000000000000000 RBX: ffff97f59d8b6000 RCX: 0000000000000000
 * kernel: RDX: 0000000000000000 RSI: 0000000000000000 RDI: 0000000000000000
 * kernel: RBP: ffffab858761fc88 R08: 0000000000000000 R09: 0000000000000000
 * kernel: R10: 0000000000000000 R11: 0000000000000000 R12: 000000000000000b
 * kernel: R13: 0000000000000000 R14: ffff97edc4119f00 R15: 0000000000000000
 * kernel: FS:  00007fe064ecd580(0000) GS:ffff97ed9f580000(0000) knlGS:0000000000000000
 * kernel: CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
 * kernel: CR2: 00007fe064dcbf10 CR3: 0000000b47810000 CR4: 00000000003506e0
 * kernel: Call Trace:
 * kernel:  ? dev_uevent+0xda/0x300
 * kernel:  kobject_uevent_env+0x38b/0x690
 * kernel:  device_del+0x2c8/0x420
 * kernel:  input_unregister_device+0x41/0x70
 * kernel:  uinput_destroy_device+0xba/0xc0 [uinput bd15ab9f0d37232918b9e359ddad02b02a0ea554]
 * kernel:  uinput_release+0x15/0x30 [uinput bd15ab9f0d37232918b9e359ddad02b02a0ea554]
 * kernel:  __fput+0x8c/0x240
 * kernel:  task_work_run+0x5c/0x90
 * kernel:  do_exit+0x35c/0xac0
 * kernel:  do_group_exit+0x33/0xa0
 * kernel:  __x64_sys_exit_group+0x14/0x20
 * kernel:  do_syscall_64+0x5c/0x90
 * kernel:  ? handle_mm_fault+0xd5/0x2c0
 * kernel:  ? do_user_addr_fault+0x20b/0x6b0
 * kernel:  ? exc_page_fault+0x72/0x180
 * kernel:  entry_SYSCALL_64_after_hwframe+0x44/0xae
 * kernel: RIP: 0033:0x7fe064dcbf41
 * kernel: Code: Unable to access opcode bytes at RIP 0x7fe064dcbf17.
 * kernel: RSP: 002b:00007ffcd35999c8 EFLAGS: 00000246 ORIG_RAX: 00000000000000e7
 * kernel: RAX: ffffffffffffffda RBX: 00007fe064ec2470 RCX: 00007fe064dcbf41
 * kernel: RDX: 000000000000003c RSI: 00000000000000e7 RDI: 0000000000000000
 * kernel: RBP: 0000000000000000 R08: ffffffffffffff88 R09: 0000000000000001
 * kernel: R10: 00007fe064d0ff78 R11: 0000000000000246 R12: 00007fe064ec2470
 * kernel: R13: 0000000000000001 R14: 00007fe064ec2948 R15: 0000000000000000
 * kernel: ---[ end trace 097e7ba2e646092b ]---
 */

int main(void)
{
    struct uinput_setup usetup;

    int fdo, i;

    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234; /* sample vendor */
    usetup.id.product = 0x5678; /* sample product */
    strcpy(usetup.name, "Example device");

    fdo = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fdo < 0) {
        fprintf(stderr, "Cannot open /dev/uinput: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(fdo, UI_SET_EVBIT, EV_KEY) < 0) {
        fprintf(stderr, "Cannot set ev key bits, key: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    if (ioctl(fdo, UI_SET_EVBIT, EV_SYN) < 0) {
        fprintf(stderr, "Cannot set ev syn bits, syn: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    //0x23e works
    for (i = 0; i < 0x23f; i++) {
        if (ioctl(fdo, UI_SET_KEYBIT, i) < 0) {
            fprintf(stderr, "Cannot set ev bits: %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    //Setup: https://www.kernel.org/doc/html/v5.10/input/uinput.html#libevdev
    if (ioctl(fdo, UI_DEV_SETUP, &usetup) < 0) {
        fprintf(stderr, "Cannot setup device: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (ioctl(fdo, UI_DEV_CREATE) < 0) {
        fprintf(stderr, "Cannot create device: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return 0;
}