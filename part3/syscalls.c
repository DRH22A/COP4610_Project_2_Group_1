// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/syscalls.h>

// Implemented in the kernel module
extern int start_elevator_syscall(void);
extern int issue_request_syscall(int start_floor, int dest_floor, int type);
extern int stop_elevator_syscall(void);

SYSCALL_DEFINE0(start_elevator)
{
    return start_elevator_syscall();
}

SYSCALL_DEFINE3(issue_request, int, start_floor, int, dest_floor, int, type)
{
    return issue_request_syscall(start_floor, dest_floor, type);
}

SYSCALL_DEFINE0(stop_elevator)
{
    return stop_elevator_syscall();
}

