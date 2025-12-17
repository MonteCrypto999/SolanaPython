#ifndef _STDIO_H
#define _STDIO_H
#include <stddef.h>
typedef void FILE;
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
int printf(const char*, ...);
int fprintf(FILE*, const char*, ...);
int sprintf(char*, const char*, ...);
int snprintf(char*, size_t, const char*, ...);
FILE* fopen(const char*, const char*);
int fclose(FILE*);
size_t fread(void*, size_t, size_t, FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
int fseek(FILE*, long, int);
long ftell(FILE*);
int fflush(FILE*);
int feof(FILE*);
int ferror(FILE*);
int fgetc(FILE*);
int fputc(int, FILE*);
char* fgets(char*, int, FILE*);
int fputs(const char*, FILE*);
int puts(const char*);
int getc(FILE*);
int putc(int, FILE*);
int getchar(void);
int putchar(int);
int sscanf(const char*, const char*, ...);
int vsnprintf(char*, size_t, const char*, __builtin_va_list);
int vfprintf(FILE*, const char*, __builtin_va_list);
#endif
