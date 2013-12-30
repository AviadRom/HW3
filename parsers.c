//
//  parsers.c
//  

#include "parsers.h"
#include <math.h>





/*
 * Parsers assume that given message is a string (ends with '\0')
 */

//Parse a message sent from client to server
int ParseClientMsg(char* msg){
    int ret = 0;
    
    if (msg[0] == '@' && msg[1] != ' '){
        ret = 1;
    }
    
    else if (msg[0] == '/'){
        if (strcmp(msg+1, "who") == 0){
            ret = 2;
        }
        else if (strcmp(msg+1, "leave") == 0){
            ret = 3;
        }
        else if (strcmp(msg+1, "history") == 0){
            ret = 4;
        }
    }
    return ret;
}

//Parse a message sent from server to client
int ParseServerMsg(char* msg, char* name, int msg_len){
    int ret = -1;
    //if i get that i have left the chat - return 9000000
    if (msg[0] != '/'){
        ret = MSG;
    }
    else if (strcmp (msg,"/ack") == 0){
        ret = ACK;
    }
    else if (strcmp (msg,"/inuse") == 0){
        ret = INUSE;
    }
    else if (strcmp (msg, "/leave_ack")){
        ret = LEAVE;
    }
    else if (strcmp (msg, "/stopped")){
        ret = STOPPED;
    }
	return ret;
}
