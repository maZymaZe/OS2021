#pragma once

#ifndef _CORE_TRAPFRAME_H_
#define _CORE_TRAPFRAME_H_

#include <common/defines.h>

/*
 * Trapframe should contain register x0~x30,
 * elr_el1, spsr_el1 and  sp_el0.
 * Pay attention to the order of these registers
 * in your trapframe.
 */

typedef struct {
    /* TO-DO: Lab3 Interrupt */
    u64 tpidr_el0;
    u64 elr_el1;   // pc
    u64 spsr_el1;  // spsr
    u64 sp_el0;    // sp
    u64 x0;
    u64 x1;
    u64 x2;
    u64 x3;
    u64 x4;
    u64 x5;
    u64 x6;
    u64 x7;
    u64 x8;
    u64 x9;
    u64 x10;
    u64 x11;
    u64 x12;
    u64 x13;
    u64 x14;
    u64 x15;
    u64 x16;
    u64 x17;
    u64 x18;
    u64 x19;
    u64 x20;
    u64 x21;
    u64 x22;
    u64 x23;
    u64 x24;
    u64 x25;
    u64 x26;
    u64 x27;
    u64 x28;
    u64 x29;
    u64 x30;

} Trapframe;

#endif
