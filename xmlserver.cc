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
#include "xmlserver.h"
#include "support.h"

static char cDataPage[65536] ;
static int iDataLine[2048] ;
static int iDataUpdated[2048] ;
static int iLinesTotal = 0 ;
static int Mdata = 0 ;
static volatile int bStatusReady = 0 ;

XMLServer::XMLServer()
{
    uc_bind_port = xml_port ;
    nfds = 0 ;
    for( int i = 0 ; i < MaxConn ; i++ ) {
	fds[i] = -1 ;
	iInitial[i] = 0 ;
	iUpdateOnly[i] = 0 ;
	iCount[i] = 0 ;
    }

    devtype = TypeXML ;
    devtypestring = "XML Server" ;
    for( int i = 0 ; i < MaxConn ; i++ ) created[i] = 0 ;

    pthread_mutexattr_t xml_mutex_attr ;
    pthread_mutexattr_init( &xml_mutex_attr ) ;
    pthread_mutexattr_settype( &xml_mutex_attr, PTHREAD_MUTEX_NORMAL ) ;
    pthread_mutexattr_setprotocol( &xml_mutex_attr, PTHREAD_PRIO_INHERIT ) ;

    pthread_mutex_init( &xml_mutex, &xml_mutex_attr ) ;
    pthread_mutex_lock( &xml_mutex ) ;
    pthread_mutex_unlock( &xml_mutex ) ;

    DODBG(11)(DBGDEV, "Leaving xml server constructor.\n" ) ;
}

XMLServer::~XMLServer()
{
    DODBG(11)(DBGDEV, "xml server destructor.\n" ) ;
    for( int i = 0 ; i < MaxConn ; i++ ) {
	if( fds[i] < 0 ) continue ;
	// shutdown( fds[i], 2 ) ;
	close( fds[i] ) ;
    }
}

void XMLServer::ConnectionOff( int fd_x )
{
    shutdown( fd_x, 2 ) ;
    close( fd_x ) ;
    DODBG(11)( DBGDEV, "Closing xml connection.\n" ) ;
}

void XMLServer::RunThread( void )
{
    while( true ) {
	if( ! CreateAndBind() ) {
	    DODBG(0)( DBGDEV, "xml server could not operate.\n" ) ;
	    return ;
	}

	if( listen(fdnet, -1) < 0 ) {
	    perror( "listen" ) ;
	    DODBG(0)( DBGDEV, "Error in xml server listen.\n" ) ;
	    return ;
	}
	DODBG(11)( DBGDEV, "Listen succeeded for %s.\n", name ) ;

	fds[0] = fdnet ;

	Loop() ;

	Unbind() ;
    }
}

