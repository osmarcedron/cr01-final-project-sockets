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
 * \brief Thread argument A.
 */
static struct kthread_thread a_thread;
/**
 * \brief Thread argument B.
 */
static struct kthread_thread b_thread;

/**
 * \brief Wait queue is a data structure to manage threads that are waiting for some condition to become true.
 *
 * In this particular case, we have one for the server (wait_queue) and one for the client (client_queue).
 */
wait_queue_head_t wait_queue, client_queue;
int wait_queue_flag = 0, client_queue_flag = 0;

struct socket * acsock;

/**
 * \struct service
 * \brief Includes a socket used for listening (listen_socket) and a process descriptor (thread).
 */
struct service {
	struct socket *listen_socket;
	struct task_struct *thread;
};

/**
+ * \struct client_service
+ * \brief Includes a socket used for sending (socket) and a process descriptor (thread).
+ */
struct client_service {
	struct socket *socket;
	struct task_struct *thread;
};

struct service *svc;
struct client_service *csvc;

/**
 * \brief Receive message function.
 */
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

    // Receive the message and print when it contains something (size > 0)
	size = kernel_recvmsg(sock, &msg, &iov, 1, len, msg.msg_flags);

	if (size > 0)
		printk(KERN_ALERT "the message is : %s\n",buf);

	return size;
}

/**
 * \brief Send message function.
 */
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

    // Send the message and print when it contains
    // something (size > 0)
	size = kernel_sendmsg(sock, &msg, &iov, 1, len);

	if (size > 0)
		printk(KERN_INFO "message sent!\n");

	return size;
}

/**
 * \brief Setup all the variables (sockets, ports, ips) to be able to listen.
 */
int start_listen(void)
{
	int error, i, size;
	struct sockaddr_in sin;

    // Create socket
	error = sock_create_kern(&init_net, AF_INET, SOCK_STREAM,
							 IPPROTO_TCP, &svc->listen_socket);
	if(error<0) {
		printk(KERN_ERR "cannot create socket\n");
		return -1;
	}

    // It converts to all available interfaces to byte order.
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);

    // Option needed to allow sockets to be reusable, with this it is possible
    // to run the code multiple times without needing extra time before the
    // OS unbinds the IP addresses used
	int opt = 1;
    error = kernel_setsockopt(svc->listen_socket, SOL_SOCKET,
							  SO_REUSEADDR, (char *)&opt, sizeof (opt));
    if (error < 0) {
		printk(KERN_ERR "Error setting socket options %d\n", error);
        return error;
    }

    // Bind the socket
	error = kernel_bind(svc->listen_socket, (struct sockaddr*)&sin,
			sizeof(sin));
	if(error < 0) {
		printk(KERN_ERR "cannot bind socket, error code: %d\n", error);
		return -1;
	}

    // Listen socket
	error = kernel_listen(svc->listen_socket,5);
	if(error<0) {
		printk(KERN_ERR "cannot listen, error code: %d\n", error);
		return -1;
	}
	return 0;
}

/**
 * \brief Kthread Server.
 *
 * Implements server's logic.
 */
static int kthread_server(void* arg)
{
    // Socket server
	struct socket *acsock;

	int count = 0;
	struct kthread_thread* thread = (struct kthread_thread*) arg;

    // Ensure thread->started is 1
    complete(&thread->started);

    // Initialize the sockets
	start_listen();

	int len;
    // Stop until the number of iterations is less than NTIMES and
    // the thread->stop is not NULL
    while(!thread->stop && count < NTIMES)
    {
		int i, size, error;
		int value;

        // Get a random number < MAXBYTES
		get_random_bytes(&i, sizeof(i));
		len = (i + 1) % MAXBYTES;
		unsigned char buf[len+1];

		printk(KERN_INFO "%s: SERVER %s running (%d) and sending (%d) BYTES\n", THIS_MODULE->name,
			   __FUNCTION__, count, len);

        // Accept connections so listen_socket keeps listening and acsock becomes
        // the communication channel
		if(kernel_accept(svc->listen_socket, &acsock, 0) < 0) {
			printk(KERN_ERR "cannot accept socket\n");
			return -1;
		}
		printk(KERN_INFO "sock %d accepted\n", i++);

        // Sleep until wait_queue_flag == 1
		wait_event_interruptible(wait_queue, wait_queue_flag != 0 );

        // Send message using acsock, the message is full of 0's so when printing
        // it seems empty as it is full of end characters of strings
		memset(&buf, 0, len+1);
		send_msg(acsock, buf, len+1);

        // Receive message with the size of the buffer that the client received
		recv_msg(acsock, buf, len);
		kstrtoint(buf, 10, &value);
		printk(KERN_INFO "we sent %d and we got %s \n", len, buf);

        // Check if the values of the buffer sent and received are the same or not
		if (value == len) {
			printk(KERN_INFO "the size matches %d accepted\n", value);
		}
		else {
			printk(KERN_ERR "the size doesn't match %d\n", value);
		}

        // Wake up the client queue (once the message was sent)
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

/**
 * \brief Kthread Client.
 *
 * Implements client's logic.
 */
static int kthread_client(void* arg)
{
	int count= 0;
	int size, ip;
	struct sockaddr_in sin;

	struct kthread_thread* thread = (struct kthread_thread*) arg;

    complete(&thread->started);

    // Set up the addresses to all available
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

        // Create socket
		error = sock_create_kern(&init_net, AF_INET, SOCK_STREAM,
								 IPPROTO_TCP, &csvc->socket);
		if(error<0) {
			printk(KERN_ERR "cannot create socket\n");
			return -1;
		}

		printk(KERN_INFO "create connection \n");

        // Wake up the server to accept connections before attempting to connect
		wait_queue_flag = 1;
        wake_up_interruptible(&wait_queue);

        // Connect to server's kernel
		error = kernel_connect(csvc->socket, (struct sockaddr*)&sin,
							   sizeof(struct sockaddr_in), 0);
		if(error<0) {
			printk(KERN_ERR "cannot connect socket\n");
			return -1;
		}
		printk(KERN_INFO "sock %d connected\n", i++);

        // Receive the buffer of a size < MAXSIZE
		memset(&size_to_send, 0, 10);
		size = recv_msg(csvc->socket, buf, len+1);
		sprintf(size_to_send, "%d", size-1);

        // Send back the size of the buffer actually sent (pad to length 10)
        // Wait until the server received the message before realsing the socket
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

    // Initialize the wait queues
	init_waitqueue_head(&wait_queue);
	init_waitqueue_head(&client_queue);

    // Start both threads with a
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

	csvc->socket = NULL;

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
