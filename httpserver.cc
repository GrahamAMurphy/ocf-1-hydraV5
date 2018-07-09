#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>
#include <sys/types.h>

#include <iostream>

#pragma implementation

#include "global.h"
#include "stream.h"
#include "httpserver.h"
#include "support.h"

static char data_page[65536] ;
static int l_page = 0 ;
static char the_intro[1024] ;
static int l_intro = 0 ;

static char stat_page[65536] ;
static int l_status = 0 ;
static char stat_intro[1024] ;
static int l_status_intro = 0 ;

static char basic_intro[] =
"HTTP/1.1 200 OK\r\n"
"Content-Length: %d\r\n"
"Content-type: text/html\r\n"
"\r\n" ;

static char main_page[] = 
"<HTML>\r\n"
"<HEAD>\r\n"
"<TITLE>Status</TITLE>\r\n"
"</HEAD>\r\n"
"<FRAMESET COLS=\"45%,*\">\r\n"
"<FRAME src=\"/data.html\">\r\n"
"<FRAMESET ROWS=\"350,*\">\r\n"
"<FRAME src=\"/extended.html\">\r\n"
"<FRAME src=\"/stats.html\">\r\n"
"</FRAMESET>\r\n"
"</FRAMESET>\r\n"
"</HTML>\r\n"
"\r\n" ;

static char h204_intro[] =
"HTTP/1.1 204 OK\r\n"
"Connection: close\r\n"
"Content-type: text/html\r\n"
"\r\n" ;

#include "form_page.cc"

static char form_page[] = 
"<HTML>\r\n"
"<HEAD>\r\n"
"<TITLE>Update</TITLE>\r\n"
"<style type=\"text/css\">\r\n"
"input { font: 10px }\r\n"
"</style>\r\n"
"</HEAD>\r\n"
"<BODY>\r\n"
"<FORM ACTION=\"/query.html\" METHOD=\"GET\">\r\n"
"<INPUT TYPE=\"SUBMIT\" VALUE=\"Open Attenuator\">\r\n"
"<INPUT TYPE=\"HIDDEN\" NAME=\"INPUT\" VALUE=\"sphere attenuator 1 set 0\">\r\n"
"</FORM>\r\n"
"<FORM ACTION=\"/query.html\" METHOD=\"GET\">\r\n"
"<INPUT TYPE=\"SUBMIT\" VALUE=\"Close Attenuator\">\r\n"
"<INPUT TYPE=\"HIDDEN\" NAME=\"INPUT\" VALUE=\"sphere attenuator 1 set 255\">\r\n"
"</FORM>\r\n"
"<FORM ACTION=\"/query.html\" METHOD=\"GET\">\r\n"
"<INPUT TYPE=\"SUBMIT\" VALUE=\"CMD\">\r\n"
"<INPUT TYPE=\"TEXT\" NAME=\"INPUT\" VALUE=\"\" SIZE=\"48\">\r\n"
"</FORM>\r\n"
"<A HREF=\"/extended.html\"> Extended Commanding </A>\r\n"
"</BODY>\r\n"
"</HTML>\r\n"
"\r\n" ;

// char *form_page = form_page_l ;


void HTTPServer::Deliver( int fd )
{
    char temp[64] ;
    snprintf( temp, 64, basic_intro, sizeof(main_page) ) ;
    write( fd, temp, strlen(temp) ) ;
    write( fd, main_page, sizeof(main_page) ) ;
}

enum iPage { eMain=1, eForm=2, eLog=3, eStatus=4, eLong=5, eQuery=6 } ;

HTTPServer::HTTPServer()
{
    uc_bind_port = http_port ;
    nfds = 0 ;
    for( int i = 0 ; i < MaxConn ; i++ ) {
	fds[i] = -1 ;
	request[i] = 0 ;
    }

    devtype = TypeHTTP ;
    devtypestring = "HTTP Server" ;
    for( int i = 0 ; i < MaxConn ; i++ ) created[i] = 0 ;

    time( created+eMain ) ; // Opening page is always available.
    time( created+eForm ) ; // Form page is always available.
    time( created+eLong ) ; // Long Form page is always available.
    time( created+eQuery ) ; // Query page is always available.

    DODBG(11)(DBGDEV, "Leaving http server constructor.\n" ) ;
}

