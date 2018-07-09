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
#include "newmark.h"
#include "support.h"
#include "periodic.h"

Newmark::Newmark( void )
{
    devtype = TypeNewmark ;
    devtypestring = "Newport MM 4005" ;
    cycle = 60000 ; // 100.0 secs
    delay = 5000 ; // 10.0 secs
    ucAxis[0][0] = UCP "X" ;
    ucAxis[0][1] = UCP "X" ;
    ucAxis[1][0] = UCP "Y" ;
    ucAxis[1][1] = UCP "Y" ;

    DODBG(9)(DBGDEV, "Leaving Newmark constructor.\n" ) ; 
}

void Newmark::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: Newmark being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 4 ) ; // Two positions & states.

    u_char temp[256] ;
    const char *(fmts)[2] = { "%s position get", "%s state get" } ;

    for( int i = 0 ; i < nPeriodics ; i++ ) {
        snprintf( CP temp, 256, fmts[i], getAlias() ) ;
        mPeriodics[i] = PeriodicQ->Add( getDevice(), temp ) ;
    }

    StartThread() ;
}

int Newmark::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "X" ) == 0 ) {
	ucAxis[0][0] = UCP strdup( CP value ) ;
        return( true ) ;
    } else if( strcasecmp( CP key, "Y" ) == 0 ) {
	ucAxis[1][0] = UCP strdup( CP value ) ;
        return( true ) ;
    }
    return( Client::Parse( key, value ) ) ;
}

void Newmark::StartPeriodicTasks( void )
{
    if( delay < 0 ) delay = cycle ;
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
        PeriodicQ->Schedule( mPeriodics[i], delay + 500*i ) ;
        PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void Newmark::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int Newmark::DeviceOpen( void )
{
    StartPeriodicTasks() ;
    return( true ) ;
}

void Newmark::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int Newmark::Retrieve( u_char *resp, int mlen ) 
{
    u_char temp[256] ;
    int i ;

    if( strcasecmp( CP client_cmd.cmd[1], "position" ) == 0 ) {
        int nexc = Exchange( UCP "TP", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen); return(false); } 
	if( nexc == 0 ) { SET_407_DEVTO(resp,mlen); return(true); } 

	long posn[2] ;
	int nscan = sscanf( CP temp, "%ld,%ld", posn+0, posn+1 ) ;
	if( nscan != 2 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode position response from controller:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    return( true ) ;
	}

	for( i = 0 ; i < 2 ; i++ ) {
	    snprintf( CP temp, 64, "%ld", posn[i] ) ;
	    AddStatus( i, UCP ucAxis[i][0], temp ) ;
	}

	snprintf( CP resp, mlen, "%ld,%ld", posn[0], posn[1] ) ;
	return( true ) ;
    }

    // We are using velocity as a surrogate for state right now.

    if( strcasecmp( CP client_cmd.cmd[1], "state" ) == 0 ) {
        int nexc = Exchange( UCP "TV", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen); return(false); } 
	if( nexc == 0 ) { SET_407_DEVTO(resp,mlen); return(true); } 

	long state[2] ;
	int nscan = sscanf( CP temp, "%ld,%ld", state+0, state+1 ) ;

	if( nscan != 2 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode velocity response from controller:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    return( true ) ;
	}

	for( i = 0 ; i < 2 ; i++ ) {
	    u_char t_status1[256], t_status2[256] ;
	    snprintf( CP t_status1, 64, "State %s", ucAxis[i][0] ) ;
	    snprintf( CP t_status2, 64, "%ld", state[i] ) ;
	    AddStatus( i+2, t_status1, t_status2 ) ;
	}

	snprintf( CP resp, mlen, "%ld,%ld", state[0], state[1] ) ;
	return( true ) ;
    }

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", client_cmd.cmd[1], name ) ; 
    return( true ) ;
}

char Newmark::Axis( u_char *s_axis )
{
    int iAxis = 2 ;
    u_char *ucTAxis = UCP "XY" ;

    int i ;
    int j ;
    for( i = 0 ; i < 2 ; i++ ) {
        for( j = 0 ; j < 2 ; j++ ) {
            if( strcasecmp( CP s_axis, CP ucAxis[i][j] ) == 0 ) {
                iAxis = i ;
            }
        }
    }
    return( ucTAxis[iAxis] ) ;
}

int Newmark::Move( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    u_char temp[256] ;
    u_char absolute = 'A' ;

    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not a recognized axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    long posn = strtol( CP client_cmd.cmd[3], 0, 0 ) ;
    // Make sure the value is not preceding by white space.
    snprintf( CP temp, 64, "%ld", posn ) ;
    u_char *q = temp ;
    while( *q == ' ' && *q != '\0' ) q++ ;

    if( strcasecmp( CP client_cmd.cmd[2], "move" ) == 0 ) absolute = 'R' ;

//  use MC (motion complete).
    snprintf( CP command, 256, 
	"P%c%c=%s;BG;MG\"@\",_RP%c;MC;MG\"@\",_RP%c", 
absolute, axis, q, axis, axis ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

    return( true ) ;
}

int Newmark::AbortMotion( u_char *resp, int mlen ) 
{
    int nexc = Exchange( UCP "AB", w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
    return( true ) ;
}

int Newmark::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    int retval = 0 ;
    u_char response[256] ;

    int ncmds = client_cmd.nCmds ;
    if( ncmds == 3 && strcasecmp( CP client_cmd.cmd[2], "get" ) == 0 ) {
	retval = Retrieve( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "goto" ) == 0 ) {
	retval = Move( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "move" ) == 0 ) {
	retval = Move( response, sizeof(response) ) ;
    } else if( ncmds == 2 && strcasecmp( CP client_cmd.cmd[1], "abort" ) == 0 ) {
	retval = AbortMotion( response, sizeof(response) ) ;
    } else {
	DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[1] ) ; 
	snprintf( CP response, sizeof(response), ERR_408_BADCMD ": %s to %s",
	    client_cmd.cmd[1], name ) ; 
	retval = true ;
    }

    mQueue->SetResponse( response ) ; 
    DODBG(0)( DBGDEV, "client response %s: <%s>\n", name, response ) ;
    StatusLog->L_Command_U( name, mQueue->GetRequest(), response ) ; 
    return( retval ) ;
}

static const char* cClientOptions[] = {
    "position get",
    "state get",
    "(X|Y) goto %d",
    "(X|Y) move %d",
    "abort"
    } ;

int Newmark::ListOptions( int iN, u_char *cOption, int iOptionLength )
{
    int M = 0 ;
    strcpy( CP cOption, "" ) ;

    int iMax = sizeof(cClientOptions)/sizeof(char*) ;

    if( 0 <= iN && iN < iMax ) {
        strncpy( CP cOption, cClientOptions[iN], iOptionLength ) ;
	M = strlen( CP cOption ) ;
    } else {
	M = Client::ListOptions( iN-iMax, cOption, iOptionLength ) ;
    }
    return( M ) ;
}

