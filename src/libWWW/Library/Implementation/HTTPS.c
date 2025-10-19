/*	HyperText Transfer Protocol over SSL/TLS - Client		HTTPS.c
**	===============================================
**
**	This is a modified version of HTTP.c with SSL/TLS support
**	for HTTPS connections (https:// URLs).
**
**	Based on HTTP.c from libwww
*/

#include "HTTPS.h"

#ifdef USE_SSL

#include "HTAABrow.h"
#include "HTAlert.h"
#include "HTFormat.h"
#include "HTInit.h"
#include "HTMIME.h"
#include "HTML.h"
#include "HTParse.h"
#include "HTTCP.h"
#include "HTUtils.h"
#include "HTSSL.h"
#include "tcp.h"
#include <ctype.h>
#include <stddef.h>
#include <unistd.h>

/* Forward declarations */
extern void http_progress_notify();

#define HTTPS_VERSION "HTTP/1.0"
#define HTTPS2 /* Version is greater than 0.9 */

#define INIT_LINE_SIZE 1024    /* Start with line buffer this big */
#define LINE_EXTEND_THRESH 256 /* Minimum read size */
#define VERSION_LENGTH 20      /* for returned protocol version */

/* These are shared with HTTP.c */
extern int http_method;
extern char* http_dataToPost;

struct _HTStream {
    HTStreamClass* isa; /* all we need to know */
};

extern char* HTAppName;    /* Application name */
extern char* HTAppVersion; /* Application version */

/*	Find HTTP header (case-insensitive)
**	-----------------------------------
*/
PRIVATE char* find_header(char* headers, const char* header_name) {
    char* p = headers;
    int len = strlen(header_name);
    
    while (*p) {
        /* Check if we're at start of line */
        if (p == headers || *(p-1) == '\n') {
            /* Compare header name case-insensitively */
            int i;
            int match = 1;
            for (i = 0; i < len && p[i]; i++) {
                if (TOLOWER(p[i]) != TOLOWER(header_name[i])) {
                    match = 0;
                    break;
                }
            }
            /* Make sure we matched the full length and have room to check ':' */
            if (match && i == len && p[len] && p[len] == ':') {
                return p;
            }
        }
        p++;
    }
    return NULL;
}

