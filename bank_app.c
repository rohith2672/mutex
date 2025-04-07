#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "distributed_mutex.h"

#define MAX_PROCESSES 10
#define MAX_LINE_LEN 100

char *hostnames[MAX_PROCESSES];
int ports[MAX_PROCESSES];

int read_hosts_file(const char *filename, int *num_procs) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    int id;
    char hostname[100];
    int port;

    *num_procs = 0;
    while (fscanf(fp, "%d %s %d", &id, hostname, &port) != EOF) {
        hostnames[id] = strdup(hostname);
        ports[id] = port;
        (*num_procs)++;
    }

    fclose(fp);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <process_id>\n", argv[0]);
        exit(1);
    }

    int process_id = atoi(argv[1]);
    int num_processes;

    if (read_hosts_file("process.hosts", &num_processes) < 0) {
        fprintf(stderr, "Failed to read process.hosts\n");
        exit(1);
    }

    int balance = 1000;
    distributed_mutex_init(process_id, num_processes, hostnames, ports, &balance);

    char input;
    printf("[P%d] Start test? (y/n): ", process_id);
    scanf(" %c", &input);
    if (input != 'y') return 0;

    for (int i = 0; i < 10; i++) {
        sleep(5);
        distributed_mutex_lock();

        printf("[P%d] Old Balance: %d\n", process_id, balance);
        balance += 100;
        printf("[P%d] New Balance: %d\n", process_id, balance);

        distributed_mutex_unlock();

        for (int j = 0; j < num_processes; j++) {
            if (j != process_id)
                printf("[P%d] Balance updated to %d from P%d\n\n", process_id, balance, process_id);
        }
    }

    return 0;
}
