#ifndef DISTRIBUTED_MUTEX_H
#define DISTRIBUTED_MUTEX_H

/*
 * Distributed Mutex Module Interface
 *
 * Provides a distributed mutual exclusion mechanism using structured messages
 * (defined in msg_packet.h) and UDP sockets.
 *
 * Parameters:
 *   local_id       - The local process's ID.
 *   total_procs    - Total number of processes.
 *   host_list      - Array of hostnames for each process.
 *   port_list      - Array of UDP port numbers corresponding to each process.
 *   acct_ptr       - Pointer to the shared account balance variable.
 *
 * The API includes:
 *   dm_init()          - Initializes the module and spawns the listener thread.
 *   dm_lock()          - Enters the critical section by obtaining the distributed lock.
 *   dm_unlock()        - Exits the critical section.
 *   dm_update_balance() - Broadcasts an updated account balance.
 */
int dm_init(int local_id, int total_procs, char **host_list, int *port_list, int *acct_ptr);
void dm_lock();
void dm_unlock();
void dm_update_balance();

#endif /* DISTRIBUTED_MUTEX_H */
