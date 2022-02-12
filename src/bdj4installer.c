#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>

#include <gtk/gtk.h>

#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjstring.h"
#include "filedata.h"
#include "fileop.h"
#include "filemanip.h"
#include "fileutil.h"
#include "localeutil.h"
#include "log.h"
#include "pathutil.h"
#include "portability.h"
#include "slist.h"
#include "sysvars.h"
#include "tmutil.h"
#include "uiutils.h"

typedef enum {
  INST_BEGIN,
  INST_INIT,
  INST_SAVE,
  INST_MAKE_TARGET,
  INST_COPY_START,
  INST_COPY_FILES,
  INST_CHDIR,
  INST_CREATE_DIRS,
  INST_CLEAN,
  INST_COPY_TEMPLATES,
  INST_CONV_START,
  INST_CONVERTER,
  INST_CREATE_SHORTCUT,
  INST_CLEANUP,
  INST_FINISH,
} installstate_t;

typedef struct {
  installstate_t  instState;
  char            *home;
  char            *target;
  char            hostname [MAXPATHLEN];
  char            locale [40];
  char            rundir [MAXPATHLEN];
  char            datatopdir [MAXPATHLEN];
  char            currdir [MAXPATHLEN];
  char            unpackdir [MAXPATHLEN];
  GtkApplication  *app;
  GtkWidget       *window;
  GtkWidget       *targetEntry;
  GtkWidget       *reinstWidget;
  GtkWidget       *feedbackMsg;
  GtkEntryBuffer  *targetBuffer;
  GtkTextBuffer   *dispBuffer;
  GtkWidget       *dispTextView;
  mstime_t        validateTimer;
  bool            newinstall : 1;
  bool            reinstall : 1;
  bool            freetarget : 1;
  bool            guienabled : 1;
} installer_t;

#define INST_TEMP_FILE  "tmp/bdj4instout.txt"
#define INST_SAVE_FNAME "installdir.txt"

static int  installerCreateGui (installer_t *installer, int argc, char *argv []);
static void installerActivate (GApplication *app, gpointer udata);
static int  installerMainLoop (void *udata);
static void installerSelectDirDialog (GtkButton *b, gpointer udata);
static void installerExit (GtkButton *b, gpointer udata);
static void installerCheckDir (GtkButton *b, gpointer udata);
static void installerValidateDir (installer_t *installer);
static void installerValidateStart (GtkEditable *e, gpointer udata);
static void installerInstall (GtkButton *b, gpointer udata);
static bool installerCheckTarget (installer_t *installer, const char *dir);
static void installerInstInit (installer_t *installer);
static void installerSaveTargetDir (installer_t *installer);
static void installerMakeTarget (installer_t *installer);
static void installerCopyStart (installer_t *installer);
static void installerCopyFiles (installer_t *installer);
static void installerChangeDir (installer_t *installer);
static void installerCreateDirs (installer_t *installer);
static void installerCleanOldFiles (installer_t *installer);
static void installerCopyTemplates (installer_t *installer);
static void installerRunConversion (installer_t *installer);
static void installerCreateShortcut (installer_t *installer);
static void installerCleanup (installer_t *installer);
static void installerDisplayText (installer_t *installer, char *txt);
static void installerScrollToEnd (GtkWidget *w, GtkAllocation *retAllocSize, gpointer udata);;
static void installerGetTargetFname (installer_t *installer, char *buff, size_t len);
static void installerTemplateCopy (char *from, char *to);
static void installerSetrundir (installer_t *installer, const char *dir);

