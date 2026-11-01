#ifndef KEY4PLAY_PAD_JUMP_PLAN_H
#define KEY4PLAY_PAD_JUMP_PLAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct K4pPadJumpPlan {
    uint32_t original_delta;
    uint32_t replacement_delta;
    uintptr_t original_target;
} K4pPadJumpPlan;

/*
 * Validate the known PS4 Pad forwarding-stub signature and calculate a
 * replacement rel32 displacement. This function never writes to the target.
 */
int k4p_pad_plan_jump(const uint8_t *entry, uintptr_t entry_address,
                      uintptr_t replacement, uint8_t zero_register_opcode,
                      K4pPadJumpPlan *plan);

#ifdef __cplusplus
}
#endif

#endif
