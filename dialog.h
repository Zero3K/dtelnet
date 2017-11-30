/* dialog.h
 * Copyright (C) 1998 David Cole
 *
 * Dialog box message routing
 */
#ifndef __dialog_h
#define __dialog_h

/* Register a dialog box with the message router */
void dialogRegister(HWND dlg);
/* Unregister a dialog box with the message router */
void dialogUnRegister(HWND dlg);
/* Return whether or not a message was routed to a dialog */
BOOL dialogCheckMessage(MSG* msg);

#endif
