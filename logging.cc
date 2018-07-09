#pragma implementation

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <iostream>

#include "global.h"
#include "support.h"
#include "logging.h"
#include "httpserver.h"
#include "xmlserver.h"

Logging::Logging( const u_char *prefix )
{
    u_char dirname[1024] ;
    u_char filename[1024] ;
    u_char *p_dir ;

    if( prefix == NULL ) {
	Log = NULL ;
    } else {

	strncpy( CP dirname, CP prefix, 1024 ) ;
	p_dir = UCP strrchr( CP dirname, '/' ) ;

	if( p_dir ) {
	    *p_dir = '\0' ;
	    if( mktree( dirname ) ) {
		perror( CP dirname ) ;
		return ;
	    }
	    *p_dir = '/' ;
	}

	snprintf( CP filename, 1024, "%s.%s", prefix, NameInUT ) ;

	if( log_suffix ) 
	    snprintf( CP filename, 1024, "%s%s", prefix, log_suffix ) ;
	Log = fopen( CP filename, "a" ) ;
	if( Log ) 
	    setvbuf( Log, 0, _IONBF, 0 ) ;
    }
    if( Log == NULL )
	DODBG(0)(DBGDEV, "Transaction logging NOT ENABLED.\r\n" ) ;

    pthread_mutexattr_t logging_mutex_attr ;
    pthread_mutexattr_init( &logging_mutex_attr ) ;
    pthread_mutexattr_settype( &logging_mutex_attr, PTHREAD_MUTEX_NORMAL ) ;
    pthread_mutexattr_setprotocol( &logging_mutex_attr, PTHREAD_PRIO_INHERIT ) ;

    pthread_mutex_init( &logging_mutex, &logging_mutex_attr ) ;
    pthread_mutex_lock( &logging_mutex ) ;
    pthread_mutex_unlock( &logging_mutex ) ;
}


Logging::~Logging( void )
{
    if( Log )
	fclose( Log ) ;
}

void Logging::Lock( void )
{
    pthread_mutex_lock( &logging_mutex ) ;
}

void Logging::Unlock( void )
{
    pthread_mutex_unlock( &logging_mutex ) ;
}

void Logging::Write( const void *src, const void *level, const void *txt )
{
    time_t now ;
    time( &now ) ;
    struct tm *ut = gmtime( &now ) ;

    if( ! Log ) return ;

    fprintf( Log, "UT %04d-%02d-%02d %02d:%02d:%02d: %-4.4s (%s) %s\r\n",
	ut->tm_year+1900,
	ut->tm_mon+1,
	ut->tm_mday,
	ut->tm_hour,
	ut->tm_min,
	ut->tm_sec,
	CP level, CP src, CP txt ) ;
    fflush( Log ) ;
}

void Logging::L_Write_U( const void *src, const void *level, const void *txt )
{
    Lock() ;
    Write( src, level, txt ) ;
    Unlock() ;
} ;

void Logging::L_Command_U( const void *src, const void *cmd, const void *rtn )
{
    Lock() ;
    Write( src, "CMD", cmd ) ;
    Write( src, "RTN", rtn ) ;
    Unlock() ;
} ;

// END OF LOGGING.

#define MAXKEYS		(256)
#define MAXWIDTH	(128)

static struct status_log {
    u_char *names[MAXKEYS] ;
    u_char status[MAXKEYS][MAXWIDTH] ;
    int l_name[MAXKEYS] ;
    int l_key[MAXKEYS] ;
    int l_value[MAXKEYS] ;
    int n_attempts[MAXKEYS] ;
    int updated[MAXKEYS] ;
    double set_time[MAXKEYS] ;
    int inuse ;
    int wasused ;
    long t_log_s, t_log_us ;
    double d_t_log ;
} s_status[MAX_Q_LOG] ;

