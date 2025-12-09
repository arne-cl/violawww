# Protocol Reference

ViolaWWW supports multiple network protocols for accessing different types of resources. This document describes all supported protocols, their URL syntax, and usage notes.

---

## Overview

| Protocol | URL Scheme | Default Port | Status |
|----------|------------|--------------|--------|
| [HTTP](#http) | `http://` | 80 | Original (1992) |
| [HTTPS](#https) | `https://` | 443 | **Added in v4.0 (2025)** |
| [FTP](#ftp) | `ftp://` | 21 | Original |
| [File](#file) | `file://` | — | Original |
| [Gopher](#gopher) | `gopher://` | 70 | Original |
| [News/NNTP](#news) | `news:` | 119 | Original |
| [Telnet](#telnet) | `telnet://` | 23 | Original |
| [Rlogin](#rlogin) | `rlogin://` | 513 | Original |
| [TN3270](#tn3270) | `tn3270://` | 23 | Original |
| [WAIS](#wais) | `wais://` | 210 | Conditional |

---

## HTTP

**HyperText Transfer Protocol** — the primary protocol for World Wide Web communication.

### URL Syntax

```
http://host[:port]/path[?query]
```

### Features

- HTTP/1.0 and HTTP/1.1 support
- Keep-alive connection pooling (up to 6 connections)
- Automatic redirect following (301, 302, 303, 307, 308)
- Basic authentication support
- Chunked transfer encoding
- Internet Archive (Wayback Machine) fallback for unavailable domains

### Examples

```
http://info.cern.ch/
http://example.com:8080/path/to/resource
http://user:password@host/protected/
```

### Implementation

- **Source**: `src/libWWW/HTTP.c`, `src/libWWW/HTTP.h`
- **Authors**: Tim Berners-Lee (CERN), Henrik Frystyk Nielsen

---

## HTTPS

**HTTP Secure** — HTTP over TLS/SSL for encrypted communication.

> **Note**: HTTPS support was **not present in the original ViolaWWW** (1992-1995). It was added in version 4.0 (2025) by Evgeny Stepanischev.

### URL Syntax

```
https://host[:port]/path[?query]
```

### Features

- TLS 1.0+ support via OpenSSL 3.x
- SNI (Server Name Indication) for virtual hosting
- Keep-alive connection pooling
- Automatic redirect following (including HTTP ↔ HTTPS)
- Cross-protocol redirect support

### Security Notes

⚠️ **Warning**: For compatibility with old/self-signed certificates, SSL certificate verification is disabled (`SSL_VERIFY_NONE`). This makes connections susceptible to MITM attacks. Use for trusted sites only.

### Examples

```
https://example.com/
https://www.google.com/search?q=violawww
```

### Implementation

- **Source**: `src/libWWW/HTTPS.c`, `src/libWWW/HTTPS.h`, `src/libWWW/HTSSL.c`, `src/libWWW/HTSSL.h`
- **Dependencies**: OpenSSL 3.x (optional, compile with `USE_SSL`)
- **Author**: Evgeny Stepanischev (2025)

---

## FTP

**File Transfer Protocol** — for downloading files from FTP servers.

### URL Syntax

```
ftp://[user[:password]@]host[:port]/path
```

### Features

- Anonymous FTP (default user: `anonymous`, password: `WWWuser@`)
- Directory listing as HTML
- Binary and ASCII transfer modes
- Connection caching for multiple requests to same server
- Active mode transfers (LISTEN)

### Examples

```
ftp://ftp.example.com/pub/
ftp://user:pass@ftp.example.com/private/file.txt
ftp://ftp.gnu.org/gnu/
```

### Implementation

- **Source**: `src/libWWW/HTFTP.c`, `src/libWWW/HTFTP.h`
- **Default Port**: 21
- **Authors**: Tim Berners-Lee, Denis DeLaRoca (UCLA)

### History

- May 1991: Initial implementation (TBL)
- Jan 1992: Bug fixes for socket handling
- Dec 1992: Anonymous FTP password improvement

---

## File

**Local File Access** — for opening files from the local filesystem.

### URL Syntax

```
file://localhost/path/to/file
file:///path/to/file
file://host/path  (for NFS-mounted files)
```

### Features

- Direct filesystem access
- Automatic MIME type detection by file extension
- Directory listing as HTML
- Support for various file formats (HTML, text, images, etc.)

### Examples

```
file:///Users/username/Documents/page.html
file:///etc/hosts
file://localhost/tmp/test.txt
```

### Implementation

- **Source**: `src/libWWW/HTFile.c`, `src/libWWW/HTFile.h`
- **Authors**: Tim Berners-Lee, Strstrlen Strstrlen (CERN)

---

## Gopher

**Gopher Protocol** — a pre-WWW distributed document delivery system (RFC 1436).

### URL Syntax

```
gopher://host[:port]/[type][selector]
```

Where `type` is a single character indicating the resource type.

### Gopher Types

| Type | Description |
|------|-------------|
| `0` | Text file |
| `1` | Gopher menu (directory) |
| `2` | CSO phone book server |
| `3` | Error |
| `4` | BinHex encoded Macintosh file |
| `5` | DOS binary archive |
| `6` | UUEncoded file |
| `7` | Full-text search query |
| `8` | Telnet session |
| `9` | Binary file |
| `g` | GIF image |
| `h` | HTML document |
| `i` | Informational text |
| `s` | Sound file |
| `I` | Image file |
| `T` | TN3270 session |

### Examples

```
gopher://gopher.floodgap.com/
gopher://gopher.quux.org/1/
gopher://gopher.example.com/0/welcome.txt
```

### Implementation

- **Source**: `src/libWWW/HTGopher.c`, `src/libWWW/HTGopher.h`
- **Default Port**: 70
- **Author**: Tim Berners-Lee (adapted from News/HTTP, Sep 1990)

### History

- Sep 1990: Adapted from other access methods (TBL)
- Nov 1991: Downgraded to portable C implementation

---

## News

**NNTP (Network News Transfer Protocol)** — for reading Usenet newsgroups (RFC 977).

### URL Syntax

```
news:newsgroup.name
news:message-id
news:*                    (list all groups)
```

### Features

- Newsgroup listing and browsing
- Article retrieval and threading
- Message-ID based access
- Configurable news server

### News Server Configuration

The news server is determined by (in order of priority):

1. Environment variable `NNTPSERVER`
2. File `/usr/local/lib/rn/server`
3. Compile-time `DEFAULT_NEWS_HOST`
4. Default: `news`

### Troubleshooting

**Error: "HTNews: Can't find news host 'news'. Please define your NNTP server"**

This error occurs when ViolaWWW cannot connect to the default NNTP server (`news`). In the 1990s, most ISPs provided Usenet access and the hostname `news` typically resolved to the provider's local NNTP server. Today, public NNTP servers are rare.

**Solution:** Set the `NNTPSERVER` environment variable before launching ViolaWWW:

```bash
# One-time use
export NNTPSERVER=news.eternal-september.org
./src/vw/vw news:comp.infosystems.www.browsers

# Permanent configuration (add to ~/.bashrc or ~/.zshrc)
export NNTPSERVER=news.eternal-september.org
```

### Public NNTP Servers

| Server | Registration | Notes |
|--------|--------------|-------|
| `news.eternal-september.org` | Free account required | [eternal-september.org](https://www.eternal-september.org/) |
| `news.aioe.org` | None | Limited access |

### Examples

```
news:comp.infosystems.www.browsers
news:<message-id@host.domain>
news:alt.hypertext
```

### Implementation

- **Source**: `src/libWWW/HTNews.c`, `src/libWWW/HTNews.h`
- **Default Port**: 119
- **Author**: Tim Berners-Lee (Sep 1990)

---

## Telnet

**Telnet Protocol** — for interactive terminal sessions to remote hosts.

### URL Syntax

```
telnet://[user@]host[:port]
```

### Features

- Launches external `telnet` program
- User login hint display
- Port specification support
- Security restriction in secure mode (prevents "telnet hopping")

### Security Note

When `HTSecure` is enabled (e.g., when accessed via a gateway), telnet URLs are blocked for security reasons. Users are shown manual connection instructions instead.

### Examples

```
telnet://bbs.example.com
telnet://user@library.example.edu
telnet://mud.example.com:4000
```

### External Dependency

Requires the `telnet` command-line utility:

```bash
# macOS
brew install telnet
```

### Implementation

- **Source**: `src/libWWW/HTTelnet.c`, `src/libWWW/HTTelnet.h`
- **Default Port**: 23
- **Authors**: Tim Berners-Lee, Jean-Francois Groff, Denis DeLaRoca

### History

- Jun 1992: Telnet hopping prohibited for security (TBL)
- Dec 1992: TN3270 support added (DD)

---

## Rlogin

**Remote Login Protocol** — for Unix remote shell access.

### URL Syntax

```
rlogin://[user@]host
```

### Features

- Launches external `rlogin` program
- Supports `-l username` option for user specification
- Unix-specific protocol

### Examples

```
rlogin://server.example.com
rlogin://admin@unix-host.local
```

### External Dependency

Requires the `rlogin` command-line utility:

```bash
# macOS
brew install inetutils
```

### Implementation

- **Source**: `src/libWWW/HTTelnet.c` (shared with telnet)
- **Default Port**: 513

---

## TN3270

**TN3270 Protocol** — IBM 3270 terminal emulation over TCP/IP.

### URL Syntax

```
tn3270://host[:port]
```

### Features

- IBM mainframe terminal emulation
- Launches external `tn3270` program
- Used for accessing IBM mainframes and AS/400 systems

### Examples

```
tn3270://mainframe.example.com
tn3270://ibm-host.corporate.net:2323
```

### Implementation

- **Source**: `src/libWWW/HTTelnet.c` (shared with telnet)
- **Default Port**: 23
- **Author**: Denis DeLaRoca (Dec 1992)

---

## WAIS

**Wide Area Information Servers** — a distributed text searching system.

### URL Syntax

```
wais://host[:port]/database
wais://host[:port]/database?query
```

### Features

- Full-text search across distributed databases
- Directory of servers support
- Document retrieval by ID
- Source file caching

### Availability

WAIS support is **conditionally compiled**:

- With `DIRECT_WAIS` defined: Native WAIS client (requires WAIS library)
- Without `DIRECT_WAIS`: Requests are proxied through a WAIS gateway

### Default Gateway

When WAIS library is not linked, requests go through:
```
http://info.cern.ch:8001/
```

### Examples

```
wais://wais.example.com/database
wais://quake.think.com:210/directory-of-servers?search+terms
```

### Implementation

- **Source**: `src/libWWW/HTWAIS.c`, `src/libWWW/HTWAIS.h`
- **Default Port**: 210
- **Authors**: Brewster Kahle (Thinking Machines), Tim Berners-Lee

### History

- Sep 1991: Adapted from WAIS shell-ui.c (TBL)
- Feb 1991: HTML output cleanup
- Mar 1993: Lib 2.0 compatible module

---

## Protocol Registration

Protocols are registered at startup in `HTAccessInit()` (`src/libWWW/HTAccess.c`):

```c
HTRegisterProtocol(&HTTP);
#ifdef USE_SSL
HTRegisterProtocol(&HTTPS);
#endif
HTRegisterProtocol(&HTFile);
HTRegisterProtocol(&HTTelnet);
HTRegisterProtocol(&HTTn3270);
HTRegisterProtocol(&HTRlogin);
#ifndef DECNET
HTRegisterProtocol(&HTFTP);
HTRegisterProtocol(&HTNews);
HTRegisterProtocol(&HTGopher);
#ifdef DIRECT_WAIS
HTRegisterProtocol(&HTWAIS);
#endif
#endif
```

### Conditional Compilation

| Flag | Effect |
|------|--------|
| `USE_SSL` | Enables HTTPS support |
| `DECNET` | Disables FTP, News, Gopher (DECnet incompatible) |
| `DIRECT_WAIS` | Enables native WAIS client |
| `NO_INIT` | Disables automatic protocol registration |

---

## Adding New Protocols

To add a new protocol:

1. Create protocol handler files (`HTMyProto.c`, `HTMyProto.h`)
2. Implement the load function: `int HTLoadMyProto(const char* addr, HTParentAnchor* anchor, HTFormat format_out, HTStream* sink)`
3. Define the protocol descriptor: `GLOBALDEF PUBLIC HTProtocol HTMyProto = {"myproto", HTLoadMyProto, NULL};`
4. Register in `HTAccessInit()`: `HTRegisterProtocol(&HTMyProto);`

---

## See Also

- [Main README](../README.md) — Project overview
- [EXTERNAL_DEPENDENCIES.md](EXTERNAL_DEPENDENCIES.md) — External programs (telnet, rlogin)
- [SECURITY_REFERENCE.md](SECURITY_REFERENCE.md) — Security considerations

