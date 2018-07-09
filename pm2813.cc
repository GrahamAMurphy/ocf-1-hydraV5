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
#include "pm2813.h"
#include "support.h"
#include "periodic.h"

PM2813::PM2813( void )
{
    devtype = TypePM2813 ;
    devtypestring = "PM 2813" ;
    cycle = 10000 ; // 10.0 secs
    delay = 100000 ; // 100.0 secs
    DODBG(9)(DBGDEV, "Leaving pm2813 constructor.\n" ) ; 
}

void PM2813::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: PM2813 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 6 ) ; // One key state.

    u_char temp[256] ;
    snprintf( CP temp, 256, "%s get", getAlias() ) ;
    mPeriodics[0] = PeriodicQ->Add( getDevice(), temp ) ;

    StartThread() ;
}

void PM2813::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
	PeriodicQ->Schedule( mPeriodics[i], delay ) ;
	PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void PM2813::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int PM2813::DeviceOpen( void )
{
    StartPeriodicTasks() ;
    return( true ) ;
}

void PM2813::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

#define NOSTATUS        NoStatus(0)

int PM2813::StatusUpdate( int isel, u_char *resp, int mlen )
{
    u_char command[256] ;
    u_char tmp[256] ;
    double dVoltage = 0;
    double dCurrent = 0;
    int nscan ;

    u_char ucName[256] ;
    u_char ucValue[256] ;

    snprintf( CP command, sizeof(command), ":INST:NSEL %1d", isel ) ;

    int nexc = Exchange( command, w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(0) ; } 
    if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen) ; return(1) ; } 

    nexc = Exchange( ":MEAS:VOLT?", w_timeo, tmp, sizeof(tmp), r_timeo ) ;
    if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(0) ; } 
    if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen) ; return(1) ; } 

    nscan = sscanf( CP tmp, "%lf", &dVoltage ) ;
    if( nscan != 1 ) { 
	NOSTATUS; 
	SET_409_BADRESP(resp,mlen,tmp); 
	return(1); 
    }

    snprintf( CP ucName, sizeof(ucName), "Channel %1d (V)", isel ) ;
    snprintf( CP ucValue, sizeof(ucValue), "%7.3f", dVoltage ) ;
    AddStatus( 2*(isel-1), ucName, ucValue ) ;

    nexc = Exchange( ":MEAS:CURR?", w_timeo, tmp, sizeof(tmp), r_timeo ) ;
    if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(0) ; } 
    if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen) ; return(1) ; } 

    nscan = sscanf( CP tmp, "%lf", &dCurrent ) ;
    if( nscan != 1 ) { 
	NOSTATUS; 
	SET_409_BADRESP(resp,mlen,tmp); 
	return(1); 
    }

    snprintf( CP ucName, sizeof(ucName), "Channel %1d (A)", isel ) ;
    snprintf( CP ucValue, sizeof(ucValue), "%7.3f", dCurrent ) ;
    AddStatus( 1+2*(isel-1), ucName, ucValue ) ;

    snprintf( CP resp, mlen, "%7.3f,%7.3f", dVoltage, dCurrent ) ;

    return( -1 ) ;
}

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int PM2813::AddressPM2813( u_char *resp, int mlen )
{
    if( strcasecmp( CP client_cmd.cmd[1], "get" ) == 0 ) {
	u_char ucAll[256] ;
	int m = 0 ;
	int iStatus ;

	iStatus = StatusUpdate( 1, resp, mlen ) ;
	if( iStatus >= 0 ) return( iStatus ? true : false ) ;
	m += snprintf( m + CP ucAll, sizeof(ucAll)-m, "%s,", resp ) ;
	
	iStatus = StatusUpdate( 2, resp, mlen ) ;
	if( iStatus >= 0 ) return( iStatus ? true : false ) ;
	m += snprintf( m + CP ucAll, sizeof(ucAll)-m, "%s,", resp ) ;

	iStatus = StatusUpdate( 3, resp, mlen ) ;
	if( iStatus >= 0 ) return( iStatus ? true : false ) ;
	m += snprintf( m + CP ucAll, sizeof(ucAll)-m, "%s", resp ) ;

	strncpy( CP resp, CP ucAll, mlen ) ;
	return( true ) ;
    }

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[1] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[1], name ) ; 
    return( true ) ;
}

int PM2813::HandleRequest( void )
{
    if( Client::HandleRequest() )
	return( true ) ;

    if( client_cmd.nCmds != 2 ) {
	mQueue->SetResponse( ERR_408_BADCMD ) ;
	return( true ) ; // null or pointless command, but not a nasty error.
    }

    u_char response[256] ;

    int retval = AddressPM2813( response, sizeof(response) ) ;
    mQueue->SetResponse( response ) ; 
    DODBG(0)( DBGDEV, "client response %s: <<%s>>\n", name, response ) ; 
    StatusLog->L_Command_U( name, mQueue->GetRequest(), response ) ; 
    return( retval ) ;
}


static const char* cClientOptions[] = {
    "get"
    } ;

int PM2813::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

