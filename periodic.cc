#pragma implementation

#include <string.h>

#include "global.h"
#include "stream.h"
#include "periodic.h"
#include "support.h"

Periodic::Periodic( void )
{
    mostest = 0 ;
    for( int i = 0 ; i < MaxP ; i++ ) {
	s_periodic[i].setup = false ;
	s_periodic[i].enabled = false ;
    }

    pthread_mutexattr_t periodic_mutex_attr ;
    pthread_mutexattr_init( &periodic_mutex_attr ) ;
    pthread_mutexattr_settype( &periodic_mutex_attr, PTHREAD_MUTEX_NORMAL ) ;
    pthread_mutexattr_setprotocol( &periodic_mutex_attr, PTHREAD_PRIO_INHERIT ) ;

    pthread_mutex_init( &periodic_mutex, &periodic_mutex_attr ) ;
    pthread_mutex_lock( &periodic_mutex ) ;
    pthread_mutex_unlock( &periodic_mutex ) ;
}

void Periodic::Setup( int w, int w_id, const u_char *req )
{
    s_periodic[w].stream_id = w_id ;
    s_periodic[w].once = false ;
    s_periodic[w].setup = true ;
    s_periodic[w].enabled = false ;
    strncpy( CP s_periodic[w].request, CP req, MAX_P_REQUEST ) ;
    gettimeofday( (timeval*)(&s_periodic[w].run), 0 ) ;
}

int Periodic::Add( int w_id, const void *v_req )
{
    int next_periodic = -1 ;
    const u_char *req = (const u_char*) v_req ;

    pthread_mutex_lock( &periodic_mutex ) ;
    for( int i = 0 ; i < MaxP ; i++ ) {
	if( s_periodic[i].setup ) continue ;
	next_periodic = i ;
	Setup( i, w_id, req ) ;
	break ;
    }
    if( mostest < next_periodic ) mostest = next_periodic ;
    pthread_mutex_unlock( &periodic_mutex ) ;

    if( next_periodic < 0 ) {
	DODBG(0)(DBGDEV, "Requested periodic. None available!\r\n" ) ;
	return( -1 ) ;
    }
    return( next_periodic ) ;
}

void Periodic::Enable( int w_periodic )
{
    if( w_periodic < 0 || w_periodic >= MaxP ) return ;

    pthread_mutex_lock( &periodic_mutex ) ;
    if( s_periodic[w_periodic].interval.tv_sec >= 0 )
	s_periodic[w_periodic].enabled = true ;
    else
	s_periodic[w_periodic].enabled = false ;
    pthread_mutex_unlock( &periodic_mutex ) ;
}

void Periodic::SetPeriod( int w_period, int period )
{
    if( w_period < 0 || w_period >= MaxP ) return ;

    pthread_mutex_lock( &periodic_mutex ) ;
    s_periodic[w_period].once = ( period == 0 ) ;

    s_periodic[w_period].interval.tv_sec = (period/1000) ;
    s_periodic[w_period].interval.tv_usec = (1000*period) % 1000000 ;
    s_periodic[w_period].enabled = false ;

    if( period < 0 )
	s_periodic[w_period].interval.tv_sec = -1 ;

    pthread_mutex_unlock( &periodic_mutex ) ;
}

void Periodic::Schedule( int w_per, int init_delay )
{
    if( w_per < 0 || w_per >= MaxP ) return ;

    pthread_mutex_lock( &periodic_mutex ) ;

    struct timeval run ;
    gettimeofday( &run, 0 ) ;

    if( init_delay < 0 ) {
	TimerAdd( s_periodic[w_per].run, run, s_periodic[w_per].interval ) ;

    } else {
	int tv_usec = run.tv_usec + 1000*init_delay ;

	s_periodic[w_per].run.tv_usec = tv_usec % 1000000 ;
	s_periodic[w_per].run.tv_sec = run.tv_sec + (tv_usec/1000000) ;
    }

    pthread_mutex_unlock( &periodic_mutex ) ;
}

void Periodic::Disable( int w_periodic )
{
    if( w_periodic < 0 || w_periodic >= MaxP ) return ;

    pthread_mutex_lock( &periodic_mutex ) ;
    s_periodic[w_periodic].enabled = false ;
    pthread_mutex_unlock( &periodic_mutex ) ;
}

void Periodic::CheckAndQueue( void )
{
    struct timeval now ;

    pthread_mutex_lock( &periodic_mutex ) ;
    gettimeofday( &now, 0 ) ;

    for( int i = 0 ; i < MaxP ; i++ ) {
	if( ! s_periodic[i].setup ) continue ;
	if( ! s_periodic[i].enabled ) continue ;
	if( timer_cmp( now, >=, s_periodic[i].run ) ) {
	    s_periodic[i].enabled = false ;
	    int sid = s_periodic[i].stream_id ;
	    int qnum = streams[sid]->mQueue->AddTransaction( s_periodic[i].request, false, s_periodic[i].once? -1 : i ) ;
	    DODBG(8)( DBGDEV, "Periodic AddTransaction: %d\n", qnum ) ;
	    if( qnum < 0 ) {
		DODBG(0)( DBGDEV, "No room on queue for periodic task %s\n", 
		    streams[sid]->getName() ) ;
		continue ;
	    }
	    streams[sid]->SignalDevice() ;

	    if( s_periodic[i].once ) continue ;

	    TimerAdd( s_periodic[i].run, now, s_periodic[i].interval ) ; }
    }

    pthread_mutex_unlock( &periodic_mutex ) ;
}
