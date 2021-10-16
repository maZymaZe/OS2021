
#include <aarch64/mmu.h>
#include <common/string.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/virtual_memory.h>

void forkret();
extern void trap_return();

struct PROCESS_TABLE {
    struct proc procs[NPROC];
} pt;
int PID_GEN = 1;

/*
 * Look through the process table for an UNUSED proc.
 * If found, change state to EMBRYO and initialize
 * state (allocate stack, clear trapframe, set context for switch...)
 * required to run in the kernel. Otherwise return 0.
 * Step 1 (TODO): Call `alloc_pcb()` to get a pcb.
 * Step 2 (TODO): Set the state to `EMBRYO`.
 * Step 3 (TODO): Allocate memory for the kernel stack of the process.
 * Step 4 (TODO): Reserve regions for trapframe and context in the kernel stack.
 * Step 5 (TODO): Set p->tf and p->context to the start of these regions.
 * Step 6 (TODO): Clear trapframe.
 * Step 7 (TODO): Set the context to work with `swtch()`, `forkret()` and
 * `trap_return()`.
 */
static struct proc* alloc_proc() {
    struct proc* p;
    /* TODO: Lab3 Process */
    p = alloc_pcb();

    p->state = EMBRYO;

    void* stp = kalloc();
    // kalloc cleaned the page
    if (stp == 0) {
        p->state = UNUSED;
        return 0;
    }
    p->kstack = stp;
    p->tf = (Trapframe*)(stp + KSTACKSIZE - sizeof(Trapframe));
    (*(uint64_t*)(stp + KSTACKSIZE - sizeof(Trapframe) - 8)) = trap_return;
    (*(uint64_t*)(stp + KSTACKSIZE - sizeof(Trapframe) - 8 - 8)) =
        stp + KSTACKSIZE;
    p->context =
        (stp + KSTACKSIZE - sizeof(Trapframe) - 8 - 8 - sizeof(struct context));
    p->context->r30 = (uint64_t)forkret + 8;
    return p;
}

/*
 * Set up first user process(Only used once).
 * Set trapframe for the new process to run
 * from the beginning of the user process determined
 * by uvm_init
 * Step 1: Allocate a configured proc struct by `alloc_proc()`.
 * Step 2 (TODO): Allocate memory for storing the code of init process.
 * Step 3 (TODO): Copy the code (ranging icode to eicode) to memory.
 * Step 4 (TODO): Map any va to this page.
 * Step 5 (TODO): Set the address after eret to this va.
 * Step 6 (TODO): Set proc->sz.
 */
void spawn_init_process() {
    struct proc* p;
    extern char icode[], eicode[];
    p = alloc_proc();

    /* TODO: Lab3 Process */
    if (p == 0) {
        PANIC("failed alloc proc");
    }
    void* newpgdir = pgdir_init();
    if (newpgdir == 0) {
        PANIC("failed to alloc pgdir");
    }
    void* newpage = kalloc();
    uvm_map(newpgdir, 0, PGSIZE, K2P(newpage));
    for (int i = 0; icode + i != eicode; ++i) {
        *((char*)(newpage + i)) = (icode + i);
    }

    p->tf->elr_el1 = 0;
    p->tf->spsr_el1 = 0;
    p->tf->sp_el0 = PGSIZE;
    p->tf->x30 = 0;

    p->state = RUNNABLE;
    p->sz = PGSIZE;
}

/*
 * A fork child will first swtch here, and then "return" to user space.
 */
void forkret() {
    /* TODO: Lab3 Process */
    return;
}

/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls wait() to find out it exited.
 */
NO_RETURN void exit() {
    struct proc* p = thiscpu()->proc;
    /* TODO: Lab3 Process */
    p->state = ZOMBIE;
}