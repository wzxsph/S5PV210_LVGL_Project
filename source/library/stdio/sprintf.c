/*
 * libc/stdio/sprintf.c
 */

#include <stdio.h>

#include <limits.h>

int sprintf(char * buf, const char * fmt, ...)
{
	va_list ap;
	int rv;

	va_start(ap, fmt);
	rv = vsnprintf(buf, INT_MAX, fmt, ap);
	va_end(ap);

	return rv;
}
