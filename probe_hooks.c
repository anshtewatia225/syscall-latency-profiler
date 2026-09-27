#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/timekeeping.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "stats.h"

struct kretprobe_data {
    ktime_t entry_time;
};

struct syscall_kretprobe {
    struct kretprobe rp;
    char syscall_name[32];
    int syscall_nr;
};

static const char * const monitored_syscalls[] = {
    "__x64_sys_read",
    "__x64_sys_write",
    "__x64_sys_open",
    "__x64_sys_close",
    "__x64_sys_stat",
    "__x64_sys_fstat",
    "__x64_sys_lstat",
    "__x64_sys_poll",
    "__x64_sys_lseek",
    "__x64_sys_mmap",
    "__x64_sys_mprotect",
    "__x64_sys_munmap",
    "__x64_sys_brk",
    "__x64_sys_rt_sigaction",
    "__x64_sys_rt_sigprocmask",
    "__x64_sys_ioctl",
    "__x64_sys_pread64",
    "__x64_sys_pwrite64",
    "__x64_sys_access",
    "__x64_sys_pipe",
    "__x64_sys_select",
    "__x64_sys_sched_yield",
    "__x64_sys_mremap",
    "__x64_sys_msync",
    "__x64_sys_madvise",
    "__x64_sys_dup",
    "__x64_sys_dup2",
    "__x64_sys_pause",
    "__x64_sys_nanosleep",
    "__x64_sys_getpid",
    "__x64_sys_socket",
    "__x64_sys_connect",
    "__x64_sys_accept",
    "__x64_sys_sendto",
    "__x64_sys_recvfrom",
    "__x64_sys_bind",
    "__x64_sys_listen",
    "__x64_sys_clone",
    "__x64_sys_fork",
    "__x64_sys_vfork",
    "__x64_sys_execve",
    "__x64_sys_exit",
    "__x64_sys_wait4",
    "__x64_sys_kill",
    "__x64_sys_uname",
    "__x64_sys_fcntl",
    "__x64_sys_flock",
    "__x64_sys_fsync",
    "__x64_sys_fdatasync",
    "__x64_sys_truncate",
    "__x64_sys_ftruncate",
    "__x64_sys_getdents",
    "__x64_sys_getcwd",
    "__x64_sys_chdir",
    "__x64_sys_rename",
    "__x64_sys_mkdir",
    "__x64_sys_rmdir",
    "__x64_sys_creat",
    "__x64_sys_unlink",
    "__x64_sys_symlink",
    "__x64_sys_readlink",
    "__x64_sys_chmod",
    "__x64_sys_chown",
    "__x64_sys_umask",
    "__x64_sys_gettimeofday",
    "__x64_sys_getuid",
    "__x64_sys_getgid",
    "__x64_sys_geteuid",
    "__x64_sys_getegid",
    "__x64_sys_getppid",
    "__x64_sys_getpgrp",
    "__x64_sys_setsid",
    "__x64_sys_getgroups",
    "__x64_sys_setgroups",
    "__x64_sys_setuid",
    "__x64_sys_setgid",
    "__x64_sys_getresuid",
    "__x64_sys_setresuid",
    "__x64_sys_getresgid",
    "__x64_sys_setresgid",
    "__x64_sys_getpgid",
    "__x64_sys_setpgid",
    "__x64_sys_getsid",
    "__x64_sys_getrlimit",
    "__x64_sys_setrlimit",
    "__x64_sys_getrusage",
    "__x64_sys_sysinfo",
    "__x64_sys_times",
    "__x64_sys_syslog",
    "__x64_sys_getpriority",
    "__x64_sys_setpriority",
    "__x64_sys_sched_setparam",
    "__x64_sys_sched_getparam",
    "__x64_sys_sched_setscheduler",
    "__x64_sys_sched_getscheduler",
    "__x64_sys_sched_get_priority_max",
    "__x64_sys_sched_get_priority_min",
    "__x64_sys_sched_rr_get_interval",
    "__x64_sys_mlock",
    "__x64_sys_munlock",
    "__x64_sys_mlockall",
    "__x64_sys_munlockall",
    "__x64_sys_vhangup",
    "__x64_sys_pivot_root",
    "__x64_sys_prctl",
    "__x64_sys_arch_prctl",
    "__x64_sys_adjtimex",
    "__x64_sys_chroot",
    "__x64_sys_sync",
    "__x64_sys_acct",
    "__x64_sys_settimeofday",
    "__x64_sys_mount",
    "__x64_sys_umount2",
    "__x64_sys_swapon",
    "__x64_sys_swapoff",
    "__x64_sys_reboot",
    "__x64_sys_sethostname",
    "__x64_sys_setdomainname",
    "__x64_sys_gettid",
    "__x64_sys_readahead",
    "__x64_sys_setxattr",
    "__x64_sys_lsetxattr",
    "__x64_sys_fsetxattr",
    "__x64_sys_getxattr",
    "__x64_sys_lgetxattr",
    "__x64_sys_fgetxattr",
    "__x64_sys_listxattr",
    "__x64_sys_llistxattr",
    "__x64_sys_flistxattr",
    "__x64_sys_removexattr",
    "__x64_sys_lremovexattr",
    "__x64_sys_fremovexattr",
    "__x64_sys_tkill",
    "__x64_sys_time",
    "__x64_sys_futex",
    "__x64_sys_sched_setaffinity",
    "__x64_sys_sched_getaffinity",
    "__x64_sys_io_setup",
    "__x64_sys_io_destroy",
    "__x64_sys_io_getevents",
    "__x64_sys_io_submit",
    "__x64_sys_io_cancel",
    "__x64_sys_getdents64",
    "__x64_sys_openat",
    "__x64_sys_mkdirat",
    "__x64_sys_mknodat",
    "__x64_sys_fchownat",
    "__x64_sys_unlinkat",
    "__x64_sys_renameat",
    "__x64_sys_linkat",
    "__x64_sys_symlinkat",
    "__x64_sys_readlinkat",
    "__x64_sys_fchmodat",
    "__x64_sys_faccessat",
    "__x64_sys_pselect6",
    "__x64_sys_ppoll",
    "__x64_sys_unshare",
    "__x64_sys_set_robust_list",
    "__x64_sys_get_robust_list",
    "__x64_sys_splice",
    "__x64_sys_tee",
    "__x64_sys_sync_file_range",
    "__x64_sys_vmsplice",
    "__x64_sys_move_pages",
    "__x64_sys_utimensat",
    "__x64_sys_epoll_pwait",
    "__x64_sys_timerfd_create",
    "__x64_sys_timerfd_settime",
    "__x64_sys_timerfd_gettime",
    "__x64_sys_fallocate",
    "__x64_sys_timer_create",
    "__x64_sys_timer_settime",
    "__x64_sys_timer_gettime",
    "__x64_sys_timer_getoverrun",
    "__x64_sys_timer_delete",
    "__x64_sys_clock_settime",
    "__x64_sys_clock_gettime",
    "__x64_sys_clock_getres",
    "__x64_sys_clock_nanosleep",
    "__x64_sys_epoll_wait",
    "__x64_sys_epoll_ctl",
    "__x64_sys_tgkill",
    "__x64_sys_utimes",
    "__x64_sys_accept4",
    "__x64_sys_recvmmsg",
    "__x64_sys_sendmmsg",
    "__x64_sys_pipe2",
    "__x64_sys_inotify_init1",
    "__x64_sys_preadv",
    "__x64_sys_pwritev",
    "__x64_sys_rt_tgsigqueueinfo",
    "__x64_sys_perf_event_open",
    "__x64_sys_prlimit64",
    "__x64_sys_name_to_handle_at",
    "__x64_sys_open_by_handle_at",
    "__x64_sys_clock_adjtime",
    "__x64_sys_syncfs",
    "__x64_sys_setns",
    "__x64_sys_getcpu",
    "__x64_sys_process_vm_readv",
    "__x64_sys_process_vm_writev",
    "__x64_sys_kcmp",
    "__x64_sys_finit_module",
    "__x64_sys_sched_setattr",
    "__x64_sys_sched_getattr",
    "__x64_sys_renameat2",
    "__x64_sys_seccomp",
    "__x64_sys_getrandom",
    "__x64_sys_memfd_create",
    "__x64_sys_kexec_file_load",
    "__x64_sys_bpf",
    "__x64_sys_execveat",
    "__x64_sys_userfaultfd",
    "__x64_sys_membarrier",
    "__x64_sys_mlock2",
    "__x64_sys_copy_file_range",
    "__x64_sys_preadv2",
    "__x64_sys_pwritev2",
    "__x64_sys_pkey_mprotect",
    "__x64_sys_pkey_alloc",
    "__x64_sys_pkey_free",
    "__x64_sys_statx",
    "__x64_sys_io_pgetevents",
    "__x64_sys_rseq",
    "__x64_sys_pidfd_send_signal",
    "__x64_sys_io_uring_setup",
    "__x64_sys_io_uring_enter",
    "__x64_sys_io_uring_register",
    "__x64_sys_openat2",
    "__x64_sys_pidfd_getfd",
    "__x64_sys_faccessat2",
    "__x64_sys_process_madvise"
};