HTTPServer::~HTTPServer()
{
    DODBG(11)(DBGDEV, "http server destructor.\n" ) ;
    for( int i = 0 ; i < MaxConn ; i++ ) {
	if( fds[i] < 0 ) continue ;
	write( fds[i], h204_intro, sizeof(h204_intro) ) ;
	// shutdown( fds[i], 2 ) ;
	close( fds[i] ) ;
    }
}

void HTTPServer::ConnectionOff( int fd_x )
{
    shutdown( fd_x, 2 ) ;
    close( fd_x ) ;
    DODBG(11)( DBGDEV, "Closing http connection.\n" ) ;
}

void HTTPServer::RunThread( void )
{
    while( true ) {
	if( ! CreateAndBind() ) {
	    DODBG(0)( DBGDEV, "http server could not operate.\n" ) ;
	    return ;
	}

	if( listen(fdnet, -1) < 0 ) {
	    perror( "listen" ) ;
	    DODBG(0)( DBGDEV, "Error in http server listen.\n" ) ;
	    return ;
	}
	DODBG(11)( DBGDEV, "Listen succeeded for %s.\n", name ) ;

	fds[0] = fdnet ;

	Loop() ;

	Unbind() ;
    }
}

void HTTPServer::Loop( void )
{
    fd_set inpmask ;
    struct timeval timeout ;
    int i, n_fds, selectmax ;

    while( true ) {
	selectmax = 0 ;
	FD_ZERO( &inpmask ) ;

	for( i = 0 ; i < MaxConn ; i++ ) {
	    if( fds[i] < 0 ) continue ;
	    FD_SET( fds[i], &inpmask ) ;
	    if( selectmax < fds[i] )
		selectmax = fds[i] ;
	}

	NumConnections() ;

	selectmax++ ;
	timeout.tv_sec = 0 ;
	timeout.tv_usec = 500000 ;

        n_fds = select( selectmax, &inpmask, NULL, NULL, &timeout ) ;

	for( i = 1 ; i < MaxConn ; i++ ) {
	    if( request[i] )
		MeetRequest( i ) ;
	}

        if( n_fds == 0 ) continue ;

        if( n_fds < 0 ) {
            if( errno == 0 || errno == EINTR ) {
                continue ;
            } else {
                perror( "select" ) ;
                DODBG(0)( DBGDEV, "Fatal error in Loop/select. Sorry!\n" ) ;
                exit(1) ;
            }
        }

        for( i = 1 ; i < selectmax ; i++ ) {
	    if( fds[i] < 0 ) continue ;
	    if( FD_ISSET( fds[i], &inpmask ) ) 
		ReceiveConnection( i ) ;
        }

	if( FD_ISSET( fds[0], &inpmask ) ) {
	    AcceptConnection() ;
	    WireConnection() ;
	    DODBG(11)(DBGDEV, "Accepted connection.\n" ) ;
	}
    }
}

void HTTPServer::WireConnection( void )
{
    int file_status ;
    file_status = fcntl( fd, F_GETFL ) ;
    file_status |= O_NONBLOCK ;
    int status = fcntl( fd, F_SETFL, file_status ) ;
    if( status ) perror( "socket setfl" ) ;

    DODBG(11)(DBGDEV, "Adding HTTP connection fd=%d.\n", fd ) ;
    for( int i = 0 ; i < MaxConn ; i++ ) {
	if( fds[i] >= 0 ) continue ;
	fds[i] = fd ; fd = -1 ;
	time_last[i] = 0 ;
	request[i] = 0 ;
	req_last[i] = -1 ;
	break ;
    }
    if( fd ) ConnectionOff( fd ) ;
    NumConnections() ;
    DODBG(11)(DBGDEV, "%d connections.\n", nfds ) ;
}

void HTTPServer::NumConnections( void )
{
    nfds = 0 ;
    for( int i = 1 ; i < MaxConn ; i++ ) {
	if( fds[i] >= 0 ) nfds++ ;
    }
}

static int iWidth[4] = { 5, 5, 5, 5 } ;

