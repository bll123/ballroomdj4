/*
 * the update process to
 * update all bdj4 data files to the latest version
 * also handles some settings for new installations.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <getopt.h>

#include "audiotag.h"
#include "bdj4.h"
#include "bdj4init.h"
#include "bdjopt.h"
#include "bdjregex.h"
#include "bdjstring.h"
#include "datafile.h"
#include "dirlist.h"
#include "dirop.h"
#include "filedata.h"
#include "filemanip.h"
#include "fileop.h"
#include "log.h"
#include "localeutil.h"
#include "musicdb.h"
#include "nlist.h"
#include "osprocess.h"
#include "osutils.h"
#include "pathbld.h"
#include "pathutil.h"
#include "slist.h"
#include "song.h"
#include "songutil.h"
#include "sysvars.h"
#include "tagdef.h"
#include "tmutil.h"

#define UPDATER_TMP_FILE "tmpupdater.txt"

enum {
  UPD_NOT_DONE,
  UPD_SKIP,
  UPD_COMPLETE,
};

/* Fix audio file tags: FIX_AF_...  */
/* Fix database       : FIX_DB_...  */
/* Fix playlist       : FIX_PL_...  */
enum {
  UPD_FIRST_VERS,
  UPD_CONVERTED,        // was the original installation converted ?
  UPD_FIX_AF_MB_TAG,
  UPD_MAX,
};

static datafilekey_t upddfkeys[] = {
  { "CONVERTED",        UPD_CONVERTED,      VALUE_NUM, NULL, -1 },
  { "FIRSTVERSION",     UPD_FIRST_VERS,     VALUE_STR, NULL, -1 },
  { "FIX_AF_MB_TAG",    UPD_FIX_AF_MB_TAG,  VALUE_NUM, NULL, -1 },
};
enum {
  UPD_DF_COUNT = (sizeof (upddfkeys) / sizeof (datafilekey_t))
};

static void updaterCleanFiles (void);
static void updaterCleanRegex (const char *basedir, const char *pattern);
static int  updateGetStatus (nlist_t *updlist, int key);

