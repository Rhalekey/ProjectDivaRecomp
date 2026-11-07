#ifndef DIVA_AUDIO_OUT_H
#define DIVA_AUDIO_OUT_H

#include <stdint.h>

void audio_out_start(void);

/* Queue PCM for one channel. Fits psp_audio_set_sink; volumes are 0x8000 at full. */
void audio_out_submit(int slot, const int16_t *samples, uint32_t frames, int stereo, uint32_t vl, uint32_t vr);

/* Buffers that ran short of data: audible as gaps, and a sign the game is behind. */
uint64_t audio_out_underruns(void);
/* How many times queued sound was thrown away because the game had got ahead of the card. */
uint64_t audio_out_trims(void);
/* In ms on average, how long sound waits in the queues here and at the sound card. */
void audio_out_waits(double *queued, double *at_card);

#endif
