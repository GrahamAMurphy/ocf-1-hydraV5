#ifndef __Unidex511_h__
#define __Unidex511_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class Unidex511 : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    virtual int Parse( u_char *, u_char * ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    int Get( u_char*, int ) ;
    int Move( u_char*, int ) ;
    int Goto( u_char*, int ) ;
    int Home( u_char*, int ) ;
    int Wait( u_char*, int ) ;
    int Generic( u_char*, int ) ;
    int Rate( u_char*, int ) ;
    int FaultAcknowledge( u_char*, int ) ;
    char Axis( u_char* ) ;
    int Clear( int ) ;
    static int const nPeriodics = 2 ;
    int mPeriodics[nPeriodics] ;
    int rate[4] ;
    u_char *ucAxis[4][2] ;

public:
    Unidex511( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __Unidex511_h__
