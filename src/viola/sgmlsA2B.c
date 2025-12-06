/*
 * sgmlsA2B.c
 *
 * usage: sgmlsA2B DTDname [sdecl] [file]
 *
 *    ie: sgmlsA2B HTML sdecl test.html
 *    ie: sgmlsA2B HMML test.hmml
 *
 * encodeFromASCIIToBinary(sgmls(SGML file))
 */
#include "mystrings.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

/*
 * Search paths for the SGML parser (onsgmls from OpenSP package).
 * The program will first look for onsgmls in the same directory as sgmlsA2B,
 * then fall back to standard paths.
 * 
 * On macOS with Homebrew: brew install open-sp
 * On Linux: apt-get install opensp
 */
static const char *onsgmls_search_paths[] = {
    "/opt/homebrew/bin/onsgmls",     /* macOS Homebrew (Apple Silicon) */
    "/usr/local/bin/onsgmls",        /* macOS Homebrew (Intel) / Linux local */
    "/usr/bin/onsgmls",              /* Linux system */
    "/opt/local/bin/onsgmls",        /* MacPorts */
    NULL
};

/* Buffer to hold the discovered onsgmls path */
static char onsgmls_path[4096] = "";

/*
 * Find onsgmls executable.
 * First checks the same directory as this executable (for app bundles),
 * then searches standard paths.
 */
static const char* find_onsgmls(void) {
    char exe_path[4096];
    char exe_dir[4096];
    char candidate[4096];
    
    if (onsgmls_path[0] != '\0') {
        return onsgmls_path;
    }
    
#ifdef __APPLE__
    /* Get path to this executable */
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        /* Get directory containing the executable */
        char *dir = dirname(exe_path);
        strncpy(exe_dir, dir, sizeof(exe_dir) - 1);
        exe_dir[sizeof(exe_dir) - 1] = '\0';
        
        /* Check for onsgmls in the same directory */
        snprintf(candidate, sizeof(candidate), "%s/onsgmls", exe_dir);
        if (access(candidate, X_OK) == 0) {
            strncpy(onsgmls_path, candidate, sizeof(onsgmls_path) - 1);
            return onsgmls_path;
        }
    }
#else
    /* Linux: use /proc/self/exe */
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        char *dir = dirname(exe_path);
        strncpy(exe_dir, dir, sizeof(exe_dir) - 1);
        exe_dir[sizeof(exe_dir) - 1] = '\0';
        
        snprintf(candidate, sizeof(candidate), "%s/onsgmls", exe_dir);
        if (access(candidate, X_OK) == 0) {
            strncpy(onsgmls_path, candidate, sizeof(onsgmls_path) - 1);
            return onsgmls_path;
        }
    }
#endif
    
    /* Search standard paths */
    for (int i = 0; onsgmls_search_paths[i] != NULL; i++) {
        if (access(onsgmls_search_paths[i], X_OK) == 0) {
            strncpy(onsgmls_path, onsgmls_search_paths[i], sizeof(onsgmls_path) - 1);
            return onsgmls_path;
        }
    }
    
    /* Not found - return first path (will fail with clear error) */
    return onsgmls_search_paths[0];
}

static int verbose = 0;

#define ATTR_STACK_SIZE 512
static char* attrsStack[ATTR_STACK_SIZE];

#define TAGBUFFSIZE 1024
static char* tagDict[TAGBUFFSIZE];
static int tagDictCount = 0;

static const int version = 2;

#define MAX_LINE_SIZE 100000

static const char* lspaces = "                                                        ";

