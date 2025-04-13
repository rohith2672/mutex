#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "distributed_mutex.h"

#define MAX_PROCS 10
#define MAX_LINE  100

char *host_list[MAX_PROCS];
int port_list[MAX_PROCS];

/*
 * Loads the hosts file which should have lines formatted as:
 * <hostid> <hostname> <port>
 * For example:
 *   0 server0 5000
 *   1 server1 5001
 */
int load_hosts(const char *filename, int *num_hosts) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    int id, port;
    char hostname[100];
    *num_hosts = 0;
    while (fscanf(fp, "%d %s %d", &id, hostname, &port) != EOF) {
        host_list[id] = strdup(hostname);
        port_list[id] = port;
        (*num_hosts)++;
    }
    fclose(fp);
    return 0;
}

/* Thread function: print the current account balance every 3 seconds */
void *balance_display(void *arg) {
    int *balance_ptr = (int *)arg;
    while (1) {
        sleep(3);
        printf(">>> Current Account Balance: %d\n", *balance_ptr);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <proc_id> <op_mode>\n", argv[0]);
        printf("op_mode: 0 = Balance Check, 1 = Deposit, 2 = Withdrawal\n");
        exit(1);
    }

    int proc_id = atoi(argv[1]);
    int op_mode = atoi(argv[2]);  // 0: check; 1: deposit; 2: withdrawal
    int total_hosts;

    if (load_hosts("process.hosts", &total_hosts) < 0) {
        fprintf(stderr, "Failed to load hosts\n");
        exit(1);
    }

    int account_balance = 1000;

    if (dm_init(proc_id, total_hosts, host_list, port_list, &account_balance) < 0) {
        fprintf(stderr, "dm_init() failed\n");
        exit(1);
    }

    char user_input;
    printf("[P%d] Start test? (y/n): ", proc_id);
    fflush(stdout);
    scanf(" %c", &user_input);
    if (user_input != 'y')
        return 0;

    pthread_t disp_thread;
    pthread_create(&disp_thread, NULL, balance_display, &account_balance);

    for (int i = 0; i < 20; i++) {
        printf("[P%d] -- Iteration %d begin --\n", proc_id, i);
        fflush(stdout);
        dm_lock();

        switch (op_mode) {
            case 0:
                printf("[P%d][Iter %d] Balance Check: %d\n", proc_id, i, account_balance);
                break;
            case 1:
                printf("[P%d][Iter %d] Deposit: +100\n", proc_id, i);
                account_balance += 100;
                break;
            case 2:
                printf("[P%d][Iter %d] Withdrawal: -50\n", proc_id, i);
                account_balance -= 50;
                break;
            default:
                printf("[P%d] Unknown operation mode\n", proc_id);
                break;
        }
        fflush(stdout);

        dm_update_balance();
        dm_unlock();

        sleep(1);
    }

    pthread_join(disp_thread, NULL);

    return 0;
}
