/* Plays what the game sends to its audio channels.
 *
 * The game does its own decoding and mixing, then hands finished PCM to a few
 * channels. Each channel gets a queue here, and a thread adds the queues together
 * and feeds them to the sound card with waveOut, which needs no extra libraries.
 *
 * Latency is the point to watch in a rhythm game. The buffers below add about 46 ms.
 * A queue grows by itself: the card's clock is not the computer's, so the game hands
 * over a little more than is played, and whatever time a queue spends empty stays
 * added to it for good. One measured run reached 186 ms in 14 minutes. So a queue past
 * QUEUE_TARGET plays one frame more in each buffer, a change of 0.2% that cannot be
 * heard. That takes many seconds to undo a moment the game was held up, as when a
 * controller is plugged in or a song loads, and meanwhile the sound comes late to the
 * notes. The game hands sound over in bursts, so how long a queue is at one moment says
 * little; the shortest it has been over half a second says what stays. When even that is
 * well past the target, what stays past it is cut at once, but only while a song plays:
 * there the queue holds steady and only a hold-up makes it long, while the title and its
 * demo are heavier, keep it long all the time, and would cut often for nothing, as late
 * sound does not matter there. A queue that runs dry waits for its preroll again: one
 * longer gap sounds better than many short. */

#include "audio_out.h"

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define RATE          44100
#define SLOTS         16
#define QUEUE_FRAMES  44100
#define QUEUE_TARGET  1024     /* past this a queue is played off a little faster */
#define QUEUE_TRIM    (QUEUE_TARGET * 3 / 2)   /* past this all the time, it is cut back at once */
#define TRIM_LOOKS    43       /* buffers looked over for the shortest a queue has been: half a second */
#define PREROLL       1024     /* hold a queue back until it has this much */
#define BUFFERS       4
#define BUFFER_FRAMES 512

typedef struct {
    int16_t pcm[QUEUE_FRAMES * 2];
    uint32_t head, count;
    int primed;
    uint32_t shortest, looks;   /* the shortest it has been over the last `looks` buffers */
} queue_t;

static queue_t *g_queue;
static CRITICAL_SECTION g_lock;
static volatile LONGLONG g_underruns;
static volatile LONGLONG g_trims;
static volatile LONGLONG g_queued_sum, g_queued_count, g_card_sum, g_card_count;
static volatile LONG g_queued_now;
static volatile ULONGLONG g_song_seen_ms;   /* when the game last said a song was on */
#define SONG_GONE_MS 500

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
    LeaveCriticalSection(&g_lock);
}

static void mix(int16_t *out, uint32_t frames) {
    static int32_t sum[BUFFER_FRAMES * 2];
    int short_of_data = 0;
    uint32_t longest = 0;
    memset(sum, 0, sizeof(int32_t) * frames * 2);

    EnterCriticalSection(&g_lock);
    for (int s = 0; s < SLOTS; s++) {
        queue_t *q = &g_queue[s];
        if (!q->primed) {
            if (q->count < PREROLL) continue;
            q->primed = 1;
        }
        if (!q->looks || q->count < q->shortest) q->shortest = q->count;
        if (++q->looks >= TRIM_LOOKS) {
            if (q->shortest > QUEUE_TRIM && GetTickCount64() - g_song_seen_ms < SONG_GONE_MS) {
                queue_drop(q, q->shortest - QUEUE_TARGET);
                InterlockedIncrement64(&g_trims);
            }
            q->looks = 0;
        }
        if (q->count > longest) longest = q->count;
        const uint32_t take = q->count < frames ? q->count : frames;
        if (take < frames) {
            short_of_data = 1;
            q->primed = 0;
        }
        /* The `used` frames go smoothly into the `take` to play, and a frame more when it is long. */
        const uint32_t used = take == frames && q->count > QUEUE_TARGET ? frames + 1 : take;
        for (uint32_t i = 0; i < take; i++) {
            const uint32_t place = i * (used - 1) * 256 / (take > 1 ? take - 1 : 1);
            const uint32_t next = place >> 8, part = place & 255;
            const uint32_t at = (q->head + next) % QUEUE_FRAMES, after = (at + (next + 1 < used)) % QUEUE_FRAMES;
            sum[i * 2] += (q->pcm[at * 2] * (int32_t)(256 - part) + q->pcm[after * 2] * (int32_t)part) >> 8;
            sum[i * 2 + 1] += (q->pcm[at * 2 + 1] * (int32_t)(256 - part) + q->pcm[after * 2 + 1] * (int32_t)part) >> 8;
        }
        queue_drop(q, used);
    }
    LeaveCriticalSection(&g_lock);

    if (short_of_data) InterlockedIncrement64(&g_underruns);
    if (longest) {
        g_queued_sum += longest;
        g_queued_count++;
    }
    InterlockedExchange(&g_queued_now, (LONG)longest);
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

    uint64_t written = 0;
    for (;;) {
        for (int i = 0; i < BUFFERS; i++) {
            if (!(hdr[i].dwFlags & WHDR_DONE)) continue;
            mix(pcm[i], BUFFER_FRAMES);
            hdr[i].dwFlags &= ~(DWORD)WHDR_DONE;
            waveOutWrite(dev, &hdr[i], sizeof hdr[i]);
            written += BUFFER_FRAMES;
        }
        MMTIME played;
        played.wType = TIME_SAMPLES;
        if (waveOutGetPosition(dev, &played, sizeof played) == MMSYSERR_NOERROR && played.wType == TIME_SAMPLES) {
            g_card_sum += (LONGLONG)(written - played.u.sample);
            g_card_count++;
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
double audio_out_queued_now(void) { return (double)g_queued_now * 1000.0 / RATE; }
void audio_out_song_frame(void) { g_song_seen_ms = GetTickCount64(); }
uint64_t audio_out_trims(void) { return (uint64_t)g_trims; }

void audio_out_waits(double *queued, double *at_card) {
    *queued = g_queued_count ? (double)g_queued_sum / (double)g_queued_count * 1000.0 / RATE : 0;
    *at_card = g_card_count ? (double)g_card_sum / (double)g_card_count * 1000.0 / RATE : 0;
}
