#pragma implementation

#include <string.h>
#include <stdlib.h>

#include "global.h"
#include "stream.h"
#include "queue.h"
#include "periodic.h"

Queue::Queue( void )
{
    pthread_mutexattr_t queue_mutex_attr ;

    pthread_mutexattr_init( &queue_mutex_attr ) ;
    pthread_mutexattr_settype( &queue_mutex_attr, PTHREAD_MUTEX_NORMAL ) ;
    pthread_mutexattr_setprotocol( &queue_mutex_attr, PTHREAD_PRIO_INHERIT ) ;
    pthread_mutex_init( &queue_mutex, &queue_mutex_attr ) ;
    pthread_mutex_lock( &queue_mutex ) ;
    pthread_mutex_unlock( &queue_mutex ) ;

    InitQueue() ;
}

void Queue::InitQueue( void )
{
    pthread_mutex_lock( &queue_mutex ) ;

    max_q_length = 0 ;
    b_queue = 0 ;
    e_queue = MaxQ-1 ; 

    for( int i = 0 ; i < MaxQ ; i++ ) {
	strcpy( CP queues[i].request, "" ) ;
	strcpy( CP queues[i].response, "" ) ;
	queues[i].retval = -1 ;
	pthread_cond_init( &(queues[i].sem_wait), NULL ) ;
	pthread_cond_signal( &(queues[i].sem_wait) ) ;

	pthread_mutexattr_t queues_mutex_attr ;
	pthread_mutexattr_init( &queues_mutex_attr ) ;
	pthread_mutexattr_settype( &queues_mutex_attr, PTHREAD_MUTEX_NORMAL ) ;
	pthread_mutexattr_setprotocol( &queues_mutex_attr, PTHREAD_PRIO_INHERIT ) ;
	pthread_mutex_init( &(queues[i].mx_sem_wait), &queues_mutex_attr ) ;
	// pthread_mutex_init( &(queues[i].mx_sem_wait), NULL ) ;
	pthread_mutex_unlock( &(queues[i].mx_sem_wait) ) ;

	queues[i].mPeriodic = -1 ;
	queues[i].inuse = false ;
    }
    pthread_mutex_unlock( &queue_mutex ) ;
}

int Queue::AddTransaction( u_char *request, bool waiting, int mperiodic )
{
    int retval = -1 ;

    pthread_mutex_lock( &queue_mutex ) ;

    for( int i = 0 ; i < MaxQ ; i++ ) {
	if( queues[b_queue].inuse ) {
	    b_queue = (b_queue + 1) % MaxQ ;
	    continue ;
	}
	strncpy( CP queues[b_queue].request, CP request, MAX_Q_REQUEST ) ;
	strcpy( CP queues[b_queue].response, "" ) ;
	queues[b_queue].retval = 0 ;
	queues[b_queue].inuse = true ;
	queues[b_queue].waiting = waiting ;
	queues[b_queue].mPeriodic = mperiodic ;
	pthread_cond_init( &(queues[b_queue].sem_wait), NULL ) ;
	retval = b_queue ;
	DODBG(9)(DBGDEV, "addtrans: %d %s\n", b_queue, request ) ;
	b_queue = (b_queue + 1) % MaxQ ;
	break ;
    }
    int curr_q_length = 0 ;
    for( int i = 0 ; i < MaxQ ; i++ ) 
	if( queues[i].inuse ) curr_q_length++ ;
    if( max_q_length < curr_q_length ) max_q_length = curr_q_length ;

    if( retval < 0 ) {
	DODBG(0)(DBGDEV, "No room bq=%d eq=%d.\n", b_queue, e_queue ) ;
	for( int i = 0 ; i < MaxQ ; i++ ) {
	    DODBG(0)(DBGDEV, "Queue %d %d %d %d %d <%s>\n",
		i,
		queues[i].retval,
		queues[i].inuse,
		queues[i].waiting,
		queues[i].mPeriodic,
		queues[i].request ) ;
	}
    }

    pthread_mutex_unlock( &queue_mutex ) ;
    return( retval ) ;
}

