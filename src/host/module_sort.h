#ifndef DIVA_MODULE_SORT_H
#define DIVA_MODULE_SORT_H

#include <stdint.h>

/* The buttons, each time the game reads them: Select changes the order while the modules' screen
 * is open. The screen's own functions run only as it opens, so this is where a press is seen. */
void module_sort_input(uint32_t buttons);

#endif
