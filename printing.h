/* printing.h
 * 20010615 Mark Melvin
 *
 * dtelnet printing support
 */

#ifndef __printing_h
#define __printing_h

/* Display the printer preferences dialog */
void printingPrefDialog(void);
/* Initiate printing */
void printingStart(void);
/* Stop printing */
void printingStop(void);
/* Return printing status */
BOOL printingIsActive(void);

/* Insert a char into printing buffer */
void printingAddChar(char c);

/* do a Screen Dump to printer */
void printingPrintScreen(void);

/* Load logging parameters from the .INI file */
void printingGetProfile(void);
/* Save logging parameters to the .INI file */
void printingSaveProfile(void);

#endif
