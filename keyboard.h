#ifndef __Keyboard_h__
#define __Keyboard_h__

#pragma interface

#include <sys/types.h>
#include <termio.h>

class Keyboard : public Stream {
    int inited ;
    struct termios ncontrl, scontrl, save_contrl ;
public:
    void StartOps( void ) ;
    void InitializeKeyboard( void ) ;
    void ResetKeyboard( void ) ;
    void HandleRx( void ) ;
    Keyboard() ;
} ;

#endif // __Keyboard_h__
