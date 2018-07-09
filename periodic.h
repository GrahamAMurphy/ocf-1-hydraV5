#ifndef __Periodic_h__
#define __Periodic_h__
#pragma interface
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>

const int MAX_P_REQUEST = 256 ;

class Stream ;

class Periodic {
    static const int MaxP = 64 ;
    struct periodic {
	volatile int stream_id ;
	volatile bool setup ;
	volatile bool enabled ;
	volatile bool once ;
	u_char request[MAX_P_REQUEST] ;
	volatile struct timeval run ;
	struct timeval interval ;
    } s_periodic[MaxP] ;
    volatile int mostest ;
    pthread_mutex_t periodic_mutex ;
    void Setup(int, int, const u_char* ) ;

public:
    Periodic() ;
    void CheckAndQueue(void) ;
    int Add(int, const void* ) ;
    void Schedule( int, int ) ;
    void SetPeriod(int, int ) ;
    void Enable( int ) ;
    void Disable( int ) ;
} ;

EXTERN Periodic *PeriodicQ INITVALUE(0) ;

#endif // __Periodic_h__
