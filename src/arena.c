// This is the main program for the arena server

// The job of this module is to set the system up and then turn each
// command received from the client over to the arena_protocol module
// which will handle the actual communication protocol between clients
// (players) and the server.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "player.h"
#include "pllist.h"
#include "arena_protocol.h"

/***********************************************************************
 * The client thread handles the basic network read loop -- get a line
 * from the network connection and process it.
 */
static void* client_thread(void* arg) {
    player_info* player = (player_info*)arg;
    pllist_add(player);

    // Detatch - unpredictable thread start/stop patterns

    pthread_detach(player->thread);

    char* lineptr = NULL;
    size_t linesize = 0;

    while (player->state != PLAYER_DONE) {
        if (getline(&lineptr, &linesize, player->fp_recv) < 0) {
            // Failed getline means the client disconnected
            break;
        }
        docommand(player, lineptr);
    }

    // Finished with session, so unregister it and free resources.

    if (lineptr != NULL) {
        free(lineptr);
    }

    printf("Client %ld disconnected.\n", player->thread);
    pllist_remove(player);

    return NULL;
}

/********************************************************************
 * Make a TCP listener for port "service" (given as a sting, but
 * either a port number or service name). This function will only
 * create a public listener (listening on all interfaces).
 *
 * Either returns a file handle to use with accept(), or -1 on error.
 * In general, error reporting could be improved, but this just indicates
 * success or failure.
 */
static int create_listener(char* service) {
    int sock_fd;
    if ((sock_fd=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    // Avoid time delay in reusing port - important for debugging, but
    // probably not used in a production server.

    int optval = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    // First, use getaddrinfo() to fill in address struct for later bind

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    struct addrinfo* result;
    int rval;
    if ((rval=getaddrinfo(NULL, service, &hints, &result)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rval));
        close(sock_fd);
        return -1;
    }

    // Assign a name/addr to the socket - just blindly grabs first result
    // off linked list, but really should be exactly one struct returned.

    int bret = bind(sock_fd, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    result = NULL;  // Not really necessary, but ensures no use-after-free

    if (bret < 0) {
        perror("bind");
        close(sock_fd);
        return -1;
    }

    // Finally, set up listener connection queue
    int lret = listen(sock_fd, 128);
    if (lret < 0) {
        perror("listen");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

/************************************************************************
 * Part 2 main: networked server. Spawns a new thread for each connection.
 * Probably should put an upper limit on this, but we're not going to
 * go crazy with a million clients in this class assignment...
 */
int main(int argc, char* argv[]) {
    pllist_init();

    int sock_fd = create_listener("8080");
    if (sock_fd < 0) {
        fprintf(stderr, "Server setup failed.\n");
        exit(1);
    }

    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int comm_fd;
    while ((comm_fd=accept(sock_fd, (struct sockaddr*)&client_addr,
                           &client_addr_len)) >= 0) {
        // Got a new connection, so create a "player" and spawn a thread
        player_info* new_client = new_player(comm_fd);
        if (new_client != NULL) {
            pthread_create(&new_client->thread, NULL, client_thread, new_client);
            printf("Got connection from %s (client %ld)\n",
                   inet_ntoa(((struct sockaddr_in*)&client_addr)->sin_addr),
                   new_client->thread);
        }
    }

    printf("Shutting down...\n");

    return 0;
}
