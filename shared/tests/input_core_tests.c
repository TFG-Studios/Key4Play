#include "key4play/input_core.h"
#include "key4play/hid_report.h"
#include "key4play/input_snapshot.h"
#include "key4play/pad_jump_plan.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static K4pKeyboardReport keyboard_with(const uint8_t *usages, unsigned count,
                                       uint8_t modifiers) {
    K4pKeyboardReport report = {0};
    unsigned i;
    report.modifiers = modifiers;
    for (i = 0; i < count && i < 6; ++i) report.keys[i] = usages[i];
    return report;
}


static void test_descriptor_parser(void) {
    /* Composite descriptor: NKRO keyboard (ID 1) + signed mouse (ID 2). */
    static const uint8_t descriptor[] = {
        0x05,0x01, 0x09,0x06, 0xa1,0x01, 0x85,0x01,
        0x05,0x07, 0x19,0xe0, 0x29,0xe7, 0x15,0x00, 0x25,0x01,
        0x75,0x01, 0x95,0x08, 0x81,0x02,
        0x75,0x01, 0x95,0x08, 0x81,0x01,
        0x19,0x00, 0x29,0x65, 0x75,0x01, 0x95,0x66, 0x81,0x02,
        0xc0,
        0x05,0x01, 0x09,0x02, 0xa1,0x01, 0x85,0x02,
        0x09,0x01, 0xa1,0x00,
        0x05,0x09, 0x19,0x01, 0x29,0x03, 0x15,0x00, 0x25,0x01,
        0x75,0x01, 0x95,0x03, 0x81,0x02,
        0x75,0x01, 0x95,0x05, 0x81,0x01,
        0x05,0x01, 0x15,0x81, 0x25,0x7f,
        0x09,0x30, 0x09,0x31, 0x75,0x08, 0x95,0x02, 0x81,0x06,
        0x09,0x38, 0x95,0x01, 0x81,0x06,
        0xc0, 0xc0
    };
    K4pHidLayout layout;
    K4pKeyboardReport keyboard;
    K4pMouseReport mouse;
    uint8_t keyboard_packet[16] = {0};
    const uint8_t mouse_packet[] = {0x02, 0x05, 0xf6, 0x0a, 0xff};
    const uint8_t malformed[] = {0x75, 0x20, 0x95, 0xff, 0x81, 0x02};

    assert(k4p_hid_parse_report_descriptor(descriptor, sizeof(descriptor), &layout));
    assert(layout.uses_report_ids);
    assert(layout.report_bytes[1] == sizeof(keyboard_packet));
    assert(layout.report_bytes[2] == sizeof(mouse_packet));

    /* Report ID 1: modifier byte, padding byte, then NKRO bitmap. */
    keyboard_packet[0] = 0x01;
    keyboard_packet[1] = 0x03; /* Left Ctrl + Left Shift. */
    keyboard_packet[3u + (0x1au / 8u)] |= (uint8_t)(1u << (0x1au % 8u));
    keyboard_packet[3u + (0x2cu / 8u)] |= (uint8_t)(1u << (0x2cu % 8u));
    assert(k4p_hid_decode_keyboard(&layout, keyboard_packet,
                                   sizeof(keyboard_packet), &keyboard));
    assert(keyboard.modifiers == 0x03u);
    assert(k4p_key_down(&keyboard, 0x1au));
    assert(k4p_key_down(&keyboard, 0x2cu));
    assert(!k4p_hid_decode_keyboard(&layout, keyboard_packet, 2u, &keyboard));

    /* Report ID 2: buttons 1+3, X=-10, Y=10, wheel=-1. */
    assert(k4p_hid_decode_mouse(&layout, mouse_packet, sizeof(mouse_packet), &mouse));
    assert(mouse.buttons == 0x05u);
    assert(mouse.x == -10 && mouse.y == 10 && mouse.wheel == -1);
    assert(!k4p_hid_decode_mouse(&layout, mouse_packet, 2u, &mouse));

    assert(!k4p_hid_parse_report_descriptor(malformed, sizeof(malformed), &layout));
    assert(!k4p_hid_parse_report_descriptor(descriptor, 4097u, &layout));
}


