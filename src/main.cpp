#include "clock.h"
#include "name.h"

#include <map>

#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static unsigned short g_id;
static const char *g_name = "Sascha";

static void print_usage(const char *prog_name)
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
        g_id = tmp;
        if (strlen(argv[2]) > 11) {
            printf("Invalid NAME provided!\n");
            print_usage(argv[0]);
            exit(1);
        }
        g_name = argv[2];
    } else if (argc != 1) {
        print_usage(argv[0]);
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in sa;

    g_id = getpid();
    parse_cmdline_args(argc, argv);
    std::map<int, ns_peer_t> peers;
    clock_init();

    ns_init(&sock, &sa, NS_DEFAULT_PORT);

    struct pollfd pfd[1];
    pfd[0].fd = sock;
    pfd[0].events = POLLIN;

    /* Set the the various wait timeouts for different events */
    time_val hello_wait_time = get_time() + NS_HELLO_TIMEOUT;
    time_val election_wait_time = 0;
    int election_active = 0;
    time_val master_wait_time = 0;

    /* Send the first HELLO message to notify others of a new peer */
    ns_send_HELLO(sock, sa, g_id);
    printf("-> HELLO sent.\n");

    while (1) {
        //printf("   Next HELLO wait time: '%lld'\n", hello_wait_time);
        int ret = poll(pfd, 1, poll_time(hello_wait_time));

        if (ret == 0) {
            /* HELLO message wait time out, send another one */
            ns_send_HELLO(sock, sa, g_id);
            printf("-> HELLO sent.\n");

            //printf("   Check for clients which have not sent a HELLO recently...\n");
            time_val now_time = get_time();
            for (std::map<int, ns_peer_t>::iterator it = peers.begin(); it != peers.end(); it++) {
                // Difference is current time minus last time seen
                if ((now_time - (*it).second.last_hello) > NS_HELLO_LAST_TIME_DIFFERENCE) {
                    printf("   Missing HELLO from '%d', remove from list\n", (*it).first);
                }
                peers.erase(it);
            }

            hello_wait_time = get_time() + NS_HELLO_TIMEOUT;
        } else if (ret > 0) {
            /* An event happend on one of the poll'ed file desciptors */
            if (pfd[0].revents & POLLIN) {
                struct sockaddr_in psa; socklen_t csalen;
                ns_packet_t pack;

                if (recvfrom(sock, &pack, sizeof(pack), 0, (struct sockaddr *)&psa, &csalen) != sizeof(pack)) {
                    fprintf(stderr, "Error: Unable to read datagram!\n");
                    perror("recvfrom");
                } else {
                    unsigned short sender_id = ntohs(pack.sender_id);
                    switch (ntohs(pack.type)) {
                        case HELLO: {
                            printf("<- HELLO from '%d'.\n", sender_id);
                            if (sender_id != g_id) {
                                if (peers.count(sender_id) == 0) {
                                    ns_peer_t info;
                                    memset(&info, 0, sizeof(info));
                                    strncpy(info.name, "", strlen(""));
                                    info.last_hello = get_time();
                                    peers[sender_id] = info;
                                    printf("   Added new peer '%d' with name '%s'\n", sender_id, info.name);
                                } else {
                                    peers[sender_id].last_hello = get_time();
                                    printf("   Updated last HELLO timestamp for peer.\n");
                                }
                                printf("-> GET_NAME to '%d'.\n", sender_id);
                                ns_send_GET_NAME(sock, sa, g_id, psa, sender_id);
                            }
                            break;
                        }
                        case GET_ID: {
                            printf("<- GET_ID from '%d' to name '%s'.\n", sender_id, pack.payload.name);
                            if (sender_id != g_id && strncmp(pack.payload.name, g_name, strlen(g_name))) {
                                //printf("   Message was for me, send NAME_ID message to '%d'.\n", sender_id);
                                printf("-> NAME_ID to '%d'.\n", sender_id);
                                ns_send_NAME_ID(sock, sa, g_id, g_name, psa);
                                if (peers.count(sender_id) == 0) {
                                    ns_peer_t info;
                                    memset(&info, 0, sizeof(info));
                                    strncpy(info.name, "", strlen(""));
                                    info.last_hello = get_time();
                                    peers[sender_id] = info;
                                    printf("   Added new peer '%d' with name '%s'\n", sender_id, info.name);
                                    printf("-> GET_NAME to '%d'.\n", sender_id);
                                    ns_send_GET_NAME(sock, sa, g_id, psa, sender_id);
                                }
                            }
                            break;
                        }
                        case GET_NAME: {
                            unsigned short payload_id = ntohs(pack.payload.id);
                            printf("<- GET_NAME from '%d' to '%hd'.\n", sender_id, payload_id);
                            if (sender_id != g_id && payload_id == g_id) {
                                printf("-> NAME_ID to '%d'.\n", sender_id);
                                ns_send_NAME_ID(sock, sa, g_id, g_name, psa);
                                if (peers.count(sender_id) == 0) {
                                    ns_peer_t info;
                                    memset(&info, 0, sizeof(info));
                                    strncpy(info.name, "", strlen(""));
                                    info.last_hello = get_time();
                                    peers[sender_id] = info;
                                    printf("   Added new peer '%d' with name '%s'\n", sender_id, info.name);
                                    printf("-> GET_NAME to '%d'.\n", sender_id);
                                    ns_send_GET_NAME(sock, sa, g_id, psa, sender_id);
                                }
                            }
                            break;
                        }
                        case NAME_ID: {
                            pack.payload.name[12] = '\0';
                            printf("<- NAME_ID from '%d' with name '%s'.\n", sender_id, pack.payload.name);
                            if (sender_id != g_id) {
                                if (peers.count(sender_id) == 0) {
                                    ns_peer_t info;
                                    memset(&info, 0, sizeof(info));
                                    strncpy(info.name, "", strlen(""));
                                    info.last_hello = get_time();
                                    peers[sender_id] = info;
                                    printf("   Added new peer '%d' with name '%s'\n", sender_id, info.name);
                                } else {
                                    strncpy(peers[sender_id].name, pack.payload.name, strlen(pack.payload.name));
                                    printf("   Updated peer '%d' with name '%s'\n", sender_id, peers[sender_id].name);
                                }
                            }
                            break;
                        }
                        case START_ELECTION: {
                            printf("<- START_ELECTION from '%d'.\n", sender_id);
                            if (sender_id > g_id) {
                                printf("-> ELECTION\n");
                                ns_send_ELECTION(sock, sa, g_id);
                            }
                            //TODO:
                            break;
                        }
                        case ELECTION: {
                            printf("<- ELECTION from '%d'.\n", sender_id);
                            //TODO:
                            break;
                        }
                        case MASTER: {
                            printf("<- MASTER from '%d'.\n", sender_id);
                            //TODO:
                            break;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
