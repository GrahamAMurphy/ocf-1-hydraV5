#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <iostream>

#pragma implementation

#include "global.h"
#include "stream.h"
#include "client.h"
#include "server.h"
#include "support.h"

Server::Server( void )
{
    port_set = false ;
    bind_fail_exit = true ;
    devtype = TypeServer ;
    devtypestring = "Server" ;
    n_cmds = 0 ;
    strcpy( CP from_ip, "" ) ;
    server_state = 0 ;
    w_timeo = 120 ; // SECS! to confuse things, this is a transaction timeout.
    is_a_server = true ;

    pthread_mutexattr_t sequencer_mutex_attr ;
    pthread_mutexattr_init( &sequencer_mutex_attr ) ;
    pthread_mutexattr_settype( &sequencer_mutex_attr, PTHREAD_MUTEX_NORMAL ) ;
    pthread_mutexattr_setprotocol( &sequencer_mutex_attr, PTHREAD_PRIO_INHERIT ) ;

    pthread_mutex_init( &sequencer_mutex, &sequencer_mutex_attr ) ;
    pthread_mutex_lock( &sequencer_mutex ) ;
    pthread_mutex_unlock( &sequencer_mutex ) ;

    DODBG(9)(DBGDEV, "Leaving server constructor.\n" ) ;
}

int Server::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "port" ) == 0 ) {
        uc_bind_port = UCP strdup( CP value ) ;
	port_set = true ;
    } else if( strcasecmp( CP key, "bind-fail-exit" ) == 0 ) {
	bind_fail_exit = strtol( CP value, NULL, 0 ) ;
    } else {
        return( Stream::Parse( value, key )  ) ;
    }

    return( true ) ;
}

void Server::StartOps( void )
{
    DODBG(9)(DBGDEV, "Server being started up.\n" ) ;
    mStatus = OCF_Status->RequestStatus( 1 ) ; // debug info.

    StartThread() ;
    DODBG(9)(DBGDEV, "Server has started.\n" ) ;
}

int Server::CreateAndBind( void )
{
    bind_port = GetPortNumber( uc_bind_port ) ;
    if( bind_port < 0 ) {
	DODBG(0)(DBGDEV, "Bad port number.\n" ) ;
	return( false ) ;
    }

    bind_protocol = GetProtocol( uc_bind_port ) ;
    if( bind_protocol < 0 ) {
	DODBG(0)(DBGDEV, "Bad protocol.\n" ) ;
	return( false ) ;
    }

    socket_type = (bind_protocol == IPPROTO_UDP) ? SOCK_DGRAM : SOCK_STREAM ;

    fdnet = socket( AF_INET, socket_type, bind_protocol ) ;
    if( fdnet < 0 ) {
	perror( "socket" ) ;
	DODBG(0)(DBGDEV, "Server socket could not be opened.\n" ) ;
	return( false ) ;
    }

    SetCloseOnExec( fdnet ) ;

    DODBG(4)(DBGDEV, "Server socket: fd=%d proto=%d\n", fdnet, bind_protocol );

    SetSocketOptions( fdnet, (1<<SO_REUSEADDR) ) ;

    memset( (u_char*)&sin, 0, sizeof(sin) ) ;
    sin.sin_family = AF_INET;
    sin.sin_port = htons( bind_port ) ;
    if( bind( fdnet, (struct sockaddr*)&sin, sizeof(sin) ) < 0 ) {
        perror("bind") ;
        DODBG(0)( DBGDEV, "Error in server bind.\n" ) ;
	return( false ) ;
    }
    DODBG(2)( DBGDEV, "Bind succeeded for %s\n", name ) ;

    if( bind_protocol == IPPROTO_UDP ) 
	return( false ) ; // We are not supporting this yet.

    return( true ) ;
}

void Server::Unbind( void ) 
{
    shutdown( fdnet, 2 ) ;
    close( fdnet ) ;
    fdnet = -1 ;
}

