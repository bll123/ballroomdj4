#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <check.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#if _hdr_arpa_inet
# include <arpa/inet.h>
#endif
#if _hdr_fcntl
# include <fcntl.h>
#endif
#if _hdr_netdb
# include <netdb.h>
#endif
#if _hdr_netinet_in
# include <netinet/in.h>
#endif
#if _sys_select
# include <sys/select.h>
#endif
#if _sys_socket
# include <sys/socket.h>
#endif
#if _hdr_unistd
# include <unistd.h>
#endif
/* winsock2.h should come before windows.h */
#if _hdr_winsock2
# include <winsock2.h>
#endif
#if _hdr_windows
# include <windows.h>
#endif

#if _hdr_pthread
# include <pthread.h>
#endif

#include "sock.h"
#include "check_bdj.h"
#include "tmutil.h"
#include "log.h"
#include "tmutil.h"

static Sock_t connectWait (void);
static void * connectClose (void *id);
static void * connectCloseEarly (void *id);
static void * connectWrite (void *id);
static void * connectWriteClose (void *id);
static void * connectWriteCloseFail (void *id);

#if _lib_pthread_create
static pthread_t   thread;
#endif
static short        gport;
static int          gthreadrc;
static Sock_t       gclsock;

START_TEST(sock_server_create)
{
  int         err;

  logMsg (LOG_DBG, "=== sock_server_create");
  Sock_t s = sockServer (32700, &err);
  ck_assert_int_gt (s, 2);
  ck_assert_int_eq (socketInvalid (s), 0);
  sockClose (s);
}
END_TEST

START_TEST(sock_server_create_check)
{
  sockinfo_t    *si;
  int         err;

  logMsg (LOG_DBG, "=== sock_server_create_check");
  si = NULL;
  Sock_t s = sockServer (32701, &err);
  ck_assert_int_gt (s, 2);
  ck_assert_int_eq (socketInvalid (s), 0);
  si = sockAddCheck (si, s);
  ck_assert_int_eq (si->count, 1);
  ck_assert_int_eq (si->socklist[0], s);
  sockRemoveCheck (si, s);
  sockFreeCheck (si);
  sockClose (s);
}
END_TEST

START_TEST(sock_server_check)
{
  sockinfo_t    *si;
  int           rc;
  int         err;

  logMsg (LOG_DBG, "=== sock_server_check");
  si = NULL;
  Sock_t s = sockServer (32702, &err);
  ck_assert_int_gt (s, 2);
  ck_assert_int_eq (socketInvalid (s), 0);
  si = sockAddCheck (si, s);
  rc = sockCheck (si);
  ck_assert_int_eq (rc, 0);
  sockRemoveCheck (si, s);
  sockFreeCheck (si);
  sockClose (s);
}
END_TEST

static Sock_t
connectWait (void)
{
  int       err;

  Sock_t c = sockConnect (gport, &err, 3000);
  gclsock = -1;
  if (socketInvalid (c)) { gthreadrc = 1; }
  gclsock = c;
  return c;
}

static void *
connectClose (void *id)
{
  Sock_t c = connectWait ();
  msleep (200);
  sockClose (c);
  return NULL;
}

static void *
connectCloseEarly (void *id)
{
  Sock_t c = connectWait ();
  sockClose (c);
#if _lib_pthread_create
  pthread_exit (NULL);
#endif
  return NULL;
}

static void *
connectWrite (void *id)
{
  char          *data = { "aaaabbbbccccddddeeeeffff" };
  char          datab [4096];

  memset (datab, 'a', 4096);
  Sock_t c = connectWait ();
  int rc = sockWriteStr (c, data, strlen (data));
  if (rc != 0) { gthreadrc = 1; }
  rc = sockWriteBinary (c, datab, 4096);
  if (rc != 0) { gthreadrc = 1; }
  msleep (200);
  sockClose (c);
#if _lib_pthread_create
  pthread_exit (NULL);
#endif
  return NULL;
}

static void *
connectWriteClose (void *id)
{
  char          *data = { "aaaabbbbccccddddeeeeffff" };
  char          datab [4096];

  memset (datab, 'a', 4096);
  Sock_t c = connectWait ();
  int rc = sockWriteStr (c, data, strlen (data));
  if (rc != 0) { gthreadrc = 1; }
  rc = sockWriteBinary (c, datab, 4096);
  if (rc != 0) { gthreadrc = 1; }
  sockClose (c);
#if _lib_pthread_create
  pthread_exit (NULL);
#endif
  return NULL;
}

