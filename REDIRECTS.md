# HTTP Redirect Support

## Overview

ViolaWWW now supports automatic HTTP redirects (3xx status codes). This is essential for modern web browsing as many sites automatically redirect HTTP to HTTPS.

## Supported Redirect Codes

- **301** Moved Permanently
- **302** Found (Temporary Redirect)
- **303** See Other
- **307** Temporary Redirect
- **308** Permanent Redirect

All 3xx codes are handled the same way: parse `Location:` header and follow it.

## How It Works

1. Client makes request to server
2. Server responds with 3xx status and `Location:` header
3. Client parses the new URL from `Location:` header
4. Client closes current connection
5. Client opens new connection to redirect URL
6. Repeat up to 10 times (to prevent infinite loops)

## Implementation

### Files Modified
- `src/libWWW/Library/Implementation/HTTP.c` - Added redirect handling for HTTP
- `src/libWWW/Library/Implementation/HTTPS.c` - Added redirect handling for HTTPS

### Key Features
- **Relative URL support**: Converts relative URLs to absolute using base URL
- **Loop prevention**: Maximum 10 redirects per request
- **Cross-protocol**: Can redirect from HTTP to HTTPS and vice versa
- **Verbose logging**: With `-v` flag, shows redirect chain

## Examples

### HTTP to HTTPS Redirect
```
http://example.com
  ↓ 301 Location: https://example.com
https://example.com
  ✓ Success
```

### Multiple Redirects
```
http://old-site.com
  ↓ 301 Location: http://new-site.com
http://new-site.com
  ↓ 301 Location: https://new-site.com
https://new-site.com
  ✓ Success
```

### Too Many Redirects
```
http://loop.com
  ↓ 302 Location: http://loop.com/a
http://loop.com/a
  ↓ 302 Location: http://loop.com/b
... (8 more redirects)
http://loop.com/z
  ✗ Error: "Too many redirects"
```

## Verbose Mode

Run with `-v` to see redirect messages:

```bash
./src/vw/vw -v test_https.html
```

Output:
```
HTTP: Redirect 301 -> https://example.com/
HTTPS: Connected to example.com using TLSv1.3
```

## Limitations

1. **No redirect history UI**: Browser doesn't show the redirect chain to user
2. **Simple loop detection**: Only counts hops, doesn't detect actual loops
3. **No redirect caching**: Each redirect requires new connection
4. **No 304 Not Modified support**: Doesn't handle conditional requests

## Error Messages

### "Too many redirects"
- Occurs after 10 redirect hops
- Usually indicates infinite redirect loop
- Check server configuration

### "Redirection response but no Location header found"
- Server sent 3xx status but no `Location:` header
- Server misconfiguration
- Try accessing URL directly

## Testing

Test with sites that use redirects:

```bash
# These will redirect HTTP → HTTPS
http://example.com → https://example.com
http://wikipedia.org → https://www.wikipedia.org
http://google.com → https://www.google.com
```

## Code Example

From `HTTP.c`:

```c
case 3: /* Various forms of redirection */
{
    char* location = NULL;
    char* p;
    
    /* Parse Location header from response */
    p = strstr(start_of_data, "Location:");
    if (!p) p = strstr(start_of_data, "location:");
    if (!p) p = strstr(start_of_data, "LOCATION:");
    
    if (p) {
        /* Extract URL */
        location = HTParse(new_url, arg, PARSE_ALL);
        
        /* Clean up and follow redirect */
        redirect_count++;
        arg = location;
        goto retry;
    }
}
```

## Future Improvements

Possible enhancements:
1. Show redirect chain in browser UI
2. Smart loop detection (track visited URLs)
3. Redirect history in navigation
4. Option to disable automatic redirects
5. Cache redirect mappings
6. Support for `Refresh:` meta redirects
7. Handle 304 Not Modified properly

## Credits

Added: October 2025
Part of HTTPS implementation for ViolaWWW