void Server::AcceptConnection( void ) 
{
    char *client, *ipclient ;
    struct hostent *cname ;
    socklen_t fromlen ;
    u_long ipaccept ;
    struct sockaddr_in frominet ;

    fromlen = sizeof(frominet);
    frominet = sin ;
    while( bRunning ) {
	fd = accept(fdnet, (struct sockaddr*)&frominet, &fromlen);
	if( fd < 0 && errno == EINTR ) continue ;

	if( fd < 0 && errno != EINTR ) {
	    perror( "accept:" ) ;
	    DODBG(0)( DBGDEV, "Error in server accept.\n" ) ;
	    return ;
	}
	break ;
    }
    if( bRunning == false )
	return ;

    SetCloseOnExec( fd ) ;

    cname = gethostbyaddr( (char*)&frominet.sin_addr, 
        sizeof(struct in_addr), frominet.sin_family );
    ipclient = (char*)inet_ntoa(frominet.sin_addr);
    strncpy( CP from_ip, ipclient, sizeof(from_ip) ) ;

    if(cname) client = (char*)cname->h_name;
    else client = ipclient ;

    ipaccept = frominet.sin_addr.s_addr ;

    from_port = ntohs( frominet.sin_port ) ;

    DODBG(11)( DBGDEV, "Accepted from %s (%s:%d) fd=%d\n",
            client, ipclient, from_port, fdnet ) ;

    char c_status[256] ;
    snprintf( c_status, 256, "Connection from %s", CP client );
    StatusLog->L_Write_U( name, "INFO", c_status ) ;
}

void Server::Disconnect( void )
{
    DODBG(0)(DBGDEV, "Connection to server was terminated. \n" ) ;
    shutdown( fd, 2 ) ;
    close( fd) ;
    fd = -1 ;
}

void Server::PleaseReport( int wline, int scol )
{
    snprintf( gStatus[wline][scol++], MAXDEVSTR, "P %d", bind_port ) ;
    switch( server_state ) {
    case 0:
	strncpy( gStatus[wline][scol++], "Unconnected", MAXDEVSTR ) ;
	break ;
    case 1:
	strncpy( gStatus[wline][scol++], "Listening", MAXDEVSTR ) ;
	break ;
    case 2:
	strncpy( gStatus[wline][scol++], "Connected", MAXDEVSTR ) ;
	strncpy( gStatus[wline][scol++], "to", MAXDEVSTR ) ;
	strncpy( gStatus[wline][scol++], CP from_ip, MAXDEVSTR ) ;
	snprintf( gStatus[wline][scol++], MAXDEVSTR, "C=%d", n_cmds ) ;
	break ;
    }
}   

void Server::RunThread( void )
{
    if( ! port_set ) return ;

    while( bRunning ) {
	server_state = 0 ;
	if( ! CreateAndBind() ) { 
	    DODBG(0)(DBGDEV, "There is another system.\n" ) ;
	    DODBG(0)(DBGDEV, "Sequencer cannot operate.\n" ) ;
	    if( bind_fail_exit ) exit(0) ;
	    return ;
	}

	server_state = 1 ;
	if( listen(fdnet, -1) < 0 ) {
	    perror( "listen" ) ;
	    DODBG(4)( DBGDEV, "Error in server listen.\n" ) ;
	    DODBG(0)(DBGDEV, "Sequencer cannot operate.\n" ) ;
	    if( bind_fail_exit ) exit(0) ;
	    return ;
	}
	DODBG(2)( DBGDEV, "Listen succeeded for %s.\n", name ) ;

	AcceptConnection() ;
	Unbind() ;

	server_state = 2 ;
	ServerActive() ;
	StatusLog->L_Write_U( name, "INFO", "Connection terminated." ) ;
    }
}

