#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("System Engineer");
MODULE_DESCRIPTION("Syscall Latency Profiler using Kprobes and Procfs");
MODULE_VERSION("1.0");

// Forward declarations
void stats_init(void);
void stats_exit(void);
int probe_hooks_init(void);
void probe_hooks_exit(void);
int procfs_init(void);
void procfs_exit(void);

static int __init latency_profiler_init(void)
{
    int ret;

    pr_info("syscall_latency_profiler: initializing module...\n");

    stats_init();

    ret = probe_hooks_init();
    if (ret < 0) {
        stats_exit();
        return ret;
    }

    ret = procfs_init();
    if (ret < 0) {
        probe_hooks_exit();
        stats_exit();
        return ret;
    }

    pr_info("syscall_latency_profiler: module loaded successfully\n");
    return 0;
}

static void __exit latency_profiler_exit(void)
{
    pr_info("syscall_latency_profiler: unloading module...\n");

    procfs_exit();
    probe_hooks_exit();
    stats_exit();

    pr_info("syscall_latency_profiler: module unloaded successfully\n");
}

module_init(latency_profiler_init);
module_exit(latency_profiler_exit);
