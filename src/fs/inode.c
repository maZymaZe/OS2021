#include <common/string.h>
#include <core/arena.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <fs/inode.h>

// this lock mainly prevents concurrent access to inode list `head`, reference
// count increment and decrement.
static SpinLock lock;
static ListNode head;

static const SuperBlock* sblock;
static const BlockCache* cache;
static Arena arena;

// return which block `inode_no` lives on.
static INLINE usize to_block_no(usize inode_no) {
    return sblock->inode_start + (inode_no / (INODE_PER_BLOCK));
}

// return the pointer to on-disk inode.
static INLINE InodeEntry* get_entry(Block* block, usize inode_no) {
    return ((InodeEntry*)block->data) + (inode_no % INODE_PER_BLOCK);
}

// return address array in indirect block.
static INLINE u32* get_addrs(Block* block) {
    return ((IndirectBlock*)block->data)->addrs;
}

// initialize inode tree.
void init_inodes(const SuperBlock* _sblock, const BlockCache* _cache) {
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};

    init_spinlock(&lock, "InodeTree");
    init_list_node(&head);
    sblock = _sblock;
    cache = _cache;
    init_arena(&arena, sizeof(Inode), allocator);

    inodes.root = inodes.get(ROOT_INODE_NO);
}

// initialize in-memory inode.
static void init_inode(Inode* inode) {
    init_spinlock(&inode->lock, "Inode");
    init_rc(&inode->rc);
    init_list_node(&inode->node);
    inode->inode_no = 0;
    inode->valid = false;
}

// see `inode.h`.
static usize inode_alloc(OpContext* ctx, InodeType type) {
    assert(type != INODE_INVALID);

    // TODO
    u32 inum;
    for (inum = 1; inum <= sblock->num_inodes; inum++) {
        Block* bp = cache->acquire(to_block_no(inum));
        InodeEntry* dip = get_entry(bp, inum);
        if (dip->type == 0) {
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            cache->sync(ctx, bp);
            cache->release(bp);
            // return (inode_get(inum))->inode_no;
            return inum;
        }
        cache->release(bp);
    }

    PANIC("failed to allocate inode on disk");
}

// see `inode.h`.
static void inode_lock(Inode* inode) {
    assert(inode->rc.count > 0);
    acquire_spinlock(&inode->lock);
}

// see `inode.h`.
static void inode_unlock(Inode* inode) {
    assert(holding_spinlock(&inode->lock));
    assert(inode->rc.count > 0);
    release_spinlock(&inode->lock);
}

// see `inode.h`.
static void inode_sync(OpContext* ctx, Inode* inode, bool do_write) {
    // TODO
    Block* bp = cache->acquire(to_block_no(inode->inode_no));
    InodeEntry* dip = get_entry(bp, inode->inode_no);
    if (do_write && inode->valid) {
        dip->type = inode->entry.type;
        dip->major = inode->entry.major;
        dip->minor = inode->entry.minor;
        dip->num_links = inode->entry.num_links;
        dip->num_bytes = inode->entry.num_bytes;
        dip->indirect = inode->entry.indirect;
        memmove(dip->addrs, inode->entry.addrs, sizeof(inode->entry.addrs));
        cache->sync(ctx, bp);

    } else if (!inode->valid) {
        inode->valid = true;
        inode->entry.type = dip->type;
        inode->entry.major = dip->major;
        inode->entry.minor = dip->minor;
        inode->entry.num_links = dip->num_links;
        inode->entry.num_bytes = dip->num_bytes;
        inode->entry.indirect = dip->indirect;
        memmove(inode->entry.addrs, dip->addrs, sizeof(inode->entry.addrs));
    }
    cache->release(bp);
}

