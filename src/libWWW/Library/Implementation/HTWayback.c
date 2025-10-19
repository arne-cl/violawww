/*		Wayback Machine (Internet Archive) Integration		HTWayback.c
**		==============================================
**
**	This module provides integration with the Internet Archive's
**	Wayback Machine for accessing archived versions of websites.
*/

#include "HTWayback.h"
#include "HTParse.h"
#include "HTTCP.h"
#include "HTUtils.h"
#include "tcp.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WAYBACK_API_HOST "web.archive.org"
#define WAYBACK_API_PORT 80
#define WAYBACK_API_TIMEOUT 5 /* seconds */

/*	Check if URL is available in Wayback Machine
**	---------------------------------------------
**
**  Uses the Wayback Availability API to check if a snapshot exists
*/
PUBLIC char* HTWaybackCheck PARAMS((CONST char* url)) {
    int s = -1;
    SockA soc_address;
    SockA* sin = &soc_address;
    char* request = NULL;
    char* response_buffer = NULL;
    char* wayback_url = NULL;
    int status;
    int response_len = 0;
    int total_read = 0;
    char* url_encoded = NULL;
    char* timestamp_start = NULL;
    char* url_start = NULL;
    char* p;
    int i;

    if (!url || !*url) {
        return NULL;
    }

    if (TRACE) {
        fprintf(stderr, "Wayback: Checking %s\n", url);
    }

    /* CDX API accepts full URLs as-is: url=http://www.example.com/path
    ** We only need to URL-encode characters that would break the query string
    ** Keep protocol, slashes, colons as-is - they're valid in query parameters
    */
    url_encoded = (char*)malloc(strlen(url) * 3 + 1);
    if (!url_encoded) {
        return NULL;
    }

    /* Minimal URL encoding - only encode what's truly problematic */
    {
        CONST char* src = url;
        char* dst = url_encoded;
        while (*src) {
            if (*src == ' ') {
                *dst++ = '+';
            } else if (*src == '&') {
                strcpy(dst, "%26");
                dst += 3;
            } else if (*src == '#') {
                strcpy(dst, "%23");
                dst += 3;
            } else {
                /* Keep everything else as-is, including : / ? */
                *dst++ = *src;
            }
            src++;
        }
        *dst = '\0';
    }
    
    /* Set up socket address for archive.org */
    memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    sin->sin_port = htons(WAYBACK_API_PORT);

    /* Resolve archive.org hostname */
    {
        struct hostent* phost;
        phost = gethostbyname(WAYBACK_API_HOST);
        if (!phost) {
            if (TRACE) {
                fprintf(stderr, "Wayback: Cannot resolve %s\n", WAYBACK_API_HOST);
            }
            free(url_encoded);
            return NULL;
        }
        memcpy(&sin->sin_addr, phost->h_addr, phost->h_length);
    }

    /* Create socket */
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        if (TRACE) {
            fprintf(stderr, "Wayback: Cannot create socket (errno=%d)\n", errno);
        }
        free(url_encoded);
        return NULL;
    }

    /* Set socket timeout */
    {
        struct timeval timeout;
        timeout.tv_sec = WAYBACK_API_TIMEOUT;
        timeout.tv_usec = 0;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    }

    /* Connect to archive.org */
    status = connect(s, (struct sockaddr*)sin, sizeof(*sin));
    if (status < 0) {
        if (TRACE) {
            fprintf(stderr, "Wayback: Cannot connect to %s (errno=%d)\n", WAYBACK_API_HOST, errno);
        }
        close(s);
        free(url_encoded);
        return NULL;
    }

    /* Build HTTP request for Wayback CDX API (get EARLIEST snapshot) */
    request =
        (char*)malloc(strlen(url_encoded) + 512); /* Extra space for HTTP headers and path */
    if (!request) {
        close(s);
        free(url_encoded);
        return NULL;
    }

    sprintf(request,
            "GET /cdx/search/cdx?url=%s&limit=1&sort=ascending HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: ViolaWWW/3.3 (Wayback)\r\n"
            "Connection: close\r\n"
            "\r\n",
            url_encoded, WAYBACK_API_HOST);

    free(url_encoded);

    /* Send request */
    status = NETWRITE(s, request, strlen(request));
    free(request);
    if (status < 0) {
        if (TRACE) {
            fprintf(stderr, "Wayback: Failed to send request (status=%d)\n", status);
        }
        close(s);
        return NULL;
    }

    /* Read response */
    response_buffer = (char*)malloc(8192); /* Should be enough for JSON response */
    if (!response_buffer) {
        close(s);
        return NULL;
    }

    total_read = 0;
    while (total_read < 8191) {
        response_len = NETREAD(s, response_buffer + total_read, 8191 - total_read);
        if (response_len <= 0) {
            break;
        }
        total_read += response_len;
    }
    response_buffer[total_read] = '\0';
    close(s);

    if (total_read == 0) {
        if (TRACE) {
            fprintf(stderr, "Wayback: No response received\n");
        }
        free(response_buffer);
        return NULL;
    }

    /* Parse CDX text response to get EARLIEST snapshot
    ** CDX returns simple space-separated format (default, not JSON):
    ** urlkey timestamp original mimetype statuscode digest length
    ** Example:
    ** ru,hackzone)/ 19970327140651 http://www.hackzone.ru:80/ text/html 200 7ZWL... 564
    **
    ** We need fields [1]=timestamp and [2]=original
    */

    /* Skip HTTP headers - find empty line (double newline) */
    {
        char*         body = strstr(response_buffer, "\r\n\r\n");
        if (!body) {
            body = strstr(response_buffer, "\n\n");
        }
        if (!body) {
            if (TRACE) {
                fprintf(stderr, "Wayback: Cannot find response body\n");
            }
            free(response_buffer);
            return NULL;
        }
        body += 4; /* Skip past \r\n\r\n */
        if (*body == '\n') body++; /* Skip extra newline if present */
        
        /* Check for chunked encoding and skip chunk size if present */
        if (strstr(response_buffer, "Transfer-Encoding: chunked") || 
            strstr(response_buffer, "transfer-encoding: chunked")) {
            /* Skip chunk size line (hex number like "6b" followed by \r\n) */
            char* chunk_data = body;
            /* Skip to end of hex size line */
            while (*chunk_data && *chunk_data != '\r' && *chunk_data != '\n') {
                chunk_data++;
            }
            /* Skip \r\n */
            if (*chunk_data == '\r') chunk_data++;
            if (*chunk_data == '\n') chunk_data++;
            body = chunk_data;
            if (TRACE) {
                fprintf(stderr, "Wayback: Detected chunked encoding, skipped chunk size\n");
            }
        }
        
        /* Parse space-separated fields */
        {
            char timestamp[32] = {0};
            char original_url[512] = {0};
            char* field_start = body;
            char* field_end;
            int field_index = 0;

            /* Parse first 3 fields: urlkey timestamp original */
            while (*field_start && field_index < 3) {
                /* Skip leading whitespace */
                while (*field_start == ' ' || *field_start == '\t' || *field_start == '\r' || *field_start == '\n') {
                    field_start++;
                }
                
                if (*field_start == '\0') break;

                /* Find end of field (space, tab, or newline) */
                field_end = field_start;
                while (*field_end && *field_end != ' ' && *field_end != '\t' && *field_end != '\r' && *field_end != '\n') {
                    field_end++;
                }

                /* Extract field */
                if (field_index == 1) {
                    /* timestamp */
                    int len = field_end - field_start;
                    if (len > 0 && len < 32) {
                        strncpy(timestamp, field_start, len);
                        timestamp[len] = '\0';
                    }
                } else if (field_index == 2) {
                    /* original URL */
                    int len = field_end - field_start;
                    if (len > 0 && len < 512) {
                        strncpy(original_url, field_start, len);
                        original_url[len] = '\0';
                    }
                }

                field_start = field_end;
                field_index++;
            }

            /* Check if we got both timestamp and original URL */
            if (timestamp[0] == '\0' || original_url[0] == '\0') {
                if (TRACE) {
                    fprintf(stderr, "Wayback: No snapshots found\n");
                }
                free(response_buffer);
                return NULL;
            }

            /* Construct Wayback URL with EARLIEST timestamp */
            wayback_url = (char*)malloc(strlen(original_url) + 128);
            if (!wayback_url) {
                free(response_buffer);
                return NULL;
            }
            sprintf(wayback_url, "https://web.archive.org/web/%s/%s", timestamp, original_url);
        }
    }

    free(response_buffer);

    if (TRACE) {
        fprintf(stderr, "Wayback: Redirecting to archived snapshot: %s\n", wayback_url);
    }

    return wayback_url;
}

