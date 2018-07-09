#ifndef __FilterWheel_h__
#define __FilterWheel_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class FilterWheel : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    int AddressFilter( u_char*, int ) ;
    enum FW_state_select { FW_state, FW_goal } ;
    int UpdateFWState( FW_state_select, u_char* ) ;
    static int const nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    int fw_state, fw_goal ;
    virtual void StartPeriodicTasks( void ) ;
    virtual void StopPeriodicTasks( void ) ;
public:
    FilterWheel( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __FilterWheel_h__