int
main (int argc, char *argv[])
{
  installer_t   installer;
  int           status;
  char          tbuff [512];
  char          buff [MAXPATHLEN];
  FILE          *fh;
  int           c = 0;
  int           option_index = 0;

  static struct option bdj_options [] = {
    { "installer",  no_argument,        NULL,   12 },
    { "reinstall",  no_argument,        NULL,   'r' },
    { "guienabled", no_argument,        NULL,   'g' },
    { "unpackdir",  required_argument,  NULL,   'u' },
    { "debug",      required_argument,  NULL,   'd' },
    { "theme",      required_argument,  NULL,   't' },
    { "debugself",  no_argument,        NULL,   'D' },
    { NULL,         0,                  NULL,   0 }
  };


  localeInit ();

  installer.unpackdir [0] = '\0';
  buff [0] = '\0';
  installer.home = getenv ("HOME");
  if (installer.home == NULL) {
    /* probably a windows machine */
    installer.home = getenv ("USERPROFILE");
  }
  installer.instState = INST_BEGIN;
  installer.target = buff;
  installer.locale [0] = '\0';
  installer.app = NULL;
  installer.window = NULL;
  installer.targetEntry = NULL;
  installer.reinstWidget = NULL;
  installer.feedbackMsg = NULL;
  installer.targetBuffer = NULL;
  installer.dispBuffer = NULL;
  installer.dispTextView = NULL;
  getcwd (installer.currdir, MAXPATHLEN);
  installer.newinstall = true;
  installer.reinstall = false;
  installer.freetarget = false;
  installer.guienabled = true;
  mstimeset (&installer.validateTimer, 3600000);

  while ((c = getopt_long_only (argc, argv, "p:d:t:", bdj_options, &option_index)) != -1) {
    switch (c) {
      case 12: {
        break;
      }
      case 'd': {
        break;
      }
      case 'D': {
        break;
      }
      case 't': {
        break;
      }
      case 'g': {
        installer.guienabled = false;
        break;
      }
      case 'r': {
        installer.reinstall = true;
        break;
      }
      case 'u': {
        strlcpy (installer.unpackdir, optarg, MAXPATHLEN);
        break;
      }
      default: {
        break;
      }
    }
  }

  /* want to do an initial validation */
  if (installer.guienabled) {
    mstimeset (&installer.validateTimer, 500);
  }

  if (*installer.unpackdir == '\0') {
    fprintf (stderr, "Error: unpackdir argument is required\n");
    exit (1);
  }

  snprintf (tbuff, sizeof (tbuff), "%s/BDJ4", installer.home);
  strlcpy (buff, tbuff, sizeof (buff));
  if (isMacOS ()) {
    snprintf (buff, sizeof (buff), "%s/Applications/BallroomDJ4.app", installer.home);
  }

  /* the data in sysvars will not be correct.  don't use it.  */
  /* the installer only needs the hostname, os info and locale */
  sysvarsInit (argv[0]);

  strlcpy (installer.hostname, sysvarsGetStr (SV_HOSTNAME), MAXPATHLEN);

  installerGetTargetFname (&installer, tbuff, sizeof (tbuff));
  fh = fopen (tbuff, "r");
  if (fh != NULL) {
    /* installer.target is pointing at buff */
    fgets (buff, MAXPATHLEN, fh);
    stringTrim (buff);
    fclose (fh);
  }

  if (isWindows ()) {
    /* installer.target is pointing at buff */
    strlcpy (tbuff, installer.target, sizeof (tbuff));
    pathToWinPath (installer.target, tbuff, sizeof (tbuff));
  }

  if (installer.guienabled) {
    g_timeout_add (UI_MAIN_LOOP_TIMER, installerMainLoop, &installer);
    status = installerCreateGui (&installer, 0, NULL);
  } else {
    installer.instState = INST_INIT;
    while (installer.instState != INST_FINISH) {
      installerMainLoop (&installer);
      if (installer.instState == INST_BEGIN) {
        /* exit the loop */
        installer.instState = INST_FINISH;
      }
      mssleep (50);
    }
  }

  if (installer.freetarget && installer.target != NULL) {
    free (installer.target);
  }
  logEnd ();

  return status;
}

static int
installerCreateGui (installer_t *installer, int argc, char *argv [])
{
  int             status;

  installer->app = gtk_application_new (
      "org.ballroomdj.BallroomDJ.installer",
      G_APPLICATION_FLAGS_NONE
  );
  g_signal_connect (installer->app, "activate", G_CALLBACK (installerActivate), installer);

  status = g_application_run (G_APPLICATION (installer->app), argc, argv);
  if (GTK_IS_WIDGET (installer->window)) {
    gtk_widget_destroy (installer->window);
  }
  g_object_unref (installer->app);
  return status;
}