OCFStatus::OCFStatus( u_char *inDir )
{

    if( ! inDir ) {
	bWriteStatus = false ;
	strcpy( CP Dir, "" ) ;

	// DODBG(0)( DBGDEV, "No OCF status logging directory given.\r\n" ) ;
	// exit(0) ;
    } else if( access( CP inDir, F_OK ) ) { // Not found.

	DODBG(0)( DBGDEV, "OCF status logging directory not found.\n" ) ;
	DODBG(0)( DBGDEV, "Will try to create directory: %s.\n", inDir ) ;
	int status = mktree( inDir ) ;
	if( status ) {
	    DODBG(0)( DBGDEV, "Could not create directory: %s.\n", inDir ) ;
	    exit(0) ;
	} else {
	    strncpy( CP Dir, CP inDir, 1024 ) ;
	}

	bWriteStatus = true ;

    } else {
	if( access( CP inDir, (X_OK|W_OK|R_OK) ) ) { 
	    DODBG(0)( DBGDEV, 
	    "Permissions for OCF status logging directory(%s) not as needed.\n",
		inDir) ;
	    exit(0) ;
	} else {
	    strncpy( CP Dir, CP inDir, 1024 ) ;
	}

	bWriteStatus = true ;
    }

    if( bWriteStatus ) {
	DODBG(0)( DBGDEV, "OCF status logging directory is: %s.\n", Dir );
    } else {
	DODBG(0)( DBGDEV, "OCF status logging is NOT ENABLED.\n" ) ;
    }

    m_interval = 100 ;

    for( int i = 0 ; i < MAX_Q_LOG ; i++ ) {
	memset( s_status[i].status, 0, MAXKEYS*MAXWIDTH ) ;
	for( int j = 0 ; j < MAXKEYS ; j++ ) {
	    s_status[i].l_key[j] = 0 ;
	    s_status[i].l_value[j] = 0 ;
	    s_status[i].names[j] = 0 ;
	    s_status[i].l_name[j] = 0 ;
	    s_status[i].n_attempts[j] = 0 ;
	    s_status[i].updated[j] = 0 ;
	}
	s_status[i].inuse = false ;
	s_status[i].wasused = false ;
    }
    sem_init( &sem_writing, 0, 0 ) ;
    q_log_a = 0 ;
    q_log_p = 0 ;
    t_requests = 1 ;

    m_t_timer = 0 ;
    m_t_writer = 0 ;

    pthread_mutexattr_t logging_mutex_attr ;
    pthread_mutexattr_init( &logging_mutex_attr ) ;
    pthread_mutexattr_settype( &logging_mutex_attr, PTHREAD_MUTEX_NORMAL ) ;
    pthread_mutexattr_setprotocol( &logging_mutex_attr, PTHREAD_PRIO_INHERIT ) ;

    pthread_mutex_init( &ocf_logging_mutex, &logging_mutex_attr ) ;
    pthread_mutex_lock( &ocf_logging_mutex ) ;
    pthread_mutex_unlock( &ocf_logging_mutex ) ;
}

void OCFStatus::SetInterval( int interval )
{
    m_interval = interval ;
}

void *RunOCFTimer( void * not_used )
{
    OCF_Status->LoggingTimer() ;
    return( 0 ) ;
}

void *RunOCFWriter( void * not_used )
{
    OCF_Status->LoggingWriter() ;
    return( 0 ) ;
}

void OCFStatus::StartThread( void )
{
    pthread_attr_t t_attribute ;

    DODBG(9)( DBGDEV, "OCF_Status: about to create thread.\n" ) ;

    pthread_attr_init( &t_attribute ) ;
    pthread_create( &m_t_timer, &t_attribute, RunOCFTimer, 0 ) ;

    sleep(1) ;

    pthread_attr_init( &t_attribute ) ;
    pthread_create( &m_t_writer, &t_attribute, RunOCFWriter, 0 ) ;

    DODBG(9)( DBGDEV, "OCF_Status: created thread.\n" ) ;
}

