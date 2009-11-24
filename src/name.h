#ifndef NAME_H
#define NAME_H

#include <arpa/inet.h>

#define ELECTION_TIMEOUT 300
#define MASTER_TIMEOUT 600

/**
 *
 */
typedef enum packet_type {
    HELLO = 1,
    GET_ID,
    GET_NAME,
    NAME_ID,
    START_ELECTION,
    ELECTION,
    MASTER
} packet_type_t;

/**
 *
 */
typedef struct packet {
    unsigned short sender_id;
    unsigned short type;
    union {
        unsigned short id;
        char name[12];
    } payload;
} __attribute((packed)) packet_t;

/**
 *
 */
typedef struct client_info
{
    char name[12];
    int last_hello;
} client_info_t;

void send_HELLO(int sock, struct sockaddr_in sa, unsigned short my_id);
void send_ELECTION(int sock, struct sockaddr_in sa, unsigned short my_id);
void send_GET_NAME(int sender_id, int sock, struct sockaddr_in sa, struct sockaddr_in csa, unsigned short my_id);
void send_NAME_ID(int sock, struct sockaddr_in sa, struct sockaddr_in csa, unsigned short my_id, const char *my_name);

#endif
