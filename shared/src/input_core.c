#include "key4play/input_core.h"

#include <math.h>
#include <string.h>

enum {
    HID_KEY_A = 0x04, HID_KEY_C = 0x06, HID_KEY_D = 0x07,
    HID_KEY_E = 0x08, HID_KEY_Q = 0x14, HID_KEY_R = 0x15,
    HID_KEY_S = 0x16, HID_KEY_V = 0x19, HID_KEY_W = 0x1a,
    HID_KEY_SPACE = 0x2c, HID_KEY_TAB = 0x2b, HID_KEY_ENTER = 0x28,
    HID_KEY_UP = 0x52, HID_KEY_DOWN = 0x51,
    HID_KEY_LEFT = 0x50, HID_KEY_RIGHT = 0x4f
};

static float clampf(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

static int key(const K4pKeyboardReport *report, uint8_t usage) {
    return k4p_key_down(report, usage);
}

void k4p_settings_default(K4pSettings *settings) {
    if (!settings) return;
    settings->mouse_sensitivity = 0.012f;
    settings->mouse_deadzone = 0.0f;
    settings->mouse_acceleration = 0.0f;
}

void k4p_pad_state_neutral(K4pPadState *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->lx = 128;
    state->ly = 128;
    state->rx = 128;
    state->ry = 128;
}

int k4p_decode_boot_keyboard(const uint8_t *data, size_t length,
                             K4pKeyboardReport *report) {
    size_t i;
    if (!data || !report || length < 8) return 0;

    /* HID boot-keyboard rollover/error usages make the key array ambiguous. */
    for (i = 2; i < 8; ++i) {
        if (data[i] >= 1 && data[i] <= 3) return 0;
    }

    report->modifiers = data[0];
    report->reserved = data[1];
    for (i = 0; i < 6; ++i) report->keys[i] = data[i + 2];
    return 1;
}

int k4p_decode_boot_mouse(const uint8_t *data, size_t length,
                          K4pMouseReport *report) {
    if (!data || !report || length < 3) return 0;
    report->buttons = data[0] & 0x07u;
    report->x = (int8_t)data[1];
    report->y = (int8_t)data[2];
    report->wheel = length >= 4 ? (int8_t)data[3] : 0;
    return 1;
}

int k4p_key_down(const K4pKeyboardReport *report, uint8_t usage) {
    size_t i;
    if (!report || usage == 0) return 0;
    for (i = 0; i < 6; ++i) {
        if (report->keys[i] == usage) return 1;
    }
    return 0;
}

void k4p_map_keyboard(const K4pKeyboardReport *keyboard, K4pPadState *out) {
    int w, a, s, d, up, down, left, right;
    if (!out) return;
    k4p_pad_state_neutral(out);
    if (!keyboard) return;

    w = key(keyboard, HID_KEY_W);
    a = key(keyboard, HID_KEY_A);
    s = key(keyboard, HID_KEY_S);
    d = key(keyboard, HID_KEY_D);

    if (a || d) {
        out->axis_override |= K4P_AXIS_LX;
        out->lx = (a == d) ? 128 : (a ? 0 : 255);
    }
    if (w || s) {
        out->axis_override |= K4P_AXIS_LY;
        out->ly = (w == s) ? 128 : (w ? 0 : 255);
    }

    up = key(keyboard, HID_KEY_UP);
    down = key(keyboard, HID_KEY_DOWN);
    left = key(keyboard, HID_KEY_LEFT);
    right = key(keyboard, HID_KEY_RIGHT);
    if (up) out->buttons |= K4P_BTN_UP;
    if (down) out->buttons |= K4P_BTN_DOWN;
    if (left) out->buttons |= K4P_BTN_LEFT;
    if (right) out->buttons |= K4P_BTN_RIGHT;

    if (key(keyboard, HID_KEY_SPACE)) out->buttons |= K4P_BTN_CROSS;
    if (key(keyboard, HID_KEY_E)) out->buttons |= K4P_BTN_SQUARE;
    if (key(keyboard, HID_KEY_Q)) out->buttons |= K4P_BTN_TRIANGLE;
    if (keyboard->modifiers & K4P_MOD_LEFT_CTRL) out->buttons |= K4P_BTN_CIRCLE;
    if (keyboard->modifiers & K4P_MOD_LEFT_SHIFT) out->buttons |= K4P_BTN_L1;
    if (key(keyboard, HID_KEY_R)) out->buttons |= K4P_BTN_R1;
    if (key(keyboard, HID_KEY_C)) out->buttons |= K4P_BTN_L3;
    if (key(keyboard, HID_KEY_V)) out->buttons |= K4P_BTN_R3;
    if (key(keyboard, HID_KEY_TAB)) out->buttons |= K4P_BTN_SHARE;
    if (key(keyboard, HID_KEY_ENTER)) out->buttons |= K4P_BTN_OPTIONS;
}

static uint8_t axis_from_float(float value) {
    int converted = (int)(128.0f + clampf(value, -1.0f, 1.0f) * 127.0f);
    if (converted < 0) converted = 0;
    if (converted > 255) converted = 255;
    return (uint8_t)converted;
}

static float process_mouse_axis(float delta, const K4pSettings *settings) {
    float sensitivity = clampf(settings->mouse_sensitivity, 0.0001f, 0.2f);
    float deadzone = clampf(settings->mouse_deadzone, 0.0f, 0.95f);
    float acceleration = clampf(settings->mouse_acceleration, 0.0f, 2.0f);
    float value = clampf(delta * sensitivity, -1.0f, 1.0f);
    value *= 1.0f + acceleration * fminf(1.0f, fabsf(value));

    if (fabsf(value) <= deadzone) return 0.0f;
    if (deadzone > 0.0f) {
        float scaled = (fabsf(value) - deadzone) / (1.0f - deadzone);
        value = copysignf(scaled, value);
    }
    return clampf(value, -1.0f, 1.0f);
}

void k4p_map_mouse(const K4pMouseReport *mouse, const K4pSettings *settings,
                   K4pPadState *out) {
    K4pSettings defaults;
    if (!out) return;
    k4p_pad_state_neutral(out);
    if (!mouse) return;
    if (!settings) {
        k4p_settings_default(&defaults);
        settings = &defaults;
    }

    /* Mouse left click -> R2; right click -> L2; middle -> touchpad click. */
    if (mouse->buttons & 0x01u) {
        out->buttons |= K4P_BTN_R2;
        out->r2 = 255;
    }
    if (mouse->buttons & 0x02u) {
        out->buttons |= K4P_BTN_L2;
        out->l2 = 255;
    }
    if (mouse->buttons & 0x04u) out->buttons |= K4P_BTN_TOUCHPAD;

    /*
     * Mouse reports carry deltas, not absolute stick positions. Only override
     * an axis when that report contains movement on the axis; a zero-delta
     * report must allow the physical DS4 stick to pass through unchanged.
     */
    if (mouse->x != 0) {
        out->axis_override |= K4P_AXIS_RX;
        out->rx = axis_from_float(process_mouse_axis((float)mouse->x, settings));
    }
    if (mouse->y != 0) {
        out->axis_override |= K4P_AXIS_RY;
        out->ry = axis_from_float(process_mouse_axis((float)mouse->y, settings));
    }
}

void k4p_merge_pad_state(K4pPadState *physical, const K4pPadState *mapped) {
    if (!physical || !mapped) return;
    physical->buttons |= mapped->buttons;
    if (mapped->axis_override & K4P_AXIS_LX) physical->lx = mapped->lx;
    if (mapped->axis_override & K4P_AXIS_LY) physical->ly = mapped->ly;
    if (mapped->axis_override & K4P_AXIS_RX) physical->rx = mapped->rx;
    if (mapped->axis_override & K4P_AXIS_RY) physical->ry = mapped->ry;
    if (mapped->l2 > physical->l2) physical->l2 = mapped->l2;
    if (mapped->r2 > physical->r2) physical->r2 = mapped->r2;
}