int
main (int argc, char *argv [])
{
  bool        newinstall = false;
  bool        converted = false;
  int         c = 0;
  int         option_index = 0;
  char        *home = NULL;
  char        homemusicdir [MAXPATHLEN];
  char        tbuff [MAXPATHLEN];
  int         value;
  char        *tval = NULL;
  bool        isbdj4 = false;
  bool        bdjoptchanged = false;
  int         haveitunes = 0;
  int         flags;
  int         statusflags [UPD_MAX];
  int         processflags [UPD_MAX];
  bool        processaf = false;
  bool        processdb = false;
  datafile_t  *df;
  nlist_t     *updlist = NULL;
  musicdb_t   *musicdb = NULL;

  static struct option bdj_options [] = {
    { "newinstall", no_argument,        NULL,   'n' },
    { "converted",  no_argument,        NULL,   'c' },
    { "bdj4updater",no_argument,        NULL,   0 },
    { "bdj4",       no_argument,        NULL,   'B' },
    /* ignored */
    { "nodetach",   no_argument,        NULL,   0 },
    { "wait",       no_argument,        NULL,   0 },
    { "debugself",  no_argument,        NULL,   0 },
    { "msys",       no_argument,        NULL,   0 },
    { "theme",      required_argument,  NULL,   0 },
    { NULL,         0,                  NULL,   0 }
  };

  optind = 0;
  while ((c = getopt_long_only (argc, argv, "Bnc", bdj_options, &option_index)) != -1) {
    switch (c) {
      case 'B': {
        isbdj4 = true;
        break;
      }
      case 'n': {
        newinstall = true;
        break;
      }
      case 'c': {
        converted = true;
        break;
      }
      default: {
        break;
      }
    }
  }

  if (! isbdj4) {
    fprintf (stderr, "updater: not started with launcher\n");
    exit (1);
  }

  for (int i = 0; i < UPD_MAX; ++i) {
    processflags [i] = 0;
  }

  flags = bdj4startup (argc, argv, NULL, "up", ROUTE_NONE,
      BDJ4_INIT_NO_LOCK | BDJ4_INIT_NO_DB_LOAD);

  audiotagInit ();

  logStartAppend ("bdj4updater", "up",
      LOG_IMPORTANT | LOG_BASIC | LOG_MAIN | LOG_REDIR_INST);
  logMsg (LOG_INSTALL, LOG_IMPORTANT, "=== updater started");

  pathbldMakePath (tbuff, sizeof (tbuff),
      "updater", BDJ4_CONFIG_EXT, PATHBLD_MP_DATA);
  df = datafileAllocParse ("updater", DFTYPE_KEY_VAL, tbuff,
      upddfkeys, UPD_DF_COUNT);
  updlist = datafileGetList (df);

  tval = nlistGetStr (updlist, UPD_FIRST_VERS);
  if (tval == NULL || ! *tval) {
    logMsg (LOG_INSTALL, LOG_IMPORTANT, "first installation");
    nlistSetStr (updlist, UPD_FIRST_VERS, sysvarsGetStr (SV_BDJ4_VERSION));
    nlistSetNum (updlist, UPD_CONVERTED, converted);
  } else {
    converted = nlistGetNum (updlist, UPD_CONVERTED);
  }

  /* always figure out where the home music dir is */
  /* this is used on new intalls to set the music dir */
  /* also needed to check for the itunes dir every time */
  home = sysvarsGetStr (SV_HOME);
  if (isLinux ()) {
    const char  *targv [5];
    int         targc = 0;
    char        data [200];
    char        *prog;

    prog = sysvarsGetStr (SV_PATH_XDGUSERDIR);
    if (*prog) {
      *data = '\0';
      targc = 0;
      targv [targc++] = prog;
      targv [targc++] = "MUSIC";
      targv [targc++] = NULL;
      osProcessPipe (targv, OS_PROC_DETACH, data, sizeof (data));
      stringTrim (data);
      stringTrimChar (data, '/');

      /* xdg-user-dir returns the home folder if the music dir does */
      /* not exist */
      if (*data && strcmp (data, home) != 0) {
        strlcpy (homemusicdir, data, sizeof (homemusicdir));
      } else {
        snprintf (homemusicdir, sizeof (homemusicdir), "%s/Music", home);
      }
    }
  }
  if (isWindows ()) {
    char    *data;

    snprintf (homemusicdir, sizeof (homemusicdir), "%s/Music", home);
    data = osRegistryGet (
        "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders",
        "My Music");
    if (data != NULL && *data) {
      /* windows returns the path with %USERPROFILE% */
      strlcpy (homemusicdir, home, sizeof (homemusicdir));
      strlcat (homemusicdir, data + 13, sizeof (homemusicdir));
    } else {
      data = osRegistryGet (
          "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
          "My Music");
      if (data != NULL && *data) {
        /* windows returns the path with %USERPROFILE% */
        strlcpy (homemusicdir, home, sizeof (homemusicdir));
        strlcat (homemusicdir, data + 13, sizeof (homemusicdir));
      }
    }
  }
  if (isMacOS ()) {
    snprintf (homemusicdir, sizeof (homemusicdir), "%s/Music", home);
  }
  pathNormPath (homemusicdir, sizeof (homemusicdir));

  if (newinstall) {
    logMsg (LOG_INSTALL, LOG_IMPORTANT, "new install or re-install");
    tval = bdjoptGetStr (OPT_M_VOLUME_INTFC);
    if (tval == NULL || ! *tval || strcmp (tval, "libvolnull") == 0) {
      if (isWindows ()) {
        bdjoptSetStr (OPT_M_VOLUME_INTFC, "libvolwin");
      }
      if (isMacOS ()) {
        bdjoptSetStr (OPT_M_VOLUME_INTFC, "libvolmac");
      }
      if (isLinux ()) {
        bdjoptSetStr (OPT_M_VOLUME_INTFC, "libvolpa");
      }
      bdjoptchanged = true;
    }

    tval = bdjoptGetStr (OPT_M_STARTUPSCRIPT);
    if (isLinux () && (tval == NULL || ! *tval)) {
      pathbldMakePath (tbuff, sizeof (tbuff),
          "scripts/linux/bdjstartup", ".sh", PATHBLD_MP_MAINDIR);
      bdjoptSetStr (OPT_M_STARTUPSCRIPT, tbuff);
      pathbldMakePath (tbuff, sizeof (tbuff),
          "scripts/linux/bdjshutdown", ".sh", PATHBLD_MP_MAINDIR);
      bdjoptSetStr (OPT_M_SHUTDOWNSCRIPT, tbuff);
      bdjoptchanged = true;
    }

    tval = bdjoptGetStr (OPT_M_DIR_MUSIC);
    if (tval == NULL || ! *tval) {
      bdjoptSetStr (OPT_M_DIR_MUSIC, homemusicdir);
      bdjoptchanged = true;
    }
  }

  /* always check and see if itunes exists, unless a conversion was run */
  /* support for old versions of itunes is minimal */

  if (! converted) {
    tval = bdjoptGetStr (OPT_M_DIR_ITUNES_MEDIA);
    if (tval == NULL || ! *tval) {
      snprintf (tbuff, sizeof (tbuff), "%s/%s/%s",
          homemusicdir, ITUNES_NAME, ITUNES_MEDIA_NAME);
      if (fileopIsDirectory (tbuff)) {
        bdjoptSetStr (OPT_M_DIR_ITUNES_MEDIA, tbuff);
        bdjoptchanged = true;
        ++haveitunes;
      }
      snprintf (tbuff, sizeof (tbuff), "%s/%s/%s",
          homemusicdir, ITUNES_NAME, ITUNES_XML_NAME);
      if (fileopFileExists (tbuff)) {
        bdjoptSetStr (OPT_M_ITUNES_XML_FILE, tbuff);
        bdjoptchanged = true;
        ++haveitunes;
      } else {
        /* this is an ancient itunes name, no idea if it needs to be supported */
        snprintf (tbuff, sizeof (tbuff), "%s/%s/%s",
            homemusicdir, ITUNES_NAME, "iTunes Library.xml");
        if (fileopFileExists (tbuff)) {
          bdjoptSetStr (OPT_M_ITUNES_XML_FILE, tbuff);
          bdjoptchanged = true;
          ++haveitunes;
        }
      }
    } // if itunes dir is not set
  } // if not converted

  /* a new installation, and the itunes folder and xml file */
  /* have been found. */
  if (newinstall && haveitunes == 2) {
    logMsg (LOG_INSTALL, LOG_IMPORTANT, "have itunes");
    /* set the music dir to the itunes media folder */
    bdjoptSetStr (OPT_M_DIR_MUSIC, bdjoptGetStr (OPT_M_DIR_ITUNES_MEDIA));
    /* set the organization path to the itunes standard */
    /* album-artist / album / disc-tracknum0 title */
    bdjoptSetStr (OPT_G_AO_PATHFMT,
        "{%ALBUMARTIST%/}{%ALBUM%/}{%DISC%-}{%TRACKNUMBER0% }{%TITLE%}");
    bdjoptchanged = true;
  }

  updaterCleanFiles ();

  /* audio file conversions */

  /* fix musicbrainz tags check */

  value = updateGetStatus (updlist, UPD_FIX_AF_MB_TAG);
  if (! converted && value == UPD_NOT_DONE) {
    /* if a conversion was not done */
    value = UPD_SKIP;
    nlistSetNum (updlist, UPD_FIX_AF_MB_TAG, value);
  }
  statusflags [UPD_FIX_AF_MB_TAG] = value;
  processflags [UPD_FIX_AF_MB_TAG] =
      converted &&
      value == UPD_NOT_DONE &&
      strcmp (sysvarsGetStr (SV_BDJ4_RELEASELEVEL), "alpha") != 0;
  if (processflags [UPD_FIX_AF_MB_TAG]) { processaf = true; }

  if (processaf || processdb) {
    pathbldMakePath (tbuff, sizeof (tbuff),
        MUSICDB_FNAME, MUSICDB_EXT, PATHBLD_MP_DATA);
    musicdb = dbOpen (tbuff);
  }

  if (processaf) {
    char        *ffn;
    slistidx_t  dbiteridx;
    song_t      *song;
    dbidx_t     dbidx;
    bool        process;
    char        *data;
    slist_t     *taglist;
    int         rewrite;

    logMsg (LOG_INSTALL, LOG_IMPORTANT, "processing audio files");
    dbStartIterator (musicdb, &dbiteridx);
    while ((song = dbIterate (musicdb, &dbidx, &dbiteridx)) != NULL) {
      ffn = songFullFileName (songGetStr (song, TAG_FILE));
      process = false;

      if (processflags [UPD_FIX_AF_MB_TAG]) {
        pathinfo_t    *pi;

        pi = pathInfo (ffn);
        if (pathInfoExtCheck (pi, ".mp3") ||
            pathInfoExtCheck (pi, ".flac") ||
            pathInfoExtCheck (pi, ".ogg")) {
          process = true;
        }
        pathInfoFree (pi);
      }

      if (! process) {
        free (ffn);
        continue;
      }

      data = audiotagReadTags (ffn);
      taglist = audiotagParseData (ffn, data, &rewrite);

      if (processflags [UPD_FIX_AF_MB_TAG] && rewrite) {
        logMsg (LOG_INSTALL, LOG_IMPORTANT, "fix mb: %d %s", dbidx, ffn);
        audiotagWriteTags (ffn, taglist, taglist, rewrite, AT_KEEP_MOD_TIME);
      }

      free (data);
      slistFree (taglist);
      free (ffn);
    }
  }

  if (processdb) {
    slistidx_t  dbiteridx;
    song_t      *song;
    dbidx_t     dbidx;

    dbStartBatch (musicdb);
    logMsg (LOG_INSTALL, LOG_IMPORTANT, "processing database");
    dbStartIterator (musicdb, &dbiteridx);
    while ((song = dbIterate (musicdb, &dbidx, &dbiteridx)) != NULL) {
      ;
    }
    dbEndBatch (musicdb);
  }

  if (processaf || processdb) {
    if (processflags [UPD_FIX_AF_MB_TAG]) {
      nlistSetNum (updlist, UPD_FIX_AF_MB_TAG, UPD_COMPLETE);
    }

    if (processdb) {
      // ### save the database
    }
    dbClose (musicdb);
  }

  if (bdjoptchanged) {
    bdjoptSave ();
  }

  pathbldMakePath (tbuff, sizeof (tbuff),
      "updater", BDJ4_CONFIG_EXT, PATHBLD_MP_DATA);
  datafileSaveKeyVal ("updater", tbuff, upddfkeys, UPD_DF_COUNT, updlist);

  bdj4shutdown (ROUTE_NONE, NULL);
  bdjoptCleanup ();

  return 0;
}

