#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define PROC_NAME "timer"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A kernel module that tracks current and elapsed time");

// Store the last time read was called
static struct timespec64 last_time = {0, 0};
static int first_read = 1;  // Flag

// This function is called when /proc/timer is read
static int timer_proc_show(struct seq_file *m, void *v)
{
    struct timespec64 current_time;

    // Gets current time
    ktime_get_real_ts64(&current_time);

    // Prints current time
    seq_printf(m, "current time: %lld.%09ld seconds\n",
               (long long)current_time.tv_sec,
               current_time.tv_nsec);

    // Calculates and prints elapsed time if not the first read, 
    if (!first_read) {
        long long elapsed_sec;
        long elapsed_nsec;

        elapsed_sec = current_time.tv_sec - last_time.tv_sec;
        elapsed_nsec = current_time.tv_nsec - last_time.tv_nsec;

        // Handle nanosecond underflow
        if (elapsed_nsec < 0) {
            elapsed_sec--;
            elapsed_nsec += 1000000000L;
        }

        seq_printf(m, "elapsed time: %lld.%09ld seconds\n",
                   elapsed_sec,
                   elapsed_nsec);
    }

    // Update last_time for next read
    last_time = current_time;
    first_read = 0;

    return 0;
}

// This function is called when /proc/timer is opened
static int timer_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, timer_proc_show, NULL);
}

// File operations structure for /proc/timer
static const struct proc_ops timer_proc_fops = {
    .proc_open = timer_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

//This function is called when the module is loaded
static int __init timer_init(void)
{
    if (!proc_create(PROC_NAME, 0444, NULL, &timer_proc_fops)) {
        pr_err("failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    pr_info("module loaded\n");
    pr_info("/proc/%s created\n", PROC_NAME);

    first_read = 1;
    last_time = (struct timespec64){0, 0};

    return 0;
}

//This function is called when the module is removed
static void __exit timer_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
    pr_info("/proc/%s removed\n", PROC_NAME);
    pr_info("module unloaded\n");
}

module_init(timer_init);
module_exit(timer_exit);