static void
installerActivate (GApplication *app, gpointer udata)
{
  installer_t           *installer = udata;
  GtkWidget             *window;
  GtkWidget             *vbox;
  GtkWidget             *hbox;
  GtkWidget             *widget;
  GtkWidget             *scwidget;
  GtkWidget             *image;
  GError                *gerr;


  window = gtk_application_window_new (GTK_APPLICATION (app));

  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_window_set_focus_on_map (GTK_WINDOW (window), FALSE);
  gtk_window_set_title (GTK_WINDOW (window), _("BDJ4 installer"));
  gtk_window_set_default_icon_from_file ("img/bdj4_icon.svg", &gerr);
  gtk_window_set_default_size (GTK_WINDOW (window), 1000, 600);
  installer->window = window;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_margin_top (GTK_WIDGET (vbox), 20);
  gtk_widget_set_margin_bottom (GTK_WIDGET (vbox), 10);
  gtk_widget_set_margin_start (GTK_WIDGET (vbox), 10);
  gtk_widget_set_margin_end (GTK_WIDGET (vbox), 10);
  gtk_widget_set_hexpand (GTK_WIDGET (vbox), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (vbox), TRUE);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_hexpand (GTK_WIDGET (hbox), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox), FALSE, FALSE, 0);

  widget = gtk_label_new (_("Enter the destination folder where BallroomDJ 4 will be installed."));
  gtk_widget_set_halign (widget, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (widget), FALSE, FALSE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_hexpand (GTK_WIDGET (hbox), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox), FALSE, FALSE, 0);

  installer->targetBuffer = gtk_entry_buffer_new (installer->target, -1);
  installer->targetEntry = gtk_entry_new_with_buffer (installer->targetBuffer);
  gtk_entry_set_width_chars (GTK_ENTRY (installer->targetEntry), 80);
  gtk_entry_set_max_length (GTK_ENTRY (installer->targetEntry), MAXPATHLEN);
  gtk_entry_set_input_purpose (GTK_ENTRY (installer->targetEntry), GTK_INPUT_PURPOSE_FREE_FORM);
  gtk_widget_set_halign (installer->targetEntry, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (GTK_WIDGET (installer->targetEntry), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (installer->targetEntry),
      TRUE, TRUE, 0);
  g_signal_connect (installer->targetEntry, "changed", G_CALLBACK (installerValidateStart), installer);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_hexpand (GTK_WIDGET (hbox), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox), FALSE, FALSE, 0);

  installer->reinstWidget = gtk_check_button_new_with_label (_("Re-install"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (installer->reinstWidget),
      installer->reinstall);
  gtk_box_pack_start (GTK_BOX (hbox), installer->reinstWidget, FALSE, FALSE, 0);
  g_signal_connect (installer->reinstWidget, "toggled",
      G_CALLBACK (installerCheckDir), installer);

  installer->feedbackMsg = gtk_label_new ("");
  uiutilsSetCss (GTK_WIDGET (installer->feedbackMsg),
      "label { color: #ffa600; }");
  gtk_widget_set_halign (installer->feedbackMsg, GTK_ALIGN_END);
  gtk_box_pack_end (GTK_BOX (hbox), installer->feedbackMsg, FALSE, FALSE, 0);

  /* button box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_hexpand (GTK_WIDGET (hbox), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox), FALSE, FALSE, 0);

  widget = gtk_button_new ();
  assert (widget != NULL);
  gtk_button_set_label (GTK_BUTTON (widget), _("Exit"));
  gtk_widget_set_margin_start (GTK_WIDGET (widget), 2);
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (widget), FALSE, FALSE, 0);
  g_signal_connect (widget, "clicked", G_CALLBACK (installerExit), installer);

  widget = gtk_button_new ();
  assert (widget != NULL);
  gtk_button_set_label (GTK_BUTTON (widget), _("Install"));
  gtk_widget_set_margin_start (GTK_WIDGET (widget), 2);
  gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (widget),
      FALSE, FALSE, 0);
  g_signal_connect (widget, "clicked", G_CALLBACK (installerInstall), installer);

  scwidget = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (scwidget), FALSE);
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (scwidget), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scwidget), TRUE);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scwidget), 500);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scwidget), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_hexpand (GTK_WIDGET (scwidget), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (scwidget), FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (scwidget),
      FALSE, FALSE, 0);

  installer->dispBuffer = gtk_text_buffer_new (NULL);
  installer->dispTextView = gtk_text_view_new_with_buffer (installer->dispBuffer);
  gtk_widget_set_size_request (installer->dispTextView, -1, 400);
  gtk_widget_set_can_focus (installer->dispTextView, FALSE);
  gtk_widget_set_halign (installer->dispTextView, GTK_ALIGN_FILL);
  gtk_widget_set_valign (installer->dispTextView, GTK_ALIGN_START);
  gtk_widget_set_hexpand (installer->dispTextView, TRUE);
  gtk_widget_set_vexpand (installer->dispTextView, TRUE);
  gtk_container_add (GTK_CONTAINER (scwidget), GTK_WIDGET (installer->dispTextView));

  /* push the text view to the top */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  assert (hbox != NULL);
  gtk_widget_set_vexpand (GTK_WIDGET (hbox), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox), TRUE, TRUE, 0);

  gtk_widget_show_all (GTK_WIDGET (window));
}

