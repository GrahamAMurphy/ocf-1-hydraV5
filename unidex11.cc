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
#include "unidex11.h"
#include "support.h"
#include "periodic.h"

Unidex11::Unidex11( void )
{
    devtype = TypeUnidex11 ;
    devtypestring = "Unidex 11" ;
    cycle = 100000 ; // 100.0 secs
    delay = 9000 ; // 9.0 secs
    rate[0] = 1000 ;
    rate[1] = 1000 ;
    ucAxis[0][0] = UCP "Axis1" ;
    ucAxis[0][1] = UCP "Axis1" ;
    ucAxis[1][0] = UCP "Axis2" ;
    ucAxis[1][1] = UCP "Axis2" ;

    DODBG(9)(DBGDEV, "Leaving Unidex11 constructor.\n" ) ; 
}

void Unidex11::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: Unidex11 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 4 ) ; // Four positions.

    u_char temp[256] ;
    const char *(fmts)[1] = { "%s position get" } ;

    for( int i = 0 ; i < nPeriodics ; i++ ) {
        snprintf( CP temp, 256, fmts[i], getAlias() ) ;
        mPeriodics[i] = PeriodicQ->Add( getDevice(), temp ) ;
    }

    StartThread() ;
}

int Unidex11::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "axis1" ) == 0 ) {
        ucAxis[0][0] = UCP strdup( CP value ) ;
        return( true ) ;
    } else if( strcasecmp( CP key, "axis2" ) == 0 ) {
        ucAxis[1][0] = UCP strdup( CP value ) ;
        return( true ) ;
    }
    return( Client::Parse( key, value ) ) ;
}

void Unidex11::StartPeriodicTasks( void )
{
    if( delay < 0 ) delay = cycle ;
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
        PeriodicQ->Schedule( mPeriodics[i], delay + 500*i ) ;
        PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void Unidex11::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int Unidex11::DeviceOpen( void )
{
    StartPeriodicTasks() ;
    return( true ) ;
}

void Unidex11::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

int Unidex11::Clear( int times )
{
    u_char tmp[256] = "" ;
    for( int i = 0 ; i < times ; i++ ) {
        int nexc = Exchange( UCP "I AB *", w_timeo, tmp, sizeof(tmp), -r_timeo);
        if( nexc < 0 ) return( -1 ) ;
	if( nexc == 0 ) {
	    nexc = Exchange( UCP "I AB *", w_timeo, tmp, sizeof(tmp), -r_timeo);
	    DODBG(0)(DBGDEV, "Unidex buffer has been cleared.\r\n" ) ;
	    return( 1 ) ;
	}
    }
    return( 0 ) ;
}

#define NOPX	NoStatus(0) ;
#define NOPY	NoStatus(1) ;

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int Unidex11::Retrieve( u_char *resp, int mlen ) 
{
    u_char temp[256] ;
    int i ;

    if( strcasecmp( CP client_cmd.cmd[1], "position" ) == 0 ) {
        if( Clear( 5 ) != 1 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 

        int nexc = Exchange( UCP "PX", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOPX; SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { NOPX; SET_407_DEVTO(resp,mlen) ; return( true ) ; } 

	long posn[2] ;

	int nscan = sscanf( CP temp, "%ld", posn+0 ) ;
	if( nscan != 1 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode response from motor:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    NOPX ;
	    return( true ) ;
	}

        nexc = Exchange( UCP "PY", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOPY; SET_406_BADCON(resp,mlen); return(false); } 
	if( nexc == 0 ) { NOPY; SET_407_DEVTO(resp,mlen); return(true) ; } 

	nscan = sscanf( CP temp, "%ld", posn+1 ) ;
	if( nscan != 1 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode response from motor:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    NOPY ;
	    return( true ) ;
	}

        for( i = 0 ; i < 2 ; i++ ) {
            snprintf( CP temp, 64, "%ld", posn[i] ) ;
            AddStatus( i, UCP ucAxis[i][0], temp ) ;
        }

	snprintf( CP resp, mlen, "%ld,%ld", posn[0], posn[1] ) ;
	return( true ) ;
    }

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[2], name ) ; 

    return( true ) ;
}

int Unidex11::Move( u_char *resp, int mlen ) 
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

    if( Clear( 5 ) != 1 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 

    snprintf( CP command, 256, "P%1c", axis ) ;
    int nexc = Exchange( command, w_timeo, temp, sizeof(temp), r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_407_DEVTO(resp,mlen) ; return( true ) ; } 

    long curr_posn ;
    int nscan = sscanf( CP temp, "%ld", &curr_posn ) ;
    if( nscan != 1 ) {
        DODBG(0)(DBGDEV, 
            "Could not decode response from motor:%s.\r\n", temp ) ;
	SET_409_BADRESP(resp,mlen,temp) ;
        return( true ) ;
    }

    long new_posn = strtol( CP client_cmd.cmd[3], 0, 0 ) ;
    long move = new_posn - curr_posn ;

    // Make sure the value is not preceded by white space.
    snprintf( CP temp, 64, "%ld", move ) ;
    u_char *q = temp ;
    while( *q == ' ' && *q != '\0' ) q++ ;

    snprintf( CP command, 256, "I %1c F%d D%s *", axis, rate[axis-'X'], q ) ;

    // Don't expect a reply.
    nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

    return( true ) ;
}

char Unidex11::Axis( u_char *s_axis )
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

int Unidex11::SetRate( u_char *resp, int mlen ) 
{
    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    long new_rate = strtol( CP client_cmd.cmd[3], 0, 0 ) ;

    if( new_rate > 0 && new_rate < 10000 ) {
	snprintf( CP resp, mlen, "%d", rate[ axis - 'X' ] ) ;
	rate[ axis - 'X' ] = new_rate ;
	DODBG(3)( DBGDEV, "Set unidex rate for %c axis to %ld steps/s.\n", 
	    axis, new_rate ) ;
    } else {
	snprintf( CP resp, mlen, ERR_410_BADRATE ": %ld", new_rate ) ; 
    }

    return( true ) ;
}

int Unidex11::Goto( u_char *resp, int mlen ) 
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

    long new_posn = strtol( CP client_cmd.cmd[3], 0, 0 ) ;

    // Make sure the value is not preceded by white space.
    snprintf( CP temp, 64, "%ld", new_posn ) ;
    u_char *q = temp ;
    while( *q == ' ' && *q != '\0' ) q++ ;

    if( Clear( 5 ) != 1 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 

    snprintf( CP command, 256, "I %1c F%d D%s *", axis, rate[axis-'X'], q ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

    return( true ) ;
}

int Unidex11::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    int retval = 0 ;
    u_char response[256] ;

    int ncmds = client_cmd.nCmds ;
    if( ncmds == 3 && strcasecmp( CP client_cmd.cmd[2], "get" ) == 0 ) {
	retval = Retrieve( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "goto" ) == 0 ) {
	retval = Goto( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "move" ) == 0 ) {
	retval = Move( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "rate" ) == 0 ) {
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
    "(axis1|axis2) goto %d",
    "(axis1|axis2) move %d",
    "(axis1|axis2) rate %d"
    } ;

int Unidex11::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

