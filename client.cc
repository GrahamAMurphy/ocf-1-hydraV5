#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>

#include <iostream>

#pragma implementation

#include "global.h"
#include "stream.h"
#include "client.h"
#include "support.h"

Client::Client( void )
{
    ip_set = false ;
    port_set = false ;
    retry_time = 60 ;
    retry_ticker = 0 ;
    retry_count = 0 ;
    disconnects = 0 ;
    devtype = TypeClient ;
    devtypestring = "Client" ;
    r_timeo = 20000 ; // 20.0 secs
    w_timeo = 20000 ; // 20.0 secs

    target_port = 0 ;
    uc_ip = 0 ;
    connection_good = false ;
    reconnect = false ;
    can_report = true ;

    client_state = 0 ;
    n_cmds = 0 ;
    cmd_time = 0 ;
    new_timing = true ;

    sem_init( &sem_device, 0, 0 ) ;
    mQueue = new Queue() ;
    is_a_client = true ;

    DODBG(9)(DBGDEV, "Leaving client constructor.\n" ) ;
}

void Client::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: client being started up.\n", name ) ;
    StartThread() ;
}

int Client::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "target" ) == 0 ) {
	if( test_target_0 && strcasecmp( CP value, "TestHost0" ) == 0 ) {
	    value = test_target_0 ;
	}
	uc_target = UCP strdup( CP value ) ;
	ip_set = true ;
    } else if( strcasecmp( CP key, "port" ) == 0 ) {
	uc_target_port = UCP strdup( CP value ) ;
	port_set = true ;
    } else if( strcasecmp( CP key, "retry" ) == 0 ) {
	int t_retry_time = strtol( CP value, NULL, 0 ) ;
	if( t_retry_time > 0 ) retry_time = t_retry_time ;
    } else if( strcasecmp( CP key, "timeout/write" ) == 0 ) {
	double d_t_o_w = strtod( CP value, NULL ) ;
	if( d_t_o_w > 0 ) w_timeo = (int)(d_t_o_w * 1000) ;
    } else if( strcasecmp( CP key, "timeout/read" ) == 0 ) {
	double d_t_o_r = strtod( CP value, NULL ) ;
	if( d_t_o_r > 0 ) r_timeo = (int)(d_t_o_r * 1000) ;
    } else if( strcasecmp( CP key, "cycle" ) == 0 ) {
	double d_cycle = strtod( CP value, NULL ) ;
	if( d_cycle >= 0 ) cycle = (int)(d_cycle * 1000) ;
	else cycle = -1 ;
    } else if( strcasecmp( CP key, "delay" ) == 0 ) {
	double d_delay = strtod( CP value, NULL ) ;
	if( d_delay >= 0 ) delay = (int)(d_delay * 1000) ;
    } else {
	return( Stream::Parse( key, value ) ) ;
    }
    return( true ) ;
}

int Client::SetupDetails( void )
{
    target_port = GetPortNumber( uc_target_port ) ;
    if( target_port < 0 ) {
	DODBG(0)(DBGDEV, "Bad port number.\n" ) ;
	return( false ) ;
    }

    target_protocol = GetProtocol( uc_target_port ) ;
    if( target_protocol < 0 ) {
	DODBG(0)(DBGDEV, "Bad protocol.\n" ) ;
	return( false ) ;
    }

    socket_type = (target_protocol == IPPROTO_UDP) ? SOCK_DGRAM : SOCK_STREAM ;

    fd = socket( AF_INET, socket_type, target_protocol ) ;
    if( fd < 0 ) {
	perror( CP name ) ;
	DODBG(0)(DBGDEV, "Client socket could not be opened.\n" ) ;
	return( false ) ;
    }

    SetCloseOnExec( fd ) ;

    DODBG(4)(DBGDEV, "Client %s socket: fd=%d port=%d\n", 
	name, fd, target_port );

    int status ;
    u_char full[256], ip[256] ;

    DODBG(4)( DBGDEV,
        "%s: will attempt connection to: %s port=%d\n",
        name, uc_target, target_port
    ) ;

    status = GetHostnameInfo( uc_target, &sin, full, ip ) ;
    if( !status ) {
	DODBG(0)(DBGDEV, "%s: could not resolve target: %s\n", name, uc_target);
	return( false ) ;
    }
    if( uc_ip ) free( uc_ip ) ;
    uc_ip = UCP strdup( CP ip ) ;

    sin.sin_family = AF_INET;
    sin.sin_port = htons( target_port ) ;
    return( true ) ;
}

