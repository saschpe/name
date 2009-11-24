#include "clock.h"

#include <glib.h>

#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#define NAME_PORT 57539
#define POLL_TIMEOUT 10000
#define MAX_CLIENTS 16

typedef enum packet_type {
    HELLO = 1,
    GET_ID,
    GET_NAME,
    NAME_ID,
    START_ELECTION,
    ELECTION,
    MASTER
} packet_type_t;

typedef struct packet {
    unsigned short sender_id;
    unsigned short type;
    union {
        unsigned short id;
        char name[12];
    } payload;
} __attribute((packed)) packet_t;

typedef struct client_info
{
    char name[12];
    int last_hello;
} client_info_t;

static GHashTable *g_client_hash_table;
static unsigned short g_my_id;
static const char *g_my_name = "Sascha";

/*
    FUNCTIONS
*/

void print_usage(const char *prog_name)
{
    printf("Usage: %s [ID, NAME]\n"
           "    ID   : integer between 0 and 65535\n"
           "    NAME : string of max. 11 characters\n", prog_name);
}

void send_HELLO(int sock, struct sockaddr_in sa)
{
    struct packet pack;

    sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    memset(&pack, 0, sizeof(pack));
    pack.sender_id = htons(g_my_id);
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
void send_GET_NAME(int sender_id, int sock, struct sockaddr_in sa, struct sockaddr_in csa)
{
    struct packet tmp;

    client_info_t *info = g_hash_table_lookup(g_client_hash_table, &sender_id);
    if (info != NULL) {
        if (info->name != NULL && strlen(info->name) == 0) {
            printf("  No name stored, send a GET_NAME message to '%d'.\n", sender_id);
            /* send GET_NAME message */
            struct packet tmp;
            memset(&tmp, 0, sizeof(tmp));
            tmp.sender_id = htons(g_my_id);
            tmp.type = htons(GET_NAME);
            tmp.payload.id = htons(sender_id);
            sa.sin_addr.s_addr = csa.sin_addr.s_addr;
            if (sendto(sock, (void *)&tmp, sizeof(tmp), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
                perror("sendto");
            }
        } else {
            printf("  Already have a name for '%d': '%s'\n", sender_id, info->name);
        }
    } else {
        fprintf(stderr, "Error: The id '%d' is not stored!\n", sender_id);
    }
}

void send_NAME_ID(int sock, struct sockaddr_in sa, struct sockaddr_in csa)
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

static void parse_cmdline_args(int argc, char *argv[])
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

static void free_a_hash_table_entry(gpointer key, gpointer value, gpointer user_data)
{
    g_free(key);
    g_free(value);
}

static void hash_table_insert(GHashTable *table, int id, const char *name)
{
    client_info_t *info = (client_info_t *)malloc(sizeof(client_info_t));

    if (info != NULL) {
        // This stupid bullshit is necessary because glib hash tables don't make
        // copies of provided keys and values, instead store pointers.
        int *key = (int *)malloc(sizeof(int));
        *key = id;
        strncpy(info->name, name, strlen(name));
        g_hash_table_insert(g_client_hash_table, key, info);
    } else {
        fprintf(stderr, "Error: Unable to allocate memory for client info!\n");
    }
}

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in sa;

    g_my_id = getpid();
    g_client_hash_table = g_hash_table_new(g_int_hash, g_int_equal);
    clock_init();
    parse_cmdline_args(argc, argv);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket"); exit(3);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(NAME_PORT);
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
    send_HELLO(sock, sa);

    struct pollfd pfd[1];
    pfd[0].fd = sock;
    pfd[0].events = POLLIN;

    /* printf("listening for datagrams...\n"); */
    while (1) {
        int wait_time = poll_time(get_time() + POLL_TIMEOUT * 1000);
        int ret = poll(pfd, 1, wait_time);

        if (ret == 0) {
            printf("  Timeout, send another HELLO\n");
            send_HELLO(sock, sa);
        } else if (ret > 0) {
            if (pfd[0].revents & POLLIN) {
                int bytes_read;
                struct sockaddr_in csa;
                socklen_t csalen;
                struct packet pack;

                if ((bytes_read  = recvfrom(sock, &pack, sizeof(pack), 0, (struct sockaddr *)&csa, &csalen)) < 0) {
                    fprintf(stderr, "Error: Unable to read datagram!\n");
                } else {
                    unsigned short sender_id = ntohs(pack.sender_id);
                    switch (ntohs(pack.type)) {
                        case HELLO: {
                            printf("HELLO message received from '%d'.\n", sender_id);
                            if (sender_id != g_my_id) {
                                client_info_t *info = g_hash_table_lookup(g_client_hash_table, &sender_id);
                                if (info == NULL) {
                                    hash_table_insert(g_client_hash_table, sender_id, "");
                                }
                                info->last_hello = get_time();
                                send_GET_NAME(sender_id, sock, sa, csa);
                            } else {
                                printf("  Discarded message sent by myself.\n");
                            }
                            break;
                        }
                        case GET_ID: {
                            printf("GET_ID message received from '%d' for '%s'.\n", sender_id, pack.payload.name);
                            if (sender_id != g_my_id && strncmp(pack.payload.name, g_my_name, strlen(g_my_name))) {
                                printf("  Message was for me, send NAME_ID message to '%d'.\n", sender_id);
                                send_NAME_ID(sock, sa, csa);
                            } else {
                                printf("  Discarded message either sent by me or not destinated to me.\n");
                            }
                            break;
                        }
                        case GET_NAME: {   // Stupid C89, needs a block inside 'case' to allow variable declarations
                            unsigned short payload_id = ntohs(pack.payload.id);
                            printf("GET_NAME message received from '%d' for '%hd'.\n", sender_id, payload_id);
                            if (sender_id != g_my_id && payload_id == g_my_id) {
                                send_GET_NAME(sender_id, sock, sa, csa);
                                printf("  Message was for me, send NAME_ID message to '%d'.\n", sender_id);
                                send_NAME_ID(sock, sa, csa);
                            } else {
                                printf("  Discarded message either sent by me or not destinated to me.\n");
                            }
                            break;
                        }
                        case NAME_ID: {
                            pack.payload.name[12] = '\0';
                            printf("NAME_ID message received from '%d' with name '%s'.\n", sender_id, pack.payload.name);
                            client_info_t *info = g_hash_table_lookup(g_client_hash_table, &sender_id);
                            if (info == NULL) {
                                hash_table_insert(g_client_hash_table, sender_id, pack.payload.name);
                            } else {
                                strncpy(info->name, pack.payload.name, strlen(pack.payload.name));
                            }
                            printf("  Stored name '%s' for '%d' in local database.\n", info->name, sender_id);
                            break;
                        }
                        case START_ELECTION: {
                            break;
                        }
                        case ELECTION: {
                            break;
                        }
                        case MASTER: {
                            break;
                        }
                    }
                }
            }
        }

    }
    g_hash_table_foreach(g_client_hash_table, free_a_hash_table_entry, NULL);
    g_hash_table_destroy(g_client_hash_table);
    return 0;
}
