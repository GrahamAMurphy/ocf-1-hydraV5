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
#include "unidex511.h"
#include "support.h"
#include "periodic.h"

Unidex511::Unidex511( void )
{
    devtype = TypeUnidex511 ;
    devtypestring = "Unidex 511" ;
    cycle = 10000 ; // 100.0 secs
    delay = 5000 ; // 9.0 secs
    rate[0] = 1000 ;
    rate[1] = 1000 ;
    ucAxis[0][0] = UCP "Axis1" ;
    ucAxis[0][1] = UCP "Axis1" ;
    ucAxis[1][0] = UCP "Axis2" ;
    ucAxis[1][1] = UCP "Axis2" ;
    DODBG(9)(DBGDEV, "Leaving Unidex511 constructor.\n" ) ; 
}

void Unidex511::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: Unidex511 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( 5 ) ; // two positions, three states.

    u_char temp[256] ;
    const char *(fmts)[2] = { "%s position get", "%s state get"  } ;

    for( int i = 0 ; i < nPeriodics ; i++ ) {
        snprintf( CP temp, 256, fmts[i], getAlias() ) ;
        mPeriodics[i] = PeriodicQ->Add( getDevice(), temp ) ;
    }

    StartThread() ;
}

int Unidex511::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "axis1" ) == 0 ) {
        ucAxis[0][0] = UCP strdup( CP value ) ;
        return( true ) ;
    } else if( strcasecmp( CP key, "axis2" ) == 0 ) {
        ucAxis[1][0] = UCP strdup( CP value ) ;
        return( true ) ;
    }
    return( Client::Parse( key, value ) ) ;
}

