#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "lock.h"
#include "log.h"
#include "pathbld.h"
#include "rafile.h"
#include "tmutil.h"

typedef struct rafile {
  FILE          *fh;
  char          *fname;
  int           version;
  rafileidx_t   size;
  rafileidx_t   count;
  unsigned int  inbatch : 1;
  unsigned int  locked : 1;
} rafile_t;

static int  raReadHeader (rafile_t *);
static void raWriteHeader (rafile_t *, int);
static void raLock (rafile_t *);
static void raUnlock (rafile_t *);

static char ranulls [RAFILE_REC_SIZE];

rafile_t *
raOpen (char *fname, int version)
{
  struct stat     statbuf;
  rafile_t        *rafile;
  int             frc;
  int             rc;
  char            *mode;

  logProcBegin (LOG_PROC, "raOpen");
  rafile = malloc (sizeof (rafile_t));
  assert (rafile != NULL);
  rafile->fh = NULL;

  frc = stat (fname, &statbuf);
  mode = "rb+";
  if (frc != 0) {
    mode = "wb+";
  }

  rafile->fh = fopen (fname, mode);
  rafile->inbatch = 0;
  rafile->locked = 0;
  rafile->version = version;
  rafile->size = RAFILE_REC_SIZE;

  if (statbuf.st_size == 0L) {
    frc = 1;
  }
  if (frc == 0) {
    rc = raReadHeader (rafile);

    if (rc != 0) {
      /* probably an incorrect filename   */
      /* don't try to do anything with it */
      free (rafile);
      logProcEnd (LOG_PROC, "raOpen", "bad-header");
      return NULL;
    }
  }
  if (frc != 0) {
    rafile->count = 0L;
    raLock (rafile);
    raWriteHeader (rafile, version);
    raUnlock (rafile);
  }
  rafile->fname = strdup (fname);
  assert (rafile->fname != NULL);
  memset (ranulls, 0, RAFILE_REC_SIZE);
  logProcEnd (LOG_PROC, "raOpen", "");
  return rafile;
}

void
raClose (rafile_t *rafile)
{
  logProcBegin (LOG_PROC, "raClose");
  if (rafile != NULL) {
    fclose (rafile->fh);
    raUnlock (rafile);
    rafile->fh = NULL;
    if (rafile->fname != NULL) {
      free (rafile->fname);
    }
    free (rafile);
  }
  logProcEnd (LOG_PROC, "raClose", "");
}

inline rafileidx_t
raGetCount (rafile_t *rafile)
{
  return rafile->count;
}

rafileidx_t
raGetNextRRN (rafile_t *rafile)
{
  raLock (rafile);
  return (rafile->count + 1L);
}

void
raStartBatch (rafile_t *rafile)
{
  logProcBegin (LOG_PROC, "raStartBatch");
  raLock (rafile);
  rafile->inbatch = 1;
  logProcEnd (LOG_PROC, "raStartBatch", "");
}

void
raEndBatch (rafile_t *rafile)
{
  logProcBegin (LOG_PROC, "raEndBatch");
  rafile->inbatch = 0;
  raUnlock (rafile);
  logProcEnd (LOG_PROC, "raEndBatch", "");
}

int
raWrite (rafile_t *rafile, rafileidx_t rrn, char *data)
{
  size_t  len = strlen (data);
  bool    isnew = false;

  logProcBegin (LOG_PROC, "raWrite");
  /* leave one byte for the terminating null */
  if (len > (RAFILE_REC_SIZE - 1)) {
    logMsg (LOG_DBG, LOG_RAFILE, "bad data len", "%ld", len);
    return 1;
  }

  raLock (rafile);
  if (rrn == RAFILE_NEW) {
    isnew = true;
    ++rafile->count;
    rrn = rafile->count;
    raWriteHeader (rafile, rafile->version);
  } else {
    if (rrn > rafile->count) {
      rafile->count = rrn;
      raWriteHeader (rafile, rafile->version);
    }
  }
  if (! isnew) {
    fseek (rafile->fh, RRN_TO_OFFSET (rrn), SEEK_SET);
    fwrite (ranulls, RAFILE_REC_SIZE, 1, rafile->fh);
  }
  fseek (rafile->fh, RRN_TO_OFFSET (rrn), SEEK_SET);
  fwrite (data, len, 1, rafile->fh);
  if (isnew) {
    /* write one null byte to the next record so */
    /* that the last record has a size and is readable */
    fseek (rafile->fh, RRN_TO_OFFSET (rrn + 1), SEEK_SET);
    fwrite (ranulls, 1, 1, rafile->fh);
  }
  fflush (rafile->fh);
  raUnlock (rafile);
  logProcEnd (LOG_PROC, "raWrite", "");
  return 0;
}

