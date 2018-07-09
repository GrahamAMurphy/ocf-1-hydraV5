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
#include "klingermc4.h"
#include "support.h"
#include "periodic.h"

KlingerMC4::KlingerMC4( void )
{
    devtype = TypeKlingerMC4 ;
    devtypestring = "Klinger MC4/MD4" ;
    cycle = 60000 ; // 60.0 secs
    delay = 10000 ;  // 10.0 secs
    ucAxis[0][0] = UCP "Axis1" ;
    ucAxis[0][1] = UCP "Axis1" ;
    ucAxis[1][0] = UCP "Axis2" ;
    ucAxis[1][1] = UCP "Axis2" ;
    ucAxis[2][0] = UCP "Axis3" ;
    ucAxis[2][1] = UCP "Axis3" ;
    ucAxis[3][0] = UCP "Axis4" ;
    ucAxis[3][1] = UCP "Axis4" ;

    DODBG(9)(DBGDEV, "Leaving KlingerMC4 constructor.\n" ) ; 
}

void KlingerMC4::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: KlingerMC4 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 5 ) ; // Four positions & states.

    u_char temp[256] ;
    const char *(fmts)[1] = { "%s position get" } ;

    for( int i = 0 ; i < nPeriodics ; i++ ) {
        snprintf( CP temp, 256, fmts[i], getAlias() ) ;
        mPeriodics[i] = PeriodicQ->Add( getDevice(), temp ) ;
    }

    StartThread() ;
}

int KlingerMC4::Parse( u_char *key, u_char *value )
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
    } else if( strcasecmp( CP key, "axis4" ) == 0 ) {
        ucAxis[3][0] = UCP strdup( CP value ) ;
        return( true ) ;
    }
    return( Client::Parse( key, value ) ) ;
}