void Client::DisplayConnection( void )
{
    struct sockaddr localsock ;
    socklen_t localsocklen ;
    int noport ;

    localsocklen = sizeof(localsock) ;
    memset( (void*)(&localsock), 0, localsocklen ) ;
    int status = getsockname( fd, &localsock, &localsocklen ) ;
    if( status ) {
	perror( "getsockname" ) ;
	return ;
    }

    // noport = *(unsigned short*)localsock.sa_data ;
    memcpy( (void*)&noport, (void*)localsock.sa_data, sizeof(noport) ) ;
    from_port = ntohs( noport ) ;

    DODBG(1)( DBGDEV, "%s: connected to %s (%s:%d) fd=%d\n",
            name, uc_target, uc_ip, target_port, fd ) ;

    char c_status[256] ;
    snprintf( c_status, 256, "Connected to %s(%d)", CP uc_target, target_port );
    StatusLog->L_Write_U( name, "INFO", c_status ) ;
}

int Client::ConnectionStarted( void )
{
    ResetRxTimedOut() ;
    mQueue->InitQueue() ;
    return( DeviceOpen() ) ;
}

void Client::ConnectionEnded( void )
{
    DeviceClose() ;
    shutdown( fd, 2 ) ;
    close(fd) ;
    fd = -1 ;
}

int Client::DeviceOpen( void )
{
    DODBG(0)(DBGDEV, "Device Open base: should never see this.\n" ) ;
    return( true ) ;
}

void Client::DeviceClose( void )
{
    DODBG(0)(DBGDEV, "Device Close base: should never see this.\n" ) ;
}

void Client::RunThread( void )
{
    if( ! port_set ) return ;
    if( ! ip_set ) return ;

    retry_count = 0 ;

    while( true ) {
	time_t c_start, c_end ; 

	n_cmds = 0 ;
	while( true ) {
	    client_state = 0 ;

	    while( ! SetupDetails() ) sleep( 60 ) ; // Just once/minute.

	    DODBG(4)(DBGDEV, "Attempting connection for %s.\n", name ) ;
	    client_state = 1 ;
	    int status = 0 ;
	    while( 1 ) {
		status = connect( fd, (struct sockaddr*)&sin, sizeof(sin)) ;
		if( !status ) break ;
		if( errno == EINTR ) continue ;
		break ;
	    }
	    if( !status ) break ;
	    int s_errno = errno ;

	    if( s_errno == EINPROGRESS ) {
		fprintf( stderr, "<%s> IN PROGRESS error\n", name ) ;
	    } else if( s_errno == EISCONN ) {
		fprintf( stderr, "<%s> CONNECTION IN PLACE error\n", name ) ;
	    } else if( s_errno == ECONNREFUSED ) {
		fprintf( stderr, "<%s> CONNECTION REFUSED error\n", name ) ;
	    } else {
		perror( CP name ) ;
		fprintf( stderr, "<%s> CLIENT/CONNECT error\n", name ) ;
	    }

	    DODBG(4)(DBGDEV, "Retry connect for %s after sleeping for %d s.\n", name, retry_time ) ;

	    shutdown(fd,2) ; close(fd) ; fd = -1 ;

	    client_state = 6 ;
	    retry_count++ ;
	    retry_ticker = retry_time ;
	    while( retry_ticker > 0 ) {
		sleep( 1 ) ;
		retry_ticker-- ;
	    }
	}
	DisplayConnection() ;
	time(&c_start) ;

	client_state = 2 ;
	connection_good = ConnectionStarted() ;
	reconnect = false ;
	if( retry_count )
	    fprintf( stderr, "%s: Connection finally accepted.\n", name ) ;

	retry_count = 0 ;
	ClearWait() ;

	while( connection_good && bRunning ) {
	    struct timeval tv_1, tv_2 ;

	    client_state = 3 ;
	    WaitOnSignal() ;
	    if( bRunning == false )
		break ;

	    client_state = 4 ;

	    gettimeofday( &tv_1, 0 ) ;

	    InitiateTransaction() ;
	    connection_good = HandleRequest() ;

	    gettimeofday( &tv_2, 0 ) ;
	    mQueue->ReleaseWait() ;
	    if( ! mQueue->GetWaiting() ) {
		mQueue->ReleaseThisTransaction() ;
	    }
	    client_state = 5 ;

	    int t_cmd_time = (tv_2.tv_usec - tv_1.tv_usec)/1000 + 
		1000* (tv_2.tv_sec - tv_1.tv_sec) ;
	    DODBG(2)(DBGDEV, "%s required %d ms (Conn=%s).\n", 
		name, t_cmd_time, connection_good?"OK":"FAIL" ) ;

	    n_cmds++ ;
	    if( new_timing ) {
		cmd_time = t_cmd_time ; new_timing = false ;
	    } else {
		if( cmd_time < t_cmd_time ) cmd_time = t_cmd_time ;
	    }

	    if( reconnect ) break ;
	}
	ConnectionEnded() ;
	time(&c_end) ;
	disconnects++ ;
	if( (c_end-c_start) < retry_time ) {
	    client_state = 6 ;
	    retry_ticker = 5 + retry_time - (c_end-c_start) ;
	    while( retry_ticker > 0 ) {
		sleep( 1 ) ;
		retry_ticker-- ;
	    }
	}
    }
}

