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
#include "monochromator.h"
#include "support.h"
#include "periodic.h"

Monochromator::Monochromator( void )
{
    devtype = TypeMonochromator ;
    devtypestring = "Monochromator" ;
    cycle = 0 ; // Just once.
    delay = 1000 ; // 1.0 secs
    flush_after_timeout = true ;
    DODBG(9)(DBGDEV, "Leaving monochromator constructor.\n" ) ; 
}

void Monochromator::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: Monochromator being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 5 ) ; // Two key states.

    u_char temp[256] ;
    const char *(fmts)[3] = { 
	"%s wlen get", 
	"%s filter get",
	"%s grating get"
	} ;

    for( int i = 0 ; i < nPeriodics ; i++ ) {
	snprintf( CP temp, 256, fmts[i], getAlias() ) ;
	mPeriodics[i] = PeriodicQ->Add( getDevice(), temp ) ;
    }

    StartThread() ;
}

void Monochromator::StartPeriodicTasks( void )
{
    if( delay < 0 ) delay = cycle ;

    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
	PeriodicQ->Schedule( mPeriodics[i], delay + 2500*i ) ;
	PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void Monochromator::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
	PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int Monochromator::DeviceOpen( void )
{
    StartPeriodicTasks() ;
    return( true ) ;
}

void Monochromator::DeviceClose( void )
{
    StopPeriodicTasks() ;
}


// Only returns false if we deem a communication error demands or implies
// a disconnect.
int Monochromator::AddressWavelengthNM( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    u_char tmp[256] ;

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
        int nexc = Exchange( UCP "?NM", w_timeo, tmp, sizeof(tmp), r_timeo ) ;
        if( nexc < 0 ) { 
	    NoStatus(0) ; SET_406_BADCON(resp,mlen) ; return( false ) ;
	}
	if( nexc == 0 ) { 
	    NoStatus(0) ; SET_407_DEVTO(resp,mlen) ; return( true ) ; 
	}

	// Parse out return value.
	double value1 ;
	int nscan = sscanf( CP tmp, "?NM %lf", &value1 ) ;
	if( nscan != 1 ) {
	    NoStatus(0) ;
	    SET_409_BADRESP(resp,mlen,tmp) ;
	    return( true ) ;
	}
	snprintf( CP resp, mlen, "%.4f", value1 ) ;
	AddStatus( 0, UCP "Wavelength", resp ) ;
	return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "goto" ) == 0 ) {
	double posn = strtod( CP client_cmd.cmd[3], 0 ) ;
	snprintf( CP command, sizeof(command), "%.1f >NM", posn ) ;
        int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

	return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[2], name ) ; 

    return( true ) ;
}

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int Monochromator::AddressWavelength( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    u_char tmp[256] ;

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
        int nexc = Exchange( UCP "?NM", w_timeo, tmp, sizeof(tmp), r_timeo ) ;
        if( nexc < 0 ) { 
	    NoStatus(0) ; SET_406_BADCON(resp,mlen) ; return( false ) ;
	}
	if( nexc == 0 ) { 
	    NoStatus(0) ; SET_407_DEVTO(resp,mlen) ; return( true ) ; 
	}

	// Parse out return value.
	double value1 ;
	int nscan = sscanf( CP tmp, "?NM %lf", &value1 ) ;
	if( nscan != 1 ) {
	    NoStatus(0) ;
	    SET_409_BADRESP(resp,mlen,tmp) ;
	    return( true ) ;
	}
	snprintf( CP resp, mlen, "%.4f", value1 ) ;
	AddStatus( 0, UCP "Wavelength", resp ) ;
	return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "goto" ) == 0 ) {
	double posn = strtod( CP client_cmd.cmd[3], 0 ) ;
	snprintf( CP command, sizeof(command), "/%.1f GOTO", posn ) ;
        int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
	return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[2], name ) ; 

    return( true ) ;
}

int Monochromator::AddressMirror( u_char *resp, int mlen )
{
    u_char tmp[256] ;

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
        int nexc = Exchange( UCP "?MIRROR", w_timeo, tmp, sizeof(tmp), r_timeo);
        if( nexc < 0 ) { 
	    NoStatus(1) ; SET_406_BADCON(resp,mlen) ; return( false ) ;
	} 
	if( nexc == 0 ) { 
	    NoStatus(1) ; SET_407_DEVTO(resp,mlen) ; return( true ) ;
	} 
	u_char sMirror[64] ;
	int nscan = sscanf( CP tmp, "%*s %s", sMirror ) ;
	if( nscan != 1 ) {
	    NoStatus(2) ;
	    SET_409_BADRESP(resp,mlen,tmp) ;
	    return( true ) ;
	}
	AddStatus( 2, UCP "Mirror", sMirror ) ;
	snprintf( CP resp, mlen, "%s", sMirror ) ;
	return( true ) ;
    }

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "entrance" ) == 0 ) {
        int nexc = Exchange( UCP "ENT-MIRROR", w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
	AddStatus( 1, UCP "Mirror", resp ) ;
	return( true ) ;
    } 

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "exit" ) == 0 ) {
        int nexc = Exchange( UCP "EXIT-MIRROR", w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
	AddStatus( 1, UCP "Mirror", resp ) ;
	return( true ) ;
    } 

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "side" ) == 0 ) {
        int nexc = Exchange( UCP "SIDE", w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
	AddStatus( 2, UCP "Mirror", resp ) ;
	return( true ) ;
    } 

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "front" ) == 0 ) {
        int nexc = Exchange( UCP "FRONT", w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
	AddStatus( 2, UCP "Mirror", resp ) ;
	return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[2], name ) ; 

    return( true ) ;
}

