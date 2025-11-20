#include "misc.h"
#include "error.h"
#include "hash.h"
#include "ident.h"
#include "loader.h"
#include "mystrings.h"
#include "obj.h"
#include "packet.h"
#include "scanutils.h"
#include "slotaccess.h"
#include "utils.h"
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int cmd_history = 0;
int verbose = 0;

char strBuff[32];

/* Dynamic packet concatenation buffer.
 * Note: ViolaWWW is single-threaded (X11 event loop), so no mutex needed.
 * Initial size is conservative (1KB), grows exponentially as needed.
 */
static char* pkinfo_concat_buf = NULL;
static size_t pkinfo_concat_cap = 0;

typedef struct PkinfoOldBufNode {
    char* buf;
    struct PkinfoOldBufNode* next;
} PkinfoOldBufNode;

static PkinfoOldBufNode* pkinfo_old_buffers = NULL;

#define PKINFO_INITIAL_SIZE 1024
#define PKINFO_MAX_TMP_SIZE 128

static const char pkinfo_null_repr[] = "(NULL)";

static void remember_old_pkinfo_buffer(char* buf)
{
    if (!buf)
        return;

    PkinfoOldBufNode* node = malloc(sizeof(*node));
    if (!node) {
        fprintf(stderr, "PkInfos2Str: unable to track old buffer for cleanup\n");
        return;
    }
    node->buf = buf;
    node->next = pkinfo_old_buffers;
    pkinfo_old_buffers = node;
}

void cleanup_pkinfo_concat(void)
{
    free(pkinfo_concat_buf);
    pkinfo_concat_buf = NULL;

    while (pkinfo_old_buffers) {
        PkinfoOldBufNode* node = pkinfo_old_buffers;
        pkinfo_old_buffers = node->next;
        free(node->buf);
        free(node);
    }
    pkinfo_concat_cap = 0;
}

static int ensure_pkinfo_capacity(size_t required, size_t preserve_len)
{
    if (required <= pkinfo_concat_cap)
        return 1;

    /* Start small, grow exponentially (2x) for better amortized performance */
    size_t new_cap = pkinfo_concat_cap ? pkinfo_concat_cap * 2 : PKINFO_INITIAL_SIZE;
    while (new_cap < required) {
        /* Protect against integer overflow */
        if (new_cap > SIZE_MAX / 2) {
            fprintf(stderr, "PkInfos2Str: required size %zu exceeds maximum\n", required);
            return 0;
        }
        new_cap *= 2;
    }

    char* new_buf = malloc(new_cap);
    if (!new_buf) {
        fprintf(stderr, "PkInfos2Str: failed to grow buffer to %zu bytes\n", new_cap);
        return 0;
    }

    if (pkinfo_concat_buf) {
        size_t copy_len = preserve_len;
        if (copy_len >= pkinfo_concat_cap && pkinfo_concat_cap > 0)
            copy_len = pkinfo_concat_cap - 1;
        if (copy_len > 0) {
            memmove(new_buf, pkinfo_concat_buf, copy_len);
            new_buf[copy_len] = pkinfo_concat_buf[copy_len];
        } else {
            new_buf[0] = pkinfo_concat_buf[0];
        }
        remember_old_pkinfo_buffer(pkinfo_concat_buf);
    } else {
        new_buf[0] = '\0';
    }

    pkinfo_concat_buf = new_buf;
    pkinfo_concat_cap = new_cap;
    return 1;
}

static inline int append_chunk(const char* chunk, size_t len, size_t* used)
{
    if (!chunk || len == 0)
        return 1;

    /* Protect against integer overflow in size calculation */
    if (*used > SIZE_MAX - len - 1) {
        fprintf(stderr, "PkInfos2Str: concatenation size overflow\n");
        return 0;
    }

    size_t needed = *used + len + 1;
    if (!ensure_pkinfo_capacity(needed, *used))
        return 0;

    memmove(pkinfo_concat_buf + *used, chunk, len);
    *used += len;
    pkinfo_concat_buf[*used] = '\0';
    return 1;
}

/*
 * parse the string and set the numbers onto the array.
 *
 * RETURN: number of transfered.
 */
int transferNumList2Array(char* numStr, int* array, int n)
{
    int count = 0;
    int stri = 0;
    int ai = 0;

    // make sure to start out with number
    while (*numStr)
        if (isdigit(*numStr))
            break;

    for (;;) {
        if (isdigit(*numStr)) {
            strBuff[stri++] = *numStr;
        } else {
            if (stri > 0) {
                strBuff[stri] = '\0';
                stri = 0;
                array[ai++] = atoi(strBuff);
                if (++count >= n)
                    return count;
            }
        }
        if (*numStr == '\0')
            break;
        ++numStr;
    }
    return count;
}

