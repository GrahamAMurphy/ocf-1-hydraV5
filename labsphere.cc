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
#include "labsphere.h"
#include "support.h"
#include "periodic.h"

Labsphere::Labsphere( void )
{
    devtype = TypeLabsphere ;
    devtypestring = "Labsphere SC-5500" ;
    cycle = 5000 ; // 5.0 secs
    delay = 2000 ; // 2.0 secs

    attenuator_state[0] = -1 ; // The "don't know" state.
    attenuator_state[1] = -1 ;
    attenuator_state[2] = -1 ;
    attenuator_state[3] = -1 ;

    DODBG(9)(DBGDEV, "Leaving labsphere constructor.\n" ) ; 
}

void Labsphere::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: Labsphere being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 5 ) ; // Intensity & attenuators.

    u_char temp[256] ;
    snprintf( CP temp, 256, "%s detector get", getAlias() ) ;
    mPeriodics[0] = PeriodicQ->Add( getDevice(), temp ) ;

    StartThread() ;
}

void Labsphere::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
        PeriodicQ->Schedule( mPeriodics[i], delay ) ;
        PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void Labsphere::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

void Labsphere::JKJoke( u_char value, u_char mask, u_char *str )
{
    int unmask = 0x80 ;
    int posn = 8 ;
    u_char small[8] ;

    strcpy( CP str, "" ) ;

    while( unmask ) {
	if( mask & unmask ) {
	    if( value & unmask ) 
		snprintf( CP small, 8, "J%1d", posn ) ;
	    else
		snprintf( CP small, 8, "K%1d", posn ) ;
	    strcat( CP str, CP small ) ;
	}
	unmask >>= 1 ;
	posn-- ;
    }

}

void Labsphere::GetAttenuatorCmds( int attenuator, int level, u_char **str )
{
    u_char *buffer ;
    int b_len ;
    u_char pinout[128] ;

    buffer = str[0] ;
    str[0] = 0 ;

    JKJoke( level, 0xff, pinout ) ;
    sprintf( CP buffer, "P1%sX", pinout ) ;
    b_len = strlen( CP buffer ) + 1 ;
    str[0] = buffer ;
    str[1] = 0 ;
    buffer += b_len ;

    JKJoke( attenuator, 0x07, pinout ) ;
    sprintf( CP buffer, "P2%sX", pinout ) ;
    b_len = strlen( CP buffer ) + 1 ;
    str[1] = buffer ;
    str[2] = 0 ;
    buffer += b_len ;

    JKJoke( 0x04, 0x04, pinout ) ;
    sprintf( CP buffer, "P2%sX", pinout ) ;
    b_len = strlen( CP buffer ) + 1 ;
    str[2] = buffer ;
    str[3] = 0 ;
    buffer += b_len ;
}

int Labsphere::SetAttenuator( int s_atten, int level, u_char *resp, int mlen )
{
    int nexc = 1 ;
    u_char tmp[256] ;
    u_char *(cmds[8]) ;
    u_char cmds_buffer[1024] ;

    // this should NEVER happen!
    if( s_atten < 1 || s_atten > 4 ) return( false ) ;

    cmds[0] = cmds_buffer ; cmds[1] = 0 ;
    GetAttenuatorCmds( s_atten-1, level, cmds ) ;
    for( int i = 0 ; i < 8 ; i++ ) {
	if( cmds[i] == 0 ) break ;
	fprintf( stderr, "lab cmd %d <%s>\n", i, cmds[i] ) ;
	StatusLog->L_Write_U( name, "CMD", cmds[i] ) ;
	nexc = Exchange( cmds[i], w_timeo, resp, sizeof(resp), r_timeo ) ;
	// These commands are critical to correct operation.
	// and cannot be read back in a reliable manner.
	if( nexc <= 0 ) return( nexc ) ; // something happened.
    }
    attenuator_state[s_atten-1] = level ;

    snprintf( CP tmp, sizeof(tmp), "Att %d", s_atten ) ;
    snprintf( CP resp, mlen, "%d", level ) ;
    AddStatus( s_atten, tmp, resp ) ;
    return( nexc ) ;
}

