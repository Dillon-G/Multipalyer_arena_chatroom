// This module is built on top of alist.h, to provide for a threadsafe
// list of players in the system. There is one global list, managed
// by this module, and it is locked any time the list is accessed or
// changed in some way. The lock can also apply to changes within a
// player struct, so things like changing the player name can go here
// to make sure there are not thread-safety issues.

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "alist.h"
#include "pllist.h"

// The list of all players

static alist all_players;

// A global lock, to ensure that the list doesn't change when being accessed

static pthread_rwlock_t listlock;

/***************************************************************************
 * Callback function for use by the list routines to destroy and free a
 * player_info struct.
 */
void player_free(void* p) {
    player_info* ap = (player_info*)p;
    player_destroy(ap);
    free(ap);
}

/***************************************************************************
 * Initializes the list of players. Should be called once at the beginning
 * of main, when the program starts up.
 */
void pllist_init(void) {
    alist_init(&all_players, player_free);
    pthread_rwlock_init(&listlock, NULL);
}

/***************************************************************************
 * pllist_add adds a new player to the list.
 */
void pllist_add(player_info* newplayer) {
    pthread_rwlock_wrlock(&listlock);
    alist_add(&all_players, newplayer);
    pthread_rwlock_unlock(&listlock);
}

/***************************************************************************
 * pllist_list lists players in the list.
 */
// void pllist_list(player_info* newplayer) {

// }

/***************************************************************************
 * pllist_find_nolock searches the list of players for a registered
 * player with the given name. This is only for internal use by this
 * module and does no locking - it is assume that the user of this function
 * will surround the use with the appropriate locks.
 */
static player_info* pllist_find_nolock(char* name) {
    for (int i = 0; i < alist_size(&all_players); i++) {
        player_info* thisplayer = alist_get(&all_players, i);
        if ((thisplayer->state != PLAYER_UNREG) &&
            (strcmp(thisplayer->name, name) == 0)) {
            return thisplayer;
        }
    }

    return NULL;
}

/***************************************************************************
 * pllist_link links a given name to a player, the only time this
 * is used is when a name is already checked to exist but a
 * failsafe was added regardless.
 */
player_info* pllist_link(char* name) {
    // If the given name is null
    if (name == NULL) {
        printf("Error: Null name\n");
        return NULL;
    }
    // Links the player to the given name
    for (int i = 0; i < alist_size(&all_players); i++) {
        player_info* thisplayer = alist_get(&all_players, i);
        if (thisplayer != NULL && thisplayer->state != PLAYER_UNREG &&
            strcmp(thisplayer->name, name) == 0) {
            return thisplayer;
        }
    }
    return NULL;
}

/***************************************************************************
 * pllist_list lists all players within the same room as the
 * player who ran the command, the players are returned
 * separated by comma except the last player in the list.
 */
void pllist_list(player_info* player) {
    // Loops through the list of players
    for (int i = 0; i < alist_size(&all_players); i++) {
        player_info* thisplayer = alist_get(&all_players, i);

        // If a player is in the same lobby as the original player
        if (player->in_room == thisplayer->in_room) {
            // List that player, if it's the last one then no comma
            if (i == alist_size(&all_players) - 1) {
                fprintf(player->fp_send, "%s", thisplayer->name);
            } else {
                fprintf(player->fp_send, "%s, ", thisplayer->name);
            }
        }
    }
}

/***************************************************************************
 * pllist_announce_arrival announces when a player enters the same
 * arena as the other players in that arena, to said players.
 */
void pllist_announce_arrival(player_info* player) {
    // Loops through the player list
    for (int i = 0; i < alist_size(&all_players); i++) {
        player_info* thisplayer = alist_get(&all_players, i);
        // Finds players in the lobby and sends them a message about
        // The new player joining, this is also sent to the player who joined.
        if (player->in_room == thisplayer->in_room) {
            fprintf(thisplayer->fp_send, "%s has joined the room!", player->name);
            fprintf(thisplayer->fp_send, "\n");
        }
    }
}

/***************************************************************************
 * pllist_announce_departure announces when a player leaves the same
 * arena as the other players in that arena, to said players.
 */
void pllist_announce_departure(player_info* player) {
    // Loops through the player list
    for (int i = 0; i < alist_size(&all_players); i++) {
        player_info* thisplayer = alist_get(&all_players, i);
        // Finds players in the lobby and sends them a message about
        // The other player leaving, this isnt sent to the player who left.
        if (player->in_room == thisplayer->in_room && !(strcmp(thisplayer->name, player->name) == 0)) {
            fprintf(thisplayer->fp_send, "%s has left the room!", player->name);
            fprintf(thisplayer->fp_send, "\n");
        }
    }
}

/***************************************************************************
 * pllist_exists checks the list of players to see if a registered
 * player with the given name, and if no such player is in the list
 * then it sets the name of "player" to "name". Returns true/false
 * depending on whether the player was added (true means successfully
 * added).
 */
int pllist_addifnew(player_info* player, char* name) {
    int success = 0;
    pthread_rwlock_wrlock(&listlock);
    player_info* thisplayer = pllist_find_nolock(name);
    if (thisplayer == NULL) {
        strcpy(player->name, name);
        success = 1;
    }
    pthread_rwlock_unlock(&listlock);
    return success;
}

/***************************************************************************
 * pllist_remove scans the list of players for the specific struct
 * passed in, and then removes it from the list. Typically this is called
 * from a thread for their own player_info before the thread exits.
 */
void pllist_remove(player_info* ditch) {
    pthread_rwlock_wrlock(&listlock);
    for (int i = 0; i < alist_size(&all_players); i++) {
        if (alist_get(&all_players, i) == ditch) {
            alist_remove(&all_players, i);
            pthread_rwlock_unlock(&listlock);
            return;
        }
    }

    printf("Couldn't find player to remove - this shouldn't happen\n");
    pthread_rwlock_unlock(&listlock);
}
