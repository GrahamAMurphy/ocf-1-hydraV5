#define FILE_H_NEED (2)

#pragma implementation

#include <iostream>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "global.h"
#include "queue.h"
#include "stream.h"
#include "support.h"
	#include "client.h"

Stream::Stream()
{
    name = UCP "No name" ;

    int i ;
    for( i = 0 ; i < MAXALIASES ; i++ ) alias[i] = 0 ;
    m_t_thread = 0 ;

    tx_pnt = 0 ;
    rx_pnt = 0 ;
    fd = -1 ;
    devicenum = -1 ;
    devtype = TypeNone ;
    devtypestring = "Unspecified" ;
    r_timeo = 30000 ; // 30.0 secs
    w_timeo = 30000 ; // 30.0 secs
    enabled = true ;
    mStatus = 1000 ;
    stream_state = 0 ;
    errors = 0 ;
    cycle = 1000000000 ;
    delay = -1 ;
    l_errno = 0 ;
    last_rx_timedout = false ;
    flush_after_timeout = false ;
    can_report = true ;

    mQueue = 0 ;
    is_a_client = false ;
    is_a_server = false ;

//  Device server standard of CR-LF for all output.
    iOutputTermS = 0 ;
    iOutputTermL = 2 ;

    iInputTermS = 0 ;
    iInputTermL = 2 ;

    DODBG(9)(DBGDEV, "Exiting stream constructor.\n" ) ;
}

Stream::~Stream()
{
// General purpose base destructor ... no implementation details since life is 
// way too complex.
}

int Stream::SetOptions( u_char* setup )
{
    DODBG(9)(DBGDEV, "%s: %s\n", name, setup ) ;
    u_char w_tmp[2048] ;
    int n_parsed = 0 ;

    if( ! setup ) return( true ) ;

    strncpy( CP w_tmp, CP setup, 2048 ) ;

    sParse parse = { w_tmp, 0, 0 } ;

    while( true ) {
        bool state = GenericParser( parse, UCP ",", UCP "=" ) ;
        if( ! state ) break ;
        if( ! Parse( parse.key, parse.value ) ) return( false ) ;
	n_parsed++ ;
    }

    return( n_parsed ) ;
}

int Stream::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "output-termination" ) == 0 ) {
        if( strcasecmp( CP value, "cr" ) == 0 ) {
	    iOutputTermS = 0 ;
	    iOutputTermL = 1 ;
	} else if( strcasecmp( CP value, "cr-lf" ) == 0 ) {
	    iOutputTermS = 0 ;
	    iOutputTermL = 2 ;
	} else if( strcasecmp( CP value, "lf" ) == 0 ) {
	    iOutputTermS = 1 ;
	    iOutputTermL = 1 ;
	} else if( strcasecmp( CP value, "lf-cr" ) == 0 ) {
	    iOutputTermS = 1 ;
	    iOutputTermL = 2 ;
	} else if( strcasecmp( CP value, "none" ) == 0 ) {
	    iOutputTermS = 0 ;
	    iOutputTermL = 0 ;
	}
	return( true ) ;
    } else if( strcasecmp( CP key, "input-termination" ) == 0 ) {
        if( strcasecmp( CP value, "cr" ) == 0 ) {
	    iInputTermS = 0 ;
	    iInputTermL = 1 ;
	} else if( strcasecmp( CP value, "cr-lf" ) == 0 ) {
	    iInputTermS = 0 ;
	    iInputTermL = 2 ;
	} else if( strcasecmp( CP value, "lf" ) == 0 ) {
	    iInputTermS = 1 ;
	    iInputTermL = 1 ;
	} else if( strcasecmp( CP value, "lf-cr" ) == 0 ) {
	    iInputTermS = 1 ;
	    iInputTermL = 2 ;
	} else if( strcasecmp( CP value, "none" ) == 0 ) {
	    iInputTermS = 0 ;
	    iInputTermL = 0 ;
	}
	return( true ) ;
    }


    return( false ) ;
}

void Stream::setName( const u_char *in_name )
{
    name = (u_char*) strdup( (char*)in_name ) ;
}

void Stream::setAlias( u_char *in_alias )
{
    int i ;
    for( i = MAXALIASES ; i > 1 ; i-- ) {
	alias[i-1] = alias[i-2] ;
    }
    alias[0] = (u_char*) strdup( (char*)in_alias ) ;
}

const u_char *Stream::getAlias( int iSelect = 0 )
{
    if( iSelect < MAXALIASES )
	return( alias[iSelect] ) ;
    else
	return( 0 ) ;
}

const char* Stream::getTypeString( void )
{
    return( devtypestring ) ;
}

void Stream::StartOps( void ) 
{
    DODBG(9)(DBGDEV, "In start ops: %d\n", devicenum ) ;
}

void Stream::StopOps( void ) 
{
    DODBG(9)(DBGDEV, "In stop ops: %d\n", devicenum ) ;
    if( m_t_thread )
	pthread_cancel( m_t_thread ) ;
    m_t_thread = 0 ;
}

