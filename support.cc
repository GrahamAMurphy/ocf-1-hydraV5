#pragma implementation

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "global.h"
#include "support.h"

void SetSocketOptions( int fd, int options )
{
    if (options & (1<<SO_DEBUG) ) {
	int tmp = -1 ;
	if (setsockopt(fd, SOL_SOCKET,SO_DEBUG,(void*)&tmp, sizeof(tmp)) < 0) {
	    DODBG(4)( stderr, "Error. setsockopt/debug.\n" ) ;
	    perror("setsocket:debug") ;
	}
	DODBG(4)(stderr, "Set SO_DEBUG\n" ) ;
    }

    if (options & (1<<SO_REUSEADDR) ) {
	int tmp = -1 ;
	if (setsockopt(fd, SOL_SOCKET,SO_REUSEADDR,(void*)&tmp, sizeof(tmp))<0){
	    DODBG(4)( stderr, "Error. setsockopt/reuse.\n" ) ;
	    perror("setsocket:reuse") ;
	}
	DODBG(4)(stderr, "Set SO_REUSEADDR\n" ) ;
    }

    if (options & (1<<SO_KEEPALIVE) ) {
	int tmp = -1 ;
	if (setsockopt(fd, SOL_SOCKET,SO_KEEPALIVE,(void*)&tmp, sizeof(tmp))<0){
	    DODBG(4)( stderr, "Error. setsockopt/keepalive.\n" ) ;
	    perror("setsocket:keepalive") ;
	}
	DODBG(4)(stderr, "Set SO_KEEPALIVE\n" ) ;
    }

#ifdef linux
    if (options & (1<<SO_SNDTIMEO) ) {
	struct timeval tv ;
	tv.tv_sec = 5 ;
	tv.tv_usec = 0 ;
	if (setsockopt(fd, SOL_SOCKET,SO_SNDTIMEO,(void*)&tv, sizeof(tv))<0){
	    DODBG(4)( stderr, "Error. setsockopt/timeout.\n" ) ;
	    perror("setsocket:sndtimeo") ;
	}
	DODBG(4)(stderr, "Set SO_SNDTIMEO\n" ) ;
    }
#endif // linux

}

int GetPortNumber( u_char *sport )
{
    u_long portno = 0 ;
    struct servent *sp ;
    char *bport ;
    u_char tport[128] ;
    u_char *protocol ;

    if( ! sport ) 
	return( -1 ) ;

    strncpy( CP tport, CP sport, 128 ) ; /* Make a copy of input string */
    protocol = UCP strchr( CP tport, '/' ) ;
    if( protocol ) {
	*protocol = '\0' ;
	protocol++ ;
    } else {
	protocol = UCP "tcp" ;
    }

    // portno = strtoul( (const char*) tport, (char**)&bport, 0 ) ;
    portno = strtoul( (const char*) tport, (char**)(&bport), 0 ) ;
    DODBG(9)( stderr, "portnumber: <%s> <%s> %ld\n", tport, bport, portno ) ;

    if( *bport != '\0' ) {
	sp = getservbyname( CP tport, CP protocol ) ;
	if (sp == NULL) {
	    return( -1 ) ;
	}
	DODBG(9)( stderr, "Got service by name.\n" ) ;
	DODBG(9)( stderr, "s_name: %s\n", sp->s_name ) ;
	DODBG(9)( stderr, "s_aliases: %s\n", *(sp->s_aliases)) ;
	DODBG(9)( stderr, "s_port: %d\n", sp->s_port ) ;
	DODBG(9)( stderr, "s_proto: %s\n", sp->s_proto ) ;
	portno = ntohs( sp->s_port ) ;
	DODBG(9)( stderr, "portno: %ld\n", portno ) ;
    } 

    return( portno ) ;
}

int GetProtocol( u_char *sport )
{
    u_char tport[128] ;
    u_char *protocol ;
    struct protoent *s_proto ;

    strncpy( CP tport, CP sport, 128 ) ; /* Make a copy of input string */
    protocol = UCP strchr( CP tport, '/' ) ;
    if( protocol ) {
	*protocol = '\0' ;
	protocol++ ;
    } else {
	protocol = UCP "tcp" ;
    }
    s_proto = getprotobyname( CP protocol ) ;

    if( s_proto ) {
	DODBG(8)( stderr, "%08lx %d <%s> from %s\n",
	    (long)s_proto, s_proto->p_proto, s_proto->p_name, protocol ) ;
	return( s_proto->p_proto ) ;
    } else {
	DODBG(0)( stderr, "Unknown protocol\n" ) ;
	return 0 ;
    }
}

