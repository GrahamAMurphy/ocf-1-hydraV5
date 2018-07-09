#ifndef __Stream_h__
#define __Stream_h__

#pragma interface

#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>

#ifdef FILE_H_HAVE
#undef FILE_H_HAVE
#endif
#define FILE_H_HAVE (2)
#include "extern.h"

const static int TX_BUFFSZ = 1024 ;
const static int RX_BUFFSZ = 1024 ;
const static int MAXSTREAM = 32 ;
const static int MAXALIASES = 8 ;
const static u_char OUTPUT_TERMINATION[4] = "\r\n\r" ;
const static u_char INPUT_TERMINATION[4] = "\r\n\r" ;

#include "queue.h"

class Stream {
    int devicenum ;
    int WriteStream( const u_char*, int ) ;
    int ReadStream( u_char*, int, int ) ;
    int l_errno ;

protected:
    volatile int fd ;
    int iOutputTermS ;
    int iOutputTermL ;
    int iInputTermS ;
    int iInputTermL ;
    u_char *name ;
    u_char *alias[MAXALIASES] ;
    u_char buff_tx[TX_BUFFSZ] ;
    int tx_pnt ;
    u_char buff_rx[RX_BUFFSZ] ;
    int rx_pnt ;
    int r_timeo, w_timeo ;
    int mStatus ;
    volatile int stream_state ;
    int cycle, delay ;
    volatile int enabled ;
    volatile bool can_report ;
    volatile int errors, n_cmds ;
    inline void ClearNCmds( void ) { n_cmds = 0 ; }
    inline void ClearNErrs( void ) { errors = 0 ; }
    sem_t sem_device ;
    int is_a_client ;
    int is_a_server ;
    pthread_t m_t_thread ;
    void WaitOnSignal( void ) ;
    void ClearWait( void ) ;
    int Exchange( const void*, int, u_char*, int, int ) ;
    int DoExchange( const void*, int, u_char*, int, int ) ;
    bool flush_after_timeout ;
    bool last_rx_timedout ;
    inline void ResetRxTimedOut( void ) { last_rx_timedout = false ; } ;
    inline void SetRxTimedOut( void ) { last_rx_timedout = true ; } ;
    int GetRxTimedOut( void ) ;
    virtual int Parse( u_char *key, u_char *value ) ;

public:
    Stream() ;
    virtual ~Stream() ;
    inline void Enable( bool start ) { enabled = start ; }
    inline int IsEnabled( void ) { return( enabled ) ; }
    inline int getFd( void ) { return( fd ) ; } ;
    inline void setFd( int in_fd ) { fd = in_fd ; } ;
    inline void setDevice( int device ) { devicenum = device ; } ;
    inline int getDevice( void ) { return( devicenum ) ; } ;
    void setName( const u_char* ) ;
    inline void setName( const char *in ) { setName( (const u_char*)in ) ; } ;
    inline const u_char* getName( void ) { return( (const u_char*) name ) ; } ;
    void setAlias( u_char* ) ;
    inline void setAlias( char *in ) { setAlias( (u_char*)in ) ; } ;
    const u_char* getAlias( int ) ;
    inline const u_char* getAlias( void ) { return getAlias( 0 ) ; } ;

    enum DeviceType { 
	TypeNone = 0, 
	TypeServer,
	TypeClient,
	TypeTempMon,
	TypeMonochromator,
	TypeFilterWheel,
	TypeSR630,
	TypeMM4005,
	TypeKE617,
	TypeKE6514,
	TypeLabsphere,
	TypeKlingerMC4,
	TypeUnidex11,
	TypeUnidex511,
	TypePRM100,
	TypePM2813,
	TypeUDTS370,
	TypeUDTS470,
	TypeHTTP,
	TypeXML,
	TypeThermalServer,
	TypeSBI4000,
	TypeNewmark,
	TypeNewportTH
	} ;
    void setType( DeviceType device ) { devtype = device ; } ;
    DeviceType getType( void ) { return( devtype ) ; } ;
    const char *getTypeString( void ) ;
    Queue *mQueue ;
    virtual void StartOps( void ) ;
    virtual void RunThread( void ) ;
    virtual void StopOps( void ) ;
    void StartThread( void ) ;
    void SignalDevice( void ) ;
    int SetOptions( u_char* ) ;
    virtual void PleaseReport( int, int ) ;
    inline void Enable( void ) { enabled = true ; }
    inline void Disable( void ) { enabled = false ; }
    inline int IsAClient( void ) { return( is_a_client ) ; } ;
    inline int IsAServer( void ) { return( is_a_server ) ; } ;
    bool Active( void ) ;

protected:
    const char *devtypestring ;
    DeviceType devtype ;
} ;

EXTERN Stream *streams[MAXSTREAM] ;

// put this prototype here for now.
void * RunObjectInThread( void * ) ;


#endif // __Stream_h__
