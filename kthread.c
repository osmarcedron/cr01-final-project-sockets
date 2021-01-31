/*
 * kthread - kernel thread worker module.
 * Copyright (c) 2021, Osmar Cedron, Victor Merckle
 */

/**
 * \file kthread.c
 * \brief Thread kernel module for Scalevisor project.
 * \author Osmar Cedron, Victor Merckle
 * \date 2021
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/wait.h> /* wait_queue_head_t, wait_event_interruptible, wake_up_interruptible  */
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

/**
 * \brief Variable that represents the maximum number of bytes sent.
 */
static u32 MAXBYTES = 8096;
module_param(MAXBYTES, int, S_IRUSR | S_IWUSR);

/**
 * \brief Variable that represents the number of times the data will be sent.
 */
static u32 NTIMES = 16;
module_param(NTIMES, int, S_IRUSR | S_IWUSR);

/**
 * \struct kthread_thread
 * \brief Argument structure for thread.
 */
struct kthread_thread
{
    /**
     * \brief Variable condition for thread started.
     */
    struct completion started;

    /**
     * \brief Variable condition for thread stopped.
     */
    struct completion stopped;

    /**
     * \brief Variable to tell the thread to stop.
     */
    bool stop;

    /**
     * \brief Value for the thread.
     */
    int value;
};

/**
 * \brief Thread argument.
 */
static struct kthread_thread a_thread;
static struct kthread_thread b_thread;

wait_queue_head_t wait_queue, client_queue;
int wait_queue_flag = 0, client_queue_flag = 0;

/**
 * \brief Function for the thread.
 * \param arg argument.
 * \return 0 if success, -1 otherwise.
 */
/* static int kthread_runner(void* arg) */
/* { */
/*     struct kthread_thread* thread = (struct kthread_thread*) arg; */

/*     complete(&thread->started); */
/* 	int count= 0; */
/*     while(!thread->stop && count < NTIMES) */
/*     { */
/* 		int i, lessthan8096; */
/* 		get_random_bytes(&i, sizeof(i)); */
/* 		lessthan8096 = i % MAXBYTES; */
/*         printk(KERN_INFO "%s: %s running (%d)\n", THIS_MODULE->name, */
/*                 __FUNCTION__, lessthan8096); */
/*         mdelay(500); */
/* 		count++; */
/*     } */

/*     complete(&thread->stopped); */
/*     return 0; */
/* } */

static int kthread_server(void* arg)
{
	int count = 0;
	struct kthread_thread* thread = (struct kthread_thread*) arg;

    complete(&thread->started);
    while(!thread->stop && count < NTIMES)
    {
		printk(KERN_INFO "%s: SERVER %s running (%d)\n", THIS_MODULE->name,
			   __FUNCTION__, count);

		wait_event_interruptible(wait_queue, wait_queue_flag != 0 );

		wait_queue_flag = 0;
		client_queue_flag = 1;
        wake_up_interruptible(&client_queue);
		count++;
    }
    complete(&thread->stopped);
	do_exit(0);

    return 0;
}


static int kthread_client(void* arg)
{
	int count= 0;
	struct kthread_thread* thread = (struct kthread_thread*) arg;

    complete(&thread->started);
    while(!thread->stop && count < NTIMES)
    {
		printk(KERN_INFO "%s: CLIENT %s running (%d)\n", THIS_MODULE->name,
                __FUNCTION__, count);

		wait_queue_flag = 1;
        wake_up_interruptible(&wait_queue);
		count++;

		client_queue_flag = 0;
		wait_event_interruptible(client_queue, client_queue_flag != 0 );
	}
    complete(&thread->stopped);
	do_exit(0);

	return 0;
}



/**
 * \brief Module initialization.
 *
 * Set up stuff when module is added.
 * \return 0 if success, negative value otherwise.
 */
static int __init kthread_init(void)
{
    printk(KERN_INFO "%s: initialization\n", THIS_MODULE->name);

	init_waitqueue_head(&wait_queue);
	init_waitqueue_head(&client_queue);

    a_thread.stop = 0;
    a_thread.value = 42;
    init_completion(&a_thread.started);
    init_completion(&a_thread.stopped);

    b_thread.stop = 0;
    b_thread.value = 42;
    init_completion(&b_thread.started);
    init_completion(&b_thread.stopped);

	if(IS_ERR(kthread_run(kthread_server, &a_thread, "A")) ||
	   IS_ERR(kthread_run(kthread_client, &b_thread, "B")))
    {
        return -ENOMEM;
    }

    printk(KERN_INFO "%s: wait for thread A starts\n", THIS_MODULE->name);
    /* wait the thread to be started */
    wait_for_completion(&a_thread.started);
    printk(KERN_INFO "%s: runner thread A started\n", THIS_MODULE->name);

    printk(KERN_INFO "%s: wait for thread B starts\n", THIS_MODULE->name);
    /* wait the thread to be started */
    wait_for_completion(&b_thread.started);
    printk(KERN_INFO "%s: runner thread B started\n", THIS_MODULE->name);

    return 0;
}

/**
 * \brief Module finalization.
 *
 * Clean up stuff when module is removed.
 */
static void __exit kthread_exit(void)
{
    /* ask ending of thread */
    a_thread.stop = 1;
    a_thread.value = 0;
    wait_for_completion(&a_thread.stopped);

    printk(KERN_INFO "%s: exit A\n", THIS_MODULE->name);

    b_thread.stop = 1;
    b_thread.value = 0;
    wait_for_completion(&b_thread.stopped);

	printk(KERN_INFO "%s: exit B\n", THIS_MODULE->name);
}

/* entry/exit points of the module */
module_init(kthread_init);
module_exit(kthread_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Osmar cedron, Victor Merckle");
MODULE_DESCRIPTION("Kthread module");
MODULE_VERSION("0.1");