int Labsphere::DeviceOpen( void )
{
    int nexc ;
    u_char resp[256] ;
    const char *command = "L0H1A0C2N1Z1F1X" ; // From DO.

    StatusLog->L_Write_U( name, "CMD", command ) ;
    nexc = Exchange( command, w_timeo, resp, sizeof(resp), -r_timeo ) ;
    // These commands are critical to correct operation.
    if( nexc <= 0 ) return( false ) ; // something happened.

    StartPeriodicTasks() ;
    return( true ) ;
}

void Labsphere::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

#define NOSTATUS        NoStatus(0)

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int Labsphere::AddressDetector( u_char *resp, int mlen )
{
    u_char tmp[256] ;

    if( strcasecmp( CP client_cmd.cmd[2], "get" ) == 0 ) {
        int nexc = Exchange( UCP "X", w_timeo, tmp, sizeof(tmp), r_timeo ) ;
        if( nexc < 0 ) { NOSTATUS; SET_406_BADCON(resp,mlen); return(false) ; } 
	if( nexc == 0 ) { NOSTATUS; SET_407_DEVTO(resp,mlen); return(true) ; } 

	double value ;
	int nscan = sscanf( CP tmp, "%le", &value ) ;
	if( nscan != 1 ) { NOSTATUS ; SET_407_DEVTO(resp,mlen); return(true) ; }

	snprintf( CP resp, mlen, "%.4e", value ) ;
	AddStatus( 0, UCP "Detector", resp ) ;
	return( true ) ;
    }

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[1], name ) ; 
    return( true ) ;
}

int Labsphere::AddressAttenuator( u_char *resp, int mlen )
{
    int atten_g = -1 ;
    if( client_cmd.cmd[2] ) atten_g = strtol( CP client_cmd.cmd[2], 0, 0 ) ;

    if( atten_g < 1 || atten_g > 4 ) atten_g = -1 ;
    if( atten_g < 1) {
	DODBG(0)(DBGDEV, "Invalid attenuator: %s\n", client_cmd.cmd[2] ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s",
	    client_cmd.cmd[1], name ) ;
	return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[3], "get" ) == 0 ) {
	snprintf( CP resp, mlen, "%d", attenuator_state[ atten_g-1 ] ) ;
	return( true ) ;
    }

    if( client_cmd.nCmds != 5 || STRCASECMP(client_cmd.cmd[3], "set" ) != 0) {
	DODBG(0)(DBGDEV, "Bad command: %s\n", client_cmd.cmd[3] ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s",
	client_cmd.cmd[1], name ) ;
	return( true ) ;
    }

    int value ;
    int nscan = sscanf( CP client_cmd.cmd[4], "%d", &value ) ;
    if( nscan != 1 || value < 0 || value > 255 ) {
	DODBG(0)(DBGDEV, "Bad attenuator value: %s\n", client_cmd.cmd[4] ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s",
	client_cmd.cmd[1], name ) ;
	return( true ) ;
    }

    int nexc = SetAttenuator( atten_g, value, resp, mlen ) ;
    if( nexc < 0 ) { 
	NoStatus(atten_g) ; SET_406_BADCON(resp,mlen); return(false) ;
    } 
    if( nexc == 0 ) { 
	NoStatus(atten_g) ; SET_407_DEVTO(resp,mlen); return(true) ;
    }
    return( true ) ;
}

int Labsphere::HandleRequest( void )
{
    if( Client::HandleRequest() )
	return( true ) ;

    if( client_cmd.nCmds < 3 ) {
        DODBG(0)(DBGDEV, "Null command: %s\n", client_cmd.cmd[1] ) ;
        mQueue->SetResponse( ERR_408_BADCMD ) ;
        return( true ) ;
    }

    u_char response[256] ;
    int retval = 0 ;
    int nCmds = client_cmd.nCmds ;

    if( STRCASECMP( client_cmd.cmd[1], "attenuator" ) == 0 ) {
        retval = AddressAttenuator( response, sizeof(response) ) ;
    } else if( nCmds==3  && STRCASECMP( client_cmd.cmd[1], "detector" ) == 0 ) {
        retval = AddressDetector( response, sizeof(response) ) ;
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
    "detector get",
    "attenuator %d get",
    "attenuator %d set %d"
    } ;

int Labsphere::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

