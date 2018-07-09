#ifndef __XMLServer_h__
#define __XMLServer_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "server.h"
#include "support.h"

class XMLServer : public Server {
    int nfds ;
    static const int MaxConn = 32 ;
    int fds[MaxConn] ;
    int iInitial[MaxConn] ;
    int iUpdateOnly[MaxConn] ;
    int iCount[MaxConn] ;
    time_t time_last[MaxConn] ;
    pthread_mutex_t xml_mutex ;

    void Deliver( int ) ;
    int GraspGet( int ) ;
    void NumConnections( void ) ;
    int DeliverOne( int, int ) ;
    void ConnectionClose( int ) ;
    void ConnectionOff( int ) ;
    void ExecuteCommand( u_char* ) ;
    void Loop( void ) ;
    void WireConnection( void ) ;
    void ReceiveConnection( int ) ;
    time_t created[MaxConn] ;
    void MeetRequest( int ) ;
public:
    XMLServer() ;
    ~XMLServer() ;
    void RunThread( void ) ;
    void Preamble( void ) ;
    void Body( u_char*, u_char*, u_char*, u_char*, int, int ) ;
    void Postamble( void ) ;
    void BuildStatus( int ) ;
    void PleaseReport( int, int ) ;
} ;

EXTERN XMLServer *XML INITVALUE(0) ;
#endif // __XMLServer_h__
