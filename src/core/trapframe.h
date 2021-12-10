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
#define uint64_t u64
typedef struct {
    /* TO-DO: Lab3 Interrupt */
    uint64_t elr_el1;   // pc
    uint64_t spsr_el1;  // spsr
    uint64_t sp_el0;    // sp
    uint64_t x0;
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    uint64_t x6;
    uint64_t x7;
    uint64_t x8;
    uint64_t x9;
    uint64_t x10;
    uint64_t x11;
    uint64_t x12;
    uint64_t x13;
    uint64_t x14;
    uint64_t x15;
    uint64_t x16;
    uint64_t x17;
    uint64_t x18;
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;
    uint64_t x30;

} Trapframe;

#endif
