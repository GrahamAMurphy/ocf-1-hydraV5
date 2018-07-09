#ifndef __Client_h__
#define __Client_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"

class Client : public Stream {
    u_char *uc_target ;
    u_char *uc_target_port ;
    u_char *uc_ip ;
    bool ip_set, port_set ;
    int target_port, target_protocol ;
    int socket_type ;
    int from_port ;
    struct sockaddr_in sin ;
    void RunThread( void ) ;
    int SetupDetails(void) ;
    int InitiateTransaction( void ) ;
    int ConnectionStarted( void ) ;
    void ConnectionEnded( void ) ;

protected:
    int retry_time ;
    int connection_good ;
    volatile int disconnects ;
    volatile int retry_ticker ;
    volatile int retry_count ;
    sCmd client_cmd ;
    volatile int client_state ;
    volatile int cmd_time ;
    volatile bool new_timing ;
    void DisplayConnection(void) ;
    virtual int HandleRequest( void ) ;
    virtual int DeviceOpen( void ) ;
    virtual void DeviceClose( void ) ;
    virtual void StartPeriodicTasks( void ) ;
    virtual void StopPeriodicTasks( void ) ;
    virtual int Parse( u_char*, u_char* ) ;
    void AddStatus( int index, u_char* key, u_char* value ) ;
    int NoStatus( int index ) ;

public:
    Client( void ) ;
    virtual void StartOps( void ) ;
    virtual void PleaseReport( int, int ) ;
    virtual int ListOptions( int, u_char *, int ) ;
    void ResetRetry( void ) ;
    int reconnect ;
} ;

#endif // __Client_h__