static void test_pad_jump_plan(void) {
    uint8_t stub[16];
    K4pPadJumpPlan plan;
    int32_t original_delta = -0x200;
    unsigned i;

    memset(stub, 0, sizeof(stub));
    stub[0] = 0x31u;
    stub[1] = 0xc9u; /* xor ecx, ecx */
    stub[2] = 0xe9u; /* jmp rel32 */
    memcpy(stub + 3u, &original_delta, sizeof(original_delta));
    for (i = 7u; i < 16u; ++i) stub[i] = 0x90u;

    assert(k4p_pad_plan_jump(stub, 0x10000000u, 0x10001000u,
                             0xc9u, &plan) == 0);
    assert(plan.original_delta == (uint32_t)original_delta);
    assert(plan.replacement_delta == 0x0ff9u);
    assert(plan.original_target == 0x0ffffe07u);

    /* The second Pad entry uses xor edx, edx. */
    stub[1] = 0xd2u;
    assert(k4p_pad_plan_jump(stub, 0x10000000u, 0x10002000u,
                             0xd2u, &plan) == 0);

    stub[1] = 0xc9u;
    stub[7] = 0x00u;
    assert(k4p_pad_plan_jump(stub, 0x10000000u, 0x10001000u,
                             0xc9u, &plan) == -5);
    stub[7] = 0x90u;

    stub[1] = 0x90u;
    assert(k4p_pad_plan_jump(stub, 0x10000000u, 0x10001000u,
                             0xc9u, &plan) == -4);
    stub[1] = 0xc9u;

    original_delta = -8;
    memcpy(stub + 3u, &original_delta, sizeof(original_delta));
    assert(k4p_pad_plan_jump(stub, 0x10000000u, 0x10001000u,
                             0xc9u, &plan) == -6);

    original_delta = -0x200000;
    memcpy(stub + 3u, &original_delta, sizeof(original_delta));
    assert(k4p_pad_plan_jump(stub, 0x10000000u, 0x10001000u,
                             0xc9u, &plan) == -6);

    original_delta = -0x200;
    memcpy(stub + 3u, &original_delta, sizeof(original_delta));
    assert(k4p_pad_plan_jump(stub, 0x10000001u, 0x10001000u,
                             0xc9u, &plan) == -2);
    assert(k4p_pad_plan_jump(stub, 0x10003ff8u, 0x10004000u,
                             0xc9u, &plan) == -2);
    assert(k4p_pad_plan_jump(stub, 0x10000000u, 0x9000000000ULL,
                             0xc9u, &plan) == -9);
}

static void test_input_snapshot(void) {
    K4pInputSnapshot snapshot;
    K4pInputFrame frame;
    K4pPadState keyboard;
    K4pPadState mouse;
    K4pPadState combined;

    k4p_input_snapshot_init(&snapshot);
    assert(k4p_input_snapshot_read(&snapshot, &frame));
    assert(frame.generation == 0u);

    /* The game polling hook must skip a busy snapshot instead of spinning. */
    snapshot.lock_word = 1L;
    assert(!k4p_input_snapshot_read(&snapshot, &frame));
    snapshot.lock_word = 0L;
    assert(!frame.keyboard_connected && !frame.mouse_connected);

    k4p_pad_state_neutral(&keyboard);
    keyboard.buttons = K4P_BTN_CROSS;
    keyboard.axis_override = K4P_AXIS_LX;
    keyboard.lx = 20;
    k4p_input_snapshot_publish_keyboard(&snapshot, &keyboard, 1);
    assert(k4p_input_snapshot_read(&snapshot, &frame));
    assert(frame.keyboard_connected && frame.generation == 1u);
    assert(frame.keyboard.buttons == K4P_BTN_CROSS);

    k4p_pad_state_neutral(&mouse);
    mouse.buttons = K4P_BTN_R2;
    mouse.r2 = 255;
    mouse.axis_override = K4P_AXIS_RX;
    mouse.rx = 220;
    k4p_input_snapshot_publish_mouse(&snapshot, &mouse, 1);
    assert(k4p_input_snapshot_read(&snapshot, &frame));
    assert(frame.mouse_connected && frame.generation == 2u);

    k4p_input_snapshot_compose(&frame, &combined);
    assert((combined.buttons & (K4P_BTN_CROSS | K4P_BTN_R2)) ==
           (K4P_BTN_CROSS | K4P_BTN_R2));
    assert(combined.lx == 20 && combined.rx == 220 && combined.r2 == 255);

    /* A disconnect publishes neutral state instead of leaving stuck keys. */
    k4p_input_snapshot_publish_keyboard(&snapshot, 0, 0);
    assert(k4p_input_snapshot_read(&snapshot, &frame));
    assert(!frame.keyboard_connected);
    assert(frame.keyboard.buttons == 0u);
    assert(frame.keyboard.lx == 128 && frame.keyboard.ly == 128);
    assert(frame.generation == 3u);

    k4p_input_snapshot_publish_mouse(&snapshot, 0, 0);
    assert(k4p_input_snapshot_read(&snapshot, &frame));
    assert(!frame.mouse_connected && frame.mouse.buttons == 0u);
    assert(frame.generation == 4u);
}