/*
        version info in ASCII\n
        0(4b)
        byteOrderHint(4b)
        versionID(4b)
        DTDNameSize(4b)	DTDName			(added in version 2)
        TAGDICT(b)	tagCount(4b)
        [		tagID(4b)	size(4b) 	tagName
        ]...
        [
                        TAG(b)	tagID(4b)
                [	NAME(b)	size(4b)	name	]
                [	REF(b)	size(4b)	ref	]
                        DATA(b)	size(4b)	data
                [	GEOM(b) width(4b) height(4b)	]
                        END
        ]...


NEW VERSION, DUMBER BUT MORE GENERALIZED

        version info in ASCII\n
        0(4b)
        byteOrderHint(4b)
        versionID(4b)
        DTDNameSize(4b)	DTDName			(added in version 2)
        TAGDICT(b)	tagCount(4b)
        [		tagID(4b)	size(4b) 	tagName
        ]...
        [
                        TAG(b)	tagID(4b)
                        [ATTR(b) type(b) size(4b) attrName size(4b) attrValue
                         ...]
                        DATA(b)	size(4b)	data
                        END
        ]...

*/
/*		[	GEOM(b) width(2b) height(2b)	]*/

#define TOKEN_TAGDICT 1
#define TOKEN_TAG 2
#define TOKEN_END 3
#define TOKEN_ATTR 4
#define TOKEN_DATA 5

enum sgmlsAttributeTypes {
    SGMLS_ATTR_IMPLIED,
    SGMLS_ATTR_CDATA,
    SGMLS_ATTR_TOKEN,
    SGMLS_ATTR_ID,
    SGMLS_ATTR_IDREF,
    SGMLS_ATTR_ENTITY,
    SGMLS_ATTR_NOTATION
};

static inline void emitToken(int i)
{
    putchar(i & 255);
}

static inline void emitStr(const char* s, int n)
{
    fwrite(s, 1, (size_t)n, stdout);
}

static inline void emitInt(int i)
{
    putchar((i >> 24) & 255);
    putchar((i >> 16) & 255);
    putchar((i >> 8) & 255);
    putchar(i & 255);
}

static char* filterCtrl(const char* inStr, int* size)
{
    char c, *cp, *outStr;

    outStr = cp = (char*)malloc(sizeof(char) * (strlen(inStr) + 1));

    while ((c = *inStr++)) {
        if (c == '\\') {
            switch (c = *inStr++) {
            case 'n':
                *cp++ = '\n'; /* newline */
                continue;
            case 't':
                *cp++ = '\t'; /* horizontal tab */
                continue;
            case 'b':
                *cp++ = '\b'; /* backspace */
                continue;
            case 'r':
                *cp++ = '\r'; /* carriage */
                continue;
            case 'f':
                *cp++ = '\f'; /* form feed */
                continue;
            case '\\':
                *cp++ = '\\'; /* backslash */
                continue;
            case '\'':
                *cp++ = '\''; /* single quote */
                continue;
            case '\"':
                *cp++ = '\"';
                continue;
            default:
                if (isdigit(c)) {
                    /* ie: "\011" ->
                     * ' ','\t',' ',' '
                     */
                    int i;
                    i = (int)(c - '0') * 64;
                    i += (int)(*inStr++ - '0') * 8;
                    i += (int)(*inStr++ - '0');

                    *cp = (char)i;

                    ++cp;
                    *cp = ' ';
                    ++cp;
                    *cp = ' ';
                    ++cp;
                } else {
                    *cp++ = '\\';
                    *cp++ = c;
                }
                continue;
            }
        } else {
            *cp++ = c;
        }
    }
    *cp = '\0';

    *size = (int)(cp - outStr);

    return outStr;
}

static int findTagID(const char* tagName)
{
    int i;

    for (i = 0; i < tagDictCount; i++) {
        if (!strcmp(tagDict[i], tagName))
            return i;
    }
    return -1; /* error! */
}

static int buildDict(const char* tag, char** srcp, int level)
{
    char tagName[100];
    char line[MAX_LINE_SIZE];
    char* end;
    int i;
    bool endP = false;

    do {
        end = strchr(*srcp, '\n');
        if (!end) {
            end = strchr(*srcp, '\0');
            strncpy(line, *srcp, end - *srcp);
            line[end - *srcp] = '\0';
            *srcp = end;
            endP = true;

        } else {
            strncpy(line, *srcp, end - *srcp);
            line[end - *srcp] = '\0';
            *srcp = end + 1;
        }

        if (line[0] == '(') {
            snprintf(tagName, sizeof(tagName), "%s", line + 1);

            for (i = 0; i < tagDictCount; i++) {
                if (!strcmp(tagDict[i], tagName))
                    break;
            }
            if (i >= tagDictCount) {
                tagDict[tagDictCount] = saveString(tagName);
                ++tagDictCount;
            }
            buildDict(tagName, srcp, level + 1);
        } else if (line[0] == ')') {
            return 1;
        }
    } while (!endP);

    return 1;
}

