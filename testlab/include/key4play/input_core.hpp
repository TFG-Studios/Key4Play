#pragma once

namespace key4play {

struct Stick {
    float x{0.0F};
    float y{0.0F};
};

struct Settings {
    float mouse_sensitivity{0.012F};
    float deadzone{0.08F};
    float smoothing{0.0F};
    float acceleration{0.0F};
};

struct InputFrame {
    Stick left;
    Stick right;
};

float clamp_unit(float value);
Stick apply_deadzone(Stick stick, float deadzone);
Stick mouse_delta_to_stick(float dx, float dy, const Settings& settings);
InputFrame make_frame(bool up, bool down, bool left, bool right,
                      float mouse_dx, float mouse_dy,
                      const Settings& settings);

} // namespace key4play