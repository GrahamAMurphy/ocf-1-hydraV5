#ifndef __Server_h__
#define __Server_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "stream.h"

class Server : public Stream {
protected:
    u_char *uc_bind_port ;
    bool port_set ;
    int bind_port ;
    int bind_protocol ;
    int bind_fail_exit ;
    int socket_type ;
    int fdnet ;
    int from_port ;
    u_char from_ip[16] ;
    volatile int n_cmds ;
    volatile int server_state ;
    struct sockaddr_in sin ;
    pthread_mutex_t sequencer_mutex ;
    virtual int Parse( u_char*, u_char* ) ;
    void RunThread( void ) ;
    virtual void ServerActive( void ) ;
    int CreateAndBind( void ) ;
    void Unbind( void ) ;
    void AcceptConnection( void ) ;
    void Disconnect( void ) ;
    int SequencerCmd( void ) ;

public:
    Server( void ) ;
    virtual void StartOps( void ) ;
    virtual void PleaseReport( int, int ) ;
} ;

#endif // __Server_h__
