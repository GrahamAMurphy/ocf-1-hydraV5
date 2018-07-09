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
#include "mm4005.h"
#include "support.h"
#include "periodic.h"

MM4005::MM4005( void )
{
    devtype = TypeMM4005 ;
    devtypestring = "Newport MM 4005" ;
    cycle = 100000 ; // 100.0 secs
    delay = 10000 ; // 10.0 secs
    ucAxis[0][0] = UCP "Axis1" ;
    ucAxis[0][1] = UCP "Axis1" ;
    ucAxis[1][0] = UCP "Axis2" ;
    ucAxis[1][1] = UCP "Axis2" ;
    ucAxis[2][0] = UCP "Axis3" ;
    ucAxis[2][1] = UCP "Axis3" ;

    DODBG(9)(DBGDEV, "Leaving mm4005 constructor.\n" ) ; 
}

void MM4005::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: MM4005 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 7 ) ; // Three positions & states.

    u_char temp[256] ;
    const char *(fmts)[2] = { "%s position get", "%s state get" } ;

    for( int i = 0 ; i < nPeriodics ; i++ ) {
        snprintf( CP temp, 256, fmts[i], getAlias() ) ;
        mPeriodics[i] = PeriodicQ->Add( getDevice(), temp ) ;
    }

    StartThread() ;
}

int MM4005::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "axis1" ) == 0 ) {
	ucAxis[0][0] = UCP strdup( CP value ) ;
        return( true ) ;
    } else if( strcasecmp( CP key, "axis2" ) == 0 ) {
	ucAxis[1][0] = UCP strdup( CP value ) ;
        return( true ) ;
    } else if( strcasecmp( CP key, "axis3" ) == 0 ) {
	ucAxis[2][0] = UCP strdup( CP value ) ;
        return( true ) ;
    }
    return( Client::Parse( key, value ) ) ;
}

