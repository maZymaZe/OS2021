#include <elf.h>

#include "trap.h"

#include <fs/file.h>
#include <fs/inode.h>

#include <aarch64/mmu.h>
#include <common/string.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/virtual_memory.h>

// static u64 auxv[][2] = {{AT_PAGESZ, PAGE_SIZE}};

// Load a program segment into pagetable at virtual address va.
// va-off must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int loaduvm(PTEntriesPtr pgdir, u64 va, Inode* ip, u32 offset, u32 sz) {
    printf("va%x\n", va);

    int n;
    u64 pa, i, va0;
    while (sz > 0) {
        va0 = ROUNDDOWN(va, PAGE_SIZE);
        pa = (u64)uva2ka(pgdir, (char*)va0);
        if (pa == 0)
            PANIC("addr not exist");
        n = MIN(PGSIZE - (va - va0), sz);
        if (inodes.read(ip, (u8*)pa + (va - va0), offset, (usize)n) != (usize)n)
            return -1;
        printf("read %llx ,dest %llx,off %llx,sz %llx  ||va0:%llx\n", ip, pa,
               offset, n, va0);
        offset += n;
        sz -= n;
        va += n;
    }
    return 0;

    // int n;
    // u64 pa, i;
    // for (i = 0; i < sz; i += PGSIZE) {
    //     pa = (u64)uva2ka(pgdir, (char*)(va + i));
    //     if (pa == 0)
    //         PANIC("addr not exist");
    //     if (sz - i < PGSIZE)
    //         n = (int)(sz - i);
    //     else
    //         n = PGSIZE;
    //     if (inodes.read(ip, (u8*)pa, offset + i, (usize)n) != (usize)n)
    //         return -1;
    //     printf("read %x ,dest %x,off %x,sz %x\n", ip, pa, offset + i, n);
    // }
    // return 0;

    // if ((u64)(va - offset) % PGSIZE) {
    //     PANIC("loaduvm: addr 0x%p not page aligned\n", va);
    // }
    // // first page
    // do {
    //     u64 va = ROUNDDOWN((u64)va, PGSIZE);
    //     u64* pte = pgdir_walk(pgdir, va, 0);
    //     if (pte == 0) {
    //         PANIC("loaduvm: addr 0x%p should exist\n", va);
    //     }
    //     u64 pa = PTE_ADDRESS(*pte);
    //     u64 start = (u64)va % PGSIZE;
    //     u32 n = MIN(sz, PGSIZE - start);
    //     if (inodes.read(ip, P2K(pa + start), offset, n) != n) {
    //         return -1;
    //     }
    //     printf("read %x ,dest %x,off %x,sz %x\n", ip, P2K(pa + start),
    //     offset,
    //            n);
    //     offset += n;
    //     va += n;
    //     sz -= n;
    // } while (0);
    // for (u32 i = 0; i < sz; i += PGSIZE) {
    //     u64* pte = pgdir_walk(pgdir, va + i, 0);
    //     if (pte == 0) {
    //         PANIC("loaduvm: addr 0x%p should exist\n", va + i);
    //     }
    //     u64 pa = PTE_ADDRESS(*pte);
    //     u32 n = MIN(sz - i, PGSIZE);
    //     if (inodes.read(ip, P2K(pa), offset + i, n) != n) {
    //         return -1;
    //     }
    //     printf("read %x ,dest %x,off %x,sz %x\n", ip, P2K(pa), offset, n);
    // }
    // return 0;
}

