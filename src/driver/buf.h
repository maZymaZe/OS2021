#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <common/string.h>

#define BSIZE 512

#define B_VALID 0x2 /* Buffer has been read from disk. */
#define B_DIRTY 0x4 /* Buffer needs to be written to disk. */

struct buf {
    int flags;
    u32 blockno;
    u8 data[BSIZE];  // 1B*512

    /*
     * Add other necessary elements. It depends on you.
     */
    /* TODO: Lab7 driver. */
    struct buf *Prev, *Next;
};
struct bufQueue {
    struct buf *begin, *end;
    int sz;
};
void initBufQueue(struct bufQueue* x) {
    x->begin = x->end = NULL;
    x->sz = 0;
}
void push(struct bufQueue* x, struct buf* y) {
    if (x->sz == 0) {
        x->begin = x->end = y;
        x->sz = 1;
        return;
    }
    x->end->Next = y;
    y->Prev = x->end;
    x->end = y;
    x->sz++;
}
void pop(struct bufQueue* x) {
    if (x->sz == 0)
        PANIC("EMPTY");
    if (x->sz == 1) {
        x->begin = x->end = NULL;
        x->sz = 0;
        return;
    }
    x->begin = x->begin->Next;
    x->begin->Prev = NULL;
    x->sz--;
}
void size(struct bufQueue* x) {
    return x->sz;
}
struct buf* front(struct bufQueue* x) {
    if (x->sz == 0)
        PANIC("EMPTY");
    return x->begin;
}
struct buf* back(struct bufQueue* x) {
    if (x->sz == 0)
        PANIC("EMPTY");
    return x->end;
}
int empty(struct bufQueue* x) {
    return x->sz == 0;
}

/*
 * Add some useful functions to use your buffer list, such as push, pop and so
 * on.
 */

/* TODO: Lab7 driver. */