// see `inode.h`.
static Inode* inode_get(usize inode_no) {
    assert(inode_no > 0);
    assert(inode_no < sblock->num_inodes);
    acquire_spinlock(&lock);
    // TODO
    ListNode *p = head.next, *empty = 0;
    Inode* ip;
    while (p != &head) {
        if (!p) {
            PANIC("empty p");
        }
        ip = container_of(p, Inode, node);
        if (ip->rc.count > 0 && ip->inode_no == inode_no) {
            increment_rc(&(ip->rc));
            release_spinlock(&lock);
            return ip;
        }
        if (empty == 0 && ip->rc.count == 0) {
            empty = &(ip->node);
        }
        p = p->next;
    }
    if (empty) {
        ip = container_of(empty, Inode, node);
    } else {
        ip = (Inode*)alloc_object(&arena);
        init_inode(ip);
        merge_list(&head, &(ip->node));
    }

    ip->inode_no = inode_no;
    increment_rc(&(ip->rc));
    ip->valid = 0;
    inode_lock(ip);
    inode_sync(NULL, ip, false);
    inode_unlock(ip);

    release_spinlock(&lock);
    return ip;

    return NULL;
}
// see `inode.h`.
static void inode_clear(OpContext* ctx, Inode* inode) {
    InodeEntry* entry = &inode->entry;
    for (int i = 0; i < INODE_NUM_DIRECT; i++) {
        if (entry->addrs[i]) {
            cache->free(ctx, entry->addrs[i]);
            entry->addrs[i] = 0;
        }
    }
    if (entry->indirect) {
        Block* bp = cache->acquire(entry->indirect);
        u32* a = (bp->data);
        for (int i = 0; i < INODE_NUM_INDIRECT; i++) {
            if (a[i]) {
                cache->free(ctx, a[i]);
            }
        }
        cache->release(bp);
        cache->free(ctx, entry->indirect);
        entry->indirect = 0;
    }
    entry->num_bytes = 0;
    inode_sync(ctx, inode, true);
    // TODO
}

// see `inode.h`.
static Inode* inode_share(Inode* inode) {
    acquire_spinlock(&lock);
    increment_rc(&inode->rc);
    release_spinlock(&lock);
    return inode;
}

// see `inode.h`.
static void inode_put(OpContext* ctx, Inode* inode) {
    // TODO
    acquire_spinlock(&lock);

    // printf("%d %d %d\n", inode->rc.count, inode->valid,
    // inode->entry.num_links);
    if (inode->rc.count == 1 && inode->valid && inode->entry.num_links == 0) {
        // printf("hello\n");

        inode_lock(inode);
        release_spinlock(&lock);

        inode_clear(ctx, inode);
        inode->entry.type = 0;
        inode_sync(ctx, inode, true);
        inode->valid = 0;

        inode_unlock(inode);
        acquire_spinlock(&lock);

        ListNode* node = &(inode->node);
        node = detach_from_list(node);
        decrement_rc(&(inode->rc));
        release_spinlock(&lock);
        free_object(inode);
        return;
    }
    decrement_rc(&(inode->rc));
    release_spinlock(&lock);
}

// this function is private to inode layer, because it can allocate block
// at arbitrary offset, which breaks the usual file abstraction.
//
// retrieve the block in `inode` where offset lives. If the block is not
// allocated, `inode_map` will allocate a new block and update `inode`, at
// which time, `*modified` will be set to true.
// the block number is returned.
//
// NOTE: caller must hold the lock of `inode`.
static usize inode_map(OpContext* ctx,
                       Inode* inode,
                       usize offset,
                       bool* modified) {
    InodeEntry* entry = &inode->entry;

    // TODO
    u32 addr;
    *modified = false;
    if (offset < INODE_NUM_DIRECT) {
        if ((addr = entry->addrs[offset]) == 0) {
            entry->addrs[offset] = (cache->alloc(ctx));
            addr = entry->addrs[offset];
            *modified = true;
        }

        return addr;
    }
    offset -= INODE_NUM_DIRECT;
    if (offset < INODE_NUM_INDIRECT) {
        if ((addr = (entry->indirect)) == 0) {
            addr = entry->indirect = cache->alloc(ctx);
        }
        Block* bp = cache->acquire(addr);
        u32* a = bp->data;
        if ((addr = a[offset]) == 0) {
            addr = a[offset] = cache->alloc(ctx);
            *modified = true;
            cache->sync(ctx, bp);
        }
        cache->release(bp);
        return addr;
    }
    PANIC("offset out of bound");
    return 0;
}

