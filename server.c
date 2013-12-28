#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "parsers.h"

#define MAX_INT 2147000000
#define TRUE 1
#define FALSE 0
#define list_head_client client_list.head->next

static char* history[100];
static int hist_head;
static int hist_size;
static sem_t hist_lock;

struct msg_thread;
struct client_node;
struct msg_node;
struct msg_queue;
struct client_list;
struct name_list;
struct name_node;

int server_alive;
struct Client_List client_list;

struct msg_thread {
	int th_id;
};

struct msg_node{
	char* msg;
	struct msg_node* next;
	struct msg_node* prev;
	int msg_len;
};

struct msg_queue {
	struct msg_node* head;
	struct msg_node* tail;
	int size;
	sem_t* queue_lock;
};

struct Client_Node {
	struct Client_Node *next;
	struct Client_Node *prev;
	pid_t pid;
	char* name;
	int fifo_in;
	int fifo_out;
	sem_t *fifo_in_lock;
	pthread_t private_thread;
	pthread_t public_thread;
	struct msg_queue private_queue;
	struct msg_queue public_queue;
	sem_t *access_lock; /* (unlimited access) */
	int alive;
};

struct Client_List {
	struct Client_Node* head;
};

typedef struct Client_Node Client_Node;

/**************************************msg queue******************************/

int init_queue(struct msg_queue* queue) {
	queue = (struct msg_queue*)malloc(sizeof(struct msg_queue));
	struct msg_node *head = (struct msg_node*)malloc(sizeof(struct msg_node));
	struct msg_node *tail = (struct msg_node*)malloc(sizeof(struct msg_node));
	head->prev = NULL;
	head->next = tail;
	tail->prev = head;
	tail->next = NULL;
	queue->head = head;
	queue->tail = tail;
	queue->size = 0;
	return sem_init(queue->queue_lock, 0, 1);
}

void destroy_queue(struct msg_queue* queue){
	int i;
	for(i=0; i<queue->size; i++){
		free(queue->head->next->msg);
		queue_pop(queue);
	}
	free(queue->head);
	free(queue->tail);
	sem_destroy(queue->queue_lock);
	free(queue);
}

void queue_pop(struct msg_queue* queue) {
	struct msg_node *tmp = queue->head->next;
	queue->head->next = tmp->next;
	tmp->next->prev = queue->head->next;
	free(tmp);
}

void queue_insert_msg(struct msg_queue* queue, char* msg, int msg_len){
	struct msg_node* node = malloc(sizeof(struct msg_node));
	node->msg = msg;
	node->msg_len = msg_len;
	node->next = queue->tail;
	node->prev = queue->tail->prev;
	queue->tail->prev->next = node;
	queue->tail->prev = node;
	queue->size++;
}

/*****************************************************************************/

/**********************************listener_run_thread*************************/

void handle_msg(Client_Node* client, char* msg, int msg_len);

pthread_t listener_thread;
void* listener_run_thread(){
	char msg_len_arr[4] = {0};
	int msg_len;
	int gotten = 0;
	int gotten_tmp = 0;
	char *msg;

	sem_wait(client_list.head->next.access_lock);
	Client_Node *current = client_list.head;
	Client_Node *tmp = NULL;

	while (server_alive){
		if (current != client_list.head){
			gotten = read(current->fifo_out, &msg_len_arr, sizeof(char)*4);
			if (gotten > 0) {
				while (gotten < 4) {
					gotten_tmp = read(current->fifo_out, (msg_len_arr + gotten), sizeof(char)*(4-gotten));
					if (gotten_tmp < 0) {
						continue;
					}
					gotten += gotten_tmp;
				}
				msg_len = msg_len_arr[0]*pow(256,3) + msg_len_arr[1]*pow(256,2)+ msg_len_arr[2]*256 + msg_len_arr[3];
				gotten = 0;
				msg = malloc(sizeof(char)*msg_len);
				while (gotten <  msg_len){
					gotten_tmp = read(current->fifo_out, msg+gotten, sizeof(char)*(msg_len-gotten));
					if (gotten_tmp < 0){
						continue;
					}
					gotten += gotten_tmp;
				}
				handle_msg(current, msg, msg_len);
			}
		}
		try_next:
		tmp = current->next;
		sem_wait(tmp->access_lock);
		if (tmp != current->next) {
			sem_post(tmp->access_lock);
			goto try_next;
		}
		tmp = current;
		current = current->next;
		sem_post(tmp.access_lock);
	}
	return 0;
}

