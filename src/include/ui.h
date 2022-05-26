#ifndef INC_UI_H
#define INC_UI_H

#include <stdbool.h>

#if BDJ4_USE_GTK
# include <gtk/gtk.h>
#endif

#include "nlist.h"
#include "slist.h"
#include "song.h"
#include "uiutils.h"

/* uigeneral.c */
/* general routines that are called by the ui specific code */
void uiutilsUIWidgetInit (UIWidget *uiwidget);
bool uiutilsCallbackHandler (UICallback *uicb);
bool uiutilsCallbackLongHandler (UICallback *uicb, long value);

/* uigtkdialog.c */

enum {
  RESPONSE_NONE,
  RESPONSE_DELETE_WIN,
  RESPONSE_CLOSE,
  RESPONSE_APPLY,
  RESPONSE_RESET,
};

typedef struct {
  char        *label;
  UIWidget    *window;
  const char  *startpath;
  const char  *mimefiltername;
  const char  *mimetype;
} uiselect_t;

char  *uiSelectDirDialog (uiselect_t *selectdata);
char  *uiSelectFileDialog (uiselect_t *selectdata);
void uiCreateDialog (UIWidget *uiwidget, UIWidget *window,
    UICallback *uicb, const char *title, ...);
void  uiDialogPackInDialog (UIWidget *uidialog, UIWidget *boxp);
void  uiDialogDestroy (UIWidget *uidialog);

/* uigtkmenu.c */
void uiCreateMenubar (UIWidget *uiwidget);
void uiCreateSubMenu (UIWidget *uimenuitem, UIWidget *uimenu);
void uiMenuCreateItem (UIWidget *uimenu, UIWidget *uimenuitem, const char *txt, UICallback *uicb);
void uiMenuCreateCheckbox (UIWidget *uimenu, UIWidget *uimenuitem,
    const char *txt, int active, UICallback *uicb);
void uiMenuInit (uimenu_t *menu);
void uiMenuAddMainItem (UIWidget *uimenubar, UIWidget *uimenuitem,
    uimenu_t *menu, const char *txt);
void uiMenuDisplay (uimenu_t *menu);
void uiMenuClear (uimenu_t *menu);

/* uigtklabel.c */
void  uiCreateLabel (UIWidget *uiwidget, const char *label);
void  uiCreateColonLabel (UIWidget *uiwidget, const char *label);
void  uiLabelSetColor (UIWidget *uilabel, const char *color);
void  uiLabelDarkenColor (UIWidget *uilabel, const char *color);
void  uiLabelSetFont (UIWidget *uilabel, const char *font);
void  uiLabelSetText (UIWidget *uilabel, const char *text);
void  uiLabelEllipsizeOn (UIWidget *uiwidget);
void  uiLabelSetMaxWidth (UIWidget *uiwidget, int width);
void  uiLabelAlignEnd (UIWidget *uiwidget);

/* uigtkbutton.c */
void uiCreateButton (UIWidget *uiwidget, UICallback *uicb, char *title, char *imagenm);
void uiButtonSetPressCallback (UIWidget *uiwidget, UICallback *uicb);
void uiButtonSetReleaseCallback (UIWidget *uiwidget, UICallback *uicb);
void uiButtonSetImage (UIWidget *uiwidget, const char *imagenm, const char *tooltip);
void uiButtonSetImageIcon (UIWidget *uiwidget, const char *nm);
void uiButtonAlignLeft (UIWidget *widget);

/* uigtkentry.c */
uientry_t *uiEntryInit (int entrySize, int maxSize);
void uiEntryFree (uientry_t *entry);
void uiEntryCreate (uientry_t *entry);
void uiEntrySetIcon (uientry_t *entry, const char *name);
void uiEntryClearIcon (uientry_t *entry);
UIWidget * uiEntryGetUIWidget (uientry_t *entry);
void uiEntryPeerBuffer (uientry_t *targetentry, uientry_t *sourceentry);
const char * uiEntryGetValue (uientry_t *entry);
void uiEntrySetValue (uientry_t *entry, const char *value);
void uiEntrySetColor (uientry_t *entry, const char *color);
void uiEntrySetValidate (uientry_t *entry,
    uiutilsentryval_t valfunc, void *udata);
