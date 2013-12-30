//
//  parsers.h
//  


#ifndef _parsers_h
#define _parsers_h

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WHO 0
#define	HISTORY 1

#define MSG 2
#define ACK 3
#define INUSE 4
#define LEAVE 5
#define STOPPED 6

void convertIntToChars(int toConvert, char* dest);

int convertCharsToInt (char* toConvert);

//Parse a message sent from client to server
int ParseClientMsg(char* msg);

//Parse a message sent from server to client
int ParseServerMsg(char* msg, char* name, int msg_len);

#endif
