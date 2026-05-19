# xv6-labs-2025 实验报告

> 仓库：`JasonHong314/xv6-labs-2025`  
> 覆盖分支：`util`、`syscall`、`pgtbl`、`traps`、`cow`、`lock`、`fs`、`net`  


## 0. 仓库组织方式

| 分支 | 对应实验 | 本报告重点 |
|---|---|---|
| `util` | 用户态工具实验 | `sleep`、`sixfive`、`memdump`、`find` |
| `syscall` | 系统调用实验 | `pause/sleep`、`interpose`、`attack/secret` |
| `pgtbl` | 页表实验 | `USYSCALL`、`vmprint`、`superpage` |
| `traps` | Trap 实验 | `sigalarm`、`sigreturn`、trapframe 保存恢复 |
| `cow` | Copy-on-Write 实验 | `fork` 懒拷贝、页引用计数、写时复制、`copyout` 处理 |
| `lock` | 锁实验 | per-CPU freelist、读写自旋锁、锁统计 |
| `fs` | 文件系统实验 | 大文件双重间接索引、符号链接 |
| `net` | 网卡实验 | E1000 发送、接收、中断处理 |



# 1. util 

## 1.1 `sleep`

### 题目

实现一个用户态命令，使用户可以执行：

```sh
sleep 10
```

让当前进程睡眠指定 tick 数。由于本仓库中系统调用命名为 `pause`，所以用户态 `sleep.c` 实际调用 `pause(ticks)`。

### 涉及文件

```text
user/sleep.c
Makefile
user/user.h
user/usys.pl
kernel/sysproc.c
```

### 核心代码

```c
#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"

int main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "Usage: sleep ticks\n");
    exit(1);
  }
  int ticks = atoi(argv[1]);
  pause(ticks);
  exit(0);
}
```

### 代码解析

#### 1. 参数数量检查

```c
if(argc != 2){
  fprintf(2, "Usage: sleep ticks\n");
  exit(1);
}
```

用户态程序约定 `argv[0]` 是程序名，`argv[1]` 是 tick 数。如果参数不是 2 个，说明用户没有传入 tick 数，直接输出错误提示并退出。


#### 2. 字符串转整数

```c
int ticks = atoi(argv[1]);
```


#### 3. 调用系统调用

```c
pause(ticks);
```


### Makefile 

`Makefile` 中将该程序加入 `UPROGS`：

```makefile
UPROGS=\
    ...
    $U/_sleep\
    $U/_sixfive\
    $U/_find\
```

xv6 编译用户程序时，会把 `user/sleep.c` 编译成 `_sleep`，并打包进文件系统镜像。否则即使代码写好了，进入 xv6 shell 后也找不到命令。



## 1.2 `sixfive`

### 题目

实现一个用户态程序，读取文件内容，识别其中的整数，并输出能被 5 或 6 整除的数。



### 涉及文件

```text
user/sixfive.c
Makefile
```

### 核心代码

```c
#define DELIMS "-\r\t\n./,"

void process_file(int fd) {
    char buf[1];
    char num[32];
    int n = 0;

    while (read(fd, buf, 1) == 1) {
        char c = buf[0];
        if (strchr(DELIMS, c)) {
            if (n > 0) {
                num[n] = '\0';
                int val = atoi(num);
                if (val % 5 == 0 || val % 6 == 0)
                    printf("%d\n", val);
                n = 0;
            }
        } else if (c >= '0' && c <= '9') {
            if (n < 31) num[n++] = c;
        } else {
            n = 0;
        }
    }

    if (n > 0) {
        num[n] = '\0';
        int val = atoi(num);
        if (val % 5 == 0 || val % 6 == 0)
            printf("%d\n", val);
    }
}
```

### 代码解析

#### 1. 分隔符定义

```c
#define DELIMS "-\r\t\n./,"
```

程序把 `-`、换行、制表符、点、斜杠、逗号等字符看作数字之间的分隔符。读到分隔符时，如果当前已经收集到数字字符，就把它作为一个完整整数处理。

#### 2. 逐字节读取文件

```c
char buf[1];

while (read(fd, buf, 1) == 1) {
    char c = buf[0];
    ...
}
```

xv6 用户态库比较简陋，没有复杂的标准 C 文件流接口，所以这里使用系统调用 `read(fd, buf, 1)` 每次读取一个字节。

逐字节读取的优点是解析逻辑简单：每次只判断当前字符是数字、分隔符还是非法字符。

#### 3. 数字缓冲区

```c
char num[32];
int n = 0;
```

`num` 用来保存当前正在解析的数字字符串。`n` 表示已经读入多少位数字。

```c
else if (c >= '0' && c <= '9') {
    if (n < 31) num[n++] = c;
}
```

这段逻辑只接受数字字符，并且最多保留 31 个字符，最后一位留给 `'\0'`。这避免了字符串没有结束符的问题。

#### 4. 遇到分隔符时处理一个数字

```c
if (strchr(DELIMS, c)) {
    if (n > 0) {
        num[n] = '\0';
        int val = atoi(num);
        if (val % 5 == 0 || val % 6 == 0)
            printf("%d\n", val);
        n = 0;
    }
}
```

这里的核心逻辑是：

1. 如果 `n > 0`，说明之前已经读到若干数字；
2. 给字符串补上 `'\0'`；
3. 用 `atoi` 转成整数；
4. 判断 `val % 5 == 0 || val % 6 == 0`；
5. 输出符合条件的数字；
6. `n = 0`，准备解析下一个数字。

#### 5. 文件结尾时还要处理残留数字

```c
if (n > 0) {
    num[n] = '\0';
    int val = atoi(num);
    if (val % 5 == 0 || val % 6 == 0)
        printf("%d\n", val);
}
```

这是一个重要边界情况。如果文件最后一个数字后面没有分隔符，循环中不会触发处理逻辑，所以文件读完后必须再处理一次缓冲区。

### 主函数代码

```c
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "usage: sixfive file...\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(2, "sixfive: cannot open %s\n", argv[i]);
            continue;
        }
        process_file(fd);
        close(fd);
    }
    exit(0);
}
```

### 主函数解析

程序支持多个文件：

```sh
sixfive a.txt b.txt c.txt
```

每个文件依次打开、处理、关闭。

```c
if (fd < 0) {
    fprintf(2, "sixfive: cannot open %s\n", argv[i]);
    continue;
}
```

如果某个文件打不开，不直接退出，而是继续处理后续文件。这种设计比直接 `exit(1)` 更友好。


---

## 1.3 `memdump` 

### 题目

`memdump` 根据格式字符串解释一段内存。它类似一个简化版的内存查看器，支持把同一段字节解释成整数、指针、短整数、字符、字符串等。

### 涉及文件

```text
user/memdump.c
```

### 格式字符含义

| 格式字符 | 含义 | 消耗字节数 |
|---|---|---|
| `i` | int | 4 |
| `p` | 64 位地址/指针值 | 8 |
| `h` | short | 2 |
| `c` | char | 1 |
| `s` | char* 指针指向的字符串 | 8 |
| `S` | 从当前位置开始的字符串 | 直到 `\0` |

### 核心代码

```c
void
memdump(char *fmt, char *data)
{
  for(int i = 0; fmt[i] != '\0'; i++){
    switch(fmt[i]){

      case 'i':
        printf("%d\n", *(int*)data);
        data += 4; 
        break;

      case 'p':
        printf("%lx\n", *(uint64*)data);
        data += 8;
        break;

      case 'h':
        printf("%d\n", *(short*)data);
        data += 2;
        break;

      case 'c':
        printf("%c\n", *(char*)data);
        data += 1;
        break;

      case 's':
        printf("%s\n", *(char**)data);
        data += 8;
        break;

      case 'S':
        printf("%s\n", data);
        while(*data != '\0') data++;
        data++;
        break;
    }
  }
}
```

### 代码解析

#### 1. 格式字符串驱动解析

```c
for(int i = 0; fmt[i] != '\0'; i++){
    switch(fmt[i]){
        ...
    }
}
```

程序逐个读取格式字符，每个格式字符决定“当前 data 指针处的若干字节应该怎么解释”。

#### 2. `i`：按 int 解释

```c
printf("%d\n", *(int*)data);
data += 4;
```

`data` 是 `char*`，表示字节地址。  
`(int*)data` 把当前地址强制转换为 `int*`，`*(int*)data` 读取 4 字节作为整数。

然后 `data += 4`，移动到下一段数据。

#### 3. `p`：按 64 位数解释

```c
printf("%lx\n", *(uint64*)data);
data += 8;
```

xv6 是 64 位 RISC-V，因此地址宽度是 8 字节。  
这里用 `%lx` 输出十六进制长整数，适合显示指针值。

#### 4. `h`：按 short 解释

```c
printf("%d\n", *(short*)data);
data += 2;
```

读取 2 字节。注意 `printf("%d")` 会把 short 提升成 int 输出。

#### 5. `c`：按字符解释

```c
printf("%c\n", *(char*)data);
data += 1;
```

只读取 1 字节。

#### 6. `s` 与 `S` 的区别

```c
case 's':
  printf("%s\n", *(char**)data);
  data += 8;
  break;
```

`s` 表示当前 data 中存放的是一个指针。  
所以先把 `data` 当成 `char**`，取出里面保存的地址，再把该地址当字符串输出。

```c
case 'S':
  printf("%s\n", data);
  while(*data != '\0') data++;
  data++;
  break;
```

`S` 表示当前 data 本身就是字符串首地址，不需要再解引用一次。

### 示例结构体

```c
struct sss {
  char *ptr;
  int num1;
  short num2;
  char byte;
  char bytes[8];
} example;
```

这个结构体用于测试不同类型在内存中的排列。  
例如格式串 `"pihcS"` 表示：

1. `p`：读 `ptr` 指针值；
2. `i`：读 `num1`；
3. `h`：读 `num2`；
4. `c`：读 `byte`；
5. `S`：读 `bytes` 字符数组。



## 1.4 `find` 与 `-exec`

### 题目

实现一个类似 Unix `find` 的用户态程序：

```sh
find <dir> <name>
find <dir> <name> -exec cmd ...
```

功能包括：

