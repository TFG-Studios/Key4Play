#include "key4play/pad_hook.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <orbis/Pad.h>
#include <orbis/libkernel.h>

#include "Detour.h"
#include "key4play/input_snapshot.h"

extern K4pInputSnapshot g_k4p_input_snapshot;

/*
 * OpenOrbis OrbisPadData ABI checks. The hook must not modify a struct whose
 * layout differs from the target ABI; fail the build rather than guessing.
 */
_Static_assert(offsetof(OrbisPadData, buttons) == 0u, "OrbisPadData.buttons ABI");
_Static_assert(offsetof(OrbisPadData, leftStick) == 4u, "OrbisPadData.leftStick ABI");
_Static_assert(offsetof(OrbisPadData, rightStick) == 6u, "OrbisPadData.rightStick ABI");
_Static_assert(offsetof(OrbisPadData, analogButtons) == 8u, "OrbisPadData.analogButtons ABI");
_Static_assert(sizeof(OrbisPadData) == 120u, "OrbisPadData size ABI");

typedef int32_t (*K4pPadReadFn)(int32_t handle, OrbisPadData *data,
                                int32_t count);
typedef int32_t (*K4pPadReadStateFn)(int32_t handle, OrbisPadData *data);

static Detour g_read_detour;
static Detour g_read_state_detour;
static int g_read_hook_installed;
static int g_read_state_hook_installed;

static void overlay_input(OrbisPadData *sample) {
    K4pInputFrame frame;
    K4pPadState mapped;

    if (!sample) return;
    if (!k4p_input_snapshot_read(&g_k4p_input_snapshot, &frame)) return;
    k4p_input_snapshot_compose(&frame, &mapped);

    /* Preserve every physical DS4 field not explicitly controlled by K4P. */
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
    K4pPadReadFn original = (K4pPadReadFn)g_read_detour.StubPtr;
    int32_t result;
    int32_t i;

    if (!original) return -1;
    result = original(handle, data, count);
    if (result <= 0 || !data || count <= 0 || result > count) return result;

    for (i = 0; i < result; ++i) overlay_input(&data[i]);
    return result;
}

static int32_t k4p_scePadReadState_hook(int32_t handle, OrbisPadData *data) {
    K4pPadReadStateFn original =
        (K4pPadReadStateFn)g_read_state_detour.StubPtr;
    int32_t result;

    if (!original) return -1;
    result = original(handle, data);
    /* scePadReadState uses zero for a successful sample in the SDK example. */
    if (result == 0 && data) overlay_input(data);
    return result;
}

static void restore_detour(Detour *detour) {
    uint64_t current_jump = 0;
    int owns_entry_patch = 0;
    int had_stub;

    if (!detour) return;
    had_stub = detour->StubPtr != 0;

    if (had_stub) {
        /*
         * GoldHEN's x64 restore path copies exactly the decoded instruction
         * span. Its x32 path computes an unsafe restore length for this
         * 64-bit target, so these hooks must stay in x64 mode.
         */
        if (detour->FunctionPtr) {
            const uint8_t *entry = (const uint8_t *)detour->FunctionPtr;
            if (entry[0] == 0xffu && entry[1] == 0x25u) {
                memcpy(&current_jump, entry + 6, sizeof(current_jump));
                owns_entry_patch = current_jump == (uint64_t)(uintptr_t)detour->TrampolinePtr;
            }
        }

        Detour_RestoreFunction(detour);
        sceKernelMunmap(detour->StubPtr, detour->StubSize);
        detour->StubPtr = 0;
        detour->StubSize = 0;
    }

    /*
     * If another plugin chained a detour after ours, GoldHEN restores our
     * original instructions into our trampoline so that the later plugin's
     * trampoline can still call through it. Keep that mapping alive.
     */
    if (detour->TrampolinePtr && (!had_stub || owns_entry_patch))
        sceKernelMunmap(detour->TrampolinePtr,
                        sizeof(detour->JumpInstructions64));

    detour->TrampolinePtr = 0;
}

int k4p_pad_hook_start(void *read_target, void *read_state_target) {
    void *stub;

    if (g_read_hook_installed && g_read_state_hook_installed) return 0;
    if (!read_target || !read_state_target || read_target == read_state_target) {
        printf("[Key4Play] Invalid or overlapping Pad export addresses\n");
        return -1;
    }

    Detour_Construct(&g_read_detour, DetourMode_x64);
    stub = Detour_DetourFunction(&g_read_detour,
                                (uint64_t)(uintptr_t)read_target,
                                (void *)(uintptr_t)k4p_scePadRead_hook);
    if (!stub) {
        printf("[Key4Play] scePadRead detour installation failed\n");
        restore_detour(&g_read_detour);
        return -1;
    }
    g_read_hook_installed = 1;

    Detour_Construct(&g_read_state_detour, DetourMode_x64);
    stub = Detour_DetourFunction(&g_read_state_detour,
                                (uint64_t)(uintptr_t)read_state_target,
                                (void *)(uintptr_t)k4p_scePadReadState_hook);
    if (!stub) {
        printf("[Key4Play] scePadReadState detour installation failed; rolling back\n");
        restore_detour(&g_read_state_detour);
        restore_detour(&g_read_detour);
        g_read_hook_installed = 0;
        return -1;
    }
    g_read_state_hook_installed = 1;

    printf("[Key4Play] Controller overlay hooks installed (DS4 sample preserved)\n");
    return 0;
}

void k4p_pad_hook_stop(void) {
    /*
     * GoldHEN unload should occur at title shutdown, not while game threads
     * are actively polling. Detours are restored before their trampolines
     * are released, following the upstream GoldHEN plugin lifecycle pattern.
     */
    if (g_read_state_hook_installed) {
        restore_detour(&g_read_state_detour);
        g_read_state_hook_installed = 0;
    }
    if (g_read_hook_installed) {
        restore_detour(&g_read_detour);
        g_read_hook_installed = 0;
    }
    printf("[Key4Play] Controller overlay hooks removed\n");
}