void Unidex511::StartPeriodicTasks( void )
{
    if( delay < 0 ) delay = cycle ;
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->SetPeriod( mPeriodics[i], cycle ) ;
        PeriodicQ->Schedule( mPeriodics[i], delay + 500*i ) ;
        PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void Unidex511::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int Unidex511::DeviceOpen( void )
{
    StartPeriodicTasks() ;
    return( true ) ;
}

void Unidex511::DeviceClose( void )
{
    StopPeriodicTasks() ;
}

int Unidex511::Clear( int times )
{
    u_char tmp[256] = "" ;
    for( int i = 0 ; i < times ; i++ ) {
        int nexc = Exchange( UCP "I AB *", w_timeo, tmp, sizeof(tmp), -r_timeo);
        if( nexc < 0 ) return( -1 ) ;
	if( nexc == 0 ) {
	    nexc = Exchange( UCP "I AB *", w_timeo, tmp, sizeof(tmp), -r_timeo);
	    DODBG(0)(DBGDEV, "Unidex buffer has been cleared.\r\n" ) ;
	    return( 1 ) ;
	}
    }
    return( 0 ) ;
}

#define NOPX	NoStatus(0) ;
#define NOPY	NoStatus(1) ;

// Only returns false if we deem a communication error demands or implies
// a disconnect.
int Unidex511::Get( u_char *resp, int mlen ) 
{
    u_char temp[256] ;
    bool bXStopped = false ;
    bool bYStopped = false ;
    int i ;

    if( strcasecmp( CP client_cmd.cmd[1], "position" ) == 0 ) {
        int nexc = Exchange( UCP "PX8", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOPX; SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { NOPX; SET_407_DEVTO(resp,mlen) ; return( true ) ; } 

	long posn[2] ;

	int nscan = sscanf( CP temp, "%ld", posn+0 ) ;
	if( nscan != 1 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode response from motor:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    NOPX ;
	    return( true ) ;
	}

        nexc = Exchange( UCP "PY8", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOPY; SET_406_BADCON(resp,mlen); return(false); } 
	if( nexc == 0 ) { NOPY; SET_407_DEVTO(resp,mlen); return(true) ; } 

	nscan = sscanf( CP temp, "%ld", posn+1 ) ;
	if( nscan != 1 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode response from motor:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    NOPY ;
	    return( true ) ;
	}

	snprintf( CP temp, 64, "%ld", posn[0] ) ;
	AddStatus( 0, ucAxis[0][0], temp ) ;
	snprintf( CP temp, 64, "%ld", posn[1] ) ;
	AddStatus( 1, ucAxis[1][0], temp ) ;

	snprintf( CP resp, mlen, "%ld,%ld", posn[0], posn[1] ) ;
	return( true ) ;
    }
    if( strcasecmp( CP client_cmd.cmd[1], "state" ) == 0 ) {
        int nexc = Exchange( UCP "PS1", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOPX; SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { NOPX; SET_407_DEVTO(resp,mlen) ; return( true ) ; } 

	long state[3] ;

	int nscan = sscanf( CP temp, "%ld", state+0 ) ;
	if( nscan != 1 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode response from motor:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    return( true ) ;
	}

        nexc = Exchange( UCP "PS2", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOPY; SET_406_BADCON(resp,mlen); return(false); } 
	if( nexc == 0 ) { NOPY; SET_407_DEVTO(resp,mlen); return(true) ; } 

	nscan = sscanf( CP temp, "%ld", state+1 ) ;
	if( nscan != 1 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode response from motor:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    return( true ) ;
	}

        nexc = Exchange( UCP "PS5", w_timeo, temp, sizeof(temp), r_timeo ) ;
        if( nexc < 0 ) { NOPY; SET_406_BADCON(resp,mlen); return(false); } 
	if( nexc == 0 ) { NOPY; SET_407_DEVTO(resp,mlen); return(true) ; } 

	nscan = sscanf( CP temp, "%ld", state+2 ) ;
	if( nscan != 1 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode response from motor:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    return( true ) ;
	}
	state[0] &= 0xFFFF ;
	state[1] &= 0xFFFF ;
	state[2] &= 0xFF ;

        for( i = 0 ; i < 2 ; i++ ) {
            u_char t_status1[256], t_status2[256] ;
            snprintf( CP t_status1, 64, "State %s", ucAxis[i][0] ) ;
            snprintf( CP t_status2, 64, "%04lx", state[i] ) ;
            AddStatus( i+2, t_status1, t_status2 ) ;
        }
	snprintf( CP temp, 64, "%02lx", state[2] ) ;
	AddStatus( 4, UCP "State", temp ) ;

	bXStopped = ( state[2] & 0x10 ) ? false : true ;
	bYStopped = ( state[2] & 0x20 ) ? false : true ;
	if( bXStopped && bYStopped && ( (state[0] != 0) || state[1] != 0 ) ) {
	    nexc = Exchange( UCP "IFA", w_timeo, temp, sizeof(temp), r_timeo ) ;
	    if( nexc < 0 ) { SET_406_BADCON(resp,mlen); return(false); } 
	    if( nexc == 0 ) { SET_407_DEVTO(resp,mlen); return(true) ; } 
	}

	snprintf( CP resp, mlen, "%04lx,%04lx,%02lx", state[0], state[1], state[2] ) ;
	return( true ) ;
    }

    DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[2] ) ;
    snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
        client_cmd.cmd[2], name ) ; 

    return( true ) ;
}

int Unidex511::Wait( u_char *resp, int mlen ) 
{
    long state ;
    bool bXStopped = false ;
    bool bYStopped = false ;
    u_char temp[256] ;
    struct timeval now ;
    double dNow, dThen, dStart ;

    gettimeofday( &now, NULL ) ;
    dNow = dStart = now.tv_sec + 1e-6 * now.tv_usec ;

    long total_time = strtol( CP client_cmd.cmd[2], 0, 0 ) ;
    dThen = dStart + total_time * 1e-3 ;

    while( 1 ) {
	int nexc = Exchange( UCP "PS5", w_timeo, temp, sizeof(temp), r_timeo ) ;
	if( nexc < 0 ) { NOPY; SET_406_BADCON(resp,mlen); return(false); } 
	if( nexc == 0 ) { NOPY; SET_407_DEVTO(resp,mlen); return(true) ; } 

	int nscan = sscanf( CP temp, "%ld", &state ) ;
	if( nscan != 1 ) {
	    DODBG(0)(DBGDEV, 
		"Could not decode response from motor:%s.\r\n", temp ) ;
	    SET_409_BADRESP(resp,mlen,temp) ;
	    return( true ) ;
	}
	// here we include knowledge of the bits.
	bXStopped = ( state & 0x10 ) ? false : true ;
	bYStopped = ( state & 0x20 ) ? false : true ;

	if( bXStopped && bYStopped ) break ;
	gettimeofday( &now, NULL ) ;
	dNow = now.tv_sec + 1e-6 * now.tv_usec ;

	if( dNow > dThen ) break ;
	usleep( 500000 ) ;
    }
    snprintf( CP resp, mlen, "%.3lf", dNow-dStart ) ;
    return( true ) ;
}
int Unidex511::Move( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    u_char temp[256] ;
    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    long move = strtol( CP client_cmd.cmd[3], 0, 0 ) ;

    // Make sure the value is not preceded by white space.
    snprintf( CP temp, 64, "%ld", move ) ;
    u_char *q = temp ;
    while( *q == ' ' && *q != '\0' ) q++ ;

    // snprintf( CP command, 256, "IIN %1c%s F%d", axis, q, rate[axis-'X'] ) ;
    // Not sure about feed rates yet.
    snprintf( CP command, 256, "IIN %1c%s", axis, q ) ;

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

    return( true ) ;
}

int Unidex511::Home( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    char axis = Axis( client_cmd.cmd[1] ) ;

    // snprintf( CP command, 256, "IIN %1c%s F%d", axis, q, rate[axis-'X'] ) ;
    // Not sure about feed rates yet.
    if( axis ) {
	snprintf( CP command, 256, "IHOME %1c", axis ) ;
    } else if( strcasecmp( CP client_cmd.cmd[1], "all" ) == 0 ) {
	snprintf( CP command, 256, "IHOME X Y" ) ;
    } else {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    // Don't expect a reply.
    int nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

    return( true ) ;
}

int Unidex511::FaultAcknowledge( u_char *resp, int mlen ) 
{
    // Don't expect a reply.
    int nexc = Exchange( "IFA", w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

    return( true ) ;
}

char Unidex511::Axis( u_char *s_axis )
{
    int iAxis = 2 ;
    u_char *ucTAxis = UCP "XY" ;

    int i ;
    int j ;
    for( i = 0 ; i < 2 ; i++ ) {
	for( j = 0 ; j < 2 ; j++ ) {
	    if( strcasecmp( CP s_axis, CP ucAxis[i][j] ) == 0 ) {
		iAxis = i ;
	    }
	}
    }
    return( ucTAxis[iAxis] ) ;
}

int Unidex511::Rate( u_char *resp, int mlen ) 
{
    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    long new_rate = strtol( CP client_cmd.cmd[3], 0, 0 ) ;

    if( new_rate > 0 && new_rate < 10000 ) {
	snprintf( CP resp, mlen, "%d", rate[ axis - 'X' ] ) ;
	rate[ axis - 'X' ] = new_rate ;
	DODBG(3)( DBGDEV, "Set unidex rate for %c axis to %ld steps/s.\n", 
	    axis, new_rate ) ;
    } else {
	snprintf( CP resp, mlen, ERR_410_BADRATE ": %ld", new_rate ) ; 
    }

    return( true ) ;
}

int Unidex511::Goto( u_char *resp, int mlen ) 
{
    u_char command[256] ;
    u_char temp[256] ;
    char axis = Axis( client_cmd.cmd[1] ) ;

    if( !axis ) {
	DODBG(0)(DBGDEV, "Not an axis (%s).\n", name ) ;
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

    long new_posn = strtol( CP client_cmd.cmd[3], 0, 0 ) ;
    snprintf( CP command, sizeof(command), "P%1c8", axis ) ;

    int nexc = Exchange( command, w_timeo, temp, sizeof(temp), r_timeo ) ;
    if( nexc < 0 ) { NOPX; SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { NOPX; SET_407_DEVTO(resp,mlen) ; return( true ) ; } 

    long posn ;

    int nscan = sscanf( CP temp, "%ld", &posn ) ;
    if( nscan != 1 ) {
	DODBG(0)(DBGDEV, 
	    "Could not decode response from motor:%s.\r\n", temp ) ;
	SET_409_BADRESP(resp,mlen,temp) ;
	return( true ) ;
    }

    // Make sure the value is not preceded by white space.
    snprintf( CP temp, 64, "%ld", new_posn-posn ) ;
    u_char *q = temp ;
    while( *q == ' ' && *q != '\0' ) q++ ;

    // snprintf( CP command, 256, "IIN %1c%s F%d", axis, q, rate[axis-'X'] ) ;
    // Not sure about feed rates yet.
    snprintf( CP command, 256, "IIN %1c%s", axis, q ) ;

    // Don't expect a reply.
    nexc = Exchange( command, w_timeo, resp, mlen, -r_timeo ) ;
    if( nexc < 0 ) { SET_406_BADCON(resp,mlen) ; return( false ) ; } 
    if( nexc == 0 ) { SET_105_DEVTO(resp,mlen) ; return( true ) ; } 

    return( true ) ;
}

int Unidex511::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    int retval = 0 ;
    u_char response[256] ;

    int ncmds = client_cmd.nCmds ;
    if( strcasecmp( CP client_cmd.cmd[1], "g" ) == 0 ) {
	retval = Generic( response, sizeof(response) ) ;
    } else if( ncmds == 3 && strcasecmp( CP client_cmd.cmd[2], "get" ) == 0 ) {
	retval = Get( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "goto" ) == 0 ) {
	retval = Goto( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "move" ) == 0 ) {
	retval = Move( response, sizeof(response) ) ;
    } else if( ncmds == 3 && strcasecmp( CP client_cmd.cmd[2], "home" ) == 0 ) {
	retval = Home( response, sizeof(response) ) ;
    } else if( ncmds == 3 && strcasecmp( CP client_cmd.cmd[1], "wait" ) == 0 ) {
	retval = Wait( response, sizeof(response) ) ;
    } else if( ncmds == 4 && strcasecmp( CP client_cmd.cmd[2], "rate" ) == 0 ) {
	retval = Rate( response, sizeof(response) ) ;
    } else if( ncmds == 2 && strcasecmp( CP client_cmd.cmd[1], "clear" ) == 0 ) {
	retval = FaultAcknowledge( response, sizeof(response) ) ;
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

int Unidex511::Generic( u_char *resp, int mlen ) 
{
    char tout[1024] ;
    char temp[1024] ;
    int i, n = 0 ;
    int bDoWait = true ;

    if( client_cmd.cmd[1][0] == 'G' ) {
	bDoWait = false ;
    }
    fprintf( stderr, "XXXXXXXXXXX <%s> %d\n", client_cmd.cmd[1], bDoWait ) ;

    for( i = 2 ; i < client_cmd.nCmds ; i++ ) {
	n += snprintf( tout+n, sizeof(tout)-n, "%s ", client_cmd.cmd[i] ) ;
    }
    if( n == 0 ) {
	snprintf( CP resp, mlen, "Bad generic command." ) ;
	return( true ) ;
    }

    tout[--n] = 0 ;
    for( i = 0 ; i < n ; i++ ) {
	tout[i] = toupper( tout[i] ) ;
    }

    int nexc = Exchange( UCP tout, w_timeo, UCP temp, sizeof(temp), bDoWait ? r_timeo : (-r_timeo) ) ;
    if( nexc > 0 ) {
	if( bDoWait ) {
	    u_long ulValue = strtoul( temp, NULL, 0 ) ;
	    snprintf( CP resp, mlen, "%s %08lx", temp, ulValue ) ;
	} else {
	    strncpy( CP resp, CP temp, mlen ) ;
	}
    } else {
        if( nexc < 0 ) { NOPX; SET_406_BADCON(resp,mlen) ; return( false ) ; } 
	if( nexc == 0 ) { NOPX; SET_407_DEVTO(resp,mlen) ; return( true ) ; } 
    }

    return( true ) ;
}



static const char* cClientOptions[] = {
    "position get",
    "state get",
    "wait %d",
    "(axis1|axis2) move %d",
    "(axis1|axis2) goto %d",
    "(axis1|axis2) home",
    "(axis1|axis2) rate %d",
    "clear"
    } ;

int Unidex511::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

#if 0

6.4.11. PSn: Print Status
The PSn command returns a 32 bit number. The "n" is the status word requested. The
value and corresponding status of "n" is shown in Table 6-19.
Table 6-19. Values of "n" and Corresponding Status for the PSn Command
n Status
0 16 IN/8 OUT inputs
1 Axis 1 status
2 Axis 2 status
3 Axis 3 status
4 Axis 4 status
5 Axis enable / in position / comm / queue not empty / halted
6 Current MFO
7 Joystick status
8 Current board number
9 Commands in queue for plane 1
10 Commands in queue for plane 2
11 Commands in queue for plane 3
12 Commands in queue for plane 4

Returned values for this function follow.
n = 0 returns 16 input line condition
n = 1-4 returns fault/trap/limit information for axes 1-4, respectively,
(with the following bit assignments):
bit 0 1 = position error, 0 = no fault
bit 1 1 = RMS current error, 0 = no fault
bit 2 1 = integral error, 0 = no fault
bit 3 1 = hardware limit +, 0 = no fault
bit 4 1 = hardware limit -, 0 = no fault
bit 5 1 = software limit +, 0 = no fault
bit 6 1 = software limit -, 0 = no fault
bit 7 1 = driver fault, 0 = no fault
bit 8 1 = feedback device error, 0 = no fault
bits 9-11 unused
bit 12 1 = feedrate > max setting error, 0 = no fault
bit 13 1 = velocity error, 0 = no fault
bit 14 1 = emergency stop, 0 = no fault
bits 15-30 unused
n = 5 returns axis active/in position/plane information (with the
following bit assignments):
bit 0 1 = axis 1 enabled, 0 = disabled
bit 1 1 = axis 2 enabled, 0 = disabled
bit 2 1 = axis 3 enabled, 0 = disabled
bit 3 1 = axis 4 enabled, 0 = disabled
bit 4 1 = axis 1 not in position, 0 = in position
bit 5 1 = axis 2 not in position, 0 = in position
bit 6 1 = axis 3 not in position, 0 = in position
bit 7 1 = axis 4 not in position, 0 = in position
bit 8 1 = plane 1 comm. busy, 0 = comm. OK
bit 9 1 = plane 2 comm. busy, 0 = comm. OK
bit 10 1 = plane 3 comm. busy, 0 = comm. OK
bit 11 1 = plane 4 comm. busy, 0 = comm. OK
bit 12 1 = queue 1 buffer is not empty, 0 = empty *
bit 13 1 = queue 2 buffer is not empty, 0 = empty *
bit 14 1 = queue 3 buffer is not empty, 0 = empty *
bit 15 1 = queue 4 buffer is not empty, 0 = empty *
bit 16 1 = plane 1 halted, 0 = plane 1 "running"
bit 17 1 = plane 2 halted, 0 = plane 2 "running"
bit 18 1 = plane 3 halted, 0 = plane 3 "running"
bit 19 1 = plane 4 halted, 0 = plane 4 "running"
bit 20 unused
bit 21 1 = feedhold active
bit 22 1 = DSP interrupt generated
bit 23 1 = command in DSP buffer

n = 6 returns the current manual feedrate override % (MFO)
n = 7 returns the joystick status (with the following bit assignments):
bit 0-1 00 = high velocity mode
01 = low velocity mode
1x = absolute positioning mode
bit 2-3 00 = plane 1 active
01 = plane 2 active
1x = block delete active (digitizing mode)
bit 4 1 = joystick interlock open (error)
0 = joystick interlock closed (normal)
bit 5-7 000 = no current horizontal axis defined
001 = axis 1 active
010 = axis 2 active
011 = axis 3 active
100 = axis 4 active
bit 8-10 current vertical axis (0-4)
000 = no current vertical axis defined
001 = axis 1 is the active vertical axis
010 = axis 2 is the active vertical axis
011 = axis 3 is the active vertical axis
100 = axis 4 is the active vertical axis
bit 11 1 = received joystick cancel command
bit 12-13 unused
bit 14 0 = joystick is deactivated
1 = joystick is now active
n = 8 returns the currently active board number (1-6)
n = 9-12 returns the number of actions remaining in queue for planes
1-4. This data is meaningless when used with the PLC or
QUEUE commands.

#endif
