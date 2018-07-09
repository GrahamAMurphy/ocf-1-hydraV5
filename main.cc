#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#define FILE_H_NEED (1)

#include "global.h"
#include "main.h"
#include "stream.h"


#include "client.h"
#include "ke617.h"
#include "ke6514.h"
#include "filterwheel.h"
#include "httpserver.h"
#include "xmlserver.h"
#include "keyboard.h"
#include "logging.h"
#include "monochromator.h"
#include "mm4005.h"
#include "klingermc4.h"
#include "unidex11.h"
#include "unidex511.h"
#include "pm2813.h"
#include "prm100.h"
#include "periodic.h"
#include "server.h"
#include "sr630.h"
#include "labsphere.h"
#include "udts370.h"
#include "udts470.h"
#include "tempmon.h"
#include "thermalserver.h"
#include "newportTH.h"
#include "sbi4000.h"
#include "newmark.h"

static const int MajorTick = 50000 ;
static int vTopDev = -1 ;
static int EnableKeyboard = true ;
static int nKeyboard = -1 ;
static int log_not_blocking = true ;
static const char *sConfig = 0 ;
static int bConsoleStatusOn = true ;
static int bLogStatusOn = true ;
static int bLogLogOn = true ;

static int ArgC = 0 ;
static char **ArgV = NULL ;
static char **EnvP = NULL ;

int TopDev( void )
{
    return( vTopDev ) ;
}

static volatile int CheckPeriodics = 0 ;
static volatile int Ticks = 0 ;
static volatile int DoDebug = 0 ;
static volatile int bTimerRunning ;
static pthread_t p_Timer_thread ;
static sem_t sem_periodic ;


void * RunTimer( void *v_w_device )
{
    struct timespec ticker ;
    struct timespec request ;
    struct timespec remainder ;

    ticker.tv_sec = MajorTick / 1000000 ;
    ticker.tv_nsec = 1000 * (MajorTick % 1000000) ;

    request = ticker ;
    bTimerRunning = 1 ;

    while( 1 ) {
	int iStatus = nanosleep( &request, &remainder ) ;
	if( iStatus != 0 ) {
	    request = remainder ;
	    continue ;
	}
	if( bTimerRunning ) {
	    // fprintf( stderr, "Q" ) ;
	    CheckPeriodics++ ;
	    Ticks++ ;
	}
	request = ticker ;
	sem_wait( &sem_periodic ) ;
    }

    return( 0 ) ;
}

void SetupTimer( void )
{
    pthread_attr_t t_attribute ;

    DODBG(9)(DBGDEV, "About to create timer thread.\n" ) ;

    pthread_attr_init( &t_attribute ) ;
    pthread_create( &p_Timer_thread, &t_attribute, RunTimer, (void*)NULL ) ;
    DODBG(9)(DBGDEV, "Created timer thread.\n" ) ;
}

void TimerHold( void )
{
    bTimerRunning = 0 ;
}
void TimerRelease( void )
{
    bTimerRunning = 1 ;
}

void RequestDebug( int signal_was )
{
    if( signal_was == SIGUSR1 ) {
	DbgLevel++ ;
	fprintf( stderr, "Debug level = %d\n", DbgLevel ) ;
    }
    if( signal_was == SIGUSR2 ) {
	if( --DbgLevel < 0 ) DbgLevel = 0 ;
	fprintf( stderr, "Debug level = %d\n", DbgLevel ) ;
    }
    if( signal_was == 0 ) {
	DoDebug = 1 ;
    }
}

void BrokenPipe( int dummy )
{
    DODBG(10)( DBGDEV, "Broken pipe ignored.\r\n" ) ;
}

void DoRestart( int dummy )
{
    DODBG(0)(DBGDEV, "Restart.\r\n" ) ;
    fprintf( stderr, "Restart.\r\n" ) ;
    bDoRestart = true ;
    bRunning = false ;
}