void XMLServer::Loop( void )
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

	if( bStatusReady ) {
	    pthread_mutex_lock( &xml_mutex ) ;
	    for( i = 1 ; i < MaxConn ; i++ ) {
		MeetRequest( i ) ;
	    }
	    bStatusReady = 0 ;
	    pthread_mutex_unlock( &xml_mutex ) ;
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

void XMLServer::WireConnection( void )
{
    int file_status ;
    file_status = fcntl( fd, F_GETFL ) ;
    file_status |= O_NONBLOCK ;
    int status = fcntl( fd, F_SETFL, file_status ) ;
    if( status ) perror( "socket setfl" ) ;

    DODBG(11)(DBGDEV, "Adding XML connection fd=%d.\n", fd ) ;
    for( int i = 0 ; i < MaxConn ; i++ ) {
	if( fds[i] >= 0 ) continue ;
	fds[i] = fd ; fd = -1 ;
	time_last[i] = 0 ;
	iInitial[i] = 1 ;
	iUpdateOnly[i] = 0 ;
	iCount[i] = 0 ;
	break ;
    }
    if( fd ) ConnectionOff( fd ) ;
    NumConnections() ;
    DODBG(11)(DBGDEV, "%d connections.\n", nfds ) ;
}

void XMLServer::NumConnections( void )
{
    nfds = 0 ;
    for( int i = 1 ; i < MaxConn ; i++ ) {
	if( fds[i] >= 0 ) nfds++ ;
    }
}

void XMLServer::Preamble( void )
{
    pthread_mutex_lock( &xml_mutex ) ;
    Mdata = 0 ;
    iLinesTotal = 0 ;
}

void XMLServer::Body( u_char *name, u_char *key, u_char *value, u_char *age, int n_attempts, int updated )
{
    int l_name  = name ? strlen(CP name) : 0 ;
    int l_key = key ? strlen( CP key ) : 0 ;
    int l_value = value ? strlen( CP value ) : 0 ;

    if( l_name == 0 ) {
	iDataLine[iLinesTotal] = Mdata ; iDataUpdated[iLinesTotal] = updated ; iLinesTotal++ ;
	Mdata += snprintf( cDataPage+Mdata, sizeof(cDataPage)-Mdata, "<%s value=\"%s\" />", "timestamp", CP key ) ;
    } else {
	iDataLine[iLinesTotal] = Mdata ; iDataUpdated[iLinesTotal] = updated ; iLinesTotal++ ;
	Mdata += snprintf( cDataPage+Mdata, sizeof(cDataPage)-Mdata, "<status device=\"%s\"", CP name ) ;

	if( l_key != 0 ) {
	    iDataLine[iLinesTotal] = Mdata ; iDataUpdated[iLinesTotal] = updated ; iLinesTotal++ ;
	    Mdata += snprintf( cDataPage+Mdata, sizeof(cDataPage)-Mdata, " type=\"%s\"", CP key ) ;
	}
	if( l_value != 0 ) {
	    iDataLine[iLinesTotal] = Mdata ; iDataUpdated[iLinesTotal] = updated ; iLinesTotal++ ;
	    Mdata += snprintf( cDataPage+Mdata, sizeof(cDataPage)-Mdata, " value=\"%s\"", CP value ) ;
	}
	if( n_attempts > 0 ) {
	    iDataLine[iLinesTotal] = Mdata ; iDataUpdated[iLinesTotal] = updated ; iLinesTotal++ ;
	    Mdata += snprintf( cDataPage+Mdata, sizeof(cDataPage)-Mdata, " fails=\"%d\"", n_attempts ) ;
	}

	iDataLine[iLinesTotal] = Mdata ; iDataUpdated[iLinesTotal] = updated ; iLinesTotal++ ;
	Mdata += snprintf( cDataPage+Mdata, sizeof(cDataPage)-Mdata, " />" ) ;

	// if( n_attempts ) strcat( cDataPage, "<font color=\"red\">" ) ;
    }
    iDataLine[iLinesTotal] = Mdata ; iDataUpdated[iLinesTotal] = updated ; iLinesTotal++ ;
    Mdata += snprintf( cDataPage+Mdata, sizeof(cDataPage)-Mdata, "\r\n" ) ;
}

void XMLServer::Postamble( void )
{
    iDataLine[iLinesTotal] = Mdata ; iDataUpdated[iLinesTotal] = 0 ;
    pthread_mutex_unlock( &xml_mutex ) ;
    bStatusReady = 1 ;
}

void XMLServer::ReceiveConnection( int w_i )
{
    int mode = GraspGet( w_i ) ;
    if( mode < 0 ) {
        ConnectionClose( w_i ) ;
        return ;
    }
    if( mode == 1 ) iUpdateOnly[w_i] = 0 ;
    else if( mode == 2 ) iUpdateOnly[w_i] = 1 ;

// Currently we don't handle any requests through this channel. Maybe we will ...
// But we don't slam the door shut either.
}

int XMLServer::GraspGet ( int w_i )
{
    char buffer[1024] ;
    int fdx = fds[w_i] ;

    int nread = read( fdx, buffer, sizeof(buffer)-1 ) ;
    if( nread <= 0 ) {
        // These can be confusing!
        // perror( "graspget" ) ;
        return( -1 ) ;
    }
    buffer[nread] = '\0' ;
    DODBG(0)(DBGDEV, "XML Request: <%s>.\n", buffer ) ;

    char *p = strpbrk( buffer, "\r\n" ) ;
    if( p ) *p = '\0' ;
    else return( 0 ) ;

    if( strcasecmp( buffer, "delta" ) == 0 ) {
	return( 2 ) ;
    } else if( strcasecmp( buffer, "full" ) == 0 ) {
	return( 1 ) ;
    }

    return( 0 ) ;
}

void XMLServer::ConnectionClose( int w_i )
{
    DODBG(11)(DBGDEV, "Closing %d\n", w_i ) ;
    ConnectionOff( fds[w_i] ) ;
    iInitial[w_i] = 0 ;
    iUpdateOnly[w_i] = 0 ;
    time_last[w_i] = 0 ;
    fds[w_i] = -1 ;
}

void XMLServer::BuildStatus( int cols )
{
}

static char sXMLHeader[] =
    "<?xml version=\"1.0\" standalone=\"yes\" ?>\r\n"
	"<!DOCTYPE xmlstream [\r\n"
	"<!ELEMENT xmlstream (state+) >\r\n"
	"<!ELEMENT state (timestamp,status*) >\r\n"
	"<!ATTLIST state mode (delta | full) \"full\">\r\n"
	"<!ATTLIST state count CDATA #REQUIRED>\r\n"
	"<!ELEMENT timestamp EMPTY>\r\n"
	"<!ATTLIST timestamp value CDATA #REQUIRED>\r\n"
	"<!ELEMENT status EMPTY>\r\n"
	"<!ATTLIST status device CDATA #REQUIRED>\r\n"
	"<!ATTLIST status type CDATA #REQUIRED>\r\n"
	"<!ATTLIST status value CDATA #IMPLIED>\r\n"
	"<!ATTLIST status fails CDATA #IMPLIED>\r\n"
	"]>\r\n"
	"<xmlstream>\r\n"
    ;

static char sXMLStateOpen[] = "<state %scount=\"%d\" >\r\n" ;
static char sXMLStateClose[] = "</state>\r\n" ;

void XMLServer::MeetRequest( int w_i )
{
    int fdx = fds[w_i] ;
    int i = 0 ;
    int M = 0 ;

    iCount[w_i]++ ;

#if 0

    char temp[256] ;

    if( iInitial[w_i] == 1 ) {
	write( fdx, sXMLHeader, sizeof(sXMLHeader)-1 ) ;
    }
    M = snprintf( temp, sizeof(temp), sXMLStateOpen, iUpdateOnly[w_i] ? "mode=\"delta\" " : "", iCount[w_i] ) ;
    write( fdx, temp, M ) ;

    #if 0
    if( iUpdateOnly[w_i] == 0 ) {
	write( fdx, sXMLStateOpenFull, sizeof(sXMLStateOpenFull)-1 ) ;
    } else {
	write( fdx, sXMLStateOpenDelta, sizeof(sXMLStateOpenDelta)-1 ) ;
    }
    #endif

    for( i = 0 ; i < iLinesTotal ; i++ ) {
	int len = iDataLine[i+1] - iDataLine[i] ;
	if( (iDataUpdated[i] == 1) || (iUpdateOnly[w_i] <= iDataUpdated[i]) )
	    write( fdx, cDataPage + iDataLine[i], len ) ;
    }
    iInitial[w_i] = 0 ;
    write( fdx, sXMLStateClose, sizeof(sXMLStateClose)-1 ) ;

#else

    static char *cTemp = NULL ;
    static int cTempLen = 0 ;

    if( cTemp == NULL ) {
	cTemp = (char*)malloc( 4*65536 ) ;
	cTempLen = 4*65536 ;
    }
    

    if( iInitial[w_i] == 1 ) {
	memcpy( cTemp+M, sXMLHeader, sizeof(sXMLHeader)-1 ) ;
	M += sizeof(sXMLHeader)-1 ;
    }

    M += snprintf( cTemp+M, cTempLen-M, sXMLStateOpen, iUpdateOnly[w_i] ? "mode=\"delta\" " : "", iCount[w_i] ) ;

    for( i = 0 ; i < iLinesTotal ; i++ ) {
	int len = iDataLine[i+1] - iDataLine[i] ;
	if( (iDataUpdated[i] == 1) || (iUpdateOnly[w_i] <= iDataUpdated[i]) ) {
	    memcpy( cTemp+M, cDataPage + iDataLine[i], len ) ;
	    M += len ;
	}
    }
    iInitial[w_i] = 0 ;

    memcpy( cTemp+M, sXMLStateClose, sizeof(sXMLStateClose)-1 ) ;
    M += sizeof(sXMLStateClose)-1 ;

    write( fdx, cTemp, M ) ;
#endif

}


void XMLServer::PleaseReport( int wline, int scol )
{
    snprintf( gStatus[wline][scol++], MAXDEVSTR, "P %d", bind_port ) ;
    snprintf( gStatus[wline][scol++], MAXDEVSTR, "Connections" ) ;
    snprintf( gStatus[wline][scol++], MAXDEVSTR, "%d", nfds ) ;
}   

