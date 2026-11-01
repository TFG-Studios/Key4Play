#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <orbis/libkernel.h>
#include <orbis/Usbd.h>
#include "key4play/input_core.h"
#include "key4play/hid_report.h"
#include "key4play/input_snapshot.h"
#include "key4play/pad_hook.h"

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
    uint16_t packet_size;
    uint8_t generic_report;
    int claimed;
    K4pHidLayout layout;
} K4pDevice;

static K4pUsbdApi g_usbd;
static int g_usbd_initialized = 0;
static int g_usbd_module = -1;
static int g_sysmodule_module = -1;
static volatile int g_reader_stop = 0;
static OrbisPthread g_reader_threads[2];
static int g_reader_started[2] = {0, 0};
K4pInputSnapshot g_k4p_input_snapshot;

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

static int get_report_descriptor_length(const unsigned char* extra,
                                         int32_t extra_length,
                                         uint16_t* report_length) {
    int32_t offset = 0;
    if (!extra || extra_length <= 0 || !report_length) return 0;

    while (offset + 2 <= extra_length) {
        uint8_t descriptor_length = extra[offset];
        uint8_t descriptor_type = extra[offset + 1];
        int32_t i;

        if (descriptor_length < 2u || offset + descriptor_length > extra_length)
            return 0;

        if (descriptor_type == 0x21u) { /* HID class descriptor. */
            uint8_t descriptor_count;
            if (descriptor_length < 6u) return 0;
            descriptor_count = extra[offset + 5];
            if ((uint32_t)6u + (uint32_t)descriptor_count * 3u >
                descriptor_length) return 0;

            for (i = 0; i < descriptor_count; ++i) {
                int32_t entry = offset + 6 + i * 3;
                if (extra[entry] == 0x22u) { /* Report descriptor. */
                    uint16_t size = (uint16_t)extra[entry + 1] |
                                    ((uint16_t)extra[entry + 2] << 8);
                    if (size == 0u || size > 4096u) return 0;
                    *report_length = size;
                    return 1;
                }
            }
        }
        offset += descriptor_length;
    }
    return 0;
}

static int layout_has_collection(const K4pHidLayout* layout, int mouse) {
    unsigned i;
    uint8_t wanted = mouse ? K4P_HID_COLLECTION_MOUSE
                           : K4P_HID_COLLECTION_KEYBOARD;
    if (!layout) return 0;
    for (i = 0; i < layout->field_count; ++i) {
        if (layout->fields[i].collection == wanted) return 1;
    }
    return 0;
}

static int layout_has_report(const K4pHidLayout* layout, uint8_t report_id,
                             int mouse) {
    unsigned i;
    uint8_t wanted = mouse ? K4P_HID_COLLECTION_MOUSE
                           : K4P_HID_COLLECTION_KEYBOARD;
    if (!layout) return 0;
    for (i = 0; i < layout->field_count; ++i) {
        if (layout->fields[i].collection == wanted &&
            layout->fields[i].report_id == report_id) return 1;
    }
    return 0;
}

static int fetch_hid_layout(libusb_device_handle* handle,
                            const struct libusb_interface_descriptor* alt,
                            uint16_t packet_size, int mouse,
                            K4pHidLayout* layout) {
    unsigned char report_descriptor[4096];
    uint16_t report_length = 0;
    int transferred;
    unsigned id;
    int matching_report = 0;

    if (!handle || !alt || !layout ||
        !get_report_descriptor_length(alt->extra, alt->extra_length,
                                       &report_length)) return 0;

    transferred = g_usbd.control_transfer(handle, 0x81u, 0x06u, 0x2200u,
        alt->bInterfaceNumber, report_descriptor, report_length, 1000u);
    if (transferred <= 0 || transferred > (int)sizeof(report_descriptor))
        return 0;
    if (!k4p_hid_parse_report_descriptor(report_descriptor,
                                         (size_t)transferred, layout) ||
        !layout_has_collection(layout, mouse)) return 0;

    for (id = 0; id < 256u; ++id) {
        if (layout->report_bytes[id] != 0u &&
            layout_has_report(layout, (uint8_t)id, mouse)) {
            if (layout->report_bytes[id] > packet_size) return 0;
            matching_report = 1;
        }
    }
    return matching_report;
}

