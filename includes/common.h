#include <stdio.h>
#include <stdarg.h>

#ifndef COMMON_H_
#define COMMON_H_

#define DEBUG 0 

#define NOPRINT 0
#define INFO 1
#define VERBOSE 2 
#define TROUBLESHOOT 3

int print_level;

#define debug_print(level, fmt, ...) \
	do { if (level <= print_level) { fprintf(stdout, fmt, __VA_ARGS__); }} while(0)

void set_print_level(int level);

#endif //COMMON_H_