void Stream::PleaseReport( int wline, int scol )
{
    for( int i = scol ; i < MAXITEM ; i++ )
	snprintf( gStatus[wline][i], MAXDEVSTR, "Col%d", i ) ;
}

void Stream::RunThread( void )
{
    while( 1 ) {
	DODBG(9)(DBGDEV, "In a dummy running thread for %s\n", name ) ;
	sleep(5) ;
    }
}

void Stream::StartThread( void )
{
    pthread_attr_t t_attribute ;

    DODBG(9)(DBGDEV, "%s: about to create thread.\n", name ) ;
    pthread_attr_init( &t_attribute ) ;
    pthread_create( &m_t_thread, &t_attribute, RunObjectInThread, (void*)&devicenum ) ;
    DODBG(9)(DBGDEV, "%s: created thread.\n", name ) ;
}


// This is generic.

void * RunObjectInThread( void *v_w_device )
{
    int *w_device = static_cast<int *>(v_w_device) ;

    DODBG(5)( DBGDEV, "In RunObjectInThread.\n" ) ;
    if( w_device && streams[*w_device] ) {
	streams[*w_device]->RunThread() ;
    } else {
	DODBG(0)( DBGDEV, "Failed in RunObjectInThread.\n" ) ;
    }
    return( 0 ) ;
}

bool Stream::Active( void )
{
    return( enabled && (fd >= 0) ) ;
}

void Stream::WaitOnSignal( void )
{
    DODBG(9)(DBGDEV, "Waiting %s\n", name ) ;
    sem_wait( &sem_device ) ;
    DODBG(9)(DBGDEV, "Released %s\n", name ) ;
}

void Stream::ClearWait( void )
{
    DODBG(9)(DBGDEV, "Clearing Wait ... %s\n", name ) ;
    for( int i = 0 ; i < 100 ; i++ )
	if( sem_trywait( &sem_device ) ) break ;
    DODBG(9)(DBGDEV, "... done.\n" ) ;
}

void Stream::SignalDevice( void )
{
    DODBG(9)(DBGDEV, "Signalling %s\n", name ) ;
    sem_post( &sem_device ) ;
}

int Stream::GetRxTimedOut( void )
{
    return( last_rx_timedout && flush_after_timeout ) ;
}

int Stream::Exchange( const void *v_out, int wtimeo,
    u_char *in, int maxin, int rtimeo )
{
    bool flag_time_o = true ;
    if( rtimeo < 0 ) {
	rtimeo = -rtimeo ;
	flag_time_o = false ;
    }

    int nexc = DoExchange( v_out, wtimeo, in, maxin, rtimeo ) ;
    if( nexc < 0 ) return( nexc ) ;

    if( nexc > 0 ) {
	if( GetRxTimedOut() ) {
	    DODBG(0)(DBGDEV, "Read of %s following timeout flushed.\n", name ) ;
	    nexc = 0 ;
	}
	ResetRxTimedOut() ;
	return( nexc ) ;
    }

    if( flag_time_o ) {
	DODBG(0)(DBGDEV, "Unexpected timeout reading from %s.\n", name ) ;
	SetRxTimedOut() ;
	errors++ ;
    }
    return( 0 ) ;
}

int Stream::DoExchange( const void *v_out, int wtimeo, u_char *in, int maxin, int rtimeo )
{
    if( in ) *in = '\0' ;

    if( ! FlushStream( fd ) ) return( -1 ) ; // This is grounds for disconnect.

    const u_char *out = (const u_char*)v_out ;

    DODBG(7)(DBGDEV, "%s write: %s\n", name, out ) ;
    int nwrite = WriteStream( out, wtimeo ) ;
    if( nwrite <= 0 ) return( -1 ) ; // This is grounds for disconnecting.

    if( ! in )
	// Nothing to read into, so just return.
	// Return a fake "1" so that we don't trigger an error response.
	return( 1 ) ; 

    int nread = ReadStream( in, maxin, rtimeo ) ;
    if( nread <= 0 ) return( nread ) ;

    // This must be true by virtue of our protocol with labview!
    // if( in[nread-2] == '\r' ) in[nread-2] = '\0' ;
    // in[nread-1] = '\0' ;

    DODBG(7)(DBGDEV, "%s read: Bytes=%d <%s>\n", name, nread, in ) ;
    if( nread >= 5 && strncasecmp( CP in, "ERR 4", 5 ) == 0 ) {
	DODBG(0)(DBGDEV, "Labview returned %s for %s\n", in, name ) ;
	if( strncasecmp( CP in, "ERR 431", 7 ) == 0 ) {
	    fprintf( stderr, "Device server says device not active: %s\n", name ) ;
	    return( -1 ) ;
	}
	// fprintf( stderr, "Labview returned timeout for %s\n", name ) ;
	return( 0 ) ;
    }

    return( nread ) ;
}

