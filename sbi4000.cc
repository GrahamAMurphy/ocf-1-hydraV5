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
#include "sbi4000.h"
#include "support.h"
#include "periodic.h"

SBI4000::SBI4000( void )
{
    devtype = TypeSBI4000 ;
    devtypestring = "SBI 4000 Blackbody Controller" ;
    cycle = 30000 ; // 30.0 secs
    delay = 5678 ; // 5.0 secs
    DODBG(9)(DBGDEV, "Leaving sbi4000 constructor.\n" ) ; 
}

void SBI4000::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: SBI4000 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 3 ) ; // Three key state.

    u_char temp[256] ;
    snprintf( CP temp, 256, "%s get temp", getAlias() ) ;
    mPeriodics[0] = PeriodicQ->Add( getDevice(), temp ) ;

    snprintf( CP temp, 256, "%s get status", getAlias() ) ;
    mPeriodics[1] = PeriodicQ->Add( getDevice(), temp ) ;

    snprintf( CP temp, 256, "%s get errors", getAlias() ) ;
    mPeriodics[2] = PeriodicQ->Add( getDevice(), temp ) ;

    StartThread() ;
}

void SBI4000::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
	PeriodicQ->Schedule( mPeriodics[i], (i+1)*delay ) ;
	PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void SBI4000::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int SBI4000::DeviceOpen( void )
{
    StartPeriodicTasks() ;
    return( true ) ;
}

void SBI4000::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

#define NOSTATUS	NoStatus(0)

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int SBI4000::AddressSBI4000( u_char *resp, int mlen )
{
    u_char command[256] ;
    u_char tmp[256] ;

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[1], "get" ) == 0 ) {

	if( STRCASECMP( client_cmd.cmd[2], "temp" ) == 0 ) {
	    int nexc = Exchange( UCP "MT", w_timeo, tmp, sizeof(tmp), r_timeo);
	    if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(false); }
	    if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen); return( true ) ; }
	    if( strncasecmp( CP tmp, "T=", 2 ) == 0 ) {
		double dTemp = strtod( CP tmp+2, NULL ) ;
		snprintf( CP resp, mlen, "%.1f", dTemp ) ;
		AddStatus( 0, UCP "Temperature", resp ) ;
		return( true ) ;
	    }
	    NOSTATUS; SET_409_BADRESP(resp,mlen,tmp); return(false);
	}
	if( STRCASECMP( client_cmd.cmd[2], "status" ) == 0 ) {
	    int nexc = Exchange( UCP "MS", w_timeo, tmp, sizeof(tmp), r_timeo);
	    if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(false); }
	    if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen); return( true ) ; }
	    if( strncasecmp( CP tmp, "SR=", 3 ) == 0 ) {
		strncpy( CP resp, CP tmp+3, mlen ) ;
		AddStatus( 1, UCP "Status", resp ) ;
		return( true ) ;
	    }
	    NOSTATUS; SET_409_BADRESP(resp,mlen,tmp); return(false);
	}
	if( STRCASECMP( client_cmd.cmd[2], "errors" ) == 0 ) {
	    int nexc = Exchange( UCP "ME", w_timeo, tmp, sizeof(tmp), r_timeo);
	    if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(false); }
	    if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen); return( true ) ; }
	    if( strncasecmp( CP tmp, "ER=", 3 ) == 0 ) {
		strncpy( CP resp, CP tmp+3, mlen ) ;
		AddStatus( 2, UCP "Errors", resp ) ;
		return( true ) ;
	    }
	    NOSTATUS; SET_409_BADRESP(resp,mlen,tmp); return(false);
	}
    }

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[1], "set" ) == 0 ) {

	double dTemp = strtod( CP client_cmd.cmd[2], NULL ) ;
	snprintf( CP command, sizeof(command), "D%05.1f", dTemp );
	fprintf( stderr, "Try: <%s>\n", command ) ;

	/* Don't worry about return value */
	int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
	if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

	strncpy( CP resp, ERR_107_IGN, mlen ) ;
	return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[1] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", client_cmd.cmd[1], name ) ;
    return( true ) ;
}

int SBI4000::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    if( client_cmd.nCmds < 2 ) {
	DODBG(0)(DBGDEV, "Null command: %s\n", client_cmd.cmd[1] ) ;
	mQueue->SetResponse( ERR_408_BADCMD ) ;
	return( true ) ;
    }

    u_char response[256] ;

    int retval = AddressSBI4000( response, sizeof(response) ) ;
    mQueue->SetResponse( response ) ;
    DODBG(5)( DBGDEV, "Response of %s: <%s>\n", name, response ) ;
    StatusLog->L_Command_U( name, mQueue->GetRequest(), response ) ;
    return( retval ) ;
}

static const char* cClientOptions[] = {
    "get temp",
    "get status",
    "get error",
    "set %5.1f"
    } ;

int SBI4000::ListOptions( int iN, u_char *cOption, int iOptionLength )
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
