#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <orbis/Usbd.h>

/* OpenOrbis currently forward-declares this libusb ABI type only. */
struct libusb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
    uint8_t bRefresh;
    uint8_t bSynchAddress;
    const unsigned char* extra;
    int32_t extra_length;
};

#define K4P_PUBLIC __attribute__((visibility("default")))
#define K4P_HIDDEN __attribute__((visibility("hidden")))

K4P_PUBLIC const char* g_pluginName = "Key4Play USB Probe";
K4P_PUBLIC const char* g_pluginDesc = "Enumerates USB HID interfaces; no input injection";
K4P_PUBLIC const char* g_pluginAuth = "TFG-Studios";
K4P_PUBLIC uint32_t g_pluginVersion = 0x00000100;

typedef struct K4pUsbdApi {
    int32_t (*init)(void);
    int32_t (*exit)(void);
    int32_t (*get_device_list)(libusb_device***);
    int32_t (*free_device_list)(libusb_device**);
    int32_t (*get_device_descriptor)(libusb_device*, struct libusb_device_descriptor*);
    int32_t (*get_active_config_descriptor)(libusb_device*, struct libusb_config_descriptor**);
    int32_t (*free_config_descriptor)(struct libusb_config_descriptor*);
} K4pUsbdApi;

static K4pUsbdApi g_usbd;
static int g_usbd_initialized = 0;
static int g_usbd_module = -1;
static int g_sysmodule_module = -1;

static int load_system_module(const char* filename) {
    const char* sandbox = sceKernelGetFsSandboxRandomWord();
    char path[256];
    int result = 0;
    if (!sandbox || !sandbox[0]) return -1;
    snprintf(path, sizeof(path), "/%s/common/lib/%s", sandbox, filename);
    return (int32_t)sceKernelLoadStartModule(path, 0, 0, 0, 0, &result);
}

static int resolve_symbol(int module, const char* name, void** out) {
    int result = sceKernelDlsym(module, name, out);
    if (result < 0 || !*out) {
        printf("[Key4Play] dlsym %s failed: %d\n", name, result);
        return result < 0 ? result : -1;
    }
    return 0;
}

static int load_usb_api(void) {
    int32_t (*sysmodule_is_loaded)(uint16_t) = 0;
    int32_t (*sysmodule_load)(uint16_t) = 0;
    int result;

    g_sysmodule_module = load_system_module("libSceSysmodule.sprx");
    if (g_sysmodule_module < 0) {
        printf("[Key4Play] failed to load libSceSysmodule: %d\n", g_sysmodule_module);
        return g_sysmodule_module;
    }
    if (resolve_symbol(g_sysmodule_module, "sceSysmoduleIsLoaded",
                       (void**)&sysmodule_is_loaded) < 0 ||
        resolve_symbol(g_sysmodule_module, "sceSysmoduleLoadModule",
                       (void**)&sysmodule_load) < 0) return -1;

    if (!sysmodule_is_loaded(0x00B7u)) {
        result = sysmodule_load(0x00B7u);
        if (result < 0) {
            printf("[Key4Play] sceSysmoduleLoadModule(USBD) failed: %d\n", result);
            return result;
        }
    }

    g_usbd_module = load_system_module("libSceUsbd.sprx");
    if (g_usbd_module < 0) {
        printf("[Key4Play] failed to load libSceUsbd: %d\n", g_usbd_module);
        return g_usbd_module;
    }

#define K4P_RESOLVE(field, symbol) \
    do { result = resolve_symbol(g_usbd_module, symbol, (void**)&g_usbd.field); \
         if (result < 0) return result; } while (0)
    K4P_RESOLVE(init, "sceUsbdInit");
    K4P_RESOLVE(exit, "sceUsbdExit");
    K4P_RESOLVE(get_device_list, "sceUsbdGetDeviceList");
    K4P_RESOLVE(free_device_list, "sceUsbdFreeDeviceList");
    K4P_RESOLVE(get_device_descriptor, "sceUsbdGetDeviceDescriptor");
    K4P_RESOLVE(get_active_config_descriptor, "sceUsbdGetActiveConfigDescriptor");
    K4P_RESOLVE(free_config_descriptor, "sceUsbdFreeConfigDescriptor");
#undef K4P_RESOLVE
    return 0;
}

static void inspect_device(libusb_device* device, unsigned index,
                           unsigned* keyboard_count, unsigned* mouse_count,
                           unsigned* other_hid_count) {
    struct libusb_device_descriptor descriptor;
    struct libusb_config_descriptor* config = 0;
    int result;
    unsigned i, j;

    result = g_usbd.get_device_descriptor(device, &descriptor);
    if (result < 0) {
        printf("[Key4Play] USB device %u: descriptor read failed (%d)\n", index, result);
        return;
    }
    printf("[Key4Play] USB %u VID=%04x PID=%04x class=%02x configs=%u\n",
           index, descriptor.idVendor, descriptor.idProduct,
           descriptor.bDeviceClass, descriptor.bNumConfigurations);

    result = g_usbd.get_active_config_descriptor(device, &config);
    if (result < 0 || !config) {
        printf("[Key4Play] USB %u: no active configuration (%d)\n", index, result);
        return;
    }

    for (i = 0; i < config->bNumInterfaces; ++i) {
        struct libusb_interface* interface;
        if (!config->interface) break;
        interface = &config->interface[i];
        if (!interface->altsetting) continue;
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
            if (alt->endpoint) {
                unsigned e;
                for (e = 0; e < alt->bNumEndpoints; ++e) {
                    const struct libusb_endpoint_descriptor* endpoint = &alt->endpoint[e];
                    if ((endpoint->bmAttributes & 0x03u) == 0x03u) {
                        printf("[Key4Play]     interrupt %s endpoint=0x%02x maxPacket=%u interval=%u\n",
                               (endpoint->bEndpointAddress & 0x80u) ? "IN" : "OUT",
                               endpoint->bEndpointAddress,
                               endpoint->wMaxPacketSize & 0x07ffu,
                               endpoint->bInterval);
                    }
                }
            }
        }
    }
    g_usbd.free_config_descriptor(config);
}

static int enumerate_usb_devices(void) {
    libusb_device** devices = 0;
    int count, i;
    unsigned keyboards = 0, mice = 0, other_hid = 0;
    count = g_usbd.get_device_list(&devices);
    if (count < 0 || !devices) {
        printf("[Key4Play] sceUsbdGetDeviceList failed: %d\n", count);
        return count < 0 ? count : -1;
    }
    for (i = 0; i < count; ++i) {
        if (devices[i]) inspect_device(devices[i], (unsigned)i,
                                       &keyboards, &mice, &other_hid);
    }
    g_usbd.free_device_list(devices);
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
    result = load_usb_api();
    if (result < 0) return result;
    result = g_usbd.init();
    if (result < 0) {
        printf("[Key4Play] sceUsbdInit failed: %d\n", result);
        return result;
    }
    g_usbd_initialized = 1;
    result = enumerate_usb_devices();
    if (result < 0) {
        g_usbd.exit();
        g_usbd_initialized = 0;
    }
    return result;
}

K4P_PUBLIC int plugin_unload(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;
    if (g_usbd_initialized) {
        g_usbd.exit();
        g_usbd_initialized = 0;
    }
    /* Keep sysmodules loaded: other system components may use them. */
    printf("[Key4Play] plugin_unload complete\n");
    return 0;
}

K4P_HIDDEN int module_start(int64_t argc, const void* args) {
    (void)argc; (void)args; return 0;
}
K4P_HIDDEN int module_stop(int64_t argc, const void* args) {
    (void)argc; (void)args; return 0;
}