void CloseOutAll( void )
{
    if( OCF_Status )
	OCF_Status->StopOps() ;

    pthread_cancel( p_Timer_thread ) ;

    for( int i = 0 ; i <= TopDev() ; i++ ) {
	if( ! streams[i]->IsEnabled() ) continue ;
	DODBG(2)( DBGDEV, "Stopping device: <%s>.\r\n", streams[i]->getName() ) ;
	streams[i]->StopOps() ;
	DODBG(9)( DBGDEV, "Device <%s> stopped.\r\n", streams[i]->getName() );
    }
}

void NiceExit( int dummy )
{
    DODBG(0)(DBGDEV, "Exiting.\r\n" ) ;
    fprintf( stderr, "Exiting.\r\n" ) ;
    bDoRestart = false ;
    bRunning = false ;
}

void PollableEvent( int dummy )
{
    fprintf( stderr, "Pollable event detected.\r\n" ) ;
    abort() ;
    fprintf( stderr, "Pollable event detected and ignored.\r\n" ) ;
}

void HostBasedDefaults( void )
{
    if( ! this_host ) return ;

    if( strcasecmp( CP this_host, "ocfmaster" ) == 0 ) {
	if( ! test_target_0 ) test_target_0 = UCP "sri-ocf-1.jhuapl.edu" ;
    } else if( strcasecmp( CP this_host, "sri-ocf-1" ) == 0 ) {
	if( ! test_target_0 ) test_target_0 = UCP "localhost" ;
    }
}

static void CreatePid( void )
{
    FILE *out = fopen( ".pid", "w" ) ;
    if( out ) {
	fprintf( out, "%d\n", getpid() ) ;
	fclose( out ) ;
	chmod( ".pid", 0666 ) ;
    } else {
	perror( ".pid" ) ;
    }
}

void DisplayKeyboardCommands( void )
{
    DODBG(0)( DBGDEV, "\007\r\n" ) ;
    DODBG(0)( DBGDEV, "To quit, type 'q'.\r\n" ) ;
    DODBG(0)( DBGDEV, "Type Control-T to display overall status.\r\n" ) ;
    DODBG(0)( DBGDEV, "Type Control-R to retry all inoperative connections.\r\n" ) ;
    DODBG(0)( DBGDEV, "Type Control-D to increase the debug level.\r\n" ) ;
    DODBG(0)( DBGDEV, "Type Control-E to decrease the debug level.\r\n" ) ;
    DODBG(0)( DBGDEV, "Type Control-] to suspend keyboard control.\r\n" ) ;
    DODBG(0)( DBGDEV, "\r\n" ) ;
}

static time_t start_time ;

