# Syscall Latency Profiler

A lightweight Linux Kernel Module (LKM) designed to profile and measure system call execution latency with nanosecond precision using `kretprobes` and export live statistics via `procfs`.

---

## Overview

The **Syscall Latency Profiler** dynamically attaches return probes (`kretprobe`) to monitored x86_64 system calls. For every syscall invocation across the system, it records:
- Entry timestamp and exit timestamp using `ktime_get()`
- Elapsed execution latency in nanoseconds ($t_{\text{exit}} - t_{\text{entry}}$)
- Calling process details (PID and command name `comm`)

Collected metrics are aggregated concurrently in kernel hash tables and exposed to userspace through virtual files in `/proc/syscall_latency/`.

---

## Features

- **High Precision Timing**: Calculates latency in nanoseconds using kernel monotonic clocks (`ktime_get()`).
- **Comprehensive Syscall Coverage**: Hooks into over 100 common Linux x86_64 system calls (I/O, process control, memory management, networking, signals, epoll, and io_uring).
- **Dual Aggregation Levels**:
  - **Global**: Aggregate metrics across the entire system.
  - **Per-PID**: Breakdown of system calls tracked separately for each active process.
- **Metrics Tracked**:
  - `COUNT`: Total invocation count.
  - `AVG_LAT(ns)`: Cumulative average latency in nanoseconds.
  - `MAX_LAT(ns)`: Peak latency recorded for the call.
- **Sorted Userspace Output**: Automatically sorts syscall entries in descending order of average latency for quick identification of bottlenecks.
- **Concurrency & Safe Synchronization**: Protected via bucket hash tables and irq-safe spinlocks (`spin_lock_irqsave`).
- **Clean Lifecycle**: Graceful probe deregistration and memory deallocation on module unloading.

---

## Architecture

The project consists of four modular components:

```
[ Userspace ] <--- cat /proc/syscall_latency/{global,pids}
       |
+-------------------------------------------------------+
| Linux Kernel Space                                    |
|                                                       |
|  procfs_interface.c   <-- Formats & sorts snapshots   |
|         ^                                             |
|         |                                             |
|      stats.c          <-- Global & PID Hash Tables    |
|         ^                 (spinlocks, atomics)        |
|         |                                             |
|   probe_hooks.c       <-- kretprobes (__x64_sys_*)    |
|                           entry_handler / ret_handler |
|         ^                                             |
|         |                                             |
| latency_profiler.c    <-- Module init & exit hooks    |
+-------------------------------------------------------+
```

- **`latency_profiler.c`**: Entry point (`module_init`) and exit point (`module_exit`) orchestrating subsystem initialization.
- **`probe_hooks.c`**: Instantiates and registers `struct kretprobe` instances for monitored syscall symbols (`__x64_sys_*`). Captures entry timestamps in `kretprobe_instance->data` and calculates delta at return.
- **`stats.c` / `stats.h`**: Manages kernel hash tables (`global_syscall_table` and `pid_syscall_table`) storing counts, total durations, and maximum latencies. Provides snapshot generation for reads.
- **`procfs_interface.c`**: Implements `seq_file` operations for `/proc/syscall_latency/global` and `/proc/syscall_latency/pids`.

---

## Prerequisites

- **Architecture**: x86_64
- **Kernel Version**: Linux 5.x / 6.x
- **Kernel Configuration**:
  - `CONFIG_KPROBES=y`
  - `CONFIG_KRETPROBES=y`
  - `CONFIG_PROC_FS=y`
- **Dependencies**:
  - GCC / Clang
  - GNU Make
  - Linux kernel headers matching running kernel (`linux-headers-$(uname -r)` on Debian/Ubuntu or `kernel-devel-$(uname -r)` on Fedora/RHEL)

---

## Building

To build the kernel module:

```bash
make
```

This compiles `latency_profiler.c`, `probe_hooks.c`, `stats.c`, and `procfs_interface.c` into the loadable module:
```text
syscall_latency_profiler.ko
```

To clean build artifacts:

```bash
make clean
```

---

## Usage

### 1. Load the Module

Insert the module into the running kernel using `insmod`:

```bash
sudo insmod syscall_latency_profiler.ko
```

Verify that the module loaded successfully:

```bash
sudo dmesg | tail -n 10
```

You should see log output similar to:
```text
syscall_latency_profiler: initializing module...
syscall_latency_profiler: successfully registered 105/105 kretprobes
syscall_latency_profiler: module loaded successfully
```

### 2. Inspect System-Wide Latency

Read from `/proc/syscall_latency/global` to view all tracked syscalls sorted from highest to lowest average latency:

```bash
cat /proc/syscall_latency/global
```

**Example Output:**
```text
SYSCALL              | COUNT      | AVG_LAT(ns)     | MAX_LAT(ns)    
--------------------------------------------------------------------
nanosleep            | 42         | 50012400        | 50120000       
poll                 | 310        | 4512300         | 15200340       
epoll_wait           | 1205       | 1204500         | 8940000        
read                 | 8450       | 12450           | 342000         
write                | 6320       | 8910            | 120400         
openat               | 980        | 4200            | 45100          
close                | 1020       | 1120            | 15400          
```

### 3. Inspect Per-Process Latency

Read from `/proc/syscall_latency/pids` to view breakdown by PID and command:

```bash
cat /proc/syscall_latency/pids
```

**Example Output:**
```text
PID: 1234 (node)
SYSCALL              | COUNT      | AVG_LAT(ns)     | MAX_LAT(ns)    
--------------------------------------------------------------------
epoll_wait           | 450        | 2100340         | 9500000        
read                 | 1200       | 14200           | 85000          
write                | 890        | 9200            | 43000          

PID: 5678 (redis-server)
SYSCALL              | COUNT      | AVG_LAT(ns)     | MAX_LAT(ns)    
--------------------------------------------------------------------
epoll_wait           | 3200       | 850000          | 4500000        
read                 | 9400       | 3100            | 28000          
write                | 9400       | 3400            | 31000          
```

### 4. Real-time Monitoring

To monitor syscall latencies live in your terminal:

```bash
watch -n 1 "cat /proc/syscall_latency/global | head -n 25"
```

To filter for a specific process or syscall:

```bash
# Filter for a specific syscall
cat /proc/syscall_latency/global | grep -E "SYSCALL|read"

# Filter for a specific PID from the per-PID list
grep -A 10 "PID: 1234" /proc/syscall_latency/pids
```

### 5. Unload the Module

Remove the module from the kernel when profiling is finished:

```bash
sudo rmmod syscall_latency_profiler
```

Confirm safe removal in dmesg:

```bash
sudo dmesg | tail -n 5
```
```text
syscall_latency_profiler: unloading module...
syscall_latency_profiler: unregistered all kretprobes
syscall_latency_profiler: module unloaded successfully
```

---

## Configuration & Tuning

- **Monitored Syscalls**: You can add or modify syscall names in `monitored_syscalls[]` within [`probe_hooks.c`].
- **Concurrent Probe Instances**: `skrp->rp.maxactive` is set to `64` by default in [`probe_hooks.c`]. For extremely high concurrency workloads, this value can be adjusted.
- **Hash Table Size**: Hash table bit depth (`SYSCALL_HASH_BITS` and `PID_HASH_BITS`) can be configured in [`stats.h`].
