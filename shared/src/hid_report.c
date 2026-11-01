#include "key4play/hid_report.h"

#include <string.h>

#define HID_MAX_DESCRIPTOR_BYTES 4096u
#define HID_MAX_ITEMS 512u
#define HID_MAX_GLOBAL_DEPTH 8u
#define HID_MAX_COLLECTION_DEPTH 16u
#define HID_MAX_LOCAL_USAGES 32u
#define HID_MAX_REPORT_BITS (K4P_HID_MAX_REPORT_BYTES * 8u)

enum {
    HID_TYPE_MAIN = 0,
    HID_TYPE_GLOBAL = 1,
    HID_TYPE_LOCAL = 2,
    HID_TAG_INPUT = 8,
    HID_TAG_COLLECTION = 10,
    HID_TAG_END_COLLECTION = 12,
    HID_GLOBAL_USAGE_PAGE = 0,
    HID_GLOBAL_LOGICAL_MIN = 1,
    HID_GLOBAL_LOGICAL_MAX = 2,
    HID_GLOBAL_REPORT_SIZE = 7,
    HID_GLOBAL_REPORT_ID = 8,
    HID_GLOBAL_REPORT_COUNT = 9,
    HID_GLOBAL_PUSH = 10,
    HID_GLOBAL_POP = 11,
    HID_LOCAL_USAGE = 0,
    HID_LOCAL_USAGE_MIN = 1,
    HID_LOCAL_USAGE_MAX = 2
};

typedef struct HidGlobalState {
    uint16_t usage_page;
    int32_t logical_min;
    int32_t logical_max;
    uint32_t report_size;
    uint32_t report_count;
    uint8_t report_id;
} HidGlobalState;

typedef struct HidUsage {
    uint16_t page;
    uint16_t usage;
} HidUsage;

typedef struct HidLocalState {
    HidUsage usages[HID_MAX_LOCAL_USAGES];
    uint8_t usage_count;
    HidUsage usage_min;
    HidUsage usage_max;
    uint8_t has_range;
} HidLocalState;

static uint32_t read_unsigned(const uint8_t *data, size_t size) {
    uint32_t value = 0;
    size_t i;
    for (i = 0; i < size; ++i) value |= (uint32_t)data[i] << (8u * i);
    return value;
}

static int32_t read_signed(const uint8_t *data, size_t size) {
    uint32_t value = read_unsigned(data, size);
    unsigned bits = (unsigned)(size * 8u);
    if (bits == 0u || bits >= 32u) return (int32_t)value;
    if (value & (1u << (bits - 1u))) value |= (~0u << bits);
    return (int32_t)value;
}

static HidUsage decode_usage(uint32_t value, size_t size,
                             uint16_t current_page) {
    HidUsage usage;
    if (size == 4u && (value >> 16) != 0u) {
        usage.page = (uint16_t)(value >> 16);
        usage.usage = (uint16_t)value;
    } else {
        usage.page = current_page;
        usage.usage = (uint16_t)value;
    }
    return usage;
}

static void local_reset(HidLocalState *local) {
    memset(local, 0, sizeof(*local));
}

static int local_add_usage(HidLocalState *local, HidUsage usage) {
    if (local->usage_count >= HID_MAX_LOCAL_USAGES) return 0;
    local->usages[local->usage_count++] = usage;
    return 1;
}

static int collection_for_usage(HidUsage usage) {
    if (usage.page != 0x01u) return K4P_HID_COLLECTION_NONE;
    if (usage.usage == 0x06u) return K4P_HID_COLLECTION_KEYBOARD;
    if (usage.usage == 0x02u) return K4P_HID_COLLECTION_MOUSE;
    return K4P_HID_COLLECTION_NONE;
}

static int field_is_relevant(uint16_t page, uint16_t usage_min,
                             uint16_t usage_max, uint8_t collection) {
    if (collection == K4P_HID_COLLECTION_KEYBOARD) return page == 0x07u;
    if (collection == K4P_HID_COLLECTION_MOUSE) {
        if (page == 0x09u) return 1;
        if (page == 0x01u &&
            ((usage_min <= 0x30u && usage_max >= 0x30u) ||
             (usage_min <= 0x31u && usage_max >= 0x31u) ||
             (usage_min <= 0x38u && usage_max >= 0x38u))) return 1;
    }
    return 0;
}