int main( int argc, char **argv, char **envp )
{
    void PrintVersion( void ) ;

    time( &StartTicks ) ;

    signal( SIGHUP, DoRestart ) ;
    signal( SIGTERM, NiceExit ) ;
    // signal( SIGALRM, Triggered ) ;
    signal( SIGPIPE, BrokenPipe ) ;
    signal( SIGPOLL, PollableEvent ) ;
    signal( SIGUSR1, RequestDebug ) ;
    signal( SIGUSR2, RequestDebug ) ;

    ArgC = argc ;
    ArgV = argv ;
    EnvP = envp ;

    SetNameInUT() ;
    CreatePid() ;

    // MOVED THIS TO AVOID COMMANDLINE BEING OVERRIDDEN 2011-04-02
    SetThisHost() ;
    HostBasedDefaults() ;

    foreign( argc-1, argv+1 ) ;

    if( DbgFile ) {
	DBGDEV = fopen( DbgFile, "w" ) ;
	if( ! DBGDEV ) perror( DbgFile ) ;
    }
    if( ! DBGDEV ) DBGDEV = stderr ;

    setvbuf( DBGDEV, 0, _IONBF, 0 ) ;

    if( log_not_blocking ) { // add some debug.
	int fd, file_status ;
	fd = fileno( DBGDEV ) ;
	if( fd < 0 ) perror("blocking.fileno") ;
	file_status = fcntl( fd, F_GETFL ) ;
	if( file_status < 0 ) perror("blocking.fcntl.get") ;
	file_status |= O_NONBLOCK ;
	int status = fcntl( fd, F_SETFL, file_status ) ;
	if( status ) perror("blocking.fcntl.set") ;
    }

    PrintVersion() ;

    if( EnableKeyboard ) {
	// Has input been redirected or have we been forked?
	if( ! isatty(0) ) EnableKeyboard = false ;
	if( tcgetpgrp(0) != getpgid(0) ) EnableKeyboard = false ;
    }

    DODBG(0)(DBGDEV, "Keyboard input is %s\n",
	EnableKeyboard ? "Enabled" : "Disabled" ) ;

    SetupTimer() ;


    if( ! log_prefix ) log_prefix = UCP "Logs/Cal" ;
    if( ! bLogLogOn ) log_prefix = NULL ;

    time( &start_time ) ;
    StatusLog = new Logging( log_prefix ) ;
    if( ! StatusLog ) {
	DODBG(0)(DBGDEV, "Could not start logging.\r\n" ) ;
	exit(0) ;
    }
    StatusLog->L_Write_U( "main", "INFO", "HYDRA Starting." ) ;

    if( ! status_dir ) status_dir = UCP "State" ;
    if( ! bLogStatusOn ) status_dir = NULL ;

    OCF_Status = new OCFStatus( status_dir ) ;
    OCF_Status->SetInterval( 1 ) ;
    OCF_Status->StartThread() ;

    PeriodicQ = new Periodic() ;

    vTopDev = -1 ;
    if( EnableKeyboard ) {
	nKeyboard = 0 ;
	streams[nKeyboard] = new Keyboard ;
	streams[nKeyboard]->setName ( "keyboard" ) ;
	streams[nKeyboard]->setDevice( nKeyboard ) ;
	vTopDev = 0 ;
    }

    if( ! sConfig ) {
	DODBG(0)(DBGDEV, "No configuration file specified. Stopping.\r\n" ) ;
	exit(0) ;
    }

    DODBG(0)( DBGDEV, "Opening config file: %s\n", sConfig ) ;
    FILE *FINIT = fopen( sConfig, "r" ) ;
    if( FINIT == NULL ) {
	DODBG(0)( DBGDEV, "Could not open config file: %s\n", sConfig ) ;
	DODBG(0)( DBGDEV, "Cannot recover.\n" ) ;
	exit(0) ;
    }

    for( int i = 0 ; i < 16384 ; i++ ) {
        int status = InitializePorts( FINIT, i ) ;
        if( status ) break ;
    }
    fclose( FINIT ) ;

    if( TopDev() < ( EnableKeyboard ? 1 : 0 ) ) {
        DODBG(0)( DBGDEV, "No devices found.\r\n" ) ;
        DODBG(0)( DBGDEV, "Cannot proceed.\r\n" ) ;
        exit(0) ;
    }

    if( http_port ) {
	vTopDev++ ;
	streams[vTopDev] = HTTP = new HTTPServer ;
	streams[vTopDev]->setName ( "HTTP Server" ) ;
	streams[vTopDev]->setDevice( vTopDev ) ;
    }

    if( xml_port ) {
	vTopDev++ ;
	streams[vTopDev] = XML = new XMLServer ;
	streams[vTopDev]->setName ( "XML Server" ) ;
	streams[vTopDev]->setDevice( vTopDev ) ;
    }

    DODBG(4)( DBGDEV, "Starting up operations.\r\n" ) ;
    DODBG(0)( DBGDEV, "Major thread activity begins now.\n" ) ;
    for( int i = 0 ; i <= TopDev() ; i++ ) {
	if( ! streams[i]->IsEnabled() ) continue ;
	DODBG(0)( DBGDEV, "Starting device: <%s>.\r\n", streams[i]->getName() ) ;
	streams[i]->StartOps() ;
	DODBG(0)( DBGDEV, "Device <%s> started.\r\n", streams[i]->getName() );
    }
    DODBG(4)( DBGDEV, "Startup completed.\n" ) ;

    if( EnableKeyboard ) {
	DisplayKeyboardCommands() ;
    }

    DODBG(4)( DBGDEV, "Starting timer and central handler.\n" ) ;

    Loop() ;

    if( HTTP )
	delete HTTP ;

    if( XML )
	delete XML ;

    if( DBGDEV )
	fclose( DBGDEV ) ;
}

