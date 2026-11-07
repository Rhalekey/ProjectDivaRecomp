/* Plays what the game sends to its audio channels.
 *
 * The game does its own decoding and mixing, then hands finished PCM to a few
 * channels. Each channel gets a queue here, and a thread adds the queues together
 * and feeds them to the sound card with waveOut, which needs no extra libraries.
 *
 * Latency is the point to watch in a rhythm game. The buffers below add about
 * 46 ms and the preroll about 35 ms more, and a queue that drifts ahead of the
 * card is trimmed rather than left to delay everything after it. A queue that runs
 * dry waits for its preroll again: one longer gap sounds better than many short. */

#include "audio_out.h"

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define RATE          44100
#define SLOTS         16
#define QUEUE_FRAMES  44100
#define MAX_QUEUED    8192     /* past this a queue has drifted: trim it */
#define KEEP_QUEUED   2048
#define PREROLL       1536     /* hold a queue back until it has this much */
#define BUFFERS       4
#define BUFFER_FRAMES 512

typedef struct {
    int16_t pcm[QUEUE_FRAMES * 2];
    uint32_t head, count;
    int primed;
} queue_t;

static queue_t *g_queue;
static CRITICAL_SECTION g_lock;
static volatile LONGLONG g_underruns;
static volatile LONGLONG g_trims;

static void queue_drop(queue_t *q, uint32_t frames) {
    q->head = (q->head + frames) % QUEUE_FRAMES;
    q->count -= frames;
}

void audio_out_submit(int slot, const int16_t *samples, uint32_t frames, int stereo, uint32_t vl, uint32_t vr) {
    if (!g_queue || slot < 0 || slot >= SLOTS) return;
    queue_t *q = &g_queue[slot];
    EnterCriticalSection(&g_lock);
    for (uint32_t i = 0; i < frames; i++) {
        const int32_t l = stereo ? samples[i * 2] : samples[i];
        const int32_t r = stereo ? samples[i * 2 + 1] : samples[i];
        if (q->count == QUEUE_FRAMES) queue_drop(q, 1);
        const uint32_t at = (q->head + q->count) % QUEUE_FRAMES;
        q->pcm[at * 2] = (int16_t)((l * (int32_t)vl) >> 15);
        q->pcm[at * 2 + 1] = (int16_t)((r * (int32_t)vr) >> 15);
        q->count++;
    }
    if (q->count > MAX_QUEUED) {
        queue_drop(q, q->count - KEEP_QUEUED);
        InterlockedIncrement64(&g_trims);
    }
    LeaveCriticalSection(&g_lock);
}

static void mix(int16_t *out, uint32_t frames) {
    static int32_t sum[BUFFER_FRAMES * 2];
    int short_of_data = 0;
    memset(sum, 0, sizeof(int32_t) * frames * 2);

    EnterCriticalSection(&g_lock);
    for (int s = 0; s < SLOTS; s++) {
        queue_t *q = &g_queue[s];
        if (!q->primed) {
            if (q->count < PREROLL) continue;
            q->primed = 1;
        }
        const uint32_t take = q->count < frames ? q->count : frames;
        if (take < frames) {
            short_of_data = 1;
            q->primed = 0;
        }
        for (uint32_t i = 0; i < take; i++) {
            const uint32_t at = (q->head + i) % QUEUE_FRAMES;
            sum[i * 2] += q->pcm[at * 2];
            sum[i * 2 + 1] += q->pcm[at * 2 + 1];
        }
        queue_drop(q, take);
    }
    LeaveCriticalSection(&g_lock);

    if (short_of_data) InterlockedIncrement64(&g_underruns);
    for (uint32_t i = 0; i < frames * 2; i++) {
        const int32_t v = sum[i];
        out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
}

static unsigned __stdcall audio_thread(void *arg) {
    HANDLE ready = (HANDLE)arg;
    WAVEFORMATEX fmt = { 0 };
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = RATE;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = RATE * 4;

    HWAVEOUT dev;
    if (waveOutOpen(&dev, WAVE_MAPPER, &fmt, (DWORD_PTR)ready, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        fprintf(stderr, "audio: could not open the sound device; continuing without sound\n");
        return 0;
    }

    static int16_t pcm[BUFFERS][BUFFER_FRAMES * 2];
    static WAVEHDR hdr[BUFFERS];
    for (int i = 0; i < BUFFERS; i++) {
        hdr[i].lpData = (LPSTR)pcm[i];
        hdr[i].dwBufferLength = BUFFER_FRAMES * 4;
        waveOutPrepareHeader(dev, &hdr[i], sizeof hdr[i]);
        hdr[i].dwFlags |= WHDR_DONE;
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    for (;;) {
        for (int i = 0; i < BUFFERS; i++) {
            if (!(hdr[i].dwFlags & WHDR_DONE)) continue;
            mix(pcm[i], BUFFER_FRAMES);
            hdr[i].dwFlags &= ~(DWORD)WHDR_DONE;
            waveOutWrite(dev, &hdr[i], sizeof hdr[i]);
        }
        WaitForSingleObject(ready, 20);
    }
}

void audio_out_start(void) {
    g_queue = (queue_t *)calloc(SLOTS, sizeof *g_queue);
    if (!g_queue) return;
    InitializeCriticalSection(&g_lock);
    HANDLE ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    _beginthreadex(NULL, 0, audio_thread, ready, 0, NULL);
}

uint64_t audio_out_underruns(void) { return (uint64_t)g_underruns; }
uint64_t audio_out_trims(void) { return (uint64_t)g_trims; }
