//
//  client.c
//  


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "parsers.h"

int serverFifoActionMsg;
int alive;
int exitFlag;

void convertIntToChars(int toConvert, char* dest){
    int i, tmp;
    tmp = toConvert;
    for (i=3; i>=0; i--){
        dest[i]= tmp%256;
        tmp /= 256;
    }
}

void* FifoListenerThread(void* arg){
    while (alive) {
        int res;
        //read from fifo
        res = ParseServerMsg(msg,name);
        if (res == 1){
            pthread_create(&writerThread, (pthread_attr_t *)NULL, FifoWriterThread, NULL);
        }
        else if (ret < 0){
            exitFlag = ret;
            alive = 0;
        }
    }
    return 0;//close the thread.
}

void* FifoWriterThread(void* arg){
    while (alive) {
        
    }
    return 0;
}

int main(int argc, const char*[] argv){
    if (argc > 2){
        printf("Client Error: too many arguments\n");
        return -3;
    }

    if (access( "./server.txt", F_OK ) == -1) {
        printf("The chat service is not available\n");
        return -1;
    }
    
    static char* name;
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
        unlink(fifopath);
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
    serverFifoActionMsg = 0
    unsigned listenerThread;
    static unsigned writerThread;
    
    pthread_create(&listenerThread, (pthread_attr_t *)NULL, FifoListenerThread, NULL);
    
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