void Loop( void )
{
    fd_set inpmask ;
    int nfds, selectmax = 0 ;
    struct timeval timeout ;
    int kbfd = -1 ;
    Keyboard *pKeyboard = 0;

    selectmax = 0 ;
    if( EnableKeyboard ) {
	kbfd = streams[nKeyboard]->getFd() ;
	selectmax = 1 + kbfd ;
	pKeyboard = dynamic_cast<Keyboard*>(streams[nKeyboard]) ;
    }

    while( bRunning ) {
	if( CheckPeriodics ) RunPeriodics() ;

	FD_ZERO( &inpmask ) ;
	if( kbfd >= 0 )
            FD_SET( kbfd, &inpmask ) ;

	timeout.tv_sec = 0 ;
	timeout.tv_usec = 10000 ;

	nfds = select( selectmax, &inpmask, NULL, NULL, &timeout ) ;
	if( nfds == 0 ) continue ;

	if( nfds < 0 ) {
	    if( errno == 0 || errno == EINTR ) {
		continue ;
	    } else {
		perror( "select" ) ;
		DODBG(0)( DBGDEV, "Fatal error in Loop/select. Sorry!\n" ) ;
		exit(1) ;
	    }
	}

	if( (kbfd >= 0) && FD_ISSET( kbfd, &inpmask ) ) {
	    pKeyboard->HandleRx() ;
	}
    }

    if( kbfd >= 0 ) {
	pKeyboard->ResetKeyboard() ;
    }

    CloseOutAll() ;

    if( bDoRestart ) {
	int i ;
	void CloseFrom( int fdmin ) ;

	sleep( 2 ) ;
	CloseFrom( 3 ) ;
	for( i = 0 ; i < ArgC ; i++ ) {
	    fprintf( stderr, "%3d <%s>\n", i, ArgV[i] ) ;
	}
	fprintf( stderr, "Restarting now ...\n" ) ;
	sleep( 1 ) ;
	// execve( ArgV[0], ArgV, EnvP ) ;
	execvp( ArgV[0], ArgV ) ;
    }
}

void CloseFrom( int fdmin )
{
    int i ;
    int fdmax = getdtablesize() ;
    for( i = fdmin ; i < fdmax ; i++ ) 
        (void)close( i ) ;
}

void RunPeriodics( void )
{
    PeriodicQ->CheckAndQueue() ;
    UpdateStatus() ;
    CheckPeriodics = 0 ;
    if( DoDebug && bConsoleStatusOn ) {
	DisplayConsoleStatus(stderr) ;
	DoDebug = 0 ;
    }
    sem_post( &sem_periodic ) ;
}