// see `inode.h`.
static void inode_read(Inode* inode, u8* dest, usize offset, usize count) {
    InodeEntry* entry = &inode->entry;
    usize end = offset + count;
    assert(offset <= entry->num_bytes);
    assert(end <= entry->num_bytes);
    assert(offset <= end);

    // TODO
    u32 tot, m;
    bool mdfd;
    Block* bp;
    for (tot = 0; tot < count; tot += m, offset += m, dest += m) {
        bp = cache->acquire(inode_map(NULL, inode, offset / BLOCK_SIZE, &mdfd));
        m = MIN(count - tot, BLOCK_SIZE - offset % BLOCK_SIZE);
        memmove(dest, bp->data + offset % BLOCK_SIZE, m);
        cache->release(bp);
    }
}

// see `inode.h`.
static void inode_write(OpContext* ctx,
                        Inode* inode,
                        u8* src,
                        usize offset,
                        usize count) {
    InodeEntry* entry = &inode->entry;
    usize end = offset + count;
    assert(offset <= entry->num_bytes);
    assert(end <= INODE_MAX_BYTES);
    assert(offset <= end);

    // TODO
    u32 tot, m;
    bool mdfd;
    Block* bp;
    for (tot = 0; tot < count; tot += m, offset += m, src += m) {
        bp = cache->acquire(inode_map(ctx, inode, offset / BLOCK_SIZE, &mdfd));
        m = MIN(count - tot, BLOCK_SIZE - offset % BLOCK_SIZE);
        memmove(bp->data + offset % BLOCK_SIZE, src, m);
        cache->sync(ctx, bp);
        cache->release(bp);
    }
    if (count > 0 && offset > inode->entry.num_bytes) {
        inode->entry.num_bytes = offset;
        inode_sync(ctx, inode, true);
    }
}

// see `inode.h`.
static usize inode_lookup(Inode* inode, const char* name, usize* index) {
    InodeEntry* entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);

    // TODO
    u32 off, inum;
    DirEntry de;
    for (off = 0; off < entry->num_bytes; off += sizeof(DirEntry)) {
        inode_read(inode, &de, off, sizeof(DirEntry));
        if (de.inode_no == 0)
            continue;
        if (strncmp(de.name, name, FILE_NAME_MAX_LENGTH) == 0) {
            if (index)
                *index = off;
            inum = de.inode_no;
            // FIXME
            return inum;
        }
    }

    return 0;
}

// see `inode.h`.
static usize inode_insert(OpContext* ctx,
                          Inode* inode,
                          const char* name,
                          usize inode_no) {
    InodeEntry* entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);

    // TODO
    usize index;
    if (inode_lookup(inode, name, &index) != 0) {
        // printf("???\n");
        return -1;
    }
    u32 off;
    DirEntry de;
    for (off = 0; off < inode->entry.num_bytes; off += sizeof(DirEntry)) {
        inode_read(inode, &de, off, sizeof(DirEntry));
        if (de.inode_no == 0)
            break;
    }
    strncpy(de.name, name, FILE_NAME_MAX_LENGTH);
    de.inode_no = inode_no;
    inode_write(ctx, inode, &de, off, sizeof(DirEntry));
    return off;
}

// see `inode.h`.
static void inode_remove(OpContext* ctx, Inode* inode, usize index) {
    InodeEntry* entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);
    DirEntry de;
    memset(&de, 0, sizeof(de));
    inode_write(ctx, inode, &de, index, sizeof(DirEntry));
    // TODO
}

InodeTree inodes = {
    .alloc = inode_alloc,
    .lock = inode_lock,
    .unlock = inode_unlock,
    .sync = inode_sync,
    .get = inode_get,
    .clear = inode_clear,
    .share = inode_share,
    .put = inode_put,
    .read = inode_read,
    .write = inode_write,
    .lookup = inode_lookup,
    .insert = inode_insert,
    .remove = inode_remove,
};
