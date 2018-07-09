#ifndef __UDTS370_h__
#define __UDTS370_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class UDTS370 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    int AddressUDTS370( u_char*, int ) ;
    int AddressWavelength( u_char*, int ) ;
    int AddressCalibrationSet( u_char*, int ) ;
    int AddressUnits( u_char*, int ) ;
    static int const nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    int s370_calset ;
    int s370_wavelength ;
    u_char s370_units[16] ;
    int SetCalibration( int ) ;
    int SetWavelength( int ) ;
    int SetUnits( u_char* ) ;
    virtual void StartPeriodicTasks( void ) ;
    virtual void StopPeriodicTasks( void ) ;
public:
    UDTS370( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __UDTS370_h__
