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
#include "tempmon.h"
#include "support.h"
#include "periodic.h"

TempMon::TempMon( void )
{
    r_timeo = 2000 ;
    w_timeo = 2000 ;
    devtype = TypeTempMon ;
    devtypestring = "Temperature Mon" ;
    cycle = 10000 ; // 10.0 secs per channel.
    n_sensors = 8 ;
    n_heaters = 10 ;
    w_sensor = 0 ;
    DODBG(9)(DBGDEV, "Leaving tempmon constructor.\n" ) ; 
}

int TempMon::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "sensors" ) == 0 ) {
        int t_n_sensors = strtol( CP value, NULL, 0 ) ;
        if( t_n_sensors > 0 ) n_sensors = t_n_sensors ;
        return( true ) ;
    }
    return( Client::Parse( key, value ) ) ;
}


void TempMon::StartOps( void )
{
    u_char temp_str[256] ;

    DODBG(9)(DBGDEV, "%s: TempMon being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( n_sensors ) ; // Just the temperatures.
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	sprintf( CP temp_str, "tempmon get temp -1" ) ;
	mPeriodics[i] = PeriodicQ->Add( getDevice(), temp_str ) ;
    }

    StartThread() ;
}

int TempMon::DeviceOpen( void )
{
    u_char temp_str[256] ;

    for( int i = 0 ; i < n_sensors ; i++ ) {
        snprintf( CP temp_str, sizeof(temp_str), "get temp %d name", i ) ;
        int nexc = Exchange( temp_str, w_timeo, temp_names[i], 64, r_timeo ) ;
        if( nexc <= 0 ) return( false ) ;
	DODBG(0)( DBGDEV, "%s: <<%s>>\n", name, temp_names[i] );
	StatusLog->L_Command_U( name, temp_str, temp_names[i] ) ;
    }

    for( int i = 0 ; i < n_heaters ; i++ ) {
        snprintf( CP temp_str, sizeof(temp_str), "get heater %d name", i ) ;
        int nexc = Exchange( temp_str, w_timeo, heater_names[i], 64, r_timeo ) ;
        if( nexc <= 0 ) return( false ) ;
	DODBG(0)( DBGDEV, "%s: <<%s>>\n", name, heater_names[i] );
	StatusLog->L_Command_U( name, temp_str, heater_names[i] ) ;
    }
    StartPeriodicTasks() ;
    return( true ) ;
}

void TempMon::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

void TempMon::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
        PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void TempMon::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int TempMon::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    mQueue->SetResponse( ERR_408_BADCMD ) ;

    if( client_cmd.nCmds != 4 )
	return( true ) ; // null or unknown command

    if( strcasecmp( CP client_cmd.cmd[1], "get" ) != 0 )
	return( true ) ; // null or unknown command

    if( strcasecmp( CP client_cmd.cmd[2], "temp" ) != 0 )
	return( true ) ; // null or unknown command

    u_char command[256] ;

    int w_t = strtol( CP client_cmd.cmd[3], NULL, 0 ) ;

    if( w_t == -1 ) {
	w_t = w_sensor ;
	w_sensor = (w_sensor + 1) % n_sensors ;
    } else if( w_t < 0 || w_t > 7 )
	return( true ) ;

    snprintf( CP command, sizeof(command), "get temp %d temp", w_t ) ;

    u_char response[256] ;

    int nexc ;
    nexc = Exchange( command, w_timeo, response, sizeof(response), r_timeo ) ;
    if( nexc <= 0 ) { NoStatus(w_t) ; return( (nexc==0) ) ; }

    DODBG(0)( DBGDEV, "Response from %s: <<%s>>\n", name, response ) ;
    AddStatus( w_t, temp_names[w_t], response ) ;

    StatusLog->L_Command_U( name, command, response ) ;
    mQueue->SetResponse( response ) ;
    return( true ) ;
}

static const char* cClientOptions[] = {
    "get temp %d"
    } ;

int TempMon::ListOptions( int iN, u_char *cOption, int iOptionLength )
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
