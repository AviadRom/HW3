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
#include "./parsers.h"

#define MAX_INT 2147000000
#define TRUE 1
#define FALSE 0
#define list_is_empty (client_list.head->next == client_list.head)

static char* history[100];
static int hist_head;
static int hist_size;
static sem_t hist_lock;

struct msg_thread;
struct Client_Node;
struct msg_node;
struct msg_queue;
struct Client_List;
struct name_list;
struct name_node;

typedef struct Client_Node Client_Node;

int server_alive;
struct Client_List client_list;

struct msg_thread {
	int th_id;
};

struct msg_node {
	char* msg;
	int msg_len;
	char* reciever;
	struct msg_node* next;
	struct msg_node* prev;
};

struct msg_queue {
	struct msg_node* head;
	struct msg_node* tail;
	int size;
	sem_t queue_lock;
};

struct Client_Node {
	struct Client_Node *next;
	struct Client_Node *prev;
	pid_t pid;
	char* name;
	int nameLen;
	int fifo_in;
	int fifo_out;
	sem_t fifo_in_lock;
	pthread_t private_thread;
	pthread_t public_thread;
	struct msg_queue private_queue;
	struct msg_queue public_queue;
	sem_t access_lock; /* (unlimited access) */
	int alive;
};

struct Client_List {
	struct Client_Node* head;
};




/**************************************msg queue******************************/

void queue_pop(struct msg_queue* queue) {
	struct msg_node *tmp = queue->head->next;
	queue->head->next = tmp->next;
	tmp->next->prev = queue->head->next;
	free(tmp);
}

void queue_insert_msg(struct msg_queue* queue, char* msg, int msg_len, char* reciever){
	struct msg_node* node = malloc(sizeof(struct msg_node));
	node->msg = msg;
	node->msg_len = msg_len;
	node->next = queue->tail;
	node->prev = queue->tail->prev;
    node->reciever = reciever;
	queue->tail->prev->next = node;
	queue->tail->prev = node;
	queue->size++;
}

int init_queue(struct msg_queue* queue) {
	queue = (struct msg_queue*)malloc(sizeof(struct msg_queue));
	queue->head = (struct msg_node*)malloc(sizeof(struct msg_node));
	queue->tail = (struct msg_node*)malloc(sizeof(struct msg_node));
	queue->head->prev = NULL;
	queue->head->next = queue->tail;
	queue->tail->prev = queue->head;
	queue->tail->next = NULL;
	queue->size = 0;
	return sem_init(&queue->queue_lock, 0, 1);
}

void destroy_queue(struct msg_queue* queue){
	int i;
	/* loop wont execute on public & private queues (used only for 'who') */
	for(i = 0; i<queue->size; i++){
		free(queue->head->next->msg);
		queue_pop(queue);
	}
	free(queue->head);
	free(queue->tail);
	sem_destroy(&queue->queue_lock);
	free(queue);
}


/**************************************write / read methods******************************/

int my_write(int fifo, void* source, int len) {
	char* my_source = (char*)source;
	int done = write(fifo, my_source, len);
	while (done < len){
		if (done < 0) {
			printf(" # ERROR: could not send connection message to server.\n");
			break;
		}
		done += write(fifo, (my_source + done) , (len - done)); //make sure we finish writing
	}
}

int my_read(int fifo, void* source, int len) {
	char *my_source = (char*)source;
	int temp;
	int gotten = read(fifo, my_source, len);
	while (gotten < len) {
		if (gotten == 0 && server_alive == 0) {
			return FALSE;
		}
		temp = read(fifo, (my_source + gotten), (len - gotten));
		if (temp < 0) {
			return FALSE;
		}
		gotten += temp;
	}
	return TRUE;
}

/*****************************************************************************/

/**********************************listener_run_thread*************************/
void* who_thread(void* client_node);
void handle_msg(Client_Node* client, char* msg, int msg_len);
void* history_thread(void* client_node);

pthread_t listener_thread;
void* listener_run_thread(){
	char msg_len_arr[4] = {0};
	int gotten = 0;
	int gotten_tmp = 0;
    
	sem_wait(&client_list.head->next->access_lock);
	Client_Node *current = client_list.head;
	Client_Node *tmp = NULL;
    
	while (server_alive){
		if (current != client_list.head){
            my_read (current->fifo_out, msg_len_arr, sizeof(char)*4);
            int* msg_len = (int*)msg_len_arr;
            char* msg = malloc(sizeof(char)* (*msg_len));
            my_read (current->fifo_out, msg, *msg_len);
            handle_msg(current, msg, *msg_len);
        }
    }
try_next:
    tmp = current->next;
    sem_wait(&tmp->access_lock);
    if (tmp != current->next) {
        sem_post(&tmp->access_lock);
        goto try_next;
    }
    tmp = current;
    current = current->next;
    sem_post(&tmp->access_lock);
	return 0;
}

