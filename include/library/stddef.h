#ifndef __STDDEF_H__
#define __STDDEF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

#if defined(__cplusplus)
#define NULL		(0)
#else
#define NULL		((void *)0)
#endif

#if (__GNUC__ >= 4)
#define offsetof(type, member)	__builtin_offsetof(type, member)
#else
#define offsetof(type, field)	((size_t)(&((type *)0)->field))
#endif

#ifndef __WCHAR_TYPE__
#define __WCHAR_TYPE__ int
#endif
typedef __WCHAR_TYPE__ wchar_t;

enum {
	FALSE	= 0,
	TRUE	= 1,
};

#ifdef __cplusplus
}
#endif

#endif /* __STDDEF_H__ */
