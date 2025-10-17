#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/utsname.h>

int main(void) {
    struct utsname u;
    (void)syscall(SYS_getpid);
    (void)syscall(SYS_getppid);
    (void)syscall(SYS_getuid);
    (void)syscall(SYS_getgid);
    (void)syscall(SYS_getpgid, 0);
    return 0;
}