void KlingerMC4::StartPeriodicTasks( void )
{
    if( delay < 0 ) delay = cycle ;
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
        PeriodicQ->Schedule( mPeriodics[i], delay + 500*i ) ;
        PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void KlingerMC4::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int KlingerMC4::DeviceOpen( void )
{
    int nexc ;
    u_char resp[256] ;

    const char *command = "FSFF" ;
    nexc = Exchange( command, w_timeo, resp, sizeof(resp), r_timeo ) ;
    // This is critical to operating correctly, so a timeout
    // forces a reconnect.
    if( nexc <= 0 ) return( false ) ;

    command = "?" ;
    nexc = Exchange( command, w_timeo, resp, sizeof(resp), r_timeo ) ;
    // This is critical to operating correctly, so a timeout
    // forces a reconnect.
    if( nexc <= 0 ) return( false ) ;

    StartPeriodicTasks() ;
    return( true ) ;
}

void KlingerMC4::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

#define NOSTATUS	for( int AA = 0 ; AA < 5 ; AA++ ) { NoStatus( AA ) ; }

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int KlingerMC4::Retrieve( u_char *resp, int mlen ) 
{
    u_char temp[256] ;
    int nexc ;
    static int query_mode = 0 ;
    int i ;

    const char *command = query_mode ? "?" : "" ;

    nexc = Exchange( command, w_timeo, temp, sizeof(temp), r_timeo ) ;
    if( nexc < 0 ) { NOSTATUS ; SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { NOSTATUS ; SET_407_DEVTO(resp,mlen) ; return( true ) ; } 

    int state[4] ;
    state[0] = temp[1] & 0xF ;
    state[1] = temp[3] & 0xF ;
    state[2] = temp[5] & 0xF ;
    state[3] = temp[7] & 0xF ;

    double posn[4] ;
    int nscan = sscanf( CP temp+8, "W=%lf X=%lf Y=%lf Z=%lf",
	posn+0, posn+1, posn+2, posn+3 ) ;

    if( nscan != 4 ) {
	DODBG(0)(DBGDEV, 
	    "Could not decode response from motor:%s.\r\n", temp ) ;
	query_mode = 1 - query_mode ;
	SET_409_BADRESP(resp,mlen,temp) ; 
	NOSTATUS ;
	return( true ) ;
    }

    for( i = 0 ; i < 4 ; i++ ) {
	snprintf( CP temp, 64, "%.1f", posn[i] ) ;
	AddStatus( 0, UCP ucAxis[i][0], temp ) ;
    }

    snprintf( CP temp, 64, "0x%02x%02x%02x%02x", 
	state[0], state[1], state[2], state[3] ) ;
    AddStatus( 4, UCP "State", temp ) ;

    if( strcasecmp( CP client_cmd.cmd[1], "position" ) == 0 ) {
	snprintf( CP resp, mlen, "%.1f,%.1f,%.1f,%.1f", 
	    posn[0], posn[1], posn[2], posn[3] ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "state" ) == 0 ) {
	snprintf( CP resp, mlen, "0x%02x%02x%02x%02x", 
	    state[0], state[1], state[2], state[3] ) ;
    } else {
        DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
    }
    return( true ) ;
}

int KlingerMC4::Move( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    u_char temp[256] ;
    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    double posn = strtod( CP client_cmd.cmd[3], 0 ) ;
    // Make sure the value is not preceding by white space.
    char dirn = 0 ;
    if( posn > 0 ) dirn = '+' ;
    else {
	posn = -posn ;
	dirn = '-' ;
    }

    snprintf( CP temp, 64, "%.0f", posn ) ;
    u_char *q = temp ;
    while( *q == ' ' && *q != '\0' ) q++ ;

    snprintf( CP command, 256, "%1c%1c;N%1c%s", dirn, axis, axis, q ) ;
    int nexc = Exchange( command, w_timeo, resp, mlen, r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_407_DEVTO(resp,mlen) ; return( true ) ; } 

    snprintf( CP command, 256, "M%1c", axis ) ;

    // Don't expect a reply.
    nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
    return( true ) ;
}

char KlingerMC4::Axis( u_char *s_axis )
{
    int iAxis = 4 ;
    u_char *ucTAxis = UCP "WXYZ" ;

    int i ;
    int j ;
    for( i = 0 ; i < 4 ; i++ ) {
        for( j = 0 ; j < 2 ; j++ ) {
            if( strcasecmp( CP s_axis, CP ucAxis[i][j] ) == 0 ) {
		iAxis = i ;
            }
        }
    }
    return( ucTAxis[iAxis] ) ;
}

int KlingerMC4::Goto( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    u_char temp[256] ;
    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    double posn = strtod( CP client_cmd.cmd[3], 0 ) ;
    // Make sure the value is not preceding by white space.
    snprintf( CP temp, 64, "%.0f", posn ) ;
    u_char *q = temp ;
    while( *q == ' ' && *q != '\0' ) q++ ;

    snprintf( CP command, 256, "P%1c%s", axis, q ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

    return( true ) ;
}

int KlingerMC4::SetRate( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    u_char temp[256] ;
    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    long new_rate = strtol( CP client_cmd.cmd[3], 0, 0 ) ;

    // Make sure the value is not preceded by white space.
    snprintf( CP temp, 64, "%ld", new_rate ) ;
    u_char *q = temp ;
    while( *q == ' ' && *q != '\0' ) q++ ;

    snprintf( CP command, 256, "R%1c%s", axis, q ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
    return( true ) ;
}

int KlingerMC4::Motor( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    char axis = Axis( client_cmd.cmd[1] ) ;
    char mode = 0 ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    if( strcasecmp( CP client_cmd.cmd[3], "on" ) == 0 ) {
	mode = 'E' ;
    } else if( strcasecmp( CP client_cmd.cmd[3], "off" ) == 0 ) {
	mode = 'D' ;
    }

    if( !mode ) {
	DODBG(0)(DBGDEV, "Not a motor mode (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    snprintf( CP command, 256, "K%1c%1c", axis, mode ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
    return( true ) ;
}

int KlingerMC4::Home( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    snprintf( CP command, 256, "O%1c", axis ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
    return( true ) ;
}

int KlingerMC4::Zero( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    snprintf( CP command, 256, "A%1c", axis ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
    return( true ) ;
}


int KlingerMC4::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    int retval = 0 ;
    u_char response[256] ;

    int ncmds = client_cmd.nCmds ;
    if( ncmds == 3 && strcasecmp( CP client_cmd.cmd[2], "get" ) == 0 ) {
	retval = Retrieve( response, sizeof(response) ) ;
    } else if( ncmds == 3 && strcasecmp( CP client_cmd.cmd[2], "home" ) == 0){
	retval = Home( response, sizeof(response) ) ;
    } else if( ncmds == 3 && strcasecmp( CP client_cmd.cmd[2], "zero" ) == 0){
	retval = Zero( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "goto" ) == 0 ) {
	retval = Goto( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "move" ) == 0 ) {
	retval = Move( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "motor" ) == 0 ){
	retval = Motor( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "rate" ) == 0 ){
	retval = SetRate( response, sizeof(response) ) ;
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
    "(axis1|axis2|axis3|axis4) home",
    "(axis1|axis2|axis3|axis4) zero",
    "(axis1|axis2|axis3|axis4) motor (on|off)",
    "(axis1|axis2|axis3|axis4) rate %d",
    "(axis1|axis2|axis3|axis4) goto %d",
    "(axis1|axis2|axis3|axis4) move %d"
    } ;

int KlingerMC4::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

