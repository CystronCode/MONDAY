// monday/security/seccomp_filter.h
#pragma once

#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// Installs a strict seccomp-bpf syscall allowlist for sandboxed subprocesses.
// Allowed syscalls: read, write, close, mmap, munmap, brk, fstat, lseek, exit_group, exit, rt_sigreturn.
// Any unallowed syscall triggers kernel termination of the process (SIGSYS/SIGKILL).
monday_error_t monday_install_seccomp_filter(void);

#ifdef __cplusplus
}
#endif