static int append_field(K4pHidLayout *layout, const HidGlobalState *global,
                        uint16_t page, uint16_t usage_min, uint16_t usage_max,
                        uint16_t bit_offset, uint16_t report_count,
                        int is_variable, uint8_t collection) {
    K4pHidField *field;
    if (!field_is_relevant(page, usage_min, usage_max, collection)) return 1;
    if (layout->field_count >= K4P_HID_MAX_FIELDS) return 0;

    field = &layout->fields[layout->field_count++];
    field->usage_page = page;
    field->usage_min = usage_min;
    field->usage_max = usage_max;
    field->bit_offset = bit_offset;
    field->report_size = (uint16_t)global->report_size;
    field->report_count = report_count;
    field->logical_min = global->logical_min;
    field->logical_max = global->logical_max;
    field->report_id = global->report_id;
    field->collection = collection;
    field->is_variable = (uint8_t)(is_variable != 0);
    return 1;
}

static int append_input_fields(K4pHidLayout *layout,
                               const HidGlobalState *global,
                               const HidLocalState *local,
                               uint32_t flags, uint16_t bit_offset,
                               uint8_t collection) {
    uint32_t i;
    int is_variable = (flags & 0x02u) != 0u;

    if (flags & 0x01u) return 1; /* Constant/padding input. */

    if (local->has_range) {
        if (local->usage_min.page != local->usage_max.page) return 0;
        if (is_variable) {
            return append_field(layout, global, local->usage_min.page,
                                local->usage_min.usage, local->usage_max.usage,
                                bit_offset, (uint16_t)global->report_count,
                                1, collection);
        }
        return append_field(layout, global, local->usage_min.page,
                            local->usage_min.usage, local->usage_max.usage,
                            bit_offset, (uint16_t)global->report_count,
                            0, collection);
    }

    if (local->usage_count == 0u) return 1;

    if (!is_variable) {
        /* Array values are usages, not individual one-bit switches. */
        return append_field(layout, global, local->usages[0].page,
                            local->usages[0].usage,
                            local->usages[local->usage_count - 1u].usage,
                            bit_offset, (uint16_t)global->report_count,
                            0, collection);
    }

    for (i = 0; i < global->report_count && i < local->usage_count; ++i) {
        uint32_t offset = (uint32_t)bit_offset + i * global->report_size;
        HidUsage usage = local->usages[i];
        if (offset > 0xffffu) return 0;
        if (!append_field(layout, global, usage.page, usage.usage,
                          usage.usage, (uint16_t)offset, 1u, 1,
                          collection)) return 0;
    }
    return 1;
}

static int read_field_bits(const uint8_t *data, size_t length,
                           uint32_t bit_offset, uint32_t bit_count,
                           uint32_t *value) {
    uint32_t result = 0;
    uint32_t i;
    if (!data || !value || bit_count == 0u || bit_count > 32u) return 0;
    if ((uint64_t)bit_offset + bit_count > (uint64_t)length * 8u) return 0;
    for (i = 0; i < bit_count; ++i) {
        uint32_t source_bit = bit_offset + i;
        uint32_t bit = (data[source_bit / 8u] >> (source_bit % 8u)) & 1u;
        result |= bit << i;
    }
    *value = result;
    return 1;
}

static int32_t field_value(const K4pHidField *field, uint32_t raw) {
    uint32_t bits = field->report_size;
    if (field->logical_min >= 0 || bits == 0u || bits >= 32u)
        return (int32_t)raw;
    if (raw & (1u << (bits - 1u))) raw |= (~0u << bits);
    return (int32_t)raw;
}

static int select_report(const K4pHidLayout *layout, const uint8_t *data,
                         size_t length, uint8_t *report_id) {
    uint8_t id = 0;
    if (!layout || !data || !report_id) return 0;
    if (layout->uses_report_ids) {
        if (length < 1u) return 0;
        id = data[0];
    }
    if (layout->report_bytes[id] == 0u ||
        length < layout->report_bytes[id]) return 0;
    *report_id = id;
    return 1;
}

static void add_keyboard_usage(K4pKeyboardReport *report, uint16_t usage) {
    size_t i;
    if (usage >= 0xe0u && usage <= 0xe7u) {
        report->modifiers |= (uint8_t)(1u << (usage - 0xe0u));
        return;
    }
    if (usage < 0x04u || usage > 0xe7u) return;
    for (i = 0; i < 6u; ++i) {
        if (report->keys[i] == (uint8_t)usage) return;
        if (report->keys[i] == 0u) {
            report->keys[i] = (uint8_t)usage;
            return;
        }
    }
}

