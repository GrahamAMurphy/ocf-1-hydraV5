#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>
#include <sys/types.h>

#include <iostream>

#pragma implementation

#include "global.h"
#include "stream.h"
#include "thermalserver.h"
#include "support.h"

static u_char uc_temp_names[64][128] ;

ThermalServer::ThermalServer( void )
{
    devtype = TypeThermalServer ;
    devtypestring = "ThermalServer" ;
    uc_temp_file = 0 ;
    names_read = false ;
    for( int i = 0 ; i < 64 ; i++ )
	snprintf( CP uc_temp_names[i], 128, "Temp %d", i ) ;

    DODBG(9)(DBGDEV, "Leaving thermal server constructor.\n" ) ;
}

int ThermalServer::Parse( u_char *key, u_char *value )
{
    if( strcasecmp( CP key, "names" ) == 0 ) {
	uc_temp_file = UCP strdup( CP value ) ;
	return( true ) ;
    }
    
    return( Server::Parse( key, value ) ) ;
}

void ThermalServer::ReadInTempNames( void )
{
    FILE *inp = fopen( CP uc_temp_file, "r" ) ;
    if( ! inp ) return ;

    u_char line[256], t_use[256] ;
    u_char *status ;
    int w_temp ;
    int n_read = 0 ;

    while( 1 ) {
	status = UCP fgets( CP line, 256, inp ) ;
	if( ! status ) break ;
	int nscan = sscanf( CP line, "%*[^\t]\t%d\t\"%[^\"]", &w_temp, t_use ) ;
	if( nscan != 2 ) break ;
	fprintf( stderr, "temps= %d -> %s\n", w_temp, t_use ) ;
	if( w_temp < 0 || w_temp > 63 ) continue ;

	strncpy( CP uc_temp_names[w_temp], CP t_use, 128 ) ;
	n_read++ ;
    }
    if( n_read > 0 )
	names_read = true ;
}

void ThermalServer::StartOps( void )
{
    DODBG(9)(DBGDEV, "ThermalServer being started up.\n" ) ;
    mStatus = OCF_Status->RequestStatus( 64 ) ; // debug info.
    if( uc_temp_file ) ReadInTempNames() ;

    StartThread() ;
    DODBG(9)(DBGDEV, "ThermalServer has started.\n" ) ;
}

#define D_SYNC_0xFA	(0xfa)
#define D_SYNC_0xF3	(0xf3)

static union {
    u_char uc_val[8] ;
    double d_val ;
    u_long ul_val ;
} swaps ;

void ThermalServer::ServerActive( void )
{
    n_cmds = 0 ;
    rx_pnt = 0 ;
    int nread ;
    u_long ship_doubles ;
    u_char *p_key, s_value[256] ;
    double dMET = 0 ;

    while( 1 ) {
	nread = read( fd, buff_rx, 1 ) ;
	if( nread <= 0 ) { Disconnect() ; return ; }
	if( buff_rx[0] != D_SYNC_0xFA ) continue ;

	nread = read( fd, buff_rx, 1 ) ;
	if( nread <= 0 ) { Disconnect() ; return ; }
	if( buff_rx[0] != D_SYNC_0xF3 ) continue ;

	nread = read( fd, buff_rx, 4 ) ;
	if( nread <= 3 ) { Disconnect() ; return ; }

	for( int i = 0 ; i < 4 ; i++ )
	    swaps.uc_val[3-i] = buff_rx[i] ;
	ship_doubles = swaps.ul_val ;

	DODBG(4)(DBGDEV, "Receiving %ld doubles.\n", ship_doubles ) ;
	if( ship_doubles > 64 ) { Disconnect() ; return ; }

	for( u_int w_d = 0 ; w_d < ship_doubles ; w_d++ ) {
	    nread = read( fd, buff_rx, 8 ) ;
	    if( nread <= 7 ) { Disconnect() ; return ; }

	    for( int j = 0 ; j < 8 ; j++ )
		swaps.uc_val[7-j] = buff_rx[j] ;

	    if( w_d == 0 ) {
		p_key = UCP "MET" ;
		dMET = swaps.d_val ;
		snprintf( CP s_value, sizeof(s_value), "%.0f", swaps.d_val );
	    } else {
		p_key = uc_temp_names[w_d] ;
		snprintf( CP s_value, sizeof(s_value), "%.3f", swaps.d_val );
	    }
	    OCF_Status->AddStatus( mStatus+w_d, name, p_key, s_value ) ;
	}
	char c_status[256] ;
        snprintf( c_status, 256, "%ld doubles stamped %.0f", ship_doubles, dMET);
	StatusLog->L_Write_U( name, "INFO", c_status ) ;
    }
}