int uiEntryValidate (uientry_t *entry, bool forceflag);
int uiEntryValidateDir (uientry_t *edata, void *udata);
int uiEntryValidateFile (uientry_t *edata, void *udata);
/* these routines will be removed at a later date */
GtkWidget * uiEntryGetWidget (uientry_t *entry);

/* uigtkspinbox.c */
uispinbox_t *uiSpinboxTextInit (void);
void  uiSpinboxTextFree (uispinbox_t *spinbox);
void  uiSpinboxTextCreate (uispinbox_t *spinbox, void *udata);
void  uiSpinboxTextSet (uispinbox_t *spinbox, int min, int count,
    int maxWidth, slist_t *list, nlist_t *keylist, uispinboxdisp_t textGetProc);
int   uiSpinboxTextGetIdx (uispinbox_t *spinbox);
int   uiSpinboxTextGetValue (uispinbox_t *spinbox);
void  uiSpinboxTextSetValue (uispinbox_t *spinbox, int ivalue);

void  uiSpinboxIntCreate (UIWidget *uiwidget);
void  uiSpinboxDoubleCreate (UIWidget *uiwidget);

uispinbox_t *uiSpinboxTimeInit (void);
void  uiSpinboxTimeFree (uispinbox_t *spinbox);
void  uiSpinboxTimeCreate (uispinbox_t *spinbox, void *udata, UICallback *convcb);
ssize_t uiSpinboxTimeGetValue (uispinbox_t *spinbox);
void  uiSpinboxTimeSetValue (uispinbox_t *spinbox, ssize_t value);

void  uiSpinboxSetRange (uispinbox_t *spinbox, long min, long max);
void  uiSpinboxWrap (uispinbox_t *spinbox);
void  uiSpinboxSet (UIWidget *uispinbox, double min, double max);
double uiSpinboxGetValue (UIWidget *uispinbox);
void  uiSpinboxSetValue (UIWidget *uispinbox, double ivalue);
bool  uiSpinboxIsChanged (uispinbox_t *spinbox);
void  uiSpinboxResetChanged (uispinbox_t *spinbox);
void  uiSpinboxAlignRight (uispinbox_t *spinbox);
void  uiSpinboxSetColor (uispinbox_t *spinbox, const char *color);
UIWidget * uiSpinboxGetUIWidget (uispinbox_t *spinbox);
void uiSpinboxSetValueChangedCallback (uispinbox_t *spinbox, UICallback *uicb);

/* these routines will be removed at a later date */
GtkWidget * uiSpinboxTextCreateW (uispinbox_t *spinbox, void *udata);
GtkWidget * uiSpinboxTimeCreateW (uispinbox_t *spinbox, void *udata, UICallback *convcb);
GtkWidget * uiSpinboxIntCreateW (void);
GtkWidget * uiSpinboxDoubleCreateW (void);
void    uiSpinboxSetW (GtkWidget *spinbox, double min, double max);
double uiSpinboxGetValueW (GtkWidget *spinbox);
void    uiSpinboxSetValueW (GtkWidget *spinbox, double ivalue);


/* uigtkdropdown.c */

void uiDropDownInit (uidropdown_t *dropdown);
void uiDropDownFree (uidropdown_t *dropdown);
GtkWidget * uiDropDownCreate (GtkWidget *parentwin,
    char *title, void *processSelectionCallback,
    uidropdown_t *dropdown, void *udata);
GtkWidget * uiComboboxCreate (GtkWidget *parentwin,
    char *title, void *processSelectionCallback,
    uidropdown_t *dropdown, void *udata);
nlistidx_t uiDropDownSelectionGet (uidropdown_t *dropdown, GtkTreePath *path);
void uiDropDownSetList (uidropdown_t *dropdown, slist_t *list,
    const char *selectLabel);
void uiDropDownSetNumList (uidropdown_t *dropdown, slist_t *list,
    const char *selectLabel);
void uiDropDownSelectionSetNum (uidropdown_t *dropdown, nlistidx_t idx);
void uiDropDownSelectionSetStr (uidropdown_t *dropdown, char *stridx);

/* uigtklink.c */
void uiCreateLink (UIWidget *uiwidget, char *label, char *uri);
void uiLinkSet (UIWidget *uilink, char *label, char *uri);
void uiLinkSetActivateCallback (UIWidget *uilink, UICallback *uicb);

