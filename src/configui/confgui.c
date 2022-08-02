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
#include "sysvars.h"
#include "tmutil.h"
#include "ui.h"
#include "uiduallist.h"
#include "uinbutil.h"
#include "validate.h"

static bool confuiLinkCallback (void *udata);
static long confuiValMSCallback (void *udata, const char *txt);

void
confuiMakeNotebookTab (UIWidget *boxp, confuigui_t *gui, char *txt, int id)
{
  UIWidget    uiwidget;

  logProcBegin (LOG_PROC, "confuiMakeNotebookTab");
  uiCreateLabel (&uiwidget, txt);
  uiWidgetSetAllMargins (&uiwidget, 0);
  uiWidgetExpandHoriz (boxp);
  uiWidgetExpandVert (boxp);
  uiWidgetSetAllMargins (boxp, uiBaseMarginSz * 2);
  uiNotebookAppendPage (&gui->notebook, boxp, &uiwidget);
  uiutilsNotebookIDAdd (gui->nbtabid, id);

  logProcEnd (LOG_PROC, "confuiMakeNotebookTab", "");
}

void
confuiMakeItemEntry (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    char *txt, int widx, int bdjoptIdx, char *disp)
{
  UIWidget    hbox;

  logProcBegin (LOG_PROC, "confuiMakeItemEntry");
  uiCreateHorizBox (&hbox);
  confuiMakeItemEntryBasic (gui, &hbox, sg, txt, widx, bdjoptIdx, disp);
  uiBoxPackStart (boxp, &hbox);
  logProcEnd (LOG_PROC, "confuiMakeItemEntry", "");
}

void
confuiMakeItemEntryChooser (confuigui_t *gui, UIWidget *boxp,
    UIWidget *sg, char *txt, int widx, int bdjoptIdx,
    char *disp, void *dialogFunc)
{
  UIWidget    hbox;
  UIWidget    uiwidget;

  logProcBegin (LOG_PROC, "confuiMakeItemEntryChooser");
  uiCreateHorizBox (&hbox);
  confuiMakeItemEntryBasic (gui, &hbox, sg, txt, widx, bdjoptIdx, disp);
  uiutilsUICallbackInit (&gui->uiitem [widx].callback,
      dialogFunc, gui);
  uiCreateButton (&uiwidget, &gui->uiitem [widx].callback,
      "", NULL);
  uiButtonSetImageIcon (&uiwidget, "folder");
  uiWidgetSetMarginStart (&uiwidget, 0);
  uiBoxPackStart (&hbox, &uiwidget);
  uiBoxPackStart (boxp, &hbox);
  logProcEnd (LOG_PROC, "confuiMakeItemEntryChooser", "");
}

void
confuiMakeItemEntryBasic (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    char *txt, int widx, int bdjoptIdx, char *disp)
{
  UIWidget  *uiwidgetp;

  gui->uiitem [widx].basetype = CONFUI_ENTRY;
  gui->uiitem [widx].outtype = CONFUI_OUT_STR;
  confuiMakeItemLabel (boxp, sg, txt);
  uiEntryCreate (gui->uiitem [widx].entry);
  uiwidgetp = uiEntryGetUIWidget (gui->uiitem [widx].entry);
  uiutilsUIWidgetCopy (&gui->uiitem [widx].uiwidget, uiwidgetp);
  uiWidgetSetMarginStart (uiwidgetp, uiBaseMarginSz * 4);
  uiBoxPackStart (boxp, uiwidgetp);
  if (disp != NULL) {
    uiEntrySetValue (gui->uiitem [widx].entry, disp);
  } else {
    uiEntrySetValue (gui->uiitem [widx].entry, "");
  }
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;
}

