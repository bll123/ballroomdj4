#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if _hdr_unistd
# include <unistd.h>
#endif
#if _hdr_io
# include <io.h>
#endif
#if _hdr_windows
# include <windows.h>
#endif

#include "fileop.h"
#include "bdjstring.h"
#include "portability.h"
#include "pathutil.h"
#include "sysvars.h"

/* boolean */
inline int
fileopExists (char *fname)
{
  struct stat   statbuf;

  int rc = stat (fname, &statbuf);
  return (rc == 0);
}

inline int
fileopDelete (const char *fname)
{
  int rc = unlink (fname);
  return rc;
}

int
fileopCopy (char *fname, char *nfn)
{
  char      cmd [MAXPATHLEN];
  char      tfname [MAXPATHLEN];
  char      tnfn [MAXPATHLEN];

  if (isWindows ()) {
    pathToWinPath (fname, tfname, MAXPATHLEN);
    pathToWinPath (nfn, tnfn, MAXPATHLEN);
    snprintf (cmd, MAXPATHLEN, "copy /y/b \"%s\" \"%s\" >NUL",
        tfname, tnfn);
  } else {
    snprintf (cmd, MAXPATHLEN, "cp -f '%s' '%s' >/dev/null 2>&1", fname, nfn);
  }
  int rc = system (cmd);
  return rc;
}

int
fileopMove (char *fname, char *nfn)
{
  int       rc = -1;

  /*
   * Windows won't rename to an existing file, but does
   * not return an error.
   */
  if (isWindows()) {
    fileopDelete (nfn);
  }
  rc = rename (fname, nfn);
  return rc;
}

int
fileopMakeDir (char *dirname)
{
  int rc;
#if _args_mkdir == 2 && _define_S_IRWXU
  rc = mkdir (dirname, S_IRWXU);
#endif
#if _args_mkdir == 1
  rc = mkdir (dirname);
#endif
  return rc;
}