/* uigtktextbox.c */
uitextbox_t  *uiTextBoxCreate (int height);
void  uiTextBoxFree (uitextbox_t *tb);
UIWidget * uiTextBoxGetScrolledWindow (uitextbox_t *tb);
char  *uiTextBoxGetValue (uitextbox_t *tb);
void  uiTextBoxSetReadonly (uitextbox_t *tb);
void  uiTextBoxScrollToEnd (uitextbox_t *tb);
void  uiTextBoxAppendStr (uitextbox_t *tb, const char *str);
void  uiTextBoxAppendBoldStr (uitextbox_t *tb, const char *str);
void  uiTextBoxSetValue (uitextbox_t *tb, const char *str);
void  uiTextBoxDarken (uitextbox_t *tb);
void  uiTextBoxHorizExpand (uitextbox_t *tb);
void  uiTextBoxVertExpand (uitextbox_t *tb);
void  uiTextBoxSetHeight (uitextbox_t *tb, int h);

/* uigtknotebook.c */
void  uiCreateNotebook (UIWidget *uiwidget);
void  uiNotebookTabPositionLeft (UIWidget *uiwidget);
void  uiNotebookAppendPage (UIWidget *uinotebook, UIWidget *uiwidget, UIWidget *uilabel);
void  uiNotebookSetActionWidget (UIWidget *uinotebook, UIWidget *uiwidget);
void  uiNotebookSetPage (UIWidget *uinotebook, int pagenum);
void  uiNotebookSetCallback (UIWidget *uinotebook, UICallback *uicb);
void  uiNotebookHideShowPage (UIWidget *uinotebook, int pagenum, bool show);

/* uigtkbox.c */
void uiCreateVertBox (UIWidget *uiwidget);
void uiCreateHorizBox (UIWidget *uiwidget);
void uiBoxPackInWindow (UIWidget *uiwindow, UIWidget *uibox);
void uiBoxPackStart (UIWidget *uibox, UIWidget *uiwidget);
void uiBoxPackStartExpand (UIWidget *uibox, UIWidget *uiwidget);
void uiBoxPackEnd (UIWidget *uibox, UIWidget *uiwidget);
/* these routines will be removed at a later date */
void uiBoxPackInWindowUW (UIWidget *uiwindow, GtkWidget *widget);
void uiBoxPackInWindowWU (GtkWidget *window, UIWidget *uibox);
void uiBoxPackStartUW (UIWidget *uibox, GtkWidget *widget);
void uiBoxPackStartExpandUW (UIWidget *uibox, GtkWidget *widget);
void uiBoxPackEndUW (UIWidget *uibox, GtkWidget *widget);
/* these routines will be removed at a later date */
void uiBoxPackStartWU (GtkWidget *box, UIWidget *uiwidget);
void uiBoxPackStartExpandWU (GtkWidget *box, UIWidget *uiwidget);
void uiBoxPackEndWU (GtkWidget *box, UIWidget *uiwidget);
/* these routines will be removed at a later date */
GtkWidget *uiCreateVertBoxWW (void);
GtkWidget *uiCreateHorizBoxWW (void);
void uiBoxPackInWindowWW (GtkWidget *window, GtkWidget *box);
void uiBoxPackStartWW (GtkWidget *box, GtkWidget *widget);
void uiBoxPackStartExpandWW (GtkWidget *box, GtkWidget *widget);
void uiBoxPackEndWW (GtkWidget *box, GtkWidget *widget);

/* uigtkpbar.c */
void uiCreateProgressBar (UIWidget *uiwidget, char *color);
void uiProgressBarSet (UIWidget *uipb, double val);

/* uigtktreeview.c */
GtkWidget * uiCreateTreeView (void);
GtkTreeViewColumn * uiAddDisplayColumns (GtkWidget *tree,
    slist_t *sellist, int col, int fontcol, int ellipsizeCol);
GType * uiAddDisplayTypes (GType *types, slist_t *sellist, int *col);
void  uiSetDisplayColumns (GtkListStore *store, GtkTreeIter *iter,
    slist_t *sellist, song_t *song, int col);
int   uiTreeViewGetSelection (GtkWidget *tree, GtkTreeModel **model, GtkTreeIter *iter);
void  uiTreeViewAllowMultiple (GtkWidget *tree);

