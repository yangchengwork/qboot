#ifndef BIOS_STRING_H
#define BIOS_STRING_H

#include <stddef.h>

unsigned long strlen(const char *buf);
char *strcat(char *dest, const char *src);
char *strcpy(char *dest, const char *src);
int strcmp(const char *a, const char *b);
char *strchr(const char *s, int c);
char *strstr(const char *s1, const char *s2);
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memchr(const void *s, int c, size_t n);

void *malloc(int n);
void *malloc_fseg(int n);

#endif
