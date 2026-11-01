#include "key4play/input_core.h"

#include <assert.h>
#include <stdint.h>

static K4pKeyboardReport keyboard_with(const uint8_t *usages, unsigned count,
                                       uint8_t modifiers) {
    K4pKeyboardReport report = {0};
    unsigned i;
    report.modifiers = modifiers;
    for (i = 0; i < count && i < 6; ++i) report.keys[i] = usages[i];
    return report;
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
        const uint8_t usages[] = {0x08, 0x14, 0x06, 0x19, 0x2b, 0x28};
        keyboard = keyboard_with(usages, 6, 0);
        k4p_map_keyboard(&keyboard, &mapped);
        assert(mapped.buttons & K4P_BTN_SQUARE);
        assert(mapped.buttons & K4P_BTN_TRIANGLE);
        assert(mapped.buttons & K4P_BTN_L3);
        assert(mapped.buttons & K4P_BTN_R3);
        assert(mapped.buttons & K4P_BTN_SHARE);
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
    return 0;
}