void HTTPServer::Preamble( void )
{
    l_intro = 0 ;
    l_page = 0 ;
    created[eLog] = 0 ;

    strcpy( data_page, "<html>\r\n" ) ;
    strcat( data_page, "<head>\r\n" ) ;
    strcat( data_page, "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"1\">\r\n" ) ;
    strcat( data_page, "<style>\r\n" ) ;
    strcat( data_page, "TD { white-space: nowrap ; padding-left : 5pt ; padding-right : 5pt } \r\n" ) ;
    strcat( data_page, "</style>\r\n" ) ;
    strcat( data_page, "</head>\r\n" ) ;
    strcat( data_page, "<body>\r\n" ) ;
    strcat( data_page, "<table rules=\"all\" border=\"1\" style=\"font-size: 8pt ; \" cellspacing=\"0\" cellpadding=\"1\" >\r\n" ) ;
    strcat( data_page, "<colgroup>\r\n" ) ;

    #if 0
    int M = strlen( data_page ) ;

    M += snprintf( data_page+M, sizeof(data_page)-M, "<col width=\"*\">\r\n" ) ;
    M += snprintf( data_page+M, sizeof(data_page)-M, "<col width=\"*\">\r\n" ) ;
    M += snprintf( data_page+M, sizeof(data_page)-M, "<col width=\"*\">\r\n" ) ;
    M += snprintf( data_page+M, sizeof(data_page)-M, "<col width=\"120\">\r\n" ) ;
    #else

    int M = strlen( data_page ) ;

    M += snprintf( data_page+M, sizeof(data_page)-M, "<col width=\"%d\">\r\n", 8*iWidth[0] ) ;
    M += snprintf( data_page+M, sizeof(data_page)-M, "<col width=\"%d\">\r\n", 8*iWidth[1] ) ;
    M += snprintf( data_page+M, sizeof(data_page)-M, "<col width=\"%d\">\r\n", 8*iWidth[2] ) ;
    M += snprintf( data_page+M, sizeof(data_page)-M, "<col width=\"%d\">\r\n", 8*iWidth[3] ) ;

    iWidth[0] = 5 ;
    iWidth[1] = 5 ;
    iWidth[2] = 5 ;
    iWidth[3] = 5 ;

    #endif
}

void HTTPServer::Body( u_char *name, u_char *key, u_char *value, u_char *age, int n_attempts, int updated )
{
    int l_name  = name ? strlen(CP name) : 0 ;
    int l_key = key ? strlen( CP key ) : 0 ;
    int l_value = value ? strlen( CP value ) : 0 ;

    if( l_name > 256 ) l_name = 256 ;
    if( l_key > 256 ) l_key = 256 ;
    if( l_value > 256 ) l_value = 256 ;

    int l_age = 0 ;
    if( age ) {
	while( *age == ' ' ) age++ ;
	l_age = strlen( CP age ) ;
    }
    if( l_age > 256 ) l_age = 256 ;

    int iSpan = 0 ;

    strcat( data_page, "<tr>" ) ;

    if( l_name == 0 ) {
	strcat( data_page, "<td colspan=4>" ) ;
	strncat( data_page, CP key, l_key ) ;
	iSpan = 1 ;
    } else {
	strcat( data_page, "<td align=\"left\">" ) ;
	strncat( data_page, CP name, l_name ) ;

	if( l_value == 0 ) {
	    strcat( data_page, "<td colspan=2>" ) ;
	    strncat( data_page, CP key, l_key ) ;
	    iSpan = 2 ;
	} else {
	    strcat( data_page, "<td align=\"left\">" ) ;
	    strncat( data_page, CP key, l_key ) ;
	    strcat( data_page, "<td align=\"center\">" ) ;
	    strncat( data_page, CP value, l_value ) ;
	}

	strcat( data_page, "<td align=\"right\">" ) ;

	if( n_attempts ) strcat( data_page, "<font color=\"red\">" ) ;
	else if( updated ) strcat( data_page, "<font color=\"green\">" ) ;

	if( age )
	    strncat( data_page, CP age, l_age ) ;

	if( n_attempts || updated ) strcat( data_page, "</font>" ) ;
    }
    if( iSpan == 0 ) {
	if( iWidth[0] < l_name )
	    iWidth[0] = l_name ;
	if( iWidth[2] < l_key )
	    iWidth[1] = l_key ;
	if( iWidth[2] < l_value )
	    iWidth[2] = l_value ;
	if( iWidth[3] < l_age )
	    iWidth[3] = l_age ;
    }

    strcat( data_page, "</tr>\r\n" ) ;
}

