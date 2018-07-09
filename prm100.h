#ifndef __PRM100_h__
#define __PRM100_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class PRM100 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    static int const nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    virtual void StartPeriodicTasks( void ) ;
    virtual void StopPeriodicTasks( void ) ;
    int AddressPRM100( u_char*, int ) ;
    int IntegrationTime( u_char*, int ) ;
    int CalculateTimeParameter( double ) ;
    double CalculateTrueTime( int ) ;
    u_char ucIntegrationParameter ;
public:
    PRM100( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __PRM100_h__
