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

static void peers_add(std::map<unsigned short, ns_peer_t> &peers, unsigned short id)
{
    ns_peer_t info;
    memset(&info, 0, sizeof(info));
    strncpy(info.name, "", strlen(""));
    info.last_hello = get_time();
    peers[id] = info;
    printf("   Added new peer '%d' with name '%s'\n", id, info.name);
}

static void peers_cleanup(std::map<unsigned short, ns_peer_t> &peers)
{
    //printf("   Check for clients which have not sent a HELLO recently...\n");
    time_val now_time = get_time();
    for (std::map<unsigned short, ns_peer_t>::iterator it = peers.begin(); it != peers.end(); it++) {
        // Difference is current time minus last time seen
        if ((now_time - (*it).second.last_hello) > NS_HELLO_LAST_TIME_DIFFERENCE) {
            printf("   Missing HELLO from '%d', remove from list\n", (*it).first);
            peers.erase(it);
        }
    }
}

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in sa;

    g_id = getpid();
    master_id = g_id;
    parse_cmdline_args(argc, argv);
    std::map<unsigned short, ns_peer_t> peers;

    clock_init();
    ns_init(&sock, &sa, NS_DEFAULT_PORT);

    struct pollfd pfd[1];
    pfd[0].fd = sock;
    pfd[0].events = POLLIN;

    /* Set the the various wait timeouts for different events */
    time_val remaining_hello_wait_time = get_time() + NS_HELLO_TIMEOUT;
    time_val remaining_election_wait_time = 0;
    time_val remaining_wait_time = remaining_hello_wait_time;

    /* Simple state tracking (whish: state machine) */
    int in_election = 0;
    int wait_for_master = 0;

    /* Send the first HELLO message to notify others of a new peer */
    ns_send_HELLO(sock, sa, g_id);
    printf("-> HELLO\n");
    /* Send the first START_ELECTION message to notify others of a new peer */
    ns_send_START_ELECTION(sock, sa, g_id);

    while (1) {
        //printf("   Next HELLO wait time: '%lld'\n", remaining_hello_wait_time);
        time_val poll_timestamp = get_time();
        int ret = poll(pfd, 1, poll_time(remaining_wait_time));

        if (ret == 0) {
            /* Generic timeout occured */
            time_val poll_diff = poll_timestamp - get_time();

            /* Handle hello timeout */
            remaining_hello_wait_time -= poll_diff;
            if (remaining_hello_wait_time <= 0) {
                /* HELLO message wait timeout, send another one */
                ns_send_HELLO(sock, sa, g_id);
                printf("-> HELLO\n");

                peers_cleanup(peers);

                remaining_hello_wait_time = get_time() + NS_HELLO_TIMEOUT;
                remaining_wait_time = remaining_hello_wait_time;
            }

            /* Handle election timeout if in election */
            if (in_election) {
                remaining_election_wait_time -= poll_diff;
                if (remaining_election_wait_time <= 0) {
                    printf("   Election timeout occured!\n");
                    // TODO: React on this
                }
                remaining_wait_time = remaining_election_wait_time;
            }

        } else if (ret > 0) {
            /* An event happend on one of the poll'ed file desciptors */
            if (pfd[0].revents & POLLIN) {
                struct sockaddr_in psa; socklen_t csalen = sizeof(struct sockaddr_in);
                ns_packet_t pack;

                if (recvfrom(sock, &pack, sizeof(ns_packet_t), 0, (struct sockaddr *)&psa, &csalen) == -1) {
                    fprintf(stderr, "Error: Unable to read datagram!\n");
                    perror("recvfrom");
                } else {
                    unsigned short sender_id = ntohs(pack.sender_id);
                    switch (ntohs(pack.type)) {
                        case HELLO: {
                            printf("<- HELLO from '%d'.\n", sender_id);
                            if (sender_id != g_id) {
                                if (peers.count(sender_id) == 0 || strlen(peers[sender_id].name) == 0) {
                                    peers_add(peers, sender_id);
                                    printf("-> GET_NAME to '%d'.\n", sender_id);
                                    ns_send_GET_NAME(sock, sa, g_id, psa, sender_id);
                                } else {
                                    peers[sender_id].last_hello = get_time();
                                    printf("   Updated last HELLO timestamp for peer.\n");
                                }
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
                                    peers_add(peers, sender_id);
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
                                    peers_add(peers, sender_id);
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
                                    peers_add(peers, sender_id);
                                }
                                strncpy(peers[sender_id].name, pack.payload.name, strlen(pack.payload.name));
                                printf("   Updated peer '%d' with name '%s'\n", sender_id, peers[sender_id].name);
                            }
                            break;
                        }
                        case START_ELECTION: {
                            /* Set state */
                            in_election = 1;

                            printf("<- START_ELECTION from '%d'.\n", sender_id);
                            if (sender_id >= g_id) {
                                printf("-> ELECTION\n");
                                wait_for_master = 0;
                                ns_send_ELECTION(sock, sa, g_id);
                                remaining_election_wait_time = get_time() + NS_ELECTION_TIMEOUT;
                            } else {
                                wait_for_master = 1;
                                remaining_election_wait_time = get_time() + NS_MASTER_TIMEOUT;
                            }
                            break;
                        }
                        case ELECTION: {
                            printf("<- ELECTION from '%d'.\n", sender_id);
                            //TODO:
                            break;
                        }
                        case MASTER: {
                            /* Set state */
                            in_election = 0;
                            wait_for_master = 0;

                            master_id = sender_id;
                            printf("<- MASTER from '%d'.\n", sender_id);
                            break;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
