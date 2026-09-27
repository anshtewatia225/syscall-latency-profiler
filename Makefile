obj-m += syscall_latency_profiler.o
syscall_latency_profiler-objs := latency_profiler.o probe_hooks.o stats.o procfs_interface.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