static void
updaterCleanFiles (void)
{
  FILE    *fh;
  char    dfn [MAXPATHLEN];
  char    fname [MAXPATHLEN];
  char    tfn [MAXPATHLEN];
  char    *basedir;

  pathbldMakePath (fname, sizeof (fname),
      "cleanuplist", BDJ4_CONFIG_EXT, PATHBLD_MP_INSTDIR);
  basedir = sysvarsGetStr (SV_BDJ4MAINDIR);

  fh = fileopOpen (fname, "r");
  if (fh != NULL) {
    while (fgets (dfn, sizeof (dfn), fh) != NULL) {
      if (*dfn == '#') {
        continue;
      }

      stringTrim (dfn);
      stringTrimChar (dfn, '/');

      if (*dfn == '\0') {
        continue;
      }

      if (strcmp (dfn, ":main") == 0) {
        basedir = sysvarsGetStr (SV_BDJ4MAINDIR);
        continue;
      }
      if (strcmp (dfn, ":data") == 0) {
        basedir = sysvarsGetStr (SV_BDJ4DATATOPDIR);
        continue;
      }

      if (strstr (dfn, "*") != NULL ||
          strstr (dfn, "$") != NULL ||
          strstr (dfn, "[") != NULL) {
        updaterCleanRegex (basedir, dfn);
        continue;
      }

      strlcpy (tfn, basedir, sizeof (tfn));
      strlcat (tfn, dfn, sizeof (tfn));

      if (fileopIsDirectory (tfn)) {
        diropDeleteDir (tfn);
      } else {
        if (fileopFileExists (tfn)) {
          fileopDelete (tfn);
        }
      }
    }
    fclose (fh);
  }
}

