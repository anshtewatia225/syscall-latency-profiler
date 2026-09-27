#ifndef STATS_H
#define STATS_H

#include <linux/types.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>

#define SYSCALL_HASH_BITS 8
#define PID_HASH_BITS 8

struct syscall_stat_entry {
    int syscall_nr;
    char name[32];
    u64 count;
    u64 total_lat;
    u64 max_lat;
    struct hlist_node node;
};

struct pid_stat_entry {
    pid_t pid;
    char comm[TASK_COMM_LEN];
    int syscall_nr;
    char syscall_name[32];
    u64 count;
    u64 total_lat;
    u64 max_lat;
    struct hlist_node node;
};

struct syscall_snapshot {
    int syscall_nr;
    char name[32];
    u64 count;
    u64 avg_lat;
    u64 max_lat;
};

struct pid_snapshot {
    pid_t pid;
    char comm[TASK_COMM_LEN];
    struct syscall_snapshot *snaps;
    int count;
};

void stats_init(void);
void stats_exit(void);
void stats_record(int syscall_nr, const char *name, pid_t pid, const char *comm, u64 latency_ns);

int stats_get_global_snapshots(struct syscall_snapshot **out_snaps, int *out_count);
int stats_get_pid_snapshots(pid_t target_pid, struct syscall_snapshot **out_snaps, int *out_count);
int stats_get_all_pid_snapshots(struct pid_snapshot **out_snaps, int *out_count);

#endif /* STATS_H */