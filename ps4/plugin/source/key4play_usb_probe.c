#include <stdint.h>
#include <stdio.h>
#include <orbis/Sysmodule.h>
#include <orbis/Usbd.h>

#define K4P_PUBLIC __attribute__((visibility("default")))
#define K4P_HIDDEN __attribute__((visibility("hidden")))

K4P_PUBLIC const char* g_pluginName = "Key4Play USB Probe";
K4P_PUBLIC const char* g_pluginDesc = "Enumerates USB HID interfaces; no input injection";
K4P_PUBLIC const char* g_pluginAuth = "TFG-Studios";
K4P_PUBLIC uint32_t g_pluginVersion = 0x00000100;

static int g_usbd_initialized = 0;

static void inspect_device(libusb_device* device, unsigned index,
                           unsigned* keyboard_count, unsigned* mouse_count,
                           unsigned* other_hid_count) {
    struct libusb_device_descriptor descriptor;
    struct libusb_config_descriptor* config = 0;
    int result;
    unsigned i, j;

    result = sceUsbdGetDeviceDescriptor(device, &descriptor);
    if (result < 0) {
        printf("[Key4Play] USB device %u: descriptor read failed (%d)\n", index, result);
        return;
    }

    printf("[Key4Play] USB %u VID=%04x PID=%04x class=%02x configs=%u\n",
           index, descriptor.idVendor, descriptor.idProduct,
           descriptor.bDeviceClass, descriptor.bNumConfigurations);

    result = sceUsbdGetActiveConfigDescriptor(device, &config);
    if (result < 0 || !config) {
        printf("[Key4Play] USB %u: no active configuration (%d)\n", index, result);
        return;
    }

    for (i = 0; i < config->bNumInterfaces; ++i) {
        struct libusb_interface* interface = &config->interface[i];
        for (j = 0; j < (unsigned)interface->num_altsetting; ++j) {
            struct libusb_interface_descriptor* alt = &interface->altsetting[j];
            if (alt->bInterfaceClass != 0x03) continue;

            if (alt->bInterfaceSubClass == 0x01 && alt->bInterfaceProtocol == 0x01) {
                ++(*keyboard_count);
                printf("[Key4Play]   HID boot keyboard interface=%u alt=%u\n",
                       alt->bInterfaceNumber, alt->bAlternateSetting);
            } else if (alt->bInterfaceSubClass == 0x01 && alt->bInterfaceProtocol == 0x02) {
                ++(*mouse_count);
                printf("[Key4Play]   HID boot mouse interface=%u alt=%u\n",
                       alt->bInterfaceNumber, alt->bAlternateSetting);
            } else {
                ++(*other_hid_count);
                printf("[Key4Play]   other HID interface=%u subclass=%u protocol=%u\n",
                       alt->bInterfaceNumber, alt->bInterfaceSubClass,
                       alt->bInterfaceProtocol);
            }
        }
    }
    sceUsbdFreeConfigDescriptor(config);
}

static int enumerate_usb_devices(void) {
    libusb_device** devices = 0;
    int count, i;
    unsigned keyboards = 0, mice = 0, other_hid = 0;

    count = sceUsbdGetDeviceList(&devices);
    if (count < 0 || !devices) {
        printf("[Key4Play] sceUsbdGetDeviceList failed: %d\n", count);
        return count < 0 ? count : -1;
    }

    for (i = 0; i < count; ++i) {
        if (devices[i]) inspect_device(devices[i], (unsigned)i,
                                       &keyboards, &mice, &other_hid);
    }
    sceUsbdFreeDeviceList(devices);
    printf("[Key4Play] USB scan complete: boot keyboards=%u boot mice=%u other HID=%u\n",
           keyboards, mice, other_hid);
    return 0;
}

/* Discovery only: no device open/claim, report transfers, controller hooks or game memory writes. */
K4P_PUBLIC int plugin_load(int argc, const char* argv[]) {
    int result;
    (void)argc;
    (void)argv;
    printf("[Key4Play] plugin_load: USB discovery-only milestone\n");

    if (!sceSysmoduleIsLoaded(ORBIS_SYSMODULE_USBD)) {
        result = sceSysmoduleLoadModule(ORBIS_SYSMODULE_USBD);
        if (result < 0) {
            printf("[Key4Play] failed to load libSceUsbd: %d\n", result);
            return result;
        }
    }

    result = sceUsbdInit();
    if (result < 0) {
        printf("[Key4Play] sceUsbdInit failed: %d\n", result);
        return result;
    }
    g_usbd_initialized = 1;
    return enumerate_usb_devices();
}

K4P_PUBLIC int plugin_unload(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;
    if (g_usbd_initialized) {
        sceUsbdExit();
        g_usbd_initialized = 0;
    }
    /* Leave the system module loaded; other system components may use it. */
    printf("[Key4Play] plugin_unload complete\n");
    return 0;
}

K4P_HIDDEN int module_start(int64_t argc, const void* args) {
    (void)argc; (void)args; return 0;
}
K4P_HIDDEN int module_stop(int64_t argc, const void* args) {
    (void)argc; (void)args; return 0;
}
