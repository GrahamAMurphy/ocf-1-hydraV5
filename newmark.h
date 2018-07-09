#ifndef __Newmark_h__
#define __Newmark_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class Newmark : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    virtual int Parse( u_char *, u_char * ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    int Retrieve( u_char*, int ) ;
    int Move( u_char*, int ) ;
    int AbortMotion( u_char*, int ) ;
    int Motor( u_char*, int ) ;
    static int const nPeriodics = 2 ;
    int mPeriodics[nPeriodics] ;
    char Axis( u_char* ) ;
    u_char *ucAxis[2][2] ;

public:
    Newmark( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __Newmark_h__