static int
installerMainLoop (void *udata)
{
  installer_t *installer = udata;

  if (mstimeCheck (&installer->validateTimer)) {
    installerValidateDir (installer);
    mstimeset (&installer->validateTimer, 3600000);
  }

  switch (installer->instState) {
    case INST_BEGIN: {
      /* do nothing */
      break;
    }
    case INST_INIT: {
      installerInstInit (installer);
      break;
    }
    case INST_SAVE: {
      installerSaveTargetDir (installer);
      break;
    }
    case INST_MAKE_TARGET: {
      installerMakeTarget (installer);
      break;
    }
    case INST_COPY_START: {
      installerCopyStart (installer);
      break;
    }
    case INST_COPY_FILES: {
      installerCopyFiles (installer);
      break;
    }
    case INST_CHDIR: {
      installerChangeDir (installer);
      break;
    }
    case INST_CREATE_DIRS: {
      installerCreateDirs (installer);

      logStartAppend ("bdj4installer", "in", LOG_IMPORTANT);
      logMsg (LOG_INSTALL, LOG_IMPORTANT, "target: %s", installer->target);

      break;
    }
    case INST_CLEAN: {
      installerCleanOldFiles (installer);
      break;
    }
    case INST_COPY_TEMPLATES: {
      installerCopyTemplates (installer);
      break;
    }
    case INST_CONV_START: {
      if (installer->newinstall || installer->reinstall) {
        installerDisplayText (installer, _("-- Starting conversion process."));
      }
      installer->instState = INST_CONVERTER;
      break;
    }
    case INST_CONVERTER: {
      installerRunConversion (installer);
      break;
    }
    case INST_CREATE_SHORTCUT: {
      installerCreateShortcut (installer);
      break;
    }
    case INST_CLEANUP: {
      installerCleanup (installer);
      break;
    }
    case INST_FINISH: {
      installerDisplayText (installer, _("## Installation complete."));
      installer->instState = INST_BEGIN;
      break;
    }
  }

  return TRUE;
}

static void
installerCheckDir (GtkButton *b, gpointer udata)
{
  installer_t   *installer = udata;

  installerValidateDir (installer);
}

static void
installerValidateDir (installer_t *installer)
{
  struct stat   statbuf;
  int           rc;
  bool          locok = false;
  const char    *dir;
  bool          exists = false;

  if (! installer->guienabled) {
    return;
  }

  if (installer->feedbackMsg == NULL ||
      installer->targetBuffer == NULL ||
      installer->reinstWidget == NULL) {
    mstimeset (&installer->validateTimer, 500);
    return;
  }

  dir = gtk_entry_buffer_get_text (installer->targetBuffer);
  installer->reinstall = gtk_toggle_button_get_active (
      GTK_TOGGLE_BUTTON (installer->reinstWidget));
  gtk_label_set_text (GTK_LABEL (installer->feedbackMsg), "");

  exists = fileopExists (dir);
  if (exists) {
    exists = installerCheckTarget (installer, dir);
    if (exists) {
      if (installer->reinstall) {
        gtk_label_set_text (GTK_LABEL (installer->feedbackMsg), _("Over-writing existing BDJ4 installation."));
      } else {
        gtk_label_set_text (GTK_LABEL (installer->feedbackMsg), _("Updating existing BDJ4 installation."));
      }
    } else {
      gtk_label_set_text (GTK_LABEL (installer->feedbackMsg), _("Error: Existing folder."));
    }
  } else {
    gtk_label_set_text (GTK_LABEL (installer->feedbackMsg), "");
  }
}

static void
installerValidateStart (GtkEditable *e, gpointer udata)
{
  installer_t   *installer = udata;

  if (! installer->guienabled) {
    return;
  }

  /* if the user is typing, clear the message */
  gtk_label_set_text (GTK_LABEL (installer->feedbackMsg), "");
  mstimeset (&installer->validateTimer, 500);
}

static void
installerExit (GtkButton *b, gpointer udata)
{
  installer_t   *installer = udata;

  if (installer->guienabled) {
    g_application_quit (G_APPLICATION (installer->app));
  } else {
    installer->instState = INST_FINISH;
  }
  return;
}

static void
installerInstall (GtkButton *b, gpointer udata)
{
  installer_t *installer = udata;

  installer->instState = INST_INIT;
}

static bool
installerCheckTarget (installer_t *installer, const char *dir)
{
  char        tbuff [MAXPATHLEN];
  bool        exists;
  const char  *fn;

  installerSetrundir (installer, dir);

  if (isWindows ()) {
    snprintf (tbuff, sizeof (tbuff), "%s/bin/bdj4.exe", installer->rundir);
  } else {
    snprintf (tbuff, sizeof (tbuff), "%s/bin/bdj4", installer->rundir);
  }
  exists = fileopExists (tbuff);
  if (exists) {
    installer->newinstall = false;
  } else {
    installer->newinstall = true;
  }

  return exists;
}

