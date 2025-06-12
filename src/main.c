#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // for sleep
#include "raft_types.h"
#include "raft_fsm.h"
#include "raft_network.h"
#include "raft_peer.h"

#define MAX_PEERS 10

raft_node_t global_node;
peer_t peers[MAX_PEERS];
int num_peers = 0;
int my_id;


// Helper: parse comma-separated peers string
void parse_peers(const char *peers_str) {
    char *peers_copy = strdup(peers_str);
    char *token = strtok(peers_copy, ",");
    while (token && num_peers < MAX_PEERS) {
        char *colon = strchr(token, ':');
        if (!colon) {
            fprintf(stderr, "Invalid peer format: %s\n", token);
            exit(1);
        }
        *colon = '\0';
        strcpy(peers[num_peers].host, token);
        peers[num_peers].port = atoi(colon + 1);
        num_peers++;
        token = strtok(NULL, ",");
    }
    free(peers_copy);
}

void* election_timer_thread(void* arg) {
    while (1) {
        usleep(10000);  // check every 10ms

        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);

        long long now_ms = now.tv_sec * 1000LL + now.tv_nsec / 1000000;
        long long elapsed_ms = now_ms - global_node.last_heartbeat_ms;

        if (global_node.state != LEADER && elapsed_ms >= global_node.election_timeout_ms) {
            printf("Election timer expired → triggering election (elapsed %lld ms)\n", elapsed_ms);
            raft_fsm_handle(&global_node, EV_TIMEOUT, NULL);

            // Reset election timeout and last_heartbeat_ms
            global_node.election_timeout_ms = 150 + rand() % 150;
            global_node.last_heartbeat_ms = now_ms;
        }
    }

    return NULL;
}



int main(int argc, char **argv) {
    int my_port = -1;
    const char *peers_str = NULL;

    global_node.currentTerm = 0;
    global_node.votedFor = -1;
    global_node.state = FOLLOWER;
    global_node.commitIndex = 0;
    global_node.lastApplied = 0;
    global_node.logSize = 0;
    global_node.voteCount = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--id=", 5) == 0)
        {
            my_id = atoi(argv[i] + 5);
        }
        else if (strncmp(argv[i], "--port=", 7) == 0)
        {
            my_port = atoi(argv[i] + 7);
        }
        else if (strncmp(argv[i], "--peers=", 8) == 0)
        {
            peers_str = argv[i] + 8;
        }
    }

    if (my_id < 0 || my_port < 0 || !peers_str) {
        fprintf(stderr, "Usage: %s --id=N --port=PORT --peers=host1:port1,host2:port2,...\n", argv[0]);
        exit(1);
    }

    srand(time(NULL) + my_id);
    global_node.election_timeout_ms = 150 + rand() % 150;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    global_node.last_heartbeat_ms = now.tv_sec * 1000LL + now.tv_nsec / 1000000;


    parse_peers(peers_str);

    printf("Node ID: %d\n", my_id);
    printf("Listening on port: %d\n", my_port);
    printf("Peers:\n");
    for (int i = 0; i < num_peers; i++) {
        printf("  %s:%d\n", peers[i].host, peers[i].port);
    }

    // Start network server in background thread
    start_network_server(my_port);

    pthread_t timer_thread;
    pthread_create(&timer_thread, NULL, election_timer_thread, NULL);


    // Initialize raft node
    raft_node_t node;
    node.currentTerm = 0;
    node.votedFor = -1;
    node.state = FOLLOWER;
    node.commitIndex = 0;
    node.lastApplied = 0;
    node.logSize = 0;

    printf("Starting node in FOLLOWER state\n");

    while (1) {
        usleep(50000);

        // Only LEADER sends heartbeats
        if (global_node.state == LEADER) {
            printf("Leader: Sending heartbeat\n");

            for (int j = 0; j < num_peers; j++) {
                printf("Sending AppendEntries RPC to %s:%d (term %d)\n",
                    peers[j].host, peers[j].port, global_node.currentTerm);

                append_entries_args_t args;
                args.term = global_node.currentTerm;
                args.leaderId = my_id;
                args.prevLogIndex = global_node.logSize - 1;
                args.prevLogTerm = (global_node.logSize > 0)
                                ? global_node.log[global_node.logSize - 1].term
                                : 0;
                args.leaderCommit = global_node.commitIndex;

                append_entries_reply_t reply;
                int ret = send_append_entries(peers[j].host, peers[j].port, &args, &reply);

                if (ret == 0) {
                    printf("AppendEntries RPC to %s:%d success=%d, term=%d\n",
                        peers[j].host, peers[j].port, reply.success, reply.term);
                } else {
                    printf("AppendEntries RPC to %s:%d failed (ret=%d)\n",
                        peers[j].host, peers[j].port, ret);
                }
            }
        }

        // No need to fire EV_TIMEOUT → the election timer thread already handles that!
    }
    return 0;
}