bool GenericParser( sParse &in, u_char *del_phrase, u_char *del_value )
{
    char *p1 = (char*)(in.in) ;
    char *p2 = strsep( &p1, CP del_phrase ) ;
    if( ! p2 )
        return( false ) ;

    in.in = (u_char*)p1 ;
    char *p3 = strsep( &p2, CP del_value ) ;

    in.key = (u_char*)p3 ;
    in.value = (u_char*)p2 ;
    return( true ) ;
}

int strchomp( u_char *in )
{
    u_char *p, *q ;
    int off_p ;

    int sp_len = strspn( CP in, " \t" ) ;
    if( sp_len ) {
	int str_len = strlen( CP in ) ;
	memmove( in, in+sp_len, str_len-sp_len+1 ) ;
    } 

    p = in ;
    q = UCP strpbrk( CP p, "\r\n" ) ;

    if( ! q ) { // Line is not terminated by a line feed.
	q = p + strlen(CP p) ;
    }
    *q = ' ' ;

    off_p = q - p ;
    for( ; off_p >= 0 ; off_p-- ) {
	if( p[off_p] != ' ' && p[off_p] != '\t' ) return( off_p ) ;
	p[off_p] = '\0' ;
    }
    return( 0 );
}

int FlushStream( int fd, bool do_write )
{
    u_char sbuff[64] ;
    int maxs = fd + 1 ;
    struct timeval timeout = { 0, 0 } ;
    fd_set inpmask ;
    int loopy = 0 ;
    int nread = 0 ;

    FD_ZERO(&inpmask) ;
    FD_SET( fd, &inpmask ) ;

    while( select( maxs, &inpmask, NULL, NULL, &timeout ) ) {
	nread = read( fd, sbuff, 64 ) ;
	if( nread <= 0 ) break ;
	if( do_write )
	    write( 2, sbuff, nread ) ;
	loopy++ ;
	if( loopy > 50 ) break ;
    }
    return( (nread>=0) ) ;
}

int TokenizeRequest( const u_char *input, sCmd &output )
{
    int i ;
    int ntoks = 0 ;

    strncpy( CP output.line, CP input, TOKEN_CMD_MAXLINE ) ;

    u_char *p = output.line ;
    u_char *q ;

    for( i = 0 ; i < TOKEN_CMD_MAXARGS ; i++ ) output.cmd[i] = 0 ;

    for( i = 0 ; i < (TOKEN_CMD_MAXARGS-1) ; i++ ) {
	q = p + strspn( CP p, " \t\r\n" ) ; // q points to first non-white
	// fprintf( stderr, "%d %08lx %08lx %02x\n", ntoks, p, q, (q!=NULL?*q:'/') ) ;

	if( *q == '\0' ) break ;
	// output.cmd[i] = q ;
	output.cmd[ntoks] = q ;
	ntoks++ ;

	p = q + strcspn( CP q, " \t\r\n" ) ; // p points to end of non-white
	if( *p == '\0' ) break ;
	*p = '\0' ; p++ ;
    }
    output.nCmds = ntoks ;
    return( ntoks ) ;
}

void TimerAdd( volatile struct timeval &c, volatile struct timeval &a, volatile struct timeval &b )
{
    c.tv_sec = a.tv_sec + b.tv_sec ;
    long tmp = a.tv_usec + b.tv_usec ;
    c.tv_sec += (tmp / 1000000 ) ;
    c.tv_usec = tmp % 1000000 ;
}

void SetThisHost( void )
{
    char host[256] ;
    int status = gethostname( host, 256 ) ;
    if( status ) {
	perror( "SetThisHost" ) ;
	return ;
    }

    DODBG(0)( stderr, "This host is named: %s\n", host ) ;
    char *p = strchr( host, '.' ) ;
    if( p ) *p = '\0' ;

    this_host = UCP strdup( host ) ;
}

int mktree( const u_char *dirname )
{
    int status = 0 ;
    u_char dirwork[1024] ;
    u_char *p = dirwork ;

    strncpy( CP dirwork, CP dirname, 1024 ) ;

    while( true ) {
        p = UCP strchr( CP p, '/' ) ;
        if( p ) *p = '\0' ;
        status = mkdir( CP dirwork, 0777 ) ;

        if( status && errno != EEXIST ) {
            perror( CP dirname ) ;
            return( -1 ) ;
        }
	if( !p ) break ;
        *p = '/' ; p++ ;
    }
    return( 0 ) ;
}

// Upgrade to handle threads. gethostbyname_r appears to occasionally
// cause a weird crash. Not sure about this, but I'm very suspicious.
// So, I'm going back to gethostbyname and mutex'ing !

