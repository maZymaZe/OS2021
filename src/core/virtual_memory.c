#include <aarch64/intrinsic.h>
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

int uvm_map(PTEntriesPtr pgdir, void* va, size_t sz, uint64_t pa) {
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
        uint64_t* pte = &(pgdir[PX(level, vak)]);
        if (*pte & PTE_VALID) {
            pgdir = P2K(PTE_ADDRESS(*pte));
        } else {
            if (!alloc || (pgdir = (PTEntriesPtr)kalloc()) == 0)
                return 0;
            *pte = K2P((uint64_t)pgdir) | PTE_TABLE;
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

int my_uvm_map(PTEntriesPtr pgdir, void* va, size_t sz, uint64_t pa) {
    /* TO-DO: Lab2 memory*/
    uint64_t a, last;
    PTEntriesPtr pte;
    if (!sz)
        PANIC("map:sz 0");
    a = ROUNDDOWN(va, PGSIZE);
    last = ROUNDDOWN((va + sz - 1), PGSIZE);
    while (true) {
        if ((pte = my_pgdir_walk(pgdir, a, 1)) == 0)
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

void virtual_memory_init(VMemory* vmem_ptr) {
    vmem_ptr->pgdir_init = my_pgdir_init;
    vmem_ptr->pgdir_walk = my_pgdir_walk;
    vmem_ptr->vm_free = my_vm_free;
    vmem_ptr->uvm_map = my_uvm_map;
}

void init_virtual_memory() {
    virtual_memory_init(&vmem);
}

void vm_test() {
    /* TO-DO: Lab2 memory*/

    const int TEST_RUNS = 200;

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

    *((int64_t*)P2K(0)) = 0xac;
    char* p = kalloc();
    memset(p, 0, PAGE_SIZE);
    uvm_map((uint64_t*)p, (void*)0x1000, PAGE_SIZE, 0);
    uvm_switch(p);
    PTEntry* pte = pgdir_walk(p, (void*)0x1000, 0);
    if (pte == 0) {
        puts("walk should not return 0");
        while (1)
            ;
    }
    if (((uint64_t)pte >> 48) == 0) {
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
    if (*((int64_t*)0x1000) == 0xac) {
        puts("Test_Map_Region Pass!");
    } else {
        puts("Test_Map_Region Fail!");
        while (1)
            ;
    }

    // Certify that your code works!
}