void OCFStatus::StopOps( void )
{
    DODBG(9)( DBGDEV, "OCF_Status: destroying threads.\n" ) ;
    if( m_t_timer )
	pthread_cancel( m_t_timer ) ;
    if( m_t_writer ) ;
	pthread_cancel( m_t_writer ) ;
}

int OCFStatus::RequestStatus( int num_log )
{
    int retval ;

    pthread_mutex_lock( &ocf_logging_mutex ) ;
    int f_request = t_requests ;
    if( (t_requests+num_log) > MAXKEYS ) {
	retval = -1 ;
    } else {
	t_requests += num_log ;
	retval = f_request ;
    }
    pthread_mutex_unlock( &ocf_logging_mutex ) ;
    return( retval ) ;
}

void OCFStatus::AddStatus( int w_log, u_char *name, u_char *key, u_char *value )
{
    pthread_mutex_lock( &ocf_logging_mutex ) ;

    if( w_log > 0 && w_log < t_requests ) {
	int l_key ;
	s_status[q_log_a].wasused = true ;
	strncpy( CP s_status[q_log_a].status[w_log], CP key, MAXWIDTH ) ;
	s_status[q_log_a].l_key[w_log] = l_key = strlen( CP key ) ;

	if( value ) {
	    strncpy( CP s_status[q_log_a].status[w_log] + l_key + 1, 
		CP value, MAXWIDTH-l_key-1 ) ;
	    s_status[q_log_a].l_value[w_log] = strlen( CP value ) ;
	} else {
	    s_status[q_log_a].l_value[w_log] = 0 ;
	}

	s_status[q_log_a].names[w_log] = name ;
	if( name )
	    s_status[q_log_a].l_name[w_log] = strlen( CP name ) ;
	else
	    s_status[q_log_a].l_name[w_log] = 0 ;

	s_status[q_log_a].n_attempts[w_log] = 0 ;
	s_status[q_log_a].updated[w_log] = 1 ;

	struct timeval now ;
	gettimeofday( &now, 0 ) ;

	s_status[q_log_a].set_time[w_log] = (double)now.tv_sec + 1e-6 * now.tv_usec ;
    }

    pthread_mutex_unlock( &ocf_logging_mutex ) ;
}


int OCFStatus::NoStatus( int w_log )
{
    int retval = -1 ;
    pthread_mutex_lock( &ocf_logging_mutex ) ;

    if( w_log > 0 && w_log < t_requests ) {
	s_status[q_log_a].wasused = true ;
	if( s_status[q_log_a].n_attempts[w_log] < 0 ) {
	    retval = s_status[q_log_a].n_attempts[w_log] = 1 ;
	} else {
	    retval = ++s_status[q_log_a].n_attempts[w_log] ;
	}
	s_status[q_log_a].updated[w_log] = 0 ;
    }

    pthread_mutex_unlock( &ocf_logging_mutex ) ;
    return( retval ) ;
}

void OCFStatus::Disconnected( int w_log )
{
    pthread_mutex_lock( &ocf_logging_mutex ) ;

    if( w_log > 0 && w_log < t_requests ) {
	s_status[q_log_a].n_attempts[w_log] = -1 ;
    }
    pthread_mutex_unlock( &ocf_logging_mutex ) ;
}

void OCFStatus::Reconnected( int w_log )
{
    pthread_mutex_lock( &ocf_logging_mutex ) ;

    if( w_log > 0 && w_log < t_requests ) {
	s_status[q_log_a].n_attempts[w_log] = 0 ;
    }
    pthread_mutex_unlock( &ocf_logging_mutex ) ;
}

