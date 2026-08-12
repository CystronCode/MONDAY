// monday/security/seccomp_filter.c
#include "monday/security/seccomp_filter.h"
#include "monday/core/platform.h"
#include <stddef.h>

#if defined(__linux__)
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <errno.h>
#include <unistd.h>

#ifndef SECCOMP_SET_MODE_FILTER
#define SECCOMP_SET_MODE_FILTER 1
#endif

monday_error_t monday_install_seccomp_filter(void) {
    // Prevent gaining new privileges via setuid binaries
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        return MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "Failed PR_SET_NO_NEW_PRIVS");
    }

    struct sock_filter filter[] = {
        // Validate architecture
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, arch))),
#if defined(__x86_64__)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
#elif defined(__i386__)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_I386, 1, 0),
#elif defined(__aarch64__)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_AARCH64, 1, 0),
#else
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 1, 0),
#endif
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

        // Load syscall number
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),

        // Allowed syscalls
#if defined(__NR_read)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_read, 10, 0),
#endif
#if defined(__NR_write)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_write, 9, 0),
#endif
#if defined(__NR_close)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_close, 8, 0),
#endif
#if defined(__NR_fstat)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fstat, 7, 0),
#endif
#if defined(__NR_lseek)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_lseek, 6, 0),
#endif
#if defined(__NR_mmap)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mmap, 5, 0),
#endif
#if defined(__NR_munmap)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_munmap, 4, 0),
#endif
#if defined(__NR_brk)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_brk, 3, 0),
#endif
#if defined(__NR_exit_group)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_exit_group, 2, 0),
#endif
#if defined(__NR_exit)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_exit, 1, 0),
#endif
#if defined(__NR_rt_sigreturn)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_rt_sigreturn, 0, 1),
#endif

        // Default action: Kill process
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        // Allow action
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };

    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof(filter) / sizeof(filter[0])),
        .filter = filter,
    };

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        return MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "Failed to apply seccomp filter");
    }

    return MONDAY_ERROR(MONDAY_OK, "Seccomp filter installed successfully");
}

#else

monday_error_t monday_install_seccomp_filter(void) {
    // Non-Linux platforms stub
    return MONDAY_ERROR(MONDAY_OK, "Seccomp not supported on non-Linux OS");
}

#endif
