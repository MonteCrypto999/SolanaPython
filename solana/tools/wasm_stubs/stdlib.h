#ifndef _STDLIB_H
#define _STDLIB_H
#include <stddef.h>
void* malloc(size_t);
void* calloc(size_t, size_t);
void* realloc(void*, size_t);
void free(void*);
void abort(void);
int atoi(const char*);
long atol(const char*);
double atof(const char*);
long strtol(const char*, char**, int);
long long strtoll(const char*, char**, int);
double strtod(const char*, char**);
int abs(int);
long labs(long);
void exit(int);
void qsort(void*, size_t, size_t, int (*)(const void*, const void*));
#endif
