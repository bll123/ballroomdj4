#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>

#include "bdj4.h"
#include "bdjstring.h"
#include "filemanip.h"
#include "fileop.h"
#include "slist.h"
#include "osutils.h"
#include "pathutil.h"
#include "queue.h"
#include "sysvars.h"

#if _hdr_windows
# include <windows.h>
#endif

int
filemanipMove (char *fname, char *nfn)
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
filemanipCopy (char *fname, char *nfn)
{
  char      cmd [MAXPATHLEN];
  char      tfname [MAXPATHLEN];
  char      tnfn [MAXPATHLEN];
  int       rc = -1;


  if (isWindows ()) {
    pathToWinPath (tfname, fname, MAXPATHLEN);
    pathToWinPath (tnfn, nfn, MAXPATHLEN);
#if _lib_CopyFileW
    {
      wchar_t   *wtfname;
      wchar_t   *wtnfn;
      wtfname = osToWideString (tfname);
      wtnfn = osToWideString (tnfn);

      rc = CopyFileW (wtfname, wtnfn, 0);
      rc = (rc == 0);
      free (wtfname);
      free (wtnfn);
    }
#endif
  } else {
    snprintf (cmd, sizeof (cmd), "cp -f '%s' '%s' >/dev/null 2>&1", fname, nfn);
    rc = system (cmd);
  }
  return rc;
}

/* link if possible, otherwise copy */
/* windows has had symlinks for ages, but they require either */
/* admin permission, or have the machine in developer mode */
int
filemanipLinkCopy (char *fname, char *nfn)
{
  int       rc = -1;

  if (isWindows ()) {
    rc = filemanipCopy (fname, nfn);
  } else {
#if _lib_symlink
    rc = symlink (fname, nfn);
#endif
  }
  return rc;
}

slist_t *
filemanipBasicDirList (char *dirname, char *extension)
{
  dirhandle_t   *dh;
  char          *fname;
  slist_t       *fileList;
  pathinfo_t    *pi;
  char          temp [MAXPATHLEN];
  char          *cvtname;
  gsize         bread, bwrite;
  GError        *gerr = NULL;


  snprintf (temp, sizeof (temp), "basic-dir-%s", dirname);
  if (extension != NULL) {
    strlcat (temp, "-", sizeof (temp));
    strlcat (temp, extension, sizeof (temp));
  }
  fileList = slistAlloc (temp, LIST_UNORDERED, NULL);
  dh = osDirOpen (dirname);
  while ((fname = osDirIterate (dh)) != NULL) {
    snprintf (temp, sizeof (temp), "%s/%s", dirname, fname);
    if (fileopIsDirectory (temp)) {
      free (fname);
      continue;
    }

    if (extension != NULL) {
      pi = pathInfo (fname);
      if (pathInfoExtCheck (pi, extension) == false) {
        pathInfoFree (pi);
        free (fname);
        continue;
      }
      pathInfoFree (pi);
    }

    gerr = NULL;
    cvtname = g_filename_to_utf8 (fname, strlen (fname),
        &bread, &bwrite, &gerr);
    if (cvtname != NULL) {
      slistSetStr (fileList, cvtname, NULL);
    }
    free (cvtname);
    free (fname);
  }
  osDirClose (dh);

  return fileList;
}

slist_t *
filemanipRecursiveDirList (char *dirname)
{
  dirhandle_t   *dh;
  char          *fname;
  slist_t       *fileList;
  queue_t       *dirQueue;
  char          temp [MAXPATHLEN];
  char          *cvtname;
  char          *p;
  size_t        dirnamelen;
  gsize         bread, bwrite;
  GError        *gerr = NULL;

  dirnamelen = strlen (dirname);
  snprintf (temp, sizeof (temp), "rec-dir-%s", dirname);
  fileList = slistAlloc (temp, LIST_UNORDERED, NULL);
  dirQueue = queueAlloc (free);

  queuePush (dirQueue, strdup (dirname));
  while (queueGetCount (dirQueue) > 0) {
    char  *dir;

    dir = queuePop (dirQueue);

    dh = osDirOpen (dir);
    while ((fname = osDirIterate (dh)) != NULL) {
      if (strcmp (fname, ".") == 0 ||
          strcmp (fname, "..") == 0) {
        free (fname);
        continue;
      }

      gerr = NULL;
      cvtname = g_filename_to_utf8 (fname, strlen (fname),
          &bread, &bwrite, &gerr);
      snprintf (temp, sizeof (temp), "%s/%s", dir, cvtname);
      if (cvtname != NULL) {
        if (fileopIsDirectory (temp)) {
          queuePush (dirQueue, strdup (temp));
        } else {
          p = temp + dirnamelen + 1;
          slistSetStr (fileList, temp, p);
        }
      }
      free (cvtname);
      free (fname);
    }

    osDirClose (dh);
    free (dir);
  }
  queueFree (dirQueue);

  /* need it sorted so that the relative filename can be retrieved */
  slistSort (fileList);
  return fileList;
}

void
filemanipDeleteDir (const char *dir)
{
  char      cmd [MAXPATHLEN];
  char      tdir [MAXPATHLEN];

  if (! fileopIsDirectory (dir)) {
    return;
  }

  if (isWindows ()) {
    pathToWinPath (tdir, dir, sizeof (tdir));
    snprintf (cmd, sizeof (cmd), "rmdir /s/q \"%s\" >NUL", tdir);
  } else {
    snprintf (cmd, sizeof (cmd), "rm -rf '%s' >/dev/null 2>&1", dir);
  }
  system (cmd);
}