void Server::ServerActive( void )
{
    int ck_pnt = 0 ;
    n_cmds = 0 ;
    rx_pnt = 0 ;

    while( 1 ) {
	u_char *p_eol = 0 ;
	int nread = 0 ;
	if( rx_pnt > ck_pnt ) {
	    p_eol = (u_char*)memchr( buff_rx+ck_pnt, '\n', (rx_pnt-ck_pnt) ) ;

	    if( p_eol ) ck_pnt = p_eol - buff_rx ;
	    else ck_pnt = rx_pnt ;
	}
	if( ! p_eol ) {
	    while( 1 ) {
		nread = read( fd, buff_rx+rx_pnt, RX_BUFFSZ-rx_pnt ) ;
		if( nread <= 0 ) {
		    if( errno == EINTR ) continue ;
		    perror( CP name ) ;
		    Disconnect() ;
		    return ;
		}
		break ;
	    }
	}

	rx_pnt += nread ;

	p_eol = (u_char*)memchr( buff_rx+ck_pnt, '\n', (rx_pnt-ck_pnt) ) ;
	if( p_eol ) {
	    ck_pnt = p_eol - buff_rx + 1 ;
	    if( ck_pnt > 1 && p_eol[-1] == '\r' ) p_eol-- ;
	    *p_eol = '\0' ;
	}
	else {
	    ck_pnt = rx_pnt ;
	    continue ;
	}

	buff_rx[rx_pnt] = '\0' ; // This is just so we can print out.

	if( *buff_rx == '#' || *buff_rx == '\0' ) {
	    ck_pnt = rx_pnt = 0 ;
	    continue ;
	}
	DODBG(0)(DBGDEV, "%s received <%s>\n", name, buff_rx ) ;

	int nwrite ;
	n_cmds++ ;

	pthread_mutex_lock( &sequencer_mutex ) ;
	SequencerCmd() ;
	pthread_mutex_unlock( &sequencer_mutex ) ;

	/* We write out whatever was provided */
	while( tx_pnt > 0 ) {
	    nwrite = write( fd, buff_tx, tx_pnt ) ;
	    if( nwrite <= 0 ) {
		if( errno == EINTR ) continue ;
		perror( CP name ) ;
		Disconnect() ;
		return ;
	    }
	    break ;
	}
	rx_pnt = rx_pnt - ck_pnt ;
	if( rx_pnt )
	    memmove( buff_rx, buff_rx+ck_pnt, rx_pnt ) ;
	ck_pnt = 0 ;
    }
}

