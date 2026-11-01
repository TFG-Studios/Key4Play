#include "key4play/input_snapshot.h"

#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static void snapshot_lock(K4pInputSnapshot *snapshot) {
#if defined(_MSC_VER)
    while (_InterlockedExchange(&snapshot->lock_word, 1L) != 0L) {
        /* Short critical sections; no OS dependency in the shared core. */
    }
#else
    while (__atomic_exchange_n(&snapshot->lock_word, 1L, __ATOMIC_ACQUIRE) != 0L) {
        /* Short critical sections; no OS dependency in the shared core. */
    }
#endif
}

static int snapshot_try_lock(K4pInputSnapshot *snapshot) {
#if defined(_MSC_VER)
    return _InterlockedCompareExchange(&snapshot->lock_word, 1L, 0L) == 0L;
#else
    long expected = 0L;
    return __atomic_compare_exchange_n(&snapshot->lock_word, &expected, 1L,
                                       0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
#endif
}

static void snapshot_unlock(K4pInputSnapshot *snapshot) {
#if defined(_MSC_VER)
    _InterlockedExchange(&snapshot->lock_word, 0L);
#else
    __atomic_store_n(&snapshot->lock_word, 0L, __ATOMIC_RELEASE);
#endif
}

static void publish(K4pInputSnapshot *snapshot, K4pPadState *target,
                    uint8_t *connected_flag, const K4pPadState *state,
                    int connected) {
    if (!snapshot) return;
    snapshot_lock(snapshot);

    if (connected && state) {
        *target = *state;
        *connected_flag = 1u;
    } else {
        k4p_pad_state_neutral(target);
        *connected_flag = 0u;
    }

    ++snapshot->generation;
    snapshot_unlock(snapshot);
}

void k4p_input_snapshot_init(K4pInputSnapshot *snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    k4p_pad_state_neutral(&snapshot->keyboard);
    k4p_pad_state_neutral(&snapshot->mouse);
}

void k4p_input_snapshot_publish_keyboard(K4pInputSnapshot *snapshot,
                                         const K4pPadState *state,
                                         int connected) {
    if (!snapshot) return;
    publish(snapshot, &snapshot->keyboard, &snapshot->keyboard_connected,
            state, connected);
}

void k4p_input_snapshot_publish_mouse(K4pInputSnapshot *snapshot,
                                      const K4pPadState *state,
                                      int connected) {
    if (!snapshot) return;
    publish(snapshot, &snapshot->mouse, &snapshot->mouse_connected,
            state, connected);
}

int k4p_input_snapshot_read(K4pInputSnapshot *snapshot, K4pInputFrame *frame) {
    if (!snapshot || !frame) return 0;
    /* Never spin in the gamepad polling hook; skip one sample if busy. */
    if (!snapshot_try_lock(snapshot)) return 0;
    frame->keyboard = snapshot->keyboard;
    frame->mouse = snapshot->mouse;
    frame->generation = snapshot->generation;
    frame->keyboard_connected = snapshot->keyboard_connected;
    frame->mouse_connected = snapshot->mouse_connected;
    snapshot_unlock(snapshot);
    return 1;
}

void k4p_input_snapshot_compose(const K4pInputFrame *frame,
                                K4pPadState *combined) {
    if (!combined) return;
    k4p_pad_state_neutral(combined);
    if (!frame) return;

    if (frame->keyboard_connected)
        k4p_merge_pad_state(combined, &frame->keyboard);
    if (frame->mouse_connected)
        k4p_merge_pad_state(combined, &frame->mouse);
}
