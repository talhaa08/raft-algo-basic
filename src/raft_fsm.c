#include "raft_fsm.h"
#include "raft_types.h"
#include "raft_peer.h"
#include "raft_network.h"
#include <stdio.h>

extern int num_peers;
extern peer_t peers[];
extern int my_id;

void become_follower(raft_node_t *node, int term) {
    node->state = FOLLOWER;
    node->currentTerm = term;
    node->votedFor = -1;
    printf("Transition to FOLLOWER (term %d)\n", term);
}

void become_candidate(raft_node_t *node) {
    node->state = CANDIDATE;
    node->currentTerm++;
    node->votedFor = my_id;
    node->voteCount = 1;

    // Reset election timeout FIRST → to prevent recursion
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    node->last_heartbeat_ms = now.tv_sec * 1000LL + now.tv_nsec / 1000000;

    printf("Transition to CANDIDATE (term %d)\n", node->currentTerm);
    printf("Candidate %d: sending RequestVote to %d peers\n", my_id, num_peers);

    for (int j = 0; j < num_peers; j++) {
        printf("Sending RequestVote RPC to %s:%d (term %d)\n",
               peers[j].host, peers[j].port, node->currentTerm);

        request_vote_args_t args;
        args.term = node->currentTerm;
        args.candidateId = my_id;
        args.lastLogIndex = node->logSize - 1;
        args.lastLogTerm = (node->logSize > 0) ? node->log[node->logSize - 1].term : 0;

        request_vote_reply_t reply;
        int ret = send_request_vote(peers[j].host, peers[j].port, &args, &reply);

        if (ret == 0) {
            printf("Received vote reply: voteGranted=%d, term=%d\n", reply.voteGranted, reply.term);

            int voteGranted = reply.voteGranted;
            raft_fsm_handle(node, EV_RCVD_VOTE_RESP, &voteGranted);
        } else {
            printf("RequestVote RPC to %s:%d failed (ret=%d)\n", peers[j].host, peers[j].port, ret);
        }
    }
}

void become_leader(raft_node_t *node) {
    node->state = LEADER;
    printf("Transition to LEADER (term %d)\n", node->currentTerm);

    // Here you'd send initial empty AppendEntries to peers (heartbeat)
}

void raft_fsm_handle(raft_node_t *node, raft_event_t event, void *msg) {
    switch (node->state) {
        case FOLLOWER: {
            if (event == EV_TIMEOUT) {
                become_candidate(node);
            } else if (event == EV_RCVD_HEARTBEAT) {
                struct timespec now;
                clock_gettime(CLOCK_REALTIME, &now);
                node->last_heartbeat_ms = now.tv_sec * 1000LL + now.tv_nsec / 1000000;

                printf("Follower: Received heartbeat → resetting timeout\n");
            }
            break;
        }

        case CANDIDATE: {
            if (event == EV_TIMEOUT) {
                become_candidate(node);  // restart election
            } else if (event == EV_RCVD_VOTE_RESP) {
                int voteGranted = *((int*)msg);
                if (voteGranted) {
                    node->voteCount++;
                    printf("Candidate: Vote granted! Current vote count = %d\n", node->voteCount);
                }

                int majority = (num_peers + 1) / 2 + 1;
                if (node->voteCount >= majority) {
                    become_leader(node);
                }
            }
            break;
        }

        case LEADER: {
            if (event == EV_TIMEOUT) {
                printf("Leader: Sending heartbeat\n");
                // heartbeat is handled in main loop now
            }
            break;
        }
    }
}