int Server::SequencerCmd( void )
{
    u_char *s_target = buff_rx ;

    for( ; *s_target != '\0' ; s_target++ ) {
	if( *s_target != ' ' && *s_target != '\t' ) break ;
    }

    StatusLog->L_Write_U( name, "INFO", s_target ) ;
    DODBG(2)(DBGDEV, "%s received command: %s\n", name, s_target ) ;

    u_char temp[256] ;
    snprintf( CP temp, 256, "#Cmds=%d Last=%s", n_cmds, s_target ) ;
    OCF_Status->AddStatus( mStatus, name, temp, 0 ) ;

    bool do_wait = true ;
    if( *s_target == '@' ) { // Hack to find out wait or not.
	s_target++ ;
	do_wait = false ;
    }
    DODBG(4)(DBGDEV, "Will %swait for response.\n", do_wait?"":"NOT " ) ;

    u_char *e_target = UCP strpbrk( CP s_target, " \t" ) ;
    if( ! e_target ) {
	e_target = s_target ;
	while( *e_target != '\0' ) e_target++ ;
    }

    int l_target = e_target - s_target ;

    if( l_target == 0 ) {
	DODBG(4)( DBGDEV, "Null command %s.\n", s_target ) ;
	snprintf( CP buff_tx, TX_BUFFSZ, ERR_400_NULL " %s.\r\n", 
	    s_target );
	tx_pnt = strlen( CP buff_tx ) ;
	return( false ) ;
    }

    u_char target[1024] ;
    memcpy( target, s_target, l_target ) ;
    target[l_target] = '\0' ;

    DODBG(4)( DBGDEV, "Target of command: %s.\n", target ) ;

    for(  u_char *p_t = target ; *p_t != '\0' ; p_t++ ) {
	*p_t = tolower( *p_t ) ;
    }

    if( strcmp( "query", CP target ) == 0 ) {
	int HandleQuery( u_char *ucRequest, u_char *ucResponse, int iResponseSize ) ;
	tx_pnt = HandleQuery( buff_rx, buff_tx, sizeof(buff_tx) ) ;
	return( true ) ;
    }

    ENTRY newent, *ep ;
    newent.key = CP target ;
    newent.data = (void*) -1 ;

    ep = hsearch( newent, FIND ) ;
    if( ! ep ) {
	DODBG(0)( DBGDEV, "Target of command not known!!!! <%s>\n", target ) ;
	snprintf( CP buff_tx, TX_BUFFSZ, ERR_401_UKNTGT ": <%s>.\r\n", target );
	tx_pnt = strlen( CP buff_tx ) ;
	return( false ) ;
    }

    int wdev = (long) ep->data ;
    if( wdev <= 0 || wdev > TopDev() ) {
	DODBG(0)( DBGDEV, "Target out of range: %d\n", wdev ) ;
	snprintf( CP buff_tx, TX_BUFFSZ, ERR_402_BADTGT ": <%d>.\r\n", wdev ) ;
	tx_pnt = strlen( CP buff_tx ) ;
	return( false ) ;
    }

    DODBG(4)( DBGDEV, "Target device is: <%s>\n", streams[wdev]->getName() ) ;

    if( ! streams[wdev]->Active() ) {
	DODBG(4)( DBGDEV, "Target device is not active.\n" ) ;
	snprintf( CP buff_tx, TX_BUFFSZ, 
	    ERR_403_TGTOFF": <%s>.\r\n", streams[wdev]->getName() ) ;
	tx_pnt = strlen( CP buff_tx ) ;
	return( false ) ;
    }

    if( ! streams[wdev]->IsAClient() ) {
	DODBG(4)( DBGDEV, "Target device is not a client.\n" ) ;
	snprintf( CP buff_tx, TX_BUFFSZ, 
	    ERR_411_NOTCLNT": <%s>.\r\n", streams[wdev]->getName() ) ;
	tx_pnt = strlen( CP buff_tx ) ;
	return( false ) ;
    }

    int wdev_qnum ;
    wdev_qnum = streams[wdev]->mQueue->AddTransaction( buff_rx, do_wait ) ;
    if( wdev_qnum < 0 ) {
	DODBG(0)( DBGDEV, "No room on queue for %s\n", streams[wdev]->getName()) ;
	snprintf( CP buff_tx, TX_BUFFSZ, 
	    ERR_404_QUEFLL " for %s.\r\n", streams[wdev]->getName() ) ;
	tx_pnt = strlen( CP buff_tx ) ;
	return( false ) ;
    }

    // make sure device knows about the request.
    DODBG(4)( DBGDEV, "Signalling <%s>\n", streams[wdev]->getName() ) ;
    streams[wdev]->SignalDevice() ;

    if( do_wait ) {
	DODBG(4)( DBGDEV, "Waiting for <%s> %d\n", 
	    streams[wdev]->getName(), wdev_qnum ) ;

	if( streams[wdev]->mQueue->WaitTransaction( wdev_qnum, w_timeo ) ) {
	    snprintf( CP buff_tx, TX_BUFFSZ, "%s\r\n",
		streams[wdev]->mQueue->GetResponse( wdev_qnum ) ) ;
	} else {
	    snprintf( CP buff_tx, TX_BUFFSZ, 
		ERR_405_SEQTO " for %s.\r\n", streams[wdev]->getName() );
	}

	DODBG(4)( DBGDEV, "Response was: >%s", buff_tx ) ;
	streams[wdev]->mQueue->ReleaseTransaction( wdev_qnum ) ;
    } else {
	strncpy( CP buff_tx, ERR_100_NOWAIT "\r\n", TX_BUFFSZ ) ;
    }
    tx_pnt = strlen( CP buff_tx ) ;

    return( true ) ;
}