int k4p_hid_parse_report_descriptor(const uint8_t *descriptor, size_t length,
                                    K4pHidLayout *layout) {
    HidGlobalState global;
    HidGlobalState global_stack[HID_MAX_GLOBAL_DEPTH];
    HidLocalState local;
    uint8_t collection_stack[HID_MAX_COLLECTION_DEPTH];
    uint16_t report_bits[256] = {0};
    uint8_t report_seen[256] = {0};
    uint8_t saw_id_zero = 0;
    uint8_t saw_nonzero_id = 0;
    size_t offset = 0;
    unsigned item_count = 0;
    unsigned global_depth = 0;
    unsigned collection_depth = 0;

    if (!descriptor || !layout || length == 0u ||
        length > HID_MAX_DESCRIPTOR_BYTES) return 0;

    memset(layout, 0, sizeof(*layout));
    memset(&global, 0, sizeof(global));
    local_reset(&local);

    while (offset < length) {
        uint8_t prefix = descriptor[offset++];
        uint32_t value;
        size_t item_size;
        uint8_t item_type;
        uint8_t item_tag;

        if (++item_count > HID_MAX_ITEMS) return 0;

        if (prefix == 0xfeu) {
            size_t long_size;
            if (offset + 2u > length) return 0;
            long_size = descriptor[offset];
            offset += 2u; /* Length and long-item tag. */
            if (long_size > length - offset) return 0;
            offset += long_size;
            continue;
        }

        item_size = prefix & 0x03u;
        if (item_size == 3u) item_size = 4u;
        item_type = (uint8_t)((prefix >> 2) & 0x03u);
        item_tag = (uint8_t)(prefix >> 4);
        if (item_size > length - offset) return 0;
        value = read_unsigned(descriptor + offset, item_size);

        if (item_type == HID_TYPE_GLOBAL) {
            switch (item_tag) {
                case HID_GLOBAL_USAGE_PAGE:
                    if (value > 0xffffu) return 0;
                    global.usage_page = (uint16_t)value;
                    break;
                case HID_GLOBAL_LOGICAL_MIN:
                    global.logical_min = read_signed(descriptor + offset,
                                                     item_size);
                    break;
                case HID_GLOBAL_LOGICAL_MAX:
                    global.logical_max = global.logical_min < 0
                        ? read_signed(descriptor + offset, item_size)
                        : (int32_t)value;
                    break;
                case HID_GLOBAL_REPORT_SIZE:
                    if (value == 0u || value > 32u) return 0;
                    global.report_size = value;
                    break;
                case HID_GLOBAL_REPORT_ID:
                    if (value == 0u || value > 255u) return 0;
                    global.report_id = (uint8_t)value;
                    layout->uses_report_ids = 1u;
                    break;
                case HID_GLOBAL_REPORT_COUNT:
                    if (value == 0u || value > 128u) return 0;
                    global.report_count = value;
                    break;
                case HID_GLOBAL_PUSH:
                    if (global_depth >= HID_MAX_GLOBAL_DEPTH) return 0;
                    global_stack[global_depth++] = global;
                    break;
                case HID_GLOBAL_POP:
                    if (global_depth == 0u) return 0;
                    global = global_stack[--global_depth];
                    break;
                default:
                    break;
            }
        } else if (item_type == HID_TYPE_LOCAL) {
            HidUsage usage;
            if (item_tag == HID_LOCAL_USAGE ||
                item_tag == HID_LOCAL_USAGE_MIN ||
                item_tag == HID_LOCAL_USAGE_MAX) {
                usage = decode_usage(value, item_size, global.usage_page);
                if (item_tag == HID_LOCAL_USAGE) {
                    if (!local_add_usage(&local, usage)) return 0;
                } else if (item_tag == HID_LOCAL_USAGE_MIN) {
                    local.usage_min = usage;
                    local.has_range = 1u;
                } else {
                    local.usage_max = usage;
                    local.has_range = 1u;
                }
            }
        } else if (item_type == HID_TYPE_MAIN) {
            if (item_tag == HID_TAG_COLLECTION) {
                uint8_t inherited = collection_depth
                    ? collection_stack[collection_depth - 1u]
                    : K4P_HID_COLLECTION_NONE;
                if (collection_depth >= HID_MAX_COLLECTION_DEPTH) return 0;
                if (value == 0x01u) {
                    HidUsage app_usage;
                    if (local.has_range) app_usage = local.usage_min;
                    else if (local.usage_count) app_usage = local.usages[0];
                    else {
                        app_usage.page = 0;
                        app_usage.usage = 0;
                    }
                    {
                        int kind = collection_for_usage(app_usage);
                        if (kind != K4P_HID_COLLECTION_NONE)
                            inherited = (uint8_t)kind;
                    }
                }
                collection_stack[collection_depth++] = inherited;
            } else if (item_tag == HID_TAG_END_COLLECTION) {
                if (collection_depth == 0u) return 0;
                --collection_depth;
            } else if (item_tag == HID_TAG_INPUT) {
                uint32_t report_size = global.report_size;
                uint32_t report_count = global.report_count;
                uint32_t bits;
                uint8_t id = global.report_id;
                uint8_t collection = collection_depth
                    ? collection_stack[collection_depth - 1u]
                    : K4P_HID_COLLECTION_NONE;

                if (report_size == 0u || report_count == 0u ||
                    report_size > 32u || report_count > 128u) return 0;
                bits = report_size * report_count;
                if (bits > HID_MAX_REPORT_BITS) return 0;
                if (id == 0u) saw_id_zero = 1u;
                else saw_nonzero_id = 1u;
                if (saw_id_zero && saw_nonzero_id) return 0;

                if (!report_seen[id]) {
                    report_seen[id] = 1u;
                    report_bits[id] = id ? 8u : 0u;
                }
                if ((uint32_t)report_bits[id] + bits > HID_MAX_REPORT_BITS)
                    return 0;
                if (!append_input_fields(layout, &global, &local, value,
                                         report_bits[id], collection)) return 0;
                report_bits[id] = (uint16_t)(report_bits[id] + bits);
                layout->report_bytes[id] =
                    (uint8_t)((report_bits[id] + 7u) / 8u);
            }

            /* Local items apply to one Main item only. */
            local_reset(&local);
        }

        offset += item_size;
    }

    if (global_depth != 0u || collection_depth != 0u ||
        layout->field_count == 0u) return 0;
    return 1;
}

