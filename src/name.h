#ifndef NAME_H
#define NAME_H

#include "clock.h"

#include <arpa/inet.h>

#define NS_ELECTION_TIMEOUT_MICROSECONDS 300
#define NS_ELECTION_TIMEOUT_MILLISECONDS (NS_ELECTION_TIMEOUT_MICROSECONDS * 1000)
#define NS_HELLO_TIMEOUT_MICROSECONDS 10000
#define NS_HELLO_TIMEOUT_MILLISECONDS (NS_HELLO_TIMEOUT_MICROSECONDS * 1000)
#define NS_MASTER_TIMEOUT_MICROSECONDS 600
#define NS_MASTER_TIMEOUT_MILLISECONDS (NS_MASTER_TIMEOUT_MICROSECONDS * 1000)

/**
 * Defines the possible packet types
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
 * Describes the packet structure.
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
 * Holds the information about other clients
 */
typedef struct ns_peer
{
    char name[12];
    time_val last_hello;
} ns_peer_t;

void ns_init(int *sock, struct sockaddr_in *sa, int port);

void ns_send_HELLO(int sock, struct sockaddr_in sa, unsigned short id);
void ns_send_GET_ID(int sock, struct sockaddr_in sa, unsigned short id, struct sockaddr_in csa, int cid);
void ns_send_GET_NAME(int sock, struct sockaddr_in sa, unsigned short id, struct sockaddr_in csa, int cid);
void ns_send_NAME_ID(int sock, struct sockaddr_in sa, unsigned short id, const char *name, struct sockaddr_in csa);

void ns_send_START_ELECTION(int sock, struct sockaddr_in sa, unsigned short id);
void ns_send_ELECTION(int sock, struct sockaddr_in sa, unsigned short id);
void ns_send_MASTER(int sock, struct sockaddr_in sa, unsigned short id);

#endif
