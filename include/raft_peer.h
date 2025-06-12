#ifndef RAFT_PEER_H
#define RAFT_PEER_H

typedef struct {
    char host[64];
    int port;
} peer_t;

#endif // RAFT_PEER_H