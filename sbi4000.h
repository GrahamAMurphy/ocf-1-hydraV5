#ifndef __SBI4000_h__
#define __SBI4000_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class SBI4000 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;

    static int const nPeriodics = 3 ;
    int mPeriodics[nPeriodics] ;
    virtual void StartPeriodicTasks( void ) ;
    virtual void StopPeriodicTasks( void ) ;

    int AddressSBI4000( u_char *, int ) ;

public:
    SBI4000( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __SBI4000_h__
