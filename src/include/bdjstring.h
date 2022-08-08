#ifndef INC_BDJSTRING
#define INC_BDJSTRING

#include "config.h"

char *    stringToLower (char *s);
char *    stringToUpper (char *s);
void      stringTrim (char *s);
void      stringTrimChar (char *s, unsigned char c);
int       istringCompare (void *, void *);
size_t    istrlen (const char *);
int       versionCompare (const char *v1, const char *v2);

#if ! _lib_strlcat
size_t strlcat(char *dst, const char *src, size_t siz);
#endif
#if ! _lib_strlcpy
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

#endif /* INC_BDJSTRING */
