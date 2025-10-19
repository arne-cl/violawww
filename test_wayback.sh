#!/bin/bash
# Test script for Web Archive integration

echo "=== Testing Web Archive Integration ==="
echo ""
echo "1. Testing with a domain that exists (should work normally, with 30s timeout):"
echo "   viola.org - DNS exists but server doesn't respond"
echo ""
echo "2. Testing with a non-existent domain (should redirect to Web Archive):"
echo "   Use a domain that doesn't exist but is archived"
echo ""
echo "Run manually:"
echo "  ./src/vw/vw http://www.hackzone.ru/"
echo ""
echo "Expected behavior:"
echo "  - DNS lookup fails"
echo "  - Wayback checks archive.org CDX API"
echo "  - Finds earliest snapshot: 1997-03-27"
echo "  - Redirects to: https://web.archive.org/web/19970327140651/http://www.hackzone.ru:80/"
echo ""