static void *
connectWriteCloseFail (void *id)
{
  char          *data = { "aaaabbbbccccddddeeeeffff" };
  char          datab [4096];

  memset (datab, 'a', 4096);
  Sock_t c = connectWait ();
  int rc = sockWriteStr (c, data, strlen (data));
  rc = sockWriteBinary (c, datab, 4096);
  /* rc may be 0 or non-zero depending on system */
  sockClose (c);
#if _lib_pthread_create
  pthread_exit (NULL);
#endif
  return NULL;
}

START_TEST(sock_connect_accept)
{
  int           err;
  Sock_t        r = -1;
  Sock_t        l = -1;

  logMsg (LOG_DBG, "=== sock_connect_accept");
  gport = 32703;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectClose, NULL);
#endif

  l = sockServer (32703, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);
  r = sockAccept (l, &err);
  ck_assert_int_eq (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);

  msleep (200);
  sockClose (r);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST

START_TEST(sock_check_connect_accept)
{
  sockinfo_t    *si;
  Sock_t        rc;
  int           err;
  Sock_t        l = -1;
  Sock_t        r = -1;

  logMsg (LOG_DBG, "=== sock_check_connect_accept");
  sockClose (gclsock);
  si = NULL;
  gport = 32704;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectClose, NULL);
#endif

  l = sockServer (32704, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);
  si = sockAddCheck (si, l);
  rc = sockCheck (si);
  int count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, l);
  r = sockAccept (l, &err);
  ck_assert_int_eq (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);
  msleep (200);
  sockClose (r);
  sockRemoveCheck (si, l);
  sockFreeCheck (si);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST

START_TEST(sock_write)
{
  sockinfo_t    *si;
  Sock_t        rc;
  int           err;
  Sock_t        l = -1;
  Sock_t        r = -1;

  logMsg (LOG_DBG, "=== sock_write");
  sockClose (gclsock);
  si = NULL;
  gport = 32705;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectWrite, NULL);
#endif

  l = sockServer (32705, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);
  si = sockAddCheck (si, l);
  rc = sockCheck (si);
  int count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, l);
  r = sockAccept (l, &err);
  ck_assert_int_eq (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);
  msleep (200);
  sockClose (r);
  sockRemoveCheck (si, l);
  sockFreeCheck (si);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST

START_TEST(sock_write_read)
{
  sockinfo_t    *si;
  Sock_t        rc;
  int           err;
  Sock_t        l = -1;
  Sock_t        r = -1;
  char          *data = { "aaaabbbbccccddddeeeeffff" };
  char          datab [4096];
  char          *ndata;
  size_t        len;

  logMsg (LOG_DBG, "=== sock_write_read");
  sockClose (gclsock);
  si = NULL;
  gport = 32706;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectWrite, NULL);
#endif
  memset (datab, 'a', 4096);

  l = sockServer (32706, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);
  si = sockAddCheck (si, l);
  rc = sockCheck (si);
  int count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, l);
  r = sockAccept (l, &err);
  ck_assert_int_eq (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);

  msleep (30); /* give time for client to write */

  ndata = sockRead (r, &len);
  ck_assert_ptr_nonnull (ndata);
  if (ndata != NULL) {
    ck_assert_int_eq (strlen (ndata) + 1, len);
    ck_assert_int_eq (strlen (ndata), strlen (data));
    ck_assert_str_eq (ndata, data);
    free (ndata);
  }

  msleep (30); /* give time for client to write */

  ndata = sockRead (r, &len);
  ck_assert_ptr_nonnull (ndata);
  ck_assert_int_eq (len, 4096);
  if (ndata != NULL) {
    ck_assert_mem_eq (ndata, datab, 4096);
    free (ndata);
  }
  msleep (200);
  sockClose (r);
  sockRemoveCheck (si, l);
  sockFreeCheck (si);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST

