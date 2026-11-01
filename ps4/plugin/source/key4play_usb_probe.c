#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <orbis/Usbd.h>
#include "key4play/input_core.h"
#include "key4play/input_snapshot.h"

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

K4P_PUBLIC const char* g_pluginName = "Key4Play USB Reader";
K4P_PUBLIC const char* g_pluginDesc = "Reads boot HID keyboard/mouse reports; no game injection";
K4P_PUBLIC const char* g_pluginAuth = "TFG-Studios";
K4P_PUBLIC uint32_t g_pluginVersion = 0x00000200;

typedef struct K4pUsbdApi {
    int32_t (*init)(void);
    int32_t (*exit)(void);
    int32_t (*get_device_list)(libusb_device***);
    int32_t (*free_device_list)(libusb_device**);
    int32_t (*get_device_descriptor)(libusb_device*, struct libusb_device_descriptor*);
    int32_t (*get_active_config_descriptor)(libusb_device*, struct libusb_config_descriptor**);
    int32_t (*free_config_descriptor)(struct libusb_config_descriptor*);
    int32_t (*open)(libusb_device*, libusb_device_handle**);
    void (*close)(libusb_device_handle*);
    int32_t (*claim_interface)(libusb_device_handle*, int32_t);
    int32_t (*release_interface)(libusb_device_handle*, int32_t);
    int32_t (*control_transfer)(libusb_device_handle*, uint8_t, uint8_t, uint16_t,
                                uint16_t, unsigned char*, uint16_t, uint32_t);
    int32_t (*interrupt_transfer)(libusb_device_handle*, unsigned char, unsigned char*,
                                  int32_t, int32_t*, uint32_t);
} K4pUsbdApi;

typedef struct K4pDevice {
    libusb_device_handle* handle;
    int32_t interface_number;
    uint8_t endpoint_in;
    uint8_t packet_size;
    int claimed;
} K4pDevice;

static K4pUsbdApi g_usbd;
static int g_usbd_initialized = 0;
static int g_usbd_module = -1;
static int g_sysmodule_module = -1;
static volatile int g_reader_stop = 0;
static OrbisPthread g_reader_threads[2];
static int g_reader_started[2] = {0, 0};
static K4pInputSnapshot g_input_snapshot;

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
    K4P_RESOLVE(open, "sceUsbdOpen");
    K4P_RESOLVE(close, "sceUsbdClose");
    K4P_RESOLVE(claim_interface, "sceUsbdClaimInterface");
    K4P_RESOLVE(release_interface, "sceUsbdReleaseInterface");
    K4P_RESOLVE(control_transfer, "sceUsbdControlTransfer");
    K4P_RESOLVE(interrupt_transfer, "sceUsbdInterruptTransfer");
#undef K4P_RESOLVE
    return 0;
}

static void close_device(K4pDevice* device) {
    if (!device || !device->handle) return;
    if (device->claimed)
        g_usbd.release_interface(device->handle, device->interface_number);
    g_usbd.close(device->handle);
    memset(device, 0, sizeof(*device));
}