void
confuiMakeItemCombobox (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    char *txt, int widx, int bdjoptIdx, UILongCallbackFunc ddcb, char *value)
{
  UIWidget    hbox;
  UIWidget    uiwidget;
  UIWidget    *uiwidgetp;

  logProcBegin (LOG_PROC, "confuiMakeItemCombobox");
  gui->uiitem [widx].basetype = CONFUI_COMBOBOX;
  gui->uiitem [widx].outtype = CONFUI_OUT_STR;

  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);

  uiutilsUICallbackLongInit (&gui->uiitem [widx].callback, ddcb, gui);
  uiwidgetp = uiComboboxCreate (&gui->window, txt,
      &gui->uiitem [widx].callback,
      gui->uiitem [widx].dropdown, gui);

  uiDropDownSetList (gui->uiitem [widx].dropdown,
      gui->uiitem [widx].displist, NULL);
  uiDropDownSelectionSetStr (gui->uiitem [widx].dropdown, value);
  uiWidgetSetMarginStart (uiwidgetp, uiBaseMarginSz * 4);
  uiBoxPackStart (&hbox, uiwidgetp);
  uiBoxPackStart (boxp, &hbox);

  uiutilsUIWidgetCopy (&gui->uiitem [widx].uiwidget, &uiwidget);
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;
  logProcEnd (LOG_PROC, "confuiMakeItemCombobox", "");
}

void
confuiMakeItemLink (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    char *txt, int widx, char *disp)
{
  UIWidget    hbox;
  UIWidget    uiwidget;

  logProcBegin (LOG_PROC, "confuiMakeItemLink");
  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);
  uiutilsUIWidgetInit (&uiwidget);
  uiCreateLink (&uiwidget, disp, NULL);
  if (isMacOS ()) {
    uiutilsUICallbackInit (&gui->uiitem [widx].callback,
        confuiLinkCallback, gui);
    gui->uiitem [widx].uri = NULL;
    uiLinkSetActivateCallback (&uiwidget, &gui->uiitem [widx].callback);
  }
  uiBoxPackStart (&hbox, &uiwidget);
  uiBoxPackStart (boxp, &hbox);
  uiutilsUIWidgetCopy (&gui->uiitem [widx].uiwidget, &uiwidget);
  logProcEnd (LOG_PROC, "confuiMakeItemLink", "");
}

void
confuiMakeItemFontButton (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    char *txt, int widx, int bdjoptIdx, char *fontname)
{
  UIWidget    hbox;
  GtkWidget   *widget;

  logProcBegin (LOG_PROC, "confuiMakeItemFontButton");
  gui->uiitem [widx].basetype = CONFUI_FONT;
  gui->uiitem [widx].outtype = CONFUI_OUT_STR;
  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);
  if (fontname != NULL && *fontname) {
    widget = gtk_font_button_new_with_font (fontname);
  } else {
    widget = gtk_font_button_new ();
  }
  uiWidgetSetMarginStartW (widget, uiBaseMarginSz * 4);
  uiBoxPackStartUW (&hbox, widget);
  uiBoxPackStart (boxp, &hbox);
  gui->uiitem [widx].widget = widget;
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;
  logProcEnd (LOG_PROC, "confuiMakeItemFontButton", "");
}

void
confuiMakeItemColorButton (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    char *txt, int widx, int bdjoptIdx, char *color)
{
  UIWidget    hbox;
  GtkWidget   *widget;
  GdkRGBA     rgba;

  logProcBegin (LOG_PROC, "confuiMakeItemColorButton");

  gui->uiitem [widx].basetype = CONFUI_COLOR;
  gui->uiitem [widx].outtype = CONFUI_OUT_STR;
  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);
  if (color != NULL && *color) {
    gdk_rgba_parse (&rgba, color);
    widget = gtk_color_button_new_with_rgba (&rgba);
  } else {
    widget = gtk_color_button_new ();
  }
  uiWidgetSetMarginStartW (widget, uiBaseMarginSz * 4);
  uiBoxPackStartUW (&hbox, widget);
  uiBoxPackStart (boxp, &hbox);
  gui->uiitem [widx].widget = widget;
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;
  logProcEnd (LOG_PROC, "confuiMakeItemColorButton", "");
}

