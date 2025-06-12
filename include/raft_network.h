#ifndef RAFT_NETWORK_H
#define RAFT_NETWORK_H

#include "raft_types.h"

// Send RequestVote RPC to a peer (given by host and port)
// Returns 0 on success, -1 on failure
int send_request_vote(const char *host, int port, request_vote_args_t *args, request_vote_reply_t *reply);

// Starts the network server on given port (blocking call, run in thread!)
void start_network_server(int port);

// Send AppendEntries RPC to peer
int send_append_entries(const char *host, int port, append_entries_args_t *args, append_entries_reply_t *reply);

#endif // RAFT_NETWORK_H