static int find_boot_device(K4pDevice* out, int mouse) {
    libusb_device** devices = 0;
    int count, i, found = 0;
    const uint8_t wanted_protocol = mouse ? 0x02 : 0x01;

    count = g_usbd.get_device_list(&devices);
    if (count < 0 || !devices) {
        printf("[Key4Play] USB list failed: %d\n", count);
        return 0;
    }

    for (i = 0; i < count && !found; ++i) {
        struct libusb_device_descriptor descriptor;
        struct libusb_config_descriptor* config = 0;
        unsigned int interface_index;

        if (g_usbd.get_device_descriptor(devices[i], &descriptor) < 0) continue;
        if (g_usbd.get_active_config_descriptor(devices[i], &config) < 0 || !config)
            continue;
        if (!config->interface) {
            g_usbd.free_config_descriptor(config);
            continue;
        }

        for (interface_index = 0;
             interface_index < config->bNumInterfaces && !found;
             ++interface_index) {
            struct libusb_interface* interface = &config->interface[interface_index];
            int alt_index;
            if (!interface->altsetting) continue;

            for (alt_index = 0; alt_index < interface->num_altsetting && !found; ++alt_index) {
                struct libusb_interface_descriptor* alt = &interface->altsetting[alt_index];
                unsigned int endpoint_index;
                uint8_t endpoint_in = 0;
                uint8_t packet_size = 0;
                libusb_device_handle* handle = 0;
                int result;

                if (alt->bInterfaceClass != 0x03 ||
                    alt->bInterfaceSubClass != 0x01 ||
                    alt->bInterfaceProtocol != wanted_protocol ||
                    alt->bAlternateSetting != 0 || !alt->endpoint) continue;

                for (endpoint_index = 0; endpoint_index < alt->bNumEndpoints; ++endpoint_index) {
                    const struct libusb_endpoint_descriptor* endpoint = &alt->endpoint[endpoint_index];
                    uint16_t max_packet = endpoint->wMaxPacketSize & 0x07ffu;
                    if ((endpoint->bmAttributes & 0x03u) != 0x03u) continue;
                    if (!(endpoint->bEndpointAddress & 0x80u)) continue;
                    if (!max_packet || max_packet > 64) continue;
                    endpoint_in = endpoint->bEndpointAddress;
                    packet_size = (uint8_t)max_packet;
                    break;
                }
                if (!endpoint_in) continue;

                result = g_usbd.open(devices[i], &handle);
                if (result < 0 || !handle) continue;
                result = g_usbd.claim_interface(handle, alt->bInterfaceNumber);
                if (result < 0) {
                    g_usbd.close(handle);
                    continue;
                }

                /* HID SET_PROTOCOL: select boot protocol (wValue = 0). */
                result = g_usbd.control_transfer(handle, 0x21, 0x0b, 0,
                    alt->bInterfaceNumber, 0, 0, 500);
                if (result < 0) {
                    g_usbd.release_interface(handle, alt->bInterfaceNumber);
                    g_usbd.close(handle);
                    continue;
                }

                /* SET_IDLE is advisory; some devices do not implement it. */
                g_usbd.control_transfer(handle, 0x21, 0x0a, 0x0400,
                    alt->bInterfaceNumber, 0, 0, 500);

                out->handle = handle;
                out->interface_number = alt->bInterfaceNumber;
                out->endpoint_in = endpoint_in;
                out->packet_size = packet_size;
                out->claimed = 1;
                found = 1;
                printf("[Key4Play] %s attached VID=%04x PID=%04x interface=%u IN=0x%02x packet=%u\n",
                    mouse ? "Mouse" : "Keyboard", descriptor.idVendor,
                    descriptor.idProduct, alt->bInterfaceNumber,
                    endpoint_in, packet_size);
            }
        }
        g_usbd.free_config_descriptor(config);
    }

    g_usbd.free_device_list(devices);
    return found;
}

static int is_usb_timeout(int result) {
    return result == -7 || (uint32_t)result == 0x80240007u;
}

static int same_pad(const K4pPadState* a, const K4pPadState* b) {
    return a->buttons == b->buttons && a->lx == b->lx && a->ly == b->ly &&
           a->rx == b->rx && a->ry == b->ry && a->l2 == b->l2 &&
           a->r2 == b->r2 && a->axis_override == b->axis_override;
}

