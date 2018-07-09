#ifndef __ThermalServer_h__
#define __ThermalServer_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>
#include <pthread.h>

#include "server.h"
#include "support.h"

class ThermalServer : public Server {
    u_char *uc_temp_file ;
    void ReadInTempNames( void ) ;
    bool names_read ;
    virtual void ServerActive( void ) ;
    virtual void StartOps( void ) ;
    virtual int Parse( u_char *key, u_char *value ) ;
public:
    ThermalServer() ;
} ;

#endif // __ThermalServer_h__
