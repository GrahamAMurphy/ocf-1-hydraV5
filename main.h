void Loop( void ) ;
void RunPeriodics( void ) ;
int InitializePorts(FILE*, int) ;
void HandleKeyboard( void ) ;
int InsertAlias( u_char*, u_char* ) ;
void FindAndInsertAlias( u_char*) ;
void foreign( int argc, char **argv ) ;
void UpdateStatus( void ) ;
void DisplayConsoleStatus( FILE *out ) ;
void RequestDebug( int ) ;
void DisplayKeyboardCommands( void ) ;
void ResetAllRetries( void ) ;
