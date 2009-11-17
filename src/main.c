#include "clock.h"

#include <glib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#define VVS_PORT 57539

/*
    TYPEDEFS
*/

typedef enum packet_type {
    HELLO = 1,
    GET_ID,
    GET_NAME,
    NAME_ID
} packet_type_t;

typedef struct packet {
    unsigned short sender_id;
    unsigned short type;
    union {
        unsigned short id;
        char name[12];
    } payload;
} __attribute((packed)) packet_t;

/*
    GLOBALS
*/

static GHashTable *g_id_name_hash_table;
static unsigned short g_my_id;
static const char *g_my_name = "default";

/*
    FUNCTIONS
*/

void print_usage(const char *prog_name)
{
    printf("Usage: %s [ID, NAME]\n"
           "    ID   : integer between 0 and 65535\n"
           "    NAME : string of max. 11 characters\n", prog_name);
}

void check_name_or_send_get_name(struct packet pack, int sock, struct sockaddr_in sa, struct sockaddr_in csa)
{
    struct packet tmp;

    const char *name = g_hash_table_lookup(g_id_name_hash_table, &pack.sender_id);
    if (name != NULL || strlen(name) == 0) {
        printf("  No name stored, send a GET_NAME message to '%d'.\n", pack.sender_id);
        /* send GET_NAME message */
        struct packet tmp;
        memset(&tmp, 0, sizeof(tmp));
        tmp.sender_id = htons(g_my_id);
        tmp.type = htons(GET_NAME);
        tmp.payload.id = htons(pack.sender_id);
        sa.sin_addr.s_addr = csa.sin_addr.s_addr;
        if (sendto(sock, (void *)&tmp, sizeof(tmp), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            perror("sendto");
        }
    }
}

void send_name_id(int sock, struct sockaddr_in sa, struct sockaddr_in csa)
{
    struct packet tmp;

    memset(&tmp, 0, sizeof(tmp));
    tmp.sender_id = htons(g_my_id);
    tmp.type = htons(NAME_ID);
    strncpy(tmp.payload.name, g_my_name, strlen(g_my_name));
    sa.sin_addr.s_addr = csa.sin_addr.s_addr;
    if (sendto(sock, (void *)&tmp, sizeof(tmp), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto");
    }
}

void parse_cmdline_args(int argc, char *argv[])
{
    if (argc == 3) {
        int tmp = atoi(argv[1]); //TODO: strtol catches more errors
        if (tmp < 0 || tmp > USHRT_MAX) {
            printf("Invalid ID provided!\n");
            print_usage(argv[0]);
            exit(1);
        }
        g_my_id = tmp;
        if (strlen(argv[2]) > 11) {
            printf("Invalid NAME provided!\n");
            print_usage(argv[0]);
            exit(1);
        }
        g_my_name = argv[2];
    } else if (argc != 1) {
        print_usage(argv[0]);
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in sa;
    struct packet pack;

    g_my_id = getpid();
    g_id_name_hash_table = g_hash_table_new(g_int_hash, g_int_equal);
    clock_init();

    parse_cmdline_args(argc, argv);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket"); exit(3);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(VVS_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&sa, sizeof(sa))) {
        perror("bind"); exit(4);
    }

    /* allow broadcasts on socket */
    int bc = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc)) < 0) {
        perror("setsockopt"); exit(5);
    }

    /* broadcast HELLO message */
    sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(g_my_id);
    pack.type = htons(HELLO);
    /* inet_addr("127.0.0.1"); */
    if (sendto(sock, (void *)&pack, sizeof(pack), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("sendto"); exit(6);
    }

    /* printf("listening for datagrams...\n"); */
    while (1) {
        int bytes_read;
        struct sockaddr_in csa;
        socklen_t csalen;

        if ((bytes_read  = recvfrom(sock, &pack, sizeof(pack), 0, (struct sockaddr *)&csa, &csalen)) < 0) {
            fprintf(stderr, "Error while reading datagram!\n");
        } else {
            unsigned short sender_id = ntohs(pack.sender_id);
            switch (pack.type) {
                case HELLO:
                    printf("HELLO message received from '%d'.\n", sender_id);
                    g_hash_table_insert(g_id_name_hash_table, &sender_id, "");
                    if (sender_id != g_my_id) {
                        check_name_or_send_get_name(pack, sock, sa, csa);
                    } else {
                        printf("Discarded message sent by myself.\n");
                    }
                    break;
                case GET_ID:
                    printf("GET_ID message received from '%d' for '%s'.\n", sender_id, pack.payload.name);
                    if (sender_id != g_my_id && strncmp(pack.payload.name, g_my_name, strlen(g_my_name))) {
                        printf("  Message was for me, send NAME_ID message to '%d'.\n", sender_id);

                        send_name_id(sock, sa, csa);
                    } else {
                        printf("Discarded message either sent by me or not destinated to me.\n");
                    }
                    break;
                case GET_NAME:
                    {
                        unsigned short payload_id = ntohs(pack.payload.id);

                        printf("GET_NAME message received from '%d' for '%hd'.\n", sender_id, payload_id);
                        if (sender_id != g_my_id && payload_id == g_my_id) {
                            printf("  Message was for me, send NAME_ID message to '%d'.\n", sender_id);

                            check_name_or_send_get_name(pack, sock, sa, csa);
                            send_name_id(sock, sa, csa);
                        } else {
                            printf("Discarded message either sent by me or not destinated to me.\n");
                        }
                    } break;
                case NAME_ID:
                    pack.payload.name[12] = '\0';
                    printf("NAME_ID message received from '%d' with name '%s'.\n", sender_id, pack.payload.name);
                    g_hash_table_insert(g_id_name_hash_table, &sender_id, pack.payload.name);
                    printf("  Stored name '%s' for '%d' in local database.\n", g_hash_table_lookup(g_id_name_hash_table, &sender_id), sender_id);
                    break;
            }
        }
    }
    g_hash_table_destroy(g_id_name_hash_table);
    return 0;
}
