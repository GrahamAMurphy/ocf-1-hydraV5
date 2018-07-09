#ifndef __Monochromator_h__
#define __Monochromator_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class Monochromator : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    int AddressWavelength( u_char*, int ) ;
    int AddressWavelengthNM( u_char*, int ) ;
    int AddressMirror( u_char*, int ) ;
    int AddressGrating( u_char*, int ) ;
    int AddressFilter( u_char*, int ) ;
    int AddressRate( u_char*, int ) ;
    static const int nPeriodics = 3 ;
    int mPeriodics[nPeriodics] ;
public:
    Monochromator( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __Monochromator_h__