static void* input_reader(void* argument) {
    const int mouse = (int)(uintptr_t)argument;
    K4pDevice device;
    K4pPadState previous;
    K4pSettings settings;
    int published_connected = 0;
    uint8_t packet[64];
    memset(&device, 0, sizeof(device));
    k4p_pad_state_neutral(&previous);
    k4p_settings_default(&settings);

    while (!__atomic_load_n(&g_reader_stop, __ATOMIC_ACQUIRE)) {
        int transferred = 0;
        int result;

        if (!device.handle) {
            if (!find_boot_device(&device, mouse)) {
                if (published_connected) {
                    if (mouse) k4p_input_snapshot_publish_mouse(&g_input_snapshot, 0, 0);
                    else k4p_input_snapshot_publish_keyboard(&g_input_snapshot, 0, 0);
                    published_connected = 0;
                    k4p_pad_state_neutral(&previous);
                }
                sceKernelUsleep(500000);
                continue;
            }
            published_connected = 1;
        }

        memset(packet, 0, sizeof(packet));
        result = g_usbd.interrupt_transfer(device.handle, device.endpoint_in,
            packet, device.packet_size, &transferred, 100);

        if (result == 0 && transferred > 0 && transferred <= (int)sizeof(packet)) {
            K4pPadState current;
            int valid = 0;
            if (mouse) {
                K4pMouseReport report;
                valid = k4p_decode_boot_mouse(packet, (size_t)transferred, &report);
                if (valid) k4p_map_mouse(&report, &settings, &current);
            } else {
                K4pKeyboardReport report;
                valid = k4p_decode_boot_keyboard(packet, (size_t)transferred, &report);
                if (valid) k4p_map_keyboard(&report, &current);
            }

            if (valid) {
                if (mouse) k4p_input_snapshot_publish_mouse(&g_input_snapshot, &current, 1);
                else k4p_input_snapshot_publish_keyboard(&g_input_snapshot, &current, 1);
            } else {
                k4p_pad_state_neutral(&current);
                if (mouse) k4p_input_snapshot_publish_mouse(&g_input_snapshot, &current, 1);
                else k4p_input_snapshot_publish_keyboard(&g_input_snapshot, &current, 1);
            }

            if (valid && !same_pad(&current, &previous)) {
                printf("[Key4Play] %s state buttons=%08x LX=%u LY=%u RX=%u RY=%u L2=%u R2=%u\n",
                    mouse ? "Mouse" : "Keyboard", current.buttons,
                    current.lx, current.ly, current.rx, current.ry,
                    current.l2, current.r2);
                previous = current;
            } else if (!valid && !mouse) {
                k4p_pad_state_neutral(&previous);
            }
        } else if (is_usb_timeout(result)) {
            continue;
        } else if (result < 0) {
            printf("[Key4Play] %s USB read failed: %d; reconnecting\n",
                   mouse ? "Mouse" : "Keyboard", result);
            close_device(&device);
            sceKernelUsleep(250000);
        }
    }

    close_device(&device);
    if (mouse) k4p_input_snapshot_publish_mouse(&g_input_snapshot, 0, 0);
    else k4p_input_snapshot_publish_keyboard(&g_input_snapshot, 0, 0);
    return 0;
}

K4P_PUBLIC int plugin_load(int argc, const char* argv[]) {
    int result;
    (void)argc;
    (void)argv;

    if (g_usbd_initialized) return 0;
    printf("[Key4Play] Starting USB HID reader (no game injection)\n");
    result = load_usb_api();
    if (result < 0) return result;

    result = g_usbd.init();
    if (result < 0) {
        printf("[Key4Play] sceUsbdInit failed: %d\n", result);
        return result;
    }
    g_usbd_initialized = 1;
    k4p_input_snapshot_init(&g_input_snapshot);
    __atomic_store_n(&g_reader_stop, 0, __ATOMIC_RELEASE);

    result = scePthreadCreate(&g_reader_threads[0], 0, input_reader, 0, "k4p-keyboard");
    if (result != 0) {
        printf("[Key4Play] keyboard reader thread failed: %d\n", result);
        g_usbd.exit();
        g_usbd_initialized = 0;
        return result;
    }
    g_reader_started[0] = 1;

    result = scePthreadCreate(&g_reader_threads[1], 0, input_reader,
                              (void*)(uintptr_t)1, "k4p-mouse");
    if (result != 0) {
        printf("[Key4Play] mouse reader thread failed: %d\n", result);
        __atomic_store_n(&g_reader_stop, 1, __ATOMIC_RELEASE);
        scePthreadJoin(g_reader_threads[0], 0);
        g_reader_started[0] = 0;
        g_usbd.exit();
        g_usbd_initialized = 0;
        return result;
    }
    g_reader_started[1] = 1;

    printf("[Key4Play] USB readers started. Mapped state is diagnostic only.\n");
    return 0;
}

K4P_PUBLIC int plugin_unload(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;
    __atomic_store_n(&g_reader_stop, 1, __ATOMIC_RELEASE);

    if (g_reader_started[0]) {
        scePthreadJoin(g_reader_threads[0], 0);
        g_reader_started[0] = 0;
    }
    if (g_reader_started[1]) {
        scePthreadJoin(g_reader_threads[1], 0);
        g_reader_started[1] = 0;
    }
    if (g_usbd_initialized) {
        g_usbd.exit();
        g_usbd_initialized = 0;
    }
    printf("[Key4Play] USB reader stopped; no controller hooks were installed.\n");
    return 0;
}

K4P_HIDDEN int module_start(int64_t argc, const void* args) {
    (void)argc; (void)args; return 0;
}
K4P_HIDDEN int module_stop(int64_t argc, const void* args) {
    (void)argc; (void)args; return 0;
}
