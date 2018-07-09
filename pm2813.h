#ifndef __PM2813_h__
#define __PM2813_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class PM2813 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    int AddressPM2813( u_char*, int ) ;
    int StatusUpdate( int, u_char*, int ) ;
    static int const nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    virtual void StartPeriodicTasks( void ) ;
    virtual void StopPeriodicTasks( void ) ;
public:
    PM2813( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __PM2813_h__