START_TEST(sock_write_read_buff)
{
  sockinfo_t    *si;
  Sock_t        rc;
  int           err;
  Sock_t        l = -1;
  Sock_t        r = -1;
  char          *data = { "aaaabbbbccccddddeeeeffff" };
  char          buff [80];
  char          *ndata;
  size_t        len;

  logMsg (LOG_DBG, "=== sock_write_read_buff");
  sockClose (gclsock);
  si = NULL;
  gport = 32707;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectWrite, NULL);
#endif

  l = sockServer (32707, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);
  si = sockAddCheck (si, l);
  rc = sockCheck (si);
  int count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, l);
  r = sockAccept (l, &err);
  ck_assert_int_eq (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);

  msleep (30); /* time for client to write */

  ndata = sockReadBuff (r, &len, buff, sizeof(buff));
  ck_assert_ptr_nonnull (ndata);
  if (ndata != NULL) {
    ck_assert_int_eq (strlen (ndata) + 1, len);
    ck_assert_int_eq (strlen (ndata), strlen (data));
    ck_assert_str_eq (ndata, data);
  }
  msleep (200);
  sockClose (r);
  sockRemoveCheck (si, l);
  sockFreeCheck (si);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST

START_TEST(sock_write_read_buff_fail)
{
  sockinfo_t    *si;
  Sock_t        rc;
  int           err;
  Sock_t        l = -1;
  Sock_t        r = -1;
  char          buff [10];
  char          *ndata;
  size_t        len;

  logMsg (LOG_DBG, "=== sock_write_read_buff_fail");
  sockClose (gclsock);
  si = NULL;
  gport = 32708;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectWrite, NULL);
#endif

  l = sockServer (32708, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);
  si = sockAddCheck (si, l);
  rc = sockCheck (si);
  int count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, l);
  r = sockAccept (l, &err);
  ck_assert_int_eq  (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);

  ndata = sockReadBuff (r, &len, buff, sizeof(buff));
  ck_assert_ptr_null (ndata);
  ck_assert_int_eq (len, 0);
  msleep (200);
  sockClose (r);
  sockRemoveCheck (si, l);
  sockFreeCheck (si);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST

START_TEST(sock_write_check_read)
{
  sockinfo_t    *si;
  Sock_t        rc;
  int           err;
  Sock_t        l = -1;
  Sock_t        r = -1;
  char          *data = { "aaaabbbbccccddddeeeeffff" };
  char          datab [4096];
  char          *ndata;
  size_t        len;

  logMsg (LOG_DBG, "=== sock_write_check_read");
  sockClose (gclsock);
  si = NULL;
  gport = 32709;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectWrite, NULL);
#endif
  memset (datab, 'a', 4096);

  l = sockServer (32709, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);
  si = sockAddCheck (si, l);
  rc = sockCheck (si);
  int count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, l);
  r = sockAccept (l, &err);
  ck_assert_int_eq (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);
  si = sockAddCheck (si, r);

  rc = sockCheck (si);
  count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, r);
  ndata = sockRead (r, &len);
  ck_assert_ptr_nonnull (ndata);
  if (ndata != NULL) {
    ck_assert_int_eq (strlen (ndata) + 1, len);
    ck_assert_int_eq (strlen (ndata), strlen (data));
    ck_assert_str_eq (ndata, data);
    free (ndata);
  }

  rc = sockCheck (si);
  count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, r);
  ndata = sockRead (r, &len);
  ck_assert_ptr_nonnull (ndata);
  if (ndata != NULL) {
    ck_assert_int_eq (len, 4096);
    ck_assert_mem_eq (ndata, datab, 4096);
    free (ndata);
  }
  msleep (200);
  sockClose (r);
  sockRemoveCheck (si, l);
  sockRemoveCheck (si, r);
  sockFreeCheck (si);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST

START_TEST(sock_close)
{
  sockinfo_t    *si;
  Sock_t        rc;
  int           err;
  Sock_t        l = -1;
  Sock_t        r = -1;
  char          *ndata;
  size_t        len;

  logMsg (LOG_DBG, "=== sock_close");
  sockClose (gclsock);
  si = NULL;
  gport = 32710;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectCloseEarly, NULL);