int main(void) {
    K4pSettings settings;
    K4pKeyboardReport keyboard;
    K4pMouseReport mouse;
    K4pPadState mapped;
    K4pPadState physical;
    const uint8_t valid_keyboard[] = {0x03, 0, 0x1a, 0x04, 0x2c, 0, 0, 0};
    const uint8_t rollover_keyboard[] = {0, 0, 1, 0, 0, 0, 0, 0};
    const uint8_t mouse_packet[] = {0x07, 0xf6, 0x0a, 0x01};
    const uint8_t short_packet[] = {0, 0};

    k4p_settings_default(&settings);
    assert(settings.mouse_sensitivity > 0.0f);

    assert(k4p_decode_boot_keyboard(valid_keyboard, sizeof(valid_keyboard), &keyboard));
    assert(!k4p_decode_boot_keyboard(short_packet, sizeof(short_packet), &keyboard));
    assert(!k4p_decode_boot_keyboard(rollover_keyboard, sizeof(rollover_keyboard), &keyboard));
    assert(k4p_key_down(&keyboard, 0x1a));
    assert(!k4p_key_down(&keyboard, 0x1d));

    /* W + A + Space + left Ctrl + Shift + R map to sticks/buttons. */
    {
        const uint8_t usages[] = {0x1a, 0x04, 0x2c, 0x15};
        keyboard = keyboard_with(usages, 4, 0x03);
        k4p_map_keyboard(&keyboard, &mapped);
        assert((mapped.axis_override & (K4P_AXIS_LX | K4P_AXIS_LY)) ==
               (K4P_AXIS_LX | K4P_AXIS_LY));
        assert(mapped.lx == 0 && mapped.ly == 0);
        assert(mapped.buttons & K4P_BTN_CROSS);
        assert(mapped.buttons & K4P_BTN_CIRCLE);
        assert(mapped.buttons & K4P_BTN_L1);
        assert(mapped.buttons & K4P_BTN_R1);
    }

    /* Every non-movement binding has a deterministic controller mapping. */
    {
        const uint8_t usages[] = {0x08, 0x14, 0x06, 0x19, 0x28};
        keyboard = keyboard_with(usages, 5, 0);
        k4p_map_keyboard(&keyboard, &mapped);
        assert(mapped.buttons & K4P_BTN_SQUARE);
        assert(mapped.buttons & K4P_BTN_TRIANGLE);
        assert(mapped.buttons & K4P_BTN_L3);
        assert(mapped.buttons & K4P_BTN_R3);
        assert(mapped.buttons & K4P_BTN_OPTIONS);
    }

    /* D-pad mappings are direct button bits. */
    {
        const uint8_t usages[] = {0x52, 0x4f, 0x51, 0x50};
        keyboard = keyboard_with(usages, 4, 0);
        k4p_map_keyboard(&keyboard, &mapped);
        assert((mapped.buttons & (K4P_BTN_UP | K4P_BTN_RIGHT |
               K4P_BTN_DOWN | K4P_BTN_LEFT)) ==
               (K4P_BTN_UP | K4P_BTN_RIGHT | K4P_BTN_DOWN | K4P_BTN_LEFT));
    }

    assert(k4p_decode_boot_mouse(mouse_packet, sizeof(mouse_packet), &mouse));
    assert(mouse.x == -10 && mouse.y == 10 && mouse.wheel == 1);
    assert(!k4p_decode_boot_mouse(short_packet, sizeof(short_packet), &mouse));
    settings.mouse_sensitivity = 0.01f;
    settings.mouse_deadzone = 0.0f;
    settings.mouse_acceleration = 0.0f;
    mouse.buttons = 0x07;
    mouse.x = 20;
    mouse.y = -10;
    mouse.wheel = 0;
    k4p_map_mouse(&mouse, &settings, &mapped);
    assert(mapped.buttons & K4P_BTN_R2);
    assert(mapped.buttons & K4P_BTN_L2);
    assert(mapped.buttons & K4P_BTN_TOUCHPAD);
    assert(mapped.r2 == 255 && mapped.l2 == 255);
    assert(mapped.rx > 128 && mapped.ry < 128);
    assert((mapped.axis_override & (K4P_AXIS_RX | K4P_AXIS_RY)) ==
           (K4P_AXIS_RX | K4P_AXIS_RY));

    /* A stationary mouse must not override the physical DS4 right stick. */
    mouse.x = 0;
    mouse.y = 0;
    k4p_map_mouse(&mouse, &settings, &mapped);
    assert((mapped.axis_override & (K4P_AXIS_RX | K4P_AXIS_RY)) == 0u);
    assert((mapped.buttons & (K4P_BTN_R2 | K4P_BTN_L2 | K4P_BTN_TOUCHPAD)) ==
           (K4P_BTN_R2 | K4P_BTN_L2 | K4P_BTN_TOUCHPAD));

    /* Merge must preserve untouched physical DS4 controls and merge new ones. */
    k4p_pad_state_neutral(&physical);
    physical.buttons = K4P_BTN_CIRCLE;
    physical.lx = 200;
    physical.l2 = 70;
    {
        K4pPadState addition;
        k4p_pad_state_neutral(&addition);
        addition.buttons = K4P_BTN_CROSS;
        addition.axis_override = K4P_AXIS_LY;
        addition.ly = 10;
        addition.r2 = 255;
        k4p_merge_pad_state(&physical, &addition);
    }
    assert((physical.buttons & (K4P_BTN_CIRCLE | K4P_BTN_CROSS)) ==
           (K4P_BTN_CIRCLE | K4P_BTN_CROSS));
    assert(physical.lx == 200); /* no keyboard input: preserve DS4 axis */
    assert(physical.ly == 10);
    assert(physical.l2 == 70 && physical.r2 == 255);
    test_descriptor_parser();
    test_input_snapshot();
    test_pad_jump_plan();
    return 0;
}