int Monochromator::AddressGrating( u_char *resp, int mlen )
{
    u_char command[256] ;
    u_char tmp[256] ;

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
        int nexc = Exchange( UCP "?GRATING", w_timeo, tmp, sizeof(tmp), r_timeo);
        if( nexc < 0 ) { 
	    NoStatus(1) ; SET_406_BADCON(resp,mlen) ; return( false ) ;
	} 
	if( nexc == 0 ) { 
	    NoStatus(1) ; SET_407_DEVTO(resp,mlen) ; return( true ) ;
	} 

	double value1 ;
	int nscan = sscanf( CP tmp, "%*s %lf", &value1 ) ;
	if( nscan != 1 ) {
	    NoStatus(1) ;
	    SET_409_BADRESP(resp,mlen,tmp) ;
	    return( true ) ;
	}
	snprintf( CP resp, mlen, "%.0f", value1 ) ;
	AddStatus( 3, UCP "Grating", resp ) ;
	return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "set" ) == 0 ) {
	snprintf( CP command, sizeof(command), "%s GRATING", client_cmd.cmd[3] ) ;
        int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
	return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[2], name ) ; 

    return( true ) ;
}

int Monochromator::AddressFilter( u_char *resp, int mlen )
{
    u_char command[256] ;
    u_char tmp[256] ;

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
	snprintf( CP command, sizeof(command), "?%s", client_cmd.cmd[1] ) ;
        int nexc = Exchange( command, w_timeo, tmp, sizeof(tmp), r_timeo ) ;
        if( nexc < 0 ) { 
	    NoStatus(1) ; SET_406_BADCON(resp,mlen) ; return( false ) ; 
	}
	if( nexc == 0 ) { 
	    NoStatus(1) ; SET_407_DEVTO(resp,mlen) ; return( true ) ;
	}

	double value1 ;
	int nscan = sscanf( CP tmp, "%*s %lf", &value1 ) ;
	if( nscan != 1 ) {
	    NoStatus(1) ;
	    SET_409_BADRESP(resp,mlen,tmp) ;
	    return( true ) ;
	}
	snprintf( CP resp, mlen, "%.0f", value1 ) ;
	AddStatus( 4, UCP "Filter", resp ) ;
	return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "goto" ) == 0 ) {
	snprintf( CP command, sizeof(command), "%s %s", 
	    client_cmd.cmd[3], client_cmd.cmd[1] ) ;

        int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

	return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[2], name ) ; 

    return( true ) ;
}

#define SECS_PER_MINUTE (60)

int Monochromator::AddressRate( u_char *resp, int mlen )
{
    u_char command[256] ;
    u_char tmp[256] ;

    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
        int nexc = Exchange( UCP "?NM/MIN", w_timeo, tmp,sizeof(tmp), r_timeo);
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_407_DEVTO(resp,mlen) ; return( true ) ; } 

	double value1 ;
	int nscan = sscanf( CP tmp, "%*s %lf", &value1 ) ;
	if( nscan != 1 ) {
	    SET_407_DEVTO(resp,mlen) ;
	    return( true ) ;
	}
	snprintf( CP resp, mlen, "%.3f", value1/SECS_PER_MINUTE ) ;
	// No point in adding it to the status.
	return( true ) ;
    }

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "clamped" ) == 0 ) {
/*  	This rate arrives in nm/second and must be converted to nm/min  */

	double rate = strtod( CP client_cmd.cmd[3], 0 ) ;

	snprintf( CP command, sizeof(command), "%.1f NM/MIN", 
	    rate * SECS_PER_MINUTE ) ;

        int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
	return( true ) ;
    } 

    if( client_cmd.nCmds==4 && STRCASECMP( client_cmd.cmd[2], "set" ) == 0 ) {
/*  	This rate arrives in nm/second and must be converted to nm/min  */

	double rate = strtod( CP client_cmd.cmd[3], 0 ) ;

	snprintf( CP command, sizeof(command), "%.1f INIT-SRATE", rate * SECS_PER_MINUTE ) ;

        int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
        if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 
	return( true ) ;
    } 

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[2], name ) ; 

    return( true ) ;
}

int Monochromator::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    if( client_cmd.nCmds < 3 ) {
        DODBG(0)(DBGDEV, "Null or bad command: %s\n", client_cmd.cmd[1] ) ;
        mQueue->SetResponse( ERR_408_BADCMD ) ;
        return( true ) ;
    }

    u_char response[256] ;
    int retval = 0 ;

    if( strcasecmp( CP client_cmd.cmd[1], "wlen" ) == 0 ) {
	retval = AddressWavelength( response, sizeof(response) ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "mirror" ) == 0 ) {
	retval = AddressMirror( response, sizeof(response) ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "grating" ) == 0 ) {
	retval = AddressGrating( response, sizeof(response) ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "filter" ) == 0 ) {
	retval = AddressFilter( response, sizeof(response) ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "rate" ) == 0 ) {
	retval = AddressRate( response, sizeof(response) ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "wlen*" ) == 0 ) {
	retval = AddressWavelengthNM( response, sizeof(response) ) ;
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
    "wlen get",
    "wlen goto %f",
    "grating get",
    "grating goto %d",
    "mirror get",
    "mirror entrance",
    "mirror exit",
    "mirror front",
    "mirror side",
    "filter get",
    "filter goto %d",
    "rate get",
    "rate set %f",
    "rate clamped %f"
    } ;

int Monochromator::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

