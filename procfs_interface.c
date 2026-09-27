#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/fs.h>
#include "stats.h"

static struct proc_dir_entry *proc_dir = NULL;
static struct proc_dir_entry *proc_global = NULL;
static struct proc_dir_entry *proc_pids = NULL;

static int sort_cmp(const void *a, const void *b)
{
    const struct syscall_snapshot *sa = (const struct syscall_snapshot *)a;
    const struct syscall_snapshot *sb = (const struct syscall_snapshot *)b;
    if (sb->avg_lat > sa->avg_lat) return 1;
    if (sb->avg_lat < sa->avg_lat) return -1;
    return 0;
}

static int global_proc_show(struct seq_file *m, void *v)
{
    struct syscall_snapshot *snaps = NULL;
    int count = 0, ret, i;

    ret = stats_get_global_snapshots(&snaps, &count);
    if (ret < 0) return ret;

    if (count > 0 && snaps)
        sort(snaps, count, sizeof(struct syscall_snapshot), sort_cmp, NULL);

    seq_printf(m, "%-20s | %-10s | %-15s | %-15s\n",
               "SYSCALL", "COUNT", "AVG_LAT(ns)", "MAX_LAT(ns)");
    seq_printf(m, "--------------------------------------------------------------------\n");

    for (i = 0; i < count; i++) {
        seq_printf(m, "%-20s | %-10llu | %-15llu | %-15llu\n",
                   snaps[i].name, snaps[i].count,
                   snaps[i].avg_lat, snaps[i].max_lat);
    }

    kfree(snaps);
    return 0;
}

static int global_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, global_proc_show, NULL);
}

static const struct proc_ops global_proc_ops = {
    .proc_open    = global_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* /proc/syscall_latency/pids -- shows all tracked PIDs with per-process breakdown */
static int pids_proc_show(struct seq_file *m, void *v)
{
    struct pid_snapshot *pid_snaps = NULL;
    int pid_count = 0, ret, i, j;

    ret = stats_get_all_pid_snapshots(&pid_snaps, &pid_count);
    if (ret < 0) return ret;

    for (i = 0; i < pid_count; i++) {
        if (pid_snaps[i].count > 0 && pid_snaps[i].snaps) {
            sort(pid_snaps[i].snaps, pid_snaps[i].count,
                 sizeof(struct syscall_snapshot), sort_cmp, NULL);
        }

        seq_printf(m, "\nPID: %d (%s)\n", pid_snaps[i].pid, pid_snaps[i].comm);
        seq_printf(m, "%-20s | %-10s | %-15s | %-15s\n",
                   "SYSCALL", "COUNT", "AVG_LAT(ns)", "MAX_LAT(ns)");
        seq_printf(m, "--------------------------------------------------------------------\n");

        for (j = 0; j < pid_snaps[i].count; j++) {
            seq_printf(m, "%-20s | %-10llu | %-15llu | %-15llu\n",
                       pid_snaps[i].snaps[j].name,
                       pid_snaps[i].snaps[j].count,
                       pid_snaps[i].snaps[j].avg_lat,
                       pid_snaps[i].snaps[j].max_lat);
        }
        kfree(pid_snaps[i].snaps);
    }

    kfree(pid_snaps);
    return 0;
}

static int pids_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, pids_proc_show, NULL);
}

static const struct proc_ops pids_proc_ops = {
    .proc_open    = pids_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

int procfs_init(void)
{
    proc_dir = proc_mkdir("syscall_latency", NULL);
    if (!proc_dir) return -ENOMEM;

    proc_global = proc_create("global", 0444, proc_dir, &global_proc_ops);
    if (!proc_global) {
        remove_proc_entry("syscall_latency", NULL);
        return -ENOMEM;
    }

    proc_pids = proc_create("pids", 0444, proc_dir, &pids_proc_ops);
    if (!proc_pids) {
        remove_proc_entry("global", proc_dir);
        remove_proc_entry("syscall_latency", NULL);
        return -ENOMEM;
    }

    return 0;
}

void procfs_exit(void)
{
    remove_proc_entry("pids", proc_dir);
    remove_proc_entry("global", proc_dir);
    remove_proc_entry("syscall_latency", NULL);
}