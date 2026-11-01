#include "key4play/pad_hook.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <orbis/Pad.h>
#include <orbis/libkernel.h>

#include "key4play/input_snapshot.h"
#include "key4play/pad_jump_plan.h"

extern K4pInputSnapshot g_k4p_input_snapshot;

_Static_assert(offsetof(OrbisPadData, buttons) == 0u, "OrbisPadData.buttons ABI");
_Static_assert(offsetof(OrbisPadData, leftStick) == 4u, "OrbisPadData.leftStick ABI");
_Static_assert(offsetof(OrbisPadData, rightStick) == 6u, "OrbisPadData.rightStick ABI");
_Static_assert(offsetof(OrbisPadData, analogButtons) == 8u, "OrbisPadData.analogButtons ABI");
_Static_assert(offsetof(OrbisPadData, connected) == 76u, "OrbisPadData.connected ABI");
_Static_assert(sizeof(OrbisPadData) == 120u, "OrbisPadData size ABI");

/*
 * The exported Pad entry points on known PS4 builds may be short forwarding
 * stubs: XOR an ABI argument to zero, then JMP rel32 to the implementation.
 * We only patch this exact, verified shape. If a firmware/build presents a
 * different prologue, Key4Play refuses to install the hook rather than
 * copying/relocating unknown instructions.
 */
typedef struct K4pJumpHook {
    uint8_t *entry;
    uint8_t zero_register_opcode;
    uint32_t original_delta;
    uint32_t replacement_delta;
    void *original_target;
    int live;
} K4pJumpHook;

typedef int32_t (*K4pPadReadOriginalFn)(int32_t handle, OrbisPadData *data,
                                        int32_t count, int32_t flags);
typedef int32_t (*K4pPadReadStateOriginalFn)(int32_t handle,
                                             OrbisPadData *data,
                                             int32_t flags);

static K4pJumpHook g_read_hook;
static K4pJumpHook g_read_state_hook;
static volatile int32_t g_target_handle = -1;

static int compare_exchange_u32_unaligned(uint8_t *address,
                                          uint32_t expected,
                                          uint32_t desired) {
    unsigned char success;
    __asm__ volatile("lock cmpxchgl %3, %1; sete %0"
        : "=q"(success), "+m"(*(volatile uint8_t (*)[4])address),
          "+a"(expected)
        : "r"(desired)
        : "memory", "cc");
    return success != 0;
}

static int prepare_hook(K4pJumpHook *hook, void *entry_pointer,
                        void *replacement, uint8_t zero_register_opcode) {
    K4pPadJumpPlan plan;
    int result;

    if (!hook || !entry_pointer || !replacement) return -1;
    result = k4p_pad_plan_jump((const uint8_t *)entry_pointer,
                               (uintptr_t)entry_pointer,
                               (uintptr_t)replacement,
                               zero_register_opcode, &plan);
    if (result != 0) return result;

    hook->entry = (uint8_t *)entry_pointer;
    hook->zero_register_opcode = zero_register_opcode;
    hook->original_delta = plan.original_delta;
    hook->replacement_delta = plan.replacement_delta;
    hook->original_target = (void *)plan.original_target;
    hook->live = 0;
    return 0;
}

static int switch_hook(K4pJumpHook *hook, int enable) {
    uintptr_t page;
    uint32_t expected;
    uint32_t desired;
    int result;
    int changed;

    if (!hook) return -1;
    if (!hook->entry) return enable ? -1 : 0;
    if (!!hook->live == !!enable) return 0;

    if (hook->entry[0] != 0x31u ||
        hook->entry[1] != hook->zero_register_opcode ||
        hook->entry[2] != 0xe9u) return -2;
    {
        unsigned i;
        for (i = 7u; i < 16u; ++i) {
            if (hook->entry[i] != 0x90u) return -10;
        }
    }

    expected = enable ? hook->original_delta : hook->replacement_delta;
    desired = enable ? hook->replacement_delta : hook->original_delta;

    page = (uintptr_t)hook->entry & ~(uintptr_t)0x3fffu;
    result = sceKernelMprotect((void *)page, 0x4000u, 7u);
    if (result != 0) return result;

    changed = compare_exchange_u32_unaligned(hook->entry + 3u,
                                              expected, desired);
    result = sceKernelMprotect((void *)page, 0x4000u, 5u);
    if (result != 0) {
        int retry = sceKernelMprotect((void *)page, 0x4000u, 5u);
        if (retry != 0)
            printf("[Key4Play] Warning: code-page protection restore failed: %d / %d\n",
                   result, retry);
    }

    if (!changed) return -9;
    hook->live = enable;
    return result;
}

