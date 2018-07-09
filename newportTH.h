#ifndef __NewportTH_h__
#define __NewportTH_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class NewportTH : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    static const int nPeriodics = 2 ;
    int mPeriodics[nPeriodics] ;
public:
    NewportTH( void ) ;
    virtual void StartOps( void ) ;
    virtual int Parse( u_char*, u_char* ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __NewportTH_h__