int argNumsToInt(Packet argv[], int n, int intBuff[])
{
    for (int i = 0; i < n; i++) {
        switch (argv[i].type) {
        case PKT_FLT:
            intBuff[i] = (int)(argv[i].info.f);
            break;
        case PKT_STR:
            intBuff[i] = atoi(argv[i].info.s);
            break;
        case PKT_CHR:
            intBuff[i] = (int)(argv[i].info.c);
            break;
        case PKT_INT:
        default:
            intBuff[i] = argv[i].info.i;
            break;
        }
    }
    return n;
}

VObjList* strOListToOList(char* str)
{
    char name[1024];
    int i = 0;
    VObj* obj;
    VObjList* objl = NULL;

    for (;;) {
        while (isspace((unsigned char)str[i]))
            i++;
        i = NextWord(str, i, name, sizeof(name));
        if (AllBlank(name))
            break;
        obj = findObject(getIdent(name));
        if (obj)
            objl = appendObjToList(objl, obj);
    }
    return objl;
}

/* XXX Sins: uses global buff, 2xstrcat()...
 */
char* OListToStr(VObjList* olist)
{
    buff[0] = '\0';

    for (; olist; olist = olist->next) {
        strcat(buff, GET_name(olist->o));
        strcat(buff, " ");
    }
    return buff;
}

/* XXX Sins: uses global buff, 2xstrcat()...
 */
char* OListToStrPlusSuffix(VObjList* olist, char* suffix)
{
    buff[0] = '\0';

    for (; olist; olist = olist->next) {
        if (olist->o) {
            strcat(buff, GET_name(olist->o));
            strcat(buff, suffix);
            strcat(buff, " ");
        }
    }
    return buff;
}

/*
void setDepentShownInfo(VObj * self, int attrStrID, int position, int size)
{
        VObjList *objl;

        for (objl = GET__shownDepend(self); objl; objl = objl->next)
                if (objl->o)
                        sendMessage1N2int(objl->o, attrStrID, position, size);
}

void setNotifyShownInfo(VObj * self, int attrStrID, int position, int size)
{
        VObjList *objl;

        for (objl = GET__shownNotify(self); objl; objl = objl->next)
                if (objl->o)
                        sendMessage1N2int(objl->o, attrStrID, position, size);
}

*/

int makeArgv(char* argv[], char* argline)
{
    int argc = 0, i = 0, j = 0;

    do {
        j = i;
        i = NextWord(argline, i, buff, BUFF_SIZE);
        if (*buff)
            argv[argc++] = SaveString(buff);
    } while (i != j);
    return argc;
}

/*
 * NOTE: the returned string is stored in shared buffer space.
 */
char* PkInfos2Str(int argc, Packet argv[])
{
    size_t used = 0;
    char tmp[PKINFO_MAX_TMP_SIZE];

    if (!ensure_pkinfo_capacity(1, 0)) {
        buff[0] = '\0';
        return buff;
    }
    pkinfo_concat_buf[0] = '\0';

    for (int i = 0; i < argc; i++) {
        Packet* pk = &argv[i];

        switch (pk->type) {
        case PKT_STR:
            if (pk->info.s && !append_chunk(pk->info.s, strlen(pk->info.s), &used))
                goto fail;
            break;
        case PKT_CHR:
            tmp[0] = pk->info.c;
            if (!append_chunk(tmp, 1, &used))
                goto fail;
            break;
        case PKT_INT: {
            int len = snprintf(tmp, sizeof(tmp), "%ld", pk->info.i);
            if (len < 0 || (size_t)len >= sizeof(tmp))
                goto fail;
            if (!append_chunk(tmp, (size_t)len, &used))
                goto fail;
            break;
        }
        case PKT_FLT: {
            int len = snprintf(tmp, sizeof(tmp), "%f", pk->info.f);
            if (len < 0 || (size_t)len >= sizeof(tmp))
                goto fail;
            if (!append_chunk(tmp, (size_t)len, &used))
                goto fail;
            break;
        }
        case PKT_OBJ:
            if (pk->info.o) {
                const char* name = GET_name(pk->info.o);
                if (name && !append_chunk(name, strlen(name), &used))
                    goto fail;
            } else {
                if (!append_chunk(pkinfo_null_repr, sizeof(pkinfo_null_repr) - 1, &used))
                    goto fail;
            }
            break;
        case PKT_ARY:
            if (pk->info.y && pk->info.y->info && pk->info.y->size > 0) {
                Array* array = pk->info.y;
                for (int n = 0; n < array->size; n++) {
                    int len = snprintf(tmp, sizeof(tmp), "%ld", array->info[n]);
                    if (len < 0 || (size_t)len >= sizeof(tmp))
                        goto fail;
                    if (!append_chunk(tmp, (size_t)len, &used))
                        goto fail;
                    if (n < array->size - 1) {
                        if (!append_chunk(" ", 1, &used))
                            goto fail;
                    }
                }
            }
            break;
        default:
            if (!append_chunk("?", 1, &used))
                goto fail;
            break;
        }
    }
    return pkinfo_concat_buf;

fail:
    buff[0] = '\0';
    return buff;
}