static int build(const char* tag, char** srcp, int level, int attrsIdxBegin, int attrsCount)
{
    char tagName[100];
    char line[MAX_LINE_SIZE];
    char* end;
    int i, j;
    bool endP = false;
    int subAttrsIdxBegin = attrsIdxBegin + attrsCount;
    int subAttrsCount = 0;
    char c, *s, *datap;
    int tagID;
    char* cp;
    int ai, si, len;
    int attrTypeID;
    char attrName[128];
    char attrType[32];
    char attrValue[500];

    /*	printf("%s", lspaces + strlen(lspaces) - (level * 4));*/
    /*	fprintf(stderr, "######## TAG=\"%s\"\n", tag);
     */

    tagID = findTagID(tag);
    if (tagID != -1) {
        emitToken(TOKEN_TAG);
        emitInt(tagID);
    }

    for (i = attrsIdxBegin, j = i + attrsCount; i < j; i++) {
        /*
                        fprintf(stderr,
                                "  a[%d]={%s}\n",
                                i,
                                attrsStack[i]);
        */
        cp = attrsStack[i];

        ai = 0;
        si = 1;
        while ((c = cp[si])) {
            if (c == ' ') {
                c = cp[si++];
                break;
            }
            if (c == '\0')
                break;
            attrName[ai++] = cp[si];
            ++si;
        }
        attrName[ai] = '\0';

        attrType[0] = '\0';
        if (c != '\0') {
            ai = 0;
            while ((c = cp[si])) {
                if (c == ' ') {
                    c = cp[si++];
                    break;
                }
                if (c == '\0')
                    break;
                attrType[ai++] = cp[si];
                ++si;
            }
            attrType[ai] = '\0';
        }

        attrValue[0] = '\0';
        if (c != '\0') {
            ai = 0;
            while ((c = cp[si])) {
                if (c == '\0')
                    break;
                attrValue[ai++] = cp[si];
                ++si;
            }
            attrValue[ai] = '\0';
        }

        if (!strcmp(attrType, "IMPLIED")) {
            attrTypeID = SGMLS_ATTR_IMPLIED;
        } else if (!strcmp(attrType, "CDATA")) {
            attrTypeID = SGMLS_ATTR_CDATA;
        } else if (!strcmp(attrType, "TOKEN")) {
            attrTypeID = SGMLS_ATTR_TOKEN;
        } else if (!strcmp(attrType, "ID")) {
            attrTypeID = SGMLS_ATTR_ID;
        } else if (!strcmp(attrType, "IDREF")) {
            attrTypeID = SGMLS_ATTR_IDREF;
        } else if (!strcmp(attrType, "ENTITY")) {
            attrTypeID = SGMLS_ATTR_ENTITY;
        } else if (!strcmp(attrType, "NOTATION")) {
            attrTypeID = SGMLS_ATTR_NOTATION;
        } else {
            attrTypeID = -1;
        }
        /*		fprintf(stderr,
                                ">> attr name={%s} type={%s}=%d value={%s}\n",
                                attrName, attrType, attrTypeID, attrValue);
        */
        emitToken(TOKEN_ATTR);
        emitToken(attrTypeID);
        len = (int)strlen(attrName);
        emitInt(len);
        emitStr(attrName, len);
        if (attrTypeID > 0) {
            len = (int)strlen(attrValue);
            emitInt(len);
            emitStr(attrValue, len);
        }
    }

    do {
        end = strchr(*srcp, '\n');
        if (!end) {
            end = strchr(*srcp, '\0');
            strncpy(line, *srcp, end - *srcp);
            line[end - *srcp] = '\0';
            *srcp = end;
            endP = true;

        } else {
            strncpy(line, *srcp, end - *srcp);
            line[end - *srcp] = '\0';
            *srcp = end + 1;
        }

        if (line[0] == '(') {
            snprintf(tagName, sizeof(tagName), "%s", line + 1);
            if (verbose)
                fprintf(stderr, "%s", lspaces + strlen(lspaces) - (level * 4));

            build(tagName, srcp, level + 1, subAttrsIdxBegin, subAttrsCount);

            subAttrsIdxBegin = attrsIdxBegin + attrsCount;
            subAttrsCount = 0;

        } else if (line[0] == ')') {

            if (verbose) {
                fprintf(stderr, "%s", lspaces + strlen(lspaces) - (level * 4));
                fprintf(stderr, "ETAG=\"%s\"\n", line + 1);
            }
            emitToken(TOKEN_END);
            return 1;
        } else if (line[0] == '-') {
            char* s;
            int size;

            if (verbose) {
                fprintf(stderr, "%s", lspaces + strlen(lspaces) - (level * 4));
                fprintf(stderr, "LINE=...\n");
                fprintf(stderr, "LINE=\"%s\"\n", line + 1);
            }
            s = filterCtrl(line + 1, &size);

            if (verbose) {
                fprintf(stderr, "DATA %d \n{%s}\n", size, s);
            }
            emitToken(TOKEN_DATA);
            emitInt(size);
            emitStr(s, size);
            free(s);

        } else if (line[0] == 'A' || line[0] == 'I') {
            datap = s = line;
            attrsStack[subAttrsIdxBegin + subAttrsCount] = saveString(datap);
            ++subAttrsCount;
            while (*s) {
                if (*s++ == '\n') {
                    *(s - 1) = '\0';
                    break;
                }
            }
            /*
                                    printf("   suba[%d]={%s}\n",
                                    subAttrsIdxBegin + subAttrsCount - 1,
                                    attrsStack[subAttrsIdxBegin + subAttrsCount - 1]);
            */
        } else {
            /* ignore */
            /*			printf("%s", lspaces + strlen(lspaces) - (level * 4));
                                    printf("?=\"%s\"\n", line);
            */
        }
    } while (!endP);

    return 1;
}

