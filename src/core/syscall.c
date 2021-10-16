#include <core/console.h>
#include <core/syscall.h>

/*
 * Based on the syscall number, call the corresponding syscall handler.
 * The syscall number and parameters are all stored in the trapframe.
 * See `syscallno.h` for syscall number macros.
 */
u64 syscall_dispatch(Trapframe* frame) {
    /* TODO: Lab3 Syscall */
    struct proc* nowproc = thiscpu()->proc;
    switch (nowproc->tf->x0) {
        case SYS_myexit:
            return sys_myexit();
        case SYS_myexecve:
            return sys_myexecve("");
        default:
            PANIC("unknown syscallno");
    }
    return -1;
}