void
confuiMakeItemSpinboxText (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    UIWidget *sgB, char *txt, int widx, int bdjoptIdx,
    confuiouttype_t outtype, ssize_t value, void *cb)
{
  UIWidget    hbox;
  UIWidget    *uiwidgetp;
  nlist_t     *list;
  nlist_t     *keylist;
  size_t      maxWidth;

  logProcBegin (LOG_PROC, "confuiMakeItemSpinboxText");

  gui->uiitem [widx].basetype = CONFUI_SPINBOX_TEXT;
  gui->uiitem [widx].outtype = outtype;
  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);
  uiSpinboxTextCreate (gui->uiitem [widx].spinbox, gui);
  list = gui->uiitem [widx].displist;
  keylist = gui->uiitem [widx].sbkeylist;
  if (outtype == CONFUI_OUT_STR) {
    keylist = NULL;
  }
  maxWidth = 0;
  if (list != NULL) {
    nlistidx_t    iteridx;
    nlistidx_t    key;
    char          *val;

    nlistStartIterator (list, &iteridx);
    while ((key = nlistIterateKey (list, &iteridx)) >= 0) {
      size_t      len;

      val = nlistGetStr (list, key);
      len = strlen (val);
      maxWidth = len > maxWidth ? len : maxWidth;
    }
  }

  uiSpinboxTextSet (gui->uiitem [widx].spinbox, 0,
      nlistGetCount (list), maxWidth, list, keylist, NULL);
  uiSpinboxTextSetValue (gui->uiitem [widx].spinbox, value);
  uiwidgetp = uiSpinboxGetUIWidget (gui->uiitem [widx].spinbox);
  uiWidgetSetMarginStart (uiwidgetp, uiBaseMarginSz * 4);
  if (sgB != NULL) {
    uiSizeGroupAdd (sgB, uiwidgetp);
  }
  uiBoxPackStart (&hbox, uiwidgetp);
  uiBoxPackStart (boxp, &hbox);

  uiutilsUIWidgetCopy (&gui->uiitem [widx].uiwidget, uiwidgetp);
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;

  if (cb != NULL) {
    uiutilsUICallbackInit (&gui->uiitem [widx].callback, cb, gui);
    uiSpinboxTextSetValueChangedCallback (gui->uiitem [widx].spinbox,
        &gui->uiitem [widx].callback);
  }
  logProcEnd (LOG_PROC, "confuiMakeItemSpinboxText", "");
}

void
confuiMakeItemSpinboxTime (confuigui_t *gui, UIWidget *boxp,
    UIWidget *sg, UIWidget *sgB, char *txt, int widx,
    int bdjoptIdx, ssize_t value)
{
  UIWidget    hbox;
  UIWidget    *uiwidgetp;

  logProcBegin (LOG_PROC, "confuiMakeItemSpinboxTime");

  gui->uiitem [widx].basetype = CONFUI_SPINBOX_TIME;
  gui->uiitem [widx].outtype = CONFUI_OUT_NUM;
  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);

  uiutilsUICallbackStrInit (&gui->uiitem [widx].callback,
      confuiValMSCallback, gui);
  uiSpinboxTimeCreate (gui->uiitem [widx].spinbox, gui,
      &gui->uiitem [widx].callback);
  uiSpinboxTimeSetValue (gui->uiitem [widx].spinbox, value);
  uiwidgetp = uiSpinboxGetUIWidget (gui->uiitem [widx].spinbox);
  uiutilsUIWidgetCopy (&gui->uiitem [widx].uiwidget, uiwidgetp);
  uiWidgetSetMarginStart (uiwidgetp, uiBaseMarginSz * 4);
  if (sgB != NULL) {
    uiSizeGroupAdd (sgB, uiwidgetp);
  }
  uiBoxPackStart (&hbox, uiwidgetp);
  uiBoxPackStart (boxp, &hbox);
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;
  logProcEnd (LOG_PROC, "confuiMakeItemSpinboxTime", "");
}

