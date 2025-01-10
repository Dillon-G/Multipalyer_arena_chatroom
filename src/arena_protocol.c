// Module to implement the arena application-layer protocol.

// The protocol is fully defined in the README file. This module
// includes functions to parse and perform commands sent by a
// player (the docommand function), and has functions to send
// responses to ensure proper and consistent formatting of these
// messages.

// This gives access to the asprintf function - helpful, but not portable!
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "util.h"
#include "player.h"
#include "arena_protocol.h"
#include "pllist.h"

/************************************************************************
 * Call this response function if a command was accepted
 */
void send_ok(player_info* player) {
    fprintf(player->fp_send, "OK\n");
}

/************************************************************************
 * Call this response function if an error can be described by a simple
 * string.
 */
void send_notice(player_info* player, char* desc) {
    fprintf(player->fp_send, "NOTICE %s\n", desc);
}

/************************************************************************
 * Call this response function if an error can be described by a simple
 * string.
 */
void send_err(player_info* player, char* desc) {
    fprintf(player->fp_send, "ERR %s\n", desc);
}

/************************************************************************
 * Call this response function if you want to embed a specific string
 * argument (sarg) into an error reply (which is now a format string).
 */
void send_err_sarg(player_info* player, char* fmtstring, char* sarg) {
    fprintf(player->fp_send, "ERR ");
    fprintf(player->fp_send, fmtstring, sarg);
    fprintf(player->fp_send, "\n");
}

/************************************************************************
 * Call this response function if you want to embed a specific string
 * argument (sarg) into an error reply (which is now a format string).
 */
void send_notice_sarg(player_info* player, char* fmtstring, char* sarg, char* sarg2) {
    fprintf(player->fp_send, "NOTICE ");
    fprintf(player->fp_send, fmtstring, sarg, sarg2);
    fprintf(player->fp_send, "\n");
}

/************************************************************************
 * Handle the "LOGIN" command.
 */
static void cmd_login(player_info* player, char* arg1, char* rest) {
    if (player->state != PLAYER_UNREG) {
        send_err_sarg(player, "Already logged in as %s", player->name);
        return;
    }

    if (arg1 == NULL) {
        send_err(player, "LOGIN missing name");
        return;
    }

    if (rest != NULL) {
        send_err(player, "LOGIN should have only one argument");
        return;
    }

    char* cp = arg1;
    while (*cp != '\0') {
        if (!isalnum(*cp)) {
            send_err(player, "Invalid name -- only alphanumeric characters allowed");
            return;
        }
        cp++;
    }

    if (strlen(arg1) > PLAYER_MAXNAME) {
        send_err(player, "Invalid name -- too long");
        return;
    }

    // Check for a duplicate name - not O(1) time but only done at login
    // player_addifnew() is an atomic check-and-set

    if (pllist_addifnew(player, arg1)) {
        player->state = PLAYER_REG;
        send_ok(player);
    } else {
        send_err(player, "Invalid name -- already in use");
    }
}

/************************************************************************
 * Handle the "MOVETO" command.
 */
static void cmd_moveto(player_info* player, char* arg1, char* rest) {
    if (player->state == PLAYER_UNREG) {
        send_err(player, "Player must be logged in before MOVETO");
        return;
    }
    //Announces the departure of the player
    pllist_announce_departure(player);
    if (arg1 == NULL) {
        send_err(player, "No Room Selected.");

    } else if (strcmp(arg1, "arena0") == 0) {
        fprintf(player->fp_send, "Moved to Lobby\n");
        player->in_room = 0;

    } else if (strcmp(arg1, "arena1") == 0) {
        fprintf(player->fp_send, "Moved to Arena 1\n");
        player->in_room = 1;

    } else if (strcmp(arg1, "arena2") == 0) {
        fprintf(player->fp_send, "Moved to Arena 2\n");
        player->in_room = 2;

    } else if (strcmp(arg1, "arena3") == 0) {
        fprintf(player->fp_send, "Moved to Arena 3\n");
        player->in_room = 3;

    } else if (strcmp(arg1, "arena4") == 0) {
        fprintf(player->fp_send, "Moved to Arena 4\n");
        player->in_room = 4;

    } else {
        send_err(player, "Invalid arena!");
    }

    //Announces the arrival of the player
    pllist_announce_arrival(player);

    // Hint for future implementation (not needed until part 3): Look
    // into the strtol() function for turning the string argument into
    // a number, with error checking.
}