#define NUM_PROBES (sizeof(monitored_syscalls) / sizeof(monitored_syscalls[0]))

static struct syscall_kretprobe *probes_array;
static int registered_count = 0;

static int entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct kretprobe_data *data = (struct kretprobe_data *)ri->data;
    data->entry_time = ktime_get();
    return 0;
}

static int ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct kretprobe_data *data = (struct kretprobe_data *)ri->data;
    ktime_t now = ktime_get();
    u64 latency = ktime_to_ns(ktime_sub(now, data->entry_time));
    
    struct syscall_kretprobe *skrp = container_of(get_kretprobe(ri), struct syscall_kretprobe, rp);
    
    pid_t pid = current->pid;
    char comm[TASK_COMM_LEN];
    get_task_comm(comm, current);

    stats_record(skrp->syscall_nr, skrp->syscall_name, pid, comm, latency);
    return 0;
}

int probe_hooks_init(void)
{
    int i, ret;

    probes_array = kcalloc(NUM_PROBES, sizeof(struct syscall_kretprobe), GFP_KERNEL);
    if (!probes_array)
        return -ENOMEM;

    for (i = 0; i < NUM_PROBES; i++) {
        struct syscall_kretprobe *skrp = &probes_array[i];
        
        // Extract clean syscall name (skip "__x64_sys_")
        const char *full_name = monitored_syscalls[i];
        if (strncmp(full_name, "__x64_sys_", 10) == 0) {
            strscpy(skrp->syscall_name, full_name + 10, sizeof(skrp->syscall_name));
        } else {
            strscpy(skrp->syscall_name, full_name, sizeof(skrp->syscall_name));
        }
        skrp->syscall_nr = i; // or approximate id

        skrp->rp.kp.symbol_name = (char *)full_name;
        skrp->rp.entry_handler = entry_handler;
        skrp->rp.handler = ret_handler;
        skrp->rp.data_size = sizeof(struct kretprobe_data);
        skrp->rp.maxactive = 64;

        ret = register_kretprobe(&skrp->rp);
        if (ret < 0) {
            // Some symbols might not exist on all kernels; log and continue
            pr_debug("syscall_latency_profiler: failed to register kretprobe for %s: %d\n", full_name, ret);
            continue;
        }
        registered_count++;
    }

    pr_info("syscall_latency_profiler: successfully registered %d/%lu kretprobes\n", registered_count, NUM_PROBES);
    return 0;
}

void probe_hooks_exit(void)
{
    int i;
    for (i = 0; i < NUM_PROBES; i++) {
        struct syscall_kretprobe *skrp = &probes_array[i];
        if (skrp->rp.kp.addr) {
            unregister_kretprobe(&skrp->rp);
        }
    }
    kfree(probes_array);
    pr_info("syscall_latency_profiler: unregistered all kretprobes\n");
}