int Client::InitiateTransaction( void )
{
    const u_char *request ;
    mQueue->GetNextTransaction() ;

    request = mQueue->GetRequest() ;
    DODBG(6)( DBGDEV, "Request %s: <%s>\n", name, request ) ;

    int n_toks = TokenizeRequest( request, client_cmd ) ;
    DODBG(10)( DBGDEV, "Request %s: toks=%d\n", name, client_cmd.nCmds ) ;

    return( n_toks ) ;
}

int Client::HandleRequest( void )
{
    char resp[256] ;

    if( client_cmd.nCmds < 2 ) return( false ) ;

    if( strcasecmp( CP client_cmd.cmd[1], "errors" ) == 0 ) {
	ClearNErrs() ;
	mQueue->SetResponse( ERR_101_CLRERR ) ;
	return( true ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "commands" ) == 0 ) {
	ClearNCmds() ;
	mQueue->SetResponse( ERR_102_CLRCMD ) ;
	return( true ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "reconnect" ) == 0 ) {
	reconnect = true ;
	mQueue->SetResponse( ERR_103_RECNCT ) ;
	return( true ) ;
    } 

    if( strcasecmp( CP client_cmd.cmd[1], "cycle" ) == 0 ) {
	if( client_cmd.nCmds == 3 ) {
	    double d_cycle = strtod( CP client_cmd.cmd[2], NULL ) ;
	    if( d_cycle >= 0 ) cycle = (int)(d_cycle * 1000) ;
	    else cycle = -1 ;
	    StopPeriodicTasks() ; StartPeriodicTasks() ;
	}
	snprintf( resp, sizeof(resp), 
	    ERR_104_PERIOD " Cycle = %d Delay = %d.", cycle, delay ) ;
	mQueue->SetResponse( resp ) ;
	return( true ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "delay" ) == 0 ) {
	double d_delay ;
	if( client_cmd.nCmds == 3 )
	    d_delay = strtod( CP client_cmd.cmd[2], NULL ) ;
	else
	    d_delay = -1 ;
    
	if( d_delay >= 0 ) {
	    delay = (int)(d_delay * 1000) ;
	    StopPeriodicTasks() ; StartPeriodicTasks() ;
	}
	snprintf( resp, sizeof(resp), 
	    ERR_104_PERIOD " Cycle = %d Delay = %d.", cycle, delay ) ;
	mQueue->SetResponse( resp ) ;
	return( true ) ;

    } else if( strcasecmp( CP client_cmd.cmd[1], "timeout/read" ) == 0 ) {
	double d_t_o_r ;
	if( client_cmd.nCmds == 3 )
	    d_t_o_r = strtod( CP client_cmd.cmd[2], NULL ) ;
	else
	    d_t_o_r = -1 ;
    
	if( d_t_o_r > 0 ) r_timeo = (int)(d_t_o_r * 1000) ;
	snprintf( resp, sizeof(resp), 
	    ERR_104_PERIOD " Read timeout = %d.", r_timeo ) ;
	mQueue->SetResponse( resp ) ;
	return( true ) ;
    }

    return( false ) ;
}

void Client::AddStatus( int w_status, u_char *key, u_char *value )
{
    OCF_Status->AddStatus( mStatus+w_status, name, key, value ) ;
}

int Client::NoStatus( int w_status )
{
    return( OCF_Status->NoStatus( mStatus+w_status ) ) ;
}

