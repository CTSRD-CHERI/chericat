#include <stdio.h>
#include <stdarg.h>

#ifndef COMMONS_H_
#define COMMONS_H_

#define DEBUG 0 

#define NOPRINT 0
#define INFO 1
#define VERBOSE 2 
#define TROUBLESHOOT 3

#define PRINT_LEVEL INFO 

#define debug_print(level, fmt, ...) \
	do { if (level <= PRINT_LEVEL) { fprintf(stdout, fmt, __VA_ARGS__); }} while(0)

#endif //COMMONS_H_
