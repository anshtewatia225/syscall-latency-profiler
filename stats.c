#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include "stats.h"

static DEFINE_HASHTABLE(global_syscall_table, SYSCALL_HASH_BITS);
static DEFINE_SPINLOCK(global_lock);

static DEFINE_HASHTABLE(pid_syscall_table, PID_HASH_BITS);
static DEFINE_SPINLOCK(pid_lock);

void stats_init(void)
{
    hash_init(global_syscall_table);
    hash_init(pid_syscall_table);
}

void stats_exit(void)
{
    struct syscall_stat_entry *g_entry;
    struct pid_stat_entry *p_entry;
    struct hlist_node *tmp;
    int bkt;

    spin_lock(&global_lock);
    hash_for_each_safe(global_syscall_table, bkt, tmp, g_entry, node) {
        hash_del(&g_entry->node);
        kfree(g_entry);
    }
    spin_unlock(&global_lock);

    spin_lock(&pid_lock);
    hash_for_each_safe(pid_syscall_table, bkt, tmp, p_entry, node) {
        hash_del(&p_entry->node);
        kfree(p_entry);
    }
    spin_unlock(&pid_lock);
}

void stats_record(int syscall_nr, const char *name, pid_t pid, const char *comm, u64 latency_ns)
{
    struct syscall_stat_entry *g_entry;
    struct pid_stat_entry *p_entry;
    bool g_found = false;
    bool p_found = false;
    unsigned long flags;

    spin_lock_irqsave(&global_lock, flags);
    hash_for_each_possible(global_syscall_table, g_entry, node, syscall_nr) {
        if (g_entry->syscall_nr == syscall_nr) {
            g_entry->count++;
            g_entry->total_lat += latency_ns;
            if (latency_ns > g_entry->max_lat)
                g_entry->max_lat = latency_ns;
            g_found = true;
            break;
        }
    }
    if (!g_found) {
        g_entry = kzalloc(sizeof(*g_entry), GFP_ATOMIC);
        if (g_entry) {
            g_entry->syscall_nr = syscall_nr;
            strscpy(g_entry->name, name ? name : "unknown", sizeof(g_entry->name));
            g_entry->count = 1;
            g_entry->total_lat = latency_ns;
            g_entry->max_lat = latency_ns;
            hash_add(global_syscall_table, &g_entry->node, syscall_nr);
        }
    }
    spin_unlock_irqrestore(&global_lock, flags);

    spin_lock_irqsave(&pid_lock, flags);
    hash_for_each_possible(pid_syscall_table, p_entry, node, (unsigned int)pid) {
        if (p_entry->pid == pid && p_entry->syscall_nr == syscall_nr) {
            p_entry->count++;
            p_entry->total_lat += latency_ns;
            if (latency_ns > p_entry->max_lat)
                p_entry->max_lat = latency_ns;
            p_found = true;
            break;
        }
    }
    if (!p_found) {
        p_entry = kzalloc(sizeof(*p_entry), GFP_ATOMIC);
        if (p_entry) {
            p_entry->pid = pid;
            strscpy(p_entry->comm, comm ? comm : "unknown", sizeof(p_entry->comm));
            p_entry->syscall_nr = syscall_nr;
            strscpy(p_entry->syscall_name, name ? name : "unknown", sizeof(p_entry->syscall_name));
            p_entry->count = 1;
            p_entry->total_lat = latency_ns;
            p_entry->max_lat = latency_ns;
            hash_add(pid_syscall_table, &p_entry->node, (unsigned int)pid);
        }
    }
    spin_unlock_irqrestore(&pid_lock, flags);
}

int stats_get_global_snapshots(struct syscall_snapshot **out_snaps, int *out_count)
{
    struct syscall_stat_entry *entry;
    struct syscall_snapshot *snaps;
    int count = 0, i = 0, bkt;
    unsigned long flags;

    spin_lock_irqsave(&global_lock, flags);
    hash_for_each(global_syscall_table, bkt, entry, node)
        count++;

    if (count == 0) {
        spin_unlock_irqrestore(&global_lock, flags);
        *out_snaps = NULL;
        *out_count = 0;
        return 0;
    }

    snaps = kmalloc_array(count, sizeof(struct syscall_snapshot), GFP_ATOMIC);
    if (!snaps) {
        spin_unlock_irqrestore(&global_lock, flags);
        return -ENOMEM;
    }

    hash_for_each(global_syscall_table, bkt, entry, node) {
        if (i < count) {
            snaps[i].syscall_nr = entry->syscall_nr;
            strscpy(snaps[i].name, entry->name, sizeof(snaps[i].name));
            snaps[i].count = entry->count;
            snaps[i].max_lat = entry->max_lat;
            snaps[i].avg_lat = entry->count > 0 ? entry->total_lat / entry->count : 0;
            i++;
        }
    }
    spin_unlock_irqrestore(&global_lock, flags);

    *out_snaps = snaps;
    *out_count = i;
    return 0;
}

