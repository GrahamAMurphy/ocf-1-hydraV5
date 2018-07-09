#ifndef __extern_h__

#define __extern_h__

#if ( FILE_H_HAVE == FILE_H_NEED )

#define INITZERO = 0 
#define INITVALUE(a) = (a)
#define EXTERN

#else

#define INITZERO
#define INITVALUE(a)
#define EXTERN extern

#endif // __IS_GLOBAL_C__

#endif // __extern_h__
