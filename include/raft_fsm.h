#ifndef RAFT_FSM_H
#define RAFT_FSM_H

#include "raft_types.h"

// Forward declare node type
typedef struct raft_node raft_node_t;

// Events
typedef enum {
    EV_TIMEOUT,
    EV_RCVD_HEARTBEAT,
    EV_RCVD_VOTE_REQ,
    EV_RCVD_VOTE_RESP
} raft_event_t;

void raft_fsm_handle(raft_node_t *node, raft_event_t event, void *msg);
void become_follower(raft_node_t *node, int term);
void become_candidate(raft_node_t *node);
void become_leader(raft_node_t *node);

#endif // RAFT_FSM_H
