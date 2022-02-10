#ifndef INC_BDJSTRING
#define INC_BDJSTRING

#include "config.h"

int       stringCompare (void *, void *);
int       istringCompare (void *, void *);
size_t    istrlen (const char *);
char *    stringToLower (char *s);
char *    stringToUpper (char *s);
void      stringTrim (char *s);

#if ! _lib_strlcat
size_t strlcat(char *dst, const char *src, size_t siz);
#endif
#if ! _lib_strlcpy
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

#endif /* INC_BDJSTRING */
