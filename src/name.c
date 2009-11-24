#include "name.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *
 */
void send_HELLO(int sock, struct sockaddr_in sa, unsigned short my_id)
{
    struct packet pack;

    sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(my_id);
    pack.type = htons(HELLO);
    /* inet_addr("127.0.0.1"); */
    if (sendto(sock, (void *)&pack, sizeof(pack), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto"); exit(6);
    }
}

/**
 *
 */
void send_ELECTION(int sock, struct sockaddr_in sa, unsigned short my_id)
{
    struct packet pack;

    sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(my_id);
    pack.type = htons(ELECTION);
    /* inet_addr("127.0.0.1"); */
    if (sendto(sock, (void *)&pack, sizeof(pack), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto"); exit(6);
    }
}

/**
 * Send a GET_NAME package.
 *
 * sender_id has to be in network byte order
 */
void send_GET_NAME(int sender_id, int sock, struct sockaddr_in sa, struct sockaddr_in csa, unsigned short my_id)
{
    struct packet pack;

    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(my_id);
    pack.type = htons(GET_NAME);
    pack.payload.id = htons(sender_id);
    sa.sin_addr.s_addr = csa.sin_addr.s_addr;
    if (sendto(sock, (void *)&pack, sizeof(pack), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto");
    }
}

/**
 *
 */
void send_NAME_ID(int sock, struct sockaddr_in sa, struct sockaddr_in csa, unsigned short my_id, const char *my_name)
{
    struct packet pack;

    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(my_id);
    pack.type = htons(NAME_ID);
    strncpy(pack.payload.name, my_name, strlen(my_name));
    sa.sin_addr.s_addr = csa.sin_addr.s_addr;
    if (sendto(sock, (void *)&pack, sizeof(pack), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto");
    }
}