int GetHostnameInfo( u_char *machine_name, struct sockaddr_in *sin, 
    u_char *full, u_char *ip )
{
    struct in_addr hostaddr ;
    struct hostent *hostisat ;
    int alen ;
    u_char *px ;
    int retval = true ;

    if( machine_name == NULL || *machine_name == '\0' ) return( false ) ;

    static int first = true ;
    static pthread_mutex_t gethostbyname_mutex ;
    if( first ) {
	pthread_mutexattr_t mutex_attr ;
	pthread_mutexattr_init( &mutex_attr ) ;
	pthread_mutexattr_settype( &mutex_attr, PTHREAD_MUTEX_NORMAL ) ;
	pthread_mutexattr_setprotocol( &mutex_attr, PTHREAD_PRIO_INHERIT ) ;

	pthread_mutex_init( &gethostbyname_mutex, &mutex_attr ) ;
	first = false ;
    }
    DODBG(11)(stderr, "About to lock mutex for gethostbyname.\n" ) ;
    pthread_mutex_lock( &gethostbyname_mutex ) ;
    DODBG(11)(stderr, "Successful lock of mutex for gethostbyname\n" ) ;

    hostisat = gethostbyname( CP machine_name ) ;
    if( hostisat ) {
	char **p ;

	DODBG(4)( stderr, "Target name: %s\n", hostisat->h_name ) ;
	if( full )
	    strcpy( CP full, CP hostisat->h_name ) ;

	p = hostisat->h_aliases ;
	while( *p ) {
	    DODBG(9)( stderr, "Target aliases: %s\n", *p ) ;
	    p++ ;
	}
	DODBG(11)( stderr, "Target h_addrtype: %d\n", hostisat->h_addrtype ) ;
	alen = hostisat->h_length ;
	DODBG(11)( stderr, "Target h_length: %d\n", alen ) ;

	p = hostisat->h_addr_list ;
	while( *p ) {
	    memcpy( (void*)&hostaddr, *p, alen ) ;
	    break ;
	    p++ ;
	}

	px = UCP inet_ntoa( hostaddr ) ;
	if( ip != NULL )
	    strcpy( CP ip, CP px ) ;

	DODBG(4)( stderr, "Target machine's IP: %s\n", px ) ;

	if( sin != 0 ) {
	    memset( (u_char*)sin, 0, sizeof(*sin) ) ;
	    sin->sin_family = hostisat->h_addrtype ;
	    memcpy(  (void*)&(sin->sin_addr), (void*)&hostaddr, alen ) ;
	}

    } else {
	DODBG(0)( stderr, "Could not translate name: <%s>\n", machine_name ) ;
	retval = false ;
    }

    pthread_mutex_unlock( &gethostbyname_mutex ) ;
    DODBG(11)(stderr, "Successfully unlocked mutex for gethostbyname.\n" ) ;

    return( retval ) ;
}

void SetNameInUT( void )
{
    struct tm *ut = gmtime( &StartTicks ) ;
    
    snprintf( CP NameInUT, sizeof(NameInUT), 
	"%04d%02d%02d_%02d%02d%02d",
        ut->tm_year+1900,
        ut->tm_mon+1,
        ut->tm_mday,
        ut->tm_hour,
        ut->tm_min,
        ut->tm_sec ) ;
}

static u_char static_uptime[256] ;

u_char *GetUptime( void )
{
    time_t now ;
    time(&now) ;
    long delta ;
    delta = now - StartTicks ;

    int n_mins = delta / 60 ;
    int n_secs = delta - n_mins * 60 ;
    int n_hours = n_mins / 60 ;
    n_mins = n_mins - n_hours * 60 ;
    int n_days = n_hours / 24 ;
    n_hours = n_hours - n_days * 24 ;
    if( n_days ) {
	snprintf( CP static_uptime, sizeof(static_uptime), 
	    "%d:%02d:%02d:%02d", n_days, n_hours, n_mins, n_secs ) ;
	return( static_uptime ) ;
    }
    if( n_hours ) {
	snprintf( CP static_uptime, sizeof(static_uptime), 
	    "%d:%02d:%02d", n_hours, n_mins, n_secs ) ;
	return( static_uptime ) ;
    }
    if( n_mins ) {
	snprintf( CP static_uptime, sizeof(static_uptime), 
	    "%d:%02d", n_mins, n_secs ) ;
	return( static_uptime ) ;
    }
    snprintf( CP static_uptime, sizeof(static_uptime), "00:%d", n_secs ) ;
    return( static_uptime ) ;
}

void SetCloseOnExec( int fd )
{
    long lFD_FLAG ;
    lFD_FLAG = fcntl( fd, F_GETFD, NULL ) ;
    if( lFD_FLAG >= 0 ) {
	lFD_FLAG |= FD_CLOEXEC ;
	(void)fcntl( fd, F_SETFD, lFD_FLAG ) ;
    }
}

void SuppressQuotes( u_char *p )
{
    if( p == NULL )
	return ;
    while( *p != '\0' ) {
	if( *p == '\"' ) *p = 'Q' ;
	p++ ;
    }
}
