#ifndef __global_h__
#define __global_h__

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#define FILE_H_HAVE	(1)
#include "extern.h"

EXTERN volatile int DbgLevel INITVALUE(0) ;
EXTERN FILE *DBGDEV INITVALUE(0) ;
EXTERN char *DbgFile INITVALUE(0) ;

#define DODBG(a)        if( DbgLevel >= a ) fprintf

#define ELEMENTS(a)     (sizeof(a)/sizeof(a[0]))

#define CP (char*)
#define UCP (u_char*)

#define STRMATCH(a,b)	( (a) && (b) && (strcasecmp( CP(a), CP(b) ) == 0 ) )
#define STRCASECMP(a,b)	( strcasecmp( CP(a), CP(b) ) )

#ifndef linux
#define PTHREAD_MUTEX_TIMED_NP PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_FAST_NP PTHREAD_MUTEX_TIMED_NP
#endif

EXTERN int iSigRestart INITVALUE(0) ;

typedef struct { 
    unsigned char *in ; unsigned char *key ; unsigned char *value ; 
} sParse ;

#define TOKEN_CMD_MAXLINE	(4096)
#define TOKEN_CMD_MAXARGS	(64)
typedef struct {
    u_char line[TOKEN_CMD_MAXLINE] ;
    u_char *(cmd[TOKEN_CMD_MAXARGS]) ;
    int nCmds ;
} sCmd ;

#define timer_cmp(tvp, OP, uvp )\
    ( ( (tvp).tv_sec OP (uvp).tv_sec ) ||\
      ( ( (tvp).tv_sec == (uvp).tv_sec ) &&\
      ( ( (tvp).tv_usec OP (uvp).tv_usec) ) ) )

EXTERN u_char *test_target_0 INITVALUE(0) ;

EXTERN time_t StartTicks ;
EXTERN u_char NameInUT[256] ;

EXTERN u_char *log_prefix INITVALUE(0) ;
EXTERN u_char *log_suffix INITVALUE(0) ;

EXTERN u_char *this_host INITVALUE(0) ;

EXTERN u_char *status_dir INITVALUE(0) ;
EXTERN u_char *status_file INITVALUE(0) ;

EXTERN u_char *http_port INITVALUE(0) ;
EXTERN u_char *xml_port INITVALUE(0) ;

EXTERN volatile bool bRunning INITVALUE(true) ;
EXTERN volatile bool bDoRestart INITVALUE(false) ;

#define MAXDEV		(32)
#define MAXITEM		(12)
#define MAXDEVSTR	(32)
EXTERN char gStatus[MAXDEV][MAXITEM][MAXDEVSTR] ;
EXTERN int nStatus INITVALUE(0) ;
EXTERN int gMaxLen[MAXITEM] ;
EXTERN time_t status_time INITVALUE(0) ;

EXTERN double EpochMET INITVALUE(0.0) ;
EXTERN char cVersion[64] ;

#include "logging.h"
#include "errors.h"

#endif // __global_h__
