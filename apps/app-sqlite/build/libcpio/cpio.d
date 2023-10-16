cmd_/root/.unikraft/apps/app-sqlite/build/libcpio/cpio.o := ~/llvm-project-releases/bin/clang -nostdlib -U __linux__ -U __FreeBSD__ -U __sun__ -fno-omit-frame-pointer -Werror=attributes -Wno-unused-variable -fno-stack-protector -Wall -Wextra -D __Unikraft__ -DUK_CODENAME="Tethys" -DUK_VERSION=0.5 -DUK_FULLVERSION=0.5.0~9edaa33-custom -O2 -fno-builtin -fno-PIC   -I/root/.unikraft/apps/app-sqlite/build/include -I/root/.unikraft/unikraft/arch/arm/arm64/include -I/root/.unikraft/unikraft/include -I/root/.unikraft/unikraft/lib/flexos-core/include/ -I/root/.unikraft/unikraft/lib/ukboot/include -I/root/.unikraft/unikraft/lib/posix-user/include -I/root/.unikraft/unikraft/lib/posix-user/musl-imported/include -I/root/.unikraft/unikraft/lib/posix-sysinfo/include -I/root/.unikraft/unikraft/lib/ukdebug/include -I/root/.unikraft/unikraft/lib/ukargparse/include -I/root/.unikraft/unikraft/lib/ukalloc/include -I/root/.unikraft/unikraft/lib/ukallocbbuddy/include -I/root/.unikraft/unikraft/lib/uksched/include -I/root/.unikraft/unikraft/lib/ukschedcoop/include -I/root/.unikraft/unikraft/lib/syscall_shim/include -I/root/.unikraft/unikraft/lib/vfscore/include -I/root/.unikraft/unikraft/lib/cpio/include -I/root/.unikraft/unikraft/lib/devfs/include -I/root/.unikraft/unikraft/lib/uklock/include -I/root/.unikraft/unikraft/lib/uklibparam/include -I/root/.unikraft/unikraft/lib/uktime/include -I/root/.unikraft/unikraft/lib/uktime/musl-imported/include -I/root/.unikraft/unikraft/lib/posix-process/include -I/root/.unikraft/unikraft/lib/posix-process/musl-imported/include -I/root/.unikraft/unikraft/lib/posix-process/musl-imported/arch/generic -I/root/.unikraft/unikraft/lib/uksp/include -I/root/.unikraft/unikraft/lib/uksignal/include -I/root/.unikraft/libs/tlsf -I/root/.unikraft/libs/tlsf/include -I/root/.unikraft/apps/app-sqlite/build/libtlsf/origin/TLSF-2.4.6/src -I/root/.unikraft/libs/pthread-embedded/include -I/root/.unikraft/apps/app-sqlite/build/libpthread-embedded/origin/pthread-embedded-44b41d760a433915d70a7be9809651b0a65e001d -I/root/.unikraft/apps/app-sqlite/build/libpthread-embedded/origin/pthread-embedded-44b41d760a433915d70a7be9809651b0a65e001d/platform/helper -I/root/.unikraft/libs/newlib/include -I/root/.unikraft/libs/newlib/musl-imported/include -I/root/.unikraft/libs/newlib/musl-imported/arch/generic -I/root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include     -D__ARM_64__  -DCC_VERSION=13.0   -target aarch64-none-elf -g0 -march=morello -mabi=aapcs -DTLSF_USE_LOCKS=0 -DUSE_MMAP=0 -DUSE_SBRK=0 -DUSE_PRINTF=0 -DMISSING_SYSCALL_NAMES -DMALLOC_PROVIDED -D_POSIX_REALTIME_SIGNALS -D_LDBL_EQ_DBL -D_HAVE_LONG_DOUBLE -Wno-char-subscripts -D__DYNAMIC_REENT__ -DCONFIG_UK_NETDEV_SCRATCH_SIZE=0       -g0 -D__LIBNAME__=libcpio -D__BASENAME__=cpio.c -c /root/.unikraft/unikraft/lib/cpio/cpio.c -o /root/.unikraft/apps/app-sqlite/build/libcpio/cpio.o -Wp,-MD,/root/.unikraft/apps/app-sqlite/build/libcpio/.cpio.o.d

source_/root/.unikraft/apps/app-sqlite/build/libcpio/cpio.o := /root/.unikraft/unikraft/lib/cpio/cpio.c

