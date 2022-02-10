
#include <aarch64/mmu.h>
#include <common/string.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/virtual_memory.h>
#include <driver/sd.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>

void forkret();
extern void trap_return();
extern void initenter();

static struct proc* initproc;

/*
 * Initialize the spinlock for ptable to serialize the access to ptable
 */
SpinLock waitlock;
void init_proc() {
    // init_spinlock(&waitlock, "waitlock");
}
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
    /* TO-DO: Lab3 Process */
    p = alloc_pcb();
    if (p == 0) {
        return 0;
    }
    p->state = EMBRYO;
    void* stp = kalloc();
    // kalloc cleaned the page
    if (stp == 0) {
        p->state = UNUSED;
        return 0;
    }

    p->kstack = stp + KSTACKSIZE;
    p->tf = (Trapframe*)(stp + KSTACKSIZE - sizeof(Trapframe));

    p->context =
        (stp + KSTACKSIZE - sizeof(Trapframe) - sizeof(struct context));
    p->context->r30 = (u64)initenter;

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

    /* TO-DO: Lab3 Process */
    if (p == 0) {
        PANIC("failed alloc proc");
    }
    void* newpgdir = pgdir_init();
    if (newpgdir == 0) {
        PANIC("failed to alloc pgdir");
    }
    p->pgdir = newpgdir;
    void* newpage = kalloc();
    memcpy(newpage, icode, eicode - icode);
    strncpy(p->name, "initproc", sizeof(p->name));
    uvm_map(newpgdir, 0, PGSIZE, K2P(newpage));

    p->tf->elr_el1 = 0;
    p->tf->spsr_el1 = 0;
    p->tf->sp_el0 = PGSIZE;
    p->tf->x30 = 0;

    p->state = RUNNABLE;
    p->sz = PGSIZE;

    initproc = p;
    OpContext ctx;
    bcache.begin_op(&ctx);
    p->cwd = namei("/", &ctx);
    bcache.end_op(&ctx);
}
void spawn_init_process_sd() {
    struct proc* p;
    extern char ispin[], eicode[];
    p = alloc_proc();

    /* TO-DO: Lab3 Process */
    if (p == 0) {
        PANIC("failed alloc proc");
    }
    void* newpgdir = pgdir_init();
    if (newpgdir == 0) {
        PANIC("failed to alloc pgdir");
    }
    p->pgdir = newpgdir;
    void* newpage = kalloc();
    memcpy(newpage, ispin, eicode - ispin);
    strncpy(p->name, "sdproc", sizeof(p->name));
    uvm_map(newpgdir, 0, PGSIZE, K2P(newpage));

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
int procnum = 0;
void forkret() {
    /* TO-DO: Lab3 Process */

    /* TO-DO: Lab3 Process */
    procnum++;
    int x = procnum;
    release_sched_lock();
    printf("pn:%d %s\n", procnum, thiscpu()->proc->name);
    if (x == 2) {
        // sd_test();
        //  sd_test();
        init_filesystem();
        printf("spawn init\n");
        spawn_init_process();

        //  sd_test();
    }
    // static int first = 1;
    // if (first) {
    //     // File system initialization must be run in the context of a
    //     // regular process (e.g., because it calls sleep), and thus cannot
    //     // be run from main().
    //     first = 0;
    //     printf("init fs\n");
    //     init_filesystem();
    // }

    return;
}

void wakeup1(void* chan) {
    proc *p, *cp = thiscpu()->proc;
    for (int i = 0; i < NPROC; i++) {
        p = thiscpu()->scheduler->ptable.proc + i;
        if (p != cp && p->state == SLEEPING && p->chan == chan) {
            p->state = RUNNABLE;
        }
    }
}
/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls wait() to find out it exited.
 *
 * In Lab9, you should add the following:
 * (1) close open files
 * (2) release inode `pwd`
 * (3) wake up its parent
 * (4) pass its children to `init` process
 *
 * Why not set the state to UNUSED in this function?
 */
NO_RETURN void exit() {
    proc* p = thiscpu()->proc;
    /* TODO: Lab9 Shell */

    if (p == initproc) {
        PANIC("exit INITPROC");
    }
    for (int fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd]) {
            fileclose(p->ofile[fd]);
            p->ofile[fd] = 0;
        }
    }
    OpContext ctx;
    bcache.begin_op(&ctx);
    inodes.put(&ctx, p->cwd);
    bcache.end_op(&ctx);
    p->cwd = 0;
    acquire_sched_lock();
    wakeup1(p->parent);
    for (int i = 0; i < NPROC; i++) {
        proc* q = thiscpu()->scheduler->ptable.proc + i;
        if (q->parent == p) {
            p->parent = initproc;
            if (p->state == ZOMBIE) {
                wakeup1(p->parent);
            }
        }
    }
    p->state = ZOMBIE;
    sched();
}

/*
 * Give up CPU.
 * Switch to the scheduler of this proc.
 */
void yield() {
    /* TODO: lab6 container */
    acquire_sched_lock();
    struct proc* p = thiscpu()->proc;
    p->state = RUNNABLE;
    sched();
    release_sched_lock();
}

