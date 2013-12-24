//
//  parsers.h
//  


#ifndef _parsers_h
#define _parsers_h

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//Parse a message sent from client to server
char* ParseClientMsg(char* msg);

//Parse a message sent from server to client
char* ParseServerMsg(char* msg);

#endif
