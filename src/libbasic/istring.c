#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <wchar.h>
#include <string.h>
#include <ctype.h>

#define U_CHARSET_IS_UTF8 1

/* the unicode headers are not clean, they must be ordered properly */
#include <unicode/stringoptions.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>
#include <unicode/uloc.h>
#include <unicode/unorm.h>
#include <unicode/ucol.h>
#include <unicode/ucasemap.h>
#include <unicode/utf8.h>

#include "bdjstring.h"
#include "istring.h"

static UCollator  *ucoll = NULL;
static UCaseMap   *ucsm = NULL;

void
istringInit (const char *locale)
{
  UErrorCode status = U_ZERO_ERROR;

  if (ucoll == NULL) {
    ucoll = ucol_open (locale, &status);
  }
  if (ucsm == NULL) {
    ucsm = ucasemap_open (locale, U_FOLD_CASE_DEFAULT, &status);
  }
}

void
istringCleanup (void)
{
  if (ucoll != NULL) {
    ucol_close (ucoll);
    ucoll = NULL;
  }
  if (ucsm != NULL) {
    ucasemap_close (ucsm);
    ucsm = NULL;
  }
}

/* collated comparison */
int
istringCompare (const char *str1, const char *str2)
{
  UErrorCode status = U_ZERO_ERROR;
  int   rc = 0;

  rc = ucol_strcollUTF8 (ucoll, str1, -1, str2, -1, &status);
  return rc;
}

/* this counts code points, not glyphs */
size_t
istrlen (const char *str)
{
  int32_t   offset;
  int32_t   slen = 0;
  int32_t   c = 0;
  size_t    len = 0;

  if (str == NULL) {
    return 0;
  }

  slen = strlen (str);
  offset = 0;
  while (offset < slen) {
    U8_NEXT (str, offset, slen, c);
    ++len;
  }

  return len;
}

void
istringToLower (char *str)
{
  UErrorCode  status = U_ZERO_ERROR;
  char        *dest = NULL;
  size_t      sz;
  size_t      rsz;

  sz = strlen (str);
  dest = malloc (sz + 1);
  rsz = ucasemap_utf8ToLower (ucsm, dest, sz, str, sz, &status);
  if (rsz <= sz && status == U_ZERO_ERROR) {
    strlcpy (str, dest, sz);
    free (dest);
  }
}

void
istringToUpper (char *str)
{
  UErrorCode  status = U_ZERO_ERROR;
  char        *dest = NULL;
  size_t      sz;
  size_t      rsz;

  sz = strlen (str);
  dest = malloc (sz + 1);
  rsz = ucasemap_utf8ToUpper (ucsm, dest, sz, str, sz, &status);
  if (rsz <= sz && status == U_ZERO_ERROR) {
    strlcpy (str, dest, sz);
    free (dest);
  }
}
