//
//  parsers.c
//  

#include "parsers.h"
#include <math.h>


void convertIntToChars(int toConvert, char* dest){
    int i, tmp;
    tmp = toConvert;
    for (i=3; i>=0; i--){
        dest[i]= tmp%256;
        tmp /= 256;
    }
}

// convert 4 chars array into integer representation
int convertCharsToInt (char* toConvert){
    int ret = toConvert[0]*pow(256,3) + toConvert[1]*pow(256,2) + toConvert[2]*256 + toConvert[3];
    return ret;
}

/*
 * Parsers assume that given message is a string (ends with '\0')
 */

//Parse a message sent from client to server
int ParseClientMsg(char* msg){
    int ret = 0;
    
    if (msg[0] == '@'){
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
int ParseServerMsg(char* msg, char* name){
    int ret;
    //if i get that i have left the chat - return 9000000
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