static void
updaterCleanRegex (const char *basedir, const char *pattern)
{
  char        tdir [MAXPATHLEN];
  char        tmp [MAXPATHLEN];
  slist_t     *filelist;
  slistidx_t  iteridx;
  char        *fn;
  size_t      len;
  int         dlen;
  bdjregex_t  *rx;

  strlcpy (tdir, basedir, sizeof (tdir));

  dlen = 0;
  len = strlen (pattern);
  for (ssize_t i = len - 1; i >= 0; --i) {
    if (pattern [i] == '/') {
      dlen = i;
    }
  }

  if (dlen > 0) {
    memcpy (tmp, pattern, dlen);
    tmp [dlen] = '\0';
    strlcat (tdir, "/", sizeof (tdir));
    strlcat (tdir, tmp, sizeof (tdir));
  }

  if (dlen > 0) { ++dlen; }
  rx = regexInit (pattern + dlen);

  filelist = dirlistBasicDirList (tdir, NULL);
  slistStartIterator (filelist, &iteridx);
  while ((fn = slistIterateKey (filelist, &iteridx)) != NULL) {
    if (regexMatch (rx, fn)) {
      strlcpy (tmp, tdir, sizeof (tmp));
      strlcat (tmp, "/", sizeof (tmp));
      strlcat (tmp, fn, sizeof (tmp));
      fileopDelete (tmp);
    }
  }
  nlistFree (filelist);

  regexFree (rx);
}

static int
updateGetStatus (nlist_t *updlist, int key)
{
  int   value;

  value = nlistGetNum (updlist, key);
  if (value < 0) {
    value = UPD_NOT_DONE;
    nlistSetNum (updlist, key, value);
  }

  return value;
}