void OCFStatus::LoggingTimer( void )
{
    struct timeval tv ;
    static struct timeval tv_next ;
    static int fails = 0 ;

    gettimeofday( &tv, 0 ) ;
    tv_next.tv_sec = tv.tv_sec + 2 + m_interval ;
    tv_next.tv_usec = 0 ;

    while( 1 ) {
	gettimeofday( &tv, 0 ) ;
	DODBG(11)(DBGDEV, "Logging process:  %15ld.%06ld  [%15ld]\n", tv.tv_sec, tv.tv_usec, tv_next.tv_sec ) ;
	if( tv.tv_sec < tv_next.tv_sec ) {
	    long d_secs = tv_next.tv_sec - tv.tv_sec ;
	    long d_usecs = (tv_next.tv_usec - tv.tv_usec) ;
	    d_usecs += 1000000 * d_secs + 1000 ; /* add 0.001 secs */
	    struct timespec need_sleep ;
	    d_secs = d_usecs / 1000000 ;
	    need_sleep.tv_sec = d_secs ;
	    need_sleep.tv_nsec = 1000*( d_usecs - 1000000 * d_secs ) ;
	    DODBG(11)(DBGDEV, "Logging process: sleep for: %15ld.%09ld\n", need_sleep.tv_sec, need_sleep.tv_nsec ) ;
	    nanosleep( &need_sleep, 0 ) ;
	    continue ;
	}
	// tv_next.tv_sec += m_interval ;
	if( (tv.tv_sec - tv_next.tv_sec) > 4 ) {
	    DODBG(0)(DBGDEV, "WARNING  Logging process:  %15ld.%06ld  [%15ld]\n", tv.tv_sec, tv.tv_usec, tv_next.tv_sec ) ;
	    fails++ ;
	}

	pthread_mutex_lock( &ocf_logging_mutex ) ;

	gettimeofday( &tv, 0 ) ;
	tv_next.tv_sec = tv.tv_sec + m_interval ;
	DODBG(11)(DBGDEV, "Logging unlock:  %15ld.%06ld\n", tv.tv_sec, tv.tv_usec ) ;

#if 1
	if( 1 ) {
	    static int tvd_last = 0 ;
	    static int iFails = 0 ;

	    if( tvd_last != tv.tv_sec ) {
		tvd_last = tv.tv_sec ;
		if( fails > 0 ) iFails++ ;
		fails = 0 ;

		DODBG(11)(DBGDEV, "Queue: %5ld %6d ", (tv.tv_sec%10000), iFails ) ;
		for( int j = 0, k = q_log_a ; j < MAX_Q_LOG ; j++ ) {
		    DODBG(11)(DBGDEV, "%1d ",  s_status[k].inuse ) ;
		    k = (k+1) % MAX_Q_LOG ;
		}
		DODBG(11)(DBGDEV, "\n" ) ;
	    }
	}
#endif

	int q_log_n = (q_log_a + 1) % MAX_Q_LOG ;

	if( s_status[q_log_n].inuse ) {
	    DODBG(0)(DBGDEV, "Warning: Status logging queue full!!  %15ld.%06ld\n", tv.tv_sec, tv.tv_usec ) ;
	    fails++ ;
	    pthread_mutex_unlock( &ocf_logging_mutex ) ;
	    sem_post( &sem_writing ) ;
	    continue ;
	}

	memcpy( s_status[q_log_n].status, s_status[q_log_a].status,
	    MAXKEYS*MAXWIDTH ) ;
	for( int j = 0 ; j < MAXKEYS ; j++ ) {
	    s_status[q_log_n].l_key[j] = s_status[q_log_a].l_key[j] ;
	    s_status[q_log_n].l_value[j] = s_status[q_log_a].l_value[j] ;
	    s_status[q_log_n].set_time[j] = s_status[q_log_a].set_time[j] ;
	    s_status[q_log_n].names[j] = s_status[q_log_a].names[j] ;
	    s_status[q_log_n].l_name[j] = s_status[q_log_a].l_name[j] ;
	    s_status[q_log_n].n_attempts[j] = s_status[q_log_a].n_attempts[j] ;
	    s_status[q_log_n].updated[j] = 0 ;
	}

#if 0
	if( s_status[q_log_n].inuse ) {
	    DODBG(0)(DBGDEV, "Warning: Status logging queue full!!  %15d.%06d\n", tv.tv_sec, tv.tv_usec ) ;
	    fails++ ;
	    pthread_mutex_unlock( &ocf_logging_mutex ) ;
	    sem_post( &sem_writing ) ;
	    continue ;
	}
#endif

	s_status[q_log_n].wasused = false ;

	s_status[q_log_a].inuse = true ;
	s_status[q_log_a].t_log_s = tv.tv_sec ;
	s_status[q_log_a].t_log_us = tv.tv_usec ;
	s_status[q_log_a].d_t_log = (double)tv.tv_sec + 1e-6*tv.tv_usec ;

	struct tm *ut = gmtime( &tv.tv_sec ) ;

	double currMET = tv.tv_sec + 1e-6*tv.tv_usec - EpochMET ;
	int tm_dsec = (int)( tv.tv_usec / 100000 ) ;

	snprintf( CP s_status[q_log_a].status[0], MAXWIDTH, 
	    "%04d-%02d-%02dT%02d:%02d:%02d.%01d UTC Up=%s MET=%.3f V%s",
	    ut->tm_year+1900,
	    ut->tm_mon+1,
	    ut->tm_mday,
	    ut->tm_hour,
	    ut->tm_min,
	    ut->tm_sec,
	    tm_dsec,
	    GetUptime(),
	    currMET,
	    cVersion ) ;
	s_status[q_log_a].l_key[0] = strlen( CP s_status[q_log_a].status[0] );
	s_status[q_log_a].l_value[0] = 0 ;
	s_status[q_log_a].names[0] = 0 ;
	s_status[q_log_a].updated[0] = 1 ;

	q_log_a = q_log_n ;

	pthread_mutex_unlock( &ocf_logging_mutex ) ;

	sem_post( &sem_writing ) ;
	DODBG(11)(DBGDEV, "%.3f\r\n", currMET ) ;
	gettimeofday( &tv, 0 ) ;
	DODBG(11)(DBGDEV, "Logging end:  %15ld.%06ld\n", tv.tv_sec, tv.tv_usec ) ;
    }
}