void handle_msg(Client_Node* client, char* msg, int msg_len){
	pthread_t cmd;
	switch(ParseClientMsg(msg)){
	case 0:
		//public case.
		int f_m_len = 6 + msg_len + strlen(client->name);
		char* formatted_msg = malloc(sizeof(char)*f_m_len);
		convertIntToChars(f_m_len-4, formatted_msg);
		sprintf(formatted_msg+4, "%s: %s", client->name, msg);
		free(msg);
		sem_wait(client->public_queue->queue_lock);
		queue_insert_msg(client->public_queue, formatted_msg, f_m_len);
		sem_post(client->public_queue->queue_lock);
		break;
	case 1:
		//private case.
		sem_wait(client->private_queue->queue_lock);
		queue_insert_msg(client->private_queue, msg, msg_len);
		sem_post(client->private_queue->queue_lock);
		break;
	case 2:
		//who case.
		pthread_create(&cmd, NULL, who_thread, client);
		free(msg);
		break;
	case 3:
		//leave case.
		close(client->fifo_out);
		client->alive = 0;
		free(msg);
		break;

	case 4:
		//history case.
		pthread_create(&cmd, NULL, history_thread, client);
		free(msg);
		break;
	}
}

/*******************************************************************************************/
	//haggai
/* public thread function 
	while (alive || queue.size) {
		if (queue.size > 0) {
			msg_node <- queue.first()
			broadcast (msg)
		}
	}
	
	broadcast_leave(leave msg) to everyone but me 
	send success leave msg to me
	(Wait until broadcast and self msg is done)
	(Wait for private thread to die)
	disconnect fifo.in
	fix DB pointers (my->prev->next = my->next;
					my->next->prev = my->prev;)
	wait for fifo_in_lock value to be 0
	delete client from DB
	

broadcast(final msg) {
	sem_wait(current.access_lock)
	current = my_client_node;
	do {
		sem_wait(current.fifo_in_lock)
			write to fifo
		sem_post(current.fifo_in_lock)
try_next:
		tmp = current->next;
		sem_wait(tmp.access_lock)
		
		if (tmp != current->next) {
			sem_post(tmp.access_lock)
			goto try_next
		}
		tmp = current;
		current = current->next;
		sem_post(tmp.access_lock)
	} while (current_client != start_client)
	sem_post(start_client.access_lock)
	
	document(msg)
}

	//aviad + ilai = big love	
document(msg) {
	sem_wait(hist_lock)
	write
	sem_post(hist_lock)
}
*/

	//haggai
/* private thread function 
	while (alive || queue.size) {
		if (queue.size > 0) {
			msg_node <- queue.first()
			send_private (msg, address)
		}
	}

send_private(msg, address) {
	find client by address (use access locks as used in reader & public)
	sem_wait(address.fifo_in_lock)
		write to fifo
		(if failed - msg = error)
	sem_post(...)
	
	//not necessary to lock yourself for self msg (because you wont be deleted before private thread is dead)
	sem_wait(myself.fifo_in_lock)
		write (msg) to fifo
	sem_post(...)
*/


/*****************************history+who************************************/
/* history thread handler (client_node)*/
void* history_thread(void* client_node){
	int i;
	int index
	int gotten = 0;
	int temp;
	int msg_len;
	Client_Node* client = (Client_Node*)client_node;
	sem_wait(client->access_lock);
	sem_wait(&hist_lock);
	sem_wait(client->fifo_in_lock);

	for(i = 0; i < hist_size; i++){
		index = (i + hist_head)%100;
		msg_len = history[index][0]*pow(256,3) + history[index][1]*pow(256,2)+ history[index][2]*256 + history[index][3];
		while (gotten < msg_len) {
			temp = write(client->fifo_in, (history[index] + gotten), sizeof(char)*(msg_len-gotten));
			if (temp < 0) {
				continue;
			}
			gotten += temp;
		}
		gotten = 0;
	}

	sem_post(client->fifo_in_lock);
	sem_post(&hist_lock);
	sem_post(client->access_lock);
	return 0;
}

