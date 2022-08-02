#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <stdarg.h>

#include "bdj4.h"
#include "bdj4intl.h"
#include "configui.h"
#include "log.h"
#include "songfilter.h"
#include "ui.h"

void
confuiBuildUIFilterDisplay (confuigui_t *gui)
{
  UIWidget      vbox;
  UIWidget      sg;
  nlistidx_t    val;

  logProcBegin (LOG_PROC, "confuiBuildUIFilterDisplay");
  uiCreateVertBox (&vbox);

  /* filter display */
  confuiMakeNotebookTab (&vbox, gui,
      /* CONTEXT: configuration: song filter display settings */
      _("Filter Display"), CONFUI_ID_FILTER);
  uiCreateSizeGroupHoriz (&sg);

  val = nlistGetNum (gui->filterDisplaySel, FILTER_DISP_GENRE);
  /* CONTEXT: configuration: filter display: checkbox: genre */
  confuiMakeItemCheckButton (gui, &vbox, &sg, _("Genre"),
      CONFUI_WIDGET_FILTER_GENRE, -1, val);
  gui->uiitem [CONFUI_WIDGET_FILTER_GENRE].outtype = CONFUI_OUT_CB;

  val = nlistGetNum (gui->filterDisplaySel, FILTER_DISP_DANCELEVEL);
  /* CONTEXT: configuration: filter display: checkbox: dance level */
  confuiMakeItemCheckButton (gui, &vbox, &sg, _("Dance Level"),
      CONFUI_WIDGET_FILTER_DANCELEVEL, -1, val);
  gui->uiitem [CONFUI_WIDGET_FILTER_DANCELEVEL].outtype = CONFUI_OUT_CB;

  val = nlistGetNum (gui->filterDisplaySel, FILTER_DISP_STATUS);
  /* CONTEXT: configuration: filter display: checkbox: status */
  confuiMakeItemCheckButton (gui, &vbox, &sg, _("Status"),
      CONFUI_WIDGET_FILTER_STATUS, -1, val);
  gui->uiitem [CONFUI_WIDGET_FILTER_STATUS].outtype = CONFUI_OUT_CB;

  val = nlistGetNum (gui->filterDisplaySel, FILTER_DISP_FAVORITE);
  /* CONTEXT: configuration: filter display: checkbox: favorite selection */
  confuiMakeItemCheckButton (gui, &vbox, &sg, _("Favorite"),
      CONFUI_WIDGET_FILTER_FAVORITE, -1, val);
  gui->uiitem [CONFUI_WIDGET_FILTER_FAVORITE].outtype = CONFUI_OUT_CB;

  val = nlistGetNum (gui->filterDisplaySel, FILTER_DISP_STATUSPLAYABLE);
  /* CONTEXT: configuration: filter display: checkbox: status is playable */
  confuiMakeItemCheckButton (gui, &vbox, &sg, _("Playable Status"),
      CONFUI_SWITCH_FILTER_STATUS_PLAYABLE, -1, val);
  gui->uiitem [CONFUI_SWITCH_FILTER_STATUS_PLAYABLE].outtype = CONFUI_OUT_CB;
  logProcEnd (LOG_PROC, "confuiBuildUIFilterDisplay", "");
}