1. 从指定目录递归查找文件；
2. 如果文件名匹配目标名，则输出路径；
3. 如果带 `-exec`，则对匹配到的文件执行指定命令。

### 涉及文件

```text
user/find.c
```

### 核心代码一：执行命令

```c
void
run_exec(char *file, char **cmd, int cmdargc)
{
  int pid;
  char *exec_argv[MAXARG];

  if(cmdargc + 2 > MAXARG){
    fprintf(2, "find: too many arguments for exec\n");
    return;
  }

  for(int i = 0; i < cmdargc; i++){
    exec_argv[i] = cmd[i];
  }
  exec_argv[cmdargc] = file;
  exec_argv[cmdargc + 1] = 0;

  pid = fork();
  if(pid < 0){
    fprintf(2, "find: fork failed\n");
    return;
  }

  if(pid == 0){
    exec(exec_argv[0], exec_argv);
    fprintf(2, "find: exec %s failed\n", exec_argv[0]);
    exit(1);
  } else {
    wait(0);
  }
}
```

### 代码解析

#### 1. 构造 `exec` 参数

```c
for(int i = 0; i < cmdargc; i++){
  exec_argv[i] = cmd[i];
}
exec_argv[cmdargc] = file;
exec_argv[cmdargc + 1] = 0;
```

例如：

```sh
find . a.txt -exec cat
```

则构造出的参数是：

```c
exec_argv[0] = "cat";
exec_argv[1] = "./a.txt";
exec_argv[2] = 0;
```

`exec` 要求参数数组必须以 `0` 结尾，否则内核不知道参数列表在哪里结束。

#### 2. fork + exec 模式

```c
pid = fork();

if(pid == 0){
  exec(exec_argv[0], exec_argv);
  ...
} else {
  wait(0);
}
```

父进程负责继续查找，子进程负责执行命令。  
这里父进程 `wait(0)`，是为了避免产生僵尸进程，也保证执行结果比较有序。

### 核心代码二：递归查找

```c
void
find(char *path, char *target, int exec_mode, char **cmd, int cmdargc)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  char name[DIRSIZ + 1];

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if(st.type == T_FILE){
    ...
  }

  if(st.type != T_DIR){
    close(fd);
    return;
  }

  ...
}
```

### 代码解析

#### 1. 打开路径并读取元数据

```c
fd = open(path, O_RDONLY);
fstat(fd, &st);
```

`open` 只是得到文件描述符，`fstat` 才能判断它是普通文件、目录还是设备。

#### 2. 如果当前路径是普通文件

```c
if(st.type == T_FILE){
  char *q = path + strlen(path);
  while(q >= path && *q != '/')
    q--;
  q++;

  if(strcmp(q, target) == 0){
    if(exec_mode)
      run_exec(path, cmd, cmdargc);
    else
      printf("%s\n", path);
  }

  close(fd);
  return;
}
```

这段代码从完整路径中提取文件名：

```text
./dir/a.txt
      ↑ q 最后指向 a.txt
```

如果文件名等于目标名，则输出或执行命令。

#### 3. 如果是目录，则递归遍历

```c
strcpy(buf, path);
p = buf + strlen(buf);
*p++ = '/';

while(read(fd, &de, sizeof(de)) == sizeof(de)){
  if(de.inum == 0)
    continue;

  if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
    continue;

  memmove(name, de.name, DIRSIZ);
  name[DIRSIZ] = 0;

  memmove(p, de.name, DIRSIZ);
  p[DIRSIZ] = 0;

  if(stat(buf, &st) < 0)
    continue;

  if(strcmp(name, target) == 0){
    if(exec_mode)
      run_exec(buf, cmd, cmdargc);
    else
      printf("%s\n", buf);
  }

  if(st.type == T_DIR)
    find(buf, target, exec_mode, cmd, cmdargc);
}
```

目录文件中保存的是若干 `dirent`。  
每个 `dirent` 包含：

```c
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};
```

#### 4. 跳过 `.` 和 `..`

```c
if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
  continue;
```

如果不跳过：

- `.` 会递归进入当前目录；
- `..` 会递归进入父目录；

最终会无限递归。

#### 5. 路径长度检查

```c
if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
  fprintf(2, "find: path too long\n");
  close(fd);
  return;
}
```

xv6 用户态没有动态字符串库，所以使用固定数组 `buf[512]`。  
这里必须检查路径是否会溢出。

### 主函数

```c
int
main(int argc, char *argv[])
{
  if(argc == 3){
    find(argv[1], argv[2], 0, 0, 0);
  } else if(argc >= 5 && strcmp(argv[3], "-exec") == 0){
    find(argv[1], argv[2], 1, &argv[4], argc - 4);
  } else {
    fprintf(2, "usage: find <dir> <name> [-exec cmd ...]\n");
    exit(1);
  }

  exit(0);
}
```


--
# 2. syscall 实验：系统调用机制与 Attack xv6

## 2.1 实验目标

本实验对应 6.1810 / 6.828 2025 的 syscall lab，主要目的是理解 xv6 中用户程序如何通过系统调用进入内核，并进一步完成系统调用拦截和攻击实验。本仓库 `syscall` 分支主要完成了以下内容：

1. 理解系统调用从用户态进入内核的完整路径；
2. 实现 `interpose(mask, path)` 系统调用拦截机制；
3. 完成 `attack` 程序，利用未清零物理页读取 `secret` 程序遗留的数据。

涉及的主要文件包括：

```text
kernel/syscall.h
kernel/syscall.c
kernel/sysproc.c
kernel/proc.h
kernel/proc.c
kernel/kalloc.c
user/usys.pl
user/user.h
user/attack.c
user/secret.c
```

---

## 2.2 系统调用进入内核的过程

xv6 中用户程序不能直接调用内核函数，而是通过 `ecall` 指令进入内核。用户态系统调用入口由 `user/usys.pl` 自动生成，其核心代码如下：

```perl
sub entry {
    my $name = shift;
    print ".global $name\n";
    print "$name:\n";
    print " li a7, SYS_${name}\n";
    print " ecall\n";
    print " ret\n";
}
```

这段代码说明，用户调用某个系统调用时，首先会把系统调用号放入 RISC-V 的 `a7` 寄存器，然后执行 `ecall` 进入内核。例如 `interpose` 对应的系统调用号在 `kernel/syscall.h` 中定义为：

```c
#define SYS_interpose  22
```

进入内核后，系统调用由 `kernel/syscall.c` 中的 `syscall()` 统一分发：

```c
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    ...
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

其中：

```c
num = p->trapframe->a7;
```

表示内核从当前进程的 trapframe 中取出系统调用号。用户态执行 `ecall` 时，寄存器状态会被保存到 trapframe 中，因此内核可以通过 `a7` 判断用户请求的是哪个系统调用。

真正执行系统调用的是：

```c
p->trapframe->a0 = syscalls[num]();
```

`syscalls` 是系统调用函数表，`num` 是下标。函数返回值被写入 `trapframe->a0`，因为 RISC-V 中函数返回值通过 `a0` 返回给用户态。

系统调用完整流程可以概括为：

```text
用户态函数调用
    ↓
usys.S 将系统调用号放入 a7
    ↓
执行 ecall 进入内核
    ↓
syscall() 从 trapframe->a7 读取系统调用号
    ↓
通过 syscalls[] 调用 sys_xxx()
    ↓
返回值写入 trapframe->a0
    ↓
返回用户态
```

---

## 2.3 系统调用拦截：interpose

本仓库实现了 `interpose(mask, path)` 系统调用，用于限制当前进程后续可以执行的系统调用。其思想是在每个进程中保存一个系统调用屏蔽位图，如果某个系统调用号对应的 bit 被置 1，则该系统调用会被拦截。

首先，在 `kernel/proc.h` 的 `struct proc` 中加入两个字段：

```c
struct proc {
  struct spinlock lock;

  uint64 syscall_mask;
  char allowed_path[MAXPATH];

  ...
};
```

其中，`syscall_mask` 用于记录哪些系统调用需要被拦截，`allowed_path` 用于保存允许访问的路径。比如，如果要拦截 `open`，可以设置：

```c
mask = 1ULL << SYS_open;
```

`sys_interpose()` 的实现位于 `kernel/sysproc.c`：

```c
uint64
sys_interpose(void)
{
  int mask;
  struct proc *p = myproc();

  argint(0, &mask);
  p->syscall_mask = mask;
  argstr(1, p->allowed_path, MAXPATH);
  return 0;
}
```

这里通过 `argint(0, &mask)` 读取用户传入的第一个参数，即系统调用屏蔽位图；通过 `argstr(1, p->allowed_path, MAXPATH)` 读取第二个参数，即允许访问的路径字符串。由于用户字符串位于用户地址空间，不能直接保存用户指针，必须复制到内核的 `allowed_path` 数组中。

拦截逻辑写在统一的 `syscall()` 分发入口中：

```c
if(p->syscall_mask & (1ULL << num)) {
  if(num == SYS_open || num == SYS_exec) {
    char path[MAXPATH];
    argstr(0, path, MAXPATH);
    if(strcmp(path, p->allowed_path) != 0) {
      p->trapframe->a0 = -1;
      return;
    }
  } else {
    p->trapframe->a0 = -1;
    return;
  }
}
```

这段代码的含义是：如果当前系统调用号 `num` 命中了 `syscall_mask`，则需要拦截。对于普通系统调用，直接返回 `-1`；对于 `open` 和 `exec`，由于它们第一个参数都是路径，所以可以用：

```c
argstr(0, path, MAXPATH);
```

取出用户传入的路径，并与 `p->allowed_path` 比较。如果路径不同，就拒绝执行；如果路径相同，则继续执行真正的系统调用。

为了防止进程通过 `fork` 绕过限制，本仓库还在 `kernel/proc.c` 的 `kfork()` 中复制了拦截状态：

```c
np->syscall_mask = p->syscall_mask;
safestrcpy(np->allowed_path, p->allowed_path, MAXPATH);
```

这样子进程会继承父进程的系统调用限制，保证拦截规则不会因为创建子进程而失效。

---

## 2.4 Attack xv6：利用未清零页泄露 secret

Attack xv6 的目的是说明：如果操作系统在重新分配物理页前不清除旧内容，就可能导致一个进程读到另一个进程留下的敏感数据。

`user/secret.c` 中定义了一个全局数组：

```c
#define DATASIZE (8*4096)