/*
 * Atomically release lock and sleep on chan.
 * Reacquires lock when awakened.
 */
void sleep(void* chan, SpinLock* lock) {
    /* TODO: lab6 container */
    SpinLock* lk = &(thiscpu()->scheduler->ptable.lock);
    if (lock != lk) {
        acquire_sched_lock();
        release_spinlock(lock);
    }

    struct proc* p = thiscpu()->proc;
    p->chan = chan;
    p->state = SLEEPING;
    sched();
    p->chan = 0;
    if (lock != lk) {
        release_sched_lock();
        acquire_spinlock(lock);
    }
}

/* Wake up all processes sleeping on chan. */
void wakeup(void* chan) {
    /* TODO: lab6 container */
    acquire_sched_lock();
    struct proc* cp = thiscpu()->proc;
    struct proc* p;
    for (int i = 0; i < NPROC; i++) {
        p = &thiscpu()->scheduler->ptable.proc[i];
        if (p != cp && p->state == SLEEPING && p->chan == chan) {
            p->state = RUNNABLE;
        }
    }
    release_sched_lock();
}

/*
 * Add process at thiscpu()->container,
 * execute code in src/user/loop.S
 */
void add_loop_test(int times) {
    for (int i = 0; i < times; i++) {
        /* TODO: lab6 container */
        struct proc* p;
        extern char loop_start[], loop_end[];
        p = alloc_proc();
        if (p == 0) {
            PANIC("failed alloc proc");
        }
        void* newpgdir = pgdir_init();
        if (newpgdir == 0) {
            PANIC("failed to alloc pgdir");
        }
        p->pgdir = newpgdir;
        void* newpage = kalloc();
        memcpy(newpage, loop_start, loop_end - loop_start);
        strncpy(p->name, "initproc", sizeof(p->name));
        uvm_map(newpgdir, 0, PGSIZE, K2P(newpage));

        p->tf->elr_el1 = 0;
        p->tf->spsr_el1 = 0;
        p->tf->sp_el0 = PGSIZE;
        p->tf->x30 = 0;

        p->state = RUNNABLE;
        p->sz = PGSIZE;
    }
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
/*
 * Call allocuvm or deallocuvm.
 * This function is used in `sys_brk`.
 */
int growproc(int n) {
    /* TODO: lab9 shell */
    // printf("enter growproc\n");
    proc* p = thiscpu()->proc;
    usize sz = p->sz;
    if (n > 0) {
        // FIXME
        sz = uvm_alloc(p->pgdir, p->base, p->stksz, sz, sz + n);
        if (sz == 0)
            return -1;
    } else if (n < 0) {
        sz = uvm_dealloc(p->pgdir, p->base, sz, sz + n);
    }
    p->sz = sz;
    return 0;
}

/*
 * Create a new process copying p as the parent.
 * Sets up stack to return as if from system call.
 * Caller must set state of returned proc to RUNNABLE.
 *
 * Don't forget to copy file descriptors and `cwd` inode.
 */
int fork() {
    /* TODO: Lab9 shell */
    // printf("enter fork\n");
    proc *np, *p = thiscpu()->proc;
    np = alloc_proc();
    if (np == 0)
        return -1;
    np->pgdir = uvm_copy(p->pgdir);
    if (np->pgdir == 0) {
        kfree((void*)(np->kstack) - KSTACKSIZE);
        np->kstack = 0;
        np->state = UNUSED;
        return -1;
    }
    np->sz = p->sz;
    *(np->tf) = *(p->tf);
    np->tf->x0 = 0;
    np->parent = p;

    for (int i = 0; i < NOFILE; i++) {
        if (p->ofile[i]) {
            np->ofile[i] = filedup(p->ofile[i]);
        }
    }
    strncpy(np->name, p->name, 16);
    np->cwd = inodes.share(p->cwd);
    np->state = RUNNABLE;

    return np->pid;
}

/*
 * Wait for a child process to exit and return its pid.
 * Return -1 if this process has no children.
 *
 * You can release the PCB (set state to UNUSED) of its dead children.
 */
int wait() {
    /* TODO: Lab9 shell. */
    // printf("enter wait");
    proc *p, *tp;
    SpinLock* lk = &(thiscpu()->scheduler->ptable.lock);
    tp = thiscpu()->proc;
    p = thiscpu()->scheduler->ptable.proc;
    int kids, pid;
    acquire_sched_lock();
    while (true) {
        kids = 0;
        for (int i = 0; i < NPROC; i++) {
            proc* q = p + i;
            if (q->parent != tp)
                continue;
            kids = 1;
            if (q->state == ZOMBIE) {
                int pid = q->pid;
                q->killed = 0;
                q->state = UNUSED;
                q->pid = 0;
                q->parent = 0;
                vm_free(q->pgdir);
                kfree((void*)(q->kstack) - KSTACKSIZE);
                release_sched_lock();
                return pid;
            }
        }
        if (kids == 0 || tp->killed) {
            release_sched_lock();
            return -1;
        }
        // FIXME
        sleep(tp, lk);
    }
    PANIC("???");
}
