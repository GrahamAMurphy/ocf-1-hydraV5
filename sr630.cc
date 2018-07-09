#include <math.h>
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
#include "sr630.h"
#include "support.h"
#include "periodic.h"

static enum { TypeN, TypeC, TypeP } param_types[32] ;
static u_char uc_param_names[32][64] ;

SR630::SR630( void )
{
    devtype = TypeSR630 ;
    devtypestring = "SR 630" ;
    cycle = 30000 ; // 30.0 secs ; // per value!
    access_mode = 1 ;
    uc_param_file = 0 ;
    DODBG(9)(DBGDEV, "Leaving sr630 constructor.\n" ) ; 
}

int SR630::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "names" ) == 0 ) {
        uc_param_file = UCP strdup( CP value ) ;
        return( true ) ;
    }
    return( Client::Parse( key, value ) ) ;
}

void SR630::ReadInParamNames( void )
{
    FILE *inp = fopen( CP uc_param_file, "r" ) ;
    if( ! inp ) return ;

    u_char line[256], t_type[256], t_use[256] ;
    u_char *status ;
    int w_param ;
    int n_read = 0 ;

    while( 1 ) {
        status = UCP fgets( CP line, 256, inp ) ;
        if( ! status ) break ;
        int nscan = sscanf( CP line, "%[^\t]\t%d\t\"%[^\"]", 
	    t_type, &w_param, t_use ) ;

        if( nscan != 3 ) break ;

        fprintf( stderr, "Param= %d -> %s (%s)\n", w_param, t_use, t_type ) ;
        if( w_param < 0 || w_param > 16 ) continue ;

	if( strcasecmp( CP t_type, "\"TC\"" ) == 0 ) {
	    param_types[w_param] = TypeC ;
	} else if( strcasecmp( CP t_type, "\"PT\"" ) == 0 ) {
	    param_types[w_param] = TypeP ;
	} else {
	    param_types[w_param] = TypeN ;
	}

        strncpy( CP uc_param_names[w_param], CP t_use, 128 ) ;
        n_read++ ;
    }
    if( n_read > 0 )
        names_read = true ;
}

void SR630::StartOps( void )
{
    DODBG(9)(DBGDEV, "%s: SR630 being started up.\n", name ) ;
    mStatus = OCF_Status->RequestStatus( nStatus ) ; // Lots of key states.

    u_char temp[256] ;
    snprintf( CP temp, 256, "%s 0 get", getAlias() ) ;
    mPeriodics[0] = PeriodicQ->Add( getDevice(), temp ) ;

    StartThread() ;
}

void SR630::StartPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->SetPeriod( mPeriodics[i], nPeriodics*cycle ) ;
        PeriodicQ->Schedule( mPeriodics[i], (i+1)*cycle  ) ;
        PeriodicQ->Enable( mPeriodics[i] ) ;
    }
}

void SR630::StopPeriodicTasks( void )
{
    for( int i = 0 ; i < nPeriodics ; i++ ) {
        PeriodicQ->Disable( mPeriodics[i] ) ;
    }
}