char data[DATASIZE];
```

然后将提示字符串和 secret 写入数组：

```c
strcpy(data, "This may help.");
strcpy(data + 16, argv[1]);
```

也就是说，`secret` 程序会把真正的秘密字符串写入 `data + 16` 的位置。当该进程退出后，它占用的物理页会被释放回内核空闲链表。

关键问题在 `kernel/kalloc.c`。正常 xv6 会在释放或分配页面时填充垃圾值：

```c
memset(pa, 1, PGSIZE);
```

或者：

```c
memset((char*)r, 5, PGSIZE);
```

但在 syscall lab 中，这些代码被条件编译包围：

```c
#ifndef LAB_SYSCALL
  memset(pa, 1, PGSIZE);
#endif
```

```c
#ifndef LAB_SYSCALL
  if(r)
    memset((char*)r, 5, PGSIZE);
#endif
```

当编译 syscall lab 时，`LAB_SYSCALL` 被定义，因此这些 `memset` 不会执行。结果就是：`secret` 进程退出后，物理页虽然被释放，但其中的旧数据仍然可能保留。

`user/attack.c` 利用这一点申请内存并读取残留数据：

```c
int
main(int argc, char *argv[])
{
  char *base = sbrk(32 * PGSIZE);
  base += 17 * PGSIZE +32;
  write(2, base, 8);
  exit(0);
}
```

首先：

```c
char *base = sbrk(32 * PGSIZE);
```

攻击程序申请 32 个页面，增加重新获得 secret 程序释放页面的概率。

然后：

```c
base += 17 * PGSIZE +32;
```

根据测试环境中较稳定的物理页复用顺序，定位到可能保存 secret 的位置。

最后：

```c
write(2, base, 8);
```

直接向标准错误输出 8 个字节。这里使用 `write` 而不是 `printf`，是因为 `write` 不要求字符串以 `\0` 结尾，更适合输出固定长度的 secret。

---

# 3. pgtbl 页表机制


## 3.1 `Print a page table` 

### 题目

实现页表递归打印函数，帮助观察 RISC-V Sv39 三级页表结构。

### 涉及文件

```text
kernel/vm.c
kernel/defs.h
```

### 核心代码

```c
void
vmprint_dfs(pagetable_t pagetable, int level, uint64 parent_va) {
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if (pte & PTE_V) {
      for (int j = 0; j <= 2 - level; j++) {
        printf(" ..");
      }

      uint64 va = parent_va + (i << PXSHIFT(level));
      pte_t pa = PTE2PA(pte);
      printf("%p: pte %p pa %p\n", (void *)va, (void *)pte, (void *)pa);

      if (!PTE_LEAF(pte)) {
        vmprint_dfs((pagetable_t)pa, level - 1, va);
      }
    }
  }
}

void
vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  vmprint_dfs(pagetable, 2, 0);
}
```

### 代码解析

#### 1. Sv39 页表每级 512 项

```c
for (int i = 0; i < 512; i++)
```

Sv39 中每级页表使用 9 位索引，2^9 = 512，所以每个页表页有 512 个 PTE。

#### 2. 只打印有效 PTE

```c
if (pte & PTE_V) {
  ...
}
```

无效 PTE 不参与地址转换，也不需要打印。

#### 3. 根据层级打印缩进

```c
for (int j = 0; j <= 2 - level; j++) {
  printf(" ..");
}
```

level 从 2 到 0：

- level 2：顶层页表；
- level 1：中间页表；
- level 0：叶子页表。

缩进越深，说明页表层级越低。

#### 4. 计算当前 PTE 对应的虚拟地址前缀

```c
uint64 va = parent_va + (i << PXSHIFT(level));
```

`PXSHIFT(level)` 表示当前层索引在虚拟地址中的偏移量：

```c
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
```

- level 2 对应 bit 30 起；
- level 1 对应 bit 21 起；
- level 0 对应 bit 12 起。

因此 `i << PXSHIFT(level)` 可以还原当前 PTE 覆盖的虚拟地址范围前缀。

#### 5. 区分叶子 PTE 和中间页表 PTE

```c
if (!PTE_LEAF(pte)) {
  vmprint_dfs((pagetable_t)pa, level - 1, va);
}
```

叶子 PTE 直接指向物理页。  
非叶子 PTE 指向下一级页表，所以递归打印。

`PTE_LEAF` 的定义：

```c
#define PTE_LEAF(pte) (((pte) & PTE_R) | ((pte) & PTE_W) | ((pte) & PTE_X))
```

如果 PTE 有 R/W/X 任一权限，就认为它是叶子映射。


---

## 3.3 superpage 超级页

### 题目

普通 xv6 使用 4KB 页。superpage 要求支持 2MB 大页，从而减少页表项数量和 TLB 压力。

本仓库实现了：

1. 单独的 superpage 物理页分配器；
2. `PTE_S` 标记超级页；
3. `mapsuperpages()` 映射 2MB 页；
4. `walkaddr()` 处理 2MB 页偏移；
5. `uvmunmap()` 支持部分释放超级页时拆分为普通页。

### 涉及文件

```text
kernel/riscv.h
kernel/kalloc.c
kernel/vm.c
```

### 宏定义

```c
#define SUPERPGSIZE (2 * (1 << 20)) // bytes per page
#define SUPERPGROUNDUP(sz)  (((sz)+SUPERPGSIZE-1) & ~(SUPERPGSIZE-1))
#define SUPERPGROUNDDOWN(sz) (SUPERPGROUNDUP(sz)-SUPERPGSIZE)

