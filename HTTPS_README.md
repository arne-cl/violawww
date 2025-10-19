# HTTPS Support for ViolaWWW

## Overview

Native HTTPS (SSL/TLS) support has been added to ViolaWWW browser using OpenSSL.

## What Was Added

### 1. OpenSSL Integration (`Makefile`)
- Auto-detection of OpenSSL@3 via Homebrew
- SSL compilation flags (`-DUSE_SSL`)
- SSL libraries linking (`-lssl -lcrypto`)

### 2. SSL Wrapper Layer
- **`HTSSL.h`** - SSL/TLS API declarations
- **`HTSSL.c`** - SSL/TLS implementation wrapping OpenSSL
  - `HTSSL_init()` - Initialize SSL library
  - `HTSSL_connect()` - Create SSL connection with SNI support
  - `HTSSL_read()` / `HTSSL_write()` - SSL I/O operations
  - `HTSSL_close()` - Close SSL connection
  - `HTSSL_shutdown()` - Cleanup SSL library

### 3. HTTPS Protocol Handler
- **`HTTPS.h`** - HTTPS protocol declarations
- **`HTTPS.c`** - HTTPS protocol implementation (based on HTTP.c)
  - Handles `https://` URLs
  - Uses port 443 by default
  - Wraps TCP connections with SSL/TLS
  - Supports HTTP/1.0 over TLS

### 4. Protocol Registration (`HTAccess.c`)
- Registered HTTPS protocol in `HTAccessInit()`
- Protocol available when `USE_SSL` is defined

### 5. Application Integration (`viola.c`)
- SSL library initialized at startup
- SSL library cleaned up at shutdown
- Works for both `viola` and `vw` (Motif) browsers

## Building

### Requirements
```bash
# Install OpenSSL via Homebrew
brew install openssl@3
```

### Compile
```bash
make clean
make -j4
```

The build system will automatically:
- Detect OpenSSL installation
- Enable HTTPS support if found
- Show warning if OpenSSL is missing

## Usage

### Testing HTTPS
```bash
# Run VW browser with test page
cd /Users/bolk/Projects/violawww2
./src/vw/vw test_https.html

# Or run viola browser
./src/viola/viola -o www
```

### Command Line
You can directly open HTTPS URLs:
```bash
# In browser URL bar, enter:
https://example.com
```

## SSL/TLS Details

### Features
- ✅ TLS 1.0+ support (via OpenSSL)
- ✅ SNI (Server Name Indication)
- ✅ Standard port 443
- ✅ HTTP/1.0 over TLS
- ✅ HTTP redirects (301, 302, 303, 307, 308)
- ✅ Automatic HTTP→HTTPS redirect following
- ✅ Up to 10 redirect hops
- ⚠️ Certificate verification disabled (for compatibility)

### Security Notes
**WARNING**: For simplicity and compatibility with old/self-signed certificates:
- SSL certificate verification is set to `SSL_VERIFY_NONE`
- This makes the connection susceptible to MITM attacks
- Suitable for testing and browsing trusted sites
- For production use, enable proper certificate verification

### Logging
When running with `-v` (verbose) flag, you'll see:
```
SSL/TLS initialized (OpenSSL 3.x.x)
HTSSL: Connected to example.com using TLSv1.3
```

## Testing Sites

Good sites for testing HTTPS:
1. **https://example.com** - Simple test page
2. **https://info.cern.ch** - First website (now has HTTPS)
3. **https://httpbin.org/get** - HTTP testing service
4. **https://www.google.com** - Major site (complex)
5. **https://www.wikipedia.org** - Major site (complex)

## Troubleshooting

### "SSL handshake failed"
- Check that the server supports TLS 1.0+
- Some modern servers require TLS 1.2+
- Try a simpler site like example.com first

### "Failed to initialize SSL/TLS"
- Ensure OpenSSL@3 is installed: `brew install openssl@3`
- Check that CA certificates are available
- Rebuild with: `make clean && make`

### No HTTPS support
Check build output for:
```
⚠ Building without OpenSSL (HTTPS will not be supported)
```

If you see this, install OpenSSL and rebuild.

## Architecture

### Call Stack for HTTPS Request
```
User clicks https:// link
  ↓
HTLoadHTTPS() in HTTPS.c
  ↓
socket() + connect()  # TCP connection
  ↓
HTSSL_connect()  # SSL handshake
  ↓
HTSSL_write()  # Send HTTP request over SSL
  ↓
HTSSL_read()  # Receive HTTP response over SSL
  ↓
Parse response & render
  ↓
HTSSL_close()  # Close SSL connection
```

### Files Modified/Added
- `Makefile` - Added OpenSSL detection and linking
- `src/libWWW/Library/Implementation/HTSSL.h` - New
- `src/libWWW/Library/Implementation/HTSSL.c` - New
- `src/libWWW/Library/Implementation/HTTPS.h` - New
- `src/libWWW/Library/Implementation/HTTPS.c` - New
- `src/libWWW/Library/Implementation/HTAccess.c` - Modified (protocol registration)
- `src/viola/viola.c` - Modified (SSL init/shutdown)

## Future Improvements

Possible enhancements:
1. Enable certificate verification with CA bundle
2. Add TLS 1.2/1.3 minimum version option
3. Show certificate info in UI
4. Support client certificates
5. Add HTTPS indicator in URL bar
6. Certificate error warnings/dialogs
7. HSTS (HTTP Strict Transport Security) support

## Credits

Implementation: 2025
Based on: libwww HTTP client
SSL/TLS: OpenSSL 3.x
Browser: ViolaWWW (Pei-Yuan Wei, 1990-1993)