int SR630::DeviceOpen( void )
{
    int nexc ;
    u_char command[256] ;
    u_char resp[256] ;

    const char *cmds[] = { "",
	"chan1;tnom1,1.000;unit1,CENT;span1,0;ttyp1,T;tmax1,0.0000;tmin1,0.0000;alrm1,NO;scne1,NO;meas?1",
	"chan2;tnom2,1.000;unit2,CENT;span2,0;ttyp2,T;tmax2,0.0000;tmin2,0.0000;alrm2,NO;scne2,NO;meas?2",
	"chan3;tnom3,1.000;unit3,CENT;span3,0;ttyp3,T;tmax3,0.0000;tmin3,0.0000;alrm3,NO;scne3,NO;meas?3",
	"chan4;tnom4,1.000;unit4,CENT;span4,0;ttyp4,T;tmax4,0.0000;tmin4,0.0000;alrm4,NO;scne4,NO;meas?4",
	"chan5;tnom5,1.000;unit5,CENT;span5,0;ttyp5,T;tmax5,0.0000;tmin5,0.0000;alrm5,NO;scne5,NO;meas?5",
	"chan6;tnom6,1.000;unit6,CENT;span6,0;ttyp6,T;tmax6,0.0000;tmin6,0.0000;alrm6,NO;scne6,NO;meas?6",
	"chan7;tnom7,1.000;unit7,CENT;span7,0;ttyp7,T;tmax7,0.0000;tmin7,0.0000;alrm7,NO;scne7,NO;meas?7",
	"chan8;tnom8,1.000;unit8,CENT;span8,0;ttyp8,T;tmax8,0.0000;tmin8,0.0000;alrm8,NO;scne8,NO;meas?8",
	"chan9;tnom9,1.000;unit9,CENT;span9,0;ttyp9,T;tmax9,0.0000;tmin9,0.0000;alrm9,NO;scne9,NO;meas?9",
	"chan10;tnom10,1.000;unit10,CENT;span10,0;ttyp10,T;tmax10,0.0000;tmin10,0.0000;alrm10,NO;scne10,NO;meas?10",
	"chan11;tnom11,1.000;unit11,CENT;span11,0;ttyp11,T;tmax11,0.0000;tmin11,0.0000;alrm11,NO;scne11,NO;meas?11",
	"chan12;tnom12,1.000;unit12,CENT;span12,0;ttyp12,T;tmax12,0.0000;tmin12,0.0000;alrm12,NO;scne12,NO;meas?12",
	"chan13;tnom13,1.000;unit13,CENT;span13,0;ttyp13,T;tmax13,0.0000;tmin13,0.0000;alrm13,NO;scne13,NO;meas?13",
	"chan14;tnom14,1.000;unit14,DC;span14,0;ttyp14,T;tmax14,0.0000;tmin14,0.0000;alrm14,NO;scne14,NO;meas?14",
	"chan15;tnom15,1.000;unit15,DC;span15,0;ttyp15,T;tmax15,0.0000;tmin15,0.0000;alrm15,NO;scne15,NO;meas?15",
	"chan16;tnom16,1.000;unit16,DC;span16,0;ttyp16,T;tmax16,0.0000;tmin16,0.0000;alrm16,NO;scne16,NO;meas?16" } ;

    static int save_mode = -1 ;

    if( uc_param_file )
	ReadInParamNames() ;

    if( access_mode == save_mode ) {
	access_mode = ( access_mode + 1 ) % 2 ;
    }
    save_mode = access_mode ;

    nexc = Exchange( "*RST", w_timeo, resp, sizeof(resp), -r_timeo ) ;
    if( nexc < 0 ) return( false ) ; // something happened.

    for( int i = 1 ; i <= nStatus ; i++ ) {
	snprintf( CP command, sizeof(command), "%s%s", cmds[i], 
	    (access_mode == 1) ? ";" : "" ) ;

	StatusLog->L_Write_U( name, "CMD", command ) ;
	if( access_mode == 1 ) {
	    nexc = Exchange( command, w_timeo, resp, sizeof(resp), r_timeo ) ;
	    if( nexc <= 0 ) return( false ) ; // something happened.
	} else {
	    nexc = Exchange( command, w_timeo, resp, sizeof(resp), -r_timeo ) ;
	    if( nexc < 0 ) return( false ) ; // something happened.
	    nexc = Exchange( ";", w_timeo, resp, sizeof(resp), r_timeo ) ;
	    if( nexc <= 0 ) return( false ) ; // something happened.
	}
    }
    save_mode = -1 ;

    StartPeriodicTasks() ;
    return( true ) ;
}

void SR630::DeviceClose( void )
{
    StopPeriodicTasks() ;
    for( int i = 0 ; i < nStatus ; i++ )
        OCF_Status->Disconnected( mStatus + i ) ;
}


// Only returns false if we deem a communication error demands or implies
// a disconnect.
int SR630::AddressValue( u_char *resp, int mlen )
{
    u_char command[256] ;
    u_char tmp[256] ;
    int w_couple ;
    static int t_couple = 0 ;
    int nexc ;

    w_couple = strtol( CP client_cmd.cmd[1], 0, 0 ) ;

    if( ! w_couple ) {
	w_couple = ++t_couple ;
	t_couple = (t_couple%nStatus) ;
    }

    if( w_couple < 1 || w_couple > nStatus ) {
	snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", 
	    client_cmd.cmd[1], name ) ; 
	return( true ) ;
    }

