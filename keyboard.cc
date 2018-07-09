#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>

#include <iostream>

#pragma implementation

#include "global.h"
#include "main.h"
#include "stream.h"
#include "keyboard.h"

static Keyboard *the_keyboard = 0 ;

Keyboard::Keyboard()
{
    inited = true ;
    the_keyboard = this ;
    devtypestring = "Keyboard" ;
    setFd( 0 ) ;
    DODBG(9)(DBGDEV, "Leaving keyboard constructor.\n" ) ;
}

void Keyboard::StartOps( void )
{
    Keyboard::InitializeKeyboard() ;
    DODBG(9)(DBGDEV, "Keyboard initialized.\n" ) ;
}

static struct {
    int control, value ;
} s_controls[] = {
    { VQUIT, 034 }, /* Adopt telnet ^] for suspend. */
    { VSUSP, 035 }, /* Adopt telnet ^] for suspend. */
    { VSTART, 0 }, /* I think we could use ^Q on this side. */
    { VSTOP,  0 } /* I think we could use ^S on this side. */
} ;

void ProcessContinued( int dummy ) ;

static int saw_a_suspend = false ;
void ProcessSuspended( int dummy )
{
    signal( SIGTSTP, ProcessSuspended ) ;
    signal( SIGCONT, ProcessContinued ) ;
    DODBG(9)(DBGDEV, "Suspending process.\n" ) ;

    saw_a_suspend = true ;
    signal( SIGTSTP, SIG_DFL ) ;

    #ifdef DSIGRESTART // this doesn't appear to work on Cygwin ...
    siginterrupt( SIGTSTP, iSigRestart ) ;
    siginterrupt( SIGCONT, iSigRestart ) ;
    #endif

    if( the_keyboard ) the_keyboard->ResetKeyboard() ;

    kill( getpid(), SIGTSTP ) ;
}

void ProcessContinued( int dummy )
{
    signal( SIGTSTP, ProcessSuspended ) ;
    signal( SIGCONT, ProcessContinued ) ;

    #ifdef DSIGRESTART // this doesn't appear to work on Cygwin ...
    siginterrupt( SIGTSTP, iSigRestart ) ;
    siginterrupt( SIGCONT, iSigRestart ) ;
    #endif

    if( saw_a_suspend ) {
	if( the_keyboard ) the_keyboard->InitializeKeyboard() ;
    }
    saw_a_suspend = false ;

}

void Keyboard::InitializeKeyboard( void )
{
    int status ;
    u_int dcc ;

    signal( SIGTSTP, ProcessSuspended ) ;
    signal( SIGCONT, ProcessContinued ) ;

    #ifdef DSIGRESTART // this doesn't appear to work on Cygwin ...
    siginterrupt( SIGTSTP, iSigRestart ) ;
    siginterrupt( SIGCONT, iSigRestart ) ;
    #endif

    if( inited ) {
/*  Set up keyboard to use raw mode but with signals */
        status = tcgetattr( 0, &ncontrl ) ;

        memcpy( (void*)&scontrl, (void*)&ncontrl, sizeof(ncontrl) ) ;
        memcpy( (void*)&save_contrl, (void*)&ncontrl, sizeof(ncontrl) ) ;

        for( dcc = 0 ; dcc < ELEMENTS(s_controls) ; dcc++ ) {
            int scc = s_controls[dcc].control ;
            if( s_controls[dcc].value == 0 )
                s_controls[dcc].value = ncontrl.c_cc[scc] ;
        }
        memset( ncontrl.c_cc, '\377', NCCS ) ;

        for( dcc = 0 ; dcc < ELEMENTS(s_controls) ; dcc++ ) {
            int scc = s_controls[dcc].control ;
            ncontrl.c_cc[scc] = s_controls[dcc].value ;
        }

        ncontrl.c_cc[VMIN] = 1 ;
        ncontrl.c_cc[VTIME] = 0 ;

        ncontrl.c_lflag = ISIG ;
        ncontrl.c_line = 0 ;
        // ncontrl.c_iflag &= ~(ICRNL) ;
        inited = false ;
    }

    status = tcsetattr( 0, TCSANOW, &ncontrl ) ;
    if( status == -1 ) perror("ioctl:inp") ;
}

void Keyboard::HandleRx( void )
{
    int nread ;
    int unused = 0 ;
    u_char buffer[256] ;
    nread = read( 0, buffer, 256 ) ;
    if( buffer[0] == 'q' ) {
	DODBG(0)( DBGDEV, "Exiting.\r\n" ) ;
	bRunning = false ;
    }
    for( int i = 0 ; i < nread ; i++ ) {
	if( buffer[i] == '' ) { RequestDebug(0) ; break ; }
	if( buffer[i] == '' ) { ResetAllRetries() ; break ; }
	if( buffer[i] == '' ) { RequestDebug(SIGUSR1) ; break ; }
	if( buffer[i] == '' ) { RequestDebug(SIGUSR2) ; break ; }
	if( buffer[i] == 'q' ) { bRunning = false ; break ; }
	if( buffer[i] == 'Q' ) { bRunning = false ; bDoRestart = true ; break ; }
	unused++ ;
    }
    if( unused ) {
	DisplayKeyboardCommands() ;
    }
}

void Keyboard::ResetKeyboard( void )
{
    int status = tcsetattr( 0, TCSANOW, &save_contrl ) ;
    if( status == -1 ) perror("ioctl:inp") ;
}
