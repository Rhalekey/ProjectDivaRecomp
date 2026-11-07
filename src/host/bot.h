#pragma once

#include <stdint.h>

/* See bot.c. Does nothing unless DIVA_BOT is set. */
void bot_start(void);
int bot_is_on(void);
void bot_hook(uint32_t addr);
