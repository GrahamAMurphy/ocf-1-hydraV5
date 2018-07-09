#ifndef __KE6514_h__
#define __KE6514_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class KE6514 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    int AddressKE6514( u_char*, int ) ;
    static int const nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    virtual void StartPeriodicTasks( void ) ;
    virtual void StopPeriodicTasks( void ) ;
public:
    KE6514( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __KE6514_h__