/* uigtkwindow.c */
void uiCreateMainWindow (UIWidget *uiwidget, UICallback *uicb,
    const char *title, const char *imagenm);
void uiCloseWindow (UIWidget *uiwindow);
bool uiWindowIsMaximized (UIWidget *uiwindow);
void uiWindowIconify (UIWidget *uiwindow);
void uiWindowMaximize (UIWidget *uiwindow);
void uiWindowUnMaximize (UIWidget *uiwindow);
void uiWindowDisableDecorations (UIWidget *uiwindow);
void uiWindowEnableDecorations (UIWidget *uiwindow);
void uiWindowGetSize (UIWidget *uiwindow, int *x, int *y);
void uiWindowSetDefaultSize (UIWidget *uiwindow, int x, int y);
void uiWindowGetPosition (UIWidget *uiwindow, int *x, int *y);
void uiWindowMove (UIWidget *uiwindow, int x, int y);
void uiWindowNoFocusOnStartup (UIWidget *uiwindow);
void uiCreateScrolledWindow (UIWidget *uiwindow, int minheight);
void uiWindowSetDoubleClickCallback (UIWidget *uiwindow, UICallback *uicb);
void uiWindowSetWinStateCallback (UIWidget *uiwindow, UICallback *uicb);
void uiWindowNoDim (UIWidget *uiwidget);
void uiWindowSetMappedCallback (UIWidget *uiwidget, UICallback *uicb);
/* these routines will be removed at a later date */
GtkWidget * uiCreateMainWindowW (const char *title,
    const char *imagenm, void *cb, void *udata);
void uiCloseWindowW (GtkWidget *window);
bool uiWindowIsMaximizedW (GtkWidget *window);
void uiWindowIconifyW (GtkWidget *window);
void uiWindowMaximizeW (GtkWidget *window);
void uiWindowUnMaximizeW (GtkWidget *window);
void uiWindowDisableDecorationsW (GtkWidget *window);
void uiWindowEnableDecorationsW (GtkWidget *window);
void uiWindowGetSizeW (GtkWidget *window, int *x, int *y);
void uiWindowSetDefaultSizeW (GtkWidget *window, int x, int y);
void uiWindowGetPositionW (GtkWidget *window, int *x, int *y);
void uiWindowMoveW (GtkWidget *window, int x, int y);
void uiWindowNoFocusOnStartupW (GtkWidget *window);
GtkWidget * uiCreateScrolledWindowW (int minheight);

/* uigtkscale.c */
void    uiCreateScale (UIWidget *uiwidget, double lower, double upper,
    double stepinc, double pageinc, double initvalue);
void    uiScaleSetCallback (UIWidget *uiscale, UICallback *uicb);
double  uiScaleEnforceMax (UIWidget *uiscale, double value);
double  uiScaleGetValue (UIWidget *uiscale);
void    uiScaleSetValue (UIWidget *uiscale, double value);
void    uiScaleSetRange (UIWidget *uiscale, double start, double end);

/* uigtkswitch.c */

uiswitch_t *uiCreateSwitch (int value);
void uiSwitchFree (uiswitch_t *uiswitch);
void uiSwitchSetValue (uiswitch_t *uiswitch, int value);
int uiSwitchGetValue (uiswitch_t *uiswitch);
UIWidget *uiSwitchGetUIWidget (uiswitch_t *uiswitch);
void uiSwitchSetCallback (uiswitch_t *uiswitch, UICallback *uicb);

/* uigtksizegrp.c */
void uiCreateSizeGroupHoriz (UIWidget *);
void uiSizeGroupAdd (UIWidget *uiw, UIWidget *uiwidget);
/* these routines will be removed at a later date */
void uiSizeGroupAddW (UIWidget *uiw, GtkWidget *widget);

/* uigtkutils.c */
void  uiUIInitialize (void);
void  uiUIProcessEvents (void);
void  uiCleanup (void);
void  uiSetCss (GtkWidget *w, char *style);
void  uiSetUIFont (char *uifont);
void  uiInitUILog (void);
void  uiGetForegroundColor (GtkWidget *widget, char *buff, size_t sz);