void SetOptions( u_char *p_line )
{
    u_char l_name[2048] = "" ;
    u_char l_setup[2048] = "" ;
    int nscan; 
    bool flag_start = false, flag_enable = false ;

    nscan = sscanf( CP p_line, "%*s <%[^>]> <%[^>]>", l_name, l_setup ) ;
    if( nscan != 2 ) {
	DODBG(0)( DBGDEV, "1 Option not understood: %s\n", p_line ) ;
	exit(0) ;
    }

    if( strcasecmp( CP l_setup, "enable" ) == 0 ) {
	flag_start = true ; flag_enable = true ;
    } else if( strcasecmp( CP l_setup, "disable" ) == 0 ) {
	flag_start = true ; flag_enable = false ;
    } 

    // Brute force lookup, since this will be a small table
    // and is only looked at a few times during startup!
    for( int i = 0 ; i <= TopDev() ; i++ ) {
	const u_char *p = streams[i]->getName() ;
	if( strcmp( CP p, CP l_name ) != 0 ) continue ;
	if( flag_start ) {
	    streams[i]->Enable( flag_enable ) ;
	    return ;
	}
	int status = streams[i]->SetOptions( l_setup ) ;
	if( ! status ) {
	    DODBG(0)( DBGDEV, "2 Option not understood: %s\n", p_line ) ;
	    exit(0) ;
	}
	return ;
    }
    DODBG(0)( DBGDEV, "3 Option not understood: %s\n", p_line ) ;
    exit(0) ;
}