void handle_msg(Client_Node* client, char* msg, int msg_len){
	pthread_t cmd;
    char* formatted_msg;
    int len;
    int reciever_len, i;
	switch(ParseClientMsg(msg)){
        case 0:
            /*public case.*/
            len = 2 + msg_len + strlen(client->name);
            formatted_msg = malloc(sizeof(char) * len);
            sprintf(formatted_msg, "%s: %s", client->name, msg);
            sem_wait(&client->public_queue.queue_lock);
            queue_insert_msg(&client->public_queue, formatted_msg, len, NULL);
            sem_post(&client->public_queue.queue_lock);
            break;
        case 1:
            //private case.
            reciever_len = 0;
            i=1;
            while (msg[i+1] != ' '){
                reciever_len++;
                i++;
            }
            char* reciever = malloc(reciever_len*sizeof(char)+1);
            for (i=1; i <= reciever_len ; i++){
                reciever[i-1] = msg[i];
            }
            reciever[reciever_len] = '\0';
            len = 2 + msg_len + strlen(client->name);
            formatted_msg = malloc(sizeof(char) * len);
            sprintf(formatted_msg, "%s: %s", client->name, msg);
            sem_wait(&client->private_queue.queue_lock);
            queue_insert_msg(&client->private_queue, formatted_msg, len, reciever);
            sem_post(&client->private_queue.queue_lock);
            break;
        case 2:
            //who case.
            pthread_create(&cmd, NULL, who_thread, client);
            break;
        case 3:
            //leave case.
            close(client->fifo_out);
            client->alive = 0;
            break;
            
        case 4:
            //history case.
            pthread_create(&cmd, NULL, history_thread, client);
            break;
	}
    free(msg);
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
	int index;
	int gotten = 0;
	int temp;
	int msg_len;
	Client_Node* client = (Client_Node*)client_node;
	sem_wait(&client->access_lock);
	sem_wait(&hist_lock);
	sem_wait(&client->fifo_in_lock);

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

	sem_post(&client->fifo_in_lock);
	sem_post(&hist_lock);
	sem_post(&client->access_lock);
	return 0;
}

/* who thread handler (client_node)*/
void* who_thread(void* client_node){
	struct msg_queue* whos;
	init_queue(whos);
	Client_Node* client = (Client_Node*)client_node;
	sem_wait(&client->access_lock);

	Client_Node* iterator = client;
	Client_Node* tmp_iter = NULL;
	sem_wait(&iterator->access_lock);
	do{
		int msg_len = 4+5+strlen(iterator->name);
		char* who_msg = malloc(sizeof(char)*msg_len);
		//TODO deal with it convertIntToChars(msg_len-4, who_msg);
		sprintf(who_msg+4, "who: %s", iterator->name);
		queue_insert_msg(whos, who_msg, msg_len, NULL);

		next_iter:
		tmp_iter = iterator->next;
		sem_wait(&tmp_iter->access_lock);
		if(tmp_iter != iterator->next){
			sem_post(&tmp_iter->access_lock);
			goto next_iter;
		}
		tmp_iter = iterator;
		iterator = iterator->next;
		sem_post(&tmp_iter->access_lock);
	} while (iterator != client);
	sem_post(&iterator->access_lock);

	sem_wait(&client->fifo_in_lock);
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

	sem_post(&client->fifo_in_lock);
	sem_post(&client->access_lock);
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

void broadcast(char* msg, int msg_len, Client_Node *node, int toMyself) {
	struct Client_Node *current;
	printf(" ### BROADCAST is running...\n");
	if (toMyself) {
		current = node;
	} else {
		current = node->next;
	}

	do {
		printf(" ### BROADCAST will now access lock %s\n", current->name);
		sem_wait(&current->access_lock);
		/* Skip head node */
		if (current != client_list.head) {
			printf(" ### BROADCAST will now fifo lock %s\n", current->name);
			sem_wait(&current->fifo_in_lock);
				int *len = malloc(sizeof(int));
				*len = msg_len;
				my_write(current->fifo_in, len, 4);
				my_write(current->fifo_in, msg, msg_len);
			sem_post(&current->fifo_in_lock);
		}
		
		struct Client_Node *tmp;
		while (TRUE) {
			tmp = current->next;
			sem_wait(&tmp->access_lock);
			
			if (tmp != current->next) {
				sem_post(&tmp->access_lock);
				continue;
			}
			break;
		}
		tmp = current;
		current = current->next;
		sem_post(&tmp->access_lock);
		
	} while (current != node);
	
	sem_post(&node->access_lock);
	printf(" ### BROADCAST is done for %s\n", node->name);
	document(msg, msg_len);
	/* broadcast should free the msg after sending it */
	free(msg);
}

void* public_thread_run(void* client_node) {//ToDo check with haggai about pointers
	struct Client_Node *my_client = (struct Client_Node*)client_node;
	printf(" # PUBLIC_THREAD(%d) is running.\n", my_client->pid);
	struct msg_queue my_queue = my_client->public_queue;
	/* broadcast to all other clients that my client joined the chat */
	char msg[50];
	sprintf(msg, "%s joined the chat", my_client->name);
	
	printf(" # PUBLIC_THREAD(%d) will now broadcast join msg.\n", my_client->pid);
	broadcast(msg, (my_client->nameLen + 16), my_client, TRUE);
	printf(" # PUBLIC_THREAD(%d) broadcast join is done.\n", my_client->pid);
	
	while (my_client->alive || my_queue.size) {
		if (my_queue.size > 0) {
			sem_wait(&my_queue.queue_lock);
				char* msg_tosend = my_queue.head->next->msg;
				int msg_len = my_queue.head->next->msg_len;
				queue_pop(&my_queue);
			sem_post(&my_queue.queue_lock);
			broadcast(msg_tosend, msg_len, my_client, TRUE);
		}
	}
	
	sprintf(msg, "%s left the chat", my_client->name);
	broadcast(msg, (my_client->nameLen + 14), my_client, FALSE); 	
			/* msg everyone my leave msg, and msg me something special */
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
		sem_getvalue(&my_client->access_lock, &semval);
	}
	sem_destroy(&my_client->access_lock);
	
	/* wait for fifo_in_lock value to be locked (meaning all threads finished writing, and no new threads will get in) */
	sem_wait(&my_client->fifo_in_lock);
	sem_destroy(&my_client->fifo_in_lock);
	
	//ToDo destroy queue locks

	/* delete client from DB (it was actualy removed in the bypass) */
	free(my_client->name);
	free(my_client);
	pthread_exit(NULL);
}

