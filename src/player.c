// The player module contains the player data type and management functions

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "player.h"

/************************************************************************
 * player_init initializes an player structure in the initial PLAYER_UNREG
 * state, with given send and receive FILE objects.
 */
void player_init(player_info* player, FILE *fp_send, FILE *fp_recv) {
    player->state = PLAYER_UNREG;
    player->in_room = 0;
    player->thread = 0;
    player->fp_send = fp_send;
    player->fp_recv = fp_recv;
    player->name[0] = '\0';
}

/************************************************************************
 * new_player allocates a player struct and initializes it for
 * file descriptor "comm_fd". If any of the setup fails, this returns
 * NULL (should never happen?).
 */
player_info* new_player(int comm_fd) {
    player_info* ret = malloc(sizeof(player_info));
    if (ret == NULL) {
        perror("new_player");
        exit(1);
    }

    // Duplicate the file descriptor so we have separare read and write fds
    // There may be a better way to do this, but using the same fd for both
    // read and write seems to confuse the position in I/O buffering.
    int dup_fd = dup(comm_fd);
    if (dup_fd < 0) {
        perror("new_player dup");
        free(ret);
        return NULL;
    }

    // Wrap the original fd in a FILE* for buffered/formatted writing
    FILE *sender = fdopen(comm_fd, "w");
    if (sender == NULL) {
        perror("new_player fd_open sender");
        close(dup_fd);
        close(comm_fd);
        free(ret);
        return NULL;
    }

    // Wrap the duplicate fd in a FILE* for buffered/formatted reading
    FILE *receiver = fdopen(dup_fd, "r");
    if (receiver == NULL) {
        perror("new_player fd_open receiver");
        fclose(sender);
        close(dup_fd);
        free(ret);
        return NULL;
    }

    // Set both FILE*'s to be line buffered (line-oriented app protocol)

    setvbuf(sender, NULL, _IOLBF, 0);
    setvbuf(receiver, NULL, _IOLBF, 0);

    player_init(ret, sender, receiver);
    return ret;
}

/************************************************************************
 * player_destroy frees up any resources associated with a player, like
 * file handles, so that it can be free'ed. Will be called from a pllist
 * function to ensure that it is out of the list (and won't be accessed
 * again) before it is destroyed.
 */
void player_destroy(player_info* player) {
    player->state = PLAYER_DONE;  // Just to make sure....
    fclose(player->fp_send);
    fclose(player->fp_recv);
}
