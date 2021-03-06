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
#include "udts470.h"
#include "support.h"
#include "periodic.h"

UDTS470::UDTS470( void )
{
    devtype = TypeUDTS470 ;
    devtypestring = "UDT S470" ;
    cycle = 10000 ; // 10.0 secs
    delay = 10000 ; // 10.0 secs
    s470_calset = 3 ;
    s470_wavelength = 400 ;
    s470_channel = 1 ;
    strcpy( CP s470_units, "W" ) ;

    DODBG(9)(DBGDEV, "Leaving udts470 constructor.\n" ) ; 
}

void UDTS470::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: UDTS470 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 6 ) ; // One key state.

    u_char temp[256] ;
    snprintf( CP temp, 256, "%s get", getAlias() ) ;
    mPeriodics[0] = PeriodicQ->Add( getDevice(), temp ) ;

    StartThread() ;
}

void UDTS470::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
	PeriodicQ->Schedule( mPeriodics[i], delay ) ;
	PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void UDTS470::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int UDTS470::SetCalibration( int calset )
{
    u_char ucCommand[256] ;
    u_char tmp[256] ;

    sprintf( CP ucCommand, "/K%1d", calset ) ;

    int nexc = Exchange( ucCommand, w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) return(false) ;

    nexc = Exchange( "/G", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) return( false ) ;

    s470_calset = calset ;
    snprintf( CP tmp, sizeof(tmp), "%d", s470_calset ) ;
    AddStatus( 2, UCP "Calibration Set", tmp ) ;
    return( true ) ;
}

int UDTS470::SetChannel( int iChannel )
{
    u_char ucCommand[256] ;
    u_char tmp[256] ;

    sprintf( CP ucCommand, "/C%d", iChannel ) ;
    int nexc = Exchange( ucCommand, w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) return(false) ;

    nexc = Exchange( "/G", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) return( false ) ;

    s470_channel = iChannel ;
    snprintf( CP tmp, sizeof(tmp), "%d", s470_channel ) ;
    AddStatus( 5, UCP "Channel*", tmp ) ;
    return( true ) ;
}

int UDTS470::SetWavelength( int wlen )
{
    u_char ucCommand[256] ;
    u_char tmp[256] ;

    sprintf( CP ucCommand, "/W%d", wlen ) ;
    int nexc = Exchange( ucCommand, w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) return(false) ;

    nexc = Exchange( "/G", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) return( false ) ;

    s470_wavelength = wlen ;
    snprintf( CP tmp, sizeof(tmp), "%d", s470_wavelength ) ;
    AddStatus( 3, UCP "Wavelength*", tmp ) ;
    return( true ) ;
}

