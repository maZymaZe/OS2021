#include <aarch64/intrinsic.h>
#include <common/defines.h>
#include <common/string.h>
#include <common/types.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/virtual_memory.h>

/* For simplicity, we only support 4k pages in user pgdir. */

extern PTEntries kpgdir;
VMemory vmem;
PTEntriesPtr pgdir_init() {
    return vmem.pgdir_init();
}

PTEntriesPtr pgdir_walk(PTEntriesPtr pgdir, void* vak, int alloc) {
    return vmem.pgdir_walk(pgdir, vak, alloc);
}

void vm_free(PTEntriesPtr pgdir) {
    vmem.vm_free(pgdir);
}

int uvm_map(PTEntriesPtr pgdir, void* va, size_t sz, u64 pa) {
    return vmem.uvm_map(pgdir, va, sz, pa);
}

void uvm_switch(PTEntriesPtr pgdir) {
    // FIX-ME: Use NG and ASID for efficiency.
    arch_set_ttbr0(K2P(pgdir));
}

/*
 * generate a empty page as page directory
 */

static PTEntriesPtr my_pgdir_init() {
    /* TO-DO: Lab2 memory*/
    return kalloc();
}

/*
 * return the address of the pte in user page table
 * pgdir that corresponds to virtual address va.
 * if alloc != 0, create any required page table pages.
 */

static PTEntriesPtr my_pgdir_walk(PTEntriesPtr pgdir, void* vak, int alloc) {
    /* TO-DO: Lab2 memory*/
    for (int level = 3; level > 0; level--) {
        u64* pte = &(pgdir[PX(level, vak)]);
        if (*pte & PTE_VALID) {
            pgdir = P2K(PTE_ADDRESS(*pte));
        } else {
            if (!alloc || (pgdir = (PTEntriesPtr)kalloc()) == 0)
                return 0;
            *pte = K2P((u64)pgdir) | PTE_TABLE;
        }
    }
    return &pgdir[PX(0, vak)];
}

