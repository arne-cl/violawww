/*
 * Sound Library
 *
 * well, what can I say... we're just not there yet.
 */
#include "utils.h"
#include <unistd.h>
#include "mystrings.h"
#include "error.h"
#include "file.h"
#include "hash.h"
#include "ident.h"
#include "obj.h"
#include "slotaccess.h"
#include "glib.h"

int SLBellVolume(int percent)
{
	XBell(display, percent);
	return percent;
}

int SLBell(void)
{
	/* Use X11 bell instead of writing to stdout */
	XBell(display, 0);  /* 0 = default volume */
	XFlush(display);    /* Ensure the bell is sent immediately */
	return 0;
}