static void
installerInstInit (installer_t *installer)
{
  bool        exists = false;
  char        tbuff [MAXPATHLEN];

  if (installer->guienabled) {
    installer->target = strdup (gtk_entry_buffer_get_text (installer->targetBuffer));
    installer->freetarget = true;
    installer->reinstall = gtk_toggle_button_get_active (
        GTK_TOGGLE_BUTTON (installer->reinstWidget));
  }

  if (! installer->guienabled) {
    tbuff [0] = '\0';
    printf (_("Enter the destination directory.\n"));
    printf (_("Press 'Enter' to select the default.\n"));
    printf ("[%s] : ", installer->target);
    fflush (stdout);
    fgets (tbuff, MAXPATHLEN, stdin);
    if (*tbuff == '\n' || *tbuff == '\r') {
      tbuff [0] = '\0';
    }
    if (*tbuff != '\0') {
      strlcpy (installer->target, tbuff, MAXPATHLEN);
    }
    installerSetrundir (installer, installer->target);
  }

  exists = fileopExists (installer->target);
  if (exists) {
    exists = installerCheckTarget (installer, installer->target);

    if (exists && ! installer->guienabled) {
      if (installer->reinstall) {
        printf (_("Over-writing existing BDJ4 installation."));
      } else {
        printf (_("Updating existing BDJ4 installation."));
      }
      printf ("\n");
      fflush (stdout);

      printf (_("Proceed with installation?\n"));
      printf ("[Y] : ");
      fflush (stdout);
      fgets (tbuff, MAXPATHLEN, stdin);
      if (*tbuff == '\n' || *tbuff == '\r') {
        tbuff [0] = '\0';
      }
      if (*tbuff != '\0') {
        if (strncmp (tbuff, "Y", 1) != 0 &&
            strncmp (tbuff, "y", 1) != 0) {
          printf (_(" * Installation aborted.\n"));
          fflush (stdout);
          installer->instState = INST_BEGIN;
          return;
        }
      }
    }

    if (! exists) {
      /* do not allow an overwrite of an existing directory that is not bdj4 */
      if (installer->guienabled) {
        gtk_label_set_text (GTK_LABEL (installer->feedbackMsg), _("Folder already exists."));
      }

      snprintf (tbuff, sizeof (tbuff), _("Error: Folder %s already exists."),
          installer->target);
      installerDisplayText (installer, tbuff);
      installerDisplayText (installer, _(" * Stopped"));
      installer->instState = INST_BEGIN;
      return;
    }
  }

  installer->instState = INST_SAVE;
}

static void
installerSaveTargetDir (installer_t *installer)
{
  char  tbuff [MAXPATHLEN];
  FILE  *fh;

  installerDisplayText (installer, _("-- Saving install location."));

  if (isWindows ()) {
    snprintf (tbuff, MAXPATHLEN, "%s/AppData/Roaming/BDJ4", installer->home);
  } else {
    snprintf (tbuff, MAXPATHLEN, "%s/.config/BDJ4", installer->home);
  }
  fileopMakeDir (tbuff);

  installerGetTargetFname (installer, tbuff, MAXPATHLEN);

  fh = fopen (tbuff, "w");
  if (fh != NULL) {
    fprintf (fh, "%s\n", installer->target);
    fclose (fh);
  }

  installer->instState = INST_MAKE_TARGET;
}

static void
installerMakeTarget (installer_t *installer)
{
  fileopMakeDir (installer->target);
  fileopMakeDir (installer->rundir);

  installer->instState = INST_COPY_START;
}

static void
installerCopyStart (installer_t *installer)
{
  installerDisplayText (installer, _("-- Copying files."));

  /* the unpackdir is not necessarily the same as the current dir */
  /* on mac os, they are different */
  if (chdir (installer->unpackdir) < 0) {
    fprintf (stderr, "Unable to set working dir: %s\n", installer->unpackdir);
    installerDisplayText (installer, _("Error: Unable to set working directory."));
    installerDisplayText (installer, _(" * Stopped"));
    installer->instState = INST_BEGIN;
    return;
  }

  installer->instState = INST_COPY_FILES;
}

