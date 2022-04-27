#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <check.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#include "bdj4.h"
#include "check_bdj.h"
#include "osutils.h"
#include "pathbld.h"
#include "procutil.h"
#include "sysvars.h"
#include "tmutil.h"

START_TEST(procutil_exists)
{
  pid_t     pid;
  int       rc;
  procutil_t process;

  pid = getpid ();
  process.pid = pid;
  process.hasHandle = false;
  rc = procutilExists (&process);
  ck_assert_int_eq (rc, 0);
  process.pid = 90876;
  process.hasHandle = false;
  rc = procutilExists (&process);
  ck_assert_int_lt (rc, 0);
}
END_TEST

START_TEST(procutil_start)
{
  pid_t     ppid;
  int       rc;
  char      *extension;
  char      tbuff [MAXPATHLEN];
  procutil_t *process;

  ppid = getpid ();

  extension = "";
  if (isWindows ()) {
    extension = ".exe";
  }
  pathbldMakePath (tbuff, sizeof (tbuff),
      "chkprocess", extension, PATHBLD_MP_EXECDIR);
  /* if the signal is not ignored, the process goes into a zombie state */
  /* and still exists */
#if _define_SIGCHLD
  osIgnoreSignal (SIGCHLD);
#endif
  /* abuse the --profile argument to pass the seconds to sleep */
  process = procutilStart (tbuff, 5, 0, PROCUTIL_NO_DETACH, NULL);
  ck_assert_ptr_nonnull (process);
  ck_assert_int_ne (ppid, process->pid);
  rc = procutilExists (process);
  ck_assert_int_eq (rc, 0);
  mssleep (6000);
  rc = procutilExists (process);
  ck_assert_int_ne (rc, 0);
  procutilFree (process);
}
END_TEST

START_TEST(procutil_kill)
{
  pid_t     ppid;
  int       rc;
  char      *extension;
  char      tbuff [MAXPATHLEN];
  procutil_t *process;

  ppid = getpid ();

  extension = "";
  if (isWindows ()) {
    extension = ".exe";
  }
  pathbldMakePath (tbuff, sizeof (tbuff),
      "chkprocess", extension, PATHBLD_MP_EXECDIR);
  /* if the signal is not ignored, the process goes into a zombie state */
  /* and still exists */
#if _define_SIGCHLD
  osIgnoreSignal (SIGCHLD);
#endif
  /* abuse the --profile argument to pass the seconds to sleep */
  process = procutilStart (tbuff, 60, 0, PROCUTIL_NO_DETACH, NULL);
  ck_assert_ptr_nonnull (process);
  ck_assert_int_ne (ppid, process->pid);

  rc = procutilExists (process);
  ck_assert_int_eq (rc, 0);

  mssleep (2000);
  rc = procutilExists (process);
  ck_assert_int_eq (rc, 0);

  mssleep (2000);
  rc = procutilExists (process);
  ck_assert_int_eq (rc, 0);

  rc = procutilKill (process, false);
  ck_assert_int_eq (rc, 0);

  mssleep (2000);
  rc = procutilExists (process);
  ck_assert_int_ne (rc, 0);

  procutilFree (process);
}
END_TEST

Suite *
procutil_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("procutil Suite");
  tc = tcase_create ("procutil");
  tcase_add_test (tc, procutil_exists);
  tcase_add_test (tc, procutil_start);
  tcase_add_test (tc, procutil_kill);
  tcase_set_timeout (tc, 10.0);
  suite_add_tcase (s, tc);
  return s;
}