int
raClear (rafile_t *rafile, rafileidx_t rrn)
{
  logProcBegin (LOG_PROC, "raClear");
  if (rrn < 1L || rrn > rafile->count) {
    logMsg (LOG_DBG, LOG_RAFILE, "bad rrn", "%ld", rrn);
    return 1;
  }
  raLock (rafile);
  fseek (rafile->fh, RRN_TO_OFFSET (rrn), SEEK_SET);
  fwrite (ranulls, RAFILE_REC_SIZE, 1, rafile->fh);
  raUnlock (rafile);
  logProcEnd (LOG_PROC, "raClear", "");
  return 0;
}

rafileidx_t
raRead (rafile_t *rafile, rafileidx_t rrn, char *data)
{
  rafileidx_t        rc;

  logProcBegin (LOG_PROC, "raRead");
  if (rrn < 1L || rrn > rafile->count) {
    logMsg (LOG_DBG, LOG_RAFILE, "bad rrn", "%ld", rrn);
    return 0;
  }

  raLock (rafile);
  fseek (rafile->fh, RRN_TO_OFFSET (rrn), SEEK_SET);
  rc = (rafileidx_t) fread (data, RAFILE_REC_SIZE, 1, rafile->fh);
  raUnlock (rafile);
  logProcEnd (LOG_PROC, "raRead", "");
  return rc;
}

/* local routines */

static int
raReadHeader (rafile_t *rafile)
{
  char      buff [90];
  char      *rv;
  int       rrc;
  int       version;
  int       rasize;
  rafileidx_t    count;

  logProcBegin (LOG_PROC, "raReadHeader");
  raLock (rafile);
  rrc = 1;
  fseek (rafile->fh, 0L, SEEK_SET);
  rv = fgets (buff, sizeof (buff) - 1, rafile->fh);
  if (rv != NULL && sscanf (buff, "#VERSION=%d\n", &version) == 1) {
    rafile->version = version;
    rv = fgets (buff, sizeof (buff) - 1, rafile->fh);
    if (rv != NULL) {
      rv = fgets (buff, sizeof (buff) - 1, rafile->fh);
      if (rv != NULL && sscanf (buff, "#RASIZE=%d\n", &rasize) == 1) {
        rafile->size = rasize;
        rv = fgets (buff, sizeof (buff) - 1, rafile->fh);
        if (rv != NULL && sscanf (buff, "#RACOUNT=%d\n", &count) == 1) {
          rafile->count = count;
          rrc = 0;
        }
      }
    }
  }
  raUnlock (rafile);
  logProcEnd (LOG_PROC, "raReadHeader", "");
  return rrc;
}

static void
raWriteHeader (rafile_t *rafile, int version)
{
  logProcBegin (LOG_PROC, "raWriteHeader");
  /* locks are handled by the caller */
  /* the header never gets smaller, so it's not necessary to flush its data */
  fseek (rafile->fh, 0L, SEEK_SET);
  fprintf (rafile->fh, "#VERSION=%d\n", version);
  fprintf (rafile->fh, "# Do not edit this file.\n");
  fprintf (rafile->fh, "#RASIZE=%d\n", rafile->size);
  fprintf (rafile->fh, "#RACOUNT=%d\n", rafile->count);
  fflush (rafile->fh);
  logProcEnd (LOG_PROC, "raWriteHeader", "");
}

static void
raLock (rafile_t *rafile)
{
  int     rc;
  int     count;

  logProcBegin (LOG_PROC, "raLock");
  if (rafile->inbatch) {
    logProcEnd (LOG_PROC, "raLock", "is-in-batch");
    return;
  }

  /* the music database may be shared across multiple processes */
  rc = lockAcquire (RAFILE_LOCK_FN, PATHBLD_MP_TMPDIR);
  count = 0;
  while (rc < 0 && count < 10) {
    mssleep (50);
    rc = lockAcquire (RAFILE_LOCK_FN, PATHBLD_MP_TMPDIR);
    ++count;
  }
  if (rc < 0 && count >= 10) {
    /* ### FIX */
    /* global failure; stop everything */
  }
  rafile->locked = 1;
  logProcEnd (LOG_PROC, "raLock", "");
}

static void
raUnlock (rafile_t *rafile)
{
  logProcBegin (LOG_PROC, "raUnlock");
  if (rafile->inbatch) {
    logProcEnd (LOG_PROC, "raUnlock", "is-in-batch");
    return;
  }

  lockRelease (RAFILE_LOCK_FN, PATHBLD_MP_TMPDIR);
  rafile->locked = 0;
  logProcEnd (LOG_PROC, "raUnlock", "");
}

/* for debugging only */

inline rafileidx_t
raGetSize (rafile_t *rafile)
{
  return rafile->size;
}

inline rafileidx_t
raGetVersion (rafile_t *rafile)
{
  return rafile->version;
}