static void
installerCopyFiles (installer_t *installer)
{
  char      tbuff [MAXPATHLEN];

  if (isWindows ()) {
    snprintf (tbuff, MAXPATHLEN,
        "robocopy /e /j /dcopy:DAT /timfix /njh /njs /np /ndl /nfl . \"%s\"",
        installer->rundir);
    system (tbuff);
  } else {
    snprintf (tbuff, MAXPATHLEN,
        "tar -c -f - . | (cd '%s'; tar -x -f -)",
        installer->rundir);
    system (tbuff);
  }
  installerDisplayText (installer, _("   Copy finished."));
  installer->instState = INST_CHDIR;
}

static void
installerChangeDir (installer_t *installer)
{
  strlcpy (installer->datatopdir, installer->rundir, MAXPATHLEN);
  if (isMacOS ()) {
    snprintf (installer->datatopdir, MAXPATHLEN,
        "%s/Library/Application Support/BDJ4",
        installer->home);
  }

  fileopMakeDir (installer->datatopdir);

  if (chdir (installer->datatopdir)) {
    fprintf (stderr, "Unable to set working dir: %s\n", installer->datatopdir);
    installerDisplayText (installer, _("Error: Unable to set working directory."));
    installerDisplayText (installer, _(" * Stopped"));
    installer->instState = INST_BEGIN;
    return;
  }

  installer->instState = INST_CREATE_DIRS;
}

static void
installerCreateDirs (installer_t *installer)
{
  installerDisplayText (installer, _("-- Creating folder structure."));

  /* create the directories that are not included in the distribution */
  fileopMakeDir ("data");
  /* this will create the directories necessary for the configs */
  bdjoptCreateDirectories ();
  fileopMakeDir ("tmp");
  fileopMakeDir ("http");

  installer->instState = INST_CLEAN;
}

static void
installerCleanOldFiles (installer_t *installer)
{
  FILE  *fh;
  char  tbuff [MAXPATHLEN];

  installerDisplayText (installer, _("-- Cleaning old files."));

  if (chdir (installer->rundir)) {
    fprintf (stderr, "Unable to set working dir: %s\n", installer->rundir);
    installerDisplayText (installer, _("Error: Unable to set working directory."));
    installerDisplayText (installer, _(" * Stopped"));
    installer->instState = INST_BEGIN;
    return;
  }

  fh = fopen ("install/cleanuplist.txt", "r");
  if (fh != NULL) {
    while (fgets (tbuff, MAXPATHLEN, fh) != NULL) {
      if (! fileopExists (tbuff)) {
        continue;
      }

      if (fileopIsDirectory (tbuff)) {
        filemanipDeleteDir (tbuff);
      } else {
        fileopDelete (tbuff);
      }
    }
    fclose (fh);
  }

  installer->instState = INST_COPY_TEMPLATES;
}

