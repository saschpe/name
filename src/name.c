#include "name.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *
 */
void ns_init(int *sock, struct sockaddr_in *sa, int port)
{
    memset(sa, 0, sizeof(struct sockaddr_in));
    sa->sin_family = AF_INET;
    sa->sin_port = htons(port);
    sa->sin_addr.s_addr = htonl(INADDR_ANY);

    if ((*sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket"); exit(3);
    }
    if (bind(*sock, (struct sockaddr *)sa, sizeof(struct sockaddr_in))) {
        perror("bind"); exit(4);
    }

    /* allow broadcasts on socket */
    int bc = 1;
    if (setsockopt(*sock, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc)) < 0) {
        perror("setsockopt"); exit(5);
    }
}

/**
 *
 */
void ns_send_HELLO(int sock, struct sockaddr_in sa, unsigned short id)
{
    struct ns_packet pack;

    sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(id);
    pack.type = htons(HELLO);
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
void ns_send_GET_NAME(int sock, struct sockaddr_in sa, unsigned short id, struct sockaddr_in csa, int cid)
{
    struct ns_packet pack;

    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(id);
    pack.type = htons(GET_NAME);
    pack.payload.id = htons(cid);
    sa.sin_addr.s_addr = csa.sin_addr.s_addr;
    if (sendto(sock, (void *)&pack, sizeof(pack), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto");
    }
}

/**
 *
 */
void ns_send_NAME_ID(int sock, struct sockaddr_in sa, unsigned short id, const char *name, struct sockaddr_in csa)
{
    struct ns_packet pack;

    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(id);
    pack.type = htons(NAME_ID);
    strncpy(pack.payload.name, name, strlen(name));
    sa.sin_addr.s_addr = csa.sin_addr.s_addr;
    if (sendto(sock, (void *)&pack, sizeof(pack), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto");
    }
}

/**
 *
 */
void ns_send_ELECTION(int sock, struct sockaddr_in sa, unsigned short id)
{
    struct ns_packet pack;

    sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(id);
    pack.type = htons(ELECTION);
    /* inet_addr("127.0.0.1"); */
    if (sendto(sock, (void *)&pack, sizeof(pack), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto"); exit(6);
    }
}
