/*  test_htcharset.c - Unit tests for HTCharset UTF-8 to ISO-8859-1 conversion
**
**  Compile and run:
**    make test_htcharset && ./test_htcharset
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "src/libWWW/Library/Implementation/HTCharset.h"

#define TEST_BUFFER_SIZE 1024

/* Test result structure */
typedef struct {
    const char* name;
    const char* input;
    const char* expected;
    int passed;
} TestCase;

/* Helper to run one test */
static void run_test(TestCase* test) {
    char buffer[TEST_BUFFER_SIZE];
    int result_len;
    
    /* Copy input to buffer */
    strncpy(buffer, test->input, TEST_BUFFER_SIZE - 1);
    buffer[TEST_BUFFER_SIZE - 1] = '\0';
    
    /* Convert */
    result_len = HTCharset_utf8_to_ascii_buffer(buffer, strlen(buffer));
    
    /* Check result */
    test->passed = (strcmp(buffer, test->expected) == 0);
    
    /* Print result */
    printf("%-30s ", test->name);
    if (test->passed) {
        printf("✓ PASS\n");
    } else {
        int i;
        printf("✗ FAIL\n");
        printf("  Input:    [%s]\n", test->input);
        printf("  Expected: [%s]\n", test->expected);
        printf("  Got:      [%s]\n", buffer);
        printf("  Length:   %d bytes\n", result_len);
        printf("  Hex:      ");
        for (i = 0; i < result_len && i < 40; i++) {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");
    }
}

int main() {
    TestCase tests[] = {
        /* Bullet points */
        {
            "Bullet point",
            "• First item",
            "o First item",
            0
        },
        {
            "Multiple bullets",
            "• One • Two • Three",
            "o One o Two o Three",
            0
        },
        
        /* Quotes and punctuation */
        {
            "Em-dash",
            "Hello—world—test",
            "Hello-world-test",
            0
        },
        
        /* German (diacritics should be preserved in ISO-8859-1) */
        {
            "German ü",
            "Größe",
            "Gr\xF6\xDF""e",  /* ö=0xF6, ß=0xDF in ISO-8859-1 */
            0
        },
        {
            "German ö",
            "schön",
            "sch\xF6n",  /* ö=0xF6 */
            0
        },
        {
            "German ä",
            "Käse",
            "K\xE4se",  /* ä=0xE4 */
            0
        },
        {
            "German ß",
            "Straße",
            "Stra\xDF""e",  /* ß=0xDF */
            0
        },
        {
            "German phrase",
            "1989 in Berührung kam.",
            "1989 in Ber\xFChrung kam.",  /* ü=0xFC */
            0
        },
        
        /* Polish (with diacritics, some NOT in ISO-8859-1) */
        {
            "Polish ł",
            "Łódź",
            "L\xF3""d\xB4z",  /* Ł→L, ó=0xF3, ź→´z (0xB4=acute) */
            0
        },
        {
            "Polish phrase",
            "Witaj świecie",
            "Witaj \xB4swiecie",  /* ś→´s (0xB4=acute) */
            0
        },
        
        /* Russian (Cyrillic → Latin transliteration) */
        {
            "Russian hello",
            "Привет",
            "Privet",
            0
        },
        {
            "Russian world",
            "мир",
            "mir",
            0
        },
        {
            "Russian phrase",
            "Это тест",
            "\xC9to test",  /* Э→É (0xC9) ICU transliteration */
            0
        },
        {
            "Russian long",
            "Здравствуйте, как дела?",
            "Zdravstvujte, kak dela?",
            0
        },
        
        /* Mixed content */
        {
            "Mixed bullet + Russian",
            "• Привет world",
            "o Privet world",
            0
        },
        {
            "Mixed Russian + German",
            "Привет Größe test",
            "Privet Gr\xF6\xDF""e test",  /* ö=0xF6, ß=0xDF */
            0
        }
    };
    
    int i;
    int num_tests = sizeof(tests) / sizeof(TestCase);
    int passed = 0;
    int failed = 0;
    
    printf("\n");
    printf("HTCharset UTF-8 to ISO-8859-1 Transliteration Tests\n");
    printf("====================================================\n\n");
    
    /* Run all tests */
    for (i = 0; i < num_tests; i++) {
        run_test(&tests[i]);
        if (tests[i].passed)
            passed++;
        else
            failed++;
    }
    
    /* Summary */
    printf("\n");
    printf("====================================================\n");
    printf("Results: %d passed, %d failed (total: %d)\n", passed, failed, num_tests);
    printf("\n");
    
    /* Exit with appropriate code */
    return (failed == 0) ? 0 : 1;
}

