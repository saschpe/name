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
static int g_sock;
static struct sockaddr_in g_sa;

static unsigned short g_master_id;
static unsigned short g_max_election_id;
static int g_in_election = 0;
static int g_wait_for_master = 0;
static int g_wait_again = 0;

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

/**
 * Simple helper function
 */
static void start_election()
{
    g_in_election = 1;
    g_wait_again = 1;
    g_wait_for_master = 0;
    ns_send_START_ELECTION(g_sock, g_sa, g_id);
    printf("-> START_ELECTION\n");
}

static void send_master()
{
    g_wait_for_master = 0;
    ns_send_MASTER(g_sock, g_sa, g_id);
    printf("-> MASTER\n");
}

static void send_hello()
{
    ns_send_HELLO(g_sock, g_sa, g_id);
    printf("-> HELLO\n");
}

static void peers_add(std::map<unsigned short, ns_peer_t> &peers, unsigned short id)
{
    ns_peer_t info;
    memset(&info, 0, sizeof(info));
    strncpy(info.name, "", strlen(""));
    info.last_hello = get_time();
    peers[id] = info;
    //printf("   Added new peer '%d' with name '%s'\n", id, info.name);
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
            start_election();
        }
    }
}

int main(int argc, char *argv[])
{
    g_id = getpid();
    g_master_id = g_id;
    std::map<unsigned short, ns_peer_t> peers;

    parse_cmdline_args(argc, argv);

    clock_init();
    ns_init(&g_sock, &g_sa, NS_DEFAULT_PORT);

    struct pollfd pfd[1];
    pfd[0].fd = g_sock;
    pfd[0].events = POLLIN;

    /* Send the first HELLO message to notify others of a new peer */
    send_hello();
    /* Send the first START_ELECTION message to notify others of a new peer */
    start_election();

    /* Set the the various wait timeouts for different events.
       Note that the first time out is actually an NS_ELECTION_TIMEOUT
       because we initially send an START_ELECTION packet. */
    time_val hello_wait_time = get_time() + NS_HELLO_TIMEOUT;
    time_val election_wait_time = get_time() + NS_ELECTION_TIMEOUT;

    while (1) {
        time_val poll_timestamp = get_time();
        int ret;
        if (g_in_election) {
            ret = poll(pfd, 1, poll_time(election_wait_time));
        } else {
            ret = poll(pfd, 1, poll_time(hello_wait_time));
        }

        if (ret == 0) {
            /* Generic timeout occured */
            time_val poll_diff = get_time() - poll_timestamp;

            /* Handle election timeout if in election */
            if (g_in_election) {
                hello_wait_time -= poll_diff;
                if (g_wait_for_master) {
                    printf("   Election timeout while waiting for MASTER.\n");
                    if (peers.size() == 0) {
                        send_master();  // Special case, no one is here
                    } else {
                        start_election();
                    }
                } else {
                    if (g_wait_again) {
                        g_wait_again = 0;
                        time_val election_wait_time = get_time() + NS_ELECTION_TIMEOUT;
                    } else {
                        printf("   Election timeout while waiting for ELECTION.\n");
                        send_master();
                    }
                }
                //printf("   Next election wait time: '%lld'.\n", election_wait_time);
            } else {
                /* HELLO message wait timeout, send another one */
                send_hello();
                peers_cleanup(peers);
                hello_wait_time = get_time() + NS_HELLO_TIMEOUT;
                //printf("   Next hello wait time: '%lld'\n", hello_wait_time);
            }
            //printf("   Timeout occured, hello: '%lld' and election: '%lld'\n", hello_wait_time, election_wait_time);

        } else if (ret > 0) {
            /* An event happend on one of the poll'ed file desciptors */
            if (pfd[0].revents & POLLIN) {
                struct sockaddr_in psa; socklen_t csalen = sizeof(struct sockaddr_in);
                ns_packet_t pack;

                if (recvfrom(g_sock, &pack, sizeof(ns_packet_t), 0, (struct sockaddr *)&psa, &csalen) == -1) {
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
                                    ns_send_GET_NAME(g_sock, g_sa, g_id, psa, sender_id);
                                } else {
                                    peers[sender_id].last_hello = get_time();
                                    //printf("   Updated last HELLO timestamp for peer.\n");
                                }
                            }
                            break;
                        }
                        case GET_ID: {
                            printf("<- GET_ID from '%d' to name '%s'.\n", sender_id, pack.payload.name);
                            if (sender_id != g_id && strncmp(pack.payload.name, g_name, strlen(g_name))) {
                                //printf("   Message was for me, send NAME_ID message to '%d'.\n", sender_id);
                                printf("-> NAME_ID to '%d'.\n", sender_id);
                                ns_send_NAME_ID(g_sock, g_sa, g_id, g_name, psa);
                                if (peers.count(sender_id) == 0) {
                                    peers_add(peers, sender_id);
                                    printf("-> GET_NAME to '%d'.\n", sender_id);
                                    ns_send_GET_NAME(g_sock, g_sa, g_id, psa, sender_id);
                                }
                            }
                            break;
                        }
                        case GET_NAME: {
                            unsigned short payload_id = ntohs(pack.payload.id);
                            printf("<- GET_NAME from '%d' to '%hd'.\n", sender_id, payload_id);
                            if (sender_id != g_id && payload_id == g_id) {
                                printf("-> NAME_ID to '%d'.\n", sender_id);
                                ns_send_NAME_ID(g_sock, g_sa, g_id, g_name, psa);
                                if (peers.count(sender_id) == 0) {
                                    peers_add(peers, sender_id);
                                    printf("-> GET_NAME to '%d'.\n", sender_id);
                                    ns_send_GET_NAME(g_sock, g_sa, g_id, psa, sender_id);
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
                                //printf("   Updated peer '%d' with name '%s'\n", sender_id, peers[sender_id].name);
                            }
                            break;
                        }
                        case START_ELECTION: {
                            printf("<- START_ELECTION from '%d' (%lld).\n", sender_id, get_time());
                            if (sender_id != g_id) {
                                if (peers.count(sender_id) == 0) {
                                    peers_add(peers, sender_id);
                                    printf("-> GET_NAME to '%d'.\n", sender_id);
                                    ns_send_GET_NAME(g_sock, g_sa, g_id, psa, sender_id);
                                }
                                g_in_election = 1;
                            }

                            if (sender_id < g_id) {
                                printf("-> ELECTION (%lld)\n", get_time());
                                g_wait_for_master = 0;
                                ns_send_ELECTION(g_sock, g_sa, g_id);
                                election_wait_time = get_time() + NS_ELECTION_TIMEOUT;
                            } else if (sender_id == g_id) {
                                g_wait_for_master = 0;
                                election_wait_time = get_time() + NS_ELECTION_TIMEOUT;
                            } else {
                                g_wait_for_master = 1;
                                election_wait_time = get_time() + NS_MASTER_TIMEOUT;
                            }
                            break;
                        }
                        case ELECTION: {
                            printf("<- ELECTION from '%d' (%lld).\n", sender_id, get_time());
                            if (sender_id != g_id) {
                                if (peers.count(sender_id) == 0) {
                                    peers_add(peers, sender_id);
                                    printf("-> GET_NAME to '%d'.\n", sender_id);
                                    ns_send_GET_NAME(g_sock, g_sa, g_id, psa, sender_id);
                                }
                                if (!g_in_election) {
                                    printf("   Not in an election, start a new one!\n");
                                    start_election();
                                } else {
                                    g_wait_again = 0;
                                    if (sender_id > g_id) {
                                        g_wait_for_master = 1;
                                        election_wait_time = get_time() + NS_MASTER_TIMEOUT;
                                        printf("   Someone voted higher, wait for MASTER.\n");
                                    }
                                }
                            }
                            break;
                        }
                        case MASTER: {
                            printf("<- MASTER from '%d' (%lld).\n", sender_id, get_time());
                            if (sender_id != g_id) {
                                if (peers.count(sender_id) == 0) {
                                    peers_add(peers, sender_id);
                                    printf("-> GET_NAME to '%d'.\n", sender_id);
                                    ns_send_GET_NAME(g_sock, g_sa, g_id, psa, sender_id);
                                }
                            }
                            if (!g_in_election || sender_id < g_id) {
                                start_election();
                            } else {
                                g_in_election = 0;
                                g_wait_again = 0;
                                g_wait_for_master = 0;
                                g_master_id = sender_id;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
