#define ERR_100_NOWAIT	"ERR 100 Not waiting."
#define ERR_101_CLRERR	"ERR 101 Cleared error count."
#define ERR_102_CLRCMD	"ERR 102 Cleared command count."
#define ERR_103_RECNCT	"ERR 103 Forced reconnect."
#define ERR_104_PERIOD	"ERR 104 Set Period."
#define ERR_105_DEVTO	"ERR 105 Request timed out as expected."
#define ERR_106_NULL	"ERR 106 No response."
#define ERR_107_IGN	"ERR 107 Response ignored."

#define ERR_400_NULL	"ERR 400 Null command"
#define ERR_401_UKNTGT	"ERR 401 Target of command not known"
#define ERR_402_BADTGT	"ERR 402 Target has us confused"
#define ERR_403_TGTOFF	"ERR 403 Target is not active"
#define ERR_404_QUEFLL	"ERR 404 No room on queue"
#define ERR_405_SEQTO	"ERR 405 Sequencer timed out"
#define ERR_406_BADCON	"ERR 406 Connection has failed."
#define ERR_407_DEVTO	"ERR 407 Request timed out"
#define ERR_408_BADCMD	"ERR 408 Ill-formed command"
#define ERR_409_BADRESP	"ERR 409 Bad response from device"
#define ERR_410_BADRATE	"ERR 410 Unsupported Unidex 11 rate"
#define ERR_411_NOTCLNT	"ERR 411 Target is not a client"

#define SET_406_BADCON(a,b)	strncpy( (char*)(a), ERR_406_BADCON, (b) )
#define SET_407_DEVTO(a,b)	strncpy( (char*)(a), ERR_407_DEVTO, (b) )
#define SET_409_BADRESP(a,b,c)	snprintf( (char*)(a),(b),ERR_409_BADRESP "<%s>", (c) )
#define SET_105_DEVTO(a,b)	strncpy( (char*)(a), ERR_105_DEVTO, (b) )
