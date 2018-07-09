#ifndef __SR630_h__
#define __SR630_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class SR630 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    int AddressValue( u_char*, int ) ;
    int AddressMode( u_char*, int ) ;
    static const int nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    int access_mode ;
    static const int nStatus = 16 ;
    u_char *uc_param_file ;
    void ReadInParamNames( void ) ;
    bool names_read ;
    virtual int Parse( u_char *key, u_char *value ) ;
public:
    SR630( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __SR630_h__
