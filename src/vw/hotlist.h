/*
 * hotlist.h
 *
 * Routines and data structures for handling document traversal hotlist.
 *
 */
/*
 * Copyright 1993 O'Reilly & Associates. All rights reserved.
 *
 * Permission to use, copy, and/or distribute for any purpose and
 * without fee is hereby granted, provided that both the above copyright
 * notice and this permission notice appear in all copies and derived works.
 * Fees for distribution or use of this software or derived works may only
 * be charged with express written permission of the copyright holder.
 * This software is provided ``as is'' without express or implied warranty.
 */

#ifndef _HOTLIST_H_
#define _HOTLIST_H_

/* CONSTANTS */
#define HOTLIST_CHUNK 256

/* PROTOTYPES */
void hotlistPrev(DocViewInfo* dvi);
void hotlistNext(DocViewInfo* dvi);
void hotlistBackUp(DocViewInfo* dvi);

void hotlistPrevMH(char* arg[], int argc, void* clientData);
void hotlistNextMH(char* arg[], int argc, void* clientData);
void hotlistBackUpMH(char* arg[], int argc, void* clientData);

void hotlistAdd(DocViewInfo* dvi, char* newItem);
void hotlistAddMH(char* arg[], int argc, void* clientData);
void setHotlistList(DocViewInfo* dvi, char* newList[], int numItems);
void setHotlistListMH(char* arg[], int argc, void* clientData);

void freeHotlistList(DocViewInfo* dvi);
void growHotlistList(DocViewInfo* dvi);

Widget createHotlistDialog(DocViewInfo* dvi);
void showHotlist(DocViewInfo* dvi);
void showHotlistCB(Widget widget, XtPointer clientData, XtPointer callData);
void hideHotlist(Widget button, XtPointer clientData, XtPointer callData);
void hotlistSelect(DocViewInfo* dvi, char* url);
void hotlistSelectCB(Widget list, XtPointer clientData, XtPointer callData);

void addToHotlist(DocViewInfo* dvi);
void addToHotlistCB(Widget widget, XtPointer clientData, XtPointer callData);

void addHotlistItem(Widget button, XtPointer clientData, XtPointer callData);
void deleteHotlistItem(Widget button, XtPointer clientData, XtPointer callData);
void gotoHotlistItem(Widget button, XtPointer clientData, XtPointer callData);
void editHotlistItem(Widget button, XtPointer clientData, XtPointer callData);

#endif /* _HOTLIST_H_ */