void
confuiMakeItemSpinboxNum (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    UIWidget *sgB, const char *txt, int widx, int bdjoptIdx,
    int min, int max, int value, void *cb)
{
  UIWidget    hbox;
  UIWidget    uiwidget;

  logProcBegin (LOG_PROC, "confuiMakeItemSpinboxNum");

  gui->uiitem [widx].basetype = CONFUI_SPINBOX_NUM;
  gui->uiitem [widx].outtype = CONFUI_OUT_NUM;
  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);
  uiSpinboxIntCreate (&uiwidget);
  uiSpinboxSet (&uiwidget, (double) min, (double) max);
  uiSpinboxSetValue (&uiwidget, (double) value);
  uiWidgetSetMarginStart (&uiwidget, uiBaseMarginSz * 4);
  if (sgB != NULL) {
    uiSizeGroupAdd (sgB, &uiwidget);
  }
  uiBoxPackStart (&hbox, &uiwidget);
  uiBoxPackStart (boxp, &hbox);
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;
  if (cb != NULL) {
    uiutilsUICallbackInit (&gui->uiitem [widx].callback, cb, gui);
    uiSpinboxSetValueChangedCallback (&uiwidget, &gui->uiitem [widx].callback);
  }
  uiutilsUIWidgetCopy (&gui->uiitem [widx].uiwidget, &uiwidget);
  logProcEnd (LOG_PROC, "confuiMakeItemSpinboxNum", "");
}

void
confuiMakeItemSpinboxDouble (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    UIWidget *sgB, char *txt, int widx, int bdjoptIdx,
    double min, double max, double value)
{
  UIWidget    hbox;
  UIWidget    uiwidget;

  logProcBegin (LOG_PROC, "confuiMakeItemSpinboxDouble");

  gui->uiitem [widx].basetype = CONFUI_SPINBOX_DOUBLE;
  gui->uiitem [widx].outtype = CONFUI_OUT_DOUBLE;
  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);
  uiSpinboxDoubleCreate (&uiwidget);
  uiSpinboxSet (&uiwidget, min, max);
  uiSpinboxSetValue (&uiwidget, value);
  uiWidgetSetMarginStart (&uiwidget, uiBaseMarginSz * 4);
  if (sgB != NULL) {
    uiSizeGroupAdd (sgB, &uiwidget);
  }
  uiBoxPackStart (&hbox, &uiwidget);
  uiBoxPackStart (boxp, &hbox);
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;
  uiutilsUIWidgetCopy (&gui->uiitem [widx].uiwidget, &uiwidget);
  logProcEnd (LOG_PROC, "confuiMakeItemSpinboxDouble", "");
}

void
confuiMakeItemSwitch (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    char *txt, int widx, int bdjoptIdx, int value, void *cb)
{
  UIWidget    hbox;
  UIWidget    *uiwidgetp;

  logProcBegin (LOG_PROC, "confuiMakeItemSwitch");

  gui->uiitem [widx].basetype = CONFUI_SWITCH;
  gui->uiitem [widx].outtype = CONFUI_OUT_BOOL;
  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);

  gui->uiitem [widx].uiswitch = uiCreateSwitch (value);
  uiwidgetp = uiSwitchGetUIWidget (gui->uiitem [widx].uiswitch);
  uiWidgetSetMarginStart (uiwidgetp, uiBaseMarginSz * 4);
  uiBoxPackStart (&hbox, uiwidgetp);
  uiBoxPackStart (boxp, &hbox);
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;

  if (cb != NULL) {
    uiutilsUICallbackInit (&gui->uiitem [widx].callback, cb, gui);
    uiSwitchSetCallback (gui->uiitem [widx].uiswitch,
        &gui->uiitem [widx].callback);
  }

  logProcEnd (LOG_PROC, "confuiMakeItemSwitch", "");
}

void
confuiMakeItemLabelDisp (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    char *txt, int widx, int bdjoptIdx)
{
  UIWidget    hbox;
  UIWidget    uiwidget;

  logProcBegin (LOG_PROC, "confuiMakeItemLabelDisp");

  gui->uiitem [widx].basetype = CONFUI_NONE;
  gui->uiitem [widx].outtype = CONFUI_OUT_NONE;
  uiCreateHorizBox (&hbox);
  confuiMakeItemLabel (&hbox, sg, txt);
  uiCreateLabel (&uiwidget, "");
  uiWidgetSetMarginStart (&uiwidget, uiBaseMarginSz * 4);
  uiBoxPackStart (&hbox, &uiwidget);
  uiBoxPackStart (boxp, &hbox);
  uiutilsUIWidgetCopy (&gui->uiitem [widx].uiwidget, &uiwidget);
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;
  logProcEnd (LOG_PROC, "confuiMakeItemLabelDisp", "");
}

