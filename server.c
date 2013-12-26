#include <semaphore.h>
#include <pthread.h>

/* history 
	history cyclic array of size 100
	history global lock (semaphore)
*/
static char* history[100];
static int hist_head;
static int hist_size;
static sem_t *hist_sem;

/* clients database 
		list of client-descriptors:
			client pid
			client name
			fd fifo in
			fifo_in_lock (semaphore)
			fd fifo out
			private_thread
				msg_queue
			public_thread
				msg_queue < Amsg  < Bmsg < leave_cmd
*/

struct msg_thread;
struct client_node;
struct msg_node;
struct msg_queue;
struct client_list;
struct name_list;
struct name_node;

struct msg_thread {
	int th_id;
	
};

struct client_node {
	pid_t pid;
	char* name;
	FILE* fifo_in;
	FILE* fifo_out;
	sem_t *fifo_in_lock;
	msg_queue private_queue;
	msg_queue public_queue;
	int alive;
};

struct msg_node {
	char* msg;
	struct msg_node* next;
	struct msg_node* prev;
	int msg_len;
};

struct msg_queue {
	msg_node* head;
	msg_node* tail;
	int size;
}

struct client_list {
	client_node* head;
	client_node* tail;
	int size;
}

struct name_node {
	struct name_node* next;
	struct name_node* prev;
	char* name;
}

struct name_list {
	name_node* head;
	int size;
}

/* server fifo thread function 
	open access to server.fifo
	loop read - wait for msg
	parse incoming msg
	open client fifo in
	try to add new client to DB (legal name)
		fail: send error msg to client
			  close fifo in
			  return
		succ: send good msg to client
			  open fifo.out
			  create private + public threads (remember to keep a pointer to the client node INSIDE the thread)
			  sem_wait(name_list_lock)
				add name to name_list
			  sem_post(...)
	continue loop
/*
				
/* reader_thread 
	loop on clients DB.fifo_out
		when new msg is found:
			parse to msg_len, type, message
			switch (type) {
				case (private):
					assign to private_queue of sender.
				case (public):
					assign to public_queue of sender.
				case (cmd):
					switch (message) {
						case (leave):
							close fifo.out, turn off "alive" flag
							assign to public_thread
						case (history):
							create history th_handler, assign to it.
						case (who)
							assign to who_handler.
					}
			}
*/


/* public thread function 
	while (alive || queue.size) {
		if (queue.size > 0) {
			msg_node <- queue.first()
			broadcast (msg)
		}
	}
	
	broadcast_leave(leave msg) to everyone but me
	wait for send success leave msg to me
	disconnect fifo.in
	delete client from DB
	sem_wait(name_list_lock)
		remove name from name_list
	sem_post(...)
	

broadcast(final msg) {
	current = my_client_node;
	do
		sem_wait(current.fifo_in_lock)
			write to fifo
		sem_post(current.fifo_in_lock)
		current = current->next;
	while (current_client != myself)
}
*/

/* private thread function 
	while (alive || queue.size) {
		if (queue.size > 0) {
			msg_node <- queue.first()
			send_private (msg, address)
		}
	}

send_private(msg, address) {
	sem_wait(address.fifo_in_lock)
		write to fifo
		(exit if failed)
	sem_post(...)
	
	sem_wait(myself.fifo_in_lock)
		write to fifo
	sem_post(...)
	
	
/* history thread handler (client id)
	client_node = get_client(id)
	sem_wait(client_node.fifo_in)
		for (i=0, i<size, i++)
			write to fifo (history[(head+i) % 100])
	sem_post(...)
*/

/* who thread handler (client id)
	client_node = get_client(id)
	sem_wait(name_list)
		collect all names
	sem_post(...)
	sem_wait(client_node.fifo_in_lock)
		write to fifo all names
	sem_post(...)
*/

/* exit handler */



int main() {
	
	/* initialize clients database */

	/* initialize history array */
	initialize_history();
	
	/* initialize server_fifo + reader */
	
	/* initialize reader thread */
	
	
	/* create server.txt (which means server is on) */
}