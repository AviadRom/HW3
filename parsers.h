//
//  parsers.h
//  


#ifndef _parsers_h
#define _parsers_h

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


void convertIntToChars(int toConvert, char* dest);

int convertCharsToInt (char* toConvert);

//Parse a message sent from client to server
int ParseClientMsg(char* msg);

//Parse a message sent from server to client
int ParseServerMsg(char* msg, char* name);

#endif