int InitializePorts( FILE *inp, int lineno )
{
    u_char line[4096] ;
    int lines = 0, pnt = 0 ;
    u_char *ucstatus ;
    u_char l_name[2048] = "" ;
    u_char l_type[2048] = "" ;
    int wport, nscan ;
    Stream *t_stream ;

    if( inp == NULL ) return( true) ;

    while( 1 ) {
	line[pnt] = '\0' ;
	ucstatus = UCP fgets( CP line+pnt, 2048, inp ) ;
	int i_pnt = strchomp( line ) ;
	if( *line == '#' || *line == '\0' ) {
	    if( ! ucstatus ) return( true ) ;
	    continue ;
	}

	pnt = i_pnt ;
	lines++ ;
	if( line[pnt] != '\\' ) break ;
	if( pnt > 2048 ) {
	    DODBG(0)( DBGDEV, "Line too long: <%s>\n", line ) ;
	    exit(0) ;
	}
    }
    nscan = sscanf( CP line, "%s ", l_name ) ;
    if( nscan != 1 ) {
	DODBG(0)( DBGDEV, "Line not understood: <%s>\n", line ) ;
	exit(0) ;
    }

    if( strcasecmp(CP l_name, "alias")  == 0 ) {
	FindAndInsertAlias( line ) ;
	return( false ) ;
    } else if( strcasecmp(CP l_name, "options")  == 0 ) {
	SetOptions( line ) ;
	return( false ) ;
    } else if( strcasecmp(CP l_name, "epoch") == 0 ) {
	int nscan = sscanf( CP line, "%*s %lf", &EpochMET ) ;
	if( nscan != 1 ) {
	    DODBG(0)( DBGDEV, "Line not understood: <%s>\n", line ) ;
	    exit(0) ;
	}

	time_t t_met = (time_t) EpochMET ;
	struct tm gm_met ;
	gmtime_r( &t_met, &gm_met ) ;
	DODBG(0)( DBGDEV, "Epoch for MET = %f %04d-%02d-%02dT%02d:%02d:%02dZ\n", 
	    EpochMET,
	    gm_met.tm_year+1900,
	    gm_met.tm_mon+1,
	    gm_met.tm_mday,
	    gm_met.tm_hour,
	    gm_met.tm_min,
	    gm_met.tm_sec ) ;

	return( false ) ;
    } else if( strcasecmp(CP l_name,"exit") == 0 ) {
	return( true ) ;
    }

    nscan = sscanf( CP line, "<%[^>]> %s", l_name, l_type ) ;

    if( nscan != 2 ) {
	DODBG(0)( DBGDEV, "Line not understood: <%s>\n", line ) ;
	return( true ) ;
    }

    wport = vTopDev + 1 ;
    DODBG(9)( DBGDEV, "%4d %2d Name: <%s>\n", lineno, wport, l_name ) ;
    DODBG(9)( DBGDEV, "%4d %2d Type: <%s>\n", lineno, wport, l_type ) ;

    if( strcasecmp(CP l_type,"network-server") == 0 ) {
	streams[wport] = t_stream = new Server() ;
    } else if( strcasecmp(CP l_type,"network-client") == 0 ) {
	streams[wport] = t_stream = new Client() ;
    } else if( strcasecmp(CP l_type,"network-client-tempmon") == 0 ) {
	streams[wport] = t_stream = new TempMon() ;
    } else if( strcasecmp(CP l_type,"network-client-monochromator") == 0 ) {
	streams[wport] = t_stream = new Monochromator() ;
    } else if( strcasecmp(CP l_type,"network-client-filterwheel") == 0 ) {
	streams[wport] = t_stream = new FilterWheel() ;
    } else if( strcasecmp(CP l_type,"network-client-sr630") == 0 ) {
	streams[wport] = t_stream = new SR630() ;
    } else if( strcasecmp(CP l_type,"network-client-mm4005") == 0 ) {
	streams[wport] = t_stream = new MM4005() ;
    } else if( strcasecmp(CP l_type,"network-client-klinger") == 0 ) {
	streams[wport] = t_stream = new KlingerMC4() ;
    } else if( strcasecmp(CP l_type,"network-client-unidex11") == 0 ) {
	streams[wport] = t_stream = new Unidex11() ;
    } else if( strcasecmp(CP l_type,"network-client-unidex511") == 0 ) {
	streams[wport] = t_stream = new Unidex511() ;
    } else if( strcasecmp(CP l_type,"network-client-ke617") == 0 ) {
	streams[wport] = t_stream = new KE617() ;
    } else if( strcasecmp(CP l_type,"network-client-ke6514") == 0 ) {
	streams[wport] = t_stream = new KE6514() ;
    } else if( strcasecmp(CP l_type,"network-client-pm2813") == 0 ) {
	streams[wport] = t_stream = new PM2813() ;
    } else if( strcasecmp(CP l_type,"network-client-prm100") == 0 ) {
	streams[wport] = t_stream = new PRM100() ;
    } else if( strcasecmp(CP l_type,"network-client-labsphere") == 0 ) {
	streams[wport] = t_stream = new Labsphere() ;
    } else if( strcasecmp(CP l_type,"network-client-udts370") == 0 ) {
	streams[wport] = t_stream = new UDTS370() ;
    } else if( strcasecmp(CP l_type,"network-client-udts470") == 0 ) {
	streams[wport] = t_stream = new UDTS470() ;
    } else if( strcasecmp(CP l_type,"network-server-thermal") == 0 ) {
	streams[wport] = t_stream = new ThermalServer() ;
    } else if( strcasecmp(CP l_type,"network-client-newportTH") == 0 ) {
	streams[wport] = t_stream = new NewportTH() ;
    } else if( strcasecmp(CP l_type,"network-client-sbi4000") == 0 ) {
	streams[wport] = t_stream = new SBI4000() ;
    } else if( strcasecmp(CP l_type,"network-client-newmark") == 0 ) {
	streams[wport] = t_stream = new Newmark() ;
    } else {
	DODBG(0)( DBGDEV, "\007 Unknown device: %s of type %s.\n", l_name, l_type ) ;
	exit(0) ;
    }

    t_stream->setName ( l_name ) ;
    t_stream->setDevice( wport ) ;
    vTopDev = wport ;

    InsertAlias( l_name, l_name ) ;

    return( false ) ;
}

#include <search.h>

void FindAndInsertAlias( u_char *p_line )
{
    u_char l_name[2048] = "" ;
    u_char l_alias[2048] = "" ;

    int nscan; 
    nscan = sscanf( CP p_line, "%*s <%[^>]> <%[^>]>", l_name, l_alias ) ;
    if( nscan != 2 ) {
	DODBG(0)( DBGDEV, "Alias not understood: %s\n", p_line ) ;
	return ;
    }
    if( ! InsertAlias( l_name, l_alias ) ) {
	DODBG(0)( DBGDEV, "Alias not understood: %s\n", p_line ) ;
	exit(0) ;
    }
}

