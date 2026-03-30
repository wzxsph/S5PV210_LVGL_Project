#ifndef __STDDEF_H__
#define __STDDEF_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Include types.h to get size_t, ptrdiff_t, ssize_t etc.
 * This is needed because LVGL and the GCC toolchain headers
 * expect these types to be available from <stddef.h>.
 */
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

#ifndef __WCHAR_T_DEFINED
#define __WCHAR_T_DEFINED
#ifndef __cplusplus
typedef unsigned int wchar_t;
#endif
#endif

enum {
	FALSE	= 0,
	TRUE	= 1,
};

#ifdef __cplusplus
}
#endif

#endif /* __STDDEF_H__ */
