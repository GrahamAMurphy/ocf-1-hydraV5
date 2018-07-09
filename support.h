#ifndef __support_h__
#define __support_h__

#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <search.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "global.h"
#include "logging.h"

void SetSocketOptions( int, int ) ;
int GetPortNumber( u_char* ) ;
int GetProtocol( u_char* ) ;
int GetHostnameInfo( u_char *, struct sockaddr_in *, u_char *, u_char * ) ;
bool GenericParser( sParse &, u_char *, u_char * ) ;
int strchomp( u_char * ) ;
int TopDev( void ) ;
int FlushStream( int, bool = false ) ;
int TokenizeRequest( const u_char *, sCmd & ) ;
void TimerAdd( volatile struct timeval&, volatile struct timeval&, volatile struct timeval& ) ;
void SetThisHost( void ) ;
int mktree( const u_char* ) ;
void SetNameInUT( void ) ;
u_char* GetUptime( void ) ;
void SetCloseOnExec( int ) ;
void SuppressQuotes( u_char* ) ;

#endif // __support_h__
