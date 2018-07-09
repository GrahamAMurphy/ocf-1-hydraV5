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
#include "newportTH.h"
#include "support.h"
#include "periodic.h"

NewportTH::NewportTH( void )
{
    w_timeo = 200 ;
    r_timeo = 200 ;
    devtype = TypeNewportTH ;
    devtypestring = "Newport TH Monitor" ;
    cycle = 30000 ; // 30.0 secs per check.
    delay = 30000 ; // 30.0 secs per check.
    DODBG(9)(DBGDEV, "Leaving NewportTH constructor.\n" ) ; 
}

int NewportTH::Parse( u_char *key, u_char *value )
{
    return( Client::Parse( key, value ) ) ;
}


void NewportTH::StartOps( void )
{
    u_char temp_str[256] ;

    DODBG(9)(DBGDEV, "%s: NewportTH being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 2 ) ; // Just the temperatures.
    sprintf( CP temp_str, "%s temp get", getAlias() ) ;
    mPeriodics[0] = PeriodicQ->Add( getDevice(), temp_str ) ;
    sprintf( CP temp_str, "%s humidity get", getAlias() ) ;
    mPeriodics[1] = PeriodicQ->Add( getDevice(), temp_str ) ;

    StartThread() ;
}

int NewportTH::DeviceOpen( void )
{
    StartPeriodicTasks() ;
    return( true ) ;
}

void NewportTH::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

void NewportTH::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
	PeriodicQ->Schedule( mPeriodics[i], delay + 1000*i ) ;
        PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void NewportTH::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int NewportTH::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    mQueue->SetResponse( ERR_408_BADCMD ) ;

    if( client_cmd.nCmds != 3 )
	return( true ) ; // null or unknown command

    if( strcasecmp( CP client_cmd.cmd[2], "get" ) != 0 )
	return( true ) ; // null or unknown command

    u_char command[256] ;
    u_char response[256] ;
    int nexc ;

    if( strcasecmp( CP client_cmd.cmd[1], "temp" ) == 0 ) {
	snprintf( CP command, sizeof(command), "*SRT" ) ;

	nexc = Exchange( command, w_timeo, response, sizeof(response), r_timeo ) ;
	if( nexc <= 0 ) { NoStatus(0) ; return( (nexc==0) ) ; }

	DODBG(0)( DBGDEV, "Response from %s: <%s>\n", name, response ) ;
	AddStatus( 0, UCP "Temperature", response ) ;

    } else if( strcasecmp( CP client_cmd.cmd[1], "humidity" ) == 0 ) {
	snprintf( CP command, sizeof(command), "*SRH" ) ;

	nexc = Exchange( command, w_timeo, response, sizeof(response), r_timeo ) ;
	if( nexc <= 0 ) { NoStatus(1) ; return( (nexc==0) ) ; }

	DODBG(0)( DBGDEV, "Response from %s: <%s>\n", name, response ) ;
	AddStatus( 1, UCP "Humidity", response ) ;
    } else {
	return( true ) ;
    }

    StatusLog->L_Command_U( name, command, response ) ;
    mQueue->SetResponse( response ) ;



    return( true ) ;
}

static const char* cClientOptions[] = {
    "temp get",
    "humidity get"
    } ;

int NewportTH::ListOptions( int iN, u_char *cOption, int iOptionLength )
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
