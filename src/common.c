#include "common.h"
#include "stdio.h"
#include "stdarg.h"

int print_level;

void set_print_level(int level) {
	print_level = level;
}

void format_string(char *fmt, va_list argptr, char *formatted_string);

void debug_print(int level, const char *fmt, ...) 
{
	if (level <= print_level) {
		va_list argptr;
		va_start(argptr, fmt);
		vfprintf(stdout, fmt, argptr);
		va_end(argptr);
	}
}
		
