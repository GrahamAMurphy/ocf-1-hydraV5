#include <stdio.h>
#include "global.h"

static const int buildnum =
#include "build.h"
;

void PrintVersion( void )
{
    fprintf( DBGDEV, 
	"Version "
	VERSION " Build #%d. Created " __TIMESTAMP__ "\r\n", 
	buildnum
    ) ;

    snprintf( cVersion, sizeof(cVersion), "%s", VERSION ) ;
}