static int find_hid_device(K4pDevice* out, int mouse) {
    libusb_device** devices = 0;
    int count, i, found = 0;
    const uint8_t wanted_protocol = mouse ? 0x02u : 0x01u;

    if (!out) return 0;
    count = g_usbd.get_device_list(&devices);
    if (count < 0 || !devices) {
        printf("[Key4Play] USB list failed: %d\\n", count);
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

            for (alt_index = 0;
                 alt_index < interface->num_altsetting && !found;
                 ++alt_index) {
                struct libusb_interface_descriptor* alt = &interface->altsetting[alt_index];
                unsigned int endpoint_index;
                uint8_t endpoint_in = 0;
                uint16_t packet_size = 0;
                libusb_device_handle* handle = 0;
                K4pHidLayout layout;
                int generic = 0;
                int result;

                if (alt->bInterfaceClass != 0x03u ||
                    alt->bAlternateSetting != 0 || !alt->endpoint) continue;

                for (endpoint_index = 0;
                     endpoint_index < alt->bNumEndpoints; ++endpoint_index) {
                    const struct libusb_endpoint_descriptor* endpoint =
                        &alt->endpoint[endpoint_index];
                    uint16_t max_packet = endpoint->wMaxPacketSize & 0x07ffu;
                    if ((endpoint->bmAttributes & 0x03u) != 0x03u) continue;
                    if (!(endpoint->bEndpointAddress & 0x80u)) continue;
                    if (!max_packet || max_packet > K4P_HID_MAX_REPORT_BYTES) continue;
                    endpoint_in = endpoint->bEndpointAddress;
                    packet_size = max_packet;
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

                /* Prefer descriptor-defined reports; fall back to boot mode. */
                generic = fetch_hid_layout(handle, alt, packet_size,
                                           mouse, &layout);
                if (!generic) {
                    if (alt->bInterfaceSubClass != 0x01u ||
                        alt->bInterfaceProtocol != wanted_protocol) {
                        g_usbd.release_interface(handle, alt->bInterfaceNumber);
                        g_usbd.close(handle);
                        continue;
                    }

                    /* HID SET_PROTOCOL: select boot protocol (wValue = 0). */
                    result = g_usbd.control_transfer(handle, 0x21u, 0x0bu, 0u,
                        alt->bInterfaceNumber, 0, 0, 500u);
                    if (result < 0) {
                        g_usbd.release_interface(handle, alt->bInterfaceNumber);
                        g_usbd.close(handle);
                        continue;
                    }

                    /* SET_IDLE is advisory; some devices do not implement it. */
                    g_usbd.control_transfer(handle, 0x21u, 0x0au, 0x0400u,
                        alt->bInterfaceNumber, 0, 0, 500u);
                }

                memset(out, 0, sizeof(*out));
                out->handle = handle;
                out->interface_number = alt->bInterfaceNumber;
                out->endpoint_in = endpoint_in;
                out->packet_size = packet_size;
                out->generic_report = (uint8_t)(generic != 0);
                out->claimed = 1;
                if (generic) out->layout = layout;
                found = 1;

                printf("[Key4Play] %s attached VID=%04x PID=%04x interface=%u IN=0x%02x packet=%u mode=%s\\n",
                    mouse ? "Mouse" : "Keyboard", descriptor.idVendor,
                    descriptor.idProduct, alt->bInterfaceNumber,
                    endpoint_in, packet_size,
                    generic ? "descriptor" : "boot");
            }
        }
        g_usbd.free_config_descriptor(config);
    }

    g_usbd.free_device_list(devices);
    return found;
}