void* private_thread_run(void* client_node) {
	struct Client_Node *my_client = (struct Client_Node*)client_node;
	struct msg_queue my_queue = my_client->private_queue;
	printf(" # PRIVATE_THREAD(%d) is running.\n", my_client->pid);
	
	while (my_client->alive || my_queue.size) {
		if (my_queue.size > 0) {
			sem_wait(&my_queue.queue_lock);
				char* msg_tosend = my_queue.head->next->msg;
				int msg_len = my_queue.head->next->msg_len;
				Client_Node *address = 0;//my_queue.head->next->address;
				queue_pop(&my_queue);
			sem_post(&my_queue.queue_lock);
			send_private(msg_tosend, msg_len, my_client);
		}
	}
	pthread_exit(NULL);
}

/*

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
}*/
int send_private(char* msg_tosend, int msg_len, Client_Node *sender, Client_Node *address) {
	/* I Assume that the MESSAGE was given in the correct format */
	sem_wait(&sender->fifo_in_lock);
		my_write(sender->fifo_in, msg_tosend, msg_len);
	sem_post(&sender->fifo_in_lock);
	
	sem_wait(&address->fifo_in_lock);
		my_write(address->fifo_in, msg_tosend, msg_len);
	sem_post(&address->fifo_in_lock);
}


/* open fifo.out, fifo_in
   create private + public threads (remember to keep a pointer to the client node INSIDE the thread) */
int DB_insert(char* name, int nameLen, int pid) {
	/* create the new node */
	struct Client_Node *node = (struct Client_Node*)malloc(sizeof(struct Client_Node));
	node->pid = pid;
	node->name = name;
	node->nameLen = nameLen;
	
	/* open fifo in */
	char fifoIn[25];
	sprintf(fifoIn, "fifo-%d-in", pid);
	node->fifo_in = open(fifoIn, O_RDWR);
	
	/* open fifo out */
	char fifoOut[25];
	sprintf(fifoOut, "fifo-%d-out", pid);
	node->fifo_out = open(fifoOut, O_RDWR);
	
	/* init locks */
	sem_init(&node->fifo_in_lock, 0, 1);
	sem_init(&node->access_lock, 0, MAX_INT);
	
	/* init queues for private and public threads */
	init_queue(&node->private_queue); //ToDo check return
	init_queue(&node->public_queue); //-"-
	
	/* set client alive BEFORE running threads (cuz they will die without it) */
	node->alive = 1;
	
	printf(" ### DB_insert locking access locks...\n");
	sem_wait(&client_list.head->access_lock);
	sem_wait(&client_list.head->next->access_lock);
		node->next = client_list.head->next;
		node->prev = client_list.head;
		client_list.head->next->prev = node;
		client_list.head->next = node;
	sem_post(&client_list.head->access_lock);
	sem_post(&client_list.head->next->access_lock);
	printf(" ### DB_insert unlocking...\n");
	
	/* create the threads with pointer to their client's node */
	pthread_create(&node->public_thread, NULL, public_thread_run, node);
	pthread_create(&node->private_thread, NULL, private_thread_run, node);
	
	return 0;
}

