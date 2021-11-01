#include <aarch64/mmu.h>
#include <common/types.h>
#include <core/console.h>
#include <core/physical_memory.h>

extern char end[];
PMemory pmem; /* TO-DO: Lab4 multicore: Add locks where needed */
FreeListNode head;
/*
 * Editable, as long as it works as a memory manager.
 */
static void freelist_init(void* datastructure_ptr, void* start, void* end);
static void* freelist_alloc(void* datastructure_ptr);
static void freelist_free(void* datastructure_ptr, void* page_address);

/*
 * Allocate one 4096-byte page of physical memory.
 * Returns a pointer that the kernel can use.
 * Returns 0 if the memory cannot be allocated.
 */
static void* freelist_alloc(void* datastructure_ptr) {
    FreeListNode* f = ((FreeListNode*)datastructure_ptr)->next;
    /* TO-DO: Lab2 memory*/
    if (f) {
        ((FreeListNode*)(pmem.struct_ptr))->next = f->next;
        for (int i = 0; i < PAGE_SIZE; i++)
            ((char*)f)[i] = 0;
    }
    return f;
}

/*
 * Free the page of physical memory pointed at by page_address.
 */
static void freelist_free(void* datastructure_ptr, void* page_address) {
    if ((uint64_t)page_address % PAGE_SIZE || page_address < end ||
        (void*)P2K(0x3F000000) <= page_address) {
        PANIC("ERR ADDR");
    }
    FreeListNode* f = (FreeListNode*)datastructure_ptr;
    for (int i = 0; i < PAGE_SIZE; i++)
        ((char*)page_address)[i] = 0;
    /* TO-DO: Lab2 memory*/
    FreeListNode* p = (FreeListNode*)page_address;
    p->next = f->next;
    f->next = p;
}

/*
 * Record all memory from start to end to freelist as initialization.
 */

static void freelist_init(void* datastructure_ptr, void* start, void* end) {
    free_range(start, end);
    // FreeListNode* f = (FreeListNode*)datastructure_ptr;
    // void* p = start;
    // f->next = p;
    // /* TO-DO: Lab2 memory*/

    // while (p + PAGE_SIZE < end) {
    //     FreeListNode* q = (FreeListNode*)p;
    //     q->next = p + PAGE_SIZE;
    //     p = p + PAGE_SIZE;
    // }
    // FreeListNode* q = (FreeListNode*)p;
    // q->next = NULL;
}

static void init_PMemory(PMemory* pmem_ptr) {
    pmem_ptr->struct_ptr = (void*)&head;
    pmem_ptr->page_init = freelist_init;
    pmem_ptr->page_alloc = freelist_alloc;
    pmem_ptr->page_free = freelist_free;
}

void init_memory_manager(void) {
    // HA-CK Raspberry pi 4b.
    // size_t phystop = MIN(0x3F000000, mbox_get_arm_memory());
    size_t phystop = 0x3F000000;

    // notice here for roundup
    void* ROUNDUP_end = ROUNDUP((void*)end, PAGE_SIZE);
    init_PMemory(&pmem);
    pmem.page_init(pmem.struct_ptr, ROUNDUP_end, (void*)P2K(phystop));
    init_spinlock(&pmem.lock, "pmem");
}

/*
 * Record all memory from start to end to memory manager.
 */
void free_range(void* start, void* end) {
    for (void* p = start; p + PAGE_SIZE <= end; p += PAGE_SIZE)
        pmem.page_free(pmem.struct_ptr, p);
}

/*
 * Allocate a page of physical memory.
 * Returns 0 if failed else a pointer.
 * Corrupt the page by filling non-zero value in it for debugging.
 */
void* kalloc(void) {
    acquire_spinlock(&pmem.lock);
    void* p = pmem.page_alloc(pmem.struct_ptr);
    release_spinlock(&pmem.lock);
    return p;
}

/* Free the physical memory pointed at by page_address. */
void kfree(void* page_address) {
    acquire_spinlock(&pmem.lock);
    pmem.page_free(pmem.struct_ptr, page_address);
    release_spinlock(&pmem.lock);
}
