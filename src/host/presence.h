/* What the player is doing, shown on their Discord profile. See presence.c. */
#ifndef DIVA_PRESENCE_H
#define DIVA_PRESENCE_H

#include <stdint.h>

/* Starts talking to the Discord app on this computer, as the application `client_id`. With none,
 * or with Discord closed, nothing happens and the game is not slowed. */
void presence_start(const char *client_id);

/* A text being drawn: the address the game asked for, the text that is shown (in the game's own
 * encoding), and whether it is a line placed in the middle of a message window. */
void presence_text(uint32_t asked, const char *shown, int placed);

/* The song list's panel, drawn for this song and level, with the save's scores object. */
void presence_chosen(uint32_t scores, int song, int level);

/* Each time the game updates a song: nonzero while it is on, paused or not. */
void presence_song_frame(int on);

#endif