/* who thread handler (client_node)*/
void* who_thread(void* client_node){
	struct msg_queue* whos;
	init_queue(whos);
	Client_Node* client = (Client_Node*)client_node;
	sem_wait(client->access_lock);

	Client_Node* iterator = client;
	Client_Node* tmp_iter = NULL;
	sem_wait(iterator->access_lock);
	do{
		int msg_len = 4+5+strlen(iterator->name);
		char* who_msg = malloc(sizeof(char)*msg_len);
		convertIntToChars(msg_len-4, who_msg);
		sprintf(who_msg+4, "who: %s", iterator->name);
		queue_insert_msg(whos, who_msg, msg_len);

		next_iter:
		tmp_iter = iterator->next;
		sem_wait(tmp_iter->access_lock);
		if(tmp_iter != iterator->next){
			sem_post(tmp_iter->access_lock);
			goto next_iter;
		}
		tmp_iter = iterator;
		iterator = iterator->next;
		sem_post(tmp_iter->access_lock);
	} while (iterator != client);
	sem_post(iterator->access_lock);

	sem_wait(client->fifo_in_lock);
	int i;
	int gotten = 0;
	int tmp;
	struct msg_node* current = whos->head->next;
	for(i = 0; i < whos->size; i++){
		while (gotten < current->msg_len) {
			tmp = write(client->fifo_in, (current->msg + gotten), sizeof(char)*(current->msg_len-gotten));
			if (tmp < 0) {
				continue;
			}
			gotten += tmp;
		}
		gotten = 0;
		current = current->next;
	}

	sem_post(client->fifo_in_lock);
	sem_post(client->access_lock);
	destroy_queue(whos);
	return 0;
}

/*****************************************************************************/
/* exit handler 

*/


int initialize_history() {
	hist_head = 0;
	hist_size = 0;
	return sem_init(&hist_lock, 0, 1);
}

//aviad + ilai = big love	
void document(char* msg, int msg_len) {

}

void broadcast(char* msg, int msg_len, Client_Node *node) {
	struct Client_Node *current = node;
	do {
		sem_wait(current->access_lock);
		sem_wait(current->fifo_in_lock);
			int gotten = 0;
			int temp;
			while (gotten < msg_len) {
				temp = write(current->fifo_in, (msg + gotten), sizeof(char)*(msg_len-gotten));
				if (temp < 0) {
					continue;
				}
				gotten += temp;
			}
		sem_post(current->fifo_in_lock);
	
		struct Client_Node *tmp;
		while (1) {
			tmp = current->next;
			sem_wait(tmp->access_lock);
			
			if (tmp != current->next) {
				sem_post(tmp->access_lock);
				continue;
			}
			break;
		}
		tmp = current;
		current = current->next;
		sem_post(tmp->access_lock);
		
	} while (current != node);
	
	sem_post(node->access_lock);
	
	document(msg, msg_len);
	/* broadcast should free the msg after sending it */
	free(msg);
}

void broadcast_leave(struct Client_Node *node) {
	/* very much like broadcast, just start from node->next and send a different msg to node */
}