#define PTE_S (1L << 8) // Super page PTE
```

### 代码解析

#### 1. superpage 大小

```c
#define SUPERPGSIZE (2 * (1 << 20))
```

`1 << 20` 是 1MB，所以 `2 * (1 << 20)` 是 2MB。

#### 2. 使用 PTE_S 标记超级页

```c
#define PTE_S (1L << 8)
```

RISC-V PTE 中有保留给软件使用的位，这里用 bit 8 标记这是一个 superpage 映射。

---

### 物理内存分配器：普通页和超级页分离

```c
#define SUPERPAGES_NUM 10
#define SUPERPAGES_START (PHYSTOP - SUPERPAGES_NUM * 2 * 1024 * 1024)

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem,kmem_super;
```

### 代码解析

#### 1. 预留高地址区域给 superpage

```c
#define SUPERPAGES_START (PHYSTOP - SUPERPAGES_NUM * 2 * 1024 * 1024)
```

这里预留物理内存末尾 10 个 2MB 大页，作为超级页池。

#### 2. 两套 freelist

```c
kmem
kmem_super
```

- `kmem`：普通 4KB 页；
- `kmem_super`：2MB 超级页。

这样可以保证 superpage 分配到的物理地址天然 2MB 对齐。

### 初始化

```c
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)(SUPERPAGES_START - 1));

  initlock(&kmem_super.lock, "kmem_super");
  freerange_super((void*)SUPERPAGES_START, (void*)PHYSTOP);
}
```

### 代码解析

普通页分配范围：

```c
freerange(end, (void*)(SUPERPAGES_START - 1));
```

超级页分配范围：

```c
freerange_super((void*)SUPERPAGES_START, (void*)PHYSTOP);
```

这避免普通页分配器把 superpage 区域切碎。

### 超级页释放和分配

```c
void
superfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % SUPERPGSIZE) != 0 || (uint64)pa < (uint64)SUPERPAGES_START || (uint64)pa >= PHYSTOP)
    panic("superfree");

  memset(pa, 1, SUPERPGSIZE);

  r = (struct run*)pa;

  acquire(&kmem_super.lock);
  r->next = kmem_super.freelist;
  kmem_super.freelist = r;
  release(&kmem_super.lock);
}
```

```c
void*
superalloc(void)
{
  struct run *r;

  acquire(&kmem_super.lock);
  r = kmem_super.freelist;

  if(r)
    kmem_super.freelist = r->next;
  release(&kmem_super.lock);

  if(r)
    memset((char*)r, 5, SUPERPGSIZE);
  return (void*)r;
}
```

### 代码解析

`superfree` 的检查非常关键：

```c
((uint64)pa % SUPERPGSIZE) != 0
```

确保释放的是 2MB 对齐地址。

```c
(uint64)pa < SUPERPAGES_START || (uint64)pa >= PHYSTOP
```

确保该地址确实来自 superpage 专用区域。

---

### 映射超级页

```c
int
mapsuperpages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if((va % SUPERPGSIZE) != 0)
    panic("mapsuperpages: va not aligned");

  if((size % SUPERPGSIZE) != 0)
    panic("mapsuperpages: size not aligned");

  if(size == 0)
    panic("mapsuperpages: size");

  a = va;
  last = va + size - SUPERPGSIZE;
  for(;;){
    if((pte = walk(pagetable, a, 2)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mapsuperpages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += SUPERPGSIZE;
    pa += SUPERPGSIZE;
  }
  return 0;
}
```

### 代码解析

#### 1. 地址必须 2MB 对齐

```c
if((va % SUPERPGSIZE) != 0)
  panic("mapsuperpages: va not aligned");
```

superpage 要求虚拟地址和物理地址都按 2MB 对齐。  
否则一个 PTE 无法正确覆盖完整 2MB 范围。

#### 2. 使用 `walk(pagetable, a, 2)`

```c
pte = walk(pagetable, a, 2)
```

普通 4KB 页映射使用 level 0 PTE。  
2MB superpage 使用 level 1 PTE。  
所以这里传入特殊的 `alloc == 2`，让 `walk` 返回 level 1 的 PTE。

#### 3. 写入 PTE

```c
*pte = PA2PTE(pa) | perm | PTE_V;
```

调用处会传入：

```c
PTE_S | PTE_R | PTE_U | xperm
```

因此 superpage PTE 同时包含：

- `PTE_V`：有效；
- `PTE_S`：超级页；
- `PTE_R/PTE_W/PTE_U`：正常访问权限。

---

### `walk()` 对超级页的支持

```c
if(*pte & PTE_V) {
#ifdef LAB_PGTBL
  if(PTE_LEAF(*pte))
    return pte;
#endif
  pagetable = (pagetable_t)PTE2PA(*pte);
}
...
if(alloc == 2 && level == 2)
  return &pagetable[PX(1, va)];
```

### 代码解析

#### 1. 遇到叶子 PTE 直接返回

```c
if(PTE_LEAF(*pte))
  return pte;
```

普通 `walk` 默认中间层 PTE 都指向下一级页表。  
但 superpage 的 level 1 PTE 本身就是叶子 PTE。  
所以如果遇到叶子项，不能继续当作页表地址向下走，否则会把物理数据页误当成页表。

#### 2. 返回 level 1 PTE

```c
if(alloc == 2 && level == 2)
  return &pagetable[PX(1, va)];
```

这段是为了支持 `mapsuperpages()`。  
当 `alloc == 2` 时，`walk` 不继续走到 level 0，而是在创建或找到 level 1 页表后，直接返回对应的 level 1 PTE。

---

### `walkaddr()` 对超级页偏移的支持

```c
pa = PTE2PA(*pte);
if(*pte & PTE_S)
  pa += va & (SUPERPGSIZE - 1);
else
  pa += va & (PGSIZE - 1);
return pa;
```

### 代码解析

普通 4KB 页的页内偏移是低 12 位：

```c
va & (PGSIZE - 1)
```

2MB superpage 的页内偏移是低 21 位：

```c
va & (SUPERPGSIZE - 1)
```

如果这里仍然只加 4KB 偏移，那么访问 superpage 中第二个 4KB 之后的内容都会映射错误。

---

### `uvmalloc()` 中优先使用超级页

```c
if (a + SUPERPGSIZE <= newsz && superpage_allocable()) {
  sz = SUPERPGSIZE;
  mem = superalloc(); 
  if (mem == 0) {
    uvmdealloc(pagetable, a, oldsz_bk);
    return 0;
  }
  memset(mem, 0, sz);
  if (mapsuperpages(pagetable, a, sz, (uint64)mem, PTE_S | PTE_R | PTE_U | xperm) != 0){
    superfree(mem);
    uvmdealloc(pagetable, a, oldsz_bk);
    return 0;
  }
} else {
  sz = PGSIZE;
  mem = kalloc();
  ...
}
```

### 代码解析

这段表示：

1. 如果当前虚拟地址到 `newsz` 之间至少还有 2MB；
2. 且 superpage freelist 不为空；
3. 就分配 superpage；
4. 否则退回普通 4KB 页。

### 为什么前面还要处理对齐

```c
uint64 s_oldsz = SUPERPGROUNDUP(oldsz);
for (a = oldsz; a < s_oldsz; a += PGSIZE) {
  ...
}
oldsz = s_oldsz;
```

如果进程原来的大小不是 2MB 对齐，那么不能直接从 `oldsz` 开始映射 superpage。  
必须先用普通页补齐到 2MB 边界，再开始使用 superpage。

---

### 部分释放超级页：拆分为普通页

```c
if(*pte & PTE_S){
  uint64 spa = PTE2PA(*pte);
  uint flags = PTE_FLAGS(*pte);

  if((a % SUPERPGSIZE) == 0 && (npages - i) >= (SUPERPGSIZE / PGSIZE)){
    if(do_free)
      superfree((void*)spa);
    *pte = 0;
    a += SUPERPGSIZE;
    i += SUPERPGSIZE / PGSIZE;
    continue;
  }

  uint64 baseva = SUPERPGROUNDDOWN(a);
  *pte = 0;

  for(uint64 off = 0; off < SUPERPGSIZE; off += PGSIZE){
    char *mem = kalloc();
    if(mem == 0)
      panic("uvmunmap: split superpage kalloc");

    memmove(mem, (char*)spa + off, PGSIZE);

    if(mappages(pagetable, baseva + off, PGSIZE, (uint64)mem,
                flags & ~PTE_S) != 0){
      kfree(mem);
      panic("uvmunmap: split superpage mappages");
    }
  }

  if(do_free)
    superfree((void*)spa);

  continue;
}
```

### 代码解析

这是 superpage 实验最容易被问的部分。

#### 情况一：完整释放 superpage

```c
if((a % SUPERPGSIZE) == 0 && (npages - i) >= (SUPERPGSIZE / PGSIZE)){
  if(do_free)
    superfree((void*)spa);
  *pte = 0;
  a += SUPERPGSIZE;
  i += SUPERPGSIZE / PGSIZE;
  continue;
}
```

如果释放范围正好覆盖整个 2MB 页，就可以直接释放 superpage。

#### 情况二：只释放 superpage 的一部分

如果只释放 2MB 中的一小段，不能直接 `superfree`，否则会把仍然有效的数据也释放掉。

所以做法是：

1. 删除原来的 superpage PTE；
2. 分配 512 个普通 4KB 页；
3. 把 superpage 中每个 4KB 的内容复制过去；
4. 用普通页重新映射；
5. 再释放原来的 superpage；
6. 循环重新处理当前要释放的那个普通页。

这就是代码中的：

```c
for(uint64 off = 0; off < SUPERPGSIZE; off += PGSIZE){
  char *mem = kalloc();
  memmove(mem, (char*)spa + off, PGSIZE);
  mappages(...);
}
```


---

# 4. Traps 

## 4.1 RISC-V assembly

本部分主要通过阅读用户程序的 RISC-V 反汇编代码，理解函数调用、寄存器使用和 trap 发生时的上下文保存机制。RISC-V 中常见寄存器作用如下：

```text
a0-a7：函数参数和返回值
ra：返回地址
sp：栈指针
fp/s0：帧指针
sepc：发生 trap 前的程序计数器
scause：trap 原因
stval：trap 附加信息，例如缺页地址
```

在 xv6 中，用户程序执行系统调用时，会通过用户态 stub 将系统调用号放入 `a7`，然后执行 `ecall` 进入内核。进入内核后，用户寄存器会被保存到当前进程的 `trapframe` 中，内核再根据 `scause` 判断 trap 类型。

`kernel/trap.c` 中的 `usertrap()` 是处理用户态 trap 的核心函数：

```c
if(r_scause() == 8){
  if(killed(p))
    kexit(-1);

  p->trapframe->epc += 4;

  intr_on();

  syscall();
} else if((which_dev = devintr()) != 0){
  // ok
} else {
  printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
  printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
  setkilled(p);
}
```

其中：

```c
r_scause() == 8
```

表示当前 trap 是用户态执行 `ecall` 产生的系统调用。系统调用处理前，代码会执行：

```c
p->trapframe->epc += 4;
```

这是因为 `epc` 保存的是发生 trap 时的指令地址，也就是 `ecall` 指令地址。RISC-V 一条普通指令长度为 4 字节，如果不加 4，系统调用返回用户态后会再次执行同一条 `ecall`，导致反复进入内核。因此必须让 `epc` 指向下一条用户指令。

这一部分实验的重点是理解：trap 不是普通函数调用，发生 trap 时 CPU 会切换到内核态，xv6 通过 `trapframe` 保存用户态寄存器，并在处理完成后恢复用户态继续执行。

---

## 4.2 Backtrace

Backtrace 要求实现内核调用栈回溯，用于打印当前函数调用链。它的基本原理是利用 RISC-V 的 frame pointer。每个函数调用时，栈帧中通常会保存返回地址和上一层栈帧指针，因此可以从当前 `fp` 一层层向上回溯。

RISC-V 栈帧结构可以简化理解为：

```text
fp - 8   保存返回地址 ra
fp - 16  保存上一层 frame pointer
```

因此 backtrace 的核心思路是：

```text
读取当前 fp
    ↓
通过 fp - 8 取出当前函数返回地址
    ↓
通过 fp - 16 取出上一层 fp
    ↓
重复该过程，直到离开当前内核栈页
```

代码如下：

```c
void
backtrace(void)
{
  uint64 fp = r_fp();
  uint64 stack_lower = PGROUNDDOWN(fp);
  uint64 stack_upper = stack_lower + PGSIZE;
  do {
    printf("%p\n", (uint64*)(*(uint64*)(fp - 8)));
    fp = *(uint64*)(fp - 16);
  } while (fp > stack_lower && fp < stack_upper);
}
```

其中，`r_fp()` 用于读取当前 frame pointer。循环条件必须限制在当前内核栈页范围内，否则可能继续读取无效地址，导致错误。


---

## 4.3 Alarm

Alarm 部分要求实现两个系统调用：

```c
sigalarm(interval, handler)
sigreturn()
```

`sigalarm(interval, handler)` 用于让当前进程每经过 `interval` 个 timer tick 后，跳转到用户态的 `handler` 函数执行；`sigreturn()` 用于在 handler 执行结束后恢复被中断前的用户寄存器状态，使原程序继续运行。

为了注册这两个系统调用，本仓库在 `kernel/syscall.h` 中加入了系统调用号：

```c
#define SYS_sigalarm  22
#define SYS_sigreturn 23
```

在 `kernel/syscall.c` 中加入了函数声明和系统调用表项：

```c
extern uint64 sys_sigalarm(void);
extern uint64 sys_sigreturn(void);
```

```c
[SYS_sigalarm]  sys_sigalarm,
[SYS_sigreturn] sys_sigreturn,
```

同时，在 `user/user.h` 和 `user/usys.pl` 中暴露用户态接口：

```c
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```

```perl
entry("sigalarm");
entry("sigreturn");
```

为了保存 alarm 状态，本仓库在 `kernel/proc.h` 的 `struct proc` 中加入了以下字段：

```c
// alarm
int invoking;
int ticks_passed;
int alarm_interval;
void (*alarm_handler)();
struct trapframe *trapframe_bk;
```

其中，`alarm_interval` 表示触发间隔，`alarm_handler` 保存用户态 handler 函数地址，`ticks_passed` 记录当前已经经过的 tick 数，`invoking` 用于标记当前是否正在执行 handler，防止 handler 重入，`trapframe_bk` 用于备份被中断前的用户寄存器状态。

在 `kernel/proc.c` 的 `allocproc()` 中，会为每个进程分配一个备份 trapframe，并初始化 alarm 状态：

```c
if((p->trapframe_bk = (struct trapframe *)kalloc()) == 0){
  freeproc(p);
  release(&p->lock);
  return 0;
}
p->invoking = 0;
p->ticks_passed = 0;
p->alarm_interval = 0;
p->alarm_handler = 0;
```

进程释放时，也需要在 `freeproc()` 中释放该备份页：

```c
if(p->trapframe_bk)
  kfree((void*)p->trapframe_bk);
p->trapframe_bk = 0;
```

`sys_sigalarm()` 的实现位于 `kernel/sysproc.c`：

```c
uint64
sys_sigalarm(void)
{
  struct proc *p = myproc();
  argint(0, &(p->alarm_interval));
  argaddr(1, (uint64*)&(p->alarm_handler));
  return 0;
}
```

这段代码只是保存用户传入的参数。第一个参数 `interval` 通过 `argint` 读取，第二个参数 handler 函数地址通过 `argaddr` 读取。调用 `sigalarm()` 后不会立即执行 handler，而是等待 timer interrupt 中触发。

真正的触发逻辑在 `kernel/trap.c` 的 `handle_time_passes()` 中：

```c
void
handle_time_passes(void)
{
  struct proc *p = myproc();

  if (p->alarm_interval == 0 && p->alarm_handler == 0) {
    return;
  }
  
  if (p->invoking) {
    return;
  }

  p->ticks_passed++;

  if (p->ticks_passed < p->alarm_interval) {
    return;
  }

  memmove(p->trapframe_bk, p->trapframe, PGSIZE);

  p->invoking = 1;

  p->trapframe->epc = (uint64)p->alarm_handler;

  return;
}
```

这段代码每次 timer interrupt 时检查当前进程是否需要触发 alarm。如果用户调用的是 `sigalarm(0, 0)`，则 `alarm_interval` 和 `alarm_handler` 都为 0，说明关闭 alarm，直接返回。若 `invoking` 为 1，说明当前进程已经在执行 handler，还没有调用 `sigreturn()`，此时不能再次进入 handler，否则会覆盖备份的 trapframe。

如果 alarm 已启用且没有重入，内核会递增 `ticks_passed`。当累计 tick 数达到 `alarm_interval` 后，先通过：

```c
memmove(p->trapframe_bk, p->trapframe, PGSIZE);
```

备份当前用户寄存器状态，然后设置：

```c
p->invoking = 1;
p->trapframe->epc = (uint64)p->alarm_handler;
```

这里的关键是修改 `trapframe->epc`。xv6 从 trap 返回用户态时，会从 `epc` 指定的位置继续执行。将 `epc` 改成 handler 地址后，进程返回用户态时就会开始执行 handler。

该逻辑在 `usertrap()` 的 timer interrupt 分支中调用：

```c
if(which_dev == 2){
  handle_time_passes();
  yield();
}
```

当 handler 执行完后，用户程序调用 `sigreturn()`。本仓库中的 `sys_sigreturn()` 实现如下：

```c
uint64
sys_sigreturn(void)
{ 
  struct proc *p = myproc();
  p->invoking = 0;
  p->ticks_passed = 0;
  memmove(p->trapframe, p->trapframe_bk, PGSIZE);  
  return p->trapframe->a0; 
}
```

它首先清除 `invoking`，表示 handler 已经结束，可以允许下一次 alarm 触发；然后将 `ticks_passed` 清零，开始下一轮计时；最后通过：

```c
memmove(p->trapframe, p->trapframe_bk, PGSIZE);
```

恢复 handler 执行前的用户寄存器状态，使用户程序继续从被中断的位置运行。

最后返回：

```c
return p->trapframe->a0;
```

是为了保留原来 `a0` 寄存器的值，避免 `sigreturn()` 本身破坏用户程序原来的返回值。

Alarm 的整体流程可以概括为：

```text
用户调用 sigalarm(interval, handler)
        ↓
内核保存 interval 和 handler 到 struct proc
        ↓
timer interrupt 进入 usertrap()
        ↓
handle_time_passes() 累计 ticks_passed
        ↓
达到 interval 后备份 trapframe
        ↓
修改 trapframe->epc 为 handler 地址
        ↓
返回用户态后执行 handler
        ↓
handler 调用 sigreturn()
        ↓
恢复 trapframe，清除 invoking，重置 ticks_passed
        ↓
继续执行原程序
```

本部分的关键点是：内核并不是直接调用用户态 handler，而是通过修改 `trapframe->epc` 改变返回用户态后的执行位置。同时必须备份并恢复完整 trapframe，并使用 `invoking` 防止 handler 重入。

# 5. cow 

## 5.1 为什么需要 COW

### 原始 xv6 fork 的问题

原始 `fork()` 会复制父进程的整个用户地址空间。  
如果父进程内存很大，fork 会非常慢，而且很多页子进程可能根本不会修改。

### COW 的基本思想

fork 时不立即复制物理页，而是：

1. 父子进程共享同一个物理页；
2. 把该页标记为只读；
3. 在 PTE 中设置 `PTE_COW`；
4. 当父或子尝试写该页时触发 page fault；
5. page fault handler 再真正复制一页。

---

## 5.2 定义 `PTE_COW`

### 核心代码

```c
//the 8th and 9th bits are reserved for supervisor software
#define PTE_COW (1L << 8) // copy-on-write
```

### 代码解析

RISC-V PTE 的某些位保留给操作系统软件使用，不参与硬件地址翻译。  
这里用 bit 8 表示该页是 COW 页。

注意：  
`PTE_COW` 不能使用 `PTE_R/PTE_W/PTE_X/PTE_U/PTE_V` 已有硬件位，否则会改变硬件权限语义。

---

## 5.3 物理页引用计数

### 核心代码

```c
#define PAGE_NUM ((PHYSTOP - KERNBASE) / PGSIZE)

struct {
  struct spinlock lock;
  int ref_cnt[PAGE_NUM];
} page_ref_cnt;
```

### 代码解析

COW 需要知道一个物理页被多少进程共享。

例如：

- 引用计数为 1：只有当前进程使用；
- 引用计数大于 1：父子或多个进程共享。

因此增加 `page_ref_cnt.ref_cnt[]` 数组，用物理页号作为索引。

### 页号计算

```c
static inline uint64
page_index(uint64 pa)
{
  return (pa - KERNBASE) / PGSIZE;
}
```

`pa` 是物理地址，减去 `KERNBASE` 后除以页大小，得到数组下标。

### 初始化引用计数

```c
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&page_ref_cnt.lock, "page_ref_cnt");

  for(int i = 0; i < PAGE_NUM; i++)
    page_ref_cnt.ref_cnt[i] = 0;

  freerange(end, (void*)PHYSTOP);
}
```

引用计数数组需要锁保护，因为多个 CPU 可能同时 fork、exit、page fault。

---

## 5.4 修改 `kalloc` 和 `kfree`

### `kalloc`

```c
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE);

    acquire(&page_ref_cnt.lock);
    page_ref_cnt.ref_cnt[page_index((uint64)r)] = 1;
    release(&page_ref_cnt.lock);
  }

  return (void*)r;
}
```

### 解析

新分配出的页引用计数设为 1：

```c
page_ref_cnt.ref_cnt[page_index((uint64)r)] = 1;
```

因为此时只有一个拥有者。

### `kfree`

```c
void
kfree(void *pa)
{
  struct run *r;
  uint64 idx;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  idx = page_index((uint64)pa);

  acquire(&page_ref_cnt.lock);
  if(page_ref_cnt.ref_cnt[idx] < 1)
    panic("kfree: ref_cnt");

  page_ref_cnt.ref_cnt[idx]--;

  if(page_ref_cnt.ref_cnt[idx] > 0){
    release(&page_ref_cnt.lock);
    return;
  }
  release(&page_ref_cnt.lock);

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
```

### 解析

#### 1. 先减少引用计数

```c
page_ref_cnt.ref_cnt[idx]--;
```

释放一个映射，不等于立即释放物理页。  
只有引用计数降到 0，才说明没有任何进程使用它。

#### 2. 引用计数仍大于 0 时不放回 freelist

```c
if(page_ref_cnt.ref_cnt[idx] > 0){
  release(&page_ref_cnt.lock);
  return;
}
```

如果父进程退出，但子进程还在用该页，物理页不能释放。

#### 3. 引用计数为 0 才真正释放

```c
r = (struct run*)pa;
acquire(&kmem.lock);
r->next = kmem.freelist;
kmem.freelist = r;
release(&kmem.lock);
```

这时才把页放回空闲链表。

---

## 5.5 fork 时不复制页面，而是共享页面

### 核心代码

```c
if (PTE_FLAGS(*pte) & PTE_W) {
  (*pte) |= PTE_COW;
  (*pte) &= (~PTE_W);
}

pa = PTE2PA(*pte);
flags = PTE_FLAGS(*pte);

mem = (char*)pa;

if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
  goto err;
}

