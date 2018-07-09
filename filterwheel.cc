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
#include "filterwheel.h"
#include "support.h"
#include "periodic.h"

FilterWheel::FilterWheel( void )
{
    devtype = TypeFilterWheel ;
    devtypestring = "Filter Wheel" ;
    cycle = 100000 ; // 100.0 secs
    delay = 5000 ; // 5.0 secs
    fw_state = fw_goal = -1 ;
    DODBG(9)(DBGDEV, "Leaving filterwheel constructor.\n" ) ; 
}

void FilterWheel::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: FilterWheel being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 1 ) ; // One key state.

    u_char temp[256] ;
    snprintf( CP temp, 256, "%s get", getAlias() ) ;
    mPeriodics[0] = PeriodicQ->Add( getDevice(), temp ) ;

    StartThread() ;
}

void FilterWheel::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
	PeriodicQ->Schedule( mPeriodics[i], delay ) ;
	PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void FilterWheel::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int FilterWheel::DeviceOpen( void )
{
    StartPeriodicTasks() ;
    return( true ) ;
}

void FilterWheel::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

int FilterWheel::UpdateFWState( FW_state_select FW_select, u_char *a_string )
{
    int retval = true ;

    int new_state = -1 ;
    int nscan = sscanf( CP a_string, "%*s %d", &new_state ) ;
    if( nscan != 1 )
	return( false ) ;

    if( FW_select == FW_state ) {
	int rx_state = new_state ;
	if( rx_state >= 0 && rx_state <= 6 ) {
	    fw_state = rx_state ; 
	} else {
	    retval = false ;
	}
    } else {
	int tx_goal = new_state ;
	if( tx_goal >= 0 && tx_goal <= 6 ) {
	    fw_goal = tx_goal ;
	}
    }
    return( retval ) ;
}

#define NOSTATUS	NoStatus(0)

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int FilterWheel::AddressFilter( u_char *resp, int mlen )
{
    u_char command[256] ;
    u_char tmp[256] ;

    if( client_cmd.nCmds==2 && STRCASECMP( client_cmd.cmd[1], "get" ) == 0 ) {
	int nexc = Exchange( UCP "?FILTER", w_timeo, tmp, sizeof(tmp), r_timeo);
	if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(false); }
	if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen); return( true ) ; }

	if( UpdateFWState( FW_state, tmp ) ) {
	    snprintf( CP resp, mlen, "%d", fw_state ) ;
	    AddStatus( 0, UCP "Position", resp ) ;
	} else {
	    SET_409_BADRESP(resp,mlen,tmp) ;
	    NOSTATUS ;
	    errors++ ;
	}
	return( true ) ;
    }

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[1], "goto" ) == 0 ) {
	UpdateFWState( FW_goal, client_cmd.cmd[2] ) ;

	snprintf( CP command, sizeof(command), "%s FILTER", client_cmd.cmd[2] );

	// The filter wheel doesn't ack, so we provide a very short
	// timeout just in case some error occured, and then ignore the
	// nread.
	int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
	if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

	return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[1] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	client_cmd.cmd[1], name ) ;
    return( true ) ;
}

int FilterWheel::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    if( client_cmd.nCmds < 2 ) {
	DODBG(0)(DBGDEV, "Null command: %s\n", client_cmd.cmd[1] ) ;
	mQueue->SetResponse( ERR_408_BADCMD ) ;
	return( true ) ;
    }

    u_char response[256] ;

    int retval = AddressFilter( response, sizeof(response) ) ;
    mQueue->SetResponse( response ) ;
    DODBG(0)( DBGDEV, "Response of %s: <%s>\n", name, response ) ;
    StatusLog->L_Command_U( name, mQueue->GetRequest(), response ) ;
    return( retval ) ;
}

static const char* cClientOptions[] = {
    "get",
    "goto %d"
    } ;

int FilterWheel::ListOptions( int iN, u_char *cOption, int iOptionLength )
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
