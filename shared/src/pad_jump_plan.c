#include "key4play/pad_jump_plan.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

int k4p_pad_plan_jump(const uint8_t *entry, uintptr_t entry_address,
                      uintptr_t replacement, uint8_t zero_register_opcode,
                      K4pPadJumpPlan *plan) {
    int32_t original_delta;
    uintptr_t jump_next;
    uintptr_t original_target;
    uint32_t replacement_delta;
    unsigned i;

    if (!entry || !plan || !entry_address || !replacement) return -1;
    if ((entry_address & 7u) != 0u ||
        (entry_address & 0x3fffu) > 0x3ff0u) return -2;
    if (zero_register_opcode != 0xc9u && zero_register_opcode != 0xd2u)
        return -3;

    if (entry[0] != 0x31u || entry[1] != zero_register_opcode ||
        entry[2] != 0xe9u) return -4;
    for (i = 7u; i < 16u; ++i) {
        if (entry[i] != 0x90u) return -5;
    }

    memcpy(&original_delta, entry + 3u, sizeof(original_delta));
    if (original_delta >= -16 || original_delta < -(1 << 20))
        return -6;

    if (entry_address > UINTPTR_MAX - 7u) return -7;
    jump_next = entry_address + 7u;
    if (original_delta < 0) {
        uintptr_t magnitude = (uintptr_t)(-(int64_t)original_delta);
        if (jump_next < magnitude) return -8;
        original_target = jump_next - magnitude;
    } else {
        if (jump_next > UINTPTR_MAX - (uintptr_t)original_delta) return -8;
        original_target = jump_next + (uintptr_t)original_delta;
    }

    if (replacement >= jump_next) {
        uintptr_t distance = replacement - jump_next;
        if (distance > (uintptr_t)INT32_MAX) return -9;
        replacement_delta = (uint32_t)distance;
    } else {
        uintptr_t distance = jump_next - replacement;
        if (distance > (uintptr_t)INT32_MAX + 1u) return -9;
        replacement_delta = (uint32_t)(-(int64_t)distance);
    }

    plan->original_delta = (uint32_t)original_delta;
    plan->replacement_delta = replacement_delta;
    plan->original_target = original_target;
    return 0;
}
