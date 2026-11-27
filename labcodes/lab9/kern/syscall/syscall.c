#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>
#include <clock.h>
#include <sysfile.h>

// LAB5:填写你在lab5中实现的代码

static int sys_gettime(uint64_t arg[]){
    return (int)ticks*10;
}
static int sys_lab6_set_priority(uint64_t arg[]){
    uint64_t priority = (uint64_t)arg[0];
    lab6_set_priority(priority);
    return 0;
}
static int
sys_sleep(uint64_t arg[]) {
    unsigned int time = (unsigned int)arg[0];
    return do_sleep(time);
}
static int
sys_open(uint64_t arg[]) {
    const char *path = (const char *)arg[0];
    uint32_t open_flags = (uint32_t)arg[1];
    return sysfile_open(path, open_flags);
}

static int
sys_close(uint64_t arg[]) {
    int fd = (int)arg[0];
    return sysfile_close(fd);
}

static int
sys_read(uint64_t arg[]) {
    int fd = (int)arg[0];
    void *base = (void *)arg[1];
    size_t len = (size_t)arg[2];
    return sysfile_read(fd, base, len);
}

static int
sys_write(uint64_t arg[]) {
    int fd = (int)arg[0];
    void *base = (void *)arg[1];
    size_t len = (size_t)arg[2];
    return sysfile_write(fd, base, len);
}

static int
sys_seek(uint64_t arg[]) {
    int fd = (int)arg[0];
    off_t pos = (off_t)arg[1];
    int whence = (int)arg[2];
    return sysfile_seek(fd, pos, whence);
}

static int
sys_fstat(uint64_t arg[]) {
    int fd = (int)arg[0];
    struct stat *stat = (struct stat *)arg[1];
    return sysfile_fstat(fd, stat);
}

static int
sys_fsync(uint64_t arg[]) {
    int fd = (int)arg[0];
    return sysfile_fsync(fd);
}

static int
sys_getcwd(uint64_t arg[]) {
    char *buf = (char *)arg[0];
    size_t len = (size_t)arg[1];
    return sysfile_getcwd(buf, len);
}

static int
sys_getdirentry(uint64_t arg[]) {
    int fd = (int)arg[0];
    struct dirent *direntp = (struct dirent *)arg[1];
    return sysfile_getdirentry(fd, direntp);
}

static int
sys_dup(uint64_t arg[]) {
    int fd1 = (int)arg[0];
    int fd2 = (int)arg[1];
    return sysfile_dup(fd1, fd2);
}

static int
sys_mmap(uint64_t arg[])
{
    uintptr_t addr = (uintptr_t)arg[0];
    size_t len = (size_t)arg[1];
    int prot = (int)arg[2];
    int flags = (int)arg[3];
    int fd = (int)arg[4];
    off_t offset = (off_t)arg[5];
    return sysfile_mmap(addr, len, prot, flags, fd, offset);
}

static int
sys_munmap(uint64_t arg[])
{
    uintptr_t addr = (uintptr_t)arg[0];
    size_t len = (size_t)arg[1];
    return sysfile_munmap(addr, len);
}

static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit] = sys_exit,
    [SYS_fork] = sys_fork,
    [SYS_wait] = sys_wait,
    [SYS_exec] = sys_exec,
    [SYS_yield] = sys_yield,
    [SYS_kill] = sys_kill,
    [SYS_getpid] = sys_getpid,
    [SYS_putc] = sys_putc,
    [SYS_pgdir] = sys_pgdir,
    [SYS_gettime] = sys_gettime,
    [SYS_lab6_set_priority] = sys_lab6_set_priority,
    [SYS_sleep] = sys_sleep,
    [SYS_open] = sys_open,
    [SYS_close] = sys_close,
    [SYS_read] = sys_read,
    [SYS_write] = sys_write,
    [SYS_seek] = sys_seek,
    [SYS_fstat] = sys_fstat,
    [SYS_fsync] = sys_fsync,
    [SYS_getcwd] = sys_getcwd,
    [SYS_getdirentry] = sys_getdirentry,
    [SYS_dup] = sys_dup,
    [SYS_mmap] = sys_mmap,
    [SYS_munmap] = sys_munmap,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

