#include "key4play/input_core.hpp"

#include <cassert>
#include <cmath>

int main() {
    using namespace key4play;

    assert(clamp_unit(2.0F) == 1.0F);
    assert(clamp_unit(-2.0F) == -1.0F);

    const Stick stopped = apply_deadzone({0.02F, 0.01F}, 0.1F);
    assert(stopped.x == 0.0F && stopped.y == 0.0F);

    const Stick active = apply_deadzone({0.5F, 0.0F}, 0.1F);
    assert(active.x > 0.0F && active.x <= 1.0F);

    Settings settings;
    settings.mouse_sensitivity = 0.01F;
    settings.deadzone = 0.0F;
    const Stick mouse = mouse_delta_to_stick(20.0F, -10.0F, settings);
    assert(std::abs(mouse.x - 0.2F) < 0.0001F);
    assert(std::abs(mouse.y + 0.1F) < 0.0001F);

    const InputFrame frame = make_frame(true, false, false, true, 0, 0, settings);
    assert(frame.left.x > 0.0F);
    assert(frame.left.y < 0.0F);
    return 0;
}