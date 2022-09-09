#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma gcc diagnostic push
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma gcc diagnostic ignored "-Wformat-extra-args"

#include <check.h>

#include "bdj4.h"
#include "bdjstring.h"
#include "fileop.h"
#include "log.h"
#include "osprocess.h"
#include "check_bdj.h"
/* procutil hasn't had its tests run, but need the procutilExists routine */
#include "procutil.h"
#include "sysvars.h"

static void
runchk (int flags, int *rpid, int *rexists)
{
  char        tbuff [MAXPATHLEN];
  const char  *targv [10];
  int         targc = 0;
  int         pid = 0;
  procutil_t  process;
  char        *extension;
  int         rc;

  extension = "";
  if (isWindows ()) {
    extension = ".exe";
  }
  snprintf (tbuff, sizeof (tbuff), "bin/chkprocess%s", extension);

  targv [targc++] = tbuff;
  targv [targc++] = "--profile";
  targv [targc++] = "1";          // time to sleep
  targv [targc++] = "--bdj4";
  targv [targc++] = NULL;
  pid = osProcessStart (targv, flags, NULL, NULL);
  process.pid = pid;
  process.hasHandle = false;
  rc = procutilExists (&process);
  *rpid = pid;
  *rexists = rc;
}

START_TEST(osprocess_start)
{
  int   pid;
  int   exists;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- osprocess_start");

  runchk (0, &pid, &exists);
  ck_assert_int_gt (pid, 0);
  ck_assert_int_eq (exists, 0);
}
END_TEST

START_TEST(osprocess_start_detach)
{
  int   pid;
  int   exists;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- osprocess_start_detach");

  runchk (OS_PROC_DETACH, &pid, &exists);
  ck_assert_int_gt (pid, 0);
  ck_assert_int_eq (exists, 0);
}
END_TEST

START_TEST(osprocess_start_wait)
{
  int   pid;
  int   exists;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- osprocess_start_wait");

  runchk (OS_PROC_WAIT, &pid, &exists);
  /* in this case, the pid is the return code from the program */
  ck_assert_int_eq (pid, 0);
}
END_TEST

START_TEST(osprocess_start_handle)
{
  char        tbuff [MAXPATHLEN];
  const char  *targv [10];
  int         targc = 0;
  int         pid = 0;
  procutil_t  process;
  char        *extension;
  int         rc;
  int         flags = 0;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- osprocess_start_handle");

  extension = "";
  if (isWindows ()) {
    extension = ".exe";
  }
  snprintf (tbuff, sizeof (tbuff), "bin/chkprocess%s", extension);

  targv [targc++] = tbuff;
  targv [targc++] = "--profile";
  targv [targc++] = "1";          // time to sleep
  targv [targc++] = "--bdj4";
  targv [targc++] = NULL;
  process.processHandle = NULL;
  pid = osProcessStart (targv, flags, &process.processHandle, NULL);
  ck_assert_int_gt (pid, 0);
  if (process.processHandle != NULL) {
    process.hasHandle = true;
  }
  if (isWindows ()) {
    ck_assert_ptr_nonnull (process.processHandle);
  }
  process.pid = pid;
  rc = procutilExists (&process);
  ck_assert_int_eq (rc, 0);
#if _typ_HANDLE
  if (process.processHandle != NULL) {
    CloseHandle (process.processHandle);
  }
#endif
}
END_TEST

START_TEST(osprocess_start_redirect)
{
  char        tbuff [MAXPATHLEN];
  const char  *targv [10];
  int         targc = 0;
  int         pid = 0;
  char        *extension;
  int         flags = 0;
  char        *outfn = "tmp/chkosprocess.txt";
  FILE        *fh;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- osprocess_start_redirect");

  extension = "";
  if (isWindows ()) {
    extension = ".exe";
  }
  snprintf (tbuff, sizeof (tbuff), "bin/chkprocess%s", extension);

  targv [targc++] = tbuff;
  targv [targc++] = "--profile";
  targv [targc++] = "0";          // time to sleep
  targv [targc++] = "--theme";
  targv [targc++] = "xyzzy";          // data to write
  targv [targc++] = "--bdj4";
  targv [targc++] = NULL;
  flags = OS_PROC_WAIT;
  pid = osProcessStart (targv, flags, NULL, outfn);
  /* waited, pid is the return code */
  ck_assert_int_eq (pid, 0);
  if (isWindows ()) {
    sleep (1);    // give windows a bit more time
  }
  ck_assert_int_eq (fileopFileExists (outfn), 1);
  fh = fopen (outfn, "r");
  if (fh != NULL) {
    fgets (tbuff, sizeof (tbuff), fh);
    fclose (fh);
  }
  stringTrim (tbuff);
  ck_assert_str_eq (tbuff, "xyzzy");
}
END_TEST

START_TEST(osprocess_pipe)
{
  char        tbuff [MAXPATHLEN];
  const char  *targv [10];
  int         targc = 0;
  int         pid = 0;
  procutil_t  process;
  char        *extension;
  int         flags = 0;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- osprocess_pipe");

  extension = "";
  if (isWindows ()) {
    extension = ".exe";
  }
  snprintf (tbuff, sizeof (tbuff), "bin/chkprocess%s", extension);

  targv [targc++] = tbuff;
  targv [targc++] = "--profile";
  targv [targc++] = "0";          // time to sleep
  targv [targc++] = "--theme";
  targv [targc++] = "xyzzy";          // data to write
  targv [targc++] = "--bdj4";
  targv [targc++] = NULL;
  flags = 0;
  pid = osProcessPipe (targv, flags, tbuff, sizeof (tbuff));
  process.pid = pid;
  ck_assert_int_gt (pid, 0);
  stringTrim (tbuff);
  ck_assert_str_eq (tbuff, "xyzzy");
}
END_TEST

START_TEST(osprocess_run)
{
  char        tbuff [MAXPATHLEN];
  char        *extension;
  int         flags = 0;
  char        *data;

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- osprocess_run");

  extension = "";
  if (isWindows ()) {
    extension = ".exe";
  }
  snprintf (tbuff, sizeof (tbuff), "bin/chkprocess%s", extension);

  flags = 0;
  data = osRunProgram (tbuff, "--profile", "0", "--theme", "xyzzy", "--bdj4", NULL);
  stringTrim (data);
  ck_assert_str_eq (data, "xyzzy");
  free (data);
}
END_TEST


Suite *
osprocess_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("osprocess");
  tc = tcase_create ("osprocess");
  tcase_set_tags (tc, "libcommon slow");
  tcase_add_test (tc, osprocess_start);
  tcase_add_test (tc, osprocess_start_detach);
  tcase_add_test (tc, osprocess_start_wait);
  tcase_add_test (tc, osprocess_start_handle);
  tcase_add_test (tc, osprocess_start_redirect);
  tcase_add_test (tc, osprocess_pipe);
  tcase_add_test (tc, osprocess_run);
  suite_add_tcase (s, tc);
  return s;
}