k_page_ref_inc(pa);
```

### 代码解析

#### 1. 原来可写的页才改成 COW

```c
if (PTE_FLAGS(*pte) & PTE_W) {
  (*pte) |= PTE_COW;
  (*pte) &= (~PTE_W);
}
```

如果一个页本来就是只读代码段，就不应该标记为 COW。  
只有原来可写的数据页，才需要写时复制。

#### 2. 清除写权限

```c
(*pte) &= (~PTE_W);
```

父进程页表项也要清除写权限。  
否则父进程写共享页不会触发 page fault，就会直接修改子进程看到的数据。

#### 3. 子进程映射同一个物理页

```c
mem = (char*)pa;

mappages(new, i, PGSIZE, (uint64)mem, flags)
```

这里没有 `kalloc()`，而是直接把子进程虚拟地址映射到父进程同一个物理地址。

#### 4. 增加引用计数

```c
k_page_ref_inc(pa);
```

共享物理页多了一个使用者，引用计数加 1。

### 错误回滚

```c
err:
  for (int j = 0; j < i; j += PGSIZE) {
    pte = walk(old, j, 0);
    if (PTE_FLAGS(*pte) & PTE_COW) {
      (*pte) &= (~PTE_COW);
      (*pte) |= (PTE_W);
    }
  }

  uvmunmap(new, 0, i / PGSIZE, 0);
  return -1;