#define NOSTAT NoStatus(w_couple-1)

    snprintf( CP command, sizeof(command), "meas?%d%s", w_couple,
	(access_mode == 1) ? ";" : "" ) ;

    StatusLog->L_Write_U( name, "CMD", command ) ;
    if( access_mode == 1 ) {
	nexc = Exchange( command, w_timeo, tmp, sizeof(tmp), r_timeo ) ;
	if( nexc < 0 ) { NOSTAT ; SET_406_BADCON(resp,mlen) ; return( false ); }
	if( nexc == 0 ) { NOSTAT ; SET_407_DEVTO(resp,mlen) ; return( true ) ; }
    } else {
	nexc = Exchange( command, w_timeo, tmp, sizeof(tmp), -r_timeo ) ;
	if( nexc < 0 ) { NOSTAT ; SET_406_BADCON(resp,mlen) ; return( false ); }
	nexc = Exchange( ";", w_timeo, tmp, sizeof(tmp), r_timeo ) ;
	if( nexc < 0 ) { NOSTAT ; SET_406_BADCON(resp,mlen) ; return( false ); }
	if( nexc == 0 ) { NOSTAT ; SET_407_DEVTO(resp,mlen) ; return( true ) ; }
    }

    double value = 0.0 ;
    int nscan = sscanf( CP tmp, "%lf", &value ) ;
    if( nscan != 1 || (fabs(value) > 1e7) ) {
	SET_409_BADRESP(resp,mlen,tmp) ;
	NOSTAT ;
	errors++ ;
    }
    StatusLog->L_Write_U( name, "RTN", tmp ) ;

    if( param_types[w_couple] == TypeC ) {
	snprintf( CP resp, mlen, "%.1f", value ) ;
	snprintf( CP command, sizeof(command), "%s (C)", uc_param_names[w_couple] ) ;
    } else if( param_types[w_couple] == TypeP ) {
	double pressure = 8e-13 * exp( 3.44540 * value ) ;
	snprintf( CP resp, mlen, "%.4e", pressure ) ;
	snprintf( CP command, sizeof(command), "%s (Torr)", uc_param_names[w_couple] ) ;
    } else {
	snprintf( CP resp, mlen, "%g", value ) ;
	snprintf( CP command, sizeof(command), "Channel %d", w_couple ) ;
    }

    AddStatus( w_couple-1, command, resp ) ;
    StatusLog->L_Command_U( name, command, resp ) ;
    return( true ) ;
}

int SR630::AddressMode( u_char *resp, int mlen )
{
    if( client_cmd.nCmds==3 && STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
        snprintf( CP resp, mlen, "%d", access_mode ) ;
        return( true ) ;
    }

    if( client_cmd.nCmds != 4 || STRCASECMP(client_cmd.cmd[2], "set" ) != 0) {
        DODBG(0)(DBGDEV, "Bad command: %s\n", client_cmd.cmd[2] ) ;
        snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s", client_cmd.cmd[1], name ) ;
        return( true ) ;
    }

    int value ;
    int nscan = sscanf( CP client_cmd.cmd[3], "%d", &value ) ;
    if( nscan != 1 || value < 0 || value > 1 ) {
        DODBG(0)(DBGDEV, "Bad mode value: %s\n", client_cmd.cmd[3] ) ;
        snprintf( CP resp, mlen, ERR_408_BADCMD ": %s to %s",
        client_cmd.cmd[1], name ) ;
        return( true ) ;
    }

    snprintf( CP resp, mlen, "%d", access_mode ) ;
    access_mode = value ;

    StatusLog->L_Command_U( name, "Access mode", resp ) ;
    return( true ) ;
}

int SR630::HandleRequest( void )
{
    if( Client::HandleRequest() )
        return( true ) ;

    if( client_cmd.nCmds < 3 ) {
	mQueue->SetResponse( ERR_408_BADCMD ) ;
	return( true ) ;
    }

    u_char response[256] ;
    int retval ;

    if( STRCASECMP( client_cmd.cmd[1], "mode" ) == 0 ) {
        retval = AddressMode( response, sizeof(response) ) ;
    } else if( STRCASECMP( client_cmd.cmd[2], "get" ) == 0 ) {
	retval = AddressValue( response, sizeof(response) ) ;
    } else {
        DODBG(0)(DBGDEV, "Unknown command: %s\n", client_cmd.cmd[1] ) ;
        snprintf( CP response, sizeof(response), ERR_408_BADCMD ": %s to %s",
        client_cmd.cmd[1], name ) ; 
        retval = true ;
    }

    mQueue->SetResponse( response ) ; 
    DODBG(0)( DBGDEV, "client response %s: <%s>\n", name, response ) ;
    return( retval ) ;
}


static const char* cClientOptions[] = {
    "%d get",
    "mode get",
    "mode set %d"
    } ;

int SR630::ListOptions( int iN, u_char *cOption, int iOptionLength )
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

