# Lab9 All in One Shell

本次实验的目标是运行一个用户态 Shell。



首先，添加 lab9 代码：

```bash
git fetch --all
git checkout lab9
git merge lab8 # Or lab8-dev, or else
# Handle Merge Conflicts
```

在 lab9 中，我们需要用到 musl-libc，它作为 git submodule 加入到代码仓库里。运行以下命令来编译 musl-libc：

```bash
cd build
cmake ..
make init_libc
make libc
```

该命令只需要执行一次。

## 1 File System

本次实验中，我们将实现 file 层，并补充 inode 层的功能。

### 1.1 File Descriptor Layer

#### 1.1.1 作用

> A cool aspect of the Unix interface is that most resources in Unix are represented
> as files, including devices such as the console, pipes, and of course, real files. The file
> descriptor layer is the layer that achieves this uniformity.

文件描述符层统一了这些资源（真实文件、console、pipe）的使用接口。这一层主要做两件事：

* 维护 per-process 的 file 使用情况
* 调用 inode 层的接口

File Structure:

```c
typedef struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE } type;
    int ref;
    char readable;
    char writable;
    struct pipe *pipe; // Not used
    Inode *ip;
    
    /*
    	// Here we show the function of item `off`
    	
    	// in user code
    	u8 buf[BUF_SIZE];
    	read(fd=1, buf=buf, size=BUF_SIZE);
    	// handle the first BUF_SIZE bytes
    	
    	read(fd=1, buf=buf, size=BUF_SIZE);
    	// handle the second BUF_SIZE bytes
    */
    usize off;
} File;
```

全局 File Table:

```c
struct {
    struct SpinLock lock;
    struct file file[NFILE];
} ftable;
```

#### 1.1.2 接口

实现 `file.c` 里的函数。

* `filealloc`: similar to `pcb_alloc` ，需要把 ref 置 1
* `filedup`: ref++
* `fileclose`: ref--, might call `inodes.put`
* `filestat`: call `stati`
* `fileread`: call `inodes.read` and maintain offset
* `filewrite`: call `inodes.write` and maintain offset

### 1.2 Inode Layer

补充 `inode.c` 里的接口：

* `namex()`，解析文件路径
  * 输入 path 为形如 `/foo///./bar//` 或 `foo//bar/bar1`，需要解析每一层目录并调用 `inodes.lookup` 。对于后者，应当调用`lookup(inode(foo), "bar") ->lookup(inode(foo/bar), "bar1") `。
  * `nameiparent` 用于创建新文件时。

* `stati`，获取 inode 信息

**注意**：inode 接口有改变：

* read/write 返回实际读/写的字节数。
* read 在 `1` 时，读取 `off`。

## 2 Process

实现 `proc.c` 中的 `fork, wait, growproc`，补全 `exit`。

实现 `exec.c` 中的 `execve`:

* Read ELF header and initialize memory region
* Initialize user stack
* Set trap frame

## 3 Virtual Memory

实现 `virtual_memory.c` 中的函数。

* `uvm_alloc`
* `uvm_dealloc`
* `copyout`

## 4 System Call

实现 `sysproc.c` 和 `sysfile.c` 中的函数。这些函数调用进程与文件系统的接口。

## 5 User space Code 

`src/user` 中的代码是用户态代码。

每个目录对应一个二进制。

这里面的代码可以使用 libc ，编译时将和 libc 进行链接。

添加新二进制的步骤：

* 在 `src/user` 里新建目录，以二进制的名称命名
* 在该目录下新建 `main.c` ，在里面编写这个二进制所有源代码，需要有 `int main(int argc, char *argv[]) ` 函数
* 在 `src/user/CMakeLists.txt` 里的 `bin_list` 添加这个二进制的名字，加进编译列表
* 在 `boot/CMakeLists.txt` 里的 `user_files` 添加这个二进制的名字，加进编译列表

## 6 Debug

运行 `make qemu`，目标是进入 shell。程序执行流大致是：

* `spawn_init_process -> schedule -> forkret -> trap_return -> user/init.S`
* `user/init.S: execve("/init")`
* `sys_exec -> execve -> trap_return`
* `start code of binary init (libc_start_main, ...)`
* `main() of binary init`
* `fork() -> exec("/sh")`

进入 `sh` 后，执行 `ls` 来查看可以执行的二进制。

通过已有的二进制，你可以进行以下实验：

* `echo something > a.txt` ：新建一个文件 `a.txt` ，将一段内容写入该文件
* `cat a.txt` ：输出 `a.txt` 的内容，检查文件是否写入正确



欢迎大家把遇到的棘手的问题在 slack 里分享。

