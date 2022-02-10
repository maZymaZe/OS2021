/* File descriptors */

#include "file.h"
#include <common/defines.h>
#include <common/spinlock.h>
#include <core/console.h>
#include <core/sleeplock.h>
#include <fs/inode.h>
#include "fs.h"

// struct devsw devsw[NDEV];
struct {
    struct SpinLock lock;
    struct file file[NFILE];
} ftable;
/* Optional since BSS is zero-initialized. */
void fileinit() {
    init_spinlock(&ftable.lock, "file table");
}

/* Allocate a file structure. */
struct file* filealloc() {
    /* TODO: Lab9 Shell */
    // printf("enter filealloc\n");
    struct file* f;
    acquire_spinlock(&ftable.lock);
    for (int i = 0; i < NFILE; i++) {
        f = &ftable.file[i];
        if (f->ref == 0) {
            f->ref = 1;
            release_spinlock(&ftable.lock);
            return f;
        }
    }
    release_spinlock(&ftable.lock);
    return 0;
}

/* Increment ref count for file f. */
struct file* filedup(struct file* f) {
    /* TODO: Lab9 Shell */
    // printf("enter fileup\n");
    acquire_spinlock(&ftable.lock);
    if (f->ref < 1)
        PANIC("up a ref0 file");
    f->ref++;
    release_spinlock(&ftable.lock);
    return f;
}

/* Close file f. (Decrement ref count, close when reaches 0.) */
void fileclose(struct file* f) {
    /* TODO: Lab9 Shell */
    // printf("enter fileclose\n");
    acquire_spinlock(&ftable.lock);
    if (f->ref < 1)
        PANIC("close a ref0 file");
    f->ref--;

    if (f->ref > 0) {
        release_spinlock(&ftable.lock);
        return;
    }

    struct file ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    release_spinlock(&ftable.lock);
    if (ff.type == FD_PIPE) {
        // pipeclose
    } else if (ff.type == FD_INODE) {
        OpContext ctx;
        bcache.begin_op(&ctx);
        inodes.put(&ctx, ff.ip);
        bcache.end_op(&ctx);
    }
}

/* Get metadata about file f. */
int filestat(struct file* f, struct stat* st) {
    /* TODO: Lab9 Shell */
    // printf("enter filestat\n");
    if (f->type == FD_INODE) {
        inodes.lock(f->ip);
        stati(f->ip, st);
        inodes.unlock(f->ip);
        return 0;
    }
    return -1;
}

/* Read from file f. */
isize fileread(struct file* f, char* addr, isize n) {
    /* TODO: Lab9 Shell */
    // printf("enter fileread\n");
    if (!f->readable)
        return -1;
    if (f->type == FD_INODE) {
        inodes.lock(f->ip);
        usize sz = inodes.read(f->ip, addr, f->off, n);
        if (sz > 0)
            f->off += sz;
        inodes.unlock(f->ip);
        return sz;
    }
    PANIC("not inode");
}

/* Write to file f. */
isize filewrite(struct file* f, char* addr, isize n) {
    /* TODO: Lab9 Shell */
    // printf("enter filewrite\n");
    if (!f->writable)
        return -1;
    if (f->type == FD_INODE) {
        usize mx = (OP_MAX_NUM_BLOCKS - 1 - 1 - 2) / 2 * 512;
        usize i = 0;
        while (i < n) {
            usize n1 = n - i;
            if (n1 > mx)
                n1 = mx;
            OpContext ctx;
            bcache.begin_op(&ctx);
            inodes.lock(f->ip);
            usize sz = inodes.write(&ctx, f->ip, addr + i, f->off, n);
            if (sz > 0)
                f->off += sz;
            inodes.unlock(f->ip);
            bcache.end_op(&ctx);
            if (sz < 0)
                break;
            if (sz != n1)
                PANIC("short filewrite");
            i += sz;
        }
        return i == n ? n : -1;
    }
    PANIC("not inode");
}
