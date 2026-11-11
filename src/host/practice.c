/* Practice: a song that cannot be failed and scores nothing. See practice.h. */
#include "practice.h"

#include "language.h"
#include "options.h"
#include "settings.h"
#include "ui_text.h"

#include <psprecomp/cpu.h>
#include <psprecomp/mem.h>
#include <psprecomp/render.h>

#include <stdio.h>

/* Each note's points go through 0886AF54 (song, points, kept up, ...), which adds them and the
 * combo's bonus to the song's score. Both ways through it then reach 0886B08C, with the song in s3. */
#define SCORE_ADDED 0x0886B08Cu
#define SONG_SCORE (0x10000u - 17428u)

static int g_on;

/* The song's sound is set a layer at a time by 08AB36B0 (the sound, the layer, the volume from 0 to
 * 100); layer 1 is the singing. A missed note sets it to 0 from these three places, the next hit
 * back to 100; a pause or the song's end sets both layers itself, elsewhere, and is left alone.
 * With Mute on miss off in Options, those three ask for 100 instead. */
#define SET_LAYER_VOLUME 0x08AB36B0u
#define VOCAL_LAYER 1u
static const uint32_t MISS_SILENCES[] = {0x088664D4u, 0x08867004u, 0x08856AB8u};

static void on_layer_volume(void) {
    if (!g_settings.extras || !g_options.keep_vocal) return;
    if (psp_cpu.r[PSP_REG_A1] != VOCAL_LAYER || psp_cpu.r[PSP_REG_A2] != 0) return;
    static int told;
    for (size_t i = 0; i < sizeof MISS_SILENCES / sizeof MISS_SILENCES[0]; i++) {
        if (psp_cpu.r[PSP_REG_RA] != MISS_SILENCES[i]) continue;
        psp_cpu.r[PSP_REG_A2] = 100;
        if (!told++) fprintf(stderr, "vocal: a miss leaves the singing on (from %08X)\n", MISS_SILENCES[i]);
    }
}


void practice_toggle(void) {
    g_on = !g_on;
    psp_render_gl_show_message(ui_text(g_on ? "practice.on" : "practice.off"));
    fprintf(stderr, "practice: %s\n", g_on ? "on" : "off");
}

int practice_on(void) { return g_on; }

int practice_hook(uint32_t addr) {
    if (addr == SET_LAYER_VOLUME) {
        on_layer_volume();
        return 1;
    }
    if (addr == SCORE_ADDED) {
        if (g_on) psp_write32(psp_cpu.r[PSP_REG_S3] + SONG_SCORE, 0);
        return 1;
    }
    return 0;
}