/*		Load Document from HTTPS Server			HTLoadHTTPS()
**		===============================
**
**	Given a hypertext address, this routine loads a document over SSL/TLS.
**
** On entry,
**	arg	is the hypertext reference of the article to be loaded.
**
** On exit,
**	returns	>=0	If no error, a good socket number
**		<0	Error.
*/
PUBLIC int HTLoadHTTPS ARGS4(CONST char*, arg,
                             HTParentAnchor*, anAnchor, 
                             HTFormat, format_out, 
                             HTStream*, sink) {
    int s;                   /* Socket number for returned data */
    HTSSLConnection* ssl_conn = NULL; /* SSL connection */
    char* command;           /* The whole command */
    char* eol = 0;           /* End of line if found */
    char* start_of_data;     /* Start of body of reply */
    int length;              /* Number of valid bytes in buffer */
    int status;              /* tcp return */
    char crlf[3];            /* A CR LF equivalent string */
    HTStream* target = NULL; /* Unconverted data */
    HTFormat format_in;      /* Format arriving in the message */
    char* auth = NULL;       /* Authorization information */

    CONST char* gate = 0; /* disable gateway feature */
    SockA soc_address;    /* Binary network address */
    SockA* sin = &soc_address;
    BOOL had_header = NO;
    char* text_buffer = NULL;
    char* binary_buffer = NULL;
    int buffer_length = INIT_LINE_SIZE;
    BOOL extensions = YES;
    int server_status = 0;
    ptrdiff_t offset = 0;

    int tries = 0;
    char* hostname = NULL;
    static int redirect_count = 0;  /* Track redirect depth */
    #define MAX_REDIRECTS 10

    extern int http_progress_expected_total_bytes;
    extern int http_progress_subtotal_bytes;
    extern int http_progress_total_bytes;
    extern int http_progress_reporter_level;
    static int delimSize = 0;
    static char* delim = "Content-length: ";
    char delimBuf[32];
    int i, j, delimi = 0;
    char *cp, *buffp, buff[100];
    int foundContentLength = 0;

    if (delimSize == 0)
        delimSize = strlen(delim);

    if (!arg)
        return -3;
    if (!*arg)
        return -2;

    /* Set up defaults - use port 443 for HTTPS */
#ifdef DECNET
    sin->sdn_family = AF_DECnet;
    sin->sdn_objnum = DNP_OBJ;
#else
    sin->sin_family = AF_INET;
    sin->sin_port = htons(443); /* HTTPS port */
#endif

    sprintf(crlf, "%c%c", CR, LF);

    if (TRACE) {
        fprintf(stderr, "HTTPSAccess: Direct HTTPS access for %s\n", arg);
    }

    /* Get node name and optional port number */
    {
        char* p1 = HTParse(arg, "", PARSE_HOST);
        int status = HTParseInet(sin, p1);
        free(p1);
        if (status) {
            return status;
        }
    }

    /* Extract hostname for SNI */
    hostname = HTParse(arg, "", PARSE_HOST);
    if (hostname) {
        char* colon = strchr(hostname, ':');
        if (colon)
            *colon = '\0'; /* Remove port number */
    }

retry:

    /* Check redirect limit */
    if (redirect_count >= MAX_REDIRECTS) {
        redirect_count = 0;
        if (hostname) free(hostname);
        HTAlert("Too many redirects");
        return -1;
    }

    /* Compose authorization information */
#ifdef ACCESS_AUTH
#define FREE(x)                                                                                    \
    if (x) {                                                                                       \
        free(x);                                                                                   \
        x = NULL;                                                                                  \
    }
{
    char* docname;
    char* hostport;
    char* colon;
    int portnumber;

    docname = HTParse(arg, "", PARSE_PATH);
    hostport = HTParse(arg, "", PARSE_HOST);
    if (hostport && NULL != (colon = strchr(hostport, ':'))) {
        *(colon++) = '\0';
        portnumber = atoi(colon);
    } else
        portnumber = 443;

    auth = HTAA_composeAuth(hostport, portnumber, docname);

    if (TRACE) {
        if (auth)
            fprintf(stderr, "HTTPS: Sending authorization: %s\n", auth);
        else
            fprintf(stderr, "HTTPS: Not sending authorization (yet)\n");
    }
    FREE(hostport);
    FREE(docname);
}
#endif /* ACCESS_AUTH */

    /* Create socket */
#ifdef DECNET
    s = socket(AF_DECnet, SOCK_STREAM, 0);
#else
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
    
    status = connect(s, (struct sockaddr*)&soc_address, sizeof(soc_address));
    if (status < 0) {
        if (hostname) free(hostname);
        return HTInetStatus("connect");
    }

    /* Wrap socket with SSL */
    ssl_conn = HTSSL_connect(s, hostname);
    if (!ssl_conn) {
        fprintf(stderr, "HTTPS: SSL connection failed\n");
        close(s);
        if (hostname) free(hostname);
        return -1;
    }

    if (TRACE) {
        fprintf(stderr, "HTTPS: SSL handshake completed\n");
    }

    /* Build HTTP request */
    {
        char* p1 = HTParse(arg, "", PARSE_PATH | PARSE_PUNCTUATION);
        command = malloc(4 + strlen(p1) + 2 + 31);
        if (command == NULL) {
            HTSSL_close(ssl_conn);
            if (hostname) free(hostname);
            outofmem(__FILE__, "HTLoadHTTPS");
        }

        if (http_method == HTTP_METHOD_POST)
            strcpy(command, "POST ");
        else
            strcpy(command, "GET ");

        strcat(command, p1);
        free(p1);
    }

#ifdef HTTPS2
    if (extensions) {
        strcat(command, " ");
        strcat(command, HTTPS_VERSION);
    }
#endif

    strcat(command, crlf);

    if (extensions) {
        int n;
        int i;
        HTAtom* present = WWW_PRESENT;
        char line[256];

        if (!HTPresentations)
            HTFormatInit();
        n = HTList_count(HTPresentations);

        for (i = 0; i < n; i++) {
            HTPresentation* pres = HTList_objectAt(HTPresentations, i);
            if (pres->rep_out == present) {
                if (pres->quality != 1.0) {
                    sprintf(line, "Accept: %s q=%.3f%c%c", HTAtom_name(pres->rep), pres->quality,
                            CR, LF);
                } else {
                    sprintf(line, "Accept: %s%c%c", HTAtom_name(pres->rep), CR, LF);
                }
                StrAllocCat(command, line);
            }
        }

        sprintf(line, "User-Agent:  %s/%s  libwww/%s (HTTPS)%s",
                HTAppName ? HTAppName : "unknown", HTAppVersion ? HTAppVersion : "0.0",
                HTLibraryVersion, crlf);
        StrAllocCat(command, line);

        /* Add Host: header */
        if (hostname && *hostname) {
            char* host_with_port = HTParse(arg, "", PARSE_HOST);
            sprintf(line, "Host: %s%s", host_with_port, crlf);
            StrAllocCat(command, line);
            free(host_with_port);
        }

#ifdef ACCESS_AUTH
        if (auth != NULL) {
            sprintf(line, "%s%s", auth, crlf);
            StrAllocCat(command, line);
        }
#endif

        if (http_dataToPost) {
            sprintf(line, "Content-type: application/x-www-form-urlencoded%s", crlf);
            StrAllocCat(command, line);

            sprintf(line, "Content-length: %d%s", strlen(http_dataToPost), crlf);
            StrAllocCat(command, line);

            StrAllocCat(command, crlf);
            StrAllocCat(command, http_dataToPost);
        }
    }

    StrAllocCat(command, crlf);

    /* Translate to ASCII if needed */
#ifdef NOT_ASCII
    {
        char* p;
        for (p = command; *p; p++) {
            *p = TOASCII(*p);
        }
    }
#endif

    /* Send request over SSL */
    status = HTSSL_write(ssl_conn, command, strlen(command));
    free(command);
    if (status < 0) {
        HTSSL_close(ssl_conn);
        if (hostname) free(hostname);
        return HTInetStatus("SSL write");
    }

    /* Read response */
    {
        BOOL end_of_file = NO;
        HTAtom* encoding = HTAtom_for("7bit");

        binary_buffer = (char*)malloc(buffer_length * sizeof(char));
        if (!binary_buffer) {
            HTSSL_close(ssl_conn);
            if (hostname) free(hostname);
            outofmem(__FILE__, "HTLoadHTTPS");
        }
        text_buffer = (char*)malloc(buffer_length * sizeof(char));
        if (!text_buffer) {
            HTSSL_close(ssl_conn);
            if (hostname) free(hostname);
            outofmem(__FILE__, "HTLoadHTTPS");
        }
        length = 0;

        http_progress_subtotal_bytes = 0;
        if (http_progress_reporter_level == 0) {
            http_progress_total_bytes = 0;
            http_progress_expected_total_bytes = 0;
        }
        ++http_progress_reporter_level;

        do {
            if (buffer_length - length < LINE_EXTEND_THRESH) {
                buffer_length = buffer_length + buffer_length;
                binary_buffer = (char*)realloc(binary_buffer, buffer_length * sizeof(char));
                if (!binary_buffer) {
                    HTSSL_close(ssl_conn);
                    if (hostname) free(hostname);
                    outofmem(__FILE__, "HTLoadHTTPS");
                }
                text_buffer = (char*)realloc(text_buffer, buffer_length * sizeof(char));
                if (!text_buffer) {
                    HTSSL_close(ssl_conn);
                    if (hostname) free(hostname);
                    outofmem(__FILE__, "HTLoadHTTPS");
                }
            }
            
            status = HTSSL_read(ssl_conn, binary_buffer + length, buffer_length - length - 1);
            if (status < 0) {
                HTAlert("Unexpected SSL read error on response");
                HTSSL_close(ssl_conn);
                if (hostname) free(hostname);
                return status;
            }

            if (TRACE)
                fprintf(stderr, "HTTPS: SSL read returned %d bytes.\n", status);

#ifdef VIOLA
            http_progress_notify(status);
#endif

            if (status == 0) {
                end_of_file = YES;
                break;
            }
            binary_buffer[length + status] = 0;

            /* Make ASCII copy */
#ifdef NOT_ASCII
            if (TRACE)
                fprintf(stderr, "Local codes CR=%d, LF=%d\n", CR, LF);
#endif
            {
                char* p;
                char* q;
                for (p = binary_buffer + length, q = text_buffer + length; *p; p++, q++) {
                    *q = FROMASCII(*p);
                }
                *q++ = 0;
            }

            /* Check for HTTP response */
#define STUB_LENGTH 20
            if (length < STUB_LENGTH && length + status >= STUB_LENGTH) {
                if (strncmp("HTTP/", text_buffer, 5) != 0) {
                    char* p;
                    start_of_data = text_buffer;
                    for (p = binary_buffer; p < binary_buffer + STUB_LENGTH; p++) {
                        if (((int)*p) & 128) {
                            format_in = HTAtom_for("www/unknown");
                            length = length + status;
                            goto copy;
                        }
                    }
                }
            }

            eol = strchr(text_buffer + length, 10);
            if (eol) {
                *eol = 0;
                if (eol[-1] == CR)
                    eol[-1] = 0;
            }

            length = length + status;

        } while (!eol && !end_of_file);
    }

    /* Parse response line */
    if (TRACE)
        fprintf(stderr, "HTTPS: Rx: %.70s\n", text_buffer);

    {
        int fields;
        char server_version[VERSION_LENGTH + 1];

        /* Handle old HTTP/0.9 servers */
        if (extensions && 0 == strcmp(text_buffer,
                                      "Document address invalid or access not authorised")) {
            extensions = NO;
            if (binary_buffer)
                free(binary_buffer);
            if (text_buffer)
                free(text_buffer);
            if (TRACE)
                fprintf(stderr, "HTTPS: Retry with HTTP0\n");
            HTSSL_close(ssl_conn);
            goto retry;
        }

        fields = sscanf(text_buffer, "%20s%d", server_version, &server_status);

        if (fields < 2 || strncmp(server_version, "HTTP/", 5) != 0) {
            format_in = WWW_HTML;
            start_of_data = text_buffer;
            if (eol)
                *eol = '\n';
        } else {
            format_in = HTAtom_for("www/mime");
            start_of_data = eol ? eol + 1 : text_buffer + length;

            switch (server_status / 100) {
            default:
                HTAlert("Unknown status reply from server!");
                break;

            case 3: /* Various forms of redirection */
                {
                    char* location = NULL;
                    char* p;
                    
                    /* Parse Location header from response (case-insensitive) */
                    p = find_header(start_of_data, "Location");
                    
                    if (p) {
                        char new_url[2048];
                        char* end;
                        
                        /* Skip "Location:" */
                        p += 9;
                        while (*p == ' ' || *p == '\t') p++;
                        
                        /* Extract URL until newline */
                        end = strchr(p, '\r');
                        if (!end) end = strchr(p, '\n');
                        
                        if (end) {
                            int url_len = end - p;
                            if (url_len > 0 && url_len < sizeof(new_url) - 1) {
                                strncpy(new_url, p, url_len);
                                new_url[url_len] = '\0';
                                
                                /* Make absolute URL if relative */
                                location = HTParse(new_url, arg, PARSE_ALL);
                                
                                if (TRACE) {
                                    fprintf(stderr, "HTTPS: Redirect %d -> %s\n", 
                                            server_status, location);
                                }
                                
                                /* Clean up and follow redirect */
                                if (binary_buffer) free(binary_buffer);
                                if (text_buffer) free(text_buffer);
                                if (hostname) free(hostname);
                                HTSSL_close(ssl_conn);
                                
                                redirect_count++;
                                arg = location;  /* Follow redirect */
                                
                                /* Re-extract hostname for new URL */
                                hostname = HTParse(arg, "", PARSE_HOST);
                                if (hostname) {
                                    char* colon = strchr(hostname, ':');
                                    if (colon) *colon = '\0';
                                }
                                
                                goto retry;
                            }
                        }
                    }
                    
                    /* If we couldn't parse Location header */
                    HTAlert("Redirection response but no Location header found");
                    if (location) free(location);
                }
                break;

            case 4:
#ifdef ACCESS_AUTH
                switch (server_status) {
                case 401:
                    length -= start_of_data - text_buffer;
                    if (HTAA_shouldRetryWithAuth(start_of_data, length, s)) {
                        if (binary_buffer)
                            free(binary_buffer);
                        if (text_buffer)
                            free(text_buffer);
                        if (TRACE)
                            fprintf(stderr, "HTTPS: Retry with auth\n");
                        HTSSL_close(ssl_conn);
                        goto retry;
                        break;
                    } else {
                        /* FALL THROUGH */
                    }
                default: {
                    char* p1 = HTParse(arg, "", PARSE_HOST);
                    char* message;

                    if (!(message = (char*)malloc(strlen(text_buffer) + strlen(p1) + 100))) {
                        HTSSL_close(ssl_conn);
                        if (hostname) free(hostname);
                        outofmem(__FILE__, "HTTPS 4xx status");
                    }
                    sprintf(message, "HTTPS server at %s replies:\n%s\n\n%s\n", p1, text_buffer,
                            ((server_status == 401) ? "Access Authorization package giving up.\n"
                                                    : ""));
                    status = HTLoadError(sink, server_status, message);
                    free(message);
                    free(p1);
                    goto clean_up;
                }
                }
                goto clean_up;
                break;
#else
                /* Fall through to case 5 */
#endif

            case 5: {
                char* p1 = HTParse(arg, "", PARSE_HOST);
                char* message = (char*)malloc(strlen(text_buffer) + strlen(p1) + 100);
                if (!message) {
                    HTSSL_close(ssl_conn);
                    if (hostname) free(hostname);
                    outofmem(__FILE__, "HTTPS 5xx status");
                }
                sprintf(message, "HTTPS server at %s replies:\n%s", p1, text_buffer);
                status = HTLoadError(sink, server_status, message);
                free(message);
                free(p1);
                goto clean_up;
            }
            break;

            case 2:
                break;
            }
        }
    }

    /* Set up stream stack */
copy:
    target = HTStreamStack(format_in, format_out, sink, anAnchor);

    if (!target) {
        char buffer[1024];
        if (binary_buffer)
            free(binary_buffer);
        if (text_buffer)
            free(text_buffer);
        sprintf(buffer, "Sorry, no known way of converting %s to %s.", HTAtom_name(format_in),
                HTAtom_name(format_out));
        status = HTLoadError(sink, 501, buffer);
        goto clean_up;
    }

    if (format_in == WWW_HTML) {
        target = HTNetToText(target);
    }

    /* Parse Content-Length */
    if (http_progress_reporter_level == 1) {
        cp = strstr(binary_buffer, "Content-length: ");
        if (!cp)
            cp = strstr(binary_buffer, "Content-Length: ");
        if (cp) {
            cp += strlen("Content-length: ");
            for (buffp = buff; *cp != '\n'; buffp++, cp++)
                *buffp = *cp;
            *buffp = '\0';
            http_progress_expected_total_bytes = atoi(buff);
        }
        foundContentLength = 1;
    }

    /* Strip MIME headers and copy data */
    if (server_status) {
        if (server_status / 100 == 2 && format_out == WWW_SOURCE) {
            char *sp, *end;
            char lc = '\0', llc = '\0', lllc = '\0';

            if (foundContentLength == 0 && http_progress_reporter_level == 1) {
                cp = strstr(binary_buffer, "Content-length: ");
                if (!cp)
                    cp = strstr(binary_buffer, "Content-Length: ");
                if (cp) {
                    cp += strlen("Content-length: ");
                    for (buffp = buff; *cp != '\n'; buffp++, cp++)
                        *buffp = *cp;
                    *buffp = '\0';
                    http_progress_expected_total_bytes = atoi(buff);
                }
            }

            /* Find end of headers */
            {
                sp = binary_buffer + (start_of_data - text_buffer);
                char* buffer_end = binary_buffer + length;
                while (sp < buffer_end && *sp) {
                    if (llc == '\n' && lc == '\n') {
                        offset = sp - binary_buffer - (start_of_data - text_buffer);
                        goto wheee;
                    } else if (lllc == '\n' && llc == '\r' && lc == '\n') {
                        offset = sp - binary_buffer - (start_of_data - text_buffer);
                        goto wheee;
                    }
                    lllc = llc;
                    llc = lc;
                    lc = *sp++;
                }
            }
            
            /* Continue reading to find header end */
            {
                for (;;) {
                    status = HTSSL_read(ssl_conn, binary_buffer, buffer_length - 1);
                    if (status < 0) {
                        HTAlert("Unexpected SSL read error on response");
                        HTSSL_close(ssl_conn);
                        if (hostname) free(hostname);
                        return status;
                    }
                    if (status == 0)
                        break;  /* EOF without finding end of headers */
                    sp = binary_buffer;
                    char* chunk_end = binary_buffer + status;
                    while (sp < chunk_end) {
                        if (llc == '\n' && lc == '\n') {
                            offset = sp - binary_buffer;
                            goto dofus;
                        } else if (lllc == '\n' && llc == '\r' && lc == '\n') {
                            offset = sp - binary_buffer;
                            goto dofus;
                        }
                        lllc = llc;
                        llc = lc;
                        lc = *sp++;
                    }
                }
            dofus:
                (*target->isa->put_block)(target, binary_buffer + offset, status - offset);
                /* Copy remaining data using SSL */
                for (;;) {
                    status = HTSSL_read(ssl_conn, binary_buffer, buffer_length - 1);
                    if (status <= 0)
                        break;
                    (*target->isa->put_block)(target, binary_buffer, status);
                }
                (*target->isa->free)(target);
                status = HT_LOADED;
                goto clean_up;
            }
        }
    }
    
wheee:
    {
        ptrdiff_t data_offset = start_of_data - text_buffer;
        ptrdiff_t remaining_length = length - data_offset - offset;
        (*target->isa->put_block)(target, binary_buffer + data_offset + offset,
                                  (int)remaining_length);
    }
    
    /* Copy remaining data */
    for (;;) {
        status = HTSSL_read(ssl_conn, binary_buffer, buffer_length - 1);
        if (status <= 0)
            break;
        (*target->isa->put_block)(target, binary_buffer, status);
    }
    
    (*target->isa->free)(target);
    status = HT_LOADED;

clean_up:
    --http_progress_reporter_level;
    if (http_progress_reporter_level == 0) {
        http_progress_total_bytes = 0;
        http_progress_expected_total_bytes = 0;
#ifdef VIOLA
        http_progress_notify(-1);
#endif
    }
    http_progress_subtotal_bytes = 0;

    if (binary_buffer)
        free(binary_buffer);
    if (text_buffer)
        free(text_buffer);
    if (hostname)
        free(hostname);

    if (TRACE)
        fprintf(stderr, "HTTPS: Closing SSL connection.\n");
    
    HTSSL_close(ssl_conn);

    redirect_count = 0;  /* Reset redirect counter on successful completion */
    return status;
}

/*	Protocol descriptor
*/
GLOBALDEF PUBLIC HTProtocol HTTPS = {"https", HTLoadHTTPS, 0};

#endif /* USE_SSL */

