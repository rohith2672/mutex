#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "distributed_mutex.h"

#define MAX_PROCESSES 10
#define MAX_MSG_LEN 256

typedef struct {
    int id;
    int timestamp[MAX_PROCESSES];
} Request;

int my_id, num_procs;
char **all_hostnames;
int *all_ports;
int vector_clock[MAX_PROCESSES];
int socket_fd;
sem_t mutex_sem;
int *shared_balance;

void *listener_thread(void *arg);
void send_request();
void send_reply(int to);
void send_balance_update();
void handle_request(int from);
void handle_reply(int from);

int distributed_mutex_init(int id, int nprocs, char **hostnames, int *ports, int *balance_ptr) {
    my_id = id;
    num_procs = nprocs;
    all_hostnames = hostnames;
    all_ports = ports;
    shared_balance = balance_ptr;

    sem_init(&mutex_sem, 0, 0);
    memset(vector_clock, 0, sizeof(vector_clock));

    struct sockaddr_in addr;
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(all_ports[my_id]);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }

    pthread_t t;
    pthread_create(&t, NULL, listener_thread, NULL);

    printf("[P%d] Listener started.\n", my_id);
    return 0;
}

void distributed_mutex_lock() {
    printf("[P%d] Sending REQUESTs...\n", my_id);
    send_request();

    for (int i = 0; i < num_procs - 1; i++) {
        sem_wait(&mutex_sem);
    }

    printf("[P%d] Acquired lock.\n", my_id);

    // Update balance
    printf("[P%d] Old Balance: %d\n", my_id, *shared_balance);
    *shared_balance += 100;  // Update the balance
    printf("[P%d] New Balance: %d\n", my_id, *shared_balance);

    // Broadcast the updated balance
    send_balance_update();
}

void distributed_mutex_unlock() {
    printf("[P%d] Releasing lock.\n", my_id);
}

void *listener_thread(void *arg) {
    char buffer[MAX_MSG_LEN];
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);

    while (1) {
        ssize_t len = recvfrom(socket_fd, buffer, MAX_MSG_LEN, 0, (struct sockaddr *)&src_addr, &addrlen);
        if (len > 0) {
            buffer[len] = '\0';

            if (strncmp(buffer, "REQUEST", 7) == 0) {
                int from;
                sscanf(buffer + 8, "%d", &from);
                handle_request(from);
            } else if (strncmp(buffer, "REPLY", 5) == 0) {
                int from;
                sscanf(buffer + 6, "%d", &from);
                handle_reply(from);
            } else if (strncmp(buffer, "BALANCE", 7) == 0) {
                int new_balance;
                sscanf(buffer + 8, "%d", &new_balance);
                *shared_balance = new_balance; // Update local balance

                // Get the sender's IP address or other identifier
                int from = src_addr.sin_port;  // Or extract a more meaningful identifier here if needed.

                printf("[P%d] Updated Balance from P%d: %d\n", my_id, from, *shared_balance);
            }
        }
    }
    return NULL;
}

void send_request() {
    char msg[MAX_MSG_LEN];
    sprintf(msg, "REQUEST %d", my_id);

    for (int i = 0; i < num_procs; i++) {
        if (i == my_id) continue;

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(all_ports[i]);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        sendto(socket_fd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(addr));
    }
}

void send_reply(int to) {
    char msg[MAX_MSG_LEN];
    sprintf(msg, "REPLY %d", my_id);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(all_ports[to]);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    sendto(socket_fd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(addr));
}

void send_balance_update() {
    char msg[MAX_MSG_LEN];
    sprintf(msg, "BALANCE %d", *shared_balance);

    for (int i = 0; i < num_procs; i++) {
        if (i == my_id) continue;

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(all_ports[i]);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        sendto(socket_fd, msg, strlen(msg), 0, (struct sockaddr *)&addr, sizeof(addr));
    }
}

void handle_request(int from) {
    printf("[P%d] Received REQUEST from P%d\n", my_id, from);
    send_reply(from);
}

void handle_reply(int from) {
    printf("[P%d] Received REPLY from P%d\n", my_id, from);
    sem_post(&mutex_sem);
}