int Stream::WriteStream( const u_char *out, int timeo )
{
    int maxs = fd + 1 ;
    struct timeval timeout = { 0, 0 } ;
    fd_set outmask ;
    int o_len = 0 ;
    int nwrite ;

    stream_state = 1 ;

    if( out ) o_len = strlen( CP out ) ;

    if( o_len > 0 ) {
	nwrite = write( fd, CP out, o_len ) ;
	if( nwrite <= 0 ) {
	    perror( CP name ) ;
	    return( -1 ) ;
	}
    }

    if( iOutputTermL > 0 ) {
	nwrite = write( fd, OUTPUT_TERMINATION + iOutputTermS, iOutputTermL ) ;
	if( nwrite <= 0 ) {
	    perror( CP name ) ;
	    return( -1 ) ;
	}
    }

    FD_ZERO(&outmask) ;
    FD_SET( fd, &outmask ) ;

    if( timeo <= 0 ) timeo = 1 ;
    timeout.tv_sec = timeo / 1000 ;
    timeout.tv_usec = (1000*timeo) % 1000000 ;

    stream_state = 2 ;

    int nselect = 0 ;
    while( 1 ) {
	nselect = select( maxs, NULL, &outmask, NULL, &timeout ) ;
	if( nselect < 0 && errno == EINTR ) continue ;
	if( nselect == 1 ) {
	    DODBG(9)(DBGDEV, "Write to %s was successful.\r\n", name ) ;
	    stream_state = 3 ;
	    return( 1 ) ;
	}
	if( nselect == 0 ) {
	    DODBG(0)(DBGDEV, "Write to %s timed out.\r\n", name ) ;
	    // This should get trapped as it is indicates a bad connection.
	    return( 0 ) ;
	}
	l_errno = errno ;
	perror( CP name ) ;
	return( -1 ) ;
    }
}

int Stream::ReadStream( u_char *in, int maxin, int timeo )
{
    int maxs = fd + 1 ;
    struct timeval timeout = { 0, 0 } ;
    struct timeval now, then ;
    fd_set inmask ;
    int pnt = 0 ;

    FD_ZERO(&inmask) ;

    if( timeo <= 0 ) {
	timeout.tv_sec = 10000000 ;
	timeout.tv_usec = 0 ;
    } else {
	timeout.tv_sec = timeo / 1000 ;
	timeout.tv_usec = (1000*timeo) % 1000000 ;
    }

    gettimeofday( &now, 0 ) ;
    TimerAdd( then, now, timeout ) ;

    stream_state = 4 ;

    while( true ) {
	FD_SET( fd, &inmask ) ;
	timeout.tv_sec = 0 ;
	timeout.tv_usec = 100000 ;

	gettimeofday( &now, 0 ) ;

	/*
	need to come up with mode that doesn't add cr-lf to every command!!!
	need new mode that doesn't require cr-lf in every response!!
	*/

	if( timer_cmp( now, >, then ) ) {
	    if( iInputTermL > 0 ) {
		DODBG(1)(DBGDEV, "In ReadStream: timeout.\r\n" ) ;
		return( 0 ) ; // a timeout will be a special case - nothing read.
	    } else {
		stream_state = 5 ;
		in[pnt] = '\0' ;
		return( pnt ) ;
	    }
	}

	int nselect = 0 ;
	while( 1 ) {
	    nselect = select( maxs, &inmask, NULL, NULL, &timeout ) ;
	    if( nselect < 0 && errno == EINTR ) continue ;
	    if( nselect < 0 ) {
		l_errno = errno ;
		perror( CP name ) ;
		return( -1 ) ;
	    }
	    break ;
	}

	if( nselect == 0 ) continue ;

	if( FD_ISSET(fd,&inmask) ) {
	    int nread = read( fd, in+pnt, 1 ) ;
	    if( nread < 0 ) {
		l_errno = errno ;
		perror( CP name ) ;
		stream_state = 5 ;
		return( -1 ) ;
	    } 
	    
	    if( nread == 0 ) continue ; // shouldn't happen, right?
	    // fprintf( stderr, "%02x(%d)[%c]", in[pnt], pnt, in[pnt] ) ;

	    pnt++ ;

	    if( pnt >= iInputTermL && iInputTermL > 0 ) {
		int iq ;
		int im = pnt - iInputTermL ;
		bool iOK = true ;

		for( iq = 0 ; iq < iInputTermL ; iq++, im++ ) {
		    if( in[im] == INPUT_TERMINATION[iInputTermS+iq] ) continue ;
		    iOK = false ; 
		    break ;
		}

		if( iOK ) {
		    stream_state = 5 ;
		    im = pnt - iInputTermL ;

		    for( iq = 0 ; iq < iInputTermL ; iq++, im++ ) {
			in[im] = '\0' ;
		    }

		    return( pnt ) ;
		}
	    }

	    if( pnt >= maxin ) {
		in[pnt] = '\0' ;
		stream_state = 5 ;
		return( pnt ) ;
	    }
	}
    }
} 