int HandleQuery( u_char *ucRequest, u_char *ucResponse, int iResponseSize )
{
    sCmd query_cmd ;
    strcpy( CP ucResponse, "" ) ;
    int iDone = 0 ;
    int M = 0 ;

    int n = TokenizeRequest( ucRequest, query_cmd ) ;
    if( n <= 1 ) {
	M += snprintf( CP ucResponse+M, iResponseSize-M, ERR_408_BADCMD ": <%s>.\r\n", CP ucRequest );
	return( M ) ;
    }

    if( strcasecmp( CP query_cmd.cmd[1], "all" ) == 0 ) {
	int i ;
	for( i = 0 ; i <= TopDev() ; i++ ) {
	    if( streams[i]->getType() == Stream::TypeNone ) continue ;
	    M += snprintf( CP ucResponse+M, iResponseSize-M, "<%s>", CP streams[i]->getName() ) ;
	}
	iDone = 1 ;
    } else if( strcasecmp( CP query_cmd.cmd[1], "aliases" ) == 0 ) {
	int i ;
	for( i = 0 ; i <= TopDev() ; i++ ) {
	    if( streams[i]->getType() == Stream::TypeNone ) continue ;
	    const u_char *p = streams[i]->getAlias(0) ;
	    if( p == 0 )
		continue ;

	    M += snprintf( CP ucResponse+M, iResponseSize-M, "<%s=", CP streams[i]->getName() ) ;
	    int j ;
	    int iOnce = 1 ;

	    for( j = 0 ; j < MAXALIASES ; j++ ) {
		p = streams[i]->getAlias(j) ;
		if( p == 0 ) break ;
		if( iOnce == 0 ) M += snprintf( CP ucResponse+M, iResponseSize-M, "," ) ;
		M += snprintf( CP ucResponse+M, iResponseSize-M, "%s", CP p ) ;
		iOnce = 0 ;
	    }
	    M += snprintf( CP ucResponse+M, iResponseSize-M, ">" ) ;
	}
	iDone = 1 ;
    } else if( strcasecmp( CP query_cmd.cmd[1], "enabled" ) == 0 ) {
	int i ;
	for( i = 0 ; i <= TopDev() ; i++ ) {
	    if( streams[i]->getType() == Stream::TypeNone ) continue ;
	    if( ! streams[i]->IsEnabled() ) continue ;
	    M += snprintf( CP ucResponse+M, iResponseSize-M, "<%s>", CP streams[i]->getName() ) ;
	}
	iDone = 1 ;
    }
    if( iDone > 0 ) {
	M += snprintf( CP ucResponse+M, iResponseSize-M, "\r\n" ) ;
	return( M ) ;
    }

    if( n != 3 ) {
	M += snprintf( CP ucResponse+M, iResponseSize-M, ERR_408_BADCMD ": <%s>.\r\n", CP ucRequest );
	return( M ) ;
    }

    ENTRY newent, *ep ;
    newent.key = CP query_cmd.cmd[1] ;
    newent.data = (void*) -1 ;

    ep = hsearch( newent, FIND ) ;
    if( ! ep ) {
	DODBG(0)( DBGDEV, "Target of command not known!!!! <%s>\n", query_cmd.cmd[1] ) ;
	M += snprintf( CP ucResponse+M, iResponseSize-M, ERR_401_UKNTGT ": <%s>.\r\n", query_cmd.cmd[1] );
	return( M ) ;
    }

    int wdev = (long) ep->data ;
    if( wdev <= 0 || wdev > TopDev() ) {
	DODBG(0)( DBGDEV, "Target out of range: %d\n", wdev ) ;
	M += snprintf( CP ucResponse+M, iResponseSize-M, ERR_402_BADTGT ": <%d>.\r\n", wdev ) ;
	return( M ) ;
    }
    int iSelect = strtol( CP query_cmd.cmd[2], NULL, 0 ) ;

    Client *t = dynamic_cast<Client*>(streams[wdev]) ;

    M = t->ListOptions( iSelect, ucResponse, iResponseSize ) ;
    if( M >= 0 ) {
	M += snprintf( CP ucResponse+M, iResponseSize-M, "\r\n" ) ;
    }
    return( M ) ;
}