int InsertAlias( u_char *p_name, u_char *p_alias )
{
    static int first_time = true ;

    if( first_time ) {
	first_time = false ;
	if( ! hcreate( 1024 ) ) {
	    perror( "hash create" ) ;
	    return( false ) ;
	}
    }
    
    // Brute force lookup, since this will be a small table
    // and is only looked at a few times during startup!
    for( int i = 0 ; i <= TopDev() ; i++ ) {
	const u_char *p = streams[i]->getName() ;
	if( strcmp( CP p, CP p_name ) != 0 ) continue ;

	// Convert alias to lower case.
	u_char *pAlias = UCP strdup( CP p_alias ) ;

	for( u_char *q = pAlias ; *q != '\0' ; q++ ) {
	    *q = tolower( *q ) ;
	}
	do {
	    char *p = strchr( CP pAlias, ' ' ) ;
	    if( p ) *p = '_' ;
	    else break ;
	} while(1) ;

	ENTRY newent, *ent ;
	newent.key = CP pAlias ;
	newent.data = (void*) i ;
	ent = hsearch( newent, FIND ) ;
	if( ent && ( i != (long)ent->data ) ) {
	    DODBG(0)( DBGDEV, "%s(dev=%ld) and %s(dev=%d) ambiguous.\n", ent->key, (long)ent->data, p_alias, i ) ;
	    exit(0) ;
	}

	streams[i]->setAlias( pAlias ) ;

	ent = hsearch( newent, ENTER ) ;
	if( ! ent ) {
	    perror( "hash setup" ) ;
	    return(false) ;
	}
	return(true) ;
    }
    return( false ) ;
}

void foreign( int argc, char **argv )
{
    char *ap ;

    while( argc > 0 ) {
        ap = *argv++ ; argc-- ;

        if( strcmp( ap, "--target" ) == 0 ) {
	    test_target_0 = UCP *argv ; argv++ ; argc-- ;
        } else if( strcmp( ap, "--config" ) == 0 ) {
	    sConfig = *argv ; argv++ ; argc-- ;
        } else if( strcmp( ap, "--debug-dir" ) == 0 ) {
	    DbgFile = (char*)malloc( 1024 ) ;
	    snprintf( DbgFile, 1024, "%s/%s.log", *argv, NameInUT ) ;
	    argv++ ; argc-- ;
        } else if( strcmp( ap, "--no-console-status" ) == 0 ) {
	    bConsoleStatusOn = false ;
        } else if( strcmp( ap, "--no-status" ) == 0 ) {
	    bLogStatusOn = false ;
        } else if( strcmp( ap, "--no-logging" ) == 0 ) {
	    bLogLogOn = false ;
        } else if( strcmp( ap, "--debug" ) == 0 ) {
	    DbgFile = *argv ; argv++ ; argc-- ;
        } else if( strcmp( ap, "--debug-level" ) == 0 ) {
	    DbgLevel = strtol( *argv, 0, 0 ) ; argv++ ; argc-- ;
        } else if( strcmp( ap, "--log-prefix" ) == 0 ) {
	    log_prefix = UCP *argv ; argv++ ; argc-- ;
        } else if( strcmp( ap, "--log-suffix" ) == 0 ) {
	    log_suffix = UCP *argv ; argv++ ; argc-- ;
        } else if( strcmp( ap, "--status-dir" ) == 0 ) {
	    status_dir = UCP *argv ; argv++ ; argc-- ;
        } else if( strcmp( ap, "--status-file" ) == 0 ) {
	    status_file = UCP *argv ; argv++ ; argc-- ;
        } else if( strcmp( ap, "--http" ) == 0 ) {
	    http_port = UCP *argv ; argv++ ; argc-- ;
        } else if( strcmp( ap, "--xml" ) == 0 ) {
	    xml_port = UCP *argv ; argv++ ; argc-- ;
	} else {
	    fprintf( stderr, "Unrecognized parameter: %s\n", ap ) ;
	    exit(0) ;
	}
    }
}