void HTTPServer::Postamble( void )
{
    strcat( data_page, "</colgroup>\r\n" ) ;
    strcat( data_page, "</table>\r\n" ) ;
    strcat( data_page, "</body>\r\n" ) ;
    strcat( data_page, "</html>\r\n" ) ;
    l_page = strlen( data_page ) ;

    strcpy( the_intro, "HTTP/1.1 200 OK\r\n" ) ;
    sprintf( the_intro+strlen(the_intro), "Content-Length: %d\r\n", l_page ) ;
    // Connection: close

    strcat( the_intro, "Expires: -1\r\n" ) ;
    strcat( the_intro, "Content-type: text/html\r\n" ) ;
    strcat( the_intro, "\r\n" ) ;
    l_intro = strlen( the_intro ) ;

    time( created+eLog ) ;
}

void HTTPServer::ReceiveConnection( int w_i )
{
    int mode = GraspGet( w_i ) ;
    if( mode < 0 ) {
	ConnectionClose( w_i ) ;
	return ;
    }
    // if( request[w_i] ) return ;
    request[w_i] = mode ;
    if( mode != req_last[w_i] )
	time_last[w_i] = 0 ;
}

int HTTPServer::GraspGet ( int w_i )
{
    char buffer[1024] ;
    int fdx = fds[w_i] ;

    int nread = read( fdx, buffer, 1024 ) ;
    if( nread <= 0 ) {
	// These can be confusing!
	// perror( "graspget" ) ;
	return( -1 ) ;
    }
    buffer[nread] = '\0' ;
    char *p = strpbrk( buffer, "\r\n" ) ;
    if( p ) *p = '\0' ;

    // fprintf( stderr, "%2d %s\r\n", fdx, buffer ) ;

    if( strncasecmp( buffer, "get / ", 6 ) == 0 ) {
	return( eMain ) ;
    } else if( strncasecmp( buffer, "get /f", 6 ) == 0 ) {
	return( eForm ) ;
    } else if( strncasecmp( buffer, "get /d", 6 ) == 0 ) {
	return( eLog ) ;
    } else if( strncasecmp( buffer, "get /s", 6 ) == 0 ) {
	return( eStatus ) ;
    } else if( strncasecmp( buffer, "get /e", 6 ) == 0 ) {
	return( eLong ) ;
    } else if( strncasecmp( buffer, "get /q", 6 ) == 0 ) {
	ExecuteCommand( UCP buffer ) ;
	return( eQuery ) ;
    }
    return( 0 ) ;
}

void HTTPServer::ConnectionClose( int w_i )
{
    DODBG(11)(DBGDEV, "Closing %d\n", w_i ) ;
    ConnectionOff( fds[w_i] ) ;
    request[w_i] = 0 ; 
    time_last[w_i] = 0 ;
    req_last[w_i] = -1 ;
    fds[w_i] = -1 ;
}

void HTTPServer::ExecuteCommand( u_char *buffer )
{
    u_char *p = UCP strchr( CP buffer, '=' ) ;
    if( ! p ) {
	DODBG(0)(DBGDEV, "Command rejected.\n" ) ; return ;
    }

    u_char *q = UCP strchr( CP ++p, ' ' ) ;
    if( ! q ) {
	DODBG(0)(DBGDEV, "Command rejected.\n" ) ; return ;
    }
    *q = '\0' ;

    while( true ) {
	char *plus = strchr( CP p, '+' ) ;
	if( !plus ) break ;
	*plus = ' ' ;
    }

    snprintf( CP buff_rx, RX_BUFFSZ, "@%s", p ) ;
    n_cmds++ ;

    pthread_mutex_lock( &sequencer_mutex ) ;
    SequencerCmd() ;
    pthread_mutex_unlock( &sequencer_mutex ) ;

    // We always ignore the results.
}

