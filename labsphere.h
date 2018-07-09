#ifndef __Labsphere_h__
#define __Labsphere_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "support.h"
#include "client.h"

class Labsphere : public Client {
    virtual int DeviceOpen( void ) ;
    virtual int HandleRequest( void ) ;
    virtual void DeviceClose( void ) ;
    void StartPeriodicTasks( void ) ;
    void StopPeriodicTasks( void ) ;
    int AddressDetector( u_char*, int ) ;
    int AddressAttenuator( u_char*, int ) ;
    static int const nPeriodics = 1 ;
    int mPeriodics[nPeriodics] ;
    void GetAttenuatorCmds( int, int, u_char ** ) ;
    int SetAttenuator( int, int, u_char*, int ) ;
    void JKJoke( u_char, u_char, u_char* ) ;
    int attenuator_state[4] ;
public:
    Labsphere( void ) ;
    virtual void StartOps( void ) ;
    virtual int ListOptions( int, u_char *, int ) ;
} ;

#endif // __Labsphere_h__