/*
 * NOTE: the returned string is stored in shared buffer space.
 */
char* PkInfo2Str(Packet* pk)
{
    switch (pk->type) {
    case PKT_STR:
        return pk->info.s;
    case PKT_CHR:
        sprintf(buff, "%c", pk->info.c);
        break;
    case PKT_INT:
        sprintf(buff, "%ld", pk->info.i);
        break;
    case PKT_FLT:
        sprintf(buff, "%f", pk->info.f);
        break;
    case PKT_OBJ:
        if (pk->info.o)
            sprintf(buff, "%s", GET_name(pk->info.o));
        else
            sprintf(buff, "");
        break;
    case PKT_ARY:
        if (pk->info.y) {
            Array* array = pk->info.y;
            for (int n = 0; n < array->size; n++)
                sprintf(buff, "%ld ", array->info[n]);
        }
        break;
    default:
        buff[0] = '\0';
        break;
    }
    return buff;
}

float PkInfo2Flt(Packet* pk)
{
    switch (pk->type) {
    case PKT_FLT:
        return pk->info.f;
    case PKT_INT:
        return (float)(pk->info.i);
    case PKT_STR:
        return (float)atof(pk->info.s);
    case PKT_CHR:
        return (float)(pk->info.c);
    default:
        return (float)(pk->info.f);
    }
}

char PkInfo2Char(Packet* pk)
{
    switch (pk->type) {
    case PKT_CHR:
        return pk->info.c;
    case PKT_STR:
        return pk->info.s[0];
    case PKT_INT:
        return (char)(pk->info.i); /* ?? */
    case PKT_FLT:
        return (char)(pk->info.f);
    default:
        return (char)pk->info.c;
    }
}

long PkInfo2Int(Packet* pk) {
    switch (pk->type) {
    case PKT_INT:
        return pk->info.i;
    case PKT_STR:
        if (pk->info.s) {
            return atoi(pk->info.s);
        } else {
            fprintf(stderr, "warning: PkInfo2Int() pk->info.s == NULL\n");
            return 0;
        }
    case PKT_FLT:
        return (long)(pk->info.f);
    case PKT_CHR:
        return (long)(pk->info.c); /* ?? */
    }
    return pk->info.i;
}

/* load if necessary
 */
VObj* getObject(char* objName)
{
    int symID;
    VObj* obj = NULL;

    symID = getIdent(trimEdgeSpaces(objName));
    if (symID)
        obj = findObject(symID);
    if (obj) {
        return obj;
    } else {
        char *fname, *objname;
        size_t length;
        HashEntry* entry;

        length = sizeof(char) * (strlen(objName) + 3);
        fname = (char*)malloc(length);
        objname = (char*)malloc(length);
        strcpy(fname, objName);
        strcpy(objname, objName);
        length = strlen(fname);
        if (length >= 2) {
            if (fname[length - 2] != '.' || fname[length - 1] != 'v')
                strcat(fname, ".v");
        }
        load_object(fname, NULL);
        entry = objID2Obj->get(objID2Obj, storeIdent(objname));
        free(fname);
        if (entry)
            return (VObj*)entry->val;
    }
    return NULL;
}

VObj* PkInfo2Obj(Packet* pk)
{
    VObj* obj = NULL;

    if (pk->type == PKT_OBJ) {
        return pk->info.o;
    } else if (pk->type == PKT_STR) {
        if (pk->info.s)
            return getObject(pk->info.s);
    }
    return NULL;
}
