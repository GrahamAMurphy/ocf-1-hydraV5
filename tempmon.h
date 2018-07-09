#ifndef __Tempmon_h__
#define __Tempmon_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class TempMon : public Client {
    u_char temp_names[16][64] ;
    u_char heater_names[16][64] ;
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    static const int nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    int n_sensors ;
    int n_heaters ;
    int w_sensor ;
public:
    TempMon( void ) ;
    virtual void StartOps( void ) ;
    virtual int Parse( u_char*, u_char* ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __Tempmon_h__