/* uigtkwidget.c */
/* widget interface */
void  uiWidgetDisable (UIWidget *uiwidget);
void  uiWidgetEnable (UIWidget *uiwidget);
void  uiWidgetExpandHoriz (UIWidget *uiwidget);
void  uiWidgetExpandVert (UIWidget *uiwidget);
void  uiWidgetSetAllMargins (UIWidget *uiwidget, int margin);
void  uiWidgetSetMarginTop (UIWidget *uiwidget, int margin);
void  uiWidgetSetMarginStart (UIWidget *uiwidget, int margin);
void  uiWidgetSetMarginEnd (UIWidget *uiwidget, int margin);
void  uiWidgetAlignHorizFill (UIWidget *uiwidget);
void  uiWidgetAlignHorizStart (UIWidget *uiwidget);
void  uiWidgetAlignHorizEnd (UIWidget *uiwidget);
void  uiWidgetAlignVertFill (UIWidget *uiwidget);
void  uiWidgetAlignVertStart (UIWidget *uiwidget);
void  uiWidgetDisableFocus (UIWidget *uiwidget);
void  uiWidgetHide (UIWidget *uiwidget);
void  uiWidgetShow (UIWidget *uiwidget);
void  uiWidgetShowAll (UIWidget *uiwidget);
void  uiWidgetMakePersistent (UIWidget *uiuiwidget);
void  uiWidgetClearPersistent (UIWidget *uiuiwidget);
void  uiWidgetSetSizeRequest (UIWidget *uiuiwidget, int width, int height);
bool  uiWidgetIsValid (UIWidget *uiwidget);
/* these routines will be removed at a later date */
void  uiWidgetDisableW (GtkWidget *widget);
void  uiWidgetEnableW (GtkWidget *widget);
void  uiWidgetExpandHorizW (GtkWidget *widget);
void  uiWidgetExpandVertW (GtkWidget *widget);
void  uiWidgetSetAllMarginsW (GtkWidget *widget, int margin);
void  uiWidgetSetMarginTopW (GtkWidget *widget, int margin);
void  uiWidgetSetMarginStartW (GtkWidget *widget, int margin);
void  uiWidgetAlignHorizFillW (GtkWidget *widget);
void  uiWidgetAlignHorizStartW (GtkWidget *widget);
void  uiWidgetAlignHorizEndW (GtkWidget *widget);
void  uiWidgetAlignVertFillW (GtkWidget *widget);
void  uiWidgetAlignVertStartW (GtkWidget *widget);
void  uiWidgetDisableFocusW (GtkWidget *widget);
void  uiWidgetHideW (GtkWidget *widget);
void  uiWidgetShowW (GtkWidget *widget);
void  uiWidgetShowAllW (GtkWidget *widget);
void  uiWidgetMakePersistentW (UIWidget *uiwidget);
void  uiWidgetClearPersistentW (UIWidget *uiwidget);
void  uiWidgetSetSizeRequestW (UIWidget *uiwidget, int width, int height);

/* uigtkimage.c */
void  uiImageFromFile (UIWidget *uiwidget, const char *fn);
void  uiImagePersistentFromFile (UIWidget *uiwidget, const char *fn);
void  uiImagePersistentFree (UIWidget *uiwidget);

/* uigtktoggle.c */
void uiCreateCheckButton (UIWidget *uiwidget, const char *txt, int value);
void uiCreateToggleButton (UIWidget *uiwidget, const char *txt, const char *imgname,
    const char *tooltiptxt, UIWidget *image, int value);
void uiToggleButtonSetCallback (UIWidget *uiwidget, UICallback *uicb);
void uiToggleButtonSetImage (UIWidget *uiwidget, UIWidget *image);
bool uiToggleButtonIsActive (UIWidget *uiwidget);
void uiToggleButtonSetState (UIWidget *uiwidget, int state);

/* uigtkimage.c */
void uiImageNew (UIWidget *uiwidget);
void uiImageFromFile (UIWidget *uiwidget, const char *fn);
void uiImageScaledFromFile (UIWidget *uiwidget, const char *fn, int scale);
void uiImageClear (UIWidget *uiwidget);
void uiImageGetPixbuf (UIWidget *uiwidget);
void uiImageSetFromPixbuf (UIWidget *uiwidget, UIWidget *uipixbuf);

/* uigtksep.c */
void uiCreateHorizSeparator (UIWidget *uiwidget);
void uiSeparatorSetColor (UIWidget *uiwidget, const char *color);

#endif /* INC_UI_H */