void
confuiMakeItemCheckButton (confuigui_t *gui, UIWidget *boxp, UIWidget *sg,
    const char *txt, int widx, int bdjoptIdx, int value)
{
  UIWidget    uiwidget;

  logProcBegin (LOG_PROC, "confuiMakeItemCheckButton");

  gui->uiitem [widx].basetype = CONFUI_CHECK_BUTTON;
  gui->uiitem [widx].outtype = CONFUI_OUT_BOOL;
  uiCreateCheckButton (&uiwidget, txt, value);
  uiWidgetSetMarginStart (&uiwidget, uiBaseMarginSz * 4);
  uiBoxPackStart (boxp, &uiwidget);
  uiutilsUIWidgetCopy (&gui->uiitem [widx].uiwidget, &uiwidget);
  gui->uiitem [widx].bdjoptIdx = bdjoptIdx;
  logProcEnd (LOG_PROC, "confuiMakeItemCheckButton", "");
}

void
confuiMakeItemLabel (UIWidget *boxp, UIWidget *sg, const char *txt)
{
  UIWidget    uiwidget;

  logProcBegin (LOG_PROC, "confuiMakeItemLabel");
  if (*txt == '\0') {
    uiCreateLabel (&uiwidget, txt);
  } else {
    uiCreateColonLabel (&uiwidget, txt);
  }
  uiBoxPackStart (boxp, &uiwidget);
  uiSizeGroupAdd (sg, &uiwidget);
  logProcEnd (LOG_PROC, "confuiMakeItemLabel", "");
}

void
confuiSpinboxTextInitDataNum (confuigui_t *gui, char *tag, int widx, ...)
{
  va_list     valist;
  nlistidx_t  key;
  char        *disp;
  int         sbidx;
  nlist_t     *tlist;
  nlist_t     *llist;

  va_start (valist, widx);

  tlist = nlistAlloc (tag, LIST_ORDERED, free);
  llist = nlistAlloc (tag, LIST_ORDERED, free);
  sbidx = 0;
  while ((key = va_arg (valist, nlistidx_t)) != -1) {
    disp = va_arg (valist, char *);

    nlistSetStr (tlist, sbidx, disp);
    nlistSetNum (llist, sbidx, key);
    ++sbidx;
  }
  gui->uiitem [widx].displist = tlist;
  gui->uiitem [widx].sbkeylist = llist;

  va_end (valist);
}

/* internal routines */

static bool
confuiLinkCallback (void *udata)
{
  confuigui_t *gui = udata;
  char        *uri;
  char        tmp [200];
  int         widx = -1;

  if (gui->tablecurr == CONFUI_ID_MOBILE_MQ) {
    widx = CONFUI_WIDGET_MMQ_QR_CODE;
  }
  if (gui->tablecurr == CONFUI_ID_REM_CONTROL) {
    widx = CONFUI_WIDGET_RC_QR_CODE;
  }
  if (widx < 0) {
    return UICB_CONT;
  }

  uri = gui->uiitem [widx].uri;
  if (uri != NULL) {
    snprintf (tmp, sizeof (tmp), "open %s", uri);
    system (tmp);
    return UICB_STOP;
  }
  return UICB_CONT;
}


static long
confuiValMSCallback (void *udata, const char *txt)
{
  confuigui_t *gui = udata;
  const char  *valstr;
  char        tbuff [200];
  long        val;

  logProcBegin (LOG_PROC, "confuiValMSCallback");

  uiLabelSetText (&gui->statusMsg, "");
  valstr = validate (txt, VAL_MIN_SEC);
  if (valstr != NULL) {
    snprintf (tbuff, sizeof (tbuff), valstr, txt);
    uiLabelSetText (&gui->statusMsg, tbuff);
    return -1;
  }

  val = tmutilStrToMS (txt);
  logProcEnd (LOG_PROC, "confuiValMSCallback", "");
  return val;
}