int DB_remove(int pid) {
	//is not necessary. all remove cases are taken care through setting client's alive to 0 */
}

int name_is_invalid(char* name, int name_len) {
	Client_Node *begin = client_list.head;

	if (list_is_empty) {
		return FALSE;
	}
	Client_Node *current = begin->next;
	sem_wait(&current->access_lock);
	do {
		if (strcmp(name, current->name) == 0) {
			return TRUE;
		}
		/* get next */
		struct Client_Node *tmp;
		while (1) {
			tmp = current->next;
			sem_wait(&tmp->access_lock);
			
			if (tmp != current->next) {
				sem_post(&tmp->access_lock);
				continue;
			}
			break;
		}
		tmp = current;
		current = current->next;
		sem_post(&tmp->access_lock);
		
	} while (current != begin);
	
	sem_post(&begin->access_lock);
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
	char inc_msg_len[4] = {0};
	char inc_pid[4] = {0};
	char *name = NULL;
	
	int gotten = 0;
	int temp;
	puts(" # SERVER_FIFO: opening server_fifo...");
	
	ret = mkfifo("server_fifo", 0777);
	printf(" # SERVER_FIFO: mkfifo returned %d\n", ret);
	//TODO handle errors
	int server_fifo = open("server_fifo", O_RDWR);
	printf(" # SERVER_FIFO: server_fifo opened with fd %d.\n", server_fifo);
	server_alive = 1;
	
	while (server_alive) {
		/* READ FROM CLIENT */
		
		puts(" # SERVER_FIFO: waiting for clients...");
		my_read(server_fifo, inc_msg_len, 4);
		int *nameLen = (int*)inc_msg_len;
		
		my_read(server_fifo, inc_pid, 4);
		int *pid = (int*)inc_pid;
		
		name = (char*)malloc(sizeof(char) * (*nameLen + 1));
		my_read(server_fifo, name, (*nameLen));
		name[*nameLen] = '\0';
		printf(" # SERVER_FIFO: nameLen(%d), pid(%d), name = %s\n", *nameLen, *pid, name);
		
		/* FINISHED READING FROM CLIENT */
		
		int *msg_to_client_len = malloc(sizeof(int));
		*msg_to_client_len = 4;
		char msg_to_client[10] = {0};
		sprintf((msg_to_client), "/ack");
		
		char fifoIn[25];
		sprintf(fifoIn, "fifo-%d-in", *pid);
		printf(" # SERVER_FIFO: opening client fifo (%s)...\n", fifoIn);

		int client_fifo_in = open(fifoIn, O_RDWR);
		//TODO check errors
		int success = 1;
		puts (" # SERVER_FIFO: validating name... (ok if no error printed)");
		if (name_is_invalid(name, *nameLen)) {
			puts (" # SERVER_FIFO: name is invalid.");
			success = 0;
			*msg_to_client_len = 6;
			sprintf(msg_to_client, "/inuse");
		}
		
		printf(" # SERVER_FIFO: writing msg to client %d...\n", *pid);
		my_write(client_fifo_in, msg_to_client_len, 4);
		my_write(client_fifo_in, msg_to_client, *msg_to_client_len);
		close(client_fifo_in);
		puts (" # SERVER_FIFO: done. ");
		if (!success) {
			continue;
		}
	
		puts (" # SERVER_FIFO: inserting new client to DB...");
		DB_insert(name, *nameLen, *pid);
		puts (" # SERVER_FIFO: insertion done.");
	}
	pthread_exit(NULL);
}
	
int main() {

	/* initialize clients database */
	puts(" # SERVER: initializing DB...");
	client_list.head = (Client_Node*)malloc(sizeof(Client_Node));
	client_list.head->next = client_list.head;
	client_list.head->prev = client_list.head;
	sem_init(&client_list.head->access_lock, 0, MAX_INT);
	puts(" # SERVER: DB init done.");
	
	/* initialize history array */
	puts(" # SERVER: initializing history...");
	if (initialize_history() != 0) {
		printf(" ERROR ");
		/* TODO deallocate all allocated memory */
		return 0;
	}
	puts(" # SERVER: history init done.");
	
	/* initialize server_fifo */
	puts(" # SERVER: creating server_fifo_thread...");
	pthread_create(&server_fifo_thread, NULL, server_fifo_run_thread, NULL);
	puts(" # SERVER: server_fifo_thread created.");
	
	/* initialize reader thread */
	pthread_create(&listener_thread, NULL, listener_run_thread, NULL);
	
	/* create server_on (which means server is on) */
	puts(" # SERVER: creating server_on file.");
	FILE* server_on = fopen("server_on", "ab+");
	pthread_join(server_fifo_thread, NULL);
	return 0;
}
