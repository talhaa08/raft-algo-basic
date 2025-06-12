#ifndef RAFT_TYPES_H
#define RAFT_TYPES_H

#include <pthread.h>

typedef enum {
    FOLLOWER,
    CANDIDATE,
    LEADER
} raft_state_t;

typedef struct {
    int term;
    int index;
    char command[256];  // placeholder command
} log_entry_t;

typedef struct raft_node {
    int currentTerm;
    int votedFor;
    raft_state_t state;
    int commitIndex;
    int lastApplied;
    log_entry_t log[1024];
    int logSize;
    int voteCount;

    volatile long long last_heartbeat_ms;   // Atomic timestamp → no mutex needed
    int election_timeout_ms;
} raft_node_t;


// RequestVote RPC arguments
typedef struct {
    int term;
    int candidateId;
    int lastLogIndex;
    int lastLogTerm;
} request_vote_args_t;

// RequestVote RPC reply
typedef struct {
    int term;
    int voteGranted;  // 0 = false, 1 = true
} request_vote_reply_t;

// AppendEntries RPC arguments
typedef struct {
    int term;
    int leaderId;
    int prevLogIndex;
    int prevLogTerm;
    int leaderCommit;
    // For heartbeat → no log entries sent yet
} append_entries_args_t;

// AppendEntries RPC reply
typedef struct {
    int term;
    int success;  // 1 = true, 0 = false
} append_entries_reply_t;

#endif // RAFT_TYPES_H
