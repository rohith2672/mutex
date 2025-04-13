#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>  // For hostname resolution
#include "distributed_mutex.h"
#include "msg_packet.h"

#define MAX_MSG_LEN 256

/* Global variables */
static int local_id, total_procs;
static char **host_list;
static int *port_list;
static int sock_fd;
static sem_t sem_reply;
static int *acct_ptr;
static unsigned short sequence_num = 0;
static unsigned short vec_clock[5] = {0};

/* Function prototypes */
static void *listener_thread(void *arg);
static void send_request();
static void send_reply(int dest);
static void send_balance_update();
static void handle_request(const msg_packet *msg);
static void handle_reply(const msg_packet *msg);

/* Initialize the distributed mutex module and spawn listener thread */
int dm_init(int l_id, int tot_procs, char **hosts, int *ports, int *balance_ptr) {
    local_id = l_id;
    total_procs = tot_procs;
    host_list = hosts;
    port_list = ports;
    acct_ptr = balance_ptr;

    sem_init(&sem_reply, 0, 0);
    memset(vec_clock, 0, sizeof(vec_clock));

    struct sockaddr_in addr;
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_list[local_id]);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, listener_thread, NULL)) {
        perror("pthread_create");
        return -1;
    }
    printf("[P%d] Listener started.\n", local_id);
    fflush(stdout);
    return 0;
}

void dm_lock() {
    printf("[P%d] Sending REQUEST messages...\n", local_id);
    fflush(stdout);
    send_request();

    for (int i = 0; i < total_procs - 1; i++) {
        sem_wait(&sem_reply);
    }
    printf("[P%d] Acquired lock.\n", local_id);
    fflush(stdout);
}

void dm_unlock() {
    printf("[P%d] Releasing lock.\n", local_id);
    fflush(stdout);
}

void dm_update_balance() {
    printf("[P%d] Broadcasting updated account balance: %d\n", local_id, *acct_ptr);
    fflush(stdout);
    send_balance_update();
}

/* Listener thread: continuously receives messages */
static void *listener_thread(void *arg) {
    (void)arg;
    msg_packet msg;
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);

    while (1) {
        ssize_t len = recvfrom(sock_fd, &msg, sizeof(msg_packet), 0,
                                (struct sockaddr *)&src_addr, &addrlen);
        if (len > 0) {
            switch (msg.command) {
                case CMD_REQUEST:
                    handle_request(&msg);
                    break;
                case CMD_REPLY:
                    handle_reply(&msg);
                    break;
                case CMD_HELLO:
                    /* Optional: handle HELLO messages */
                    break;
                case CMD_IN_CS:
                    printf("[P%d] Received IN_CS from P%d: updating account balance to %u\n",
                           local_id, msg.host_id, msg.acct_balance);
                    fflush(stdout);
                    *acct_ptr = msg.acct_balance;
                    break;
                default:
                    break;
            }
        }
    }
    return NULL;
}

/* Helper function: resolve a hostname to fill in destination address */
static int fill_addr_from_hostname(const char *hostname, struct sockaddr_in *addr) {
    struct hostent *he = gethostbyname(hostname);
    if (he == NULL) {
        herror("gethostbyname");
        return -1;
    }
    memcpy(&addr->sin_addr, he->h_addr_list[0], he->h_length);
    return 0;
}

/* Sends a REQUEST message to all other processes */
static void send_request() {
    msg_packet msg;
    msg.command = CMD_REQUEST;
    msg.sequence = sequence_num++;
    msg.tie_break = (unsigned int)getpid();
    msg.host_id = local_id;
    memcpy(msg.vec_clock, vec_clock, sizeof(vec_clock));
    msg.acct_balance = 0;  /* Not used for REQUEST */

    for (int i = 0; i < total_procs; i++) {
        if (i == local_id)
            continue;
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port_list[i]);
        if (fill_addr_from_hostname(host_list[i], &dest_addr) < 0)
            continue;
        sendto(sock_fd, &msg, sizeof(msg_packet), 0,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    }
}

/* Sends a REPLY message to a specific process */
static void send_reply(int dest) {
    msg_packet msg;
    msg.command = CMD_REPLY;
    msg.sequence = sequence_num++;
    msg.tie_break = (unsigned int)getpid();
    msg.host_id = local_id;
    memcpy(msg.vec_clock, vec_clock, sizeof(vec_clock));
    msg.acct_balance = 0;  /* Not used for REPLY */

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_list[dest]);
    if (fill_addr_from_hostname(host_list[dest], &dest_addr) < 0)
        return;
    sendto(sock_fd, &msg, sizeof(msg_packet), 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}

/* Broadcasts the updated balance via an IN_CS message */
static void send_balance_update() {
    msg_packet msg;
    msg.command = CMD_IN_CS;
    msg.sequence = sequence_num++;
    msg.tie_break = (unsigned int)getpid();
    msg.host_id = local_id;
    memcpy(msg.vec_clock, vec_clock, sizeof(vec_clock));
    msg.acct_balance = *acct_ptr;

    for (int i = 0; i < total_procs; i++) {
        if (i == local_id)
            continue;
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port_list[i]);
        if (fill_addr_from_hostname(host_list[i], &dest_addr) < 0)
            continue;
        sendto(sock_fd, &msg, sizeof(msg_packet), 0,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    }
}

/* Handle an incoming REQUEST by immediately sending a REPLY */
static void handle_request(const msg_packet *msg) {
    printf("[P%d] Received REQUEST from P%d\n", local_id, msg->host_id);
    fflush(stdout);
    send_reply(msg->host_id);
}

/* Handle an incoming REPLY by posting to the semaphore */
static void handle_reply(const msg_packet *msg) {
    printf("[P%d] Received REPLY from P%d\n", local_id, msg->host_id);
    fflush(stdout);
    sem_post(&sem_reply);
}