static void
installerCopyTemplates (installer_t *installer)
{
  char            from [MAXPATHLEN];
  char            to [MAXPATHLEN];
  char            tbuff [MAXPATHLEN];
  const char      *fname;
  slist_t         *dirlist;
  slistidx_t      iteridx;
  pathinfo_t      *pi;


  if (! installer->newinstall && ! installer->reinstall) {
    installer->instState = INST_CONV_START;
    return;
  }

  installerDisplayText (installer, _("-- Copying template files."));

  if (chdir (installer->datatopdir)) {
    fprintf (stderr, "Unable to set working dir: %s\n", installer->datatopdir);
    installerDisplayText (installer, _("Error: Unable to set working directory."));
    installerDisplayText (installer, _(" * Stopped"));
    installer->instState = INST_BEGIN;
    return;
  }

  snprintf (tbuff, MAXPATHLEN, "%s/templates", installer->rundir);
  dirlist = filemanipBasicDirList (tbuff, NULL);
  slistStartIterator (dirlist, &iteridx);
  while ((fname = slistIterateKey (dirlist, &iteridx)) != NULL) {
    if (strcmp (fname, "qrcode") == 0) {
      continue;
    }
    if (strcmp (fname, "qrcode.html") == 0) {
      continue;
    }
    if (strcmp (fname, "html-list.txt") == 0) {
      continue;
    }

    if (strcmp (fname, "bdj-flex-dark.html") == 0) {
      snprintf (from, MAXPATHLEN, "%s/templates/%s",
          installer->rundir, fname);
      snprintf (to, MAXPATHLEN, "http/bdj4remote.html");
      installerTemplateCopy (from, to);
      continue;
    }
    if (strcmp (fname, "mobilemq.html") == 0) {
      snprintf (from, MAXPATHLEN, "%s/templates/%s",
          installer->rundir, fname);
      snprintf (to, MAXPATHLEN, "http/%s", fname);
      installerTemplateCopy (from, to);
      continue;
    }

    pi = pathInfo (fname);
    if (pathInfoExtCheck (pi, ".crt")) {
      free (pi);
      continue;
    }
    if (pathInfoExtCheck (pi, ".html")) {
      free (pi);
      continue;
    }

    if (pathInfoExtCheck (pi, ".svg")) {
      snprintf (from, MAXPATHLEN, "%s/templates/%s",
          installer->rundir, fname);
      snprintf (to, MAXPATHLEN, "%s/img/%s",
          installer->rundir, fname);
    } else if (strncmp (fname, "bdjconfig", 9) == 0) {
      snprintf (from, MAXPATHLEN, "%s/templates/%s",
          installer->rundir, fname);

      snprintf (tbuff, MAXPATHLEN, "%.*s", (int) pi->blen, pi->basename);
      if (pathInfoExtCheck (pi, ".g")) {
        snprintf (to, MAXPATHLEN, "data/%s", tbuff);
      }
      if (pathInfoExtCheck (pi, ".p")) {
        snprintf (to, MAXPATHLEN, "data/profiles/%s", tbuff);
      }
      if (pathInfoExtCheck (pi, ".m")) {
        snprintf (to, MAXPATHLEN, "data/%s/%s", installer->hostname, tbuff);
      }
      if (pathInfoExtCheck (pi, ".mp")) {
        snprintf (to, MAXPATHLEN, "data/%s/profiles/%s",
            installer->hostname, tbuff);
      }
    } else if (pathInfoExtCheck (pi, ".txt") ||
        pathInfoExtCheck (pi, ".sequence") ||
        pathInfoExtCheck (pi, ".pldances") ||
        pathInfoExtCheck (pi, ".pl") ) {
      snprintf (from, MAXPATHLEN, "%s/templates/%s",
          installer->rundir, fname);
      snprintf (to, MAXPATHLEN, "data/%s", fname);
    }

    installerTemplateCopy (from, to);

    free (pi);
  }
  slistFree (dirlist);

  snprintf (from, MAXPATHLEN, "%s/img/favicon.ico", installer->rundir);
  snprintf (to, MAXPATHLEN, "http/favicon.ico");
  installerTemplateCopy (from, to);

  snprintf (from, MAXPATHLEN, "%s/img/led_on.svg", installer->rundir);
  snprintf (to, MAXPATHLEN, "http/led_on.svg");
  installerTemplateCopy (from, to);

  snprintf (from, MAXPATHLEN, "%s/img/led_off.svg", installer->rundir);
  snprintf (to, MAXPATHLEN, "http/led_off.svg");
  installerTemplateCopy (from, to);

  snprintf (from, MAXPATHLEN, "%s/img/ballroomdj.svg", installer->rundir);
  snprintf (to, MAXPATHLEN, "http/ballroomdj.svg");
  installerTemplateCopy (from, to);

  snprintf (from, MAXPATHLEN, "%s/img/mrc", installer->rundir);
  snprintf (to, MAXPATHLEN, "http/mrc");
  if (isWindows ()) {
    snprintf (tbuff, MAXPATHLEN,
        "robocopy /e /j /dcopy:DAT /timfix /njh /njs /np /ndl /nfl \"%s\" \"%s\"",
        from, to);
    system (tbuff);
  } else {
    snprintf (tbuff, MAXPATHLEN, "cp -r '%s' '%s'", from, "http");
    system (tbuff);
  }

  installer->instState = INST_CONV_START;
}

static void
installerRunConversion (installer_t *installer)
{
  char      tbuff [MAXPATHLEN];
  char      buff [MAXPATHLEN];

  if (! installer->newinstall && ! installer->reinstall) {
    installer->instState = INST_CREATE_SHORTCUT;
    return;
  }

  if (chdir (installer->rundir)) {
    fprintf (stderr, "Unable to set working dir: %s\n", installer->rundir);
    installerDisplayText (installer, _("Error: Unable to set working directory."));
    installerDisplayText (installer, _(" * Stopped"));
    installer->instState = INST_BEGIN;
    return;
  }

  if (installer->newinstall || installer->reinstall) {
    /* windows does not have a non-gui conversion process */
    if (installer->guienabled || isWindows()) {
      strlcpy (tbuff, "./bin/bdj4converter", MAXPATHLEN);
      if (isWindows ()) {
        strlcat (tbuff, ".exe", MAXPATHLEN);
        pathToWinPath (buff, tbuff, MAXPATHLEN);
        strlcpy (tbuff, buff, MAXPATHLEN);
      }
    } else {
      strlcpy (tbuff, "./install/install-convert.sh", MAXPATHLEN);
    }

    strlcat (tbuff, " --datatopdir \"", MAXPATHLEN);
    strlcat (tbuff, installer->datatopdir, MAXPATHLEN);
    strlcat (tbuff, "\"", MAXPATHLEN);
    if (! installer->guienabled) {
      strlcat (tbuff, " --guienabled F ", MAXPATHLEN);
    }

    system (tbuff);
    installerDisplayText (installer, _("   Conversion complete."));
  } else {
    installerDisplayText (installer, _("-- Conversion already done."));
  }

  installer->instState = INST_CREATE_SHORTCUT;
}