/************************************************************************
 * Handle the "MSG" command.
 */
static void cmd_msg(player_info* player, char* arg1, char* rest) {
    if (player->state == PLAYER_UNREG) {
        send_err(player, "Player must be logged in before MSG");
        return;
    }
    // Add a check to make sure player isnt trying to message themself
    if (strcmp(arg1, player->name) == 0) {
        send_err(player, "Player cannot MSG self");
    }
    // If they arent a check is made to see if the player in question exists
    if (!pllist_addifnew(player, arg1)) {
        player_info* player2 = pllist_link(arg1);
        send_notice_sarg(player2, "From %s: %s", player->name, rest);
        fprintf(player->fp_send, "%s: %s\n", player->name, rest);

        // If they do not it is returned
    } else {
        fprintf(player->fp_send, "Player Doesn't Exist\n");
    }
}

/************************************************************************
 * Handle the "STAT" command.
 */
static void cmd_stat(player_info* player, char* arg1, char* rest) {
    if (player->state == PLAYER_UNREG) {
        send_err(player, "");
        return;
    }
    fprintf(player->fp_send, "OK %d\n", player->in_room);
}

/************************************************************************
 * Handle the "LIST" command.
 */
static void cmd_list(player_info* player, char* arg1, char* rest) {
    if (player->state == PLAYER_UNREG) {
        send_err(player, "Player must be logged in before LIST");
        return;
    }
    fprintf(player->fp_send, "OK\n");
    pllist_list(player);
    fprintf(player->fp_send, "\n");
}

/************************************************************************
 * Handle the "BYE" command.
 */
static void cmd_bye(player_info* player, char* arg1, char* rest) {
    pllist_announce_departure(player);
    player->state = PLAYER_DONE;
    send_ok(player);
}

/************************************************************************
 * Parses and performs the actions in the line of text (command and
 * optionally arguments) passed in as "command".
 */
void docommand(player_info* player, char* command) {
    char* saveptr;
    char* cmd = strtok_r(command, " \t\r\n", &saveptr);
    if (cmd == NULL) {  // Empty line (no command) -- just ignore line
        return;
    }

    // Get first argument (if there is one)
    char* arg1 = strtok_r(NULL, " \r\n", &saveptr);
    if (arg1 != NULL) {
        // Don't consider an empty string an argument....
        if (arg1[0] == '\0')
            arg1 = NULL;
    }

    // Get the rest (if present -- trimmed)
    char* rest = NULL;
    if (arg1 != NULL) {
        rest = strtok_r(NULL, "\r\n", &saveptr);
        if (rest != NULL) {
            rest = trim(rest);
            // Don't consider an empty string an argument....
            if (rest[0] == '\0')
                rest = NULL;
        }
    }

    // Parsing result: "cmd" has the command string, "arg1" has the
    // next word after "cmd" if it exists (NULL if not), and "rest"
    // has the rest of line after the first argument (NULL if not
    // present).

    if (strcmp(cmd, "LOGIN") == 0) {
        cmd_login(player, arg1, rest);
    } else if (strcmp(cmd, "MOVETO") == 0) {
        cmd_moveto(player, arg1, rest);
    } else if (strcmp(cmd, "MSG") == 0) {
        cmd_msg(player, arg1, rest);
    } else if (strcmp(cmd, "STAT") == 0) {
        cmd_stat(player, arg1, rest);
    } else if (strcmp(cmd, "LIST") == 0) {
        cmd_list(player, arg1, rest);
    } else if (strcmp(cmd, "BYE") == 0) {
        cmd_bye(player, arg1, rest);
    } else {
        send_err(player, "Unknown command");
    }
}