```

如果中途映射失败，需要恢复父进程页表，避免父进程留下错误的 COW 状态。

---

## 5.6 写 COW 页时处理 page fault

### trap 中识别 COW page fault

```c
else if(r_scause() == 13 || r_scause() == 15){
  uint64 va = PGROUNDDOWN(r_stval());
  pte_t *pte = 0;

  if(va >= p->sz || va < PGSIZE){
    setkilled(p);
  } else {
    pte = walk(p->pagetable, va, 0);

    if(pte == 0 || (*pte & PTE_V) == 0){
      if(lazy_alloc_page(p, va) < 0)
        setkilled(p);
    } else if((*pte & PTE_COW) != 0){
      if(k_try_cow(p->pagetable, va) < 0)
        setkilled(p);
    } else {
      setkilled(p);
    }
  }
}
```

### 代码解析

#### 1. scause 13 和 15

- `13`：load page fault；
- `15`：store page fault。

COW 主要由写触发，即 store page fault，但代码中也统一处理了 page fault 场景。

#### 2. 获取 fault 地址

```c
uint64 va = PGROUNDDOWN(r_stval());
```

`stval` 保存导致异常的虚拟地址。  
需要向下对齐到页边界。

#### 3. 地址合法性检查

```c
if(va >= p->sz || va < PGSIZE){
  setkilled(p);
}
```

不能允许访问超过进程大小的地址，也不能访问低地址保护页。

#### 4. 区分 lazy allocation 和 COW

```c
if(pte == 0 || (*pte & PTE_V) == 0){
  lazy_alloc_page(p, va)
} else if((*pte & PTE_COW) != 0){
  k_try_cow(p->pagetable, va)
}
```

如果页表项不存在，是 lazy allocation。  
如果页表项存在但标记 COW，则执行写时复制。

---

## 5.7 `k_try_cow`

### 核心代码

```c
int
k_try_cow(pagetable_t pt, uint64 va)
{
  pte_t *pte;
  uint64 pa;
  uint flags;
  char *new_page;

  va = PGROUNDDOWN(va);

  if (va >= MAXVA)
    return -1;

  pte = walk(pt, va, 0);
  if (pte == 0)
    return -1;

  if ((*pte & PTE_V) == 0)
    return -1;

  if ((*pte & PTE_U) == 0)
    return -1;

  if ((*pte & PTE_COW) == 0)
    return -1;

  pa = PTE2PA(*pte);

  if (k_page_ref(pa) == 1) {
    *pte = (*pte & ~PTE_COW) | PTE_W;
    sfence_vma();
    return 0;
  }

  new_page = kalloc();
  if (new_page == 0)
    return -1;

  memmove(new_page, (void *)pa, PGSIZE);

  flags = PTE_FLAGS(*pte);
  flags = (flags | PTE_W) & ~PTE_COW;

  *pte = PA2PTE((uint64)new_page) | flags;

  k_page_ref_dec(pa);

  sfence_vma();
  return 0;
}
```

### 代码解析

#### 1. 一系列合法性检查

```c
if (pte == 0) return -1;
if ((*pte & PTE_V) == 0) return -1;
if ((*pte & PTE_U) == 0) return -1;
if ((*pte & PTE_COW) == 0) return -1;
```

确保这确实是一个用户 COW 页。  
如果不是，就不能贸然复制，否则可能让非法写入变成合法。

#### 2. 引用计数为 1 的优化

```c
if (k_page_ref(pa) == 1) {
  *pte = (*pte & ~PTE_COW) | PTE_W;
  sfence_vma();
  return 0;
}
```

如果只有当前进程引用该页，就没有必要再复制一页。  
直接恢复写权限即可。

这是 COW 中很重要的优化：  
COW 页不一定每次写都要复制，只有共享时才需要复制。

#### 3. 多进程共享时复制

```c
new_page = kalloc();
memmove(new_page, (void *)pa, PGSIZE);
```

分配新页并复制旧页内容。

#### 4. 更新 PTE

```c
flags = PTE_FLAGS(*pte);
flags = (flags | PTE_W) & ~PTE_COW;

*pte = PA2PTE((uint64)new_page) | flags;
```

新页应该：

- 保留原来的用户权限；
- 增加 `PTE_W`；
- 清除 `PTE_COW`。

#### 5. 旧物理页引用计数减一

```c
k_page_ref_dec(pa);
```

当前进程不再使用旧页，因此旧页引用计数减少。

#### 6. 刷新 TLB

```c
sfence_vma();
```

修改页表项后，需要刷新 TLB，避免 CPU 继续使用旧的只读映射。

---

## 5.8 `copyout` 中也要处理 COW

### 核心代码

```c
pte = walk(pagetable, va0, 0);
if (pte != 0 && (*pte & PTE_COW)) {
  if (k_try_cow(pagetable, va0) == 0) {}
  else return -1;
}

if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 ||
   (*pte & PTE_W) == 0)
  return -1;
pa0 = PTE2PA(*pte);
```

### 为什么 `copyout` 也要处理？

`copyout` 是内核向用户地址写数据。  
例如：

```c
read(fd, user_buf, n)
```

内核把文件内容写到 `user_buf`。  
如果 `user_buf` 所在页是 COW 页，写入不会经过用户态 store 指令，也就不一定触发普通用户态 page fault。  
所以内核必须在 `copyout` 中主动检查 `PTE_COW` 并调用 `k_try_cow`。



---

# 6. lock 实验：锁竞争优化与读写锁

## 6.1 Memory allocator

本题目要求优化 xv6 的物理页分配器，减少多个 CPU 同时调用 `kalloc()` 和 `kfree()` 时对同一把全局锁的竞争。原始 xv6 中只有一个全局空闲链表和一把 `kmem.lock`，所有 CPU 分配和释放物理页时都要竞争这把锁，因此在多核情况下容易产生较多自旋等待。

本仓库的实现思路是：将原来的全局 freelist 改成每个 CPU 一个 freelist。每个 CPU 优先从自己的空闲链表分配页面，释放页面时也优先释放到当前 CPU 的链表中。只有当前 CPU 的 freelist 为空时，才尝试从其他 CPU 的 freelist 中“偷”页面。

核心数据结构位于 `kernel/kalloc.c`：

```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];
```

这里将 `kmem` 定义为数组，每个 CPU 对应一个独立的物理页空闲链表和一把锁。这样多数情况下，每个 CPU 只需要竞争自己的锁，减少了所有 CPU 争用同一把全局锁的情况。

初始化时，需要为每个 CPU 的 freelist 初始化锁：

```c
void
kinit()
{
  for(int i =0; i < NCPU; ++i)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}
```

释放页面时，代码会先关闭中断并获取当前 CPU 编号：

```c
void
kfree(void *pa)
{
  struct run *r;

  push_off();

  int cpu = cpuid();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP){
    pop_off();
    panic("kfree");
  }

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);

  pop_off();
}
```

这里使用 `push_off()` 的原因是：获取当前 CPU 编号后，不能让进程在中间被中断并迁移到其他 CPU，否则 `cpuid()` 得到的 CPU 与后续操作的 freelist 可能不一致。因此在获取 CPU 编号和操作对应 freelist 期间要关闭中断。

在 `kfree()` 中，释放的页面会被插入当前 CPU 的 freelist：

```c
acquire(&kmem[cpu].lock);
r->next = kmem[cpu].freelist;
kmem[cpu].freelist = r;
release(&kmem[cpu].lock);
```

这样释放操作通常只需要获取当前 CPU 的锁，而不会竞争全局锁。

分配页面时，也优先从当前 CPU 的 freelist 中取页：

```c
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu = cpuid();

  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  release(&kmem[cpu].lock);
```

如果当前 CPU 的 freelist 中有空闲页，直接取出返回即可。这样可以大幅减少跨 CPU 的锁竞争。

如果当前 CPU 的 freelist 为空，则遍历其他 CPU 的 freelist 尝试偷取页面：

```c
  if(!r){
    for(int i =0; i< NCPU;++i){
      if(i == cpu){
        continue;
      }
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r){
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      else{
        release(&kmem[i].lock);
      }
    }
  }

  pop_off();
  if(r)
    memset((char*)r, 5, PGSIZE);
  return (void*)r;
}
```

这段代码保证了当前 CPU 没有空闲页时，不会马上分配失败，而是尝试从其他 CPU 的 freelist 中获得页面。为了避免自己偷自己的页面，代码中跳过了当前 CPU：

```c
if(i == cpu){
  continue;
}
```

该题目的关键点是：通过将全局 freelist 拆分为 per-CPU freelist，减少 `kmem` 锁竞争；同时通过 stealing 机制保证单个 CPU freelist 为空时仍然可以从其他 CPU 获得页面。这样既提升了并发性能，又保证了物理页分配器的正确性。

---

## 6.2 Read/Write locks

本题目要求实现读写自旋锁。普通自旋锁同一时刻只允许一个执行流进入临界区，但很多场景下多个读者可以同时读取共享数据，只要没有写者即可。因此读写锁需要支持：

1. 多个读者可以同时持锁；
2. 写者必须独占；
3. 有写者等待时，新的读者应当阻塞，避免写者饥饿。

本仓库在 `kernel/spinlock.h` 中定义了 `rwspinlock`：

```c
struct rwspinlock {
  volatile int readers;
  volatile int pending_writers;
  volatile int writer;
  struct cpu *cpu;
};
```

其中：

```text
readers：当前正在持有读锁的读者数量
pending_writers：正在等待写锁的写者数量
writer：是否有写者正在持有写锁
cpu：当前写锁持有者，用于调试和释放检查
```

读锁获取逻辑在 `read_acquire_inner()` 中：

```c
static void
read_acquire_inner(struct rwspinlock *rwlk)
{
  for(;;){
    while(__atomic_load_n(&rwlk->writer, __ATOMIC_ACQUIRE) != 0 ||
          __atomic_load_n(&rwlk->pending_writers, __ATOMIC_ACQUIRE) != 0)
      ;

    __sync_fetch_and_add(&rwlk->readers, 1);
    __sync_synchronize();

    if(__atomic_load_n(&rwlk->writer, __ATOMIC_ACQUIRE) == 0 &&
       __atomic_load_n(&rwlk->pending_writers, __ATOMIC_ACQUIRE) == 0){
      break;
    }

    __sync_fetch_and_sub(&rwlk->readers, 1);
  }
}
```

读者进入前会检查：

```c
writer != 0 || pending_writers != 0
```

如果当前有写者正在写，或者已经有写者在等待，那么读者不能继续进入。这样做是为了避免读者源源不断进入导致写者一直拿不到锁，也就是避免写者饥饿。

读者通过：

```c
__sync_fetch_and_add(&rwlk->readers, 1);
```

增加读者数量。但增加之后还需要再次检查是否有写者出现：

```c
if(writer == 0 && pending_writers == 0)
  break;
