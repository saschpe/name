#include "clock.h"
#include "hash.h"
#include "name.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PORT 57539
#define POLL_TIMEOUT 10000

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

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in sa;

    g_my_id = getpid();
    g_client_hash_table = g_hash_table_new(g_int_hash, g_int_equal);
    parse_cmdline_args(argc, argv);
    clock_init();

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket"); exit(3);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(PORT);
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
    send_HELLO(sock, sa, g_my_id);

    struct pollfd pfd[1];
    pfd[0].fd = sock;
    pfd[0].events = POLLIN;

    /* printf("listening for datagrams...\n"); */
    while (1) {
        int wait_time = poll_time(get_time() + POLL_TIMEOUT * 1000);
        int ret = poll(pfd, 1, wait_time);

        if (ret == 0) {
            printf("  Timeout, send another HELLO\n");
            send_HELLO(sock, sa, g_my_id);
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
                                if (g_hash_table_lookup(g_client_hash_table, &sender_id) == NULL) {
                                    hash_table_insert(g_client_hash_table, sender_id, "");
                                }
                                send_GET_NAME(sender_id, sock, sa, csa, g_my_id);
                            }/* else {
                                printf("  Discarded message sent by myself.\n");
                            }*/
                            break;
                        }
                        case GET_ID: {
                            printf("GET_ID message received from '%d' for '%s'.\n", sender_id, pack.payload.name);
                            if (sender_id != g_my_id && strncmp(pack.payload.name, g_my_name, strlen(g_my_name))) {
                                printf("  Message was for me, send NAME_ID message to '%d'.\n", sender_id);
                                send_NAME_ID(sock, sa, csa, g_my_id, g_my_name);
                                if (g_hash_table_lookup(g_client_hash_table, &sender_id) == NULL) {
                                    hash_table_insert(g_client_hash_table, sender_id, "");
                                    send_GET_NAME(sender_id, sock, sa, csa, g_my_id);
                                }
                            }/* else {
                                printf("  Discarded message either sent by me or not destinated to me.\n");
                            }*/
                            break;
                        }
                        case GET_NAME: {   // Stupid C89, needs a block inside 'case' to allow variable declarations
                            unsigned short payload_id = ntohs(pack.payload.id);
                            printf("GET_NAME message received from '%d' for '%hd'.\n", sender_id, payload_id);
                            if (sender_id != g_my_id && payload_id == g_my_id) {
                                printf("  Message was for me, send NAME_ID message to '%d'.\n", sender_id);
                                send_NAME_ID(sock, sa, csa, g_my_id, g_my_name);
                                if (g_hash_table_lookup(g_client_hash_table, &sender_id) == NULL) {
                                    hash_table_insert(g_client_hash_table, sender_id, "");
                                    send_GET_NAME(sender_id, sock, sa, csa, g_my_id);
                                }
                            }/* else {
                                printf("  Discarded message either sent by me or not destinated to me.\n");
                            }*/
                            break;
                        }
                        case NAME_ID: {
                            pack.payload.name[12] = '\0';
                            printf("NAME_ID message received from '%d' with name '%s'.\n", sender_id, pack.payload.name);
                            if (sender_id != g_my_id) {
                                client_info_t *info = g_hash_table_lookup(g_client_hash_table, &sender_id);
                                if (info == NULL) {
                                    hash_table_insert(g_client_hash_table, sender_id, pack.payload.name);
                                } else {
                                    strncpy(info->name, pack.payload.name, strlen(pack.payload.name));
                                }
                            }
                            break;
                        }
                        case START_ELECTION: {
                            printf("START_ELECTION message received from '%d'.\n", sender_id);
                            if (sender_id != g_my_id) {
                                send_ELECTION(sock, sa, g_my_id);
                            }
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
    g_hash_table_foreach(g_client_hash_table, hash_table_free, NULL);
    g_hash_table_destroy(g_client_hash_table);
    return 0;
}