int k4p_hid_decode_keyboard(const K4pHidLayout *layout,
                            const uint8_t *data, size_t length,
                            K4pKeyboardReport *report) {
    uint8_t report_id;
    unsigned f;
    if (!layout || !report) return 0;
    memset(report, 0, sizeof(*report));
    if (!select_report(layout, data, length, &report_id)) return 0;

    for (f = 0; f < layout->field_count; ++f) {
        const K4pHidField *field = &layout->fields[f];
        uint32_t i;
        if (field->report_id != report_id ||
            field->collection != K4P_HID_COLLECTION_KEYBOARD ||
            field->usage_page != 0x07u) continue;

        for (i = 0; i < field->report_count; ++i) {
            uint32_t raw;
            uint32_t bit_offset = (uint32_t)field->bit_offset +
                                  i * field->report_size;
            uint16_t usage;
            if (field->is_variable) {
                usage = (uint16_t)(field->usage_min + i);
                if (usage > field->usage_max) break;
            } else {
                if (!read_field_bits(data, length, bit_offset,
                                     field->report_size, &raw)) return 0;
                if (raw == 0u) continue;
                if (raw >= 1u && raw <= 3u) return 0;
                usage = (uint16_t)raw;
                if (usage < field->usage_min || usage > field->usage_max)
                    continue;
                add_keyboard_usage(report, usage);
                continue;
            }

            if (!read_field_bits(data, length, bit_offset,
                                 field->report_size, &raw)) return 0;
            if (raw != 0u) add_keyboard_usage(report, usage);
        }
    }
    return 1;
}

static int8_t clamp_mouse_axis(int32_t value) {
    if (value > 127) value = 127;
    if (value < -127) value = -127;
    return (int8_t)value;
}

int k4p_hid_decode_mouse(const K4pHidLayout *layout,
                         const uint8_t *data, size_t length,
                         K4pMouseReport *report) {
    uint8_t report_id;
    unsigned f;
    if (!layout || !report) return 0;
    memset(report, 0, sizeof(*report));
    if (!select_report(layout, data, length, &report_id)) return 0;

    for (f = 0; f < layout->field_count; ++f) {
        const K4pHidField *field = &layout->fields[f];
        uint32_t i;
        if (field->report_id != report_id ||
            field->collection != K4P_HID_COLLECTION_MOUSE) continue;

        for (i = 0; i < field->report_count; ++i) {
            uint32_t raw;
            uint32_t bit_offset = (uint32_t)field->bit_offset +
                                  i * field->report_size;
            uint16_t usage;
            int32_t value;
            if (field->is_variable) usage = (uint16_t)(field->usage_min + i);
            else usage = field->usage_min;

            if (!read_field_bits(data, length, bit_offset,
                                 field->report_size, &raw)) return 0;
            value = field_value(field, raw);

            if (field->usage_page == 0x09u) {
                if (!field->is_variable && raw != 0u) usage = (uint16_t)raw;
                if (value != 0 && usage >= 1u && usage <= 3u)
                    report->buttons |= (uint8_t)(1u << (usage - 1u));
            } else if (field->usage_page == 0x01u) {
                if (usage == 0x30u) report->x = clamp_mouse_axis(value);
                else if (usage == 0x31u) report->y = clamp_mouse_axis(value);
                else if (usage == 0x38u) report->wheel = clamp_mouse_axis(value);
            }
        }
    }
    return 1;
}