/* Free a user page table and all the physical memory pages. */
void my_vm_free_r(PTEntriesPtr pgdir, int x) {
    if (x == 3) {
        for (int i = 0; i < N_PTE_PER_TABLE; i++) {
            if (pgdir[i] & PTE_VALID) {
                kfree(P2K(PTE_ADDRESS(pgdir[i])));
            }
        }
        return;
    }
    for (int i = 0; i < N_PTE_PER_TABLE; i++) {
        if (pgdir[i] & PTE_VALID) {
            my_vm_free_r(P2K(PTE_ADDRESS(pgdir[i])), x + 1);
        }
    }
    kfree((void*)pgdir);
}
void my_vm_free(PTEntriesPtr pgdir) {
    /* TO-DO: Lab2 memory*/
    my_vm_free_r(pgdir, 0);
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might not
 * be page-aligned.
 * Return -1 if failed else 0.
 */

int my_uvm_map(PTEntriesPtr pgdir, void* va, size_t sz, u64 pa) {
    /* TO-DO: Lab2 memory*/
    u64 a, last;
    PTEntriesPtr pte;
    if (!sz)
        PANIC("map:sz 0");
    a = (u64)ROUNDDOWN(va, PGSIZE);
    last = (u64)ROUNDDOWN((va + sz - 1), PGSIZE);
    while (true) {
        if ((pte = pgdir_walk(pgdir, (void*)a, 1)) == 0)
            return -1;
        if (*pte & PTE_VALID)
            PANIC("remap");
        *pte = pa | PTE_USER_DATA;
        // printf("!%llx pte:%llx!\n", *pte, pte);
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

/*
 * Fork a process's page table.
 * Copy all user-level memory resource owned by pgdir.
 * Only used in `fork()`.
 */
static PTEntriesPtr my_uvm_copy(PTEntriesPtr pgdir) {
    /* TODO: Lab9 Shell */
    u64* pte;
    u64 pa;
    char* mem;
    PTEntriesPtr newpgdir = pgdir_init();
    for (u64 i = 0;; i += PGSIZE) {
        pte = pgdir_walk(pgdir, i, 0);
        if ((!pte) || (*pte & PTE_VALID) == 0)
            break;
        pa = P2K(PTE_ADDRESS(*pte));
        mem = kalloc();
        memmove(mem, (char*)pa, PGSIZE);
        uvm_map(newpgdir, i, PGSIZE, mem);
    }
    return newpgdir;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void uvmunmap(PTEntriesPtr pgdir, u64 va, u64 npages, int do_free) {
    u64 a;
    u64* pte;
    if (va % PGSIZE) {
        PANIC("not aligned");
    }
    for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {
        pte = pgdir_walk(pgdir, (void*)a, 0);
        if (!pte)
            PANIC("walk");
        if (!(*pte & PTE_VALID))
            PANIC("not mapped");
        if (PTE_FLAGS(*pte) == PTE_VALID)
            PANIC("not a leaf");
        if (do_free) {
            u64 pa = P2K(PTE_ADDRESS(*pte));
            kfree((void*)pa);
        }
        *pte = 0;
    }
}

/*
 * Allocate page tables and physical memory to grow process
 * from oldsz to newsz, which need not be page aligned.
 * Stack size stksz should be page aligned.
 * Returns new size or 0 on error.
 */

int my_uvm_alloc(PTEntriesPtr pgdir,
                 usize base,
                 usize stksz,
                 usize oldsz,
                 usize newsz) {
    /* TODO: Lab9 Shell */
    char* mem;
    u64 a;
    if (newsz < oldsz)
        return oldsz;
    if (base + newsz > stksz)
        PANIC("overflow");
    oldsz = ROUNDUP(oldsz, PGSIZE);
    for (a = oldsz; a < newsz; a += PGSIZE) {
        mem = kalloc();
        if (mem == 0) {
            uvm_dealloc(pgdir, 0, a, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (uvm_map(pgdir, (void*)a, PGSIZE, (u64)mem) != 0) {
            kfree(mem);
            uvm_dealloc(pgdir, 0, a, oldsz);
        }
    }
    return (int)newsz;
}

/*
 * Deallocate user pages to bring the process size from oldsz to
 * newsz.  oldsz and newsz need not be page-aligned, nor does newsz
 * need to be less than oldsz.  oldsz can be larger than the actual
 * process size.  Returns the new process size.
 */

int my_uvm_dealloc(PTEntriesPtr pgdir, usize base, usize oldsz, usize newsz) {
    /* TODO: Lab9 Shell */
    if (newsz >= oldsz)
        return (int)oldsz;
    if (ROUNDUP(newsz, PGSIZE) < ROUNDUP(oldsz, PGSIZE)) {
        int npgs =
            (int)((ROUNDUP(oldsz, PGSIZE) - ROUNDUP(newsz, PGSIZE)) / PGSIZE);
        uvmunmap(pgdir, ROUNDUP(newsz, PGSIZE), npgs, 1);
    }
    return 0;
}

// Clear PTE_U on a page. Used to create an inaccessible page beneath
// the user stack (to trap stack underflow).
void clearpteu(PTEntriesPtr pgdir, char* uva) {
    u64* pte;

    pte = pgdir_walk(pgdir, uva, 0);
    if (pte == 0) {
        PANIC("clearpteu");
    }

    // in ARM, we change the AP field (ap & 0x3) << 4)
    *pte = (*pte & ~(0x03U << 6));
}

// PAGEBREAK!
// Map user virtual address to kernel address.
char* uva2ka(u64* pgdir, char* uva) {
    // FIXME:alloc or not
    u64* pte = pgdir_walk(pgdir, uva, 0);
    if ((*pte & (PTE_VALID)) == 0)
        return 0;
    if (((*pte) & PTE_USER) == 0)
        return 0;
    return (char*)P2K(PTE_ADDRESS(*pte));
}
// PAGEBREAK!
// Map user virtual address to kernel address.
char* uva2ka1(u64* pgdir, char* uva) {
    // FIXME:alloc or not
    u64* pte = pgdir_walk(pgdir, uva, 1);
    return (char*)P2K(PTE_ADDRESS(*pte));
}
/*
 * Copy len bytes from p to user address va in page table pgdir.
 * Allocate physical pages if required.
 * Useful when pgdir is not the current page table.
 *
 * p is kernel virtual address.
 * va is target user virtual address.
 *
 * this function works as:
 * memcpy(dest=P2V(walk(pgdir, va, alloc=True)), src=p, size=len)
 */

int my_copyout(PTEntriesPtr pgdir, void* va, void* p, usize len) {
    /* TODO: Lab9 Shell */
    char *buf, *pa0;
    u64 n, va0;
    buf = p;
    while (len > 0) {
        va0 = (u64)ROUNDDOWN(va, PAGE_SIZE);
        // FIXME
        pa0 = uva2ka1(pgdir, (char*)va0);
        if (pa0 == 0)
            return -1;
        n = PAGE_SIZE - ((u64)va - va0);
        if (n > len)
            n = len;
        memmove(pa0 + ((u64)va - va0), buf, n);
        len -= n;
        buf += n;
        va = va0 + PAGE_SIZE;
    }
    return 0;
}

void virtual_memory_init(VMemory* vmt_ptr) {
    vmt_ptr->pgdir_init = my_pgdir_init;
    vmt_ptr->pgdir_walk = my_pgdir_walk;
    vmt_ptr->uvm_copy = my_uvm_copy;
    vmt_ptr->vm_free = my_vm_free;
    vmt_ptr->uvm_map = my_uvm_map;
    vmt_ptr->uvm_alloc = my_uvm_alloc;
    vmt_ptr->uvm_dealloc = my_uvm_dealloc;
    vmt_ptr->copyout = my_copyout;
}

void init_virtual_memory() {
    virtual_memory_init(&vmem);
}

void vm_test() {
    /* TO-DO: Lab2 memory*/

    // const int TEST_RUNS = 200;

    // // test1
    // void* gen[TEST_RUNS];
    // for (int i = 0; i < TEST_RUNS; i++) {
    //     printf("%x\n", gen[i] = kalloc());
    // }
    // for (int i = 0; i < TEST_RUNS; i++) {
    //     kfree(gen[i]);
    // }
    // for (int i = 0; i < TEST_RUNS; i++) {
    //     printf("%x\n", gen[i] = kalloc());
    // }
    // for (int i = 0; i < TEST_RUNS; i++) {
    //     kfree(gen[i]);
    // }

    // test2
    // void* v[TEST_RUNS];
    // void* p[TEST_RUNS];
    // void* pgdir = pgdir_init();
    // for (int i = 0; i < TEST_RUNS; i++) {
    //     v[i] = i * PGSIZE;
    // }
    // for (int i = 0; i < TEST_RUNS; i++) {
    //     uvm_map(pgdir, v[i], PGSIZE, p[i] = K2P(kalloc()));
    // }
    // for (int i = 0; i < TEST_RUNS; i++) {
    //     // printf("%llx\t%llx\n", p[i],
    //     //        P2K(PTE_ADDRESS(*my_pgdir_walk(pgdir, v[i], 0))));
    //     if (p[i] != PTE_ADDRESS(*my_pgdir_walk(pgdir, v[i], 0))) {
    //         PANIC("ERR MAP");
    //     }
    // }
    // for (int i = 0; i < TEST_RUNS; i++) {
    //     kfree(P2K(p[i]));
    // }
    // vm_free(pgdir);

    *((i64*)P2K(0)) = 0xac;
    char* p = kalloc();
    memset(p, 0, PAGE_SIZE);
    uvm_map((u64*)p, (void*)0x1000, PAGE_SIZE, 0);
    uvm_switch((u64*)p);
    PTEntry* pte = pgdir_walk((u64*)p, (void*)0x1000, 0);
    if (pte == 0) {
        puts("walk should not return 0");
        while (1)
            ;
    }
    if (((u64)pte >> 48) == 0) {
        puts("pte should be virtual address");
        while (1)
            ;
    }
    if ((*pte) >> 48 != 0) {
        puts("*pte should store physical address");
        while (1)
            ;
    }
    if (((*pte) & PTE_USER_DATA) != PTE_USER_DATA) {
        puts("*pte should contain USE_DATA flags");
        while (1)
            ;
    }
    if (*((i64*)0x1000) == 0xac) {
        puts("Test_Map_Region Pass!");
    } else {
        puts("Test_Map_Region Fail!");
        while (1)
            ;
    }

    // Certify that your code works!
}
