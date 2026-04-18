#ifndef __STDDEF_H__
#define __STDDEF_H__

#ifdef __cplusplus
extern "C" {
#endif

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

/* Standard type definitions needed by LVGL and compiler headers */
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef unsigned int		size_t;
#endif

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef signed int			ssize_t;
#endif

#ifndef _PTRDIFF_T_DEFINED
#define _PTRDIFF_T_DEFINED
typedef signed int			ptrdiff_t;
#endif

#ifndef _WCHAR_T_DEFINED
#define _WCHAR_T_DEFINED
#ifndef __cplusplus
typedef unsigned int		wchar_t;
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