int stats_get_pid_snapshots(pid_t target_pid, struct syscall_snapshot **out_snaps, int *out_count)
{
    struct pid_stat_entry *entry;
    struct syscall_snapshot *snaps;
    int count = 0, i = 0;
    unsigned long flags;

    spin_lock_irqsave(&pid_lock, flags);
    hash_for_each_possible(pid_syscall_table, entry, node, (unsigned int)target_pid) {
        if (entry->pid == target_pid)
            count++;
    }

    if (count == 0) {
        spin_unlock_irqrestore(&pid_lock, flags);
        *out_snaps = NULL;
        *out_count = 0;
        return 0;
    }

    snaps = kmalloc_array(count, sizeof(struct syscall_snapshot), GFP_ATOMIC);
    if (!snaps) {
        spin_unlock_irqrestore(&pid_lock, flags);
        return -ENOMEM;
    }

    hash_for_each_possible(pid_syscall_table, entry, node, (unsigned int)target_pid) {
        if (entry->pid == target_pid && i < count) {
            snaps[i].syscall_nr = entry->syscall_nr;
            strscpy(snaps[i].name, entry->syscall_name, sizeof(snaps[i].name));
            snaps[i].count = entry->count;
            snaps[i].max_lat = entry->max_lat;
            snaps[i].avg_lat = entry->count > 0 ? entry->total_lat / entry->count : 0;
            i++;
        }
    }
    spin_unlock_irqrestore(&pid_lock, flags);

    *out_snaps = snaps;
    *out_count = i;
    return 0;
}

int stats_get_all_pid_snapshots(struct pid_snapshot **out_snaps, int *out_count)
{
    struct pid_stat_entry *entry;
    pid_t *seen_pids;
    int pid_count = 0, max_pids = 256, bkt, i, j;
    unsigned long flags;

    seen_pids = kmalloc_array(max_pids, sizeof(pid_t), GFP_KERNEL);
    if (!seen_pids)
        return -ENOMEM;

    /* First pass: collect unique PIDs */
    spin_lock_irqsave(&pid_lock, flags);
    hash_for_each(pid_syscall_table, bkt, entry, node) {
        bool found = false;
        for (i = 0; i < pid_count; i++) {
            if (seen_pids[i] == entry->pid) {
                found = true;
                break;
            }
        }
        if (!found && pid_count < max_pids)
            seen_pids[pid_count++] = entry->pid;
    }
    spin_unlock_irqrestore(&pid_lock, flags);

    if (pid_count == 0) {
        kfree(seen_pids);
        *out_snaps = NULL;
        *out_count = 0;
        return 0;
    }

    *out_snaps = kmalloc_array(pid_count, sizeof(struct pid_snapshot), GFP_KERNEL);
    if (!*out_snaps) {
        kfree(seen_pids);
        return -ENOMEM;
    }

    /* Second pass: for each unique PID, collect its syscall snapshots */
    for (i = 0; i < pid_count; i++) {
        pid_t target_pid = seen_pids[i];
        struct syscall_snapshot *snaps = NULL;
        int count = 0, idx = 0;

        spin_lock_irqsave(&pid_lock, flags);
        hash_for_each_possible(pid_syscall_table, entry, node, (unsigned int)target_pid) {
            if (entry->pid == target_pid)
                count++;
        }

        if (count > 0) {
            snaps = kmalloc_array(count, sizeof(struct syscall_snapshot), GFP_ATOMIC);
            if (snaps) {
                hash_for_each_possible(pid_syscall_table, entry, node, (unsigned int)target_pid) {
                    if (entry->pid == target_pid && idx < count) {
                        snaps[idx].syscall_nr = entry->syscall_nr;
                        strscpy(snaps[idx].name, entry->syscall_name, sizeof(snaps[idx].name));
                        snaps[idx].count = entry->count;
                        snaps[idx].max_lat = entry->max_lat;
                        snaps[idx].avg_lat = entry->count > 0 ? entry->total_lat / entry->count : 0;

                        /* grab comm from first matching entry */
                        if (idx == 0)
                            strscpy((*out_snaps)[i].comm, entry->comm, TASK_COMM_LEN);
                        idx++;
                    }
                }
            }
        }
        spin_unlock_irqrestore(&pid_lock, flags);

        (*out_snaps)[i].pid = target_pid;
        (*out_snaps)[i].snaps = snaps;
        (*out_snaps)[i].count = idx;

        /* fallback if comm wasn't set */
        if (idx == 0)
            strscpy((*out_snaps)[i].comm, "unknown", TASK_COMM_LEN);
    }

    kfree(seen_pids);
    *out_count = pid_count;
    return 0;
}