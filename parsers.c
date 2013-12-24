//
//  parsers.c
//  

#include "parsers.h"
#include <math.h>


//Parse a message sent from client to server
char* ParseClientMsg(char* msg){
    int len = msg[0]*pow(256,3) + msg[1]*pow(256,2)+ msg[2]*256 + msg[3];
    //TODO
    
}

//Parse a message sent from server to client
int ParseServerMsg(char* msg, char* name){
    int ret;
    if (msg[4] != '/'){
        printf ("%s\n", msg+4);
        ret = 0;
    }
    else if (strcmp (msg+5,"ack") == 0){
        printf("Connected\n");
        ret = 1;
    }
    else if (strcmp (msg+5,"inuse") == 0){
        printf("Client-name %s in use\n",name);
        ret = -2;
    }
    else if (strcmp (msg+5, "leave_ack")){
        ret = -1;
    }
    else if (strcmp (msg+5, "stopped")){
        printf("The chat service was stopped\n");
        ret = -1;
    }
}
