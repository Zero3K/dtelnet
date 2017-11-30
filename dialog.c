/* dialog.c
 * Copyright (C) 1998 David Cole
 *
 * Dialog box message routing.
 *
 * Instead of getting the main message loop to remember which dialogs
 * are current, dialogs are registered here when they are created, and
 * unregistered when destroyed.  This module then handles the routing
 * of messages to all registered dialogs.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "dialog.h"

/* Keep all registered dialogs in a linked list
 */
typedef struct Dialog Dialog;
struct Dialog {
    Dialog* next;		/* next dialog in list */
    HWND dlg;			/* handle of registered dialog */
};

static Dialog* dialogs;		/* all registered dialogs */

/* Register a dialog box with the message router
 *
 * Args:
 * dlg - handle of dialog box to be registered
 */
void dialogRegister(HWND dlg)
{
    Dialog* dialog = (Dialog *)xmalloc(sizeof(*dialog));
    memset(dialog, 0, sizeof(*dialog));
    dialog->dlg = dlg;

    dialog->next = dialogs;
    dialogs = dialog;
}

/* Unregister a dialog box with the message router
 *
 * Args:
 * dlg - handle of dialog box to be unregistered
 */
void dialogUnRegister(HWND dlg)
{
    Dialog* dialog = dialogs;
    Dialog* previous = NULL;

    while (dialog != NULL) {
	if (dialog->dlg == dlg) {
	    if (previous == NULL)
		dialogs = dialog->next;
	    else
		previous->next = dialog->next;
	    free(dialog);
	    return;
	}
	previous = dialog;
	dialog = dialog->next;
    }
}

/* Return whether or not a message was routed to a registered dialog
 *
 * Args:
 * msg - pointer to a windows message
 *
 * Returns TRUE if the message was routed to a registered dialog
 */
BOOL dialogCheckMessage(MSG* msg)
{
    Dialog* dialog = dialogs;	/* iterate over all registered dialogs */
    while (dialog != NULL) {
	if (IsDialogMessage(dialog->dlg, msg))
	    /* Message was routed to dialog
	     */
	    return TRUE;
	dialog = dialog->next;
    }

    /* Message was not routed to any dialog
     */
    return FALSE;
}
