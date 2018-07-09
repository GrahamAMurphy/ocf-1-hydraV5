#ifndef __KlingerMC4_h__
#define __KlingerMC4_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class KlingerMC4 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    virtual int Parse( u_char *, u_char * ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    char Axis( u_char* ) ;
    int Retrieve( u_char*, int ) ;
    int Move( u_char*, int ) ;
    int Goto( u_char*, int ) ;
    int Motor( u_char*, int ) ;
    int SetRate( u_char*, int ) ;
    int Home( u_char*, int ) ;
    int Zero( u_char*, int ) ;
    static int const nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    u_char *ucAxis[4][2] ;

public:
    KlingerMC4( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __KlingerMC4_h__
