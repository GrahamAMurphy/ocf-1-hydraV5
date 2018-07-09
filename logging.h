#ifndef __Logging_h__
#define __Logging_h__

#pragma interface

#include <semaphore.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdio.h>

class Logging {
    pthread_mutex_t logging_mutex ;
    FILE *Log ;
    void Write( const void*, const void*, const void* ) ;
    void Lock( void ) ;
    void Unlock( void ) ;
public:
    Logging(const u_char*) ;
    ~Logging() ;
    void L_Write_U( const void *src, const void *level, const void *txt ) ;
    void L_Command_U( const void *src, const void *cmd, const void *rtn ) ;
} ;

EXTERN Logging *StatusLog INITVALUE(0) ;

#define MAX_Q_LOG	(16)

class OCFStatus {
    pthread_mutex_t ocf_logging_mutex ;
    u_char Dir[1024] ;
    int bWriteStatus ;
    pthread_t m_t_timer ;
    sem_t sem_writing ;
    pthread_t m_t_writer ;
    int m_interval ;
    int q_log_a ;
    int q_log_p ;
    int t_requests ;
public:
    OCFStatus( u_char* ) ;
    void StartThread( void ) ;
    void StopOps( void ) ;
    void SetInterval( int ) ;
    void LoggingTimer( void ) ;
    void LoggingWriter( void ) ;
    void AddStatus( int, u_char*, u_char*, u_char* ) ;
    int NoStatus( int ) ;
    void Disconnected( int ) ;
    void Reconnected( int ) ;
    int RequestStatus( int ) ;
} ;

EXTERN OCFStatus *OCF_Status INITVALUE(0) ;

#endif // __Logging_h__
