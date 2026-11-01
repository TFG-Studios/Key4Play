#ifndef KEY4PLAY_INPUT_SNAPSHOT_H
#define KEY4PLAY_INPUT_SNAPSHOT_H

#include <stdint.h>

#include "key4play/input_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A small lock-protected handoff between independent device-reader threads
 * and the future gamepad integration path. Callers must initialize the
 * snapshot before publishing or reading it.
 */
typedef struct K4pInputSnapshot {
    volatile long lock_word;
    K4pPadState keyboard;
    K4pPadState mouse;
    uint32_t generation;
    uint8_t keyboard_connected;
    uint8_t mouse_connected;
} K4pInputSnapshot;

typedef struct K4pInputFrame {
    K4pPadState keyboard;
    K4pPadState mouse;
    uint32_t generation;
    uint8_t keyboard_connected;
    uint8_t mouse_connected;
} K4pInputFrame;

void k4p_input_snapshot_init(K4pInputSnapshot *snapshot);
void k4p_input_snapshot_publish_keyboard(K4pInputSnapshot *snapshot,
                                         const K4pPadState *state,
                                         int connected);
void k4p_input_snapshot_publish_mouse(K4pInputSnapshot *snapshot,
                                      const K4pPadState *state,
                                      int connected);
int k4p_input_snapshot_read(K4pInputSnapshot *snapshot, K4pInputFrame *frame);
void k4p_input_snapshot_compose(const K4pInputFrame *frame,
                                K4pPadState *combined);

#ifdef __cplusplus
}
#endif

#endif
