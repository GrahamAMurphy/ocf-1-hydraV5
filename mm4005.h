#ifndef __MM4005_h__
#define __MM4005_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class MM4005 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    virtual int Parse( u_char *, u_char * ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    int Retrieve( u_char*, int ) ;
    int Move( u_char*, int ) ;
    int Halt( u_char*, int ) ;
    int Motor( u_char*, int ) ;
    static int const nPeriodics = 2 ;
    int mPeriodics[nPeriodics] ;
    int FindAxis( u_char* ) ;
    u_char *ucAxis[3][2] ;

public:
    MM4005( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __MM4005_h__
