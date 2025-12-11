/*
 * file.c
 */
/*
 * Copyright 1991 Pei-Yuan Wei.  All rights reserved.
 *
 * Permission to use, copy, and/or distribute for any purpose and
 * without fee is hereby granted, provided that both the above copyright
 * notice and this permission notice appear in all copies and derived works.
 * Fees for distribution or use of this software or derived works may only
 * be charged with express written permission of the copyright holder.
 * This software is provided ``as is'' without express or implied warranty.
 */
#include "file.h"
#include "mystrings.h"
#include "utils.h"
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <limits.h>
#include <unistd.h>

int io_stat;

char* vl_expandPath(char* restrict path, char* restrict buffer)
{
    if (!path)
        return NULL;
    if (path[0] == '~') {
        struct passwd* info;
        char userName[256];
        int i = 0;

        if (path[1] == '\0' || path[1] == '/') {
            char* cp = getlogin();

            /* assume path looks like: "~" "~/viola" */
            if (cp) {
                snprintf(userName, sizeof(userName), "%s", cp);
            } else {
                struct passwd* pwds;

                pwds = getpwuid(getuid());
                if (!pwds || !pwds->pw_name) {
                    fprintf(stderr, "failed to get user info\n");
                    return NULL;
                }
                snprintf(userName, sizeof(userName), "%s", pwds->pw_name);

                /* Find and truncate at ':' if present */
                char* colon = strchr(userName, ':');
                if (colon)
                    *colon = '\0';
            }
        } else {
            char c;
            /* ie: "~wei/viola" */

            /* get user's name, limit to buffer size */
            for (; i < (int)(sizeof(userName) - 1) && (c = path[i + 1]); i++) {
                if (isalpha(c))
                    userName[i] = c;
                else
                    break;
            }
            userName[i] = '\0';
        }
        if ((info = getpwnam(userName))) {
            size_t dir_len = strlen(info->pw_dir);
            size_t rest_len = strlen(&path[i + 1]);
            if (dir_len + rest_len >= MAXPATHLEN) {
                fprintf(stderr, "path too long for \"%s\"\n", path);
                return NULL;
            }
            snprintf(buffer, MAXPATHLEN, "%s%s", info->pw_dir, &path[i + 1]);
            return buffer;
        }
    } else {
        size_t path_len = strlen(path);
        if (path_len >= MAXPATHLEN) {
            fprintf(stderr, "path too long: \"%s\"\n", path);
            return NULL;
        }
        snprintf(buffer, MAXPATHLEN, "%s", path);
        return buffer;
    }
    fprintf(stderr, "failed to expand ~ for \"%s\"\n", path);
    return NULL;
}

/*
 * enter environment variables into the resource's variable list.
 *
 * Note: content buffer must be at least MAXPATHLEN bytes
 */
char* getEnvironVars(char* argv[], char* name, char* content)
{
    if (argv) {
        int ai = 0;
        char label[256];

        while (argv[ai]) {
            /* Find '=' and extract label */
            char* eq = strchr(argv[ai], '=');
            if (!eq) {
                ++ai;
                continue;
            }
            
            size_t label_len = (size_t)(eq - argv[ai]);
            if (label_len >= sizeof(label)) {
                ++ai;
                continue;
            }
            
            memcpy(label, argv[ai], label_len);
            label[label_len] = '\0';
            
            if (strcmp(name, label) == 0) {
                snprintf(content, MAXPATHLEN, "%s", eq + 1);
                return content;
            }
            ++ai;
        }
    }
    return NULL;
}

/*
 * loads a file from disk, and puts it in str.
 *
 * return:  0 on success
 *         -1 if unable to open file
 *         -2 if lseek failed
 *         -3 if malloc failed
 *         -4 if fread failed
 */
int loadFile(char* fileName, char** strp)
{
    int fd;
    FILE* fp;
    long size;

    /* Initialize output to NULL for safety */
    if (strp)
        *strp = NULL;

    /* printf("loading file '%s'\n",fileName); */

    fd = open(fileName, O_RDONLY);
    if (fd == -1) {
        io_stat = -1;
        return -1;
    }
    fp = fdopen(fd, "r");
    if (!fp) {
        close(fd);
        io_stat = -1;
        return -1;
    }

    /* determine size of file */
    size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        fclose(fp);
        io_stat = -2;
        return -2;
    }

    *strp = (char*)malloc((size_t)(size + 1));
    if (!*strp) {
        fclose(fp);
        io_stat = -3;
        return -3;
    }

    rewind(fp);
    if (size > 0 && fread(*strp, (size_t)size, 1, fp) != 1) {
        free(*strp);
        *strp = NULL;
        fclose(fp);
        io_stat = -4;
        return -4;
    }

    (*strp)[size] = '\0';

    fclose(fp);

    io_stat = 0;
    return 0;
}

/*
 * saves a str to a file
 *
 * return: -1 if unable to open file.
 */
int saveFile(char* fileName, char* str)
{
    FILE* fp;

    /*printf("save: name='%s'	 content='%s'\n",fileName,str);*/
    fp = fopen(fileName, "w");

    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'. aborted.\n", fileName);
        return -1;
    }
    fputs(str, fp);

    fclose(fp);

    return 0;
}

/*
 * Convert a relative path to an absolute path.
 * If the path is already absolute or is a URL (contains ':'),
 * return a copy of it.
 * Otherwise, make it relative to the current working directory.
 * 
 * Returns: newly allocated string (caller must free), or NULL on error.
 */
char* makeAbsolutePath(const char* path)
{
    char* result;
    char* real_path;
    char cwd[MAXPATHLEN];
    
    if (!path) {
        return NULL;
    }
    
    /* If path starts with '/', it's already absolute - return a copy */
    if (path[0] == '/') {
        return saveString(path);
    }
    
    /* If path contains ':', it's likely a URL (http://, https://, file://, etc.) */
    if (strchr(path, ':')) {
        return saveString(path);
    }
    
    /* Try to use realpath first, which resolves symlinks and normalizes */
    real_path = realpath(path, NULL);
    if (real_path) {
        result = saveString(real_path);
        free(real_path);
        return result;
    }
    
    /* Fallback: combine with current working directory */
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        size_t cwd_len = strlen(cwd);
        size_t path_len = strlen(path);
        size_t total_len = cwd_len + 1 + path_len + 1;
        
        result = (char*)malloc(total_len);
        if (result) {
            int written = snprintf(result, total_len, "%s%s%s", 
                                   cwd, 
                                   (cwd[cwd_len - 1] != '/') ? "/" : "",
                                   path);
            if (written > 0 && (size_t)written < total_len) {
                return result;
            }
            free(result);
        }
    }
    
    /* If all else fails, return a copy of the original path */
    return saveString(path);
}
