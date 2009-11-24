#ifndef NAME_H
#define NAME_H

#include <arpa/inet.h>

/**
 *
 */
typedef enum ns_packet_type {
    HELLO = 1,
    GET_ID,
    GET_NAME,
    NAME_ID,
    START_ELECTION,
    ELECTION,
    MASTER
} ns_packet_type_t;

/**
 *
 */
typedef struct ns_packet {
    unsigned short sender_id;
    unsigned short type;
    union {
        unsigned short id;
        char name[12];
    } payload;
} __attribute((packed)) ns_packet_t;

/**
 *
 */
typedef struct ns_client
{
    char name[12];
    int last_hello;
} ns_client_t;

void ns_init(int sock, struct sockaddr_in sa, int port);

void ns_send_HELLO(int sock, struct sockaddr_in sa, unsigned short id);
void ns_send_ELECTION(int sock, struct sockaddr_in sa, unsigned short id);
void ns_send_GET_NAME(int sender_id, int sock, struct sockaddr_in sa, struct sockaddr_in csa, unsigned short id);
void ns_send_NAME_ID(int sock, struct sockaddr_in sa, struct sockaddr_in csa, unsigned short id, const char *name);

#endif
