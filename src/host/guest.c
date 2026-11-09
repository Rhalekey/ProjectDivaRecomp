/* Calling the game's functions from a hook. See guest.h. */
#include "guest.h"

#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>

#include <string.h>

uint32_t guest_call_f(uint32_t function, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, float f12) {
    uint32_t saved[32];
    const float saved_f12 = psp_cpu.f[12];
    memcpy(saved, psp_cpu.r, sizeof saved);
    psp_cpu.r[PSP_REG_A0] = a0;
    psp_cpu.r[PSP_REG_A1] = a1;
    psp_cpu.r[PSP_REG_A2] = a2;
    psp_cpu.r[PSP_REG_A3] = a3;
    psp_cpu.f[12] = f12;
    psp_dispatch(function);
    const uint32_t result = psp_cpu.r[PSP_REG_V0];
    memcpy(psp_cpu.r, saved, sizeof saved);
    psp_cpu.f[12] = saved_f12;
    return result;
}

uint32_t guest_call(uint32_t function, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return guest_call_f(function, a0, a1, a2, a3, psp_cpu.f[12]);
}