int HTTPServer::DeliverOne( int mode, int w_i )
{
    char temp[128] ;
    int nwrite = 0 ;
    int fdx = fds[w_i] ;

    switch( mode ) {
    case eMain:
	snprintf( temp, 128, basic_intro, sizeof(main_page) ) ;
	nwrite = write( fdx, temp, strlen(temp) ) ;
	if( nwrite > 0 ) nwrite = write( fdx, main_page, sizeof(main_page) ) ;
	break ;
    case eForm:
	snprintf( temp, 128, basic_intro, strlen(form_page) ) ;
	nwrite = write( fdx, temp, strlen(temp) ) ;
	if( nwrite > 0 ) nwrite = write( fdx, form_page, strlen(form_page));
	break ;
    case eLog:
	nwrite = write( fdx, the_intro, strlen(the_intro) ) ;
	if( nwrite > 0 ) nwrite = write( fdx, data_page, l_page ) ;
	break ;
    case eStatus:
	nwrite = write( fdx, stat_intro, strlen(stat_intro) ) ;
	if( nwrite > 0 ) nwrite = write( fdx, stat_page, l_status ) ;
	break ;
    case eLong:
	snprintf( temp, 128, basic_intro, strlen(long_form) ) ;
	nwrite = write( fdx, temp, strlen(temp) ) ;
	if( nwrite > 0 ) nwrite = write( fdx, long_form, strlen(long_form));
	break ;
    case eQuery:
	nwrite = write( fdx, h204_intro, sizeof(h204_intro) ) ;
	break ;
    }

    /*
    if( nwrite < 0 ) 
	perror( "deliver" ) ;
    */
    return( nwrite < 0 ) ;
}

void HTTPServer::BuildStatus( int cols )
{
    strcpy( stat_page, "<html>\r\n" ) ;
    strcat( stat_page, "<head>\r\n" ) ;
    strcat( stat_page, "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"1\">\r\n" ) ;
    strcat( stat_page, "</head>\r\n" ) ;
    strcat( stat_page, "<body>\r\n" ) ;
    strcat( stat_page, "<table rules=\"all\" border=\"1\" style=\"font-size: 8pt\" cellspacing=\"0\" cellpadding=\"1\" >\r\n" ) ;

    for( int j = 0 ; j < nStatus ; j++ ) {
	strcat( stat_page, "<tr>" ) ;

	for( int k = 0 ; k < cols ; k++ ) {
	    strcat( stat_page, "<td align=\"left\">" ) ;
	    strcat( stat_page, gStatus[j][k] ) ;
	}
	strcat( stat_page, "</tr>\r\n" ) ;
    }

    strcat( stat_page, "</table>\r\n" ) ;
    strcat( stat_page, "</body>\r\n" ) ;
    strcat( stat_page, "</html>\r\n" ) ;
    l_status = strlen( stat_page ) ;

    strcpy( stat_intro, "HTTP/1.1 200 OK\r\n" ) ;
    sprintf( stat_intro+strlen(stat_intro), 
	"Content-Length: %d\r\n", l_status ) ;
    // Connection: close

    strcat( stat_intro, "Expires: -1\r\n" ) ;
    strcat( stat_intro, "Content-type: text/html\r\n" ) ;
    strcat( stat_intro, "\r\n" ) ;
    l_status_intro = strlen( stat_intro ) ;
    time( created+eStatus ) ;
}

void HTTPServer::MeetRequest( int w_i )
{
    int w_request = request[w_i] ;

    if( created[w_request] > time_last[w_i] ) {
	if( DeliverOne( w_request, w_i ) ) {
	    ConnectionClose( w_i ) ;
	} else {
	    time( time_last+w_i ) ;
	    req_last[w_i] = request[w_i] ;
	    request[w_i] = 0 ;
	}
    }
}
void HTTPServer::PleaseReport( int wline, int scol )
{
    snprintf( gStatus[wline][scol++], MAXDEVSTR, "P %d", bind_port ) ;
    snprintf( gStatus[wline][scol++], MAXDEVSTR, "Connections" ) ;
    snprintf( gStatus[wline][scol++], MAXDEVSTR, "%d", nfds ) ;
}   