#endif

  l = sockServer (32710, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);

  si = sockAddCheck (si, l);
  rc = sockCheck (si);
  int count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, l);
  r = sockAccept (l, &err);
  ck_assert_int_eq (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);
  si = sockAddCheck (si, r);

  msleep (20); /* a delay to allow client to close */

  rc = sockCheck (si);
  count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  if (rc > 0) {
    ndata = sockRead (r, &len);
    ck_assert_ptr_null (ndata);
    if (ndata != NULL) {
      free (ndata);
    }
  }
  msleep (200);
  sockClose (r);
  sockRemoveCheck (si, l);
  sockRemoveCheck (si, r);
  sockFreeCheck (si);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST

START_TEST(sock_write_close)
{
  sockinfo_t    *si;
  Sock_t        rc;
  int           err;
  Sock_t        l = -1;
  Sock_t        r = -1;
  char          datab [4096];
  char          *ndata;
  size_t        len;

  logMsg (LOG_DBG, "=== sock_write_close");
  sockClose (gclsock);
  si = NULL;
  gport = 32711;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectWriteClose, NULL);
#endif
  memset (datab, 'a', 4096);

  l = sockServer (32711, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);

  si = sockAddCheck (si, l);
  rc = sockCheck (si);
  int count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, l);
  r = sockAccept (l, &err);
  ck_assert_int_eq (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);
  si = sockAddCheck (si, r);

  msleep (20); /* a delay to allow client to close */

  rc = sockCheck (si);
  count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, r);
  ndata = sockRead (r, &len);
  ck_assert_ptr_nonnull (ndata);
  if (ndata != NULL) {
    free (ndata);
  }

  rc = sockCheck (si);
  count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, r);
  ndata = sockRead (r, &len);
  ck_assert_ptr_nonnull (ndata);
  if (ndata != NULL) {
    free (ndata);
  }
  msleep (200);
  sockClose (r);
  sockRemoveCheck (si, l);
  sockRemoveCheck (si, r);
  sockFreeCheck (si);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST

START_TEST(sock_server_close)
{
  sockinfo_t    *si;
  Sock_t        rc;
  int           err;
  Sock_t        l = -1;
  Sock_t        r = -1;
  char          datab [4096];

  logMsg (LOG_DBG, "=== sock_server_close");
  si = NULL;
  gport = 32712;
  gthreadrc = 0;
#if _lib_pthread_create
  pthread_create (&thread, NULL, connectWriteCloseFail, NULL);
#endif
  memset (datab, 'a', 4096);

  l = sockServer (32712, &err);
  ck_assert_int_gt (l, 2);
  ck_assert_int_eq (socketInvalid (l), 0);

  si = sockAddCheck (si, l);
  rc = sockCheck (si);
  int count = 0;
  while (rc == 0 && count < 100) {
    msleep (20);
    rc = sockCheck (si);
    ++count;
  }
  ck_assert_int_eq (rc, l);
  r = sockAccept (l, &err);
  ck_assert_int_eq (socketInvalid (r), 0);
  ck_assert_int_ne (l, r);
  sockClose (r);
  for (int i = 0; i < 60; ++i) {
    msleep (100);
  }
  sockClose (r);
  sockRemoveCheck (si, l);
  sockFreeCheck (si);
  sockClose (l);
  ck_assert_int_eq (gthreadrc, 0);
#if _lib_pthread_create
  pthread_join (thread, NULL);
#endif
}
END_TEST


Suite *
sock_suite (void)
{
  Suite     *s;
  TCase     *tc;

//  logStderr ();
  s = suite_create ("Sock Suite");
  tc = tcase_create ("Sock");
  tcase_add_test (tc, sock_server_create);
  tcase_add_test (tc, sock_server_create_check);
  tcase_add_test (tc, sock_server_check);
  tcase_add_test (tc, sock_connect_accept);
  tcase_add_test (tc, sock_check_connect_accept);
  tcase_add_test (tc, sock_write);
  tcase_add_test (tc, sock_write_read);
  tcase_add_test (tc, sock_write_read_buff);
  tcase_add_test (tc, sock_write_read_buff_fail);
  tcase_add_test (tc, sock_write_check_read);
  tcase_add_test (tc, sock_close);
  tcase_add_test (tc, sock_write_close);
  tcase_add_test (tc, sock_server_close);
  tcase_set_timeout (tc, 10.0);
  suite_add_tcase (s, tc);
  return s;
}
