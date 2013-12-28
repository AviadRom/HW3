//
//  client.c
//  

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

int serverFifoActionMsg;
int alive;
int exitFlag;
static char* name;

pthread_t listenerThread;
pthread_t writerThread;

void* FifoWriterThread(void*  arg);

void* FifoListenerThread(void* fifoIn){
    int fifo_in = open((char*)fifoIn, O_RDONLY);
    char msg_len_arr[4] = {0};
    int msg_len;
    int gotten = 0;
    
    while (alive) {
        int res;
        int tmp;
        char* msg;
        //read from fifo
        gotten = read(fifo_in, &msg_len_arr,sizeof(char)*4);
        if (gotten > 0){
            while (gotten < 4){
                tmp = read (fifo_in, &msg_len_arr+gotten, sizeof(char)*(4-gotten));
                if (tmp < 0){
                    continue;
                }
                gotten += tmp;
            }
            msg_len = convertCharsToInt(msg_len_arr);
            msg = malloc(sizeof(char)*msg_len);
            gotten = 0;
            while (gotten <  msg_len){
                tmp = read(fifo_in, msg+gotten, sizeof(char)*(msg_len-gotten));
                if (tmp < 0){
                    continue;
                }
                gotten += tmp;
            }
            res = ParseServerMsg(msg,name);
            if (res == 1){
                pthread_create(&writerThread, (pthread_attr_t *)NULL, FifoWriterThread, NULL);
            }
            else if (res < 0){
                exitFlag = res;
                alive = 0;
            }
        }
    }
    return 0;//close the thread.
}

void* FifoWriterThread(void* arg){
    while (alive) {
        
    }
    return 0;
}

int main(int argc, const char** argv){
    if (argc > 2){
        printf("Client Error: too many arguments\n");
        return -3;
    }

    if (access( "./server.txt", F_OK ) == -1) {
        printf("The chat service is not available\n");
        return -1;
    }
    

    int pid=getpid();
    if  (argc < 2){
        name = malloc(20 * sizeof(char));
        sprintf (name, "pid: %d",pid);
    } else {
        //Add validation of int32 length
        name = malloc((1+strlen(argv[1])) * sizeof(char));
        sprintf(name,"%s",argv[1]);
    }
    
    char fifoIn[25];
    char fifoOut[25];
    sprintf(fifoIn, "./fifo-%d-in", pid);
    sprintf(fifoOut, "./fifo-%d-out", pid);
    
    int err = mkfifo(fifoIn, 0666);
    if(err != 0) //ToDo - Extract function after we make sure it works.
    {
        // most probably file already exists, delete the file
        unlink(fifoIn);
        // try once more..
        err = mkfifo(fifoIn, 0666);
        if(err != 0)
        {
            printf ("Unable to create fifo-in for client.\n");
            return -3;
        }
    }
    err = mkfifo(fifoOut, 0666);
    if(err != 0)
    {
        // most probably file already exists, delete the file
        unlink(fifoOut);
        // try once more..
        err = mkfifo(fifoOut, 0666);
        if(err != 0)
        {
            printf ("Unable to create fifo-out for client.\n");
            return -3;
        }
    }
    alive = 1;
    serverFifoActionMsg = 0;
    
    
    pthread_create(&listenerThread, (pthread_attr_t *)NULL, FifoListenerThread, fifoIn);
    
    char* message; //ToDo Create actual msg
    message = malloc((8+strlen(name))*sizeof(char));
    char msglenC[4] = {0};
    convertIntToChars(strlen(name),msglenC);
    char pidC[4] = {0};
    convertIntToChars(pid,pidC);
    sprintf(message,"%c%c%c%c%c%c%c%c%s",msglenC[0],msglenC[1],msglenC[2],msglenC[3],pidC[0],pidC[1],pidC[2],pidC[3],name);
    int fd = open("./server-fifo", O_WRONLY);
    FILE* serverFifoLock;
    while(1){
        //wait till server fifo is unlocked
        if (access("./server-fifo.locked", F_OK) != 0) {
            //lock server fifo
            serverFifoLock = fopen("./server-fifo.locked", "a");
            break;
        }
    }
    
    int toReturn = 0;
    if(fd > 0)
    {
        int msgLen = strlen(message);
        int done = write(fd, message, msgLen);
        while (done < msgLen){
            //would happen if there was an error in write or if fifo is full.
            if (done < 0) {
                printf("Client error: could not send connection message to server.");
                toReturn = -3;
                alive = 0;
                break;
            }
            done += write(fd, message + done , msgLen-done); //make sure we finish writing
        }
        close(fd);
        fclose(serverFifoLock);
        free(message);
        //unlock server fifo
        unlink("./server-fifo.locked");
    }
    
    while (alive){ }
out:
    free(name);
    unlink(fifoOut);
    unlink(fifoIn);
    return toReturn;
}