void UpdateStatus( void )
{
    static time_t willbe = 0 ;
    static time_t dbglog = 0 ;
    time_t now ;
    int i, j = 0 ;
    int maxcols ;

    time(&now) ;
    if( now < willbe ) return ;
    willbe = now + 4 ;
    status_time = now ;

    for( i = 0 ; i <= TopDev() ; i++ ) {
	if( streams[i]->getType() == Stream::TypeNone ) continue ;

	strncpy( gStatus[j][0], CP streams[i]->getName(), MAXDEVSTR ) ;

	if( char *c_alias = CP streams[i]->getAlias() ) 
	    strncpy( gStatus[j][2], c_alias, MAXDEVSTR ) ;
	else 
	    strncpy( gStatus[j][2], "", MAXDEVSTR ) ;

	for( int k = 3 ; k < MAXITEM ; k++)
	    strncpy( gStatus[j][k], "", MAXDEVSTR ) ;

	if( streams[i]->IsEnabled() ) {
	    strncpy( gStatus[j][1], "On", MAXDEVSTR ) ;
	    streams[i]->PleaseReport( j, 3 ) ;
	} else {
	    strncpy( gStatus[j][1], "Off", MAXDEVSTR ) ;
	}
	j++ ;
    }
    nStatus = j ;

    maxcols = 0 ;
    for( int k = 0 ; k < MAXITEM ; k++ ) {
	gMaxLen[k] = 0 ;
	for( j = 0 ; j < nStatus ; j++ ) {
	    int l_str = strlen( gStatus[j][k] ) ;
	    if( gMaxLen[k] < l_str ) gMaxLen[k] = l_str ;
	}
	if( gMaxLen[k] > 0 ) maxcols = k+1 ;
    }
    if( HTTP )
	HTTP->BuildStatus( maxcols ) ;

    if( now > dbglog && bConsoleStatusOn ) {
	DisplayConsoleStatus( DBGDEV ) ;
	dbglog = now + 60 ;
    }
}

void DisplayConsoleStatus( FILE *Out )
{
    int j, k, col, p_end ;
    char temp[512] ;

    struct tm *ut = gmtime( &status_time ) ;

    fprintf( Out, "Status at %04d-%02d-%02d %02d:%02d:%02d UT\r\n",
        ut->tm_year+1900,
        ut->tm_mon+1,
        ut->tm_mday,
        ut->tm_hour,
        ut->tm_min,
        ut->tm_sec ) ;

    for( j = 0 ; j < nStatus ; j++ ) {
	col = 0 ;
	memset( temp, ' ', 512 ) ;
	p_end = 0 ;

	for( k = 0 ; k < MAXITEM ; k++ ) {
	    if( k == 2 ) continue ;
	    if( gMaxLen[k] == 0 ) continue ;
	    int l_item = strlen( gStatus[j][k] ) ;

	    strcpy( temp+col, gStatus[j][k] ) ;
	    p_end = col + l_item ;
	    temp[p_end] = ' ' ;

	    col += gMaxLen[k] ;
	    temp[col++] = '|' ;
	    temp[col] = '\0' ;
	    // col += gMaxLen[k] + 1 ;
	}

	fprintf( Out, "%s\n", temp ) ;
    }
    fprintf( Out, "\n" ) ;
}

void ResetAllRetries( void )
{
    int wdev ;
    for( wdev = 0 ; wdev <= TopDev() ; wdev++ ) {
	if( ! streams[wdev]->IsAClient() ) continue ;
	Client *t = dynamic_cast<Client*>(streams[wdev]) ;
	t->ResetRetry() ;
    }
}
