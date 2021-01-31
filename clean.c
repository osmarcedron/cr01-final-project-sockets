

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

#include <net/sock.h>

#define PORT 2325

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

/* struct socket *acsock; */
/* struct sockaddr_in sin; */

/* struct service { */
/* 	struct socket *listen_socket; */
/* 	struct task_struct *thread; */
/* }; */

/* struct client_service { */
/* 	struct socket *socket; */
/* 	struct task_struct *thread; */
/* }; */

/* struct service *svc; */
/* struct client_service *csvc; */

/* int recv_msg(struct socket *sock, unsigned char *buf, int len) */
/* { */
/* 	struct msghdr msg; */
/* 	struct kvec iov; */
/* 	int size = 0; */

/* 	iov.iov_base = buf; */
/* 	iov.iov_len = len; */

/* 	msg.msg_control = NULL; */
/* 	msg.msg_controllen = 0; */
/* 	msg.msg_flags = 0; */
/* 	msg.msg_name = 0; */
/* 	msg.msg_namelen = 0; */

/* 	size = kernel_recvmsg(sock, &msg, &iov, 1, len, msg.msg_flags); */

/* 	if (size > 0) */
/* 		printk(KERN_ALERT "the message is : %s\n",buf); */

/* 	return size; */
/* } */

/* int send_msg(struct socket *sock,char *buf,int len) */
/* { */
/* 	struct msghdr msg; */
/* 	struct kvec iov; */
/* 	int size; */

/* 	iov.iov_base = buf; */
/* 	iov.iov_len = len; */

/* 	msg.msg_control = NULL; */
/* 	msg.msg_controllen = 0; */
/* 	msg.msg_flags = 0; */
/* 	msg.msg_name = 0; */
/* 	msg.msg_namelen = 0; */

/* 	size = kernel_sendmsg(sock, &msg, &iov, 1, len); */

/* 	if (size > 0) */
/* 		printk(KERN_INFO "message sent!\n"); */

/* 	return size; */
/* } */

/* int start_listen(void) */
/* { */
/* 	int error; */
/* 	error = sock_create_kern(PF_INET, SOCK_STREAM, IPPROTO_TCP, */
/* 							 &svc->listen_socket, 1); */
/* 	if(error<0) { */
/* 		printk(KERN_ERR "cannot create socket\n"); */
/* 		return -1; */
/* 	} */

/* 	sin.sin_addr.s_addr = htonl(INADDR_ANY); */
/* 	sin.sin_family = AF_INET; */
/* 	sin.sin_port = htons(PORT); */

/* 	error = kernel_bind(svc->listen_socket, (struct sockaddr*)&sin, */
/* 			sizeof(sin)); */
/* 	if(error < 0) { */
/* 		printk(KERN_ERR "cannot bind socket, error code: %d\n", error); */
/* 		return -1; */
/* 	} */

/* 	error = kernel_listen(svc->listen_socket,5); */
/* 	if(error<0) { */
/* 		printk(KERN_ERR "cannot listen, error code: %d\n", error); */
/* 		return -1; */
/* 	} */
/* 	return 0; */
/* } */


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
	int len;
	struct kthread_thread* thread = (struct kthread_thread*) arg;

    complete(&thread->started);
	/* start_listen(); */
    while(!thread->stop && count < NTIMES)
    {
		int i, size;
		unsigned char buf[len+1];
		wait_event_interruptible(wait_queue, wait_queue_flag != 0 );
		/* if(wait_queue_flag == 2) { */
		/* 	printk(KERN_INFO "Exit Function\n"); */
        /*     return 0; */
		/* } */
		/* get_random_bytes(&i, sizeof(i)); */
		/* len = i % MAXBYTES; */

		/* if(kernel_accept(svc->listen_socket, &acsock, 0) < 0) { */
		/* 	printk(KERN_ERR "cannot accept socket\n"); */
		/* 	return -1; */
		/* } */
		/* printk(KERN_INFO "sock %d accepted\n", count++); */

		/* send_msg(acsock, buf, len); */
		/* size = recv_msg(acsock, buf, len); */
		/* if (size == len) { */
		/* 	printk(KERN_INFO "the size is correct\n"); */
		/* } */
		/* else { */
		/* 	printk(KERN_ERR "the size is incorrect\n"); */
		/* } */
		/* sock_release(acsock); */

		wait_queue_flag = 0;
		client_queue_flag = 1;
        wake_up_interruptible(&client_queue);
    }
    complete(&thread->stopped);
	do_exit(0);

    return 0;
}


