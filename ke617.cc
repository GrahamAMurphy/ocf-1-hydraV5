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
#include "ke617.h"
#include "support.h"
#include "periodic.h"

KE617::KE617( void )
{
    devtype = TypeKE617 ;
    devtypestring = "KE 617" ;
    cycle = 60000 ; // 60.0 secs
    delay = 10000 ; // 10.0 secs 
    DODBG(9)(DBGDEV, "Leaving ke617 constructor.\n" ) ; 
}

void KE617::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: KE617 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 1 ) ; // One key state.

    u_char temp[256] ;
    snprintf( CP temp, 256, "%s get", getAlias() ) ;
    mPeriodics[0] = PeriodicQ->Add( getDevice(), temp ) ;

    StartThread() ;
}

void KE617::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
	PeriodicQ->Schedule( mPeriodics[i], delay ) ;
	PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void KE617::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int KE617::DeviceOpen( void )
{

    int nexc ;
    u_char resp[256] ;
    const char *cmds[] = {
	"*RST ",
	"C0B0G0M9Q7F1R0T5X",
	"C0B0G0M9Q7F0R0T5X"
    } ;

    for( int i = 0 ; i < 3 ; i++ ) {
        StatusLog->L_Write_U( name, "CMD", cmds[i] ) ;
        nexc = Exchange( cmds[i], w_timeo, resp, sizeof(resp), -r_timeo ) ;
        if( nexc < 0 ) return( false ) ; // something happened.
    }

    StartPeriodicTasks() ;
    return( true ) ;
}

void KE617::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

#define NOSTATUS	NoStatus(0)

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int KE617::AddressKE617( u_char *resp, int mlen )
{
    u_char temp[256] ;
    if( strcasecmp( CP client_cmd.cmd[1], "get" ) == 0 ) {
        int nexc = Exchange( UCP "X", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(false) ; } 
	if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen) ; return(true) ; } 

	double value = 0;
	u_char quantity[64] ;
	int nscan = sscanf( CP temp, "%4s%lf", quantity, &value ) ;
	if( nscan != 2 ) { 
	    NOSTATUS; 
	    SET_409_BADRESP(resp,mlen,temp); 
	    return(true); 
	}

	u_char c_value[64] ;
	snprintf( CP c_value, 64, "%.4e", value ) ;

	AddStatus( 0, quantity, c_value ) ;
	return( true ) ;
    }

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[1] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[1], name ) ; 

    return( true ) ;
}

int KE617::HandleRequest( void )
{
    if( Client::HandleRequest() )
	return( true ) ;

    if( client_cmd.nCmds != 2 ) {
	mQueue->SetResponse( ERR_408_BADCMD ) ;
	return( true ) ; // null or pointless command, but not a nasty error.
    }

    u_char response[256] ;
    int retval = AddressKE617( response, sizeof(response) ) ;
    mQueue->SetResponse( response ) ;
    DODBG(0)( DBGDEV, "client response %s: <<%s>>\n", name, response ) ;
    StatusLog->L_Command_U( name, mQueue->GetRequest(), response ) ;

    return( retval ) ;
}

static const char* cClientOptions[] = {
    "get"
    } ;

int KE617::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