int Queue::WaitTransaction( int w_queue, int secs )
{
    int status ;
    struct timespec waittime ;
    struct timeval now ;
    gettimeofday( &now, NULL ) ;
    waittime.tv_sec = now.tv_sec + secs ;
    waittime.tv_nsec = now.tv_usec * 1000 ;

    status = pthread_mutex_lock( &(queues[w_queue].mx_sem_wait) ) ;
    status = pthread_cond_timedwait( &(queues[w_queue].sem_wait), &(queues[w_queue].mx_sem_wait), &waittime ) ;
    status = pthread_mutex_unlock( &(queues[w_queue].mx_sem_wait) ) ;

    if( status == 0 ) return( 1 ) ;
    else if( status == ETIMEDOUT ) return( 0 ) ;
    else {
	DODBG(0)(DBGDEV, "pthread_cond_timedwait failed with unexpected error: %d\r\n", status ) ;
	abort() ;
    }
}

void Queue::ReleaseTransaction( int w_queue )
{
    pthread_mutex_lock( &queue_mutex ) ;
    queues[w_queue].inuse = false ;
    if( queues[w_queue].mPeriodic >= 0 ) {
	PeriodicQ->Enable( queues[w_queue].mPeriodic ) ;
	queues[w_queue].mPeriodic = -1 ;
    }
    pthread_mutex_unlock( &queue_mutex ) ;
}

void Queue::ReleaseThisTransaction( void )
{
    pthread_mutex_lock( &queue_mutex ) ;
    DODBG(9)(DBGDEV, "Release %d (P=%d)\n", e_queue, queues[e_queue].mPeriodic);

    queues[e_queue].inuse = false ;
    if( queues[e_queue].mPeriodic >= 0 ) {
	PeriodicQ->Enable( queues[e_queue].mPeriodic ) ;
	queues[e_queue].mPeriodic = -1 ;
    }
    pthread_mutex_unlock( &queue_mutex ) ;
}

int Queue::GetWaiting( void )
{
    DODBG(9)(DBGDEV, "Waiting q=%d is %d.\n", e_queue, queues[e_queue].waiting ) ;
    return( queues[e_queue].waiting ) ;
}

void Queue::GetNextTransaction( void )
{
    pthread_mutex_lock( &queue_mutex ) ;
    e_queue = (e_queue + 1) % MaxQ ;
    DODBG(7)(DBGDEV, "get next: %d %s\n", e_queue, queues[e_queue].request ) ;
    pthread_mutex_unlock( &queue_mutex ) ;
}
const u_char* Queue::GetRequest( void )
{
    return( queues[e_queue].request ) ;
}

void Queue::SetResponse( const void *response )
{
    // To make sure we don't send anything pathological, I'm 
    // trimming at the earliest \r or \n.
    if( response ) {
	u_char *t_r = queues[e_queue].response ;
	strncpy( CP t_r, CP response, MAX_Q_RESPONSE ) ;

	u_char *p_eol = UCP strchr( CP t_r, '\n' ) ;
	if( p_eol ) *p_eol = '\0' ;
	p_eol = UCP strchr( CP t_r, '\r' ) ;
	if( p_eol ) *p_eol = '\0' ;
    } else {
	strncpy( CP queues[e_queue].response, ERR_106_NULL, MAX_Q_RESPONSE ) ;
    }
}

const u_char* Queue::GetResponse( int w_queue )
{
    return( queues[w_queue].response ) ;
}

void Queue::ReleaseWait( void )
{
    DODBG(8)(DBGDEV, "Release for %d\n", e_queue ) ;
    pthread_cond_signal( &(queues[e_queue].sem_wait) ) ;
}

int Queue::MaxQueue( void )
{
    return( max_q_length ) ;
}
