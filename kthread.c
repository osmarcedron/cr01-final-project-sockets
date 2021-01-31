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

struct socket * acsock;

struct service {
	struct socket *listen_socket;
	struct task_struct *thread;
};

struct client_service {
	struct socket *socket;
	struct task_struct *thread;
};

struct service *svc;
struct client_service *csvc;

int recv_msg(struct socket *sock, unsigned char *buf, int len)
{
	struct msghdr msg;
	struct kvec iov;
	int size = 0;

	iov.iov_base = buf;
	iov.iov_len = len;

	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;
	msg.msg_name = 0;
	msg.msg_namelen = 0;

	size = kernel_recvmsg(sock, &msg, &iov, 1, len, msg.msg_flags);

	if (size > 0)
		printk(KERN_ALERT "the message is : %s\n",buf);

	return size;
}

int send_msg(struct socket *sock, char *buf, int len)
{
	struct msghdr msg;
	struct kvec iov;
	int size;

	iov.iov_base = buf;
	iov.iov_len = len;

	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;
	msg.msg_name = 0;
	msg.msg_namelen = 0;

	size = kernel_sendmsg(sock, &msg, &iov, 1, len);

	if (size > 0)
		printk(KERN_INFO "message sent!\n");

	return size;
}

int start_listen(void)
{
	int error, i, size;
	struct sockaddr_in sin;

	error = sock_create_kern(&init_net, AF_INET, SOCK_STREAM,
							 IPPROTO_TCP, &svc->listen_socket);
	if(error<0) {
		printk(KERN_ERR "cannot create socket\n");
		return -1;
	}

	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);

	error = kernel_bind(svc->listen_socket, (struct sockaddr*)&sin,
			sizeof(sin));
	if(error < 0) {
		printk(KERN_ERR "cannot bind socket, error code: %d\n", error);
		return -1;
	}

	error = kernel_listen(svc->listen_socket,5);
	if(error<0) {
		printk(KERN_ERR "cannot listen, error code: %d\n", error);
		return -1;
	}
	return 0;
}

static int kthread_server(void* arg)
{
	struct socket *acsock;

	int count = 0;
	struct kthread_thread* thread = (struct kthread_thread*) arg;

    complete(&thread->started);
	start_listen();

	int len = 4;
    while(!thread->stop && count < NTIMES)
    {
		int i, size, error;
		int value;
		get_random_bytes(&i, sizeof(i));
		len = (i + 1) % MAXBYTES;
		unsigned char buf[len+1];

		printk(KERN_INFO "%s: SERVER %s running (%d) and sending (%d) BYTES\n", THIS_MODULE->name,
			   __FUNCTION__, count, len);

		if(kernel_accept(svc->listen_socket, &acsock, 0) < 0) {
			printk(KERN_ERR "cannot accept socket\n");
			return -1;
		}
		printk(KERN_INFO "sock %d accepted\n", i++);

		wait_event_interruptible(wait_queue, wait_queue_flag != 0 );

		memset(&buf, 0, len+1);
		send_msg(acsock, buf, len+1);

		recv_msg(acsock, buf, len);
		kstrtoint(buf, 10, &value);
		printk(KERN_INFO "we sent %d and we got %s \n", len, buf);

		if (value == len) {
			printk(KERN_INFO "the size matches %d accepted\n", value);
		}
		else {
			printk(KERN_ERR "the size doesn't match %d\n", value);
		}

		wait_queue_flag = 0;
		client_queue_flag = 1;
        wake_up_interruptible(&client_queue);

		count++;
		sock_release(acsock);
    }

	complete(&thread->stopped);
	do_exit(0);

    return 0;
}

static int kthread_client(void* arg)
{
	int count= 0;
	int size, ip;
	struct sockaddr_in sin;

	struct kthread_thread* thread = (struct kthread_thread*) arg;

    complete(&thread->started);

	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);

    while(!thread->stop && count < NTIMES)
    {
		int i, size, error;
		int len =8096;
		char size_to_send[10];
		unsigned char buf[len+1];

		printk(KERN_INFO "%s: CLIENT %s running (%d)\n", THIS_MODULE->name,
                __FUNCTION__, count);

		error = sock_create_kern(&init_net, AF_INET, SOCK_STREAM,
								 IPPROTO_TCP, &csvc->socket);
		if(error<0) {
			printk(KERN_ERR "cannot create socket\n");
			return -1;
		}

		printk(KERN_INFO "create connection \n");

		wait_queue_flag = 1;
        wake_up_interruptible(&wait_queue);

		error = kernel_connect(csvc->socket, (struct sockaddr*)&sin,
							   sizeof(struct sockaddr_in), 0);
		if(error<0) {
			printk(KERN_ERR "cannot connect socket\n");
			return -1;
		}
		printk(KERN_INFO "sock %d connected\n", i++);

		memset(&size_to_send, 0, 10);
		size = recv_msg(csvc->socket, buf, len+1);
		sprintf(size_to_send, "%d", size-1);
		send_msg(csvc->socket, size_to_send, 10);

		client_queue_flag = 0;
		wait_event_interruptible(client_queue, client_queue_flag != 0 );

		count++;
		kernel_sock_shutdown(csvc->socket, SHUT_RDWR);
	}
	sock_release(csvc->socket);

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

	svc = kmalloc(sizeof(struct service), GFP_KERNEL);
	csvc = kmalloc(sizeof(struct client_service), GFP_KERNEL);

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

	if (svc->listen_socket){
		kernel_sock_shutdown(svc->listen_socket, SHUT_RDWR);
		sock_release(svc->listen_socket);
		svc->listen_socket = NULL;
	}

	/* if (csvc->socket){ */
	/* 	kernel_sock_shutdown(csvc->socket, SHUT_RDWR); */
	/* 	sock_release(csvc->socket); */
	/* 	csvc->socket = NULL; */
	/* } */

	kfree(svc);
	kfree(csvc);
}

/* entry/exit points of the module */
module_init(kthread_init);
module_exit(kthread_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Osmar cedron, Victor Merckle");
MODULE_DESCRIPTION("Kthread module");
MODULE_VERSION("0.1");