```

如果在读者增加计数的过程中有写者到来，则需要撤销本次读者计数：

```c
__sync_fetch_and_sub(&rwlk->readers, 1);
```

然后重新尝试。这是为了避免读者和写者并发进入导致互斥性被破坏。

读锁释放比较简单：

```c
static void
read_release_inner(struct rwspinlock *rwlk)
{
  int old;

  old = __sync_fetch_and_sub(&rwlk->readers, 1);
  if(old <= 0)
    panic("read_release");
}
```

释放读锁时将 `readers` 减一。如果原来的读者数量小于等于 0，说明出现了没有持有读锁却释放读锁的错误，因此 panic。

写锁获取逻辑如下：

```c
static void
write_acquire_inner(struct rwspinlock *rwlk)
{
  __sync_fetch_and_add(&rwlk->pending_writers, 1);

  for(;;){
    while(__atomic_load_n(&rwlk->readers, __ATOMIC_ACQUIRE) != 0 ||
          __atomic_load_n(&rwlk->writer, __ATOMIC_ACQUIRE) != 0)
      ;

    if(__sync_bool_compare_and_swap(&rwlk->writer, 0, 1)){
      rwlk->cpu = mycpu();
      __sync_fetch_and_sub(&rwlk->pending_writers, 1);
      __sync_synchronize();
      return;
    }
  }
}
```

写者首先执行：

```c
__sync_fetch_and_add(&rwlk->pending_writers, 1);
```

表示自己已经开始等待写锁。这样后续新读者在获取读锁时会看到 `pending_writers != 0`，从而停止进入读临界区。

随后写者等待两个条件满足：

```c
readers == 0
writer == 0
```

也就是说，当前没有读者，也没有其他写者。由于可能有多个写者同时等待，所以真正设置 `writer` 时使用 CAS：

```c
__sync_bool_compare_and_swap(&rwlk->writer, 0, 1)
```

只有一个写者能成功把 `writer` 从 0 改成 1。成功后记录当前 CPU：

```c
rwlk->cpu = mycpu();
```

并减少等待写者数量：

```c
__sync_fetch_and_sub(&rwlk->pending_writers, 1);
```

写锁释放逻辑如下：

```c
static void
write_release_inner(struct rwspinlock *rwlk)
{
  if(__atomic_load_n(&rwlk->writer, __ATOMIC_ACQUIRE) == 0 ||
     rwlk->cpu != mycpu())
    panic("write_release");

  rwlk->cpu = 0;
  __sync_synchronize();
  __atomic_store_n(&rwlk->writer, 0, __ATOMIC_RELEASE);
}
```

释放写锁前会检查当前是否真的有写者持锁，以及当前 CPU 是否是写锁持有者。如果不是，则说明释放操作错误，直接 panic。释放时将 `writer` 置为 0，允许后续读者或写者进入。

对外提供的读写锁接口如下：

```c
void
read_acquire(struct rwspinlock *rwlk)
{
  push_off();
  read_acquire_inner(rwlk);
}

void
read_release(struct rwspinlock *rwlk)
{
  read_release_inner(rwlk);
  pop_off();
}

void
write_acquire(struct rwspinlock *rwlk)
{
  push_off();
  write_acquire_inner(rwlk);
}

void
write_release(struct rwspinlock *rwlk)
{
  write_release_inner(rwlk);
  pop_off();
}
```

这里和普通自旋锁类似，进入锁操作前使用 `push_off()` 关闭中断，释放后使用 `pop_off()` 恢复中断，避免持锁期间被中断打断造成死锁或状态不一致。

初始化函数为：

```c
void
initrwlock(struct rwspinlock *rwlk)
{
  rwlk->readers = 0;
  rwlk->pending_writers = 0;
  rwlk->writer = 0;
  rwlk->cpu = 0;
}
```

本题目的核心是实现一个写者优先的读写锁。多个读者可以并发进入，但只要有写者正在等待，新的读者就不能继续进入；写者需要等所有读者退出，并通过 CAS 独占 `writer` 标志。这样既保证了读读并发，又保证了写操作的互斥性，并避免写者长期饥饿。

# 7. fs 

## 7.1 大文件支持：双重间接索引

### 原始问题

原 xv6 inode 只支持：

- 12 个直接块；
- 1 个一级间接块。

文件最大大小有限。

本仓库将直接块减少为 11 个，腾出一个地址槽作为二级间接块指针，从而支持更大文件。

### 涉及文件

```text
kernel/fs.h
kernel/fs.c
kernel/file.h
mkfs/mkfs.c
```

### 修改 inode 布局

```c
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint))
#define NDOUBLE_INDIRECT (NINDIRECT * NINDIRECT)//建立三级索引机制
#define MAXFILE (NDIRECT + NINDIRECT + NDOUBLE_INDIRECT)

struct dinode {
  ...
  uint addrs[NDIRECT+1+1];   // 11 direct blocks, 1 indirect block and 1 double indirect block  
};
```

### 代码解析

#### 1. 为什么 `NDIRECT` 从 12 改为 11

inode 中 `addrs[]` 数组大小固定。  
为了增加一个二级间接块指针，需要牺牲一个直接块指针。

布局变成：

```text
addrs[0..10]       11 个直接块
addrs[11]          一级间接块
addrs[12]          二级间接块
```

#### 2. 最大文件大小

```c
MAXFILE = NDIRECT + NINDIRECT + NDOUBLE_INDIRECT
```

如果块大小是 1024 字节，`uint` 是 4 字节，则：

```text
NINDIRECT = 1024 / 4 = 256
NDOUBLE_INDIRECT = 256 * 256 = 65536
```

最大数据块数大约是：

```text
11 + 256 + 65536
```

比原始 xv6 大很多。

---

## 7.2 `bmap`：逻辑块号到磁盘块号

### 直接块逻辑

```c
if(bn < NDIRECT){
  if((addr = ip->addrs[bn]) == 0){
    addr = balloc(ip->dev);
    if(addr == 0)
      return 0;
    ip->addrs[bn] = addr;
  }
  return addr;
}
bn -= NDIRECT;
```

### 解析

如果逻辑块号 `bn` 小于 11，直接使用 inode 中的 `addrs[bn]`。

如果还没有分配磁盘块：

```c
addr = balloc(ip->dev);
ip->addrs[bn] = addr;
```

---

### 一级间接块逻辑

```c
if(bn < NINDIRECT){
  if((addr = ip->addrs[NDIRECT]) == 0){
    addr = balloc(ip->dev);
    if(addr == 0)
      return 0;
    ip->addrs[NDIRECT] = addr;
  }
  bp = bread(ip->dev, addr);
  a = (uint*)bp->data;
  if((addr = a[bn]) == 0){
    addr = balloc(ip->dev);
    if(addr){
      a[bn] = addr;
      log_write(bp);
    }
  }
  brelse(bp);
  return addr;
}
```

### 解析

一级间接块本身是一个磁盘块，里面存放 256 个数据块号。

流程：

1. 如果 `ip->addrs[NDIRECT]` 为空，先分配一级间接块；
2. `bread` 读出间接块；
3. 把块内容当作 `uint[]`；
4. `a[bn]` 就是目标数据块号；
5. 如果为空，分配数据块并写回日志。

---

### 二级间接块逻辑

```c
bn -= NINDIRECT;
if (bn < NDOUBLE_INDIRECT){
  addr = ip->addrs[NDIRECT+1];
  if(addr == 0){
    addr = balloc(ip->dev);
    if(addr == 0)
      return 0;
    ip->addrs[NDIRECT+1] = addr;
  }

  struct buf* bp_l1 = bread(ip->dev, addr);
  uint* a_l1 = (uint*)bp_l1->data;
  uint idx_l1 = bn / NINDIRECT;
  if ((addr = a_l1[idx_l1]) == 0) {
    addr = balloc(ip->dev);
    if (addr == 0) {
      brelse(bp_l1);
      return 0;
    }
    a_l1[idx_l1] = addr;
    log_write(bp_l1);
  }

  brelse(bp_l1);

  struct buf* bp_l2 = bread(ip->dev, addr);
  uint* a_l2 = (uint*)bp_l2->data;
  uint idx_l2 = bn % NINDIRECT;
  if ((addr = a_l2[idx_l2]) == 0) {
    addr = balloc(ip->dev);
    if (addr) {
      a_l2[idx_l2] = addr;
      log_write(bp_l2);
    }
  }

  brelse(bp_l2);
  return addr;
}
```

### 代码解析

二级间接块是两层索引：

```text
inode.addrs[12]
    ↓
一级索引块，里面有 256 个二级索引块地址
    ↓
二级索引块，每个里面有 256 个数据块地址
    ↓
