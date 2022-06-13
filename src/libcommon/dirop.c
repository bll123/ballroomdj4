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
#include <wchar.h>

#include <glib.h>

#include "bdj4.h"
#include "bdjstring.h"
#include "dirop.h"
#include "fileop.h"
#include "slist.h"
#include "osutils.h"
#include "pathutil.h"
#include "queue.h"

#if _hdr_windows
# include <windows.h>
#endif

slist_t *
diropBasicDirList (const char *dirname, const char *extension)
{
  dirhandle_t   *dh;
  char          *fname;
  slist_t       *fileList;
  pathinfo_t    *pi;
  char          temp [MAXPATHLEN];
  char          *cvtname;
  gsize         bread, bwrite;
  GError        *gerr = NULL;


  if (! fileopIsDirectory (dirname)) {
    return NULL;
  }

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
diropRecursiveDirList (const char *dirname, int flags)
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

  if (! fileopIsDirectory (dirname)) {
    return NULL;
  }

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
          if ((flags & DIROP_DIRS) == DIROP_DIRS) {
            p = temp + dirnamelen + 1;
            slistSetStr (fileList, temp, p);
          }
        } else {
          if ((flags & DIROP_FILES) == DIROP_FILES) {
            p = temp + dirnamelen + 1;
            slistSetStr (fileList, temp, p);
          }
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
diropDeleteDir (const char *dirname)
{
  dirhandle_t   *dh;
  char          *fname;
  char          temp [MAXPATHLEN];


  if (! fileopIsDirectory (dirname)) {
    return;
  }

  dh = osDirOpen (dirname);
  while ((fname = osDirIterate (dh)) != NULL) {
    if (strcmp (fname, ".") == 0 ||
        strcmp (fname, "..") == 0) {
      free (fname);
      continue;
    }

    snprintf (temp, sizeof (temp), "%s/%s", dirname, fname);
    if (fname != NULL) {
      if (fileopIsDirectory (temp)) {
        diropDeleteDir (temp);
      } else {
        fileopDelete (temp);
      }
    }
    free (fname);
  }

  osDirClose (dh);

#if _lib_RemoveDirectoryW
  {
    wchar_t       * tdirname;

    tdirname = osToFSFilename (dirname);
    RemoveDirectoryW (tdirname);
    free (tdirname);
  }
#else
  rmdir (dirname);
#endif
}