void* public_thread_run(void* client_node) {//ToDo check with haggai about pointers
	struct Client_Node *my_client = (struct Client_Node*)client_node;
	struct msg_queue my_queue = my_client->public_queue;
	
	while (my_client->alive || my_queue.size) {
		if (my_queue.size > 0) {
			sem_wait(my_queue.queue_lock);
			char* msg_tosend = my_queue.head->next->msg;
			int msg_len = my_queue.head->next->msg_len;
			queue_pop(&my_queue);
			sem_post(my_queue.queue_lock);
			broadcast(msg_tosend, msg_len, my_client);
		}
	}
	
	broadcast_leave(my_client); 	/* msg everyone my leave msg, and msg me something special */
									/* (Wait until broadcast and self msg is done) */
						
	/* (Wait for private thread to die) */
	pthread_join(my_client->private_thread, NULL);
	
	/* disconnect fifo.in (assert that public and private thread queue's is empty) */
	close(my_client->fifo_in);
	
	/* fix DB pointers (wait for access lock to be free)
	   client_node is now BYPASSED in the DB */
	my_client->prev->next = my_client->next;
	my_client->next->prev = my_client->prev;
	int semval = 0;
	while (semval < MAX_INT) { 
		sem_getvalue(my_client->access_lock, &semval); 
	}
	sem_destroy(my_client->access_lock);
	
	/* wait for fifo_in_lock value to be locked (meaning all threads finished writing, and no new threads will get in) */
	sem_wait(my_client->fifo_in_lock);
	sem_destroy(my_client->fifo_in_lock);
	
	//ToDo destroy queue locks

	/* delete client from DB (it was actualy removed in the bypass) */
	free(my_client->name);
	free(my_client);
}

void* private_thread_run(void* client_node) {
	struct Client_Node *my_client = (struct Client_Node*)client_node;
	// Aviad&Ilai Note: notice that messages in private queue are written:
	//		"@[reciever name] [msg]". This is better for finding the user you need
	// 		to send the message to. BUT you need to add "[sender]: " in the begining and calculate size
		//haggai
/* private thread function 
	while (alive || queue.size) {
		if (queue.size > 0) {
			msg_node <- queue.first()
			send_private (msg, address)
		}
	}

send_private(msg, address) {
	find client by address (use access locks as used in reader & public)
	sem_wait(address.fifo_in_lock)
		write to fifo
		(if failed - msg = error)
	sem_post(...)
	
	//not necessary to lock yourself for self msg (because you wont be deleted before private thread is dead)
	sem_wait(myself.fifo_in_lock)
		write (msg) to fifo
	sem_post(...) */
}

/* open fifo.out, fifo_in
   create private + public threads (remember to keep a pointer to the client node INSIDE the thread) */
int DB_insert(char* name, int pid) {
	/* create the new node */
	struct Client_Node *node = (struct Client_Node*)malloc(sizeof(struct Client_Node));
	node->pid = pid;
	node->name = name;
	
	/* open fifo in */
	char fifoIn[25];
	sprintf(fifoIn, "fifo-%d-in", pid);
	node->fifo_in = open(fifoIn, O_RDONLY);
	
	/* open fifo out */
	char fifoOut[25];
	sprintf(fifoOut, "fifo-%d-out", pid);
	node->fifo_out = open(fifoOut, O_WRONLY);
	
	/* init locks */
	sem_init(node->fifo_in_lock, 0, 1);
	sem_init(node->access_lock, 0, MAX_INT);
	
	/* init queues for private and public threads */
	init_queue(&node->private_queue); //ToDo check return
	init_queue(&node->public_queue); //-"-
	
	/* set client alive BEFORE running threads (cuz they will die without it) */
	node->alive = 1;
	
	/* create the threads with pointer to their client's node */
	pthread_create(&node->public_thread, NULL, public_thread_run, node);
	pthread_create(&node->private_thread, NULL, private_thread_run, node);
	
	return 0;
}

int DB_remove(int pid) {
	//is not necessary. all remove cases are taken care through setting client's alive to 0 */
}

