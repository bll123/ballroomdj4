#include "bdjconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "rafile.h"

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


  rafile = malloc (sizeof (rafile_t));
  assert (rafile != NULL);
  rafile->fh = NULL;

  frc = stat (fname, &statbuf);
  mode = "r+";
  if (frc != 0) {
    mode = "w+";
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
  return rafile;
}

void
raClose (rafile_t *rafile)
{
  fclose (rafile->fh);
  raUnlock (rafile);
  rafile->fh = NULL;
  free (rafile->fname);
  free (rafile);
}

size_t
raGetCount (rafile_t *rafile)
{
  return rafile->count;
}

size_t
raGetNextRRN (rafile_t *rafile)
{
  raLock (rafile);
  return (rafile->count + 1L);
}

void
raStartBatch (rafile_t *rafile)
{
  raLock (rafile);
  rafile->inbatch = 1;
}

void
raEndBatch (rafile_t *rafile)
{
  rafile->inbatch = 0;
  raUnlock (rafile);
}

int
raWrite (rafile_t *rafile, size_t rrn, char *data)
{
  size_t    rc;

  size_t len = strlen (data);
  /* leave one byte for the terminating null */
  if (len > (RAFILE_REC_SIZE - 1)) {
    return 1;
  }

  raLock (rafile);
  if (rrn == RAFILE_NEW) {
    ++rafile->count;
    rrn = rafile->count;
    raWriteHeader (rafile, rafile->version);
  } else {
    if (rrn > rafile->count) {
      rafile->count = rrn;
      raWriteHeader (rafile, rafile->version);
    }
  }
  fseek (rafile->fh, RRN_TO_OFFSET (rrn), SEEK_SET);
  rc = fwrite (ranulls, RAFILE_REC_SIZE, 1, rafile->fh);
  fseek (rafile->fh, RRN_TO_OFFSET (rrn), SEEK_SET);
  rc = fwrite (data, len, 1, rafile->fh);
  fflush (rafile->fh);
  raUnlock (rafile);
  return 0;
}

int
raClear (rafile_t *rafile, size_t rrn)
{
  if (rrn < 1L || rrn > rafile->count) {
    return 1;
  }
  raLock (rafile);
  fseek (rafile->fh, RRN_TO_OFFSET (rrn), SEEK_SET);
  fwrite (ranulls, RAFILE_REC_SIZE, 1, rafile->fh);
  raUnlock (rafile);
  return 0;
}

size_t
raRead (rafile_t *rafile, size_t rrn, char *data)
{
  size_t        rc;

  if (rrn < 1L || rrn > rafile->count) {
    return 0;
  }

  raLock (rafile);
  fseek (rafile->fh, RRN_TO_OFFSET (rrn), SEEK_SET);
  rc = fread (data, RAFILE_REC_SIZE, 1, rafile->fh);
  raUnlock (rafile);
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
  size_t    count;

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
        if (rv != NULL && sscanf (buff, "#RACOUNT=%lu\n", &count) == 1) {
          rafile->count = count;
          rrc = 0;
        }
      }
    }
  }
  raUnlock (rafile);
  return rrc;
}

static void
raWriteHeader (rafile_t *rafile, int version)
{
  /* locks are handled by the caller */
  /* the header never gets smaller, so it's not necessary to flush its data */
  fseek (rafile->fh, 0L, SEEK_SET);
  fprintf (rafile->fh, "#VERSION=%d\n", version);
  fprintf (rafile->fh, "# Do not edit this file.\n");
  fprintf (rafile->fh, "#RASIZE=%d\n", rafile->size);
  fprintf (rafile->fh, "#RACOUNT=%ld\n", rafile->count);
  fflush (rafile->fh);
}

/* there should only ever be one process accessing  */
/* the file, so this is not using a robust method   */
/* this can be replaced later                       */
static void
raLock (rafile_t *rafile)
{
  if (rafile->inbatch) {
    return;
  }

  size_t count = 0;
  while (rafile->locked && count < 20) {
    usleep (50000L);
    ++count;
  }
  rafile->locked = 1;
}

static void
raUnlock (rafile_t *rafile)
{
  if (rafile->inbatch) {
    return;
  }

  rafile->locked = 0;
}