static void overlay_input(int32_t handle, OrbisPadData *sample) {
    K4pInputFrame frame;
    K4pPadState mapped;
    int32_t expected = -1;

    if (!sample) return;
    if (!sample->connected) {
        expected = handle;
        __atomic_compare_exchange_n(&g_target_handle, &expected, -1, 0,
                                    __ATOMIC_RELAXED, __ATOMIC_RELAXED);
        return;
    }

    __atomic_compare_exchange_n(&g_target_handle, &expected, handle, 0,
                                __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    if (__atomic_load_n(&g_target_handle, __ATOMIC_RELAXED) != handle)
        return;

    /* A busy snapshot skips this poll instead of stalling the game thread. */
    if (!k4p_input_snapshot_read(&g_k4p_input_snapshot, &frame)) return;
    k4p_input_snapshot_compose(&frame, &mapped);

    sample->buttons |= mapped.buttons;
    if (mapped.axis_override & K4P_AXIS_LX) sample->leftStick.x = mapped.lx;
    if (mapped.axis_override & K4P_AXIS_LY) sample->leftStick.y = mapped.ly;
    if (mapped.axis_override & K4P_AXIS_RX) sample->rightStick.x = mapped.rx;
    if (mapped.axis_override & K4P_AXIS_RY) sample->rightStick.y = mapped.ry;

    if (mapped.l2 > sample->analogButtons.l2)
        sample->analogButtons.l2 = mapped.l2;
    if (mapped.r2 > sample->analogButtons.r2)
        sample->analogButtons.r2 = mapped.r2;
}

static int32_t k4p_scePadRead_hook(int32_t handle, OrbisPadData *data,
                                   int32_t count) {
    K4pPadReadOriginalFn original =
        (K4pPadReadOriginalFn)g_read_hook.original_target;
    int32_t result;
    int32_t i;

    if (!original) return -1;
    result = original(handle, data, count, 0);
    if (result <= 0 || !data || count <= 0 || result > count) return result;

    for (i = 0; i < result; ++i) overlay_input(handle, &data[i]);
    return result;
}

static int32_t k4p_scePadReadState_hook(int32_t handle, OrbisPadData *data) {
    K4pPadReadStateOriginalFn original =
        (K4pPadReadStateOriginalFn)g_read_state_hook.original_target;
    int32_t result;

    if (!original) return -1;
    result = original(handle, data, 0);
    if (result == 0 && data) overlay_input(handle, data);
    return result;
}

int k4p_pad_hook_start(void *read_target, void *read_state_target) {
    int result;

    if (g_read_hook.live && g_read_state_hook.live) return 0;
    if (!read_target || !read_state_target || read_target == read_state_target)
        return -1;

    __atomic_store_n(&g_target_handle, -1, __ATOMIC_RELAXED);

    result = prepare_hook(&g_read_hook, read_target,
                          (void *)(uintptr_t)k4p_scePadRead_hook, 0xc9u);
    if (result != 0) {
        printf("[Key4Play] scePadRead entry signature rejected: %d\n", result);
        return result;
    }

    result = prepare_hook(&g_read_state_hook, read_state_target,
                          (void *)(uintptr_t)k4p_scePadReadState_hook, 0xd2u);
    if (result != 0) {
        printf("[Key4Play] scePadReadState entry signature rejected: %d\n", result);
        return result;
    }

    result = switch_hook(&g_read_hook, 1);
    if (result != 0) {
        int rollback = g_read_hook.live ? switch_hook(&g_read_hook, 0) : 0;
        printf("[Key4Play] scePadRead hook activation failed: %d (rollback=%d)\n",
               result, rollback);
        return g_read_hook.live ? 1 : result;
    }

    result = switch_hook(&g_read_state_hook, 1);
    if (result != 0) {
        int rollback_state = g_read_state_hook.live
            ? switch_hook(&g_read_state_hook, 0) : 0;
        int rollback_read = g_read_hook.live ? switch_hook(&g_read_hook, 0) : 0;
        printf("[Key4Play] scePadReadState hook activation failed: %d (state=%d read=%d)\n",
               result, rollback_state, rollback_read);
        return (g_read_hook.live || g_read_state_hook.live) ? 1 : result;
    }

    printf("[Key4Play] Signature-checked Pad hooks installed for one DS4 handle\n");
    return 0;
}

int k4p_pad_hook_stop(void) {
    int state_result = switch_hook(&g_read_state_hook, 0);
    int read_result = switch_hook(&g_read_hook, 0);

    if (state_result != 0 || read_result != 0) {
        printf("[Key4Play] Hook restore failed (state=%d read=%d); refusing normal teardown\n",
               state_result, read_result);
        return -1;
    }

    __atomic_store_n(&g_target_handle, -1, __ATOMIC_RELAXED);
    printf("[Key4Play] Signature-checked Pad hooks restored\n");
    return 0;
}
