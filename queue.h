#ifndef __Queue_h__
#define __Queue_h__
#pragma interface
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>

const int MAX_Q_REQUEST = 256 ;
const int MAX_Q_RESPONSE = 256 ;

class Queue {
    pthread_mutex_t queue_mutex ;
    static const int MaxQ = 8 ;
    struct queue {
	u_char request[MAX_Q_REQUEST] ;
	u_char response[MAX_Q_RESPONSE] ;
	pthread_cond_t sem_wait ;
	pthread_mutex_t mx_sem_wait ;
	volatile int retval ;
	volatile bool inuse, waiting ;
	volatile int mPeriodic ;
    } queues[MaxQ] ;
    volatile int b_queue, e_queue ;
    volatile int max_q_length ;
public:
    Queue() ;
    void InitQueue( void ) ;
    int AddTransaction( u_char*, bool, int = -1 ) ;
    int WaitTransaction( int, int ) ;
    void GetNextTransaction( void ) ;
    const u_char * GetRequest( void ) ;
    int GetWaiting( void ) ;
    void SetResponse( const void* ) ;
    void ReleaseWait( void ) ;
    void ReleaseTransaction( int ) ;
    void ReleaseThisTransaction( void ) ;
    const u_char * GetResponse( int ) ;
    int MaxQueue( void ) ;
} ;

#endif // __Queue_h__
