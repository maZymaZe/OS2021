# Lab2 内存管理

## 代码仓库

本实验的代码在 `lab2` 分支中。做完 lab1 后，请在 `lab1` 分支上做一次 Commit，然后加入 `lab2` 分支的代码：

```sh
# After commit
git checkout lab2

git merge lab1
# If merge conflicts exist, You should handle them and then commit
```

## 实验内容简介

### 实验目标

在本实验中，需要实现教学操作系统内核的内存管理系统。

内存管理系统分为两部分：

内核的物理内存分配器，用于分配和回收物理内存，主体位于 `src/core/memory_manage.c` 。

内核的页表，用于将虚拟地址映射到物理地址，主体位于 `src/core/virtual_memory.c` 。

### 参考文档

因为内核需要与硬件大量交互，将参考 ARM 架构文档来理解相关硬件配置。我们会在实验文档中给出可以参考 ARM 架构文档的哪一部分。

以下是一些指代方式的例子：

`ref: C5.2` 表示相关部分在 [ARMv8 Reference Manual](https://cs140e.sergio.bz/docs/ARMv8-Reference-Manual.pdf) 的 C5.2
`A53: 4.3.30` 表示相关部分在 [ARM Cortex-A53 Manual](https://cs140e.sergio.bz/docs/ARM-Cortex-A53-Manual.pdf) 的 4.3.30
`guide: 10.1` 表示相关部分在 [ARMv8-A Programmer Guide](https://cs140e.sergio.bz/docs/ARMv8-A-Programmer-Guide.pdf) 的 10.1

### 物理内存管理

物理内存是指 DRAM 中的储存单元，每个字节的物理内存都有一个地址，称为物理地址。

虚拟内存是程序（用户进程、内核）看到的存储空间。

通常，硬件会内置一个（或许可编程的）转换单元，在 [ICS-2021Spring-FDU ](https://github.com/FDUCSLG/ICS-2021Spring-FDU) 的 MIPS CPU 设计实验中，我们也实现了类似的机制。

在本实验中，你需要为内核实现一个物理内存分配器为后续用户进程的页表、栈等分配空间。

为了方便同学们自主发挥和扩展，我们抽象出了这样几个功能函数，并统一使用内存管理表 PMemory 进行管理。

```c
typedef struct {
    SpinLock lock;
    void *struct_ptr;
    void (*page_init)(void *datastructure_ptr, void *start, void *end);
    void *(*page_alloc)(void *datastructure_ptr);
    void (*page_free)(void *datastructure_ptr, void *page_address);
} PMemory;
```

其中，每一个函数指针应指向你实现的相应函数。

接下来，我们将描述每一个相应函数的参数和理应实现的功能。

```c
NORETURN void (*page_init)(void *datastructure_ptr, void *start, void *end);
/* 	parameters:
 *		void *data_structure_ptr:
 *			This should point at the data structure for page management.
 *      	For example: head of a linked-list or root of a tree.
 *		void *start:
 *			This should be the first page's address.
 *		void *end:
 *			This should be the last page's address.
 *	function:
 *		As initialization, put all free pages into your own data structure.
 */
void *(*page_alloc)(void *datastructure_ptr);
/* 	parameters:
 *		void *data_structure_ptr:
 *			This should point at the data structure for page management.
 *      	For example: head of a linked-list or root of a tree.
 *	function:
 *		Take one page out of your data structure and return its address.
 */
NORETURN void (*page_free)(void *datastructure_ptr, void *page_address);
/* 	parameters:
 *		void *data_structure_ptr:
 *			This should point at the data structure for page management.
 *      	For example: head of a linked-list or root of a tree.
 *		void *page_address:
 *			The address of the page that needs to be freed.
 *	function:
 *		Save the page into your data structure.
 */
```

敏锐的同学会发现，这个表里本质只有一些函数指针，指针指向的函数还需自己定义。

在具体的实现部分，我们推荐大家维护一个链表来存储空闲页，链表中每个节点的结构如下：

```C
typedef struct {
    void *next;
} FreeListNode;
```

**思考题：页大小为 4KB ，但为什么这样就足够了？**

在本次实验中，助教已经把指针指向了对应的函数，因此同学们只需要填充以下几个函数的内容即可。

```c
NORETURN static void freelist_init(void *freelist_ptr, void *start, void *end);
static void *freelist_alloc(void *freelist_ptr);
NORETURN static void freelist_free(void *freelist_ptr, void *page_address);
```



### 页表管理

虚拟内存与页表的管理（页的粒度、虚拟地址的大小等）与指令集架构密切相关，实验使用的 ARM 架构提供了三种页的粒度：64 KB(16 bits)，16 KB(14 bits)，4 KB(12 bits)，本学期的内核实验我们将同一且仅采用 4 KB 的页大小，在 48-bit 索引的虚拟内存中虚拟地址通过页表转换为物理地址的过程如下图。

![](.\pagetable.jpg)


当 CPU 拿到 64-bit 的虚拟地址时：

如果前 16 位全 0，CPU 从 ttbr0_el1 读出 level-0 页表。

如果前 16 位全 1，CPU 从 ttbr1_el1 开始 level-0 页表。

否则，报错。

拿到 level-0 页表后，CPU 会依次以 va[47:39] , va[38:39] , va[29:21] , va[20:12] 为索引来获取下一级页表信息（页表地址、访问权限）。

