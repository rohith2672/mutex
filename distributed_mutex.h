#ifndef DISTRIBUTED_MUTEX_H
#define DISTRIBUTED_MUTEX_H

int distributed_mutex_init(int my_id, int num_procs, char **hostnames, int *ports, int *shared_balance);
void distributed_mutex_lock();
void distributed_mutex_unlock();

#endif