int main(int argc, char** argv)
{
    int c;
    FILE* fp;
    char *buffp, buff[200000];
    char cmd[256];
    char dtd[256];
    int i;
    char* sdecl = "";
    char* doc = "";

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage: %s DTDname [sdecl] file\n", argv[0]);
        exit(1);
    }
    snprintf(dtd, sizeof(dtd), "%s", argv[1]);
    if (argc == 3) {
        doc = argv[2];
    } else if (argc == 4) {
        sdecl = argv[2];
        doc = argv[3];
    } else {
        fprintf(stderr, "usage: %s DTDname [sdecl] file\n", argv[0]);
        exit(1);
    }
    snprintf(cmd, sizeof(cmd), "%s %s %s 2>/dev/null", find_onsgmls(), sdecl, doc);
    fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "popen() failed\n");
        exit(1);
    }

    buffp = buff;
    while ((c = fgetc(fp)) != EOF) {
        *buffp = (char)c;
        buffp++;
    }
    *buffp = '\0';
    pclose(fp);

    /*	printf(">>>%s<<<\n", buff);*/

    /* Header info goes to stderr, not stdout (stdout is binary data) */
    fprintf(stderr, "Binary SGMLS file format, version=%d\n", version);
    emitInt(0);
    emitInt(0x12345678);
    emitInt(version);

    if (version >= 2) {
        emitInt((int)strlen(dtd));
        printf("%s", dtd);
    }

    buffp = buff;
    buildDict("top", &buffp, 0);

    /*	printf("==== TAG Dict ======\n");*/

    emitToken(TOKEN_TAGDICT);
    emitInt(tagDictCount);

    for (i = 0; i < tagDictCount; i++) {
        /*		fprintf(stderr, "  %4d  size=%4d  %s\n",
                                i, strlen(tagDict[i]), tagDict[i]);
        */
        int tagLen = (int)strlen(tagDict[i]);
        emitInt(i);
        emitInt(tagLen);
        emitStr(tagDict[i], tagLen);
    }

    buffp = buff;
    build("top", &buffp, 0, 0, 0);

    return 0;
}
