/*
 * history_api.h
 *
 * Minimal API for URL history access.
 * This header avoids Motif dependencies so it can be used in standalone viola.
 */

#ifndef _HISTORY_API_H_
#define _HISTORY_API_H_

#include "../vw/box.h"

/* Global list of document views (NULL in standalone viola) */
extern Box* docViews;

/* Check if URL is in browsing history. Returns 1 if visited, 0 otherwise.
 * In standalone viola, this checks a persistent file-based history.
 * dvi parameter is opaque (DocViewInfo* in vw, ignored in standalone).
 */
int isURLInHistory(void* dvi, char* url);

/* Add URL to history (for standalone viola persistence) */
void addURLToStandaloneHistory(const char* url);

#endif /* _HISTORY_API_H_ */