// This is called from another thread. Anything changeable that is referenced
// in here should be declared volatile.
void Client::PleaseReport( int wline, int scol )
{
    if( ! can_report ) return ;
    if( ! port_set || ! ip_set ) return ;

    snprintf( gStatus[wline][scol++], MAXDEVSTR, "P %d", target_port ) ;

    int l_target = strlen( CP uc_target ) ;
    if( l_target < 16 ) {
	strncpy( gStatus[wline][scol], CP uc_target, MAXDEVSTR ) ;
    } else {
	u_char *p = UCP strchr( CP uc_target, '.' ) ;
	if( p ) l_target = p - uc_target ;
	if( l_target > 15 ) l_target = 15 ;
	memcpy( gStatus[wline][scol], uc_target, l_target ) ;
	gStatus[wline][scol][l_target] = '\0' ;
    }
    scol++ ;
    snprintf( gStatus[wline][scol++], MAXDEVSTR, "Q=%d", mQueue->MaxQueue() ) ;

    switch( client_state ) {
    case 0:
        strncpy( gStatus[wline][scol++], "Startup", MAXDEVSTR ) ;
        break ;
    case 1:
        strncpy( gStatus[wline][scol++], "Connecting", MAXDEVSTR ) ;
        break ;
    case 2:
        strncpy( gStatus[wline][scol++], "Device Init", MAXDEVSTR ) ;
	break ;
    case 3:
        strncpy( gStatus[wline][scol++], "Await Cmd", MAXDEVSTR ) ;
        break ;
    case 4:
	switch( stream_state ) {
	    case 0:
	    strncpy( gStatus[wline][scol++], "Cmd Rcvd", MAXDEVSTR ) ;
	    break ;
	    case 1:
	    strncpy( gStatus[wline][scol++], "Write next", MAXDEVSTR ) ;
	    break ;
	    case 2:
	    strncpy( gStatus[wline][scol++], "Writing", MAXDEVSTR ) ;
	    break ;
	    case 3:
	    strncpy( gStatus[wline][scol++], "Write Done", MAXDEVSTR ) ;
	    break ;
	    case 4:
	    strncpy( gStatus[wline][scol++], "Reading", MAXDEVSTR ) ;
	    break ;
	    case 5:
	    strncpy( gStatus[wline][scol++], "Read Done", MAXDEVSTR ) ;
	    break ;
	}
	break ;
    case 5:
        strncpy( gStatus[wline][scol++], "Cmd Done", MAXDEVSTR ) ;
        break ;
    case 6:
        strncpy( gStatus[wline][scol++], "Retry", MAXDEVSTR ) ;
        strncpy( gStatus[wline][scol++], "in", MAXDEVSTR ) ;
        snprintf( gStatus[wline][scol++], MAXDEVSTR, "%d s", retry_ticker ) ;
	if( disconnects + retry_count )
	    snprintf( gStatus[wline][scol++], MAXDEVSTR, "R=%d,D=%d", 
		retry_count, disconnects ) ;
	break ;
    }
    if( n_cmds ) {
	snprintf( gStatus[wline][scol++], MAXDEVSTR, "C=%d", n_cmds ) ;
	snprintf( gStatus[wline][scol++], MAXDEVSTR, "%dms", cmd_time ) ;
	if( errors + disconnects )
	    snprintf( gStatus[wline][scol++], MAXDEVSTR, "E=%d,D=%d", 
		errors, disconnects ) ;
	new_timing = true ;
    }
}

void Client::StartPeriodicTasks( void )
{
    DODBG(0)(DBGDEV, "Should not be here: StartPeriodicTasks.\n" ) ;
}
void Client::StopPeriodicTasks( void )
{
    DODBG(0)(DBGDEV, "Should not be here: StopPeriodicTasks.\n" ) ;
}

static const char* cClientOptions[] = {
    "errors",
    "commands",
    "reconnect",
    "cycle %f",
    "delay %f"
    } ;

int Client::ListOptions( int iN, u_char *cOption, int iOptionLength )
{
    int M = 0 ;
    strcpy( CP cOption, "" ) ;

    int iMax = sizeof(cClientOptions)/sizeof(char*) ;
    if( 0 <= iN && iN < iMax )
	strncpy( CP cOption, cClientOptions[iN], iOptionLength ) ;

    M = strlen( CP cOption ) ;
    return( M ) ;
}

void Client::ResetRetry( void )
{
    retry_ticker = -1 ;
}