static int hooks_explicitly_enabled(void) {
    FILE* file = fopen("/data/GoldHEN/key4play.ini", "r");
    char line[128];
    int in_section = 0;
    int enabled = 0;
    if (!file) {
        printf("[Key4Play] No key4play.ini; controller overlay remains disabled\\n");
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        char* p = line;
        char* end;
        while (*p && isspace((unsigned char)*p)) ++p;
        end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1])) *--end = '\\0';
        if (*p == '\\0' || *p == ';' || *p == '#') continue;
        if (*p == '[') {
            in_section = (strcmp(p, "[Key4Play]") == 0);
            continue;
        }
        if (in_section && strcmp(p, "enable_hooks=1") == 0) {
            enabled = 1;
            break;
        }
    }
    fclose(file);
    return enabled;
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
    uint8_t packet[K4P_HID_MAX_REPORT_BYTES];
    memset(&device, 0, sizeof(device));
    k4p_pad_state_neutral(&previous);
    k4p_settings_default(&settings);

    while (!__atomic_load_n(&g_reader_stop, __ATOMIC_ACQUIRE)) {
        int transferred = 0;
        int result;

        if (!device.handle) {
            if (!find_hid_device(&device, mouse)) {
                if (published_connected) {
                    if (mouse) k4p_input_snapshot_publish_mouse(&g_k4p_input_snapshot, 0, 0);
                    else k4p_input_snapshot_publish_keyboard(&g_k4p_input_snapshot, 0, 0);
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
            if (device.generic_report) {
                uint8_t report_id = device.layout.uses_report_ids ? packet[0] : 0u;
                /* Ignore reports belonging to another collection (e.g. Consumer Control). */
                if (!layout_has_report(&device.layout, report_id, mouse))
                    continue;
                if (mouse) {
                    K4pMouseReport report;
                    valid = k4p_hid_decode_mouse(&device.layout, packet,
                                                  (size_t)transferred, &report);
                    if (valid) k4p_map_mouse(&report, &settings, &current);
                } else {
                    K4pKeyboardReport report;
                    valid = k4p_hid_decode_keyboard(&device.layout, packet,
                                                     (size_t)transferred, &report);
                    if (valid) k4p_map_keyboard(&report, &current);
                }
            } else if (mouse) {
                K4pMouseReport report;
                valid = k4p_decode_boot_mouse(packet, (size_t)transferred, &report);
                if (valid) k4p_map_mouse(&report, &settings, &current);
            } else {
                K4pKeyboardReport report;
                valid = k4p_decode_boot_keyboard(packet, (size_t)transferred, &report);
                if (valid) k4p_map_keyboard(&report, &current);
            }

            if (valid) {
                if (mouse) k4p_input_snapshot_publish_mouse(&g_k4p_input_snapshot, &current, 1);
                else k4p_input_snapshot_publish_keyboard(&g_k4p_input_snapshot, &current, 1);
            } else {
                k4p_pad_state_neutral(&current);
                if (mouse) k4p_input_snapshot_publish_mouse(&g_k4p_input_snapshot, &current, 1);
                else k4p_input_snapshot_publish_keyboard(&g_k4p_input_snapshot, &current, 1);
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
            /* HID mice may report only on motion; never leave a stale stick deflection. */
            if (mouse) {
                K4pPadState neutral;
                k4p_pad_state_neutral(&neutral);
                k4p_input_snapshot_publish_mouse(&g_k4p_input_snapshot, &neutral, 1);
                k4p_pad_state_neutral(&previous);
            }
            continue;
        } else if (result < 0) {
            printf("[Key4Play] %s USB read failed: %d; reconnecting\n",
                   mouse ? "Mouse" : "Keyboard", result);
            if (mouse) k4p_input_snapshot_publish_mouse(&g_k4p_input_snapshot, 0, 0);
            else k4p_input_snapshot_publish_keyboard(&g_k4p_input_snapshot, 0, 0);
            published_connected = 0;
            k4p_pad_state_neutral(&previous);
            close_device(&device);
            sceKernelUsleep(250000);
        }
    }

    close_device(&device);
    if (mouse) k4p_input_snapshot_publish_mouse(&g_k4p_input_snapshot, 0, 0);
    else k4p_input_snapshot_publish_keyboard(&g_k4p_input_snapshot, 0, 0);
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
    k4p_input_snapshot_init(&g_k4p_input_snapshot);
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

    printf("[Key4Play] USB readers started.\\n");
    if (hooks_explicitly_enabled()) {
        result = k4p_pad_hook_start();
        if (result < 0) {
            printf("[Key4Play] Controller hook setup failed; stopping readers\\n");
            __atomic_store_n(&g_reader_stop, 1, __ATOMIC_RELEASE);
            scePthreadJoin(g_reader_threads[0], 0);
            scePthreadJoin(g_reader_threads[1], 0);
            g_reader_started[0] = g_reader_started[1] = 0;
            g_usbd.exit();
            g_usbd_initialized = 0;
            return result;
        }
    } else {
        printf("[Key4Play] Controller overlay disabled by configuration.\\n");
    }
    return 0;
}

K4P_PUBLIC int plugin_unload(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;
    k4p_pad_hook_stop();
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