static void
installerCreateShortcut (installer_t *installer)
{
  char buff [MAXPATHLEN];

  if (chdir (installer->rundir)) {
    fprintf (stderr, "Unable to set working dir: %s\n", installer->rundir);
    installerDisplayText (installer, _("Error: Unable to set working directory."));
    installerDisplayText (installer, _(" * Stopped"));
    installer->instState = INST_BEGIN;
    return;
  }

  installerDisplayText (installer, _("-- Creating shortcut."));
  if (isWindows ()) {
    if (! chdir ("install")) {
      snprintf (buff, MAXPATHLEN, ".\\makeshortcut.bat \"%s\"",
          installer->rundir);
      system (buff);
      chdir (installer->rundir);
    }
  }
  if (isMacOS ()) {
#if _lib_symlink
    /* this must exist and match the name of the app */
    symlink ("bin/bdj4", "BDJ4");
    /* desktop shortcut */
    snprintf (buff, MAXPATHLEN, "%s/Desktop/BDJ4.app", installer->home);
    symlink (installer->target, buff);
#endif
  }
  if (isLinux ()) {
    snprintf (buff, MAXPATHLEN, "./install/linuxshortcut.sh '%s'",
        installer->rundir);
    system (buff);
  }

  installer->instState = INST_CLEANUP;
}

static void
installerCleanup (installer_t *installer)
{
  char  buff [MAXPATHLEN];
  char  *argv [5];

  if (! fileopExists (installer->unpackdir)) {
    installer->instState = INST_FINISH;
    return;
  }

  installerDisplayText (installer, _("-- Cleaning up."));
  if (isWindows ()) {
    argv [0] = ".\\install\\install-rminstdir.bat";
    argv [1] = installer->unpackdir;
    argv [2] = NULL;
    execv (argv [0], argv);
  } else {
    snprintf (buff, MAXPATHLEN, "rm -rf %s", installer->unpackdir);
    system (buff);
  }

  installer->instState = INST_FINISH;
}

static void
installerDisplayText (installer_t *installer, char *txt)
{
  GtkTextIter iter;

  if (installer->guienabled) {
    gtk_text_buffer_get_end_iter (installer->dispBuffer, &iter);
    gtk_text_buffer_insert (installer->dispBuffer, &iter, txt, -1);
    gtk_text_buffer_get_end_iter (installer->dispBuffer, &iter);
    gtk_text_buffer_insert (installer->dispBuffer, &iter, "\n", -1);
  } else {
    printf ("%s\n", txt);
    fflush (stdout);
  }
}

static void
installerScrollToEnd (GtkWidget *w, GtkAllocation *retAllocSize, gpointer udata)
{
  installer_t *installer = udata;
  GtkTextIter iter;

  gtk_text_buffer_get_end_iter (installer->dispBuffer, &iter);
  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (installer->dispTextView),
      &iter, 0, false, 0, 0);
}

static void
installerGetTargetFname (installer_t *installer, char *buff, size_t sz)
{
  if (isWindows ()) {
    snprintf (buff, sz, "%s/AppData/Roaming/BDJ4/%s",
        installer->home, INST_SAVE_FNAME);
  } else {
    snprintf (buff, sz, "%s/.config/BDJ4/%s",
        installer->home, INST_SAVE_FNAME);
  }
}

static void
installerTemplateCopy (char *from, char *to)
{
  char      localesfx [20];
  char      tbuff [MAXPATHLEN];

  snprintf (localesfx, sizeof (localesfx), ".%s", sysvarsGetStr (SV_LOCALE));
  strlcpy (tbuff, from, MAXPATHLEN);
  strlcat (tbuff, localesfx, MAXPATHLEN);
  if (fileopExists (tbuff)) {
    from = tbuff;
  }
  filemanipCopy (from, to);
}

static void
installerSetrundir (installer_t *installer, const char *dir)
{
  strlcpy (installer->rundir, dir, MAXPATHLEN);
  if (isMacOS ()) {
    strlcat (installer->rundir, "/Contents/MacOS", MAXPATHLEN);
  }
}
