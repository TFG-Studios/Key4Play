#ifndef KEY4PLAY_PAD_HOOK_H
#define KEY4PLAY_PAD_HOOK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Install/remove title-process hooks for scePadRead and scePadReadState. */
int k4p_pad_hook_start(void);
void k4p_pad_hook_stop(void);

#ifdef __cplusplus
}
#endif

#endif