int UDTS470::SetUnits( u_char *ucUnits )
{
    u_char tmp[256] ;
    int nexc ;

    if( STRCASECMP( ucUnits, "w" ) == 0 ) {
	nexc = Exchange( "/V1", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
	if( nexc < 0 ) return( false) ;
    } else if( STRCASECMP( ucUnits, "fc" ) == 0 ) {
	nexc = Exchange( "/V2", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
	if( nexc < 0 ) return( false) ;
    } else if( STRCASECMP( ucUnits, "lux" ) == 0 ) {
	nexc = Exchange( "/V3", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
	if( nexc < 0 ) return( false) ;
    } else if( STRCASECMP( ucUnits, "f" ) == 0 ) {
	nexc = Exchange( "/V4", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
	if( nexc < 0 ) return( false) ;
    } else if( STRCASECMP( ucUnits, "w/cm2" ) == 0 ) {
	nexc = Exchange( "/V5", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
	if( nexc < 0 ) return( false) ;
    } else if( STRCASECMP( ucUnits, "w/cm2*sr" ) == 0 ) {
	nexc = Exchange( "/V6", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
	if( nexc < 0 ) return( false) ;
    } else if( STRCASECMP( ucUnits, "cd/m2" ) == 0 ) {
	nexc = Exchange( "/V7", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
	if( nexc < 0 ) return( false) ;
    } else if( STRCASECMP( ucUnits, "lm" ) == 0 ) {
	nexc = Exchange( "/V8", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
	if( nexc < 0 ) return( false) ;
    } else {
	return( false) ;
    }

    nexc = Exchange( "/G", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) return( false ) ;

    nexc = Exchange( "U", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) return( false ) ;

    strncpy( CP s470_units, CP tmp, sizeof(s470_units) ) ;
    AddStatus( 4, UCP "Units", s470_units ) ;
    return( true ) ;
}

int UDTS470::DeviceOpen( void )
{
    u_char tmp[256] ;
    int nexc ;
    int bStatus ;

    nexc = Exchange( "B0", w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
    if( nexc < 0 ) return( false ) ;
    sleep(2) ;

    bStatus = SetChannel( s470_channel ) ;
    if( ! bStatus ) return( false ) ;
    sleep(1) ;

    bStatus = SetCalibration( s470_calset ) ;
    if( ! bStatus ) return( false ) ;
    sleep(1) ;

    bStatus = SetWavelength( s470_wavelength ) ;
    if( ! bStatus ) return( false ) ;
    sleep(1) ;

    bStatus = SetUnits( s470_units ) ;
    if( ! bStatus ) return( false ) ;
    sleep(1) ;

    StartPeriodicTasks() ;
    return( true ) ;
}

void UDTS470::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

#define NOSTATUS        NoStatus(0)

int UDTS470::AddressWavelength( u_char *resp, int mlen )
{
    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
	snprintf( CP resp, mlen, "%d", s470_wavelength ) ;
        return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "set" ) == 0 ) {
	int iWavelength = strtol( CP client_cmd.cmd[3], NULL, 0 ) ;
	int bStatus = SetWavelength( iWavelength ) ;
	snprintf( CP resp, mlen, "%d", s470_wavelength ) ;
	if( ! bStatus ) return( false ) ;
        return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", client_cmd.cmd[2], name ) ; 

    return( true ) ;

}

int UDTS470::AddressChannel( u_char *resp, int mlen )
{
    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
	snprintf( CP resp, mlen, "%d", s470_channel ) ;
        return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "set" ) == 0 ) {
	int iChannel = strtol( CP client_cmd.cmd[3], NULL, 0 ) ;
	int bStatus = SetChannel( iChannel ) ;
	snprintf( CP resp, mlen, "%d", s470_channel ) ;
	if( ! bStatus ) return( false ) ;
        return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", client_cmd.cmd[2], name ) ; 

    return( true ) ;

}

int UDTS470::AddressCalibrationSet( u_char *resp, int mlen )
{
    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
	snprintf( CP resp, mlen, "%d", s470_calset ) ;
        return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "set" ) == 0 ) {
	int iCalSet = strtol( CP client_cmd.cmd[3], NULL, 0 ) ;
	int bStatus = SetCalibration( iCalSet ) ;
	snprintf( CP resp, mlen, "%d", s470_calset ) ;
	if( ! bStatus ) return( false ) ;
        return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", client_cmd.cmd[2], name ) ; 

    return( true ) ;

}

int UDTS470::AddressUnits( u_char *resp, int mlen )
{
    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
	snprintf( CP resp, mlen, "%s", s470_units ) ;
        return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "set" ) == 0 ) {
	int bStatus = SetUnits( client_cmd.cmd[3] ) ;
	snprintf( CP resp, mlen, "%s", s470_units ) ;
	if( ! bStatus ) return( false ) ;
        return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", client_cmd.cmd[2], name ) ; 

    return( true ) ;

}

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int UDTS470::AddressUDTS470( u_char *resp, int mlen )
{
    u_char tmpV[256] ;
    u_char tmpS[256] ;

    u_char ucCommand[256] ;

    sprintf( CP ucCommand, "F%d", s470_channel ) ;
    int nexc = Exchange( ucCommand, w_timeo, tmpV, sizeof(tmpV), r_timeo ) ;
    if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(false) ; } 
    if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen) ; return(true) ; } 
    AddStatus( 0, UCP "Value", tmpV ) ;

    nexc = Exchange( "S", w_timeo, tmpS, sizeof(tmpS), r_timeo ) ;
    if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(false) ; } 
    if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen) ; return(true) ; } 
    AddStatus( 1, UCP "State", tmpS ) ;

    int iChannel ;
    int iWavelength ;
    char sMode[64], sGainMode[64] ;
    int iCalSet ;

    int nscan = sscanf( CP tmpS, "%d %s %d %s %d", &iChannel, sMode, &iWavelength, sGainMode, &iCalSet ) ;
    if( nscan == 5 ) {
	u_char tmp[256] ;
	s470_wavelength = iWavelength ;
	if( iChannel > 0 && iChannel < 10 ) s470_channel = iChannel ;

	snprintf( CP tmp, sizeof(tmp), "%d", s470_wavelength ) ;
	AddStatus( 3, UCP "Wavelength", tmp ) ;

	snprintf( CP tmp, sizeof(tmp), "%d", s470_channel ) ;
	AddStatus( 5, UCP "Channel", tmp ) ;
    }

    sprintf( CP resp, "%s / %s", tmpV, tmpS ) ;
    return( true ) ;
}

int UDTS470::HandleRequest( void )
{
    if( Client::HandleRequest() )
	return( true ) ;

    if( client_cmd.nCmds < 2 ) {
	mQueue->SetResponse( ERR_408_BADCMD ) ;
	return( true ) ; // null or pointless command, but not a nasty error.
    }

    u_char response[256] ;
    int retval = 0 ;

    if( strcasecmp( CP client_cmd.cmd[1], "get" ) == 0 ) {
        retval = AddressUDTS470( response, sizeof(response) ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "channel" ) == 0 ) {
        retval = AddressChannel( response, sizeof(response) ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "wlen" ) == 0 ) {
        retval = AddressWavelength( response, sizeof(response) ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "cal" ) == 0 ) {
        retval = AddressCalibrationSet( response, sizeof(response) ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "units" ) == 0 ) {
        retval = AddressUnits( response, sizeof(response) ) ;
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
    "get",
    "wlen get",
    "wlen set %d",
    "cal get",
    "cal set %1d",
    "units get",
    "units set %s"
    } ;

int UDTS470::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

