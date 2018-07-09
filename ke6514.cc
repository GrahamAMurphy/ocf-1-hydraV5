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
#include "ke6514.h"
#include "support.h"
#include "periodic.h"

KE6514::KE6514( void )
{
    devtype = TypeKE6514 ;
    devtypestring = "KE 6514" ;
    cycle = 30000 ; // 30.0 secs
    delay = 5000 ; // 10.0 secs
    DODBG(9)(DBGDEV, "Leaving ke6514 constructor.\n" ) ; 
}

void KE6514::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: KE6514 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 1 ) ; // One key state.

    u_char temp[256] ;
    snprintf( CP temp, 256, "%s get", getAlias() ) ;
    mPeriodics[0] = PeriodicQ->Add( getDevice(), temp ) ;

    StartThread() ;
}

void KE6514::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
	PeriodicQ->Schedule( mPeriodics[i], delay ) ;
	PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void KE6514::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int KE6514::DeviceOpen( void )
{
    int nexc ;
    u_char resp[256] ;
    const char *command = 
	"*RST;"
	":CONF:CURR:DC;:FORM:DATA ASCII;:FORM:SOUR2 ASC;"
	":SYST:ERR:CODE:ALL?" ;

    nexc = Exchange( command, w_timeo, resp, sizeof(resp), -r_timeo ) ;
    StatusLog->L_Command_U( name, command, resp ) ;
    if( nexc < 0 ) return( false ) ; // something happened.

    command = 
	":FORM:ELEM READ;"
	":SENS:FUNC 'CURR:DC';"
	":SENS:CURR:DC:RANG:AUTO 1;"
	":SYST:ERR:CODE:ALL?" ;

    nexc = Exchange( command, w_timeo, resp, sizeof(resp), -r_timeo ) ;
    StatusLog->L_Command_U( name, command, resp ) ;
    if( nexc < 0 ) return( false ) ; // something happened.

    command = ":SENS:FUNC?" ;
    nexc = Exchange( command, w_timeo, resp, sizeof(resp), r_timeo ) ;
    StatusLog->L_Command_U( name, command, resp ) ;
    if( nexc <= 0 ) return( false ) ; // something happened.
    fprintf( stderr, "%s: Startup function query response: %s\n", name, resp ) ;

    command = "INIT;"
	":SYST:ERR:CODE:ALL?" ;

    nexc = Exchange( command, w_timeo, resp, sizeof(resp), -r_timeo ) ;
    StatusLog->L_Command_U( name, command, resp ) ;
    if( nexc < 0 ) return( false ) ; // something happened.

    command = "READ?" ;
    nexc = Exchange( command, w_timeo, resp, sizeof(resp), r_timeo ) ;
    StatusLog->L_Command_U( name, command, resp ) ;
    if( nexc <= 0 ) return( false ) ; // something happened.

    StartPeriodicTasks() ;
    return( true ) ;
}

void KE6514::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

#define NOSTATUS        NoStatus(0)

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int KE6514::AddressKE6514( u_char *resp, int mlen )
{
    u_char tmp[256] ;

    if( strcasecmp( CP client_cmd.cmd[1], "get" ) == 0 ) {
        int nexc = Exchange( "READ?", w_timeo, tmp, sizeof(tmp), r_timeo ) ;
        if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(false) ; } 
	if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen) ; return(true) ; } 

	double value = 0;
	int nscan = sscanf( CP tmp, "%lf", &value ) ;
	if( nscan != 1 ) { 
	    NOSTATUS; 
	    SET_409_BADRESP(resp,mlen,tmp); 
	    return(true); 
	}

	snprintf( CP resp, mlen, "%.4e", value ) ;

	AddStatus( 0, UCP "DC Current", resp ) ;
	return( true ) ;
    }

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[1] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[1], name ) ; 
    return( true ) ;
}

int KE6514::HandleRequest( void )
{
    if( Client::HandleRequest() )
	return( true ) ;

    if( client_cmd.nCmds != 2 ) {
	mQueue->SetResponse( ERR_408_BADCMD ) ;
	return( true ) ; // null or pointless command, but not a nasty error.
    }

    u_char response[256] ;

    int retval = AddressKE6514( response, sizeof(response) ) ;
    mQueue->SetResponse( response ) ; 
    DODBG(0)( DBGDEV, "client response %s: <<%s>>\n", name, response ) ; 
    StatusLog->L_Command_U( name, mQueue->GetRequest(), response ) ; 
    return( retval ) ;
}

static const char* cClientOptions[] = {
    "get"
    } ;

int KE6514::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