int execve(const char* path, char* const argv[], char* const envp[]) {
    /* TODO: Lab9 Shell */
    printf("enter exec,%s\n", path);
    if (envp) {
    }
    OpContext ctx;
    bcache.begin_op(&ctx);
    Inode* ip = namei(path, &ctx);
    if (!ip)
        return -1;
    inodes.lock(ip);

    for (int i = 0; i < 12; i++) {
        printf("%d: %x\n", i, ip->entry.addrs[i]);
    }
    printf("idr %x\n", ip->entry.indirect);

    Elf64_Ehdr elf;
    PTEntriesPtr pgdir = 0;
    if (inodes.read(ip, (u8*)(&elf), 0, sizeof(elf)) < sizeof(elf)) {
        goto bad;
    }
    if (strncmp((const char*)elf.e_ident, ELFMAG, 4)) {
        PANIC("bad magic");
    }
    pgdir = pgdir_init();
    if (pgdir == 0)
        goto bad;

    u64 sz = 0;
    Elf64_Phdr ph;
    for (u64 i = 0, off = elf.e_phoff; i < elf.e_phnum;
         i++, off += sizeof(ph)) {
        if ((inodes.read(ip, (u8*)&ph, off, sizeof(ph)) != sizeof(ph)))
            goto bad;
        if (ph.p_type != PT_LOAD)
            continue;
        if (ph.p_memsz < ph.p_filesz)
            goto bad;
        sz = (u64)uvm_alloc(pgdir, 0, 8192, sz, ph.p_vaddr + ph.p_memsz);
        if (sz == 0)
            goto bad;
        // FIXME:loaduvm
        if (loaduvm(pgdir, (u64)ph.p_vaddr, ip, (u32)ph.p_offset,
                    (u32)ph.p_filesz) < 0) {
            goto bad;
        }
        printf("ph.memsz%x\n", ph.p_memsz);
        printf("pgdir:%x |pvaddr:%x | poffset:%x| pfilesz%x\n", pgdir,
               ph.p_vaddr, ph.p_offset, ph.p_filesz);
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);

    ip = 0;
    sz = ROUNDUP(sz, PGSIZE);
    sz = (u64)uvm_alloc(pgdir, 0, 8192, sz, sz + (PGSIZE << 1));
    printf("sz%x \n", sz);
    if (!sz)
        goto bad;
    // FIXME:clearpteu
    // clearpteu(pgdir, (char*)(sz - 2 * PGSIZE));
    clearpteu(pgdir, (char*)(sz - (PGSIZE << 1)));
    u64 sp = sz;
    printf("sp%x\n", sp);
    int argc = 0;
    u64 ustk[3 + 32 + 1];
    if (argv) {
        for (; argv[argc]; argc++) {
            if (argc > 32)
                goto bad;
            sp -= strlen(argv[argc]) + 1;
            sp = ROUNDDOWN(sp, 16);
            if (copyout(pgdir, (void*)sp, argv[argc], strlen(argv[argc]) + 1) <
                0) {
                goto bad;
            }
            ustk[argc] = sp;
        }
    }
    ustk[argc] = 0;
    thiscpu()->proc->tf->x0 = (u64)argc;
    if ((argc & 1) == 0)
        sp -= 8;

    u64 auxv[] = {0, AT_PAGESZ, PGSIZE, AT_NULL};
    sp -= sizeof(auxv);
    if (copyout(pgdir, (void*)sp, auxv, sizeof(auxv)) < 0)
        goto bad;

    sp -= 8;
    u64 tmp = 0;
    if (copyout(pgdir, (void*)sp, &tmp, 8) < 0)
        goto bad;

    sp = sp - (u64)(argc + 1) * 8;
    thiscpu()->proc->tf->x1 = sp;
    if (copyout(pgdir, (void*)sp, ustk, ((u64)argc + 1) * 8) < 0)
        goto bad;

    sp -= 8;
    if (copyout(pgdir, (void*)sp, &thiscpu()->proc->tf->x0, 8) < 0)
        goto bad;

    u64* oldpgdir = thiscpu()->proc->pgdir;
    thiscpu()->proc->pgdir = pgdir;
    thiscpu()->proc->sz = sz;
    thiscpu()->proc->tf->sp_el0 = sp;
    thiscpu()->proc->tf->elr_el1 = elf.e_entry;
    uvm_switch(thiscpu()->proc->pgdir);
    printf("pgdir%x | ustksp%x\n", pgdir, thiscpu()->proc->tf->x1);
    vm_free(oldpgdir);

    printf("pte: %llx\n", *pgdir_walk(pgdir, ROUNDDOWN(0x40014c, PGSIZE), 0));

    return 0;
bad:
    if (pgdir)
        vm_free(pgdir);
    if (ip) {
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
    }
    /*
     * Step1: Load data from the file stored in `path`.
     * The first `sizeof(struct Elf64_Ehdr)` bytes is the ELF header part.
     * You should check the ELF magic number and get the `e_phoff` and `e_phnum`
     * which is the starting byte of program header.
     *
     * Step2: Load program headers
     * Program headers are stored like:
     * struct Elf64_Phdr phdr[e_phnum];
     * e_phoff is the address of phdr[0].
     * For each program header, if the type is LOAD, you should:
     * (1) allocate memory, va region [vaddr, vaddr+memsz)
     * (2) copy [offset, offset + filesz) of file to va [vaddr, vaddr+filesz) of
     * memory
     *
     * Step3: Allocate and initialize user stack.
     *
     * The va of the user stack is not required to be any fixed value. It can be
     * randomized.
     *
     * Push argument strings.
     *
     * The initial stack is like
     *
     *   +-------------+
     *   | auxv[o] = 0 |
     *   +-------------+
     *   |    ....     |
     *   +-------------+
     *   |   auxv[0]   |
     *   +-------------+
     *   | envp[m] = 0 |
     *   +-------------+
     *   |    ....     |
     *   +-------------+
     *   |   envp[0]   |
     *   +-------------+
     *   | argv[n] = 0 |  n == argc
     *   +-------------+
     *   |    ....     |
     *   +-------------+
     *   |   argv[0]   |
     *   +-------------+
     *   |    argc     |
     *   +-------------+  <== sp
     *
     * where argv[i], envp[j] are 8-byte pointers and auxv[k] are
     * called auxiliary vectors, which are used to transfer certain
     * kernel level information to the user processes.
     *
     * ## Example
     *
     * ```
     * sp -= 8; *(size_t *)sp = AT_NULL;
     * sp -= 8; *(size_t *)sp = PGSIZE;
     * sp -= 8; *(size_t *)sp = AT_PAGESZ;
     *
     * sp -= 8; *(size_t *)sp = 0;
     *
     * // envp here. Ignore it if your don't want to implement envp.
     *
     * sp -= 8; *(size_t *)sp = 0;
     *
     * // argv here.
     *
     * sp -= 8; *(size_t *)sp = argc;
     *
     * // Stack pointer must be aligned to 16B!
     *
     * thisproc()->tf->sp = sp;
     * ```
     *
     * There are two important entry point addresses:
     * (1) Address of the first user-level instruction: that's stored in
     * elf_header.entry (2) Adresss of the main function: that's stored in
     * memory (loaded in part2)
     *
     */

    return -1;
}
