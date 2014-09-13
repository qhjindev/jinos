/*
 * string.h
 *
 */

#ifndef STRING_H_
#define STRING_H_

#include "types.h"

int             memcmp(const void*, const void*, uint);
void*           memmove(void*, const void*, uint);
void*           memset(void*, int, uint);
void*           memcpy(void *dst, const void *src, uint n);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint);
char*           strncpy(char*, const char*, int);



#endif /* STRING_H_ */