deps_/root/.unikraft/apps/app-sqlite/build/libcpio/cpio.o := \
  /root/.unikraft/libs/newlib/include/stdlib.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/stdlib.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/machine/ieeefp.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/_ansi.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/newlib.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/config.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/features.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/_newlib_version.h \
  /root/.unikraft/libs/newlib/include/stddef.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/reent.h \
  /root/.unikraft/libs/newlib/include/__stddef_max_align_t.h \
  /root/.unikraft/libs/newlib/include/sys/_types.h \
  /root/.unikraft/libs/newlib/include/uk/_types.h \
  /root/.unikraft/unikraft/lib/uktime/include/uk/time_types.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/stdint.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/machine/_default_types.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/_intsup.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/_stdint.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/_types.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/machine/_types.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/lock.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/cdefs.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/machine/stdlib.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/alloca.h \
  /root/.unikraft/libs/newlib/include/stdio.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/stdio.h \
  /root/.unikraft/libs/newlib/include/stdarg.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/types.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/machine/endian.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/machine/_endian.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/select.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/_sigset.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/_timeval.h \
  /root/.unikraft/libs/newlib/include/sys/timespec.h \
  /root/.unikraft/libs/newlib/include/uk/_timespec.h \
  /root/.unikraft/libs/pthread-embedded/include/sys/_pthreadtypes.h \
  /root/.unikraft/libs/pthread-embedded/include/pthread.h \
  /root/.unikraft/apps/app-sqlite/build/libpthread-embedded/origin/pthread-embedded-44b41d760a433915d70a7be9809651b0a65e001d/pthread.h \
  /root/.unikraft/libs/pthread-embedded/include/pte_types.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/timeb.h \
  /root/.unikraft/libs/newlib/include/limits.h \
  /root/.unikraft/unikraft/include/uk/arch/limits.h \
  /root/.unikraft/unikraft/include/uk/config.h \
  /root/.unikraft/unikraft/arch/arm/arm64/include/uk/asm/limits.h \
    $(wildcard include/config/stack/size/page/order.h) \
  /root/.unikraft/unikraft/arch/arm/arm64/include/uk/asm/intsizes.h \
  /root/.unikraft/libs/pthread-embedded/include/sched.h \
  /root/.unikraft/apps/app-sqlite/build/libpthread-embedded/origin/pthread-embedded-44b41d760a433915d70a7be9809651b0a65e001d/sched.h \
  /root/.unikraft/unikraft/lib/uktime/musl-imported/include/time.h \
    $(wildcard include/config/libnolibc.h) \
  /root/.unikraft/libs/newlib/include/time.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/xlocale.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/setjmp.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/machine/setjmp.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/machine/types.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/stdio.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/string.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/strings.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/string.h \
  /root/.unikraft/libs/newlib/include/stdbool.h \
  /root/.unikraft/unikraft/lib/cpio/include/uk/cpio.h \
  /root/.unikraft/unikraft/include/uk/plat/memory.h \
    $(wildcard include/config/ukplat/memrname.h) \
  /root/.unikraft/unikraft/include/uk/arch/types.h \
  /root/.unikraft/unikraft/arch/arm/arm64/include/uk/asm/types.h \
  /root/.unikraft/unikraft/lib/ukalloc/include/uk/alloc.h \
    $(wildcard include/config/libukalloc/ifstats.h) \
    $(wildcard include/config/libukalloc/ifmalloc.h) \
    $(wildcard include/config/libflexos/intelpku.h) \
    $(wildcard include/config/libflexos/morello.h) \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/errno.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/errno.h \
  /root/.unikraft/unikraft/lib/ukdebug/include/uk/assert.h \
    $(wildcard include/config/libukdebug/enable/assert.h) \
  /root/.unikraft/unikraft/include/uk/plat/bootstrap.h \
  /root/.unikraft/unikraft/include/uk/essentials.h \
    $(wildcard include/config/libnewlibc.h) \
    $(wildcard include/config/have/sched.h) \
  /root/.unikraft/libs/newlib/include/sys/param.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/param.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/syslimits.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/machine/param.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/inttypes.h \
  /root/.unikraft/unikraft/include/uk/arch/lcpu.h \
  /root/.unikraft/unikraft/arch/arm/arm64/include/uk/asm/lcpu.h \
  /root/.unikraft/unikraft/lib/ukdebug/include/uk/print.h \
    $(wildcard include/config/libukdebug/printd.h) \
    $(wildcard include/config/libukdebug/printk/crit.h) \
    $(wildcard include/config/libukdebug/printk/err.h) \
    $(wildcard include/config/libukdebug/printk/warn.h) \
    $(wildcard include/config/libukdebug/printk/info.h) \
    $(wildcard include/config/libukdebug/printk.h) \
  /root/.unikraft/unikraft/lib/flexos-core/include/flexos/literals.h \
  /root/.unikraft/unikraft/lib/flexos-core/include/flexos/impl/morello.h \
  /root/.unikraft/unikraft/lib/flexos-core/include/flexos/isolation.h \
    $(wildcard include/config/libflexos/vmept.h) \
  /root/.unikraft/unikraft/lib/flexos-core/include/flexos/impl/morello-impl.h \
  /root/.unikraft/unikraft/include/uk/page.h \
  /root/.unikraft/libs/newlib/include/sys/mount.h \
  /root/.unikraft/libs/newlib/include/sys/stat.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/stat.h \
  /root/.unikraft/libs/newlib/include/sys/_timespec.h \
  /root/.unikraft/libs/newlib/include/fcntl.h \
    $(wildcard include/config/arch/x86/64.h) \
    $(wildcard include/config/arch/arm/64.h) \
    $(wildcard include/config/arch/arm/32.h) \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/fcntl.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/fcntl.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/_default_fcntl.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/unistd.h \
  /root/.unikraft/libs/newlib/include/sys/unistd.h \
  /root/.unikraft/apps/app-sqlite/build/libnewlibc/origin/newlib-2.5.0.20170922/newlib/libc/include/sys/unistd.h \
  /root/.unikraft/unikraft/lib/posix-user/include/uk/user.h \

/root/.unikraft/apps/app-sqlite/build/libcpio/cpio.o: $(deps_/root/.unikraft/apps/app-sqlite/build/libcpio/cpio.o)

$(deps_/root/.unikraft/apps/app-sqlite/build/libcpio/cpio.o):
