/* `exception_vector.S` send all traps here. */
.global trap_entry
trap_entry:
    /* use `stp`/`ldp` in favor of `str`/`ldr` to maintain stack alignment. */
	/*
     * Build your trap frame.
     * Hint:
     * 1. Use stp/ldp in favor of str/ldr to maintain stack alignment
          For example, `stp x1, x2, [sp, #-16]!` is equivalent to
          first `push x2` and then `push x1`. Be careful about the order.
     * 2. Use mrs/msr to move from/to system registers such as
     *    ELR_EL1, SPSR_EL1, and SP_EL0.
     */

    /* TO-DO: Lab3 Interrupt. */

    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!
    stp x13, x14, [sp, #-16]!
    stp x11, x12, [sp, #-16]!
    stp x9, x10, [sp, #-16]!
    stp x7, x8, [sp, #-16]!
    stp x5, x6, [sp, #-16]!
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!
    str x0, [sp,#-8]!
    mrs x1, tpidr_el0
    mrs x2, elr_el1
    mrs x3, spsr_el1
    mrs x4, sp_el0
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!



    /*
     * Call trap_global_handler(Trapframe *frame).
     * Hint: The first argument is a stack pointer.
     */

    /* TO-DO: Lab3 Interrupt */
    add x0, sp, #0
    bl trap_global_handler
    b trap_return

.global initenter
initenter:
    bl forkret

/* return falls through to `trap_return`. */
.global trap_return
trap_return:
	/*
     * Restore registers.
     * Hint: `ldp x1, x2, [sp], #16` is equivalent to first `pop x1`
     * and then `pop x2`.
     */
    /* TO-DO: Lab3 Interrupt */
    ldp x1, x2, [sp], #16
    ldp x3, x4, [sp], #16
    msr sp_el0, x4
    msr spsr_el1, x3
    msr elr_el1, x2
    msr tpidr_el0, x1
    ldr x0, [sp], #8
    ldp	x1, x2, [sp], #16
    ldp	x3, x4, [sp], #16
    ldp	x5, x6, [sp], #16
    ldp	x7, x8, [sp], #16
    ldp	x9, x10, [sp], #16
    ldp	x11, x12, [sp], #16
    ldp	x13, x14, [sp], #16
    ldp	x15, x16, [sp], #16
    ldp	x17, x18, [sp], #16
    ldp	x19, x20, [sp], #16
    ldp	x21, x22, [sp], #16
    ldp	x23, x24, [sp], #16
    ldp	x25, x26, [sp], #16
    ldp	x27, x28, [sp], #16
    ldp x29, x30, [sp], #16
    eret



