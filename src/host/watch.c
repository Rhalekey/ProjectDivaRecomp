/* A tool for finding out which of the game's code touches some data.
 *
 *   set DIVA_WATCH=08B700F8-08B70690
 *
 * names a range of guest addresses inside one 4 KB page, and every different call
 * stack that reads or writes it goes to the log. It costs nothing unless set, and works on the
 * normal build because it is the host's memory protection that notices the access:
 * the generated code needs no instruments.
 */

#include "watch.h"

#include <psprecomp/cpu.h>
#include <psprecomp/mem.h>

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define MAX_HITS  256
#define MAX_DEPTH 10
#define SAME_FRAMES 4

typedef struct {
    uint32_t guest;
    int write;
    uint32_t stack[MAX_DEPTH];
    uint32_t a[4];
} hit;

static hit g_hits[MAX_HITS];
static volatile LONG g_count;
static LONG g_printed;

static uint8_t *g_page;
static uint32_t g_page_guest;
static DWORD g_page_size;
static uint32_t g_first, g_last;

static void arm(void) {
    DWORD old;
    VirtualProtect(g_page, g_page_size, PAGE_READWRITE | PAGE_GUARD, &old);
}

static void record(uint32_t guest, int write) {
    if (guest < g_first || guest > g_last) return;
    hit h = {guest, write, {0}, {psp_cpu.r[4], psp_cpu.r[5], psp_cpu.r[6], psp_cpu.r[7]}};
    h.stack[0] = psp_cpu.r[31];
    int depth = 1;
    for (const psp_frame *f = psp_cpu.frame; f && depth < MAX_DEPTH; f = f->prev) {
        if (f->ret) h.stack[depth++] = f->ret;
    }
    const LONG count = g_count;
    if (count >= MAX_HITS) return;
    for (LONG i = 0; i < count; i++) {
        /* The same code reading the next byte, or called from a different place
         * further out, says nothing new. */
        if (!memcmp(g_hits[i].stack, h.stack, SAME_FRAMES * sizeof h.stack[0])) return;
    }
    g_hits[count] = h;
    g_count = count + 1;
}

static LONG WINAPI on_exception(EXCEPTION_POINTERS *e) {
    const DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code == STATUS_GUARD_PAGE_VIOLATION) {
        const uint8_t *at = (const uint8_t *)e->ExceptionRecord->ExceptionInformation[1];
        if (at < g_page || at >= g_page + g_page_size) return EXCEPTION_CONTINUE_SEARCH;
        record(g_page_guest + (uint32_t)(at - g_page), e->ExceptionRecord->ExceptionInformation[0] == 1);
        /* The guard came off. One instruction later it goes back on. */
        e->ContextRecord->EFlags |= 0x100;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == EXCEPTION_SINGLE_STEP && g_page) {
        arm();
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void watch_start(void) {
    const char *value = getenv("DIVA_WATCH");
    if (!value || !value[0]) return;
    char *end;
    const uint32_t guest = (uint32_t)strtoul(value, &end, 16);
    g_first = guest;
    g_last = *end == '-' ? (uint32_t)strtoul(end + 1, NULL, 16) : guest;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    g_page_size = si.dwPageSize;
    uint8_t *host = psp_mem.ram + (guest - PSP_RAM_BASE);
    g_page = (uint8_t *)((uintptr_t)host & ~(uintptr_t)(g_page_size - 1));
    g_page_guest = guest - (uint32_t)(host - g_page);
    AddVectoredExceptionHandler(1, on_exception);
    arm();
    fprintf(stderr, "watch: guest 0x%08X..0x%08X\n", g_page_guest, g_page_guest + (uint32_t)g_page_size);
}

void watch_dump(void) {
    const LONG count = g_count;
    for (; g_printed < count; g_printed++) {
        const hit *h = &g_hits[g_printed];
        fprintf(stderr, "watch: %s 0x%08X  a0-3 %08X %08X %08X %08X  stack", h->write ? "write" : "read", h->guest,
                h->a[0], h->a[1], h->a[2], h->a[3]);
        for (int i = 0; i < MAX_DEPTH && h->stack[i]; i++) fprintf(stderr, " %08X", h->stack[i]);
        fprintf(stderr, "\n");
    }
}