int name_is_invalid(char* name, int name_len) {
	Client_Node *begin = list_head_client;
	printf(" ## name_is_invalid: begin = %d\n", begin);
	if (!begin) {
		return FALSE;
	}
	Client_Node *current = begin;
	sem_wait(current->access_lock);
	do {
		if (strcmp(name, current->name) == 0) {
			return TRUE;
		}
		/* get next */
		struct Client_Node *tmp;
		while (1) {
			tmp = current->next;
			sem_wait(tmp->access_lock);
			
			if (tmp != current->next) {
				sem_post(tmp->access_lock);
				continue;
			}
			break;
		}
		tmp = current;
		current = current->next;
		sem_post(tmp->access_lock);
		
	} while (current != begin);
	
	sem_post(begin->access_lock);
	/* end get next */
	return FALSE;
}


	
/* server fifo thread function 
	open access to server.fifo
	loop read - wait for msg
	parse incoming msg
	open client fifo in
	try to add new client to DB (legal name)
		fail: send error msg to client
			  close fifo in
		succ: send good msg to client
			  
	continue loop
//*/
pthread_t server_fifo_thread;
void* server_fifo_run_thread() {
	int ret;
	char inc_name_len[4] = {0};
	int name_len;
	char inc_pid[4] = {0};
	int pid;
	char *name = NULL;
	
	int gotten = 0;
	int temp;
	puts(" # opening server_fifo...");
	
	ret = mkfifo("server_fifo", 0777);
	printf(" ## mkfifo returned %d\n", ret);
	//TODO handle errors
	int server_fifo = open("server_fifo", O_RDWR);
	printf(" ## server_fifo opened with fd %d.\n", server_fifo);
	server_alive = 1;

	while (server_alive) {
		gotten = read(server_fifo, &inc_name_len, sizeof(char)*(4-gotten));
		if (gotten > 0) {
			while (gotten < 4) {
				temp = read(server_fifo, (inc_name_len + gotten), sizeof(char)*(4-gotten));
				if (temp < 0) {
					continue;
				}
				gotten += temp;
			}
			int name_len = inc_name_len[0]*pow(256,3) + inc_name_len[1]*pow(256,2)+ inc_name_len[2]*256 + inc_name_len[3];
			printf(" # name_len = %d\n", name_len);
			gotten = 0;
			
			while (gotten < 4) {
				temp = read(server_fifo, (inc_pid + gotten), sizeof(char)*(4-gotten));
				if (temp < 0) {
					continue;
				}
				gotten += temp;
			}
			gotten = 0;
			int pid = inc_pid[0]*pow(256,3) + inc_pid[1]*pow(256,2)+ inc_pid[2]*256 + inc_pid[3];
			printf(" # pid = %d\n", pid);
			name_len = 1;
			
			name = (char*)malloc(sizeof(char) * name_len);
			while (gotten < name_len) {
				temp = read(server_fifo, (name + gotten), sizeof(char)*(name_len-gotten));
				if (temp < 0) {
					continue;
				}
				gotten += temp;
			}
			printf(" # name = %s\n", name);
			
			char msg_to_client[15] = {0};
			msg_to_client[3] = 4;
			sprintf(msg_to_client + 4, "/ack");
			
			char fifoIn[25];
			sprintf(fifoIn, "fifo-%d-in", pid);
			puts(" # opening client fifo...");

			int client_fifo_in = open(fifoIn, O_RDWR);
			//TODO check errors
			int success = 1;
			puts (" # validating name...");
			if (name_is_invalid(name, name_len)) {
				puts (" # name is invalid.");
				success = 0;
				msg_to_client[3] = 6;
				sprintf(msg_to_client + 4, "/inuse");
			}

			write(client_fifo_in, msg_to_client, sizeof(char)*(4 + msg_to_client[3]));
			close(client_fifo_in);
			
			if (!success) {
				continue;
			}
			DB_insert(name, pid);
		}
	}
}
	
int main() {

	/* initialize clients database */
	puts("initializing DB...");
	client_list.head = (Client_Node*)malloc(sizeof(Client_Node));
	client_list.head->next = client_list.head;
	client_list.head->prev = client_list.head;
	sem_init(&client_list.head->access_lock, 0, MAX_INT);
	puts("DB init done.");
	
	/* initialize history array */
	puts("initializing history...");
	if (initialize_history() != 0) {
		printf(" ERROR ");
		/* TODO deallocate all allocated memory */
		return 0;
	}
	puts("history init done.");
	
	/* initialize server_fifo */
	puts("creating server_fifo_thread...");
	pthread_create(&server_fifo_thread, NULL, server_fifo_run_thread, NULL);
	puts("server_fifo_thread created.");
	
	/* initialize reader thread */
	pthread_create(&listener_thread, NULL, listener_run_thread, NULL);
	
	/* create server.txt (which means server is on) */
	
	pthread_join(server_fifo_thread, NULL);
	return 0;
}
