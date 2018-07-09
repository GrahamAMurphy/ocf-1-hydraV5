#ifndef __UDTS470_h__
#define __UDTS470_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class UDTS470 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    int AddressUDTS470( u_char*, int ) ;
    int AddressChannel( u_char*, int ) ;
    int AddressWavelength( u_char*, int ) ;
    int AddressCalibrationSet( u_char*, int ) ;
    int AddressUnits( u_char*, int ) ;
    static int const nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    int s470_channel ;
    int s470_calset ;
    int s470_wavelength ;
    u_char s470_units[16] ;
    int SetCalibration( int ) ;
    int SetChannel( int ) ;
    int SetWavelength( int ) ;
    int SetUnits( u_char* ) ;
    void SetChannel( u_char* ) ;
    virtual void StartPeriodicTasks( void ) ;
    virtual void StopPeriodicTasks( void ) ;
public:
    UDTS470( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __UDTS470_h__