void OCFStatus::LoggingWriter( void )
{
    while( 1 ) {
	sem_wait( &sem_writing ) ;
	while( true ) {
	    pthread_mutex_lock( &ocf_logging_mutex ) ;
	    int state = (q_log_a == q_log_p ) ;
	    pthread_mutex_unlock( &ocf_logging_mutex ) ;
	    if( state ) break ;

	    DODBG(10)(DBGDEV, "Writing log %d\n", q_log_p ) ;
	    if( s_status[q_log_p].wasused ) {
		DODBG(11)(DBGDEV, "Something to print %d\n", q_log_p ) ;
		struct tm *ut = gmtime( &(s_status[q_log_p].t_log_s) ) ;

		if( HTTP ) HTTP->Preamble() ;
		if( XML ) XML->Preamble() ;

		int fd = -1 ;
		u_char j_dir[1024] ;
		if( bWriteStatus ) {

		    if( ! status_file ) 
			snprintf( CP j_dir, 1024, "%s/%04d.%03d/%02d", 
			    Dir, 
			    ut->tm_year+1900,
			    ut->tm_yday+1, ut->tm_hour ) ;
		    else
			snprintf( CP j_dir, 1024, "%s", Dir ) ;

		    int status = mktree( j_dir ) ;
		    if( status ) {
			perror( CP j_dir ) ;
			DODBG(0)(DBGDEV, "Failed to create logging dir.\n" ) ;
			exit(0) ;
		    }

		    u_char j_file[1024] ;
		    long secs_of_hour = s_status[q_log_p].t_log_s % 3600 ;

		    if( status_file )
			snprintf( CP j_file, 1024, "%s/%s", j_dir, status_file ) ;
		    else
			snprintf( CP j_file, 1024, "%s/%02ld.%02ld", 
			    j_dir, secs_of_hour/60, secs_of_hour%60 );



		    fd = open( CP j_file, O_WRONLY|O_CREAT|O_APPEND, 0666 ) ;
		    if( fd < 0 ) {
			perror( CP j_file ) ;
			DODBG(0)(DBGDEV, "Failed to create status log file: %s\n", j_file ) ;
			// break ;
		    } else {
			SetCloseOnExec( fd ) ;
		    }

		}

		for( int i = 0 ; i < t_requests ; i++ ) {
		    int l_key = s_status[q_log_p].l_key[i] ;
		    if( l_key == 0 ) continue ;

		    int l_name = s_status[q_log_p].l_name[i] ;
		    int l_value = s_status[q_log_p].l_value[i] ;
		    u_char *p_name = s_status[q_log_p].names[i] ;
		    u_char *p_key = s_status[q_log_p].status[i] ;
		    u_char *p_value = (l_value) ? p_key+l_key+1 : 0 ;

		    int n_attempts = s_status[q_log_p].n_attempts[i] ;
		    u_char timp[MAXWIDTH] ;
		    if( n_attempts < 0 ) {
			snprintf( CP timp, MAXWIDTH, "%11.3f s (disc)",
			    s_status[q_log_p].d_t_log - 
			    s_status[q_log_p].set_time[i] ) ;
		    } else if( n_attempts == 0 ) {
			snprintf( CP timp, MAXWIDTH, "%11.3f s",
			    s_status[q_log_p].d_t_log - 
			    s_status[q_log_p].set_time[i] ) ;
		    } else {
			snprintf( CP timp, MAXWIDTH, "%11.3f s (%d fail%s)",
			    s_status[q_log_p].d_t_log - 
			    s_status[q_log_p].set_time[i],
			    n_attempts,
			    n_attempts>1?"s":"" ) ;
		    }

		    if( bWriteStatus ) {

			if( ! p_name ) {
			    write( fd, p_key, l_key ) ;
			} else {
			    write( fd, "<", 1 ) ;
			    write( fd, p_name, l_name ) ;
			    write( fd, "> <", 3 ) ;
			    write( fd, p_key, l_key ) ;
			    write( fd, "> ", 2 ) ;

			    if( l_value ) {
				write( fd, "= <", 3 ) ;
				write( fd, p_value, l_value ) ;
				write( fd, "> ", 2 ) ;
			    }
			    write( fd, timp, strlen(CP timp) ) ;
			}
			write( fd, "\r\n", 2 ) ;

		    }

		    if( HTTP || XML ) {
			if( p_name ) SuppressQuotes( p_name ) ;
			if( p_key ) SuppressQuotes( p_key ) ;
			if( p_value ) SuppressQuotes( p_value ) ;
		    }

		    if( HTTP ) {
			HTTP->Body( p_name, p_key, p_value, timp, n_attempts, s_status[q_log_p].updated[i] ) ;
		    }
		    if( XML ) {
			XML->Body( p_name, p_key, p_value, timp, n_attempts, s_status[q_log_p].updated[i] ) ;
		    }
		}
		if( HTTP ) HTTP->Postamble() ;
		if( XML ) XML->Postamble() ;

		if( fd >= 0 )
		    close( fd ) ;

	    } else {
		DODBG(11)(DBGDEV, "Nothing to print %d\n", q_log_p ) ;
	    }
	    DODBG(11)(DBGDEV, "Finished writing log %d\n", q_log_p ) ;
	    s_status[q_log_p].inuse = false ;
	    s_status[q_log_p].wasused = false ;
	    q_log_p = (q_log_p + 1) % MAX_Q_LOG ;
	}
    }
}
