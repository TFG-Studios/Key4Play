#include "key4play/input_core.hpp"

#include <algorithm>
#include <cmath>

namespace key4play {

float clamp_unit(float value) {
    return std::clamp(value, -1.0F, 1.0F);
}

Stick apply_deadzone(Stick stick, float deadzone) {
    const float dz = std::clamp(deadzone, 0.0F, 0.95F);
    const float magnitude = std::sqrt(stick.x * stick.x + stick.y * stick.y);
    if (magnitude <= dz || magnitude == 0.0F) {
        return {};
    }
    const float scaled = std::min(1.0F, (magnitude - dz) / (1.0F - dz));
    const float factor = scaled / magnitude;
    return {clamp_unit(stick.x * factor), clamp_unit(stick.y * factor)};
}

Stick mouse_delta_to_stick(float dx, float dy, const Settings& settings) {
    const float sensitivity = std::clamp(settings.mouse_sensitivity, 0.0001F, 0.2F);
    const float raw_x = dx * sensitivity;
    const float raw_y = dy * sensitivity;

    auto accelerate = [&settings](float value) {
        const float amount = std::clamp(settings.acceleration, 0.0F, 2.0F);
        return value * (1.0F + amount * std::min(1.0F, std::abs(value)));
    };

    Stick result{clamp_unit(accelerate(raw_x)), clamp_unit(accelerate(raw_y))};
    return apply_deadzone(result, settings.deadzone);
}

InputFrame make_frame(bool up, bool down, bool left, bool right,
                      float mouse_dx, float mouse_dy,
                      const Settings& settings) {
    Stick movement{
        (right ? 1.0F : 0.0F) - (left ? 1.0F : 0.0F),
        (down ? 1.0F : 0.0F) - (up ? 1.0F : 0.0F)
    };

    const float magnitude = std::sqrt(movement.x * movement.x + movement.y * movement.y);
    if (magnitude > 1.0F) {
        movement.x /= magnitude;
        movement.y /= magnitude;
    }

    return {movement, mouse_delta_to_stick(mouse_dx, mouse_dy, settings)};
}

} // namespace key4play