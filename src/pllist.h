// Function prototypes for the pllist (player list) functions

#ifndef _PLLIST_H
#define _PLLIST_H

#include "player.h"

void pllist_init(void);
void pllist_add(player_info* newplayer);
int pllist_addifnew(player_info* player, char* name);
void pllist_remove(player_info* player);
player_info* pllist_link(char* name);
void pllist_list(player_info* player);
void pllist_announce_arrival(player_info* player);
void pllist_announce_departure(player_info* player);
#endif  // _PLLIST_H
