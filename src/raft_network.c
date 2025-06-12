#include "raft_network.h"
#include "raft_types.h"
#include "raft_fsm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

enum { MSG_REQUEST_VOTE = 1, MSG_APPEND_ENTRIES = 2 };

void handle_client(int client_fd) {
    int msg_type;
    ssize_t n = read(client_fd, &msg_type, sizeof(msg_type));
    if (n != sizeof(msg_type)) {
        printf("Error reading msg_type\n");
        close(client_fd);
        return;
    }

    if (msg_type == MSG_REQUEST_VOTE) {
        request_vote_args_t args;
        request_vote_reply_t reply;

        read(client_fd, &args, sizeof(args));
        printf("Received RequestVote RPC (term %d, candidateId %d)\n", args.term, args.candidateId);

        // For now → always grant vote
        reply.term = args.term;
        reply.voteGranted = 1;

        write(client_fd, &reply, sizeof(reply));
    }
    else if (msg_type == MSG_APPEND_ENTRIES) {
        append_entries_args_t args;
        append_entries_reply_t reply;

        read(client_fd, &args, sizeof(args));
        printf("Received AppendEntries RPC (term %d, leaderId %d)\n", args.term, args.leaderId);

        reply.term = args.term;
        reply.success = 1;

        extern raft_node_t global_node;
        raft_fsm_handle(&global_node, EV_RCVD_HEARTBEAT, NULL);

        write(client_fd, &reply, sizeof(reply));
    }


    close(client_fd);
}

int send_request_vote(const char *host, int port, request_vote_args_t *args, request_vote_reply_t *reply) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    printf("Connecting to %s:%d ...\n", host, port);
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    int msg_type = MSG_REQUEST_VOTE;
    write(sockfd, &msg_type, sizeof(msg_type));
    write(sockfd, args, sizeof(*args));

    // Read reply
    ssize_t n = read(sockfd, reply, sizeof(*reply));
    if (n != sizeof(*reply)) {
        printf("Error reading RequestVoteReply\n");
        close(sockfd);
        return -1;
    }

    printf("Received vote reply: voteGranted=%d, term=%d\n", reply->voteGranted, reply->term);

    close(sockfd);
    return 0;
}


// Simple RequestVote handler
void handle_request_vote(int client_fd) {
    request_vote_args_t args;
    request_vote_reply_t reply;

    // Read args from client
    ssize_t n = read(client_fd, &args, sizeof(args));
    if (n != sizeof(args)) {
        printf("Error reading RequestVoteArgs\n");
        close(client_fd);
        return;
    }

    printf("Received RequestVote RPC (term %d, candidateId %d)\n", args.term, args.candidateId);

    // For now: always grant vote
    reply.term = args.term;
    reply.voteGranted = 1;

    // Send reply
    write(client_fd, &reply, sizeof(reply));

    close(client_fd);
}

void* server_thread(void* arg) {
    int port = *(int*)arg;
    free(arg);  // free malloc'd port arg

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Network server listening on port %d\n", port);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd >= 0) {
            handle_client(client_fd);
        }
    }
}

void start_network_server(int port) {
    pthread_t tid;
    int* port_arg = malloc(sizeof(int));
    *port_arg = port;
    pthread_create(&tid, NULL, server_thread, port_arg);
    pthread_detach(tid);
}

int send_append_entries(const char *host, int port, append_entries_args_t *args, append_entries_reply_t *reply) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    int msg_type = MSG_APPEND_ENTRIES;
    write(sockfd, &msg_type, sizeof(msg_type));
    write(sockfd, args, sizeof(*args));

    // Read reply
    ssize_t n = read(sockfd, reply, sizeof(*reply));
    if (n != sizeof(*reply)) {
        printf("Error reading AppendEntriesReply\n");
        close(sockfd);
        return -1;
    }

    printf("Received AppendEntries reply: success=%d, term=%d\n", reply->success, reply->term);

    close(sockfd);
    return 0;
}
