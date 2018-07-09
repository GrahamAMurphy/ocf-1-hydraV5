#ifndef __Unidex11_h__
#define __Unidex11_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class Unidex11 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    virtual int Parse( u_char *, u_char * ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    int Retrieve( u_char*, int ) ;
    int Move( u_char*, int ) ;
    int Goto( u_char*, int ) ;
    int SetRate( u_char*, int ) ;
    char Axis( u_char* ) ;
    int Clear( int ) ;
    static int const nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    int rate[2] ;
    u_char *ucAxis[2][2] ;
public:
    Unidex11( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __Unidex11_h__