真正的数据块
```

#### 1. 找到二级间接根块

```c
addr = ip->addrs[NDIRECT+1];
```

这是二级间接树的根。

#### 2. 计算第一层索引

```c
uint idx_l1 = bn / NINDIRECT;
```

例如 `bn = 300`，`NINDIRECT = 256`：

```text
idx_l1 = 300 / 256 = 1
```

表示使用一级索引块中的第 1 项。

#### 3. 计算第二层索引

```c
uint idx_l2 = bn % NINDIRECT;
```

继续上例：

```text
idx_l2 = 300 % 256 = 44
```

表示在第二层索引块中使用第 44 项。

#### 4. 必须对索引块调用 `log_write`

```c
log_write(bp_l1);
log_write(bp_l2);
```

xv6 文件系统有日志机制。  
修改磁盘块内容后，必须写入日志，否则崩溃恢复时元数据可能不一致。

---

## 7.3 `itrunc`：释放二级间接块

### 核心代码

```c
if (ip->addrs[NDIRECT + 1]) {
  struct buf* bp_l1 = bread(ip->dev, ip->addrs[NDIRECT + 1]);
  uint* a_l1 = (uint*)bp_l1->data;

  for (i = 0; i < NINDIRECT; ++i) {
    if (a_l1[i] == 0) {
      continue;
    }

    struct buf* bp_l2 = bread(ip->dev, a_l1[i]);
    uint* a_l2 = (uint*)bp_l2->data;
    for (j = 0; j < NINDIRECT; ++j) {
      if (a_l2[j] == 0) {
        continue;
      }

      bfree(ip->dev, a_l2[j]);
    }

    brelse(bp_l2);
    bfree(ip->dev, a_l1[i]);
  }

  brelse(bp_l1);
  bfree(ip->dev, ip->addrs[NDIRECT + 1]);
  ip->addrs[NDIRECT + 1] = 0;
}
```

### 代码解析

释放顺序必须从叶子到根：

1. 释放所有数据块；
2. 释放第二层索引块；
3. 释放第一层索引根块；
4. 清空 inode 中的地址。

---

## 7.4 符号链接 symlink

### 题目

实现：

```c
symlink(target, path)
```

创建一个符号链接文件。  
打开符号链接时，默认跟随链接打开目标文件；如果指定 `O_NOFOLLOW`，则打开符号链接本身。

### 涉及文件

```text
kernel/stat.h
kernel/fcntl.h
kernel/sysfile.c
kernel/syscall.c
user/user.h
user/usys.pl
```

### `sys_symlink`

```c
uint64 
sys_symlink(void)
{
  char target[MAXPATH]; // The target that the symlink refers to.
  char path[MAXPATH];   // The path where the symlink is created.

  argstr(0, target, MAXPATH);
  argstr(1, path, MAXPATH);

  int target_strlen = strlen(target); 

  begin_op();
  struct inode *ip = create(path, T_SYMLINK, 0, 0);
  if (!ip) {
    end_op();
    return -1;
  }

  if (writei(ip, 0, (uint64)target, 0, target_strlen) != target_strlen) {
    iunlockput(ip);
    end_op();
    return -1;
  }

  iunlockput(ip);
  end_op();

  return 0;
}
```

### 代码解析

#### 1. 读取两个路径参数

```c
argstr(0, target, MAXPATH);
argstr(1, path, MAXPATH);
```

- `target`：符号链接指向的目标；
- `path`：符号链接文件本身的路径。

#### 2. 创建 T_SYMLINK inode

```c
struct inode *ip = create(path, T_SYMLINK, 0, 0);
```

符号链接本质上是一种特殊类型的 inode。

#### 3. 把 target 路径写入 inode 数据区

```c
writei(ip, 0, (uint64)target, 0, target_strlen)
```

符号链接文件的数据内容就是目标路径字符串。  
例如：

```sh
ln -s /a/b link
```

则 `link` 的 inode 数据区保存：

```text
/a/b
```

#### 4. 日志事务

```c
begin_op();
...
end_op();
```

文件系统修改必须包裹在日志事务中，保证崩溃一致性。

---

## 7.5 修改 `open` 支持跟随符号链接

### 核心代码

```c
if(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)){
  for(int depth = 0;depth < 10 && ip->type == T_SYMLINK;depth++){
    char target[MAXPATH];

    if(readi(ip, 0, (uint64)target, 0, ip->size) != ip->size){
      iunlockput(ip);
      end_op();
      return -1;
    }

    iunlockput(ip);

    if((ip = namei(target)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
  }

  if(ip->type == T_SYMLINK){
    iunlockput(ip);
    end_op();
    return -1;
  }
}
```

### 代码解析

#### 1. 只有未设置 `O_NOFOLLOW` 时才跟随

```c
if(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW))
```

如果用户指定 `O_NOFOLLOW`，则打开符号链接自身，而不是目标文件。

#### 2. 循环跟随多级链接

```c
for(int depth = 0; depth < 10 && ip->type == T_SYMLINK; depth++)
```

符号链接可能指向另一个符号链接，所以需要循环解析。

例如：

```text
a -> b
b -> c
c -> real_file
```

#### 3. 读取 target

```c
readi(ip, 0, (uint64)target, 0, ip->size)
```

符号链接 inode 的数据区保存目标路径，所以用 `readi` 读出来。

#### 4. 释放当前 inode，再查找目标

```c
iunlockput(ip);

if((ip = namei(target)) == 0){
  end_op();
  return -1;
}
ilock(ip);
```

不能一直持有旧 symlink inode 的锁，否则可能造成问题。  
所以读取目标后释放当前 inode，再 `namei(target)` 找到目标 inode。

#### 5. 防止符号链接环

```c
if(ip->type == T_SYMLINK){
  iunlockput(ip);
  end_op();
  return -1;
}
```

循环最多 10 层。  
如果 10 层后仍然是符号链接，认为可能存在环，例如：

```text
a -> b
b -> a
```

于是返回错误。


---

# 8. net 

## 8.1 发送数据包 `e1000_transmit`

### 题目

实现 E1000 网卡的发送逻辑。  
上层网络栈传入一个以太网帧缓冲区，驱动需要把它放入发送描述符环，让硬件发送。

### 涉及文件

```text
kernel/e1000.c
kernel/e1000_dev.h
```

### 发送环结构

```c
#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
```

E1000 使用 descriptor ring。  
`tx_ring` 是发送描述符数组，硬件通过 TDT/TDH 寄存器管理发送队列。

### 核心代码

```c
int
e1000_transmit(char *buf, int len)
{
  acquire(&e1000_lock);
  uint32 idx = regs[E1000_TDT];
  if(!(tx_ring[idx].status & E1000_TXD_STAT_DD)){
    release(&e1000_lock);
    return -1;
  }
  if(tx_ring[idx].addr){
    kfree((void*)tx_ring[idx].addr);
  }

  tx_ring[idx].addr = (uint64)buf;
  tx_ring[idx].length = len;
  tx_ring[idx].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  tx_ring[idx].status = 0;
  regs[E1000_TDT] = (idx +1) % TX_RING_SIZE; 
  release(&e1000_lock);

  return 0;
}
```

### 代码解析

#### 1. 加锁保护发送环

```c
acquire(&e1000_lock);
```

多个进程可能同时发送网络包，因此对 `tx_ring` 和 `TDT` 的访问必须加锁。

#### 2. 找到下一个发送描述符

```c
uint32 idx = regs[E1000_TDT];
```

`TDT` 是发送描述符尾指针，表示软件下一次应该填哪个描述符。

#### 3. 检查描述符是否空闲

```c
if(!(tx_ring[idx].status & E1000_TXD_STAT_DD)){
  release(&e1000_lock);
  return -1;
}
```

`DD` 表示 descriptor done，说明硬件已经发送完这个描述符，可以复用。

如果没有 `DD`，说明发送环满了，返回 `-1`，让上层释放或稍后重试。

#### 4. 释放旧缓冲区

```c
if(tx_ring[idx].addr){
  kfree((void*)tx_ring[idx].addr);
}
```

上一次使用该描述符发送的 buffer 在硬件完成后可以释放。

#### 5. 填写描述符

```c
tx_ring[idx].addr = (uint64)buf;
tx_ring[idx].length = len;
tx_ring[idx].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
tx_ring[idx].status = 0;
```

- `addr`：数据包缓冲区地址；
- `length`：数据包长度；
- `EOP`：end of packet，说明这是一个完整包；
- `RS`：request status，请求硬件发送完后写回状态；
- `status = 0`：交给硬件处理。

#### 6. 更新 TDT 通知硬件

```c
regs[E1000_TDT] = (idx +1) % TX_RING_SIZE;
```

移动尾指针，硬件看到 TDT 更新后开始发送。

---

## 8.2 接收数据包 `e1000_recv`

### 题目

实现接收逻辑。  
当网卡收到数据包后，硬件把数据写入接收描述符对应的缓冲区，并设置 DD 位。驱动在中断中取出这些包，交给上层网络栈。

### 接收环初始化

```c
#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
```

初始化时：

```c
for (i = 0; i < RX_RING_SIZE; i++) {
  rx_ring[i].addr = (uint64) kalloc();
  if (!rx_ring[i].addr)
    panic("e1000");
}
```

每个接收描述符提前分配一个 2KB/4KB 缓冲区，供硬件写入收到的数据包。

### 核心代码

```c
static void
e1000_recv(void)
{
  while(1){
    uint32 idx = (regs[E1000_RDT]+1) % RX_RING_SIZE;

    if (!(rx_ring[idx].status & E1000_RXD_STAT_DD))
      return;
    net_rx((char*)rx_ring[idx].addr,rx_ring[idx].length);
    if(!(rx_ring[idx].addr=(uint64)kalloc()))
      panic("recv kalloc failed");

    rx_ring[idx].status = 0;
    regs[E1000_RDT] = idx;
  }
}
```

### 代码解析

#### 1. 找到下一个待处理接收描述符

```c
uint32 idx = (regs[E1000_RDT]+1) % RX_RING_SIZE;
```

`RDT` 表示软件已经归还给硬件的最后一个描述符。  
下一个可能收到包的位置是 `(RDT + 1) % RX_RING_SIZE`。

#### 2. 检查是否有新包

```c
if (!(rx_ring[idx].status & E1000_RXD_STAT_DD))
  return;
```

如果 DD 位没有置位，说明硬件还没收到新包，直接返回。

#### 3. 交给上层网络栈

```c
net_rx((char*)rx_ring[idx].addr, rx_ring[idx].length);
```

`net_rx` 接管当前缓冲区，处理网络包。

#### 4. 为该描述符重新分配缓冲区

```c
if(!(rx_ring[idx].addr=(uint64)kalloc()))
  panic("recv kalloc failed");
```

因为旧缓冲区交给了 `net_rx`，驱动需要为描述符分配一个新的缓冲区，供硬件继续接收后续包。

#### 5. 清空状态并归还描述符

```c
rx_ring[idx].status = 0;
regs[E1000_RDT] = idx;
```

清空 DD 位，并更新 RDT，告诉硬件该描述符可以继续使用。






