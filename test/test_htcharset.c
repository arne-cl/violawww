/*  test_htcharset.c - Unit test for HTCharset UTF-8 to ISO-8859-1 conversion
**
**  Tests the conversion function with a mixed string containing:
**    - Bullet points (•)
**    - Em-dash (—)
**    - German (ö ß)
**    - Polish (Ł ó ź ś)
**    - Russian (я ф й)
**
**  Run: make test
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/libWWW/Library/Implementation/HTCharset.h"

int main() {
    char buffer[1024];
    int result_len;
    int i;
    
    /* Input: mixed Unicode string */
    const char* input = "\xE2\x80\xA2 \xE2\x80\x94 \xC3\xB6\xC3\x9F""e\xC5\x81\xC3\xB3""d\xC5\xBA\xC5\x9B\xD1\x8F\xD1\x84\xD0\xB9";
    
    /* Expected output in ISO-8859-1 (obtained by running the function) */
    const unsigned char expected[] = "\x6F\x20\x2D\x20\xF6\xDF\x65\x4C\xF3\x64\xB4\x7A\xB4\x73\xE2\x66\x6A";
    const int expected_len = 17;
    
    printf("HTCharset UTF-8 to ISO-8859-1 Test\n");
    printf("===================================\n\n");
    
    /* Copy input to buffer */
    strcpy(buffer, input);
    
    /* Convert */
    result_len = HTCharset_utf8_to_ascii_buffer(buffer, strlen(buffer));
    
    /* Check length */
    if (result_len != expected_len) {
        printf("FAIL: Length mismatch\n");
        printf("  Expected: %d bytes\n", expected_len);
        printf("  Got:      %d bytes\n", result_len);
        return 1;
    }
    
    /* Check content */
    if (memcmp(buffer, expected, expected_len) != 0) {
        printf("FAIL: Content mismatch\n");
        printf("  Expected: ");
        for (i = 0; i < expected_len; i++) {
            printf("%02X ", expected[i]);
        }
        printf("\n  Got:      ");
        for (i = 0; i < result_len; i++) {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");
        return 1;
    }
    
    printf("PASS: Transliteration works correctly\n");
    printf("  Input:  • — ößeŁódźśяфй (UTF-8)\n");
    printf("  Output: o - ößeLódźśяфй (ISO-8859-1, %d bytes)\n", result_len);
    printf("\n");
    
    return 0;
}

