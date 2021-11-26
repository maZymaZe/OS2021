#include <core/console.h>
#include <core/proc.h>
#include <core/syscall.h>

/*
 * Based on the syscall number, call the corresponding syscall handler.
 * The syscall number and parameters are all stored in the trapframe.
 * See `syscallno.h` for syscall number macros.
 */
u64 syscall_dispatch(Trapframe* frame) {
    /* TO-DO: Lab3 Syscall */

    switch (frame->x8) {
        case SYS_myexit:
            sys_myexit();
            break;
        case SYS_myexecve:
            sys_myexecve((char*)frame->x0);
            break;
        case SYS_myprint:
            sys_myprint(frame->x0);
            break;
        default:
            PANIC("unknown syscallno");
    }
    return 0;
}