void MM4005::StartPeriodicTasks( void )
{
    if( delay < 0 ) delay = cycle ;
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
        PeriodicQ->Schedule( mPeriodics[i], delay + 500*i ) ;
        PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void MM4005::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int MM4005::DeviceOpen( void )
{
    StartPeriodicTasks() ;
    return( true ) ;
}

void MM4005::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

#define NOPOSN for( int AA=0; AA<3 ; AA++ ) { NoStatus(AA) ; }
#define NOSTATE for( int AA=0; AA<3 ; AA++ ) { NoStatus(3+AA) ; }

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int MM4005::Retrieve( u_char *resp, int mlen ) 
{
    u_char temp[256] ;
    int i ;

    if( strcasecmp( CP client_cmd.cmd[1], "position" ) == 0 ) {
        int nexc = Exchange( UCP "0TP", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOPOSN; SET_406_BADCON(resp,mlen); return(false); } 
	if( nexc == 0 ) { NOPOSN; SET_407_DEVTO(resp,mlen); return(true); } 

	double posn[3] ;
	int nscan = sscanf( CP temp, "1TP%lf,2TP%lf,3TP%lf",
	    posn+0, posn+1, posn+2 ) ;

	if( nscan != 3 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode response from motor:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    NOPOSN ;
	    return( true ) ;
	}

	for( i = 0 ; i < 3 ; i++ ) {
	    snprintf( CP temp, 64, "%.3f", posn[i] ) ;
	    AddStatus( i, UCP ucAxis[i][0], temp ) ;
	}

	snprintf( CP resp, mlen, "%.3f,%.3f,%.3f", posn[0], posn[1], posn[2] ) ;
	return( true ) ;
    }

    if( strcasecmp( CP client_cmd.cmd[1], "state" ) == 0 ) {
        int nexc = Exchange( UCP "0MS", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOSTATE; SET_406_BADCON(resp,mlen); return(false); } 
	if( nexc == 0 ) { NOSTATE; SET_407_DEVTO(resp,mlen); return(true); } 

	if( nexc < (4+1+4+1+4+1+4) ) {
	    DODBG(0)(DBGDEV, "Short response for %s state.\n", name ) ;
            errors++ ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    NOSTATE ;
	    return( true ) ;
	}

#if 0
    Description This command reads the motor status byte of the specified axis. If the axis
    number (xx) is missing or set to 0, the controller returns the motor status
    bytes for all four axes, separated by a comma.
    Each bit of the status byte represents a particular axis parameter, as
    described in the following table:
    Bit # Function Meaning for 0 1
    0 Axis in Motion NO YES
    1 Motor power ON OFF
    2 Motion direction Negative Positive
    3 Right (+) travel limit Not tripped Tripped
    4 Left (-) travel limit Not tripped Tripped
    5 Mechanical zero signal Low High
    6 Not used - Default
    7 Not used Default -
    The byte returned is in the form of an ASCII character. Converting the ASCII
    code to binary will give us the status bits values.
#endif

	// Make this a little more rigorous! GAM
	int state[3] ;
	int good = 0 ;
	good += strncmp( CP temp+ 0, "1MS", 3 ) ? 0 : 1 ;
	good += strncmp( CP temp+ 5, "2MS", 3 ) ? 0 : 1 ;
	good += strncmp( CP temp+10, "3MS", 3 ) ? 0 : 1 ;

	if( good != 3 ) {
	    DODBG(0)(DBGDEV, "Bad response for %s state.\n", name ) ;
            errors++ ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    NOSTATE ;
	    return( true ) ;
	}

	state[0] = temp[3] ;
	state[1] = temp[8] ;
	state[2] = temp[13] ;

	for( i = 0 ; i < 3 ; i++ ) {
	    u_char t_status1[256], t_status2[256] ;
	    snprintf( CP t_status1, 64, "State %s", ucAxis[i][0] ) ;
	    snprintf( CP t_status2, 64, "0x%02x", state[i] ) ;

	    AddStatus( i+3, t_status1, t_status2 ) ;
	}

	snprintf( CP resp, mlen, "0x%02x,0x%02x,0x%02x", state[0], state[1], state[2] ) ;
	return( true ) ;
    }

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", client_cmd.cmd[1], name ) ; 
    return( true ) ;
}

int MM4005::FindAxis( u_char *waxis ) 
{
    int i ;
    int j ;
    for( i = 0 ; i < 3 ; i++ ) {
	for( j = 0 ; j < 2 ; j++ ) {
	    if( strcasecmp( CP waxis, CP ucAxis[i][j] ) == 0 ) {
		return( i+1 ) ;
	    }
	}
    }
    return( 0 ) ;
}

int MM4005::Move( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    u_char temp[256] ;
    int axis = 0 ;
    u_char absolute = 'A' ;

    axis = FindAxis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not a recognized axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    double posn = strtod( CP client_cmd.cmd[3], 0 ) ;
    // Make sure the value is not preceding by white space.
    snprintf( CP temp, 64, "%f", posn ) ;
    u_char *q = temp ;
    while( *q == ' ' && *q != '\0' ) q++ ;

    if( strcasecmp( CP client_cmd.cmd[2], "move" ) == 0 ) absolute = 'R' ;

    snprintf( CP command, 256, "%1dP%c%s", axis, absolute, q ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

    return( true ) ;
}

int MM4005::Halt( u_char *resp, int mlen ) 
{
    int nexc = Exchange( UCP "ST", w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
    return( true ) ;
}

int MM4005::HandleRequest( void )
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
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "motor" ) == 0 ){
	retval = Motor( response, sizeof(response) ) ;
    } else if( ncmds == 2 && strcasecmp( CP client_cmd.cmd[1], "halt" ) == 0 ) {
	retval = Halt( response, sizeof(response) ) ;
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


int MM4005::Motor( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    int axis = 0 ;
    char mode = 0 ;

    axis = FindAxis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not a valid axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    if( strcasecmp( CP client_cmd.cmd[3], "on" ) == 0 ) {
	mode = 'O' ;
    } else if( strcasecmp( CP client_cmd.cmd[3], "off" ) == 0 ) {
	mode = 'F' ;
    }

    if( !mode ) {
	DODBG(0)(DBGDEV, "Not a motor mode (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    snprintf( CP command, 256, "%1dM%1c", axis, mode ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
    return( true ) ;
}

static const char* cClientOptions[] = {
    "position get",
    "state get",
    "(axis1|axis2|axis3) goto %f",
    "(axis1|axis2|axis3) move %f",
    "(axis1|axis2|axis3) motor [off|on]",
    "halt"
    } ;

int MM4005::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

