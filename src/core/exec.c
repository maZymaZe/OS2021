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

// static uint64_t auxv[][2] = {{AT_PAGESZ, PAGE_SIZE}};
// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int loadseg(PTEntriesPtr pgdir, u64 va, Inode* ip, u32 offset, u32 sz) {
    int i, n;
    u64 pa;
    for (i = 0; i < sz; i += PGSIZE) {
        pa = uva2ka(pgdir, va + i);
        if (pa == 0)
            PANIC("addr not exist");
        if (sz - i < PGSIZE)
            n = sz - i;
        else
            n = PGSIZE;
        if (inodes.read(ip, pa, offset + i, n) != n)
            return -1;
    }
    return 0;
}

int execve(const char* path, char* const argv[], char* const envp[]) {
    /* TODO: Lab9 Shell */
    OpContext ctx;
    bcache.begin_op(&ctx);
    Inode* ip = namei(path, &ctx);
    if (!ip)
        return -1;
    inodes.lock(ip);

    Elf64_Ehdr elf;
    u64* pgdir = 0;
    if (inodes.read(ip, (char*)&elf, 0, sizeof(elf)) < sizeof(elf)) {
        goto bad;
    }
    if (strncmp(elf.e_ident, ELFMAG, 4)) {
        PANIC("bad magic");
    }
    pgdir = pgdir_init();
    if (pgdir == 0)
        goto bad;

    u64 sz = 0;
    Elf64_Phdr ph;
    for (int i = 0, off = elf.e_phoff; i < elf.e_phnum;
         i++, off += sizeof(ph)) {
        if ((inodes.read(ip, &ph, off, sizeof(ph)) != sizeof(ph)))
            goto bad;
        if (ph.p_type != PT_LOAD)
            continue;
        if (ph.p_memsz < ph.p_filesz)
            goto bad;
        sz = uvm_alloc(pgdir, 0, 8192, sz, ph.p_vaddr + ph.p_memsz);
        if (sz == 0)
            goto bad;
        // FIXME:loaduvm
        if (loadseg(pgdir, (char*)ph.p_vaddr, ip, ph.p_offset, ph.p_filesz) <
            0) {
            goto bad;
        }
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);

    ip = 0;
    sz = ROUNDUP(sz, PGSIZE);
    sz = uvm_alloc(pgdir, 0, 8192, sz, sz + (PGSIZE << 1));
    if (!sz)
        goto bad;
    // FIXME:clearpteu
    clearpteu(pgdir, (char*)(sz - 2 * PGSIZE));
    u64 sp = sz;
    int argc = 0;
    u64 ustk[3 + 32 + 1];
    for (; argv[argc]; argc++) {
        if (argc > 32)
            goto bad;
        sp -= strlen(argv[argc]) + 1;
        sp = ROUNDDOWN(sp, 16);
        if (copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0) {
            goto bad;
        }
        ustk[argc] = sp;
    }
    ustk[argc] = 0;
    thiscpu()->proc->tf->x0 = argc;
    if ((argc & 1) == 0)
        sp -= 8;
    u64 auxv[] = {0, AT_PAGESZ, PGSIZE, AT_NULL};
    sp -= sizeof(auxv);
    if (copyout(pgdir, sp, auxv, sizeof(auxv)) < 0)
        goto bad;
    sp -= 8;
    u64 tmp;
    if (copyout(pgdir, sp, &tmp, 8) < 0)
        goto bad;
    sp = sp - (argc + 1) * 8;
    thiscpu()->proc->tf->x1 = sp;
    if (copyout(pgdir, sp, ustk, (argc + 1) * 8) < 0)
        goto bad;
    sp -= 8;
    if (copyout(pgdir, sp, &thiscpu()->proc->tf->x0, 8) < 0)
        goto bad;
    u64* oldpgdir = thiscpu()->proc->pgdir;
    thiscpu()->proc->pgdir = pgdir;
    thiscpu()->proc->sz = sz;
    thiscpu()->proc->tf->sp_el0 = sp;
    thiscpu()->proc->tf->elr_el1 = elf.e_entry;
    uvm_switch(thiscpu()->proc->pgdir);
    vm_free(oldpgdir);
    return thiscpu()->proc->tf->x0;
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