/* int start_sending(void) */
/* { */
/* 	int error, i, size, ip; */
/* 	struct sockaddr_in sin; */
/* 	char str[5]; */

/* 	ip = (10 << 24) | (1 << 16) | (1 << 8) | (3); */
/* 	sin.sin_addr.s_addr = htonl(ip); */
/* 	sin.sin_family = AF_INET; */
/* 	sin.sin_port = htons(PORT); */

/* 	i = 0; */
/* 	while (!kthread_should_stop()) { */
/* 		error = sock_create_kern(PF_INET, SOCK_STREAM, IPPROTO_TCP, */
/* 								 &csvc->socket); */
/* 		if(error<0) { */
/* 			printk(KERN_ERR "cannot create socket\n"); */
/* 			spin_lock(lock); */
/* 			csvc->running = 0; */
/* 			spin_unlock(lock); */
/* 			return -1; */
/* 		} */

/* 		error = kernel_connect(csvc->socket, (struct sockaddr*)&sin, */
/* 							   sizeof(struct sockaddr_in), 0); */
/* 		if(error<0) { */
/* 			printk(KERN_ERR "cannot connect socket\n"); */
/* 			spin_lock(lock); */
/* 			csvc->running = 0; */
/* 			spin_unlock(lock); */
/* 			return -1; */
/* 		} */
/* 		printk(KERN_ERR "sock %d connected\n", i++); */

/* 		size = recv_msg(csvc->socket, buf, len); */

/* 		sprintf(str, "%d", size); */
/* 		/\* int len = 15; *\/ */
/* 		/\* unsigned char buf[len+1]; *\/ */

/* 		/\* strncpy((char*)&buf, &text[0], 14); *\/ */
/* 		send_msg(svc->socket, buf, 4); */

/* 		kernel_sock_shutdown(csvc->socket, SHUT_RDWR); */
/* 	} */
/* 	sock_release(csvc->socket); */

/* 	return 0; */
/* } */

static int kthread_client(void* arg)
{
	int count= 0;
	int error, i, size, ip;
	struct sockaddr_in sin;
	int len = 4;
	unsigned char buf[len+1];

	/* ip = (10 << 24) | (1 << 16) | (1 << 8) | (3); */
	/* sin.sin_addr.s_addr = htonl(ip); */
	/* sin.sin_family = AF_INET; */
	/* sin.sin_port = htons(PORT); */

	struct kthread_thread* thread = (struct kthread_thread*) arg;

    complete(&thread->started);
    while(!thread->stop && count < NTIMES)
    {
		printk(KERN_INFO "%s: CLIENT %s running (%d)\n", THIS_MODULE->name,
                __FUNCTION__, count);

		wait_queue_flag = 1;
        wake_up_interruptible(&wait_queue);
		count++;

		/* error = sock_create_kern(PF_INET, SOCK_STREAM, IPPROTO_TCP, */
		/* 						 &csvc->socket, 1); */
		/* if(error<0) { */
		/* 	printk(KERN_ERR "cannot create socket\n"); */
		/* 	return -1; */
		/* } */

		/* error = kernel_connect(csvc->socket, (struct sockaddr*)&sin, */
		/* 					   sizeof(struct sockaddr_in), 0); */
		/* if(error<0) { */
		/* 	printk(KERN_ERR "cannot connect socket\n"); */
		/* 	return -1; */
		/* } */
		/* printk(KERN_ERR "sock %d connected\n", i++); */

		/* size = recv_msg(csvc->socket, buf, len); */
		/* sprintf(buf, "%d", size); */

		/* send_msg(csvc->socket, buf, len+1); */

		/* kernel_sock_shutdown(csvc->socket, SHUT_RDWR); */

		client_queue_flag = 0;
		wait_event_interruptible(client_queue, client_queue_flag != 0 );
	}
    complete(&thread->stopped);
	/* sock_release(csvc->socket); */
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
