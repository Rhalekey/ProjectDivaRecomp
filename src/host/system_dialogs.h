/* The system's screens the game asks for, drawn by us as the PSP draws its own: the list of saves to
 * choose from, and the on-screen keyboard. Both hold the game while they show, as on the console. */
#ifndef DIVA_SYSTEM_DIALOGS_H
#define DIVA_SYSTEM_DIALOGS_H

/* Gives them to psprecomp, after controls_start. */
void system_dialogs_start(void);